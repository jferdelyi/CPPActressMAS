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

// Three-step agent migration (offer, accept or reject, commit or abort) between environments connected
// through the loopback transport. Each environment runs one turn per start() (p_no_turns = 1), so the
// turns of two or three environments interleave deterministically on one thread. Agents record into a
// Recorder and never assert.

#include <doctest/doctest.h>

#include <algorithm>
#include <map>
#include <thread>

#include <Environment.h>
#include <transport/loopback/LoopbackTransport.h>

#include "Recorder.hpp"

namespace {

	using namespace cam::transport;
	using namespace std::chrono_literals;
	using Mode = cam::EnvironmentMasMode;

	cam::test::Recorder g_recorder;

	const std::uint8_t* raw(const std::string& p_text) {
		return reinterpret_cast<const std::uint8_t*>(p_text.data());
	}

	std::string code(const Result<void>& p_result) {
		return p_result ? "ok" : to_string(p_result.error().m_code);
	}

	// Label of each test environment, by id: agents only know their environment's id.
	std::map<std::string, std::string> g_labels;

	// Environment with a label, recording its transport hooks.
	struct TestEnvironment final : cam::Environment {
		explicit TestEnvironment(std::string p_label, const Mode p_mode = Mode::Sequential) : Environment(1, p_mode), m_label(std::move(p_label)) {
			g_labels[get_id()] = m_label;
		}
		void on_environment_joined(const EnvironmentInfo& p_info) override { g_recorder.record(m_label + ":joined:" + p_info.m_name); }
		void on_environment_left(const EnvironmentInfo& p_info) override { g_recorder.record(m_label + ":left:" + p_info.m_name); }
		void on_delivery_failure(const Error& p_error) override { g_recorder.record(m_label + ":failure:" + to_string(p_error.m_code)); }
	private:
		std::string m_label;
	};

	std::string label_of(const cam::Environment* p_environment) {
		return g_labels.at(p_environment->get_id());
	}

	// Goes to "away", comes back "home", then stops; counts its turns in its state.
	struct Traveler final : cam::Agent {
		explicit Traveler(const std::string& p_name) : Agent(p_name) {}
		Traveler(const std::string& p_name, std::string p_home, std::string p_away) :
				Agent(p_name), m_home(std::move(p_home)), m_away(std::move(p_away)) {}

		void setup() override {
			m_observables->insert_or_assign("hops", m_hops);
			g_recorder.record(get_name() + ":setup@" + label_of(get_environment()) + " counter=" + std::to_string(m_counter));
		}

		void default_action() override {
			++m_counter;
			if (m_hops == 0) {
				m_hops = 1;
				g_recorder.record(get_name() + ":move:" + code(move(m_away)));
			} else if (m_hops == 1) {
				m_hops = 2;
				g_recorder.record(get_name() + ":move:" + code(move(m_home)));
			} else {
				g_recorder.record(get_name() + ":done@" + label_of(get_environment()) + " counter=" + std::to_string(m_counter)
								  + " hops=" + get_observables()->at("hops").dump());
				stop();
			}
		}

		[[nodiscard]] json save_state() const override {
			return {{"home", m_home}, {"away", m_away}, {"hops", m_hops}, {"counter", m_counter}};
		}

		void load_state(const json& p_state) override {
			m_home = p_state.at("home").get<std::string>();
			m_away = p_state.at("away").get<std::string>();
			m_hops = p_state.at("hops").get<int>();
			m_counter = p_state.at("counter").get<int>();
		}

	private:
		std::string m_home;
		std::string m_away;
		int m_hops = 0;
		int m_counter = 0;
	};

