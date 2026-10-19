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

// Environments connected through the loopback transport. Each environment runs one turn per start()
// (constructed with p_no_turns = 1), so the tests interleave the turns of two environments
// deterministically on one thread: start() = turn boundary (inbox applied, agents inserted) + one turn
// + outbox flushed. Agents record into a Recorder and never assert.

#include <doctest/doctest.h>

#include <chrono>

#include <Environment.h>
#include <transport/codec/EnvelopeCodec.h>
#include <transport/loopback/LoopbackTransport.h>
#include <util/Identifier.h>

#include "Recorder.hpp"

namespace {

	using namespace cam::transport;
	using Mode = cam::EnvironmentMasMode;

	cam::test::Recorder g_recorder;

	const std::uint8_t* raw(const std::string& p_text) {
		return reinterpret_cast<const std::uint8_t*>(p_text.data());
	}

	// Records every message it receives as "<name>:<content>".
	struct Receiver final : cam::Agent {
		explicit Receiver(const std::string& p_name) : Agent(p_name) {}
		void action(const cam::MessagePointer& p_message) override {
			g_recorder.record(get_name() + ":" + p_message->to_string());
		}
	};

	// From its second turn on, sends one message by id until the send is accepted.
	struct Sender final : cam::Agent {
		Sender(const std::string& p_name, cam::AgentId p_target_id, std::string p_text = "hello") :
				Agent(p_name), m_target_id(p_target_id), m_text(std::move(p_text)) {}
		void default_action() override {
			if (m_sent) return;
			if (const auto l_result = send(m_target_id, raw(m_text), m_text.size()); l_result) {
				m_sent = true;
				g_recorder.record(get_name() + ":sent");
			} else {
				g_recorder.record(get_name() + ":" + to_string(l_result.error().m_code));
			}
		}
	private:
		cam::AgentId m_target_id;
		std::string m_text;
		bool m_sent = false;
	};

	// Records every message as "<name>:<kind>:<content>".
	struct TypedReceiver final : cam::Agent {
		explicit TypedReceiver(const std::string& p_name) : Agent(p_name) {}
		void action(const cam::MessagePointer& p_message) override {
			const char* l_kind = p_message->kind() == cam::ContentKind::Text ? "text"
								 : p_message->kind() == cam::ContentKind::Json ? "json"
								 : p_message->kind() == cam::ContentKind::Bytes ? "bytes" : "none";
			g_recorder.record(get_name() + ":" + l_kind + ":" + p_message->to_string());
		}
	};

	// From its second turn on, sends one shared text by id until the send is accepted.
	struct SharedTextSender final : cam::Agent {
		SharedTextSender(const std::string& p_name, cam::AgentId p_target_id, const std::string& p_text) :
				Agent(p_name), m_target_id(p_target_id), m_text(p_text) {}
		void default_action() override {
			if (m_sent) return;
			if (send(m_target_id, m_text)) {
				m_sent = true;
			}
		}
	private:
		cam::AgentId m_target_id;
		cam::Text m_text;
		bool m_sent = false;
	};

	// From its second turn on, sends one text by id until the send is accepted.
	struct TextSender final : cam::Agent {
		TextSender(const std::string& p_name, cam::AgentId p_target_id, std::string p_text) :
				Agent(p_name), m_target_id(p_target_id), m_text(std::move(p_text)) {}
		void default_action() override {
			if (m_sent) return;
			if (send(m_target_id, m_text)) {
				m_sent = true;
			}
		}
	private:
		cam::AgentId m_target_id;
		std::string m_text;
		bool m_sent = false;
	};

	// Sends "m1", "m2"... by id, one per turn.
	struct MultiSender final : cam::Agent {
		MultiSender(const std::string& p_name, cam::AgentId p_target_id, const int p_total) :
				Agent(p_name), m_target_id(p_target_id), m_total(p_total) {}
		void default_action() override {
			if (m_count >= m_total) return;
			const std::string l_text = "m" + std::to_string(++m_count);
			if (send(m_target_id, raw(l_text), l_text.size())) {
				g_recorder.record(get_name() + ":sent:" + l_text);
			}
		}
	private:
		cam::AgentId m_target_id;
		int m_total;
		int m_count = 0;
	};

