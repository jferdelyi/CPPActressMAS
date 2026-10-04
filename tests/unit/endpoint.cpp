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

// Endpoint tests without any environment: sequences, deduplication, bounded reordering, epochs,
// overflows, decoding at the turn boundary, directory and presence tables, counters.

#include <doctest/doctest.h>

#include <transport/CompactCodec.h>
#include <transport/LoopbackTransport.h>

namespace {

	using namespace cam::transport;

	const std::string ME = "aaaaaaaa-0000-4000-8000-000000000001";
	const std::string PEER = "bbbbbbbb-0000-4000-8000-000000000002";
	const std::string OTHER = "cccccccc-0000-4000-8000-000000000003";
	const std::string NODE = "dddddddd-0000-4000-8000-000000000004";

	// A message from PEER to ME on the unicast stream, with a given epoch and sequence.
	Envelope message(const std::uint32_t p_epoch, const std::uint64_t p_sequence, const std::string& p_sender = PEER) {
		Envelope l_envelope;
		l_envelope.m_kind = EnvelopeKind::Message;
		l_envelope.m_sender_environment = p_sender;
		l_envelope.m_epoch = p_epoch;
		l_envelope.m_stream_kind = StreamKind::In;
		l_envelope.m_stream_target = ME;
		l_envelope.m_sequence = p_sequence;
		l_envelope.m_sender_agent = "sender";
		l_envelope.m_target_kind = TargetKind::Id;
		l_envelope.m_target_id = "receiver";
		l_envelope.m_content_encoding = cam::MessageBinaryFormat::RAW;
		l_envelope.m_content = {static_cast<std::uint8_t>(p_sequence)};
		return l_envelope;
	}

	Envelope presence(const std::string& p_environment, const bool p_online, const std::uint64_t p_sequence) {
		Envelope l_envelope;
		l_envelope.m_kind = EnvelopeKind::Presence;
		l_envelope.m_sender_environment = p_environment;
		l_envelope.m_epoch = 1;
		l_envelope.m_stream_kind = StreamKind::Presence;
		l_envelope.m_stream_target = p_environment;
		l_envelope.m_sequence = p_sequence;
		Presence l_presence;
		l_presence.m_node_id = NODE;
		l_presence.m_environment_id = p_environment;
		l_presence.m_environment_name = "peer";
		l_presence.m_namespace = "default";
		l_presence.m_online = p_online;
		l_presence.m_agents = {{"agent-x", "x"}};
		l_envelope.m_payload = std::move(l_presence);
		return l_envelope;
	}

	Address in_address(const std::string& p_target = ME) {
		return Address{AddressKind::EnvIn, "default", p_target};
	}

	std::vector<std::uint64_t> sequences(const std::vector<Envelope>& p_envelopes) {
		std::vector<std::uint64_t> l_result;
		for (const auto& l_envelope: p_envelopes) {
			l_result.push_back(l_envelope.m_sequence);
		}
		return l_result;
	}

	std::shared_ptr<LoopbackTransport> open_loopback(LoopbackOptions p_options = {}) {
		auto l_transport = std::make_shared<LoopbackTransport>(TransportConfig{}, std::move(p_options));
		REQUIRE(l_transport->open());
		return l_transport;
	}

} // namespace

