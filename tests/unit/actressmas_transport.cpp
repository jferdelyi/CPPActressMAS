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

// ActressMAS transport without any connection: the identifiers derived from the names of the containers and of their
// agents, the reading of "name@container", and the content of a message as the C# reads it and back.

#include <doctest/doctest.h>

#include <memory>
#include <string>
#include <vector>

#include <AgentId.h>
#include <transport/actressmas/ActressMasTransport.h>
#include <transport/codec/ContentCodec.h>

namespace {

	using cam::transport::ActressMasTransport;
	using cam::ContentEncoding;
	using cam::transport::Envelope;

	// A message whose content is encoded as the core encodes it
	Envelope message_with(const cam::MessageContent& p_content, const ContentEncoding p_encoding) {
		Envelope l_envelope;
		l_envelope.m_content_encoding = p_encoding;
		l_envelope.m_content = cam::transport::encode_content(p_content, p_encoding).value();
		return l_envelope;
	}

	// The content of a message, decoded
	cam::MessageContent content_of(const Envelope& p_envelope) {
		return cam::transport::decode_content(p_envelope.m_content, p_envelope.m_content_encoding).value();
	}

} // namespace

TEST_SUITE("actressmas transport") {

	TEST_CASE("the identifier of a container is a canonical UUID derived from its name") {
		const std::string l_container1 = ActressMasTransport::environment_id_of("Container1");
		CHECK(cam::transport::is_canonical_uuid(l_container1));
		CHECK(l_container1 == ActressMasTransport::environment_id_of("Container1"));
		CHECK(l_container1 != ActressMasTransport::environment_id_of("Container2"));
		CHECK(l_container1 != ActressMasTransport::environment_id_of("container1"));
		// The node of a container is not its environment: a presence would read as the one of a node
		CHECK(cam::transport::is_canonical_uuid(ActressMasTransport::node_id_of("Container1")));
		CHECK(l_container1 != ActressMasTransport::node_id_of("Container1"));
		// The same value on every platform, computed apart (FNV-1a, SplitMix64, version 8): every process derives it
		CHECK(l_container1 == "5b167ab7-2d7a-8c9b-adf0-5d60526d42fc");
	}

	TEST_CASE("the identifier of an agent of another container is an agent identifier derived from name@container") {
		const std::string l_ping = ActressMasTransport::agent_id_of("ping-agent@Container1");
		CHECK(l_ping.size() == cam::AgentId::CAPACITY);
		CHECK(cam::AgentId::parse(l_ping).has_value());
		CHECK(cam::transport::is_valid_identifier(l_ping));
		CHECK(l_ping == ActressMasTransport::agent_id_of("ping-agent@Container1"));
		CHECK(l_ping != ActressMasTransport::agent_id_of("ping-agent@Container2"));
		CHECK(l_ping != ActressMasTransport::agent_id_of("pong-agent@Container1"));
	}

	TEST_CASE("name@container splits at its last @") {
		const auto l_address = ActressMasTransport::split_address("pong-agent@Container2");
		REQUIRE(l_address.has_value());
		CHECK(l_address->first == "pong-agent");
		CHECK(l_address->second == "Container2");
		const auto l_two = ActressMasTransport::split_address("a@b@C");
		REQUIRE(l_two.has_value());
		CHECK(l_two->first == "a@b");
		CHECK(l_two->second == "C");
		CHECK_FALSE(ActressMasTransport::split_address("pong-agent").has_value());
		CHECK_FALSE(ActressMasTransport::split_address("@Container2").has_value());
		CHECK_FALSE(ActressMasTransport::split_address("pong-agent@").has_value());
	}

	TEST_CASE("the content travels as the text of the C#") {
		SUBCASE("a text stays a text") {
			const auto l_text = ActressMasTransport::text_of(message_with(std::string("request-info"), ContentEncoding::Text));
			REQUIRE(l_text);
			CHECK(l_text.value() == "request-info");
			Envelope l_back;
			ActressMasTransport::set_text(l_back, l_text.value());
			CHECK(l_back.m_content_encoding == ContentEncoding::Text);
			const auto l_content = content_of(l_back);
			REQUIRE(std::holds_alternative<std::string>(l_content));
			CHECK(std::get<std::string>(l_content) == "request-info");
		}
		SUBCASE("a JSON value travels as its text, and arrives as text") {
			const auto l_json = std::make_shared<const json>(json{{"turn", 3}});
			const auto l_text = ActressMasTransport::text_of(message_with(l_json, ContentEncoding::MessagePack));
			REQUIRE(l_text);
			CHECK(l_text.value() == "{\"turn\":3}");
		}
		SUBCASE("bytes travel as base64 and come back as the same bytes") {
			const std::vector<std::uint8_t> l_bytes = {0x00, 0x01, 0xFE, 0xFF, 0x41};
			const auto l_text = ActressMasTransport::text_of(message_with(l_bytes, ContentEncoding::Raw));
			REQUIRE(l_text);
			CHECK(l_text.value() == "base64:AAH+/0E=");
			Envelope l_back;
			ActressMasTransport::set_text(l_back, l_text.value());
			CHECK(l_back.m_content_encoding == ContentEncoding::Raw);
			const auto l_content = content_of(l_back);
			REQUIRE(std::holds_alternative<std::vector<std::uint8_t>>(l_content));
			CHECK(std::get<std::vector<std::uint8_t>>(l_content) == l_bytes);
		}
		SUBCASE("a text that only looks like bytes stays a text") {
			Envelope l_back;
			ActressMasTransport::set_text(l_back, "base64:not base64!");
			CHECK(l_back.m_content_encoding == ContentEncoding::Text);
		}
	}

}
