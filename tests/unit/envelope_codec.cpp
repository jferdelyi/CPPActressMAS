/**************************************************************************
 *                                                                        *
 *  Description: CPPActressMas multi-agent framework                      *
 *  Website:     https://github.com/jferdelyi/CPPActressMAS               *
 *  Copyright:   (c) 2023-Today, Jean-François Erdelyi                    *
 *                                                                        *
 *  CPP version of ActressMAS by Florin Leon                              *
 *  https://github.com/florinleon/ActressMas                              *
 *                                                                        *
 *  This program and the accompanying materials are made available under  *
 *  the terms of the Eclipse Public License 2.0, which is available at    *
 *  https://www.eclipse.org/legal/epl-2.0/ and in the LICENSE file.       *
 *                                                                        *
 *  SPDX-License-Identifier: EPL-2.0                                      *
 *                                                                        *
 **************************************************************************/

// Envelope codec tests: round trips, binary vectors pinned under tests/unit/vectors/envelope/compact/v1
// (one .bin and one .json per vector; the .json is the readable description, the .bin the bytes a
// receiver must accept), and malformed input. CAM_WRITE_VECTORS=1 regenerates the vectors.

#include <doctest/doctest.h>

#include <cstdlib>
#include <fstream>
#include <iterator>

#include <transport/codec/CompactCodec.h>
#include <transport/codec/EnvelopeCodec.h>
#include <transport/codec/JsonCodec.h>
#include <transport/codec/ProtobufCodec.h>

namespace {

	using namespace cam::transport;

	constexpr std::size_t MAX_BYTES = 1u << 20;
	const std::string ENV_A = "0f3c9d6e-1a2b-4c3d-8e9f-a0b1c2d3e4f5";
	const std::string ENV_B = "9e8d7c6b-5a4f-4e3d-9c2b-1a0f9e8d7c6b";
	const std::string AGENT_1 = "11111111-2222-4333-8444-555555555555";
	const std::string AGENT_2 = "66666666-7777-4888-9999-aaaaaaaaaaaa";
	const std::string NODE = "abcdefab-cdef-4abc-8def-abcdefabcdef";

	std::string hex(const std::vector<std::uint8_t>& p_bytes) {
		static const char* s_digits = "0123456789abcdef";
		std::string l_result;
		l_result.reserve(p_bytes.size() * 2);
		for (const std::uint8_t l_byte: p_bytes) {
			l_result.push_back(s_digits[l_byte >> 4]);
			l_result.push_back(s_digits[l_byte & 0x0f]);
		}
		return l_result;
	}

	std::vector<std::uint8_t> unhex(const std::string& p_text) {
		std::vector<std::uint8_t> l_result;
		for (std::size_t i = 0; i + 1 < p_text.size(); i += 2) {
			l_result.push_back(static_cast<std::uint8_t>(std::stoul(p_text.substr(i, 2), nullptr, 16)));
		}
		return l_result;
	}

	std::vector<std::uint8_t> bytes(const std::string& p_text) {
		return {p_text.begin(), p_text.end()};
	}

	// ---------------------------------------------------------------- JSON description of an envelope

	json to_json(const Envelope& p_envelope);

	json payload_to_json(const Envelope& p_envelope) {
		if (const auto* l_transfer = std::get_if<AgentTransfer>(&p_envelope.m_payload)) {
			json l_queue = json::array();
			for (const auto& l_queued: l_transfer->m_queue) {
				l_queue.push_back(to_json(l_queued));
			}
			return {{"type", "transfer"}, {"transfer_id", l_transfer->m_transfer_id}, {"type_name", l_transfer->m_type_name},
					{"agent_id", l_transfer->m_agent_id}, {"agent_name", l_transfer->m_agent_name},
					{"state_encoding", static_cast<int>(l_transfer->m_state_encoding)}, {"state", hex(l_transfer->m_state)},
					{"queue", l_queue}, {"uses_observables", l_transfer->m_uses_observables}, {"observables", hex(l_transfer->m_observables)}};
		}
		if (const auto* l_control = std::get_if<AgentControl>(&p_envelope.m_payload)) {
			return {{"type", "control"}, {"transfer_id", l_control->m_transfer_id}, {"agent_id", l_control->m_agent_id},
					{"accepted", l_control->m_accepted}, {"reason", l_control->m_reason}};
		}
		if (const auto* l_presence = std::get_if<Presence>(&p_envelope.m_payload)) {
			json l_agents = json::array();
			for (const auto& [l_id, l_name]: l_presence->m_agents) {
				l_agents.push_back({{"id", l_id}, {"name", l_name}});
			}
			json l_result = {{"type", "presence"}, {"node_id", l_presence->m_node_id}, {"environment_id", l_presence->m_environment_id},
							 {"environment_name", l_presence->m_environment_name}, {"namespace", l_presence->m_namespace},
							 {"online", l_presence->m_online}, {"timestamp_ms", l_presence->m_timestamp_ms}, {"agents", l_agents}};
			if (l_presence->m_turn) {
				l_result["turn"] = *l_presence->m_turn;
			}
			return l_result;
		}
		if (const auto* l_event = std::get_if<DirectoryEvent>(&p_envelope.m_payload)) {
			return {{"type", "directory"}, {"event", static_cast<int>(l_event->m_event)}, {"agent_id", l_event->m_agent_id},
					{"agent_name", l_event->m_agent_name}, {"environment_id", l_event->m_environment_id},
					{"previous_environment_id", l_event->m_previous_environment_id}};
		}
		return {{"type", "none"}};
	}

