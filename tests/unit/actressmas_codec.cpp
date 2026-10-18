/**************************************************************************
 *                                                                        *
 *  Description: CPPActressMas multi-agent framework                      *
 *  Website:     https://github.com/jferdelyi/CPPActressMAS               *
 *  Copyright:   (c) 2023-Today, Jean-François Erdelyi                    *
 *                                                                        *
 *  CPP version of ActressMAS by Florin Leon                              *
 *  https://github.com/florinleon/ActressMas                              *
 *                                                                        *
 *  This program is free software; you can redistribute it and/or modify  *
 *  it under the terms of the GNU General License as published by         *
 *  the Free Software Foundation. This program is distributed in the      *
 *  hope that it will be useful, but WITHOUT ANY WARRANTY; without even   *
 *  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR   *
 *  PURPOSE. See the GNU General License for more details.                *
 *                                                                        *
 **************************************************************************/

// ActressMAS wire codec tests against the bytes of the C# code: tests/vectors/envelope/actressmas/v1 holds
// frames and BinaryFormatter streams captured from ActressMAS v3.1 under .NET 8 (and one Request Move Agent
// under Mono), each with its decoded description. Every vector must decode to its description; the ones
// without a null string must be written back byte for byte; malformed input is refused.

#include <doctest/doctest.h>

#include <fstream>
#include <iterator>

#include <nlohmann/json.hpp>

#include <transport/ActressMasCodec.h>
#include <transport/Base64.h>

namespace {

	using cam::transport::ActressMasCodec;
	using cam::transport::ContainerMessage;
	using cam::transport::ErrorCode;
	using cam::transport::RemoteMessage;
	using json = nlohmann::json;

	constexpr std::size_t MAX_BYTES = 1u << 20;

	// Frames, and Message streams, captured from the C# code (see docs/port/22-actressmas-wire.md)
	const std::vector<std::string> FRAMES = {"request_register", "inform_containers", "inform_containers_empty", "inform_invalid_name",
											 "send_remote_message", "send_remote_reply", "null_content", "request_move_agent_mono"};
	const std::vector<std::string> MESSAGES = {"message_remote", "message_reply", "message_conversation", "message_empty_content",
											   "message_null_content", "message_null_conversation", "message_unicode",
											   "message_contentobj_string", "message_contentobj_int"};

	// The cases whose strings are all set, which the C++ writer reproduces exactly (it never writes a null string)
	const std::vector<std::string> REPRODUCIBLE = {"request_register", "inform_containers", "inform_containers_empty", "inform_invalid_name",
												   "send_remote_message", "send_remote_reply", "request_move_agent_mono",
												   "message_remote", "message_reply", "message_conversation"};

	std::vector<std::uint8_t> read_file(const std::string& p_path) {
		std::ifstream l_file(p_path, std::ios::binary);
		return {std::istreambuf_iterator<char>(l_file), std::istreambuf_iterator<char>()};
	}

	std::string vector_path(const std::string& p_name, const std::string& p_extension) {
		return std::string(CAM_VECTORS_DIR) + "/envelope/actressmas/v1/" + p_name + p_extension;
	}

	json read_description(const std::string& p_name) {
		const auto l_bytes = read_file(vector_path(p_name, ".json"));
		return json::parse(l_bytes.begin(), l_bytes.end());
	}

	// A null string of the C# side reads as an empty one; a boxed ContentObj reads as text
	std::string text_of(const json& p_value) {
		if (p_value.is_null()) {
			return {};
		}
		if (p_value.is_object() && p_value.contains("int32")) {
			return std::to_string(p_value["int32"].get<int>());
		}
		return p_value.get<std::string>();
	}

	void check_container(const ContainerMessage& p_message, const json& p_fields) {
		CHECK(p_message.m_content == text_of(p_fields["Content"]));
		CHECK(p_message.m_info == text_of(p_fields["Info"]));
		CHECK(p_message.m_receiver == text_of(p_fields["Receiver"]));
		CHECK(p_message.m_sender == text_of(p_fields["Sender"]));
	}

	void check_remote(const RemoteMessage& p_message, const json& p_fields) {
		const std::string l_content = p_fields["Content"].is_null() ? text_of(p_fields["ContentObj"]) : text_of(p_fields["Content"]);
		CHECK(p_message.m_content == l_content);
		CHECK(p_message.m_conversation_id == text_of(p_fields["ConversationId"]));
		CHECK(p_message.m_receiver == text_of(p_fields["Receiver"]));
		CHECK(p_message.m_sender == text_of(p_fields["Sender"]));
	}

	ContainerMessage container_of(const json& p_fields) {
		return {text_of(p_fields["Sender"]), text_of(p_fields["Receiver"]), text_of(p_fields["Info"]), text_of(p_fields["Content"])};
	}