	// Moves once to a target, then records every turn it runs.
	struct Hopper final : cam::Agent {
		explicit Hopper(const std::string& p_name) : Agent(p_name) {}
		Hopper(const std::string& p_name, std::string p_target) : Agent(p_name), m_target(std::move(p_target)) {}
		void setup() override { g_recorder.record(get_name() + ":setup@" + label_of(get_environment())); }
		void default_action() override {
			if (!m_moved) {
				m_moved = true;
				g_recorder.record(get_name() + ":move:" + code(move(m_target)));
				return;
			}
			g_recorder.record(get_name() + ":running@" + label_of(get_environment()));
		}
		[[nodiscard]] json save_state() const override { return {{"target", m_target}, {"moved", m_moved}}; }
		void load_state(const json& p_state) override {
			m_target = p_state.at("target").get<std::string>();
			m_moved = p_state.at("moved").get<bool>();
		}
	private:
		std::string m_target;
		bool m_moved = false;
	};

	// Receiver that moves once at its first default_action and records every message it handles.
	struct MovingReceiver final : cam::Agent {
		explicit MovingReceiver(const std::string& p_name) : Agent(p_name) {}
		MovingReceiver(const std::string& p_name, std::string p_target) : Agent(p_name), m_target(std::move(p_target)) {}
		void setup() override { g_recorder.record(get_name() + ":setup@" + label_of(get_environment())); }
		void action(const cam::MessagePointer& p_message) override {
			g_recorder.record(get_name() + ":" + p_message->to_string() + "@" + label_of(get_environment())
							  + (p_message->get_sender_environment().empty() ? "" : " remote"));
		}
		void default_action() override {
			if (!m_moved) {
				m_moved = true;
				g_recorder.record(get_name() + ":move:" + code(move(m_target)));
			}
		}
		[[nodiscard]] json save_state() const override { return {{"target", m_target}, {"moved", m_moved}}; }
		void load_state(const json& p_state) override {
			m_target = p_state.at("target").get<std::string>();
			m_moved = p_state.at("moved").get<bool>();
		}
	private:
		std::string m_target;
		bool m_moved = false;
	};

	// Sends "<prefix><n>" to a given id every turn, from its second turn.
	struct Feeder final : cam::Agent {
		Feeder(const std::string& p_name, std::string p_target_id, std::string p_prefix) :
				Agent(p_name), m_target_id(std::move(p_target_id)), m_prefix(std::move(p_prefix)) {}
		void default_action() override {
			const std::string l_text = m_prefix + std::to_string(++m_count);
			g_recorder.record(get_name() + ":" + l_text + ":" + code(send(m_target_id, raw(l_text), l_text.size())));
		}
	private:
		std::string m_target_id;
		std::string m_prefix;
		int m_count = 0;
	};

	// Agent type never registered anywhere.
	struct Stranger final : cam::Agent {
		explicit Stranger(const std::string& p_name) : Agent(p_name) {}
		Stranger(const std::string& p_name, std::string p_target) : Agent(p_name), m_target(std::move(p_target)) {}
		void default_action() override {
			if (!m_done) {
				m_done = true;
				g_recorder.record(get_name() + ":move:" + code(move(m_target)));
			}
		}
	private:
		std::string m_target;
		bool m_done = false;
	};

	std::shared_ptr<LoopbackTransport> open_loopback(LoopbackOptions p_options = {}) {
		auto l_transport = std::make_shared<LoopbackTransport>(TransportConfig{}, std::move(p_options));
		REQUIRE(l_transport->open());
		return l_transport;
	}

	EndpointConfig config(const std::string& p_name, const std::chrono::milliseconds p_migration_timeout = 5000ms,
						  const std::chrono::milliseconds p_accept_hold = 15000ms) {
		EndpointConfig l_config;
		l_config.m_name = p_name;
		l_config.m_migration_timeout = p_migration_timeout;
		l_config.m_accept_hold_timeout = p_accept_hold;
		return l_config;
	}

	bool is_kind(const Envelope& p_envelope, const EnvelopeKind p_kind) {
		return p_envelope.m_kind == p_kind;
	}

} // namespace