	// Broadcasts once, from its second turn, with the given scope.
	struct Broadcaster final : cam::Agent {
		Broadcaster(const std::string& p_name, const Scope p_scope) : Agent(p_name), m_scope(p_scope) {}
		void default_action() override {
			if (m_done) return;
			m_done = true;
			const auto l_result = broadcast(raw("all"), 3, SendOptions{.m_scope = m_scope});
			g_recorder.record(get_name() + (l_result ? ":sent" : ":" + std::string(to_string(l_result.error().m_code))));
		}
	private:
		Scope m_scope;
		bool m_done = false;
	};

	// Sends once by name (or fragment) with the given scope and first_only flag.
	struct Namer final : cam::Agent {
		Namer(const std::string& p_name, std::string p_target, const bool p_fragment, const bool p_first_only, const Scope p_scope) :
				Agent(p_name), m_target(std::move(p_target)), m_fragment(p_fragment), m_first_only(p_first_only), m_scope(p_scope) {}
		void default_action() override {
			if (m_done) return;
			m_done = true;
			const SendOptions l_options{.m_scope = m_scope};
			const auto l_result = m_fragment ? send_by_fragment_name(m_target, raw("hi"), 2, m_first_only, l_options)
											 : send_by_name(m_target, raw("hi"), 2, m_first_only, l_options);
			g_recorder.record(get_name() + (l_result ? ":sent" : ":" + std::string(to_string(l_result.error().m_code))));
		}
	private:
		std::string m_target;
		bool m_fragment;
		bool m_first_only;
		Scope m_scope;
		bool m_done = false;
	};

	// Environment recording its transport hooks.
	struct TestEnvironment final : cam::Environment {
		explicit TestEnvironment(std::string p_label) : Environment(1, Mode::Sequential), m_label(std::move(p_label)) {}
		void on_environment_joined(const EnvironmentInfo& p_info) override { g_recorder.record(m_label + ":joined:" + p_info.m_name); }
		void on_environment_left(const EnvironmentInfo& p_info) override { g_recorder.record(m_label + ":left:" + p_info.m_name); }
		void on_delivery_failure(const Error& p_error) override { g_recorder.record(m_label + ":failure:" + to_string(p_error.m_code)); }
	private:
		std::string m_label;
	};

	std::shared_ptr<LoopbackTransport> open_loopback(LoopbackOptions p_options = {}, std::shared_ptr<const EnvelopeCodec> p_codec = nullptr) {
		auto l_transport = std::make_shared<LoopbackTransport>(TransportConfig{}, std::move(p_options), std::move(p_codec));
		REQUIRE(l_transport->open());
		return l_transport;
	}

	// These tests count turns: a generous flush budget keeps every envelope in its turn on a slow build (the json codec
	// takes about 25 ms per envelope under gcc ASan with its fake stacks, against a budget of 50 ms by default).
	EndpointConfig named(const std::string& p_name) {
		return EndpointConfig{.m_name = p_name, .m_flush_max_time = std::chrono::seconds(60)};
	}

	// Two environments, a receiver in B, a sender in A: the message crosses the transport.
	void unicast_scenario(const std::shared_ptr<LoopbackTransport>& p_transport) {
		g_recorder.clear();
		TestEnvironment l_a("A");
		TestEnvironment l_b("B");
		const cam::AgentId l_receiver = l_b.add<Receiver>("r");
		REQUIRE(l_b.connect(p_transport, named("B")));
		REQUIRE(l_a.connect(p_transport, named("A")));
		l_a.add<Sender>("s", l_receiver);

		// A learnt B from its retained presence at connect.
		const auto l_known = l_a.environments();
		REQUIRE(l_known.size() == 1);
		CHECK(l_known[0].m_id == l_b.get_id());
		CHECK(l_known[0].m_name == "B");
		CHECK(l_known[0].m_online);
		CHECK(l_known[0].m_node == p_transport->node_id());

		l_b.start();  // r inserted and announced; A's presence applied: B knows A
		CHECK(g_recorder.contains("B:joined:A"));
		REQUIRE(l_b.environments().size() == 1);
		l_a.start();  // directory learnt; s set up
		l_a.start();  // s sends to r through the transport
		CHECK(g_recorder.contains("s:sent"));
		CHECK_FALSE(g_recorder.contains("r:hello"));
		l_b.start();  // the message is applied at the boundary and handled in the turn
		CHECK(g_recorder.contains("r:hello"));
		CHECK(g_recorder.count_of("r:hello") == 1);

		CHECK(l_a.statistics().m_sent >= 1);
		CHECK(l_b.statistics().m_delivered >= 1);
		CHECK(l_b.statistics().m_rejected == 0);
		CHECK(l_a.statistics().m_rejected == 0);

		REQUIRE(l_b.disconnect());
		l_a.start();
		CHECK(g_recorder.contains("A:left:B"));
		REQUIRE(l_a.environments().size() == 1);
		CHECK_FALSE(l_a.environments()[0].m_online);
		REQUIRE(l_a.disconnect());
		CHECK_FALSE(l_a.is_connected());
	}

} // namespace