	RemoteMessage remote_of(const json& p_fields) {
		return {text_of(p_fields["Sender"]), text_of(p_fields["Receiver"]), text_of(p_fields["Content"]), text_of(p_fields["ConversationId"])};
	}

	// A valid frame whose body is replaced by the given text (for the malformed cases)
	std::vector<std::uint8_t> frame_with_body(const std::string& p_text) {
		std::vector<std::uint8_t> l_frame = ActressMasCodec::encode_frame({"Container1", "Server", "Request Register", ""});
		l_frame.resize(ActressMasCodec::HEADER_SIZE);
		for (const char l_char: p_text) {
			l_frame.push_back(static_cast<std::uint8_t>(l_char));
			l_frame.push_back(0);
		}
		const std::size_t l_size = l_frame.size();
		for (std::size_t i = 0; i < 4; i++) {
			l_frame[2 + i] = static_cast<std::uint8_t>(l_size >> (8 * (3 - i)));
		}
		return l_frame;
	}

	// The NRBF stream of a register message with a few bytes changed (for the malformed cases)
	std::vector<std::uint8_t> altered_stream(const std::string& p_from, const std::string& p_to) {
		auto l_bytes = ActressMasCodec::write_nrbf(ContainerMessage{"Container1", "Server", "Request Register", ""});
		std::string l_text(l_bytes.begin(), l_bytes.end());
		const auto l_position = l_text.find(p_from);
		REQUIRE(l_position != std::string::npos);
		l_text.replace(l_position, p_from.size(), p_to);
		return {l_text.begin(), l_text.end()};
	}

} // namespace