	json to_json(const Envelope& p_envelope) {
		json l_json = {{"version_major", p_envelope.m_version_major}, {"version_minor", p_envelope.m_version_minor},
				{"kind", static_cast<int>(p_envelope.m_kind)}, {"sender_environment", p_envelope.m_sender_environment},
				{"origin_environment", p_envelope.m_origin_environment}, {"epoch", p_envelope.m_epoch},
				{"stream_kind", static_cast<int>(p_envelope.m_stream_kind)}, {"stream_target", p_envelope.m_stream_target},
				{"sequence", p_envelope.m_sequence}, {"conversation_id", p_envelope.m_conversation_id},
				{"sender_agent", p_envelope.m_sender_agent}, {"target_kind", static_cast<int>(p_envelope.m_target_kind)},
				{"target_id", p_envelope.m_target_id}, {"target_name", p_envelope.m_target_name}, {"first_only", p_envelope.m_first_only},
				{"scope", static_cast<int>(p_envelope.m_scope)}, {"content_encoding", static_cast<int>(p_envelope.m_content_encoding)},
				{"content", hex(p_envelope.m_content)}, {"priority", p_envelope.m_priority}, {"payload", payload_to_json(p_envelope)}};
		// Since 1.1, and only when known: the descriptions of the 1.0 vectors have no such key
		if (!p_envelope.m_sender_name.empty()) {
			l_json["sender_name"] = p_envelope.m_sender_name;
		}
		return l_json;
	}

	Envelope from_json(const json& p_json) {
		Envelope l_envelope;
		l_envelope.m_version_major = p_json.at("version_major").get<std::uint8_t>();
		l_envelope.m_version_minor = p_json.at("version_minor").get<std::uint8_t>();
		l_envelope.m_kind = static_cast<EnvelopeKind>(p_json.at("kind").get<int>());
		l_envelope.m_sender_environment = p_json.at("sender_environment").get<std::string>();
		l_envelope.m_origin_environment = p_json.at("origin_environment").get<std::string>();
		l_envelope.m_epoch = p_json.at("epoch").get<std::uint32_t>();
		l_envelope.m_stream_kind = static_cast<StreamKind>(p_json.at("stream_kind").get<int>());
		l_envelope.m_stream_target = p_json.at("stream_target").get<std::string>();
		l_envelope.m_sequence = p_json.at("sequence").get<std::uint64_t>();
		l_envelope.m_conversation_id = p_json.at("conversation_id").get<std::string>();
		l_envelope.m_sender_agent = p_json.at("sender_agent").get<std::string>();
		l_envelope.m_sender_name = p_json.value("sender_name", std::string());
		l_envelope.m_target_kind = static_cast<TargetKind>(p_json.at("target_kind").get<int>());
		l_envelope.m_target_id = p_json.at("target_id").get<std::string>();
		l_envelope.m_target_name = p_json.at("target_name").get<std::string>();
		l_envelope.m_first_only = p_json.at("first_only").get<bool>();
		l_envelope.m_scope = static_cast<Scope>(p_json.at("scope").get<int>());
		l_envelope.m_content_encoding = static_cast<cam::ContentEncoding>(p_json.at("content_encoding").get<int>());
		l_envelope.m_content = unhex(p_json.at("content").get<std::string>());
		l_envelope.m_priority = p_json.at("priority").get<std::uint32_t>();
		const json& l_payload = p_json.at("payload");
		const std::string l_type = l_payload.at("type").get<std::string>();
		if (l_type == "transfer") {
			AgentTransfer l_transfer;
			l_transfer.m_transfer_id = l_payload.at("transfer_id").get<std::string>();
			l_transfer.m_type_name = l_payload.at("type_name").get<std::string>();
			l_transfer.m_agent_id = l_payload.at("agent_id").get<std::string>();
			l_transfer.m_agent_name = l_payload.at("agent_name").get<std::string>();
			l_transfer.m_state_encoding = static_cast<cam::ContentEncoding>(l_payload.at("state_encoding").get<int>());
			l_transfer.m_state = unhex(l_payload.at("state").get<std::string>());
			for (const auto& l_queued: l_payload.at("queue")) {
				l_transfer.m_queue.push_back(from_json(l_queued));
			}
			l_transfer.m_uses_observables = l_payload.at("uses_observables").get<bool>();
			l_transfer.m_observables = unhex(l_payload.at("observables").get<std::string>());
			l_envelope.m_payload = std::move(l_transfer);
		} else if (l_type == "control") {
			AgentControl l_control;
			l_control.m_transfer_id = l_payload.at("transfer_id").get<std::string>();
			l_control.m_agent_id = l_payload.at("agent_id").get<std::string>();
			l_control.m_accepted = l_payload.at("accepted").get<bool>();
			l_control.m_reason = l_payload.at("reason").get<std::string>();
			l_envelope.m_payload = std::move(l_control);
		} else if (l_type == "presence") {
			Presence l_presence;
			l_presence.m_node_id = l_payload.at("node_id").get<std::string>();
			l_presence.m_environment_id = l_payload.at("environment_id").get<std::string>();
			l_presence.m_environment_name = l_payload.at("environment_name").get<std::string>();
			l_presence.m_namespace = l_payload.at("namespace").get<std::string>();
			l_presence.m_online = l_payload.at("online").get<bool>();
			l_presence.m_timestamp_ms = l_payload.at("timestamp_ms").get<std::uint64_t>();
			if (l_payload.contains("turn")) {
				l_presence.m_turn = l_payload.at("turn").get<std::uint64_t>();
			}
			for (const auto& l_agent: l_payload.at("agents")) {
				l_presence.m_agents.emplace_back(l_agent.at("id").get<std::string>(), l_agent.at("name").get<std::string>());
			}
			l_envelope.m_payload = std::move(l_presence);
		} else if (l_type == "directory") {
			DirectoryEvent l_event;
			l_event.m_event = static_cast<DirectoryEventKind>(l_payload.at("event").get<int>());
			l_event.m_agent_id = l_payload.at("agent_id").get<std::string>();
			l_event.m_agent_name = l_payload.at("agent_name").get<std::string>();
			l_event.m_environment_id = l_payload.at("environment_id").get<std::string>();
			l_event.m_previous_environment_id = l_payload.at("previous_environment_id").get<std::string>();
			l_envelope.m_payload = std::move(l_event);
		}
		return l_envelope;
	}

	// ---------------------------------------------------------------- samples

