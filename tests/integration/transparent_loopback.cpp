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

// Environments of one process without any configuration: started, they reach each other through the loopback of the
// process (namespace "default"), as with loopback://, hidden from is_connected() and statistics(); alone there, an
// environment answers as offline. Each environment runs one turn per start(), so the turns interleave
// deterministically on one thread, but for the concurrent case. Agents record into a Recorder and never assert.

#include <doctest/doctest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <map>
#include <string>
#include <thread>

#include <Environment.h>
#include <transport/loopback/LoopbackTransport.h>

#include "Recorder.hpp"

namespace {

	using namespace cam::transport;
	using namespace std::chrono_literals;
	using Mode = cam::EnvironmentMasMode;

	cam::test::Recorder g_recorder;

	const std::string s_hello = "hello";

	const std::uint8_t* raw(const std::string& p_text) {
		return reinterpret_cast<const std::uint8_t*>(p_text.data());
	}

	std::string code(const Result<void>& p_result) {
		return p_result ? "ok" : to_string(p_result.error().m_code);
	}

	// Label of each test environment, by id: agents only know their environment's id.
	std::map<std::string, std::string> g_labels;

	// Environment with a label, one turn per start().
	struct TestEnvironment final : cam::Environment {
		explicit TestEnvironment(std::string p_label, const Mode p_mode = Mode::Sequential, std::string p_url = {}) :
				Environment({.m_turns = 1, .m_mode = p_mode, .m_url = std::move(p_url)}), m_label(std::move(p_label)) {
			g_labels[get_id()] = m_label;
		}
		void on_environment_joined(const EnvironmentInfo&) override { g_recorder.record(m_label + ":joined"); }
	private:
		std::string m_label;
	};

	std::string label_of(const cam::Environment* p_environment) {
		return g_labels.at(p_environment->get_id());
	}

	// The environments known online: one that left stays known, offline.
	std::size_t online(const std::vector<EnvironmentInfo>& p_environments) {
		return static_cast<std::size_t>(std::count_if(p_environments.begin(), p_environments.end(),
													  [](const EnvironmentInfo& p_info) { return p_info.m_online; }));
	}

	// Records every message it receives as "<name>:<content>".
	struct Receiver final : cam::Agent {
		explicit Receiver(const std::string& p_name) : Agent(p_name) {}
		void action(const cam::MessagePointer& p_message) override { g_recorder.record(get_name() + ":" + p_message->to_string()); }
	};

	// From its second turn on, sends one message by id until the send is accepted; records each refusal.
	struct Sender final : cam::Agent {
		Sender(const std::string& p_name, cam::AgentId p_target_id) : Agent(p_name), m_target_id(p_target_id) {}
		void default_action() override {
			if (m_sent) {
				return;
			}
			const auto l_result = send(m_target_id, raw(s_hello), s_hello.size());
			m_sent = static_cast<bool>(l_result);
			g_recorder.record(get_name() + ":" + (m_sent ? std::string("sent") : code(l_result)));
		}
	private:
		cam::AgentId m_target_id;
		bool m_sent = false;
	};

	// From its second turn on, sends once by name and once by broadcast with the network scope; records the results.
	struct Crier final : cam::Agent {
		explicit Crier(const std::string& p_name) : Agent(p_name) {}
		void default_action() override {
			if (m_done) {
				return;
			}
			m_done = true;
			g_recorder.record(get_name() + ":by name:" + code(send_by_name("r", raw(s_hello), s_hello.size(), false,
																			   SendOptions{.m_scope = Scope::Network})));
			g_recorder.record(get_name() + ":broadcast:" + code(broadcast(raw(s_hello), s_hello.size(), SendOptions{.m_scope = Scope::Network})));
		}
	private:
		bool m_done = false;
	};

