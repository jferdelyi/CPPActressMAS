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

// Characterization of known defects whose symptom is a wrong value. Every case asserts the behaviour of ActressMAS C# v3.1 and is
// marked should_fail(): it is expected to fail while the defect exists. Once the defect is fixed,
// doctest reports that the case "should have failed" and the marker is removed; the assertions never
// change. Then the case stays, named "fixed: ...", as a regression test: every defect found and fixed keeps its
// test. Cases named "witness: ..." show behaviour that already matches the C#.
// Defects whose symptom is a crash or a data race are separate processes in probes.cpp.

#include <doctest/doctest.h>

#include <Environment.h>
#include <transport/codec/ContentCodec.h>
#include <transport/loopback/LoopbackTransport.h>

#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#if defined(__APPLE__)
#include <mach/mach.h>
#elif defined(__linux__)
#include <fstream>
#endif

namespace {

	using Mode = cam::EnvironmentMasMode;

	const std::uint8_t* raw(const char* p_text) {
		return reinterpret_cast<const std::uint8_t*>(p_text);
	}

	struct Plain final : cam::Agent {
		explicit Plain(const std::string& p_name) : Agent(p_name) {}
	};

	struct Quitter final : cam::Agent {  // stops at its first action (turn 1; turn 0 is the setup)
		explicit Quitter(const std::string& p_name) : Agent(p_name) {}
		void default_action() override { stop(); }
	};

	// Execution order of the sequential modes (main thread only).
	std::string g_order;
	struct Logger final : cam::Agent {
		explicit Logger(const std::string& p_name) : Agent(p_name) {}
		void default_action() override { g_order += get_name(); }
	};

	std::map<std::string, int> histogram(const std::string& p_order, const std::size_t p_group) {
		std::map<std::string, int> l_histogram;
		for (std::size_t i = 0; i + p_group <= p_order.size(); i += p_group) {
			l_histogram[p_order.substr(i, p_group)]++;
		}
		return l_histogram;
	}

	int g_received = 0;
	std::vector<std::string> g_processed;
	struct Inbox final : cam::Agent {
		explicit Inbox(const std::string& p_name) : Agent(p_name) {}
		void action(const cam::MessagePointer& p_message) override {
			++g_received;
			g_processed.push_back(get_name() + ":" + p_message->to_string());
		}
	};

} // namespace