TEST_SUITE("migration") {

	TEST_CASE("move: pre-conditions") {
		g_recorder.clear();
		auto l_transport = open_loopback();
		TestEnvironment l_a("A");
		TestEnvironment l_b("B");

		SUBCASE("offline: NotConnected, the agent stays and runs on") {
			l_a.add<Hopper>("h", l_b.get_id());
			l_a.start();
			l_a.start();
			l_a.start();
			CHECK(g_recorder.contains("h:move:not connected"));
			CHECK(g_recorder.contains("h:running@A"));
			CHECK(l_a.agents_count() == 1);
		}
		SUBCASE("unknown destination and own environment") {
			l_a.register_agent_type<Hopper>("Hopper");
			REQUIRE(l_a.connect(l_transport, config("A")));
			l_a.add<Hopper>("h1", "cccccccc-0000-4000-8000-000000000003");
			l_a.add<Hopper>("h2", l_a.get_id());
			l_a.start();
			l_a.start();
			CHECK(g_recorder.contains("h1:move:unknown destination"));
			CHECK(g_recorder.contains("h2:move:invalid target"));
			REQUIRE(l_a.disconnect());
		}
		SUBCASE("type not registered at the origin") {
			REQUIRE(l_b.connect(l_transport, config("B")));
			REQUIRE(l_a.connect(l_transport, config("A")));
			l_a.add<Hopper>("h", l_b.get_id());
			l_a.start();
			l_a.start();
			CHECK(g_recorder.contains("h:move:unknown agent type"));
			CHECK(l_a.agents_count() == 1);
			REQUIRE(l_a.disconnect());
			REQUIRE(l_b.disconnect());
		}
	}

	TEST_CASE("round trip: the agent keeps its id, its state, its observables, and runs setup again") {
		g_recorder.clear();
		auto l_transport = open_loopback();
		TestEnvironment l_a("A");
		TestEnvironment l_b("B");
		l_a.register_agent_type<Traveler>("Traveler");
		l_b.register_agent_type<Traveler>("Traveler");
		REQUIRE(l_b.connect(l_transport, config("B")));
		REQUIRE(l_a.connect(l_transport, config("A")));
		const std::string l_id = l_a.add<Traveler>("t", l_a.get_id(), l_b.get_id());
		l_b.start();

		l_a.start();  // inserted, setup
		l_a.start();  // counter 1: move to B (offer sent at the end of the turn)
		CHECK(g_recorder.contains("t:move:ok"));
		CHECK(l_a.agents_count() == 1);  // still here, in transit
		l_b.start();  // offer: copy kept aside, accept sent
		CHECK(l_b.agents_count() == 0);
		l_a.start();  // accept: commit sent, copy destroyed
		CHECK(l_a.agents_count() == 0);
		CHECK(l_a.statistics().m_migrations_completed == 1);
		l_b.start();  // commit: inserted; setup runs again with the carried state
		CHECK(l_b.agents_count() == 1);
		CHECK(l_b.get_agent_name(l_id) == "t");
		CHECK(g_recorder.contains("t:setup@B counter=1"));
		CHECK(l_b.statistics().m_migrations_completed == 1);

		l_b.start();  // counter 2: move home
		l_a.start();  // offer: accept
		l_b.start();  // accept: commit
		l_a.start();  // commit: inserted, setup
		CHECK(g_recorder.contains("t:setup@A counter=2"));
		l_a.start();  // counter 3: done
		CHECK(g_recorder.contains("t:done@A counter=3 hops=2"));
		CHECK(l_a.statistics().m_migrations_completed == 2);
		CHECK(l_b.statistics().m_migrations_completed == 2);
		CHECK(l_a.statistics().m_migrations_failed == 0);
		CHECK(l_b.statistics().m_migrations_failed == 0);
		CHECK(l_a.statistics().m_stale_control == 0);

		const auto l_entries = g_recorder.entries();
		const std::vector<std::string> l_expected = {
				"A:joined:B", "B:joined:A", "t:setup@A counter=0", "t:move:ok", "t:setup@B counter=1", "t:move:ok",
				"t:setup@A counter=2", "t:done@A counter=3 hops=2"};
		CHECK(l_entries == l_expected);
		REQUIRE(l_a.disconnect());
		REQUIRE(l_b.disconnect());
	}

	TEST_CASE("refused by the destination: unregistered type, the agent resumes at the origin") {
		g_recorder.clear();
		auto l_transport = open_loopback();
		TestEnvironment l_a("A");
		TestEnvironment l_b("B");
		l_a.register_agent_type<Hopper>("Hopper");  // B does not register it
		REQUIRE(l_b.connect(l_transport, config("B")));
		REQUIRE(l_a.connect(l_transport, config("A")));
		l_a.add<Hopper>("h", l_b.get_id());
		l_b.start();
		l_a.start();  // setup
		l_a.start();  // move: offer
		l_b.start();  // reject
		CHECK(l_b.statistics().m_migrations_failed == 1);
		l_a.start();  // reject applied: agent restored and runs this turn
		CHECK(g_recorder.contains("A:failure:unknown agent type"));
		CHECK(g_recorder.contains("h:running@A"));
		CHECK(l_a.agents_count() == 1);
		CHECK(l_a.statistics().m_migrations_failed == 1);
		CHECK(l_b.agents_count() == 0);
		REQUIRE(l_a.disconnect());
		REQUIRE(l_b.disconnect());
	}

	TEST_CASE("lost acceptance: the origin times out, aborts and restores; the destination discards") {
		g_recorder.clear();
		LoopbackOptions l_options;
		l_options.m_drop_if = [](const Envelope& p_envelope) { return is_kind(p_envelope, EnvelopeKind::AgentAccept); };
		auto l_transport = open_loopback(l_options);
		TestEnvironment l_a("A");
		TestEnvironment l_b("B");
		l_a.register_agent_type<Hopper>("Hopper");
		l_b.register_agent_type<Hopper>("Hopper");
		REQUIRE(l_b.connect(l_transport, config("B", 50ms, 150ms)));
		REQUIRE(l_a.connect(l_transport, config("A", 50ms, 150ms)));
		l_a.add<Hopper>("h", l_b.get_id());
		l_b.start();
		l_a.start();
		l_a.start();  // offer
		l_b.start();  // accept (dropped by the transport)
		l_a.start();  // nothing yet: agent in transit, does not run
		CHECK_FALSE(g_recorder.contains("h:running@A"));
		std::this_thread::sleep_for(60ms);
		l_a.start();  // deadline passed: abort sent, agent restored and runs
		CHECK(g_recorder.contains("A:failure:timeout"));
		CHECK(g_recorder.contains("h:running@A"));
		CHECK(l_a.statistics().m_migrations_failed == 1);
		l_b.start();  // abort: pending copy dropped
		CHECK(l_b.agents_count() == 0);
		CHECK(l_b.statistics().m_migrations_failed == 1);
		CHECK(l_b.statistics().m_stale_control == 0);
		REQUIRE(l_a.disconnect());
		REQUIRE(l_b.disconnect());
	}

	TEST_CASE("late acceptance: ignored and counted, the agent stays at the origin") {
		g_recorder.clear();
		LoopbackOptions l_options;
		l_options.m_hold_if = [](const Envelope& p_envelope) { return is_kind(p_envelope, EnvelopeKind::AgentAccept); };
		auto l_transport = open_loopback(l_options);
		TestEnvironment l_a("A");
		TestEnvironment l_b("B");
		l_a.register_agent_type<Hopper>("Hopper");
		l_b.register_agent_type<Hopper>("Hopper");
		REQUIRE(l_b.connect(l_transport, config("B", 50ms, 150ms)));
		REQUIRE(l_a.connect(l_transport, config("A", 50ms, 150ms)));
		l_a.add<Hopper>("h", l_b.get_id());
		l_b.start();
		l_a.start();
		l_a.start();  // offer
		l_b.start();  // accept (held)
		std::this_thread::sleep_for(60ms);
		l_a.start();  // timeout: abort, restore
		CHECK(g_recorder.contains("h:running@A"));
		l_b.start();  // abort applied
		CHECK(l_transport->release_held() == 1);
		l_a.start();  // late accept: stale
		CHECK(l_a.statistics().m_stale_control == 1);
		CHECK(l_a.agents_count() == 1);
		CHECK(l_b.agents_count() == 0);
		REQUIRE(l_a.disconnect());
		REQUIRE(l_b.disconnect());
	}

	TEST_CASE("lost confirmation: the known loss window, counted on both sides") {
		g_recorder.clear();
		LoopbackOptions l_options;
		l_options.m_drop_if = [](const Envelope& p_envelope) { return is_kind(p_envelope, EnvelopeKind::AgentCommit); };
		auto l_transport = open_loopback(l_options);
		TestEnvironment l_a("A");
		TestEnvironment l_b("B");
		l_a.register_agent_type<Hopper>("Hopper");
		l_b.register_agent_type<Hopper>("Hopper");
		REQUIRE(l_b.connect(l_transport, config("B", 50ms, 100ms)));
		REQUIRE(l_a.connect(l_transport, config("A", 50ms, 100ms)));
		l_a.add<Hopper>("h", l_b.get_id());
		l_b.start();
		l_a.start();
		l_a.start();  // offer
		l_b.start();  // accept
		l_a.start();  // commit (dropped): the origin believes the move succeeded
		CHECK(l_a.agents_count() == 0);
		CHECK(l_a.statistics().m_migrations_completed == 1);
		std::this_thread::sleep_for(110ms);
		l_b.start();  // hold deadline passed: the pending copy is destroyed
		CHECK(l_b.agents_count() == 0);
		CHECK(l_b.statistics().m_migrations_failed == 1);
		REQUIRE(l_a.disconnect());
		REQUIRE(l_b.disconnect());
	}

	TEST_CASE("messages during and after the move follow the agent, in order") {
		g_recorder.clear();
		auto l_transport = open_loopback();
		TestEnvironment l_a("A");
		TestEnvironment l_b("B");
		TestEnvironment l_c("C");
		l_a.register_agent_type<MovingReceiver>("MovingReceiver");
		l_b.register_agent_type<MovingReceiver>("MovingReceiver");
		REQUIRE(l_b.connect(l_transport, config("B")));
		REQUIRE(l_a.connect(l_transport, config("A")));
		REQUIRE(l_c.connect(l_transport, config("C")));
		const std::string l_receiver = l_a.add<MovingReceiver>("r", l_b.get_id());
		l_a.add<Feeder>("f", l_receiver, "m");
		l_c.add<Feeder>("g", l_receiver, "c");

		l_b.start();
		l_a.start();  // r and f inserted (AgentAdded published), setup
		l_c.start();  // C learns r@A; g set up
		l_a.start();  // r: move (offer at the end of the turn); f: m1 to r (local, held in r's queue, carried by the offer)
		CHECK(g_recorder.contains("f:m1:ok"));
		l_b.start();  // offer accepted
		l_c.start();  // g: c1 to r, routed to A (C still believes r is at A)
		l_a.start();  // boundary: accept -> commit, r removed, directory r@B; turn: f: m2 to r, routed to B by the directory
		CHECK(l_a.agents_count() == 1);
		CHECK(g_recorder.contains("f:m2:ok"));
		l_b.start();  // commit: r inserted; m2 delivered; setup runs again
		CHECK(g_recorder.contains("r:setup@B"));
		l_a.start();  // c1 reaches A: r left, forwarded to B (grace period); f: m3 to r through the directory
		CHECK(l_a.statistics().m_forwarded == 1);
		CHECK(g_recorder.contains("f:m3:ok"));
		l_b.start();  // r handles m1, m2 then c1 and m3 arrive at this boundary
		l_b.start();
		const auto l_entries = g_recorder.entries();
		const auto l_position = [&](const std::string& p_entry) {
			return std::find(l_entries.begin(), l_entries.end(), p_entry) - l_entries.begin();
		};
		CHECK(g_recorder.contains("r:m1@B remote"));
		CHECK(g_recorder.contains("r:m2@B remote"));
		CHECK(g_recorder.contains("r:c1@B remote"));
		CHECK(g_recorder.contains("r:m3@B remote"));
		CHECK(l_position("r:m1@B remote") < l_position("r:m2@B remote"));
		CHECK(l_position("r:m2@B remote") < l_position("r:m3@B remote"));
		CHECK(l_b.statistics().m_unknown_destination == 0);
		CHECK(l_a.statistics().m_unknown_destination == 0);
		l_c.start();  // C applies AgentMoved: r@B
		l_c.start();  // g: c2 straight to B
		l_b.start();
		l_b.start();
		CHECK(g_recorder.contains("r:c2@B remote"));
		REQUIRE(l_a.disconnect());
		REQUIRE(l_b.disconnect());
		REQUIRE(l_c.disconnect());
	}

	TEST_CASE("an unknown type is refused before any agent is built") {
		g_recorder.clear();
		auto l_transport = open_loopback();
		TestEnvironment l_a("A");
		TestEnvironment l_b("B");
		l_a.register_agent_type<Stranger>("Stranger");
		REQUIRE(l_b.connect(l_transport, config("B")));
		REQUIRE(l_a.connect(l_transport, config("A")));
		l_a.add<Stranger>("s", l_b.get_id());
		l_b.start();
		l_a.start();
		l_a.start();
		l_b.start();
		l_a.start();
		CHECK(g_recorder.contains("s:move:ok"));
		CHECK(g_recorder.contains("A:failure:unknown agent type"));
		CHECK(l_a.agents_count() == 1);
		CHECK(l_b.agents_count() == 0);
		REQUIRE(l_a.disconnect());
		REQUIRE(l_b.disconnect());
	}

	TEST_CASE("parallel mode: several agents move in the same turn") {
		g_recorder.clear();
		auto l_transport = open_loopback();
		TestEnvironment l_a("A", Mode::Parallel);
		TestEnvironment l_b("B", Mode::Parallel);
		l_a.register_agent_type<Hopper>("Hopper");
		l_b.register_agent_type<Hopper>("Hopper");
		REQUIRE(l_b.connect(l_transport, config("B")));
		REQUIRE(l_a.connect(l_transport, config("A")));
		constexpr int l_count = 8;
		for (int i = 0; i < l_count; i++) {
			l_a.add<Hopper>("h" + std::to_string(i), l_b.get_id());
		}
		l_b.start();
		l_a.start();  // setup
		l_a.start();  // every hopper asks to move, from the pool threads
		CHECK(g_recorder.count_of("h0:move:ok") == 1);
		l_b.start();  // offers accepted
		l_a.start();  // commits
		CHECK(l_a.agents_count() == 0);
		l_b.start();  // inserted, setup
		CHECK(l_b.agents_count() == l_count);
		l_b.start();  // running at B
		for (int i = 0; i < l_count; i++) {
			CHECK(g_recorder.count_of("h" + std::to_string(i) + ":running@B") == 1);
		}
		CHECK(l_a.statistics().m_migrations_completed == l_count);
		CHECK(l_b.statistics().m_migrations_completed == l_count);
		REQUIRE(l_a.disconnect());
		REQUIRE(l_b.disconnect());
	}

	TEST_CASE("disconnecting during a move restores the agent") {
		g_recorder.clear();
		auto l_transport = open_loopback();
		TestEnvironment l_a("A");
		TestEnvironment l_b("B");
		l_a.register_agent_type<Hopper>("Hopper");
		l_b.register_agent_type<Hopper>("Hopper");
		REQUIRE(l_b.connect(l_transport, config("B")));
		REQUIRE(l_a.connect(l_transport, config("A")));
		l_a.add<Hopper>("h", l_b.get_id());
		l_b.start();
		l_a.start();
		l_a.start();  // offer
		REQUIRE(l_a.disconnect());  // transit abandoned
		CHECK(g_recorder.contains("A:failure:not connected"));
		l_a.start();
		CHECK(g_recorder.contains("h:running@A"));
		CHECK(l_a.agents_count() == 1);
		l_b.start();  // offer applied (accept sent to a departed environment), then A's offline presence
		REQUIRE(l_b.disconnect());
	}
}