	// Moves once to a target, then records every turn it runs.
	struct Hopper final : cam::Agent {
		explicit Hopper(const std::string& p_name) : Agent(p_name) {}
		Hopper(const std::string& p_name, std::string p_target) : Agent(p_name), m_target(std::move(p_target)) {}
		void default_action() override {
			if (!m_moved) {
				m_moved = true;
				g_recorder.record(get_name() + ":can move:" + (can_move(m_target) ? "yes" : "no"));
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

	// Sends by id to its peer at every turn until it hears from it; counts what it receives.
	struct Pinger final : cam::Agent {
		Pinger(const std::string& p_name, std::atomic<int>& p_received) : Agent(p_name), m_received(p_received) {}
		void set_peer(const cam::AgentId p_peer) { m_peer = p_peer; }
		void action(const cam::MessagePointer&) override { m_received.fetch_add(1, std::memory_order_relaxed); }
		void default_action() override { (void) send(m_peer, raw(s_hello), s_hello.size()); }
	private:
		std::atomic<int>& m_received;
		cam::AgentId m_peer;
	};

} // namespace

TEST_SUITE("transparent_loopback") {

	TEST_CASE("without configuration, the environments of a process reach each other once started, in every mode") {
		for (const Mode l_mode: {Mode::Sequential, Mode::SequentialRandom, Mode::Parallel}) {
			CAPTURE(static_cast<int>(l_mode));
			g_recorder.clear();
			TestEnvironment l_a("A", l_mode);
			TestEnvironment l_b("B", l_mode);
			const cam::AgentId l_receiver = l_a.add<Receiver>("r");
			l_b.add<Sender>("s", l_receiver);
			CHECK(l_a.environments().empty());  // before start(), nothing changes
			// A announces itself before its agents are inserted: B learns r from the presence A answers to it
			l_a.start();
			l_b.start();
			l_a.start();
			l_b.start();
			l_a.start();
			CHECK(g_recorder.contains("s:sent"));
			CHECK(g_recorder.contains("r:hello"));
			CHECK(g_recorder.contains("A:joined"));
			CHECK(g_recorder.contains("B:joined"));
			CHECK(l_a.environments().size() == 1);
			CHECK(l_b.environments().size() == 1);
		}
	}

	TEST_CASE("the network scope reaches the other environments of the process") {
		g_recorder.clear();
		TestEnvironment l_a("A");
		TestEnvironment l_b("B");
		l_a.add<Receiver>("r");
		l_b.add<Crier>("c");
		l_a.start();
		l_b.start();
		l_b.start();
		l_a.start();
		CHECK(g_recorder.contains("c:by name:ok"));
		CHECK(g_recorder.contains("c:broadcast:ok"));
		CHECK(g_recorder.count_of("r:hello") == 2);
	}

	TEST_CASE("an agent moves to another environment of the process") {
		g_recorder.clear();
		TestEnvironment l_a("A");
		TestEnvironment l_b("B");
		l_a.register_agent_type<Hopper>("TransparentHopper");
		l_a.add<Hopper>("h", l_b.get_id());
		l_b.start();
		for (int l_turn = 0; l_turn < 6; ++l_turn) {
			l_a.start();
			l_b.start();
		}
		CHECK(g_recorder.contains("h:can move:yes"));
		CHECK(g_recorder.contains("h:move:ok"));
		CHECK(g_recorder.contains("h:running@B"));
		CHECK(l_a.agents_count() == 0);
		CHECK(l_b.agents_count() == 1);
	}

	TEST_CASE("alone on the loopback of the process, an environment answers as offline") {
		g_recorder.clear();
		{
			TestEnvironment l_a("A");
			l_a.add<Crier>("c");
			l_a.start();
			l_a.start();
		}
		// Alone too, in an environment of its own (the broadcast of the crier would reach it first)
		TestEnvironment l_b("B");
		l_b.add<Hopper>("h", "cccccccc-0000-4000-8000-000000000003");
		l_b.start();
		l_b.start();
		CHECK(g_recorder.contains("c:by name:not connected"));
		CHECK(g_recorder.contains("c:broadcast:not connected"));
		CHECK(g_recorder.contains("h:can move:no"));
		CHECK(g_recorder.contains("h:move:not connected"));
		CHECK(l_b.agents_count() == 1);
	}

	TEST_CASE("an environment that names its namespace is isolated from the others") {
		g_recorder.clear();
		TestEnvironment l_a("A");
		TestEnvironment l_b("B", Mode::Sequential, "loopback://transparent-other");
		TestEnvironment l_c("C", Mode::Sequential, "loopback://transparent-third");
		const cam::AgentId l_receiver = l_a.add<Receiver>("r");
		l_b.add<Sender>("s", l_receiver);
		l_a.start();
		l_b.start();
		l_c.start();
		l_b.start();
		l_a.start();
		CHECK(g_recorder.contains("s:unknown destination"));
		CHECK_FALSE(g_recorder.contains("r:hello"));
		CHECK(l_a.environments().empty());
		CHECK(l_b.environments().empty());
		CHECK(l_c.environments().empty());
	}

	TEST_CASE("the loopback joined without configuration stays hidden and gives way to an explicit connection") {
		g_recorder.clear();
		TestEnvironment l_a("A");
		TestEnvironment l_b("B");
		l_a.start();
		l_b.start();
		l_a.start();
		CHECK_FALSE(l_a.is_connected());
		CHECK(l_a.transport_url().empty());
		CHECK(l_a.statistics().m_sent == 0);
		REQUIRE(l_a.environments().size() == 1);
		REQUIRE(l_a.connect("loopback://transparent-explicit"));
		CHECK(l_a.is_connected());
		CHECK(l_a.transport_url() == "loopback://transparent-explicit");
		l_b.start();
		l_a.start();
		CHECK(online(l_a.environments()) == 0);
		CHECK(online(l_b.environments()) == 0);  // A left the loopback of the process
		REQUIRE(l_a.disconnect());
	}

	TEST_CASE("an explicit transport adds no loopback") {
		g_recorder.clear();
		auto l_transport = std::make_shared<LoopbackTransport>(TransportConfig{});
		REQUIRE(l_transport->open());
		TestEnvironment l_a("A");
		TestEnvironment l_b("B");
		REQUIRE(l_a.connect(l_transport));
		l_a.start();
		l_b.start();
		l_a.start();
		CHECK(l_a.environments().empty());
		CHECK(l_b.environments().empty());
		REQUIRE(l_a.disconnect());
	}

	TEST_CASE("disconnect leaves the loopback of the process for good") {
		g_recorder.clear();
		TestEnvironment l_a("A");
		TestEnvironment l_b("B");
		l_a.start();
		l_b.start();
		l_a.start();
		REQUIRE(l_b.environments().size() == 1);
		const auto l_result = l_a.disconnect();
		REQUIRE_FALSE(l_result);
		CHECK(l_result.error().m_code == ErrorCode::NotConnected);  // as before: it was not connected
		l_b.start();
		l_a.start();
		CHECK(online(l_b.environments()) == 0);  // A left
		CHECK(l_a.environments().empty());  // and did not join again
	}

	TEST_CASE("two environments of the process running at the same time reach each other") {
		std::atomic<int> l_received_a{0};
		std::atomic<int> l_received_b{0};
		TestEnvironment l_a("A", Mode::Parallel);
		TestEnvironment l_b("B", Mode::Parallel);
		auto l_pinger_a = std::make_shared<Pinger>("pa", l_received_a);
		auto l_pinger_b = std::make_shared<Pinger>("pb", l_received_b);
		l_pinger_a->set_peer(l_pinger_b->get_id());
		l_pinger_b->set_peer(l_pinger_a->get_id());
		l_a.add(l_pinger_a);
		l_b.add(l_pinger_b);
		const auto l_deadline = std::chrono::steady_clock::now() + 10s;
		const auto l_run = [&](cam::Environment& p_environment) {
			while ((l_received_a.load() == 0 || l_received_b.load() == 0) && std::chrono::steady_clock::now() < l_deadline) {
				p_environment.start();
			}
		};
		std::thread l_thread_a(l_run, std::ref(l_a));
		std::thread l_thread_b(l_run, std::ref(l_b));
		l_thread_a.join();
		l_thread_b.join();
		CHECK(l_received_a.load() > 0);
		CHECK(l_received_b.load() > 0);
	}
}