TEST_SUITE("characterization") {

	// ---------------------------------------------------------------- agents added or stopped around start
	TEST_CASE("fixed: continue_simulation inserts the agents added after start") {
		cam::Environment l_env(1, Mode::Sequential);
		l_env.add<Plain>("a");
		l_env.start();
		l_env.add<Plain>("b");  // buffered; continue_simulation now opens with a turn boundary, as start()
		REQUIRE(l_env.agents_count() == 1);
		l_env.continue_simulation(2);
		CHECK(l_env.agents_count() == 2);  // C#: Add is immediate
	}

	TEST_CASE("fixed: an agent stopped during the last turn of start is removed") {
		cam::Environment l_env(2, Mode::Sequential);
		l_env.add<Quitter>("q");
		l_env.start();  // q stops at the last turn: no longer counted, destroyed at the next boundary
		CHECK(l_env.agents_count() == 0);  // C#: Stop removes immediately
	}

	TEST_CASE("fixed: continue_simulation removes the agents stopped before it") {
		cam::Environment l_env(2, Mode::Sequential);
		l_env.add<Quitter>("q");
		l_env.start();
		l_env.continue_simulation(3);  // bounded: continue_simulation(0) would never return here
		CHECK(l_env.agents_count() == 0);
	}

	// ---------------------------------------------------------------- random order
	TEST_CASE("fixed: the random order of three agents reaches the six permutations") {
		g_order.clear();
		cam::Environment l_env(2001, Mode::SequentialRandom, 0, 42);  // turn 0 = setup, then 2000 turns
		for (const char* l_name: {"A", "B", "C"}) {
			l_env.add<Logger>(l_name);
		}
		l_env.start();
		const auto l_histogram = histogram(g_order, 3);
		CHECK(l_histogram.size() == 6);  // (n-1)! orders before the Fisher-Yates of the C#: 2
		int l_last_is_c = 0;
		for (const auto& [l_key, l_count]: l_histogram) {
			if (l_key.back() == 'C') {
				l_last_is_c += l_count;
			}
		}
		CHECK(l_last_is_c > 0);  // the last inserted agent was never executed last before
	}

	TEST_CASE("fixed: the random order of two agents changes") {
		g_order.clear();
		cam::Environment l_env(1001, Mode::SequentialRandom, 0, 7);
		l_env.add<Logger>("A");
		l_env.add<Logger>("B");
		l_env.start();
		CHECK(histogram(g_order, 2).size() == 2);  // AB and BA
	}

	TEST_CASE("fixed: two environments with the same seed produce the same order") {
		const auto l_run = [](const unsigned p_seed) {
			g_order.clear();
			cam::Environment l_env(6, Mode::SequentialRandom, 0, p_seed);
			for (const char* l_name: {"A", "B", "C", "D", "E"}) {
				l_env.add<Logger>(l_name);
			}
			l_env.start();
			return g_order;
		};
		const std::string l_first = l_run(1);
		const std::string l_second = l_run(1);
		CHECK(l_first == l_second);  // the generator was static and shared by the whole process; one per environment now
	}

	// ---------------------------------------------------------------- message to an agent not inserted yet
	TEST_CASE("a message sent to an agent added but not yet inserted is delivered" * doctest::should_fail()) {
		g_received = 0;
		cam::Environment l_env(2, Mode::Sequential);
		const cam::AgentId l_id = l_env.add<Inbox>("dest");
		l_env.send("ext", l_id, raw("lost?"), 5);  // agent still in the buffer: message ignored
		l_env.start();                             // turn 0: setup; turn 1: empty queue
		CHECK(g_received == 1);  // C#: Add is immediate, the message is queued
	}

	// ---------------------------------------------------------------- migration while handling a message
	// The migrated queue no longer carries the message being handled nor the last consumed one: the
	// agent moves through the loopback transport (one turn per start(), see tests/integration/migration.cpp).
	std::string g_away;
	struct Migrant final : cam::Agent {  // moves while handling "go"
		explicit Migrant(const std::string& p_name) : Agent(p_name) {}
		void action(const cam::MessagePointer& p_message) override {
			g_processed.push_back(get_name() + ":" + p_message->to_string());
			if (p_message->to_string() == "go") {
				(void) move(g_away);
			}
		}
	};

	TEST_CASE("fixed: a copy moved while handling a message does not handle it again") {
		g_processed.clear();
		auto l_transport = std::make_shared<cam::transport::LoopbackTransport>();
		REQUIRE(l_transport->open());
		cam::Environment l_env(1, Mode::Sequential);
		cam::Environment l_copy_env(1, Mode::Sequential);
		l_env.register_agent_type<Migrant>("Migrant");
		l_copy_env.register_agent_type<Migrant>("Migrant");
		REQUIRE(l_copy_env.connect(l_transport));
		REQUIRE(l_env.connect(l_transport));
		g_away = l_copy_env.get_id();
		const cam::AgentId l_id = l_env.add<Migrant>("orig");
		l_env.start();  // setup turn
		l_env.send("ext", l_id, raw("go"), 2);
		l_env.start();  // handles "go" and asks to move during that handling
		REQUIRE(g_processed == std::vector<std::string>{"orig:go"});
		l_copy_env.start();  // offer accepted
		l_env.start();       // commit
		l_copy_env.start();  // inserted, setup
		l_copy_env.start();  // nothing to handle
		CHECK(l_copy_env.agents_count() == 1);
		CHECK(g_processed.size() == 1);
	}

	struct LateMigrant final : cam::Agent {  // moves in default_action, two turns after its last message
		explicit LateMigrant(const std::string& p_name) : Agent(p_name) {}
		int m_idle = 0;
		void action(const cam::MessagePointer& p_message) override {
			g_processed.push_back(get_name() + ":" + p_message->to_string());
		}
		void default_action() override {
			if (++m_idle == 2) {
				(void) move(g_away);
			}
		}
	};

	TEST_CASE("fixed: a copy moved two turns after its last message handles nothing") {
		g_processed.clear();
		auto l_transport = std::make_shared<cam::transport::LoopbackTransport>();
		REQUIRE(l_transport->open());
		cam::Environment l_env(1, Mode::Sequential);
		cam::Environment l_copy_env(1, Mode::Sequential);
		l_env.register_agent_type<LateMigrant>("LateMigrant");
		l_copy_env.register_agent_type<LateMigrant>("LateMigrant");
		REQUIRE(l_copy_env.connect(l_transport));
		REQUIRE(l_env.connect(l_transport));
		g_away = l_copy_env.get_id();
		const cam::AgentId l_id = l_env.add<LateMigrant>("orig");
		l_env.start();
		l_env.send("ext", l_id, raw("old"), 3);
		l_env.start();  // consumes "old"
		l_env.start();  // idle 1
		l_env.start();  // idle 2: asks to move
		REQUIRE(g_processed == std::vector<std::string>{"orig:old"});
		l_copy_env.start();
		l_env.start();
		l_copy_env.start();
		l_copy_env.start();
		CHECK(l_copy_env.agents_count() == 1);
		CHECK(g_processed.size() == 1);
	}

	int g_destroyed = 0;
	struct Tracked final : cam::Agent {
		explicit Tracked(const std::string& p_name) : Agent(p_name) {}
		~Tracked() override { ++g_destroyed; }
		void default_action() override { stop(); }
	};

	TEST_CASE("fixed: a removed agent is destroyed when it is removed") {
		g_destroyed = 0;
		{
			cam::Environment l_env(3, Mode::Sequential);
			l_env.add<Tracked>("a");
			l_env.add<Tracked>("b");
			l_env.start();  // turn 1: a and b stop; removed at the end of the turn
			CHECK(l_env.agents_count() == 0);
			CHECK(g_destroyed == 2);  // the queue moves an item out: its sentinel node no longer keeps the last added agent alive
		}
		CHECK(g_destroyed == 2);
	}

	// ---------------------------------------------------------------- agent stopped in its constructor
	struct StillBorn final : cam::Agent {
		explicit StillBorn(const std::string& p_name) : Agent(p_name) { stop(); }
	};

	TEST_CASE("fixed: an agent stopped in its constructor is never inserted") {
		cam::Environment l_env(1, Mode::Sequential);
		l_env.add<StillBorn>("zombie");
		l_env.start();  // inserted already dead: neither counted nor found, never run, erased at the next boundary
		CHECK(l_env.agents_count() == 0);
		CHECK(l_env.get_agents_by_name("zombie").empty());
	}

	// ---------------------------------------------------------------- setup of run_turn
	struct SetupCounter final : cam::Agent {
		explicit SetupCounter(const std::string& p_name) : Agent(p_name) {}
		int m_setups = 0;
		int m_defaults = 0;
		void setup() override { ++m_setups; }
		void default_action() override { ++m_defaults; }
	};

	TEST_CASE("run_turn(false) runs the setup once, in the same turn as the action" * doctest::should_fail()) {
		SetupCounter l_agent("same");  // Agent::run_turn is public; no environment needed
		for (int i = 0; i < 5; ++i) {
			l_agent.run_turn(false);
		}
		CHECK(l_agent.m_setups == 1);  // replayed at every turn today
		CHECK(l_agent.m_defaults == 5);
	}

	TEST_CASE("witness: run_turn(true) runs the setup alone at the first turn") {
		SetupCounter l_agent("separate");
		for (int i = 0; i < 5; ++i) {
			l_agent.run_turn(true);
		}
		CHECK(l_agent.m_setups == 1);
		CHECK(l_agent.m_defaults == 4);
	}

	// ---------------------------------------------------------------- exception thrown by an agent
	struct Thrower final : cam::Agent {
		explicit Thrower(const std::string& p_name) : Agent(p_name) {}
		void default_action() override { throw std::runtime_error("boom from " + get_name()); }
	};

	TEST_CASE("an exception thrown by an agent in Parallel mode propagates out of start" * doctest::should_fail()) {
		cam::Environment l_env(3, Mode::Parallel);
		l_env.add<Thrower>("t");
		CHECK_THROWS_AS(l_env.start(), std::exception);  // swallowed today: future::wait(), never get()
	}

	TEST_CASE("witness: an exception thrown by an agent in Sequential mode propagates out of start") {
		cam::Environment l_env(3, Mode::Sequential);
		l_env.add<Thrower>("t");
		CHECK_THROWS_AS(l_env.start(), std::runtime_error);
	}

	// Agents built without an identifier are gone with the protected constructor Agent(): every agent has an
	// identifier from construction.

	// ---------------------------------------------------------------- typed content of a message
	TEST_CASE("fixed: a message keeps its typed content locally and a JSON value refuses the RAW wire encoding") {
		const cam::Message l_message("s", "r", json("hello"));
		CHECK(l_message.kind() == cam::ContentKind::Json);
		CHECK(l_message.to_string() == "\"hello\"");
		const cam::Message l_text("s", "r", std::string("hello"));
		CHECK(l_text.kind() == cam::ContentKind::Text);
		CHECK(l_text.to_string() == "hello");
		const auto l_refused = cam::transport::wire_encoding(l_message.get_content(),
															cam::transport::SendOptions{.m_encoding = cam::MessageBinaryFormat::RAW});
		CHECK_FALSE(l_refused);  // used to be silently encoded in MessagePack and tagged RAW
	}

	// ---------------------------------------------------------------- move without a transport
	std::vector<std::string> g_move_results;
	struct Mover final : cam::Agent {
		explicit Mover(const std::string& p_name) : Agent(p_name) {}
		void default_action() override {
			const auto l_result = move("nowhere");
			g_move_results.emplace_back(l_result ? "ok" : cam::transport::to_string(l_result.error().m_code));
		}
	};

	TEST_CASE("fixed: move without a transport keeps the agent in its environment") {
		g_move_results.clear();
		cam::Environment l_env(3, Mode::Sequential);
		l_env.add<Mover>("m");
		l_env.start();  // turns 1 and 2: move() without a transport is refused, the agent stays
		CHECK(l_env.agents_count() == 1);
		CHECK(g_move_results == std::vector<std::string>{"not connected", "not connected"});
	}

	// ---------------------------------------------------------------- send by fragment
	std::vector<cam::AgentId> g_receivers;
	struct Named final : cam::Agent {
		explicit Named(const std::string& p_name) : Agent(p_name) {}
		void action(const cam::MessagePointer&) override { g_receivers.push_back(get_id()); }
	};

	TEST_CASE("the first agent found by fragment is the first agent sent to" * doctest::should_fail()) {
		g_receivers.clear();
		cam::Environment l_env(1, Mode::Sequential);
		for (int i = 1; i <= 9; ++i) {
			l_env.add<Named>("a" + std::to_string(i));
		}
		l_env.start();
		const auto l_lookup = l_env.get_filtered_agents("a", true);
		l_env.send_by_name("ext", "a", raw("x"), 1, true, true);
		l_env.continue_simulation(1);
		REQUIRE(l_lookup.size() == 1);
		REQUIRE(g_receivers.size() == 1);
		CHECK(l_lookup[0] == g_receivers[0]);  // insertion order for the lookup, hash order for the send
	}

	// ---------------------------------------------------------------- sender of a message from a removed agent
	std::string g_formatted;
	struct Farewell final : cam::Agent {  // says goodbye at its first action, then stops
		explicit Farewell(const std::string& p_name) : Agent(p_name) {}
		void default_action() override {
			send_by_name("listener", "bye");
			stop();
		}
	};

	struct Listener final : cam::Agent {
		explicit Listener(const std::string& p_name) : Agent(p_name) {}
		void action(const cam::MessagePointer& p_message) override { g_formatted = format(p_message); }
	};

	TEST_CASE("the format of a message names its sender, even once the sender is removed" * doctest::should_fail()) {
		g_formatted.clear();
		cam::Environment l_env(3, Mode::Sequential);
		l_env.add<Listener>("listener");
		l_env.add<Farewell>("farewell");
		l_env.start();  // turn 1: farewell sends and stops, removed at the boundary; turn 2: listener formats the message
		CHECK(g_formatted == "[farewell -> listener]: bye");  // C#: the message carries the name of its sender
	}

	// ---------------------------------------------------------------- threads of a Sequential environment
#if defined(__APPLE__) || defined(__linux__)
	int thread_count() {
#if defined(__APPLE__)
		thread_act_array_t l_list = nullptr;
		mach_msg_type_number_t l_count = 0;
		if (task_threads(mach_task_self(), &l_list, &l_count) != KERN_SUCCESS) {
			return -1;
		}
		for (mach_msg_type_number_t i = 0; i < l_count; ++i) {
			mach_port_deallocate(mach_task_self(), l_list[i]);
		}
		vm_deallocate(mach_task_self(), reinterpret_cast<vm_address_t>(l_list), l_count * sizeof(thread_act_t));
		return static_cast<int>(l_count);
#else
		std::ifstream l_status("/proc/self/status");
		for (std::string l_line; std::getline(l_status, l_line);) {
			if (l_line.rfind("Threads:", 0) == 0) {
				return std::stoi(l_line.substr(8));
			}
		}
		return -1;
#endif
	}

	TEST_CASE("fixed: a Sequential environment creates no thread") {
		const int l_before = thread_count();
		REQUIRE(l_before > 0);
		cam::Environment l_env(1, Mode::Sequential);
		CHECK(thread_count() == l_before);  // the turn threads exist in Parallel mode only
	}
#endif

	// ---------------------------------------------------------------- stop during a turn
	int g_after_stop = 0;
	int g_action_after_see_stop = 0;
	struct StopInAction final : cam::Agent {
		explicit StopInAction(const std::string& p_name) : Agent(p_name) {}
		void action(const cam::MessagePointer& p_message) override {
			if (is_dead()) {
				++g_after_stop;
			}
			if (p_message->to_string() == "1") {
				stop();
			}
		}
	};

	struct StopInSee final : cam::Agent {
		explicit StopInSee(const std::string& p_name) : Agent(p_name, true) {}
		void see(const std::vector<cam::ObservablesPointer>&) override { stop(); }
		void default_action() override { ++g_action_after_see_stop; }
	};

	TEST_CASE("fixed: the action of an agent that stops in see still runs in that turn") {
		g_action_after_see_stop = 0;
		cam::Environment l_env(2, Mode::Sequential);
		l_env.add<StopInSee>("v");
		l_env.start();  // turn 1: v stops in see(), its action runs all the same, as the C# runs Act
		CHECK(g_action_after_see_stop == 1);
	}

	TEST_CASE("witness: the messages already queued are handled after stop in action") {
		g_after_stop = 0;
		cam::Environment l_env(2, Mode::Sequential);
		const cam::AgentId l_id = l_env.add<StopInAction>("s");
		l_env.start();
		for (const char* l_text: {"1", "2", "3"}) {
			l_env.send("ext", l_id, raw(l_text), 1);
		}
		l_env.continue_simulation(1);  // s stops on "1" then handles "2" and "3" in the same turn
		CHECK(g_after_stop == 2);
	}
}