	Envelope message_by_id() {
		Envelope l_envelope;
		l_envelope.m_kind = EnvelopeKind::Message;
		l_envelope.m_sender_environment = ENV_A;
		l_envelope.m_epoch = 0x01020304;
		l_envelope.m_stream_kind = StreamKind::In;
		l_envelope.m_stream_target = ENV_B;
		l_envelope.m_sequence = 42;
		l_envelope.m_sender_agent = AGENT_1;
		l_envelope.m_target_kind = TargetKind::Id;
		l_envelope.m_target_id = AGENT_2;
		l_envelope.m_content_encoding = cam::ContentEncoding::MessagePack;
		l_envelope.m_content = json::to_msgpack(json{{"hello", "world"}, {"n", 7}});
		return l_envelope;
	}

	Envelope message_by_name_first_only() {
		Envelope l_envelope = message_by_id();
		l_envelope.m_stream_kind = StreamKind::All;
		l_envelope.m_stream_target.clear();
		l_envelope.m_sequence = 1;
		l_envelope.m_target_kind = TargetKind::Name;
		l_envelope.m_target_id.clear();
		l_envelope.m_target_name = "worker 1";
		l_envelope.m_first_only = true;
		l_envelope.m_scope = Scope::Network;
		l_envelope.m_content_encoding = cam::ContentEncoding::Raw;
		l_envelope.m_content = bytes("ping");
		return l_envelope;
	}

	Envelope broadcast_forwarded() {
		Envelope l_envelope = message_by_id();
		l_envelope.m_origin_environment = ENV_B;
		l_envelope.m_stream_kind = StreamKind::All;
		l_envelope.m_stream_target.clear();
		l_envelope.m_sequence = 0xFFFFFFFFFFFFFFFFULL;
		l_envelope.m_conversation_id = "conv-1";
		l_envelope.m_target_kind = TargetKind::Broadcast;
		l_envelope.m_target_id.clear();
		l_envelope.m_scope = Scope::Network;
		l_envelope.m_content_encoding = cam::ContentEncoding::Cbor;
		l_envelope.m_content = json::to_cbor(json::array({1, 2, 3}));
		l_envelope.m_priority = 5;
		return l_envelope;
	}

	Envelope text_message() {
		Envelope l_envelope = message_by_id();
		l_envelope.m_sequence = 43;
		l_envelope.m_content_encoding = cam::ContentEncoding::Text;
		l_envelope.m_content = bytes("hello in clear");
		return l_envelope;
	}

	Envelope empty_message() {
		Envelope l_envelope = message_by_id();
		l_envelope.m_content_encoding = cam::ContentEncoding::Raw;
		l_envelope.m_content.clear();
		return l_envelope;
	}

	Envelope presence_online() {
		Envelope l_envelope;
		l_envelope.m_kind = EnvelopeKind::Presence;
		l_envelope.m_sender_environment = ENV_A;
		l_envelope.m_epoch = 9;
		l_envelope.m_stream_kind = StreamKind::Presence;
		l_envelope.m_stream_target = ENV_A;
		l_envelope.m_sequence = 1;
		Presence l_presence;
		l_presence.m_node_id = NODE;
		l_presence.m_environment_id = ENV_A;
		l_presence.m_environment_name = "Environment A";
		l_presence.m_namespace = "lab-1";
		l_presence.m_online = true;
		l_presence.m_turn = 12;
		l_presence.m_timestamp_ms = 1759564800000ULL;
		l_presence.m_agents = {{AGENT_1, "alice"}, {AGENT_2, "bob"}};
		l_envelope.m_payload = std::move(l_presence);
		return l_envelope;
	}

	Envelope presence_offline() {
		Envelope l_envelope = presence_online();
		l_envelope.m_sequence = 2;
		auto& l_presence = std::get<Presence>(l_envelope.m_payload);
		l_presence.m_online = false;
		l_presence.m_turn.reset();
		l_presence.m_agents.clear();
		return l_envelope;
	}

	Envelope directory_added() {
		Envelope l_envelope;
		l_envelope.m_kind = EnvelopeKind::Directory;
		l_envelope.m_sender_environment = ENV_A;
		l_envelope.m_epoch = 9;
		l_envelope.m_stream_kind = StreamKind::Presence;
		l_envelope.m_stream_target = ENV_A;
		l_envelope.m_sequence = 3;
		DirectoryEvent l_event;
		l_event.m_event = DirectoryEventKind::AgentMoved;
		l_event.m_agent_id = AGENT_2;
		l_event.m_agent_name = "bob";
		l_event.m_environment_id = ENV_A;
		l_event.m_previous_environment_id = ENV_B;
		l_envelope.m_payload = std::move(l_event);
		return l_envelope;
	}

	Envelope agent_accept() {
		Envelope l_envelope;
		l_envelope.m_kind = EnvelopeKind::AgentAccept;
		l_envelope.m_sender_environment = ENV_B;
		l_envelope.m_epoch = 77;
		l_envelope.m_stream_kind = StreamKind::In;
		l_envelope.m_stream_target = ENV_A;
		l_envelope.m_sequence = 8;
		AgentControl l_control;
		l_control.m_transfer_id = "t-0001";
		l_control.m_agent_id = AGENT_1;
		l_control.m_accepted = true;
		l_envelope.m_payload = std::move(l_control);
		return l_envelope;
	}

	Envelope agent_offer_with_queue() {
		Envelope l_envelope;
		l_envelope.m_kind = EnvelopeKind::AgentOffer;
		l_envelope.m_sender_environment = ENV_A;
		l_envelope.m_epoch = 9;
		l_envelope.m_stream_kind = StreamKind::Agent;
		l_envelope.m_stream_target = ENV_B;
		l_envelope.m_sequence = 1;
		AgentTransfer l_transfer;
		l_transfer.m_transfer_id = "t-0002";
		l_transfer.m_type_name = "MobileAgent";
		l_transfer.m_agent_id = AGENT_1;
		l_transfer.m_agent_name = "alice";
		l_transfer.m_state_encoding = cam::ContentEncoding::MessagePack;
		l_transfer.m_state = json::to_msgpack(json{{"counter", 3}});
		l_transfer.m_queue.push_back(message_by_id());
		l_transfer.m_queue.push_back(empty_message());
		l_transfer.m_uses_observables = true;
		l_transfer.m_observables = json::to_msgpack(json{{"x", 1.5}});
		l_envelope.m_payload = std::move(l_transfer);
		return l_envelope;
	}