TEST_SUITE("actressmas codec") {

	TEST_CASE("every frame captured from the C# code decodes to its description") {
		for (const auto& l_name: FRAMES) {
			CAPTURE(l_name);
			const auto l_frame = read_file(vector_path(l_name, ".bin"));
			const json l_description = read_description(l_name);
			REQUIRE(!l_frame.empty());
			CHECK(l_description["layer"] == "frame");
			CHECK(l_description["assembly"].get<std::string>() == ActressMasCodec::ASSEMBLY);
			const auto l_size = ActressMasCodec::read_frame_size(l_frame, MAX_BYTES);
			REQUIRE(l_size);
			CHECK(l_size.value() == l_frame.size());
			const auto l_message = ActressMasCodec::decode_frame(l_frame, MAX_BYTES);
			REQUIRE_MESSAGE(l_message, (l_message ? std::string() : l_message.error().m_message));
			check_container(l_message.value(), l_description["fields"]);
			// The Message carried by a "Send Remote Message" decodes too
			if (l_description.contains("content_message")) {
				const auto l_remote = ActressMasCodec::decode_remote_message(l_message.value().m_content);
				REQUIRE_MESSAGE(l_remote, (l_remote ? std::string() : l_remote.error().m_message));
				check_remote(l_remote.value(), l_description["content_message"]["fields"]);
			}
		}
	}

	TEST_CASE("every Message stream captured from the C# code decodes to its description") {
		for (const auto& l_name: MESSAGES) {
			CAPTURE(l_name);
			const auto l_stream = read_file(vector_path(l_name, ".bin"));
			const json l_description = read_description(l_name);
			REQUIRE(!l_stream.empty());
			CHECK(l_description["layer"] == "nrbf");
			const auto l_message = ActressMasCodec::read_remote_nrbf(l_stream);
			REQUIRE_MESSAGE(l_message, (l_message ? std::string() : l_message.error().m_message));
			check_remote(l_message.value(), l_description["fields"]);
		}
	}

	TEST_CASE("the C++ writer reproduces the C# bytes when no string is null") {
		for (const auto& l_name: REPRODUCIBLE) {
			CAPTURE(l_name);
			const auto l_expected = read_file(vector_path(l_name, ".bin"));
			const json l_description = read_description(l_name);
			if (l_description["layer"] == "frame") {
				CHECK(ActressMasCodec::encode_frame(container_of(l_description["fields"])) == l_expected);
			} else {
				CHECK(ActressMasCodec::write_nrbf(remote_of(l_description["fields"])) == l_expected);
			}
		}
	}

	TEST_CASE("round trip of empty, long and non-ASCII strings") {
		const std::string l_long(300, 'x');  // a length prefix of two bytes
		const ContainerMessage l_container{"Container-é", "Server", "Send Remote Message", l_long};
		const auto l_decoded = ActressMasCodec::decode_frame(ActressMasCodec::encode_frame(l_container), MAX_BYTES);
		REQUIRE(l_decoded);
		CHECK(l_decoded.value().m_sender == l_container.m_sender);
		CHECK(l_decoded.value().m_content == l_long);

		const RemoteMessage l_remote{"ping@C1", "pong@C2", "héllo → 世界", ""};
		const auto l_back = ActressMasCodec::decode_remote_message(ActressMasCodec::encode_remote_message(l_remote));
		REQUIRE(l_back);
		CHECK(l_back.value().m_content == l_remote.m_content);
		CHECK(l_back.value().m_conversation_id.empty());
	}

	TEST_CASE("a malformed frame is refused") {
		const auto l_valid = ActressMasCodec::encode_frame({"Container1", "Server", "Request Register", ""});

		auto l_truncated = l_valid;
		l_truncated.resize(10);
		CHECK_FALSE(ActressMasCodec::read_frame_size(l_truncated, MAX_BYTES));
		CHECK_FALSE(ActressMasCodec::decode_frame(l_truncated, MAX_BYTES));

		auto l_marker = l_valid;
		l_marker[1] = 0xFE;
		CHECK_FALSE(ActressMasCodec::decode_frame(l_marker, MAX_BYTES));

		auto l_short = l_valid;
		l_short.pop_back();
		CHECK_FALSE(ActressMasCodec::decode_frame(l_short, MAX_BYTES));

		const auto l_large = ActressMasCodec::read_frame_size(l_valid, 100);
		REQUIRE_FALSE(l_large);
		CHECK(l_large.error().m_code == ErrorCode::TooLarge);

		CHECK_FALSE(ActressMasCodec::decode_frame(frame_with_body("AAEAAAD"), MAX_BYTES));  // not base64 (length)
		CHECK_FALSE(ActressMasCodec::decode_frame(frame_with_body("AA*A"), MAX_BYTES));     // not base64 (alphabet)
		CHECK_FALSE(ActressMasCodec::decode_frame(frame_with_body("AAAA"), MAX_BYTES));     // base64, not a stream

		auto l_wide = l_valid;
		l_wide[ActressMasCodec::HEADER_SIZE + 1] = 0x01;  // a UTF-16 character outside ASCII
		CHECK_FALSE(ActressMasCodec::decode_frame(l_wide, MAX_BYTES));
	}

	TEST_CASE("a BinaryFormatter stream of another assembly, class or layout is refused") {
		CHECK(ActressMasCodec::read_container_nrbf(altered_stream("Request", "Request")));
		CHECK_FALSE(ActressMasCodec::read_container_nrbf(altered_stream("Version=3.1.0.0", "Version=3.0.0.0")));
		CHECK_FALSE(ActressMasCodec::read_container_nrbf(altered_stream("ContainerMessage", "ContainerMessagf")));
		CHECK_FALSE(ActressMasCodec::read_container_nrbf(altered_stream("<Info>", "<Infx>")));
		// A Message is not a ContainerMessage, and the reverse
		CHECK_FALSE(ActressMasCodec::read_remote_nrbf(ActressMasCodec::write_nrbf(ContainerMessage{"a", "b", "c", "d"})));
		CHECK_FALSE(ActressMasCodec::read_container_nrbf(ActressMasCodec::write_nrbf(RemoteMessage{"a", "b", "c", "d"})));

		auto l_stream = ActressMasCodec::write_nrbf(ContainerMessage{"a", "b", "c", "d"});
		auto l_trailing = l_stream;
		l_trailing.push_back(0);
		CHECK_FALSE(ActressMasCodec::read_container_nrbf(l_trailing));
		auto l_cut = l_stream;
		l_cut.resize(l_cut.size() - 3);
		CHECK_FALSE(ActressMasCodec::read_container_nrbf(l_cut));
		auto l_unknown = l_stream;
		l_unknown[l_unknown.size() - 1] = 0x10;  // an unknown record instead of MessageEnd
		CHECK_FALSE(ActressMasCodec::read_container_nrbf(l_unknown));
	}

	TEST_CASE("a member reference reads the string it points to, and only a known one") {
		// BinaryFormatter writes a reference when the same string instance appears twice: Content and Info here
		auto l_stream = ActressMasCodec::write_nrbf(ContainerMessage{"Container1", "Server", "same", "same"});
		std::string l_text(l_stream.begin(), l_stream.end());
		// The second "same" (Info, id 4) becomes a reference to the first one (Content, id 3)
		const std::string l_second = std::string("\x06\x04\x00\x00\x00\x04same", 10);
		const auto l_position = l_text.find(l_second);
		REQUIRE(l_position != std::string::npos);
		l_text.replace(l_position, l_second.size(), std::string("\x09\x03\x00\x00\x00", 5));
		const auto l_decoded = ActressMasCodec::read_container_nrbf(std::vector<std::uint8_t>(l_text.begin(), l_text.end()));
		REQUIRE_MESSAGE(l_decoded, (l_decoded ? std::string() : l_decoded.error().m_message));
		CHECK(l_decoded.value().m_info == "same");
		CHECK(l_decoded.value().m_content == "same");

		l_text.replace(l_position, 5, std::string("\x09\x07\x00\x00\x00", 5));
		CHECK_FALSE(ActressMasCodec::read_container_nrbf(std::vector<std::uint8_t>(l_text.begin(), l_text.end())));
	}
}