TEST_SUITE("endpoint") {

	TEST_CASE("sending: not attached, sequences per destination key, stamping") {
		Endpoint l_sender(PEER, EndpointConfig{.m_name = "peer"});
		CHECK_FALSE(l_sender.is_attached());
		CHECK(l_sender.ns() == "default");

		Envelope l_envelope = l_sender.make_envelope(EnvelopeKind::Message);
		CHECK(l_envelope.m_sender_environment == PEER);
		CHECK(l_envelope.m_epoch == l_sender.epoch());
		CHECK(l_sender.epoch() != 0);

		const auto l_result = l_sender.enqueue(in_address(), std::move(l_envelope));
		REQUIRE_FALSE(l_result);
		CHECK(l_result.error().m_code == ErrorCode::NotAttached);

		auto l_transport = open_loopback();
		Endpoint l_receiver(ME, EndpointConfig{.m_name = "me"});
		REQUIRE(l_receiver.attach(l_transport));
		REQUIRE(l_sender.attach(l_transport));
		CHECK(l_sender.is_attached());

		for (int i = 0; i < 3; i++) {
			Envelope l_message = message(0, 0);
			l_message.m_sender_environment = "overwritten";
			REQUIRE(l_sender.enqueue(in_address(), std::move(l_message)));
		}
		Envelope l_other = message(0, 0);
		REQUIRE(l_sender.enqueue(in_address(OTHER), std::move(l_other)));  // another key: its own sequence
		CHECK(l_sender.flush_outbox().empty());
		CHECK(l_sender.statistics().m_sent == 4);

		const auto l_received = l_receiver.drain();
		REQUIRE(l_received.size() == 3);
		CHECK(sequences(l_received) == std::vector<std::uint64_t>{1, 2, 3});
		for (const auto& l_message: l_received) {
			CHECK(l_message.m_sender_environment == PEER);
			CHECK(l_message.m_epoch == l_sender.epoch());
			CHECK(l_message.m_stream_kind == StreamKind::In);
			CHECK(l_message.m_stream_target == ME);
		}
		CHECK(l_receiver.statistics().m_received == 3);
		CHECK(l_receiver.statistics().m_delivered == 3);
		REQUIRE(l_sender.detach());
		REQUIRE(l_receiver.detach());
		CHECK_FALSE(l_sender.detach());
	}

	TEST_CASE("sending: outbox bounded") {
		auto l_transport = open_loopback();
		Endpoint l_sender(PEER, EndpointConfig{.m_outbox_capacity = 2});
		REQUIRE(l_sender.attach(l_transport));
		REQUIRE(l_sender.enqueue(in_address(), message(0, 0)));
		REQUIRE(l_sender.enqueue(in_address(), message(0, 0)));
		const auto l_third = l_sender.enqueue(in_address(), message(0, 0));
		REQUIRE_FALSE(l_third);
		CHECK(l_third.error().m_code == ErrorCode::OutboxFull);
		CHECK(l_sender.statistics().m_outbox_overflow == 1);
		REQUIRE(l_sender.detach());
	}

	TEST_CASE("receiving: in order, reordered within the window, duplicates") {
		Endpoint l_endpoint(ME, EndpointConfig{});

		SUBCASE("in order") {
			for (std::uint64_t i = 1; i <= 3; i++) {
				CHECK(l_endpoint.deliver(in_address(), message(1, i)));
			}
			CHECK(sequences(l_endpoint.drain()) == std::vector<std::uint64_t>{1, 2, 3});
			CHECK(l_endpoint.statistics().m_duplicates == 0);
			CHECK(l_endpoint.statistics().m_lost == 0);
		}
		SUBCASE("reordered") {
			l_endpoint.deliver(in_address(), message(1, 1));
			l_endpoint.deliver(in_address(), message(1, 3));
			l_endpoint.deliver(in_address(), message(1, 2));
			CHECK(sequences(l_endpoint.drain()) == std::vector<std::uint64_t>{1, 2, 3});
			CHECK(l_endpoint.statistics().m_lost == 0);
		}
		SUBCASE("duplicates in the same drain and later") {
			l_endpoint.deliver(in_address(), message(1, 1));
			l_endpoint.deliver(in_address(), message(1, 1));
			l_endpoint.deliver(in_address(), message(1, 2));
			CHECK(sequences(l_endpoint.drain()) == std::vector<std::uint64_t>{1, 2});
			CHECK(l_endpoint.statistics().m_duplicates == 1);
			l_endpoint.deliver(in_address(), message(1, 1));
			CHECK(l_endpoint.drain().empty());
			CHECK(l_endpoint.statistics().m_duplicates == 2);
		}
		SUBCASE("the first sequence seen starts the flow") {
			l_endpoint.deliver(in_address(), message(1, 50));
			l_endpoint.deliver(in_address(), message(1, 51));
			CHECK(sequences(l_endpoint.drain()) == std::vector<std::uint64_t>{50, 51});
		}
	}

	TEST_CASE("receiving: a gap is released after the configured turns and counted as lost") {
		Endpoint l_endpoint(ME, EndpointConfig{.m_reorder_turns = 2});
		l_endpoint.deliver(in_address(), message(1, 1));
		l_endpoint.deliver(in_address(), message(1, 3));
		CHECK(sequences(l_endpoint.drain()) == std::vector<std::uint64_t>{1});
		CHECK(l_endpoint.drain().empty());
		CHECK(sequences(l_endpoint.drain()) == std::vector<std::uint64_t>{3});
		CHECK(l_endpoint.statistics().m_lost == 1);
		// The flow continues from there.
		l_endpoint.deliver(in_address(), message(1, 4));
		CHECK(sequences(l_endpoint.drain()) == std::vector<std::uint64_t>{4});
		// A late arrival of the lost one is delivered once, out of order, and never twice.
		l_endpoint.deliver(in_address(), message(1, 2));
		l_endpoint.deliver(in_address(), message(1, 2));
		CHECK(sequences(l_endpoint.drain()) == std::vector<std::uint64_t>{2});
		// Out of order: 3 released ahead of the missing 2, then 2 arriving after 3 and 4.
		CHECK(l_endpoint.statistics().m_out_of_order == 2);
		CHECK(l_endpoint.statistics().m_duplicates == 1);
	}

	TEST_CASE("receiving: the reorder buffer is bounded") {
		Endpoint l_endpoint(ME, EndpointConfig{.m_reorder_capacity = 2});
		l_endpoint.deliver(in_address(), message(1, 1));
		l_endpoint.deliver(in_address(), message(1, 3));
		l_endpoint.deliver(in_address(), message(1, 4));
		l_endpoint.deliver(in_address(), message(1, 5));
		CHECK(sequences(l_endpoint.drain()) == std::vector<std::uint64_t>{1, 3, 4, 5});
		CHECK(l_endpoint.statistics().m_lost == 1);
	}

	TEST_CASE("receiving: epochs are separate flows, echoes are dropped") {
		Endpoint l_endpoint(ME, EndpointConfig{});
		l_endpoint.deliver(in_address(), message(1, 5));
		l_endpoint.deliver(in_address(), message(2, 1));
		l_endpoint.deliver(in_address(), message(1, 6, ME));  // from myself
		auto l_sequences = sequences(l_endpoint.drain());
		std::sort(l_sequences.begin(), l_sequences.end());  // no order between flows
		CHECK(l_sequences == std::vector<std::uint64_t>{1, 5});
		CHECK(l_endpoint.statistics().m_echo == 1);
	}

	TEST_CASE("receiving: inbox bounded, invalid envelopes rejected") {
		Endpoint l_endpoint(ME, EndpointConfig{.m_inbox_capacity = 2});
		CHECK(l_endpoint.deliver(in_address(), message(1, 1)));
		CHECK(l_endpoint.deliver(in_address(), message(1, 2)));
		CHECK_FALSE(l_endpoint.deliver(in_address(), message(1, 3)));
		CHECK(l_endpoint.drain().size() == 2);
		CHECK(l_endpoint.statistics().m_inbox_overflow == 1);

		Envelope l_bad = message(1, 3);
		l_bad.m_sender_environment.clear();
		CHECK(l_endpoint.deliver(in_address(), std::move(l_bad)));
		CHECK(l_endpoint.drain().empty());
		CHECK(l_endpoint.statistics().m_rejected == 1);
	}

	TEST_CASE("receiving: bytes are decoded with the transport codec at the turn boundary") {
		const CompactCodec l_codec;
		Endpoint l_endpoint(ME, EndpointConfig{});
		auto l_encoded = l_codec.encode(message(1, 1));
		REQUIRE(l_encoded);

		SUBCASE("without a transport, bytes cannot be decoded") {
			std::vector<std::uint8_t> l_copy = l_encoded.value();
			CHECK(l_endpoint.deliver(in_address(), std::move(l_copy)));
			CHECK(l_endpoint.drain().empty());
			CHECK(l_endpoint.statistics().m_rejected == 1);
		}
		SUBCASE("with a transport") {
			auto l_transport = open_loopback();
			REQUIRE(l_endpoint.attach(l_transport));
			std::vector<std::uint8_t> l_copy = l_encoded.value();
			CHECK(l_endpoint.deliver(in_address(), std::move(l_copy)));
			CHECK(l_endpoint.deliver(in_address(), std::vector<std::uint8_t>{1, 2, 3}));
			CHECK(sequences(l_endpoint.drain()) == std::vector<std::uint64_t>{1});
			CHECK(l_endpoint.statistics().m_rejected == 1);
			REQUIRE(l_endpoint.detach());
		}
	}

	TEST_CASE("receiving: control and presence come before messages") {
		Endpoint l_endpoint(ME, EndpointConfig{});
		l_endpoint.deliver(in_address(), message(1, 1));
		l_endpoint.deliver(Address{AddressKind::EnvPresence, "default", PEER}, presence(PEER, true, 1));
		const auto l_ready = l_endpoint.drain();
		REQUIRE(l_ready.size() == 2);
		CHECK(l_ready[0].m_kind == EnvelopeKind::Presence);
		CHECK(l_ready[1].m_kind == EnvelopeKind::Message);
	}

	TEST_CASE("directory and presence tables") {
		Endpoint l_endpoint(ME, EndpointConfig{});
		CHECK_FALSE(l_endpoint.locate_agent("a"));
		l_endpoint.set_agent_location("a", PEER);
		l_endpoint.set_agent_location("b", OTHER);
		CHECK(l_endpoint.locate_agent("a") == PEER);
		l_endpoint.remove_agent("a");
		CHECK_FALSE(l_endpoint.locate_agent("a"));

		const auto l_online = std::get<Presence>(presence(PEER, true, 1).m_payload);
		l_endpoint.update_environment(l_online);
		CHECK(l_endpoint.knows_environment(PEER));
		CHECK(l_endpoint.locate_agent("agent-x") == PEER);
		const auto l_environments = l_endpoint.environments();
		REQUIRE(l_environments.size() == 1);
		CHECK(l_environments[0].m_id == PEER);
		CHECK(l_environments[0].m_name == "peer");
		CHECK(l_environments[0].m_node == NODE);
		CHECK(l_environments[0].m_online);

		const auto l_offline = std::get<Presence>(presence(PEER, false, 2).m_payload);
		l_endpoint.update_environment(l_offline);
		CHECK_FALSE(l_endpoint.knows_environment(PEER));
		CHECK_FALSE(l_endpoint.locate_agent("agent-x"));
		CHECK(l_endpoint.locate_agent("b") == OTHER);
		CHECK_FALSE(l_endpoint.environments()[0].m_online);

		CHECK_FALSE(l_endpoint.previous_host("a"));
		l_endpoint.set_previous_host("a", OTHER);
		CHECK(l_endpoint.previous_host("a") == OTHER);
	}

	TEST_CASE("loopback: namespace, capabilities, retained presence, injections") {
		SUBCASE("invalid namespace refused at open") {
			LoopbackTransport l_transport(TransportConfig{.m_namespace = "Bad Name"});
			const auto l_result = l_transport.open();
			REQUIRE_FALSE(l_result);
			CHECK(l_result.error().m_code == ErrorCode::InvalidConfiguration);
		}
		SUBCASE("attach needs an open transport; publish outside the namespace is refused") {
			auto l_transport = std::make_shared<LoopbackTransport>(TransportConfig{.m_namespace = "lab-1"});
			Endpoint l_endpoint(ME, EndpointConfig{});
			CHECK_FALSE(l_endpoint.attach(l_transport));
			REQUIRE(l_transport->open());
			REQUIRE(l_endpoint.attach(l_transport));
			CHECK(l_endpoint.ns() == "lab-1");
			CHECK(l_endpoint.address(AddressKind::AllIn).to_key() == "cam/v1/lab-1/all/in");
			const auto l_result = l_transport->publish(l_endpoint, in_address(PEER), message(1, 1));
			REQUIRE_FALSE(l_result);
			CHECK(l_result.error().m_code == ErrorCode::InvalidTarget);
			CHECK(l_transport->capabilities().m_pubsub);
			CHECK(l_transport->capabilities().m_retained_presence);
			CHECK(l_transport->codec()->name() == std::string("compact"));
			CHECK(is_canonical_uuid(l_transport->node_id()));
			REQUIRE(l_endpoint.detach());
		}
		SUBCASE("retained presence reaches a newcomer, offline presence clears it") {
			auto l_transport = open_loopback();
			Endpoint l_peer(PEER, EndpointConfig{});
			REQUIRE(l_peer.attach(l_transport));
			REQUIRE(l_peer.enqueue(Address{AddressKind::EnvPresence, "default", PEER}, presence(PEER, true, 0)));
			CHECK(l_peer.flush_outbox().empty());

			Endpoint l_me(ME, EndpointConfig{});
			REQUIRE(l_me.attach(l_transport));
			auto l_ready = l_me.drain();
			REQUIRE(l_ready.size() == 1);
			CHECK(l_ready[0].m_kind == EnvelopeKind::Presence);
			CHECK(std::get<Presence>(l_ready[0].m_payload).m_online);

			REQUIRE(l_peer.enqueue(Address{AddressKind::EnvPresence, "default", PEER}, presence(PEER, false, 0)));
			CHECK(l_peer.flush_outbox().empty());
			l_ready = l_me.drain();
			REQUIRE(l_ready.size() == 1);
			CHECK_FALSE(std::get<Presence>(l_ready[0].m_payload).m_online);

			Endpoint l_other(OTHER, EndpointConfig{});
			REQUIRE(l_other.attach(l_transport));
			CHECK(l_other.drain().empty());
			REQUIRE(l_peer.detach());
			REQUIRE(l_me.detach());
			REQUIRE(l_other.detach());
		}
		SUBCASE("drop, duplicate, reorder and hold injections") {
			std::size_t l_published = 0;
			LoopbackOptions l_options;
			l_options.m_duplicate_every_n = 1;
			l_options.m_hold_if = [](const Envelope& p_envelope) { return p_envelope.m_sequence == 2; };
			auto l_transport = open_loopback(l_options);
			Endpoint l_peer(PEER, EndpointConfig{});
			Endpoint l_me(ME, EndpointConfig{});
			REQUIRE(l_peer.attach(l_transport));
			REQUIRE(l_me.attach(l_transport));
			for (int i = 0; i < 3; i++) {
				REQUIRE(l_peer.enqueue(in_address(), message(0, 0)));
			}
			CHECK(l_peer.flush_outbox().empty());
			l_published = l_transport->published();
			CHECK(l_published == 3);
			CHECK(sequences(l_me.drain()) == std::vector<std::uint64_t>{1});  // 3 waits for 2, duplicates dropped
			CHECK(l_me.statistics().m_duplicates == 2);
			CHECK(l_transport->release_held() == 1);
			CHECK(sequences(l_me.drain()) == std::vector<std::uint64_t>{2, 3});
			CHECK(l_me.statistics().m_lost == 0);
			REQUIRE(l_peer.detach());
			REQUIRE(l_me.detach());
		}
		SUBCASE("reverse order within a window is repaired") {
			LoopbackOptions l_options;
			l_options.m_reorder_window = 3;
			auto l_transport = open_loopback(l_options);
			Endpoint l_peer(PEER, EndpointConfig{});
			Endpoint l_me(ME, EndpointConfig{});
			REQUIRE(l_peer.attach(l_transport));
			REQUIRE(l_me.attach(l_transport));
			for (int i = 0; i < 3; i++) {
				REQUIRE(l_peer.enqueue(in_address(), message(0, 0)));
			}
			CHECK(l_peer.flush_outbox().empty());
			CHECK_FALSE(l_transport->capabilities().m_ordered_per_sender);
			CHECK(sequences(l_me.drain()) == std::vector<std::uint64_t>{1, 2, 3});
			REQUIRE(l_peer.detach());
			REQUIRE(l_me.detach());
		}
		SUBCASE("drops are counted by the transport and seen as losses by the receiver") {
			LoopbackOptions l_options;
			l_options.m_drop_if = [](const Envelope& p_envelope) { return p_envelope.m_sequence == 1; };
			auto l_transport = open_loopback(l_options);
			Endpoint l_peer(PEER, EndpointConfig{.m_reorder_turns = 1});
			Endpoint l_me(ME, EndpointConfig{.m_reorder_turns = 1});
			REQUIRE(l_peer.attach(l_transport));
			REQUIRE(l_me.attach(l_transport));
			REQUIRE(l_peer.enqueue(in_address(), message(0, 0)));
			REQUIRE(l_peer.enqueue(in_address(), message(0, 0)));
			CHECK(l_peer.flush_outbox().empty());
			CHECK(l_transport->dropped() == 1);
			CHECK_FALSE(l_transport->capabilities().m_at_least_once);
			// The receiver never saw 1: its flow starts at 2, nothing is lost from its point of view.
			CHECK(sequences(l_me.drain()) == std::vector<std::uint64_t>{2});
			CHECK(l_me.statistics().m_lost == 0);
			REQUIRE(l_peer.detach());
			REQUIRE(l_me.detach());
		}
	}
}