	// Version 1.1: the name of the sender travels with the message
	Envelope message_with_sender_name() {
		Envelope l_envelope = message_by_id();
		l_envelope.m_version_minor = 1;
		l_envelope.m_sequence = 44;
		l_envelope.m_sender_name = "alice";
		return l_envelope;
	}

	// Version 1.1: the queue of a moving agent keeps the names of the senders
	Envelope agent_offer_with_named_queue() {
		Envelope l_envelope = agent_offer_with_queue();
		l_envelope.m_version_minor = 1;
		auto& l_transfer = std::get<AgentTransfer>(l_envelope.m_payload);
		l_transfer.m_transfer_id = "t-0003";
		for (auto& l_queued: l_transfer.m_queue) {
			l_queued.m_version_minor = 1;
			l_queued.m_sender_name = "bob";
		}
		return l_envelope;
	}

	struct Sample {
		const char* m_name;
		Envelope (*m_make)();
	};

	const Sample SAMPLES[] = {
			{"message_by_id", message_by_id},
			{"message_by_name_first_only", message_by_name_first_only},
			{"broadcast_forwarded", broadcast_forwarded},
			{"text_message", text_message},
			{"empty_message", empty_message},
			{"presence_online", presence_online},
			{"presence_offline", presence_offline},
			{"directory_added", directory_added},
			{"agent_accept", agent_accept},
			{"agent_offer_with_queue", agent_offer_with_queue},
			{"message_with_sender_name", message_with_sender_name},
			{"agent_offer_with_named_queue", agent_offer_with_named_queue},
	};

	std::vector<std::uint8_t> read_file(const std::string& p_path) {
		std::ifstream l_in(p_path, std::ios::binary);
		return {std::istreambuf_iterator<char>(l_in), std::istreambuf_iterator<char>()};
	}

	void write_file(const std::string& p_path, const std::vector<std::uint8_t>& p_bytes) {
		std::ofstream l_out(p_path, std::ios::binary | std::ios::trunc);
		l_out.write(reinterpret_cast<const char*>(p_bytes.data()), static_cast<std::streamsize>(p_bytes.size()));
	}

	std::vector<std::uint8_t> encode(const EnvelopeCodec& p_codec, const Envelope& p_envelope) {
		auto l_result = p_codec.encode(p_envelope);
		REQUIRE(l_result);
		return l_result.value();
	}

} // namespace