TEST_SUITE("loopback") {

	TEST_CASE("connect and disconnect") {
		auto l_transport = open_loopback();
		cam::Environment l_env(1, Mode::Sequential);
		CHECK_FALSE(l_env.is_connected());
		CHECK(l_env.environments().empty());
		CHECK(l_env.statistics().m_sent == 0);
		CHECK(is_canonical_uuid(l_env.get_id()));  // the id exists offline

		const auto l_twice = l_env.disconnect();
		REQUIRE_FALSE(l_twice);
		CHECK(l_twice.error().m_code == ErrorCode::NotConnected);

		REQUIRE(l_env.connect(l_transport, named("solo")));
		CHECK(l_env.is_connected());
		const auto l_again = l_env.connect(l_transport);
		REQUIRE_FALSE(l_again);
		CHECK(l_again.error().m_code == ErrorCode::AlreadyConnected);
		CHECK_FALSE(l_env.connect(std::shared_ptr<Transport>()));  // a null transport is refused
		REQUIRE(l_env.disconnect());
		CHECK_FALSE(l_env.is_connected());

		// The destructor disconnects a connected environment.
		{
			cam::Environment l_scoped(1, Mode::Sequential);
			REQUIRE(l_scoped.connect(l_transport));
		}
		cam::Environment l_late(1, Mode::Sequential);
		REQUIRE(l_late.connect(l_transport));
		CHECK(l_late.environments().empty());  // the scoped one withdrew its presence
	}

	TEST_CASE("unicast by id across two environments, in memory") {
		unicast_scenario(open_loopback());
	}

	TEST_CASE("unicast by id across two environments, serializing with every codec") {
		for (const auto& l_name: codec_names()) {
			CAPTURE(l_name);
			g_recorder.clear();
			unicast_scenario(open_loopback(LoopbackOptions{.m_serialize = true}, make_codec(l_name)));
		}
	}

	TEST_CASE("a text content arrives as text through every codec") {
		for (const auto& l_name: codec_names()) {
			CAPTURE(l_name);
			g_recorder.clear();
			auto l_transport = open_loopback(LoopbackOptions{.m_serialize = true}, make_codec(l_name));
			TestEnvironment l_a("A");
			TestEnvironment l_b("B");
			const cam::AgentId l_receiver = l_b.add<TypedReceiver>("r");
			REQUIRE(l_b.connect(l_transport, named("B")));
			REQUIRE(l_a.connect(l_transport, named("A")));
			l_a.add<TextSender>("s", l_receiver, "hello in clear");
			l_a.add<SharedTextSender>("t", l_receiver, "shared in clear");
			l_b.start();
			l_a.start();
			l_a.start();
			l_b.start();
			CHECK(g_recorder.contains("r:text:hello in clear"));
			CHECK(g_recorder.contains("r:text:shared in clear"));
			REQUIRE(l_a.disconnect());
			REQUIRE(l_b.disconnect());
		}
	}

	TEST_CASE("unknown destination") {
		g_recorder.clear();
		TestEnvironment l_a("A");
		const auto l_offline = l_a.send("ext", "nobody", raw("x"), 1);
		REQUIRE_FALSE(l_offline);
		CHECK(l_offline.error().m_code == ErrorCode::UnknownDestination);

		auto l_transport = open_loopback();
		REQUIRE(l_a.connect(l_transport, named("A")));
		const auto l_online = l_a.send("ext", "nobody", raw("x"), 1);
		REQUIRE_FALSE(l_online);
		CHECK(l_online.error().m_code == ErrorCode::UnknownDestination);
		CHECK(l_a.statistics().m_unknown_destination == 1);

		// A local agent is always reached directly, connected or not.
		const cam::AgentId l_local = l_a.add<Receiver>("local");
		l_a.start();
		REQUIRE(l_a.send("ext", l_local, raw("x"), 1));
		l_a.start();
		CHECK(g_recorder.contains("local:x"));
		CHECK(l_a.statistics().m_sent == 2);  // presence and the directory event, no message
	}

	TEST_CASE("network scope offline is refused, local delivery still happens") {
		g_recorder.clear();
		cam::Environment l_env(1, Mode::Sequential);
		l_env.add<Receiver>("r");
		const auto l_result = l_env.broadcast("ext", raw("x"), 1, SendOptions{.m_scope = Scope::Network});
		REQUIRE_FALSE(l_result);
		CHECK(l_result.error().m_code == ErrorCode::NotConnected);
		l_env.start();
		const auto l_by_name = l_env.send_by_name("ext", "r", raw("y"), 1, false, true, SendOptions{.m_scope = Scope::Network});
		REQUIRE_FALSE(l_by_name);
		CHECK(l_by_name.error().m_code == ErrorCode::NotConnected);
		l_env.start();
		CHECK(g_recorder.contains("r:y"));
	}

	TEST_CASE("broadcast: network scope reaches every agent of the other environment, local scope does not") {
		auto l_transport = open_loopback();
		for (const Scope l_scope: {Scope::Network, Scope::Local}) {
			g_recorder.clear();
			CAPTURE(static_cast<int>(l_scope));
			TestEnvironment l_a("A");
			TestEnvironment l_b("B");
			l_b.add<Receiver>("rb1");
			l_b.add<Receiver>("rb2");
			l_a.add<Receiver>("ra");
			l_a.add<Broadcaster>("bc", l_scope);
			REQUIRE(l_b.connect(l_transport, named("B")));
			REQUIRE(l_a.connect(l_transport, named("A")));
			l_b.start();
			l_a.start();  // setup
			l_a.start();  // broadcast
			CHECK(g_recorder.contains("bc:sent"));
			l_a.start();  // ra handles it
			l_b.start();  // rb1, rb2 handle it (network scope only)
			CHECK(g_recorder.count_of("ra:all") == 1);
			CHECK(g_recorder.count_of("bc:all") == 0);
			CHECK(g_recorder.count_of("rb1:all") == (l_scope == Scope::Network ? 1 : 0));
			CHECK(g_recorder.count_of("rb2:all") == (l_scope == Scope::Network ? 1 : 0));
			REQUIRE(l_a.disconnect());
			REQUIRE(l_b.disconnect());
		}
	}

	TEST_CASE("send by name and by fragment: first_only applies per environment") {
		auto l_transport = open_loopback();
		g_recorder.clear();
		TestEnvironment l_a("A");
		TestEnvironment l_b("B");
		l_a.add<Receiver>("worker");
		l_a.add<Receiver>("worker");
		l_b.add<Receiver>("worker");
		l_b.add<Receiver>("worker");
		l_b.add<Receiver>("other");
		l_a.add<Namer>("n1", "worker", false, true, Scope::Network);
		l_a.add<Namer>("n2", "work", true, false, Scope::Network);
		l_a.add<Namer>("n3", "other", false, false, Scope::Local);
		REQUIRE(l_b.connect(l_transport, named("B")));
		REQUIRE(l_a.connect(l_transport, named("A")));
		l_b.start();
		l_a.start();  // setup
		l_a.start();  // sends
		CHECK(g_recorder.contains("n1:sent"));
		CHECK(g_recorder.contains("n2:sent"));
		CHECK(g_recorder.contains("n3:sent"));
		l_a.start();  // local deliveries handled
		l_b.start();  // remote deliveries handled
		// n1: first "worker" of A and first "worker" of B; n2: every "worker" of A and of B.
		CHECK(g_recorder.count_of("worker:hi") == 1 + 1 + 2 + 2);
		CHECK(g_recorder.count_of("other:hi") == 0);  // local scope never leaves A
		REQUIRE(l_a.disconnect());
		REQUIRE(l_b.disconnect());
	}

	TEST_CASE("directory follows additions and removals") {
		auto l_transport = open_loopback();
		g_recorder.clear();
		TestEnvironment l_a("A");
		TestEnvironment l_b("B");
		REQUIRE(l_b.connect(l_transport, named("B")));
		REQUIRE(l_a.connect(l_transport, named("A")));
		const cam::AgentId l_receiver = l_b.add<Receiver>("r");
		l_b.start();  // r inserted: AgentAdded published
		l_a.start();  // A knows r
		REQUIRE(l_a.send("ext", l_receiver, raw("1"), 1));
		l_a.start();  // flushed
		l_b.remove(l_receiver);  // explicit removal: AgentRemoved published at the next flush
		l_b.start();  // the message finds no agent: counted, no crash
		CHECK(l_b.statistics().m_unknown_destination == 1);
		l_a.start();  // A forgets r
		const auto l_gone = l_a.send("ext", l_receiver, raw("2"), 1);
		REQUIRE_FALSE(l_gone);
		CHECK(l_gone.error().m_code == ErrorCode::UnknownDestination);

		// An agent that stops itself is removed at the boundary and announced as well.
		struct Quitter final : cam::Agent {
			explicit Quitter(const std::string& p_name) : Agent(p_name) {}
			void setup() override { stop(); }
		};
		const cam::AgentId l_quitter = l_b.add<Quitter>("q");
		l_b.start();  // inserted (AgentAdded), set up and stopped
		l_a.start();  // A knows q
		REQUIRE(l_a.send("ext", l_quitter, raw("3"), 1));
		l_b.start();  // q removed (AgentRemoved)
		l_a.start();
		CHECK_FALSE(l_a.send("ext", l_quitter, raw("4"), 1));
		REQUIRE(l_a.disconnect());
		REQUIRE(l_b.disconnect());
	}

	TEST_CASE("a message whose sender identifier is longer than an AgentId is refused and counted") {
		g_recorder.clear();
		auto l_transport = open_loopback();
		TestEnvironment l_b("B");
		const cam::AgentId l_receiver = l_b.add<Receiver>("r");
		REQUIRE(l_b.connect(l_transport, named("B")));
		l_b.start();  // r inserted

		// A peer sends to r, first with a 36-character sender identifier, then with a short one
		Endpoint l_peer(cam::generate_uuid(), EndpointConfig{.m_name = "peer"});
		REQUIRE(l_peer.attach(l_transport));
		const auto l_send = [&](const std::string& p_sender, const std::string& p_text) {
			Envelope l_envelope = l_peer.make_envelope(EnvelopeKind::Message);
			l_envelope.m_sender_agent = p_sender;
			l_envelope.m_target_kind = TargetKind::Id;
			l_envelope.m_target_id = l_receiver.str();
			l_envelope.m_content_encoding = cam::MessageBinaryFormat::Text;
			l_envelope.m_content.assign(p_text.begin(), p_text.end());
			REQUIRE(l_peer.enqueue(Address{AddressKind::EnvIn, l_peer.ns(), l_b.get_id()}, std::move(l_envelope)));
			CHECK(l_peer.flush_outbox().empty());
		};
		const auto l_rejected = l_b.statistics().m_rejected;
		l_send(cam::generate_uuid(), "too long");
		l_b.start();
		CHECK(l_b.statistics().m_rejected == l_rejected + 1);
		CHECK_FALSE(g_recorder.contains("r:too long"));
		l_send("ext", "short");
		l_b.start();
		CHECK(g_recorder.count_of("r:short") == 1);
		CHECK(l_b.statistics().m_rejected == l_rejected + 1);
		REQUIRE(l_peer.detach());
		REQUIRE(l_b.disconnect());
	}

	TEST_CASE("duplicates injected by the transport are delivered once") {
		g_recorder.clear();
		auto l_transport = open_loopback(LoopbackOptions{.m_duplicate_every_n = 1});
		TestEnvironment l_a("A");
		TestEnvironment l_b("B");
		const cam::AgentId l_receiver = l_b.add<Receiver>("r");
		REQUIRE(l_b.connect(l_transport, named("B")));
		REQUIRE(l_a.connect(l_transport, named("A")));
		l_a.add<Sender>("s", l_receiver);
		l_b.start();
		l_a.start();
		l_a.start();
		l_b.start();
		CHECK(g_recorder.count_of("r:hello") == 1);
		CHECK(l_b.statistics().m_duplicates >= 1);
		REQUIRE(l_a.disconnect());
		REQUIRE(l_b.disconnect());
	}

	TEST_CASE("a message dropped by the transport is counted as lost by the receiver") {
		// The flow starts at the first sequence seen: a loss is only visible after a first message.
		g_recorder.clear();
		LoopbackOptions l_options;
		l_options.m_drop_if = [](const Envelope& p_envelope) {
			return p_envelope.m_kind == EnvelopeKind::Message && p_envelope.m_content == std::vector<std::uint8_t>{'m', '2'};
		};
		auto l_transport = open_loopback(l_options);
		TestEnvironment l_a("A");
		TestEnvironment l_b("B");
		const cam::AgentId l_receiver = l_b.add<Receiver>("r");
		EndpointConfig l_config = named("B");
		l_config.m_reorder_turns = 2;
		REQUIRE(l_b.connect(l_transport, l_config));
		REQUIRE(l_a.connect(l_transport, named("A")));
		l_a.add<MultiSender>("s", l_receiver, 3);
		l_b.start();
		l_a.start();  // setup
		l_a.start();  // m1
		l_a.start();  // m2 (dropped by the transport)
		l_a.start();  // m3
		CHECK(g_recorder.contains("s:sent:m1"));
		CHECK(g_recorder.contains("s:sent:m2"));
		CHECK(g_recorder.contains("s:sent:m3"));
		CHECK(l_transport->dropped() == 1);
		l_b.start();  // m1 handled, m3 waits for m2
		CHECK(g_recorder.contains("r:m1"));
		CHECK_FALSE(g_recorder.contains("r:m3"));
		l_b.start();  // still waiting
		CHECK_FALSE(g_recorder.contains("r:m3"));
		l_b.start();  // gap released at the boundary, m3 handled in the turn
		CHECK(g_recorder.contains("r:m3"));
		CHECK_FALSE(g_recorder.contains("r:m2"));
		CHECK(l_b.statistics().m_lost == 1);
		REQUIRE(l_a.disconnect());
		REQUIRE(l_b.disconnect());
	}

	TEST_CASE("agents added before connect are announced by the presence") {
		g_recorder.clear();
		auto l_transport = open_loopback();
		TestEnvironment l_a("A");
		TestEnvironment l_b("B");
		const cam::AgentId l_receiver = l_b.add<Receiver>("r");
		l_b.start();  // r inserted offline
		REQUIRE(l_b.connect(l_transport, named("B")));  // presence lists r
		REQUIRE(l_a.connect(l_transport, named("A")));  // retained presence: A knows r at once
		REQUIRE(l_a.send("ext", l_receiver, raw("x"), 1));
		l_a.start();
		l_b.start();
		CHECK(g_recorder.contains("r:x"));
		REQUIRE(l_a.disconnect());
		REQUIRE(l_b.disconnect());
	}

	TEST_CASE("three environments on one transport") {
		g_recorder.clear();
		auto l_transport = open_loopback();
		TestEnvironment l_a("A");
		TestEnvironment l_b("B");
		TestEnvironment l_c("C");
		l_b.add<Receiver>("rb");
		l_c.add<Receiver>("rc");
		l_a.add<Broadcaster>("bc", Scope::Network);
		REQUIRE(l_a.connect(l_transport, named("A")));
		REQUIRE(l_b.connect(l_transport, named("B")));
		REQUIRE(l_c.connect(l_transport, named("C")));
		CHECK(l_c.environments().size() == 2);
		l_a.start();
		l_b.start();
		l_c.start();
		CHECK(l_a.environments().size() == 2);
		CHECK(l_b.environments().size() == 2);
		l_a.start();  // broadcast
		l_b.start();
		l_c.start();
		CHECK(g_recorder.count_of("rb:all") == 1);
		CHECK(g_recorder.count_of("rc:all") == 1);
		REQUIRE(l_a.disconnect());
		REQUIRE(l_b.disconnect());
		REQUIRE(l_c.disconnect());
	}
}