TEST_SUITE("envelope_codec") {

	TEST_CASE("header: magic, codec id, major version") {
		std::vector<std::uint8_t> l_bytes;
		EnvelopeCodec::write_header(l_bytes, CompactCodec::ID, CompactCodec::VERSION_MAJOR);
		REQUIRE(l_bytes.size() == ENVELOPE_HEADER_SIZE);
		CHECK(l_bytes[0] == ENVELOPE_MAGIC);
		CHECK(l_bytes[1] == 1);
		CHECK(l_bytes[2] == 1);
		CHECK(l_bytes[3] == 0);
		const auto l_header = EnvelopeCodec::read_header(l_bytes);
		REQUIRE(l_header);
		CHECK(l_header.value().m_codec_id == 1);
		CHECK(l_header.value().m_version_major == 1);
		CHECK_FALSE(EnvelopeCodec::read_header(std::vector<std::uint8_t>{0xCA, 1}));
		CHECK_FALSE(EnvelopeCodec::read_header(std::vector<std::uint8_t>{0x00, 1, 1, 0}));
	}

	TEST_CASE("codecs: names, identifiers, default") {
		const auto l_names = codec_names();
		REQUIRE(l_names.size() == 3);
		CHECK(l_names[0] == "protobuf");
		CHECK(make_codec("protobuf")->id() == ProtobufCodec::ID);
		CHECK(make_codec("compact")->id() == CompactCodec::ID);
		CHECK(make_codec("json")->id() == JsonCodec::ID);
		CHECK(make_codec("cbor") == nullptr);
		CHECK(TransportConfig{}.m_codec == "protobuf");
		for (const auto& l_name: l_names) {
			const auto l_codec = make_codec(l_name);
			REQUIRE(l_codec);
			CHECK(l_codec->name() == l_name);
			CHECK(l_codec->version_major() == 1);
		}
	}

	TEST_CASE("content encodings keep the values of the wire") {
		// The codecs write the value of the encoding (ContentEncoding of envelope.proto): a renamed or reordered
		// encoding must keep it
		CHECK(static_cast<int>(cam::ContentEncoding::Raw) == 0);
		CHECK(static_cast<int>(cam::ContentEncoding::BjData) == 1);
		CHECK(static_cast<int>(cam::ContentEncoding::Bson) == 2);
		CHECK(static_cast<int>(cam::ContentEncoding::Cbor) == 3);
		CHECK(static_cast<int>(cam::ContentEncoding::MessagePack) == 4);
		CHECK(static_cast<int>(cam::ContentEncoding::Ubjson) == 5);
		CHECK(static_cast<int>(cam::ContentEncoding::Text) == 6);
		CHECK(static_cast<int>(cam::ContentEncoding::Json) == 7);
	}

	TEST_CASE("every sample round-trips through every codec") {
		for (const auto& l_name: codec_names()) {
			const auto l_codec = make_codec(l_name);
			for (const Sample& l_sample: SAMPLES) {
				CAPTURE(l_name);
				CAPTURE(l_sample.m_name);
				const Envelope l_original = l_sample.m_make();
				REQUIRE(validate_envelope(l_original, MAX_BYTES));
				const auto l_bytes = encode(*l_codec, l_original);
				REQUIRE(l_bytes.size() > ENVELOPE_HEADER_SIZE);
				CHECK(l_bytes[0] == ENVELOPE_MAGIC);
				CHECK(l_bytes[1] == l_codec->id());
				const auto l_decoded = l_codec->decode(l_bytes, MAX_BYTES);
				REQUIRE_MESSAGE(l_decoded.has_value(), (l_decoded ? std::string() : l_decoded.error().m_message));
				CHECK(to_json(l_decoded.value()) == to_json(l_original));
			}
		}
	}

	TEST_CASE("binary vectors v1 of every codec") {
		const bool l_write = std::getenv("CAM_WRITE_VECTORS") != nullptr;
		for (const auto& l_name: codec_names()) {
		const auto l_codec_pointer = make_codec(l_name);
		const EnvelopeCodec& l_codec = *l_codec_pointer;
		const std::string l_directory = std::string(CAM_VECTORS_DIR) + "/envelope/" + l_name + "/v1/";
		for (const Sample& l_sample: SAMPLES) {
			CAPTURE(l_name);
			CAPTURE(l_sample.m_name);
			const std::string l_bin_path = l_directory + l_sample.m_name + ".bin";
			const std::string l_json_path = l_directory + l_sample.m_name + ".json";
			const Envelope l_original = l_sample.m_make();
			if (l_write) {
				write_file(l_bin_path, encode(l_codec, l_original));
				const std::string l_text = to_json(l_original).dump(2) + "\n";
				write_file(l_json_path, bytes(l_text));
			}
			const auto l_bin = read_file(l_bin_path);
			const auto l_json_bytes = read_file(l_json_path);
			REQUIRE_MESSAGE(!l_bin.empty(), "missing vector " << l_bin_path);
			REQUIRE_MESSAGE(!l_json_bytes.empty(), "missing vector " << l_json_path);
			const json l_description = json::parse(l_json_bytes.begin(), l_json_bytes.end());

			// The description is the source of truth: it encodes to the pinned bytes...
			const Envelope l_described = from_json(l_description);
			CHECK(hex(encode(l_codec, l_described)) == hex(l_bin));
			// ...and the pinned bytes decode to the description.
			const auto l_decoded = l_codec.decode(l_bin, MAX_BYTES);
			REQUIRE_MESSAGE(l_decoded.has_value(), (l_decoded ? std::string() : l_decoded.error().m_message));
			CHECK(to_json(l_decoded.value()) == l_description);
			// The in-code sample still matches its vector (a change of either is deliberate).
			CHECK(to_json(l_original) == l_description);
		}
		}
	}

	TEST_CASE("the name of the sender travels from version 1.1 on") {
		for (const auto& l_name: codec_names()) {
			CAPTURE(l_name);
			const auto l_codec = make_codec(l_name);
			const auto l_decoded = l_codec->decode(encode(*l_codec, message_with_sender_name()), MAX_BYTES);
			REQUIRE(l_decoded.has_value());
			CHECK(l_decoded.value().m_sender_name == "alice");
			// A 1.0 envelope has no name: the compact codec does not even write one
			Envelope l_old = message_with_sender_name();
			l_old.m_version_minor = 0;
			const auto l_old_decoded = l_codec->decode(encode(*l_codec, l_old), MAX_BYTES);
			REQUIRE(l_old_decoded.has_value());
			if (l_name == "compact") {
				CHECK(l_old_decoded.value().m_sender_name.empty());
			}
			// A name longer than any string a codec accepts is refused
			Envelope l_long = message_with_sender_name();
			l_long.m_sender_name.assign(ENVELOPE_MAX_STRING_BYTES + 1, 'x');
			CHECK_FALSE(l_codec->decode(encode(*l_codec, l_long), MAX_BYTES).has_value());
		}
	}

	TEST_CASE("size of a bare message stays close to the micro-benchmark") {
		const Envelope l_envelope = empty_message();
		// compact: 4 header + fixed scalars + four UUID strings, around 220 bytes; protobuf omits the
		// defaults and prefixes with varints: smaller still; json spells the names out.
		CHECK(encode(CompactCodec(), l_envelope).size() < 260);
		CHECK(encode(ProtobufCodec(), l_envelope).size() < 200);
		CHECK(encode(JsonCodec(), l_envelope).size() < 700);
	}

	TEST_CASE("protobuf: proto3 wire format details") {
		const ProtobufCodec l_codec;
		const Envelope l_original = message_by_id();
		const auto l_good = encode(l_codec, l_original);

		SUBCASE("an unknown field is skipped") {
			auto l_bytes = l_good;
			// field 99, varint: tag = (99 << 3) | 0 = 792 → varint 0x98 0x06, value 1
			l_bytes.push_back(0x98); l_bytes.push_back(0x06); l_bytes.push_back(0x01);
			// field 100, length-delimited: tag (100 << 3) | 2 = 802 → varint 0xA2 0x06, length 3, three bytes
			l_bytes.push_back(0xA2); l_bytes.push_back(0x06); l_bytes.push_back(3); l_bytes.push_back('a'); l_bytes.push_back('b'); l_bytes.push_back('c');
			const auto l_decoded = l_codec.decode(l_bytes, MAX_BYTES);
			REQUIRE_MESSAGE(l_decoded.has_value(), (l_decoded ? std::string() : l_decoded.error().m_message));
			CHECK(to_json(l_decoded.value()) == to_json(l_original));
		}
		SUBCASE("a wrong wire type is refused") {
			auto l_bytes = l_good;
			l_bytes.push_back((4 << 3) | 0);  // sender_environment as a varint
			l_bytes.push_back(1);
			CHECK_FALSE(l_codec.decode(l_bytes, MAX_BYTES));
		}
		SUBCASE("a group is refused") {
			auto l_bytes = l_good;
			l_bytes.push_back((30 << 3) | 3);  // an unknown field as a start-group
			CHECK_FALSE(l_codec.decode(l_bytes, MAX_BYTES));
		}
		SUBCASE("an enum out of range is refused by the validation") {
			auto l_bytes = l_good;
			l_bytes.push_back((3 << 3) | 0);  // kind, last occurrence wins
			l_bytes.push_back(200); l_bytes.push_back(1);
			const auto l_result = l_codec.decode(l_bytes, MAX_BYTES);
			REQUIRE_FALSE(l_result);
			CHECK(l_result.error().m_code == ErrorCode::InvalidEnvelope);
		}
		SUBCASE("a missing version is refused") {
			// Everything but field 1: rebuild from the sample with version 0, which proto3 omits.
			Envelope l_unversioned = l_original;
			l_unversioned.m_version_major = 0;
			const auto l_bytes = l_codec.encode(l_unversioned);
			REQUIRE(l_bytes);
			const auto l_result = l_codec.decode(l_bytes.value(), MAX_BYTES);
			REQUIRE_FALSE(l_result);
			CHECK(l_result.error().m_code == ErrorCode::UnsupportedVersion);
		}
		SUBCASE("defaults are omitted") {
			// A Raw empty content writes neither the encoding (0) nor the content (empty): the bare message
			// is just the header, the version, the kind and the four identifiers with their tags.
			const auto l_empty = encode(l_codec, empty_message());
			CHECK(l_empty.size() < l_good.size());
			CHECK(l_empty.size() <= ENVELOPE_HEADER_SIZE + 2 + 2 + 4 * (2 + 36) + 2 + 6);  // version, kind, four identifiers, epoch, sequence: nothing else
		}
	}

	TEST_CASE("json: readable envelope") {
		const JsonCodec l_codec;
		const auto l_bytes = encode(l_codec, text_message());
		const std::string l_text(l_bytes.begin() + ENVELOPE_HEADER_SIZE, l_bytes.end());
		CHECK(l_text.find("\"content_text\":\"hello in clear\"") != std::string::npos);
		CHECK(l_text.find("\"kind\":\"message\"") != std::string::npos);
		CHECK(l_text.find("\"content_encoding\":\"text\"") != std::string::npos);

		SUBCASE("invalid JSON is refused") {
			auto l_cut = l_bytes;
			l_cut.resize(l_cut.size() - 1);
			CHECK_FALSE(l_codec.decode(l_cut, MAX_BYTES));
		}
		SUBCASE("an unknown enum name is refused") {
			std::string l_bad = l_text;
			l_bad.replace(l_bad.find("\"kind\":\"message\""), 16, "\"kind\":\"rumour\"");
			std::vector<std::uint8_t> l_frame(l_bytes.begin(), l_bytes.begin() + ENVELOPE_HEADER_SIZE);
			l_frame.insert(l_frame.end(), l_bad.begin(), l_bad.end());
			CHECK_FALSE(l_codec.decode(l_frame, MAX_BYTES));
		}
		SUBCASE("unknown members are ignored and absent members take their default") {
			const std::string l_minimal = "{\"version_major\":1,\"kind\":\"message\",\"sender_environment\":\"" + ENV_A + "\",\"stream_target\":\"" + ENV_B +
										  "\",\"sender_agent\":\"" + AGENT_1 + "\",\"target_id\":\"" + AGENT_2 + "\",\"future_member\":{\"x\":1}}";
			std::vector<std::uint8_t> l_frame(l_bytes.begin(), l_bytes.begin() + ENVELOPE_HEADER_SIZE);
			l_frame.insert(l_frame.end(), l_minimal.begin(), l_minimal.end());
			const auto l_decoded = l_codec.decode(l_frame, MAX_BYTES);
			REQUIRE_MESSAGE(l_decoded.has_value(), (l_decoded ? std::string() : l_decoded.error().m_message));
			CHECK(l_decoded.value().m_sequence == 0);
			CHECK(l_decoded.value().m_content.empty());
		}
		SUBCASE("bad base64 is refused") {
			const std::string l_bad = "{\"version_major\":1,\"kind\":\"message\",\"sender_environment\":\"" + ENV_A + "\",\"stream_target\":\"" + ENV_B +
									  "\",\"sender_agent\":\"" + AGENT_1 + "\",\"target_id\":\"" + AGENT_2 + "\",\"content\":\"not*base64\"}";
			std::vector<std::uint8_t> l_frame(l_bytes.begin(), l_bytes.begin() + ENVELOPE_HEADER_SIZE);
			l_frame.insert(l_frame.end(), l_bad.begin(), l_bad.end());
			CHECK_FALSE(l_codec.decode(l_frame, MAX_BYTES));
		}
	}

	TEST_CASE("every codec refuses a bad header, a foreign codec id, a wrong version and garbage") {
		for (const auto& l_name: codec_names()) {
			CAPTURE(l_name);
			const auto l_codec = make_codec(l_name);
			const auto l_good = encode(*l_codec, message_by_id());
			auto l_magic = l_good;
			l_magic[0] = 0xCB;
			CHECK_FALSE(l_codec->decode(l_magic, MAX_BYTES));
			auto l_other = l_good;
			l_other[1] = static_cast<std::uint8_t>(l_codec->id() + 1);
			const auto l_other_result = l_codec->decode(l_other, MAX_BYTES);
			REQUIRE_FALSE(l_other_result);
			CHECK(l_other_result.error().m_code == ErrorCode::UnsupportedCodec);
			auto l_version = l_good;
			l_version[2] = 2;
			const auto l_version_result = l_codec->decode(l_version, MAX_BYTES);
			REQUIRE_FALSE(l_version_result);
			CHECK(l_version_result.error().m_code == ErrorCode::UnsupportedVersion);
			const auto l_large = l_codec->decode(l_good, l_good.size() - 1);
			REQUIRE_FALSE(l_large);
			CHECK(l_large.error().m_code == ErrorCode::TooLarge);
			// A cut inside the first string field never decodes; compact, with its fixed layout, refuses any
			// cut, while a proto3 or JSON prefix ending on a field boundary may still be a valid envelope.
			const std::vector<std::uint8_t> l_inside(l_good.begin(), l_good.begin() + ENVELOPE_HEADER_SIZE + 8);
			CHECK_FALSE(l_codec->decode(l_inside, MAX_BYTES));
			for (std::size_t l_size = 0; l_size < l_good.size(); l_size += 7) {
				const std::vector<std::uint8_t> l_cut(l_good.begin(), l_good.begin() + static_cast<std::ptrdiff_t>(l_size));
				const auto l_result = l_codec->decode(l_cut, MAX_BYTES);
				if (l_name == "compact") {
					CHECK_FALSE(l_result);
				} else if (l_result) {
					CHECK(validate_envelope(l_result.value(), MAX_BYTES));
				}
			}
			std::uint32_t l_seed = 12345;
			for (int i = 0; i < 1000; i++) {
				std::vector<std::uint8_t> l_bytes(static_cast<std::size_t>(i % 300));
				for (auto& l_byte: l_bytes) {
					l_seed = l_seed * 1103515245u + 12345u;
					l_byte = static_cast<std::uint8_t>(l_seed >> 16);
				}
				if (l_bytes.size() >= 3) {
					l_bytes[0] = ENVELOPE_MAGIC;
					l_bytes[1] = l_codec->id();
					l_bytes[2] = l_codec->version_major();
				}
				(void) l_codec->decode(l_bytes, MAX_BYTES);
			}
		}
	}

	TEST_CASE("compact: malformed input is refused with a validation error") {
		const CompactCodec l_codec;
		const auto l_good = encode(l_codec, message_by_id());

		SUBCASE("truncated anywhere") {
			for (std::size_t l_size = 0; l_size < l_good.size(); l_size += 7) {
				const std::vector<std::uint8_t> l_cut(l_good.begin(), l_good.begin() + static_cast<std::ptrdiff_t>(l_size));
				const auto l_result = l_codec.decode(l_cut, MAX_BYTES);
				CHECK_FALSE(l_result);
			}
		}
		SUBCASE("trailing bytes") {
			auto l_bytes = l_good;
			l_bytes.push_back(0);
			const auto l_result = l_codec.decode(l_bytes, MAX_BYTES);
			REQUIRE_FALSE(l_result);
			CHECK(l_result.error().m_code == ErrorCode::InvalidEnvelope);
		}
		SUBCASE("bad magic") {
			auto l_bytes = l_good;
			l_bytes[0] = 0xCB;
			CHECK_FALSE(l_codec.decode(l_bytes, MAX_BYTES));
		}
		SUBCASE("other codec id") {
			auto l_bytes = l_good;
			l_bytes[1] = 2;
			const auto l_result = l_codec.decode(l_bytes, MAX_BYTES);
			REQUIRE_FALSE(l_result);
			CHECK(l_result.error().m_code == ErrorCode::UnsupportedCodec);
		}
		SUBCASE("other major version") {
			auto l_bytes = l_good;
			l_bytes[2] = 2;
			const auto l_result = l_codec.decode(l_bytes, MAX_BYTES);
			REQUIRE_FALSE(l_result);
			CHECK(l_result.error().m_code == ErrorCode::UnsupportedVersion);
		}
		SUBCASE("unknown enum values") {
			auto l_bytes = l_good;
			l_bytes[5] = 200;  // kind
			CHECK_FALSE(l_codec.decode(l_bytes, MAX_BYTES));
		}
		SUBCASE("larger than the configured maximum") {
			const auto l_result = l_codec.decode(l_good, l_good.size() - 1);
			REQUIRE_FALSE(l_result);
			CHECK(l_result.error().m_code == ErrorCode::TooLarge);
		}
		SUBCASE("absurd string length") {
			auto l_bytes = l_good;
			// first string: sender environment, length at offset 6
			l_bytes[6] = 0xFF; l_bytes[7] = 0xFF; l_bytes[8] = 0xFF; l_bytes[9] = 0x7F;
			const auto l_result = l_codec.decode(l_bytes, MAX_BYTES);
			REQUIRE_FALSE(l_result);
			CHECK(l_result.error().m_code == ErrorCode::TooLarge);
		}
		SUBCASE("unknown payload tag") {
			// Each byte in turn takes a value that no tag has: the one of the payload tag is refused by name, wherever it is
			bool l_refused_tag = false;
			for (std::size_t i = 3; i < l_good.size(); i++) {
				auto l_bytes = l_good;
				l_bytes[i] = 0xEE;
				const auto l_result = l_codec.decode(l_bytes, MAX_BYTES);
				if (!l_result && l_result.error().m_message.find("unknown payload tag") != std::string::npos) {
					l_refused_tag = true;
				}
			}
			CHECK(l_refused_tag);
		}
		SUBCASE("random garbage never crashes") {
			std::uint32_t l_seed = 12345;
			for (int i = 0; i < 2000; i++) {
				std::vector<std::uint8_t> l_bytes(static_cast<std::size_t>(i % 300));
				for (auto& l_byte: l_bytes) {
					l_seed = l_seed * 1103515245u + 12345u;
					l_byte = static_cast<std::uint8_t>(l_seed >> 16);
				}
				if (l_bytes.size() >= 3) {
					l_bytes[0] = ENVELOPE_MAGIC;
					l_bytes[1] = CompactCodec::ID;
					l_bytes[2] = CompactCodec::VERSION_MAJOR;
				}
				(void) l_codec.decode(l_bytes, MAX_BYTES);
			}
		}
	}

	TEST_CASE("validation: identifiers and consistency") {
		Envelope l_envelope = message_by_id();
		CHECK(validate_envelope(l_envelope, MAX_BYTES));

		SUBCASE("empty sender environment") {
			l_envelope.m_sender_environment.clear();
			CHECK_FALSE(validate_envelope(l_envelope, MAX_BYTES));
		}
		SUBCASE("sender with a slash") {
			l_envelope.m_sender_environment = "a/b";
			CHECK_FALSE(validate_envelope(l_envelope, MAX_BYTES));
		}
		SUBCASE("stream target on the all stream") {
			l_envelope.m_stream_kind = StreamKind::All;
			CHECK_FALSE(validate_envelope(l_envelope, MAX_BYTES));
		}
		SUBCASE("missing stream target on a unicast stream") {
			l_envelope.m_stream_target.clear();
			CHECK_FALSE(validate_envelope(l_envelope, MAX_BYTES));
		}
		SUBCASE("message by name without a name") {
			l_envelope.m_target_kind = TargetKind::Name;
			CHECK_FALSE(validate_envelope(l_envelope, MAX_BYTES));
		}
		SUBCASE("message carrying a typed payload") {
			l_envelope.m_payload = AgentControl{"t", AGENT_1, true, ""};
			CHECK_FALSE(validate_envelope(l_envelope, MAX_BYTES));
		}
		SUBCASE("content over the maximum") {
			CHECK_FALSE(validate_envelope(l_envelope, l_envelope.m_content.size() - 1));
		}
		SUBCASE("control without payload") {
			l_envelope.m_kind = EnvelopeKind::AgentCommit;
			CHECK_FALSE(validate_envelope(l_envelope, MAX_BYTES));
		}
		SUBCASE("presence with an invalid namespace") {
			Envelope l_presence = presence_online();
			std::get<Presence>(l_presence.m_payload).m_namespace = "Lab 1";
			CHECK_FALSE(validate_envelope(l_presence, MAX_BYTES));
		}
		SUBCASE("another major version") {
			l_envelope.m_version_major = 2;
			const auto l_result = validate_envelope(l_envelope, MAX_BYTES);
			REQUIRE_FALSE(l_result);
			CHECK(l_result.error().m_code == ErrorCode::UnsupportedVersion);
		}
		SUBCASE("kinds, scope and encoding out of their enumerations") {
			// Values a decoder may read from the wire, beyond the last one of each enumeration
			Envelope l_kind = l_envelope;
			l_kind.m_kind = static_cast<EnvelopeKind>(200);
			CHECK_FALSE(validate_envelope(l_kind, MAX_BYTES));
			Envelope l_stream = l_envelope;
			l_stream.m_stream_kind = static_cast<StreamKind>(200);
			CHECK_FALSE(validate_envelope(l_stream, MAX_BYTES));
			Envelope l_target = l_envelope;
			l_target.m_target_kind = static_cast<TargetKind>(200);
			CHECK_FALSE(validate_envelope(l_target, MAX_BYTES));
			Envelope l_scope = l_envelope;
			l_scope.m_scope = static_cast<Scope>(200);
			CHECK_FALSE(validate_envelope(l_scope, MAX_BYTES));
			Envelope l_encoding = l_envelope;
			l_encoding.m_content_encoding = static_cast<cam::ContentEncoding>(200);
			CHECK_FALSE(validate_envelope(l_encoding, MAX_BYTES));
		}
		SUBCASE("origin environment with a slash") {
			l_envelope.m_origin_environment = "a/b";
			CHECK_FALSE(validate_envelope(l_envelope, MAX_BYTES));
		}
		SUBCASE("message without its sender agent") {
			l_envelope.m_sender_agent.clear();
			CHECK_FALSE(validate_envelope(l_envelope, MAX_BYTES));
		}
		SUBCASE("message by id without a target") {
			l_envelope.m_target_id.clear();
			CHECK_FALSE(validate_envelope(l_envelope, MAX_BYTES));
		}
		SUBCASE("message to an environment without its identifier") {
			l_envelope.m_target_kind = TargetKind::Environment;
			l_envelope.m_target_id = "a/b";
			CHECK_FALSE(validate_envelope(l_envelope, MAX_BYTES));
		}
	}

	TEST_CASE("identifiers and keys") {
		CHECK(is_valid_namespace("default"));
		CHECK(is_valid_namespace("lab-1"));
		CHECK_FALSE(is_valid_namespace(""));
		CHECK_FALSE(is_valid_namespace("Lab"));
		CHECK_FALSE(is_valid_namespace("a/b"));
		CHECK_FALSE(is_valid_namespace(std::string(33, 'a')));
		CHECK(is_valid_identifier(ENV_A));
		CHECK(is_valid_identifier("ext"));
		CHECK_FALSE(is_valid_identifier(""));
		CHECK_FALSE(is_valid_identifier("a b"));
		CHECK_FALSE(is_valid_identifier("a+b"));
		CHECK_FALSE(is_valid_identifier("a#b"));
		CHECK_FALSE(is_valid_identifier("a*b"));
		CHECK(is_canonical_uuid(ENV_A));
		CHECK_FALSE(is_canonical_uuid("0f3c9d6e1a2b4c3d8e9fa0b1c2d3e4f5"));

		const Address l_in{AddressKind::EnvIn, "lab-1", ENV_B};
		CHECK(l_in.to_key() == "cam/v1/lab-1/env/" + ENV_B + "/in");
		CHECK(Address{AddressKind::EnvAgent, "default", ENV_B}.to_key() == "cam/v1/default/env/" + ENV_B + "/agent");
		CHECK(Address{AddressKind::EnvQuery, "default", ENV_B}.to_key() == "cam/v1/default/env/" + ENV_B + "/query");
		CHECK(Address{AddressKind::EnvPresence, "default", ENV_B}.to_key() == "cam/v1/default/env/" + ENV_B + "/presence");
		CHECK(Address{AddressKind::AllIn, "default", ""}.to_key() == "cam/v1/default/all/in");
		CHECK(Address{AddressKind::NodePresence, "default", NODE}.to_key() == "cam/v1/default/node/" + NODE + "/presence");
		for (const AddressKind l_kind: {AddressKind::EnvIn, AddressKind::EnvAgent, AddressKind::EnvQuery, AddressKind::EnvPresence,
										AddressKind::AllIn, AddressKind::NodePresence}) {
			const Address l_address{l_kind, "lab-1", l_kind == AddressKind::AllIn ? std::string() : ENV_A};
			const auto l_parsed = Address::parse(l_address.to_key());
			REQUIRE(l_parsed);
			CHECK(*l_parsed == l_address);
		}
		CHECK_FALSE(Address::parse("cam/v2/default/all/in"));
		CHECK_FALSE(Address::parse("cam/v1/default/env/x"));
		CHECK_FALSE(Address::parse("cam/v1/default/env/x/other"));
		CHECK_FALSE(Address::parse("cam/v1/Default/all/in"));
		CHECK_FALSE(Address::parse(""));
	}
}
