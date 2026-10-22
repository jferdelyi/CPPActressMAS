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

// Delivery and turn semantics that the C# and the actor frameworks it cites document: the order of the messages of
// one sender (first-in first-out per pair, as Akka.NET and Orleans), messages and agents that come from outside while
// the environment runs (a service, as the use of ActressMAS at GECAD), a JSON content shared by its receivers and
// read only (the C# shares a mutable object), and the points of the turn loop of the C# manual: the action decided at
// activation, no perception in the setup turn, a message to oneself read in the same activation, the end hook at each
// start and continue.

#include <doctest/doctest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <map>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#include <Environment.h>

#include "Recorder.hpp"
#include "wait_until.hpp"

namespace {

	using Mode = cam::EnvironmentMasMode;

	cam::test::Recorder g_recorder;

	// Turn being run, set by the environment at the end of each turn (0 is the setup turn)
	std::atomic<int> g_turn{0};

	struct TurnEnvironment final : cam::Environment {
		using cam::Environment::Environment;
		void turn_finished(const int p_turn) override { g_turn = p_turn + 1; }
		void simulation_finished() override { g_recorder.record("finished"); }
	};

	// Records every text it receives as "<content>"; stops at "end".
	struct Collector final : cam::Agent {
		explicit Collector(const std::string& p_name) : Agent(p_name) {}
		void setup() override { g_recorder.record(get_name() + ":setup"); }
		void action(const cam::MessagePointer& p_message) override {
			g_recorder.record(get_name() + ":" + p_message->as_text());
			if (p_message->as_text() == "end") {
				stop();
			}
		}
	};

	// Sends p_per_turn numbered texts to a receiver at each of its first p_turns actions.
	struct Burst final : cam::Agent {
		Burst(const std::string& p_name, cam::AgentId p_receiver_id, const int p_per_turn, const int p_turns) :
				Agent(p_name), m_receiver_id(p_receiver_id), m_per_turn(p_per_turn), m_turns(p_turns) {}
		void default_action() override {
			if (m_turns == 0) {
				return;
			}
			m_turns--;
			for (int i = 0; i < m_per_turn; i++) {
				send(m_receiver_id, get_name() + ":" + std::to_string(m_next++));
			}
		}
	private:
		cam::AgentId m_receiver_id;
		int m_per_turn;
		int m_turns;
		int m_next = 0;
	};

	// Stops at its first action.
	struct Quitter final : cam::Agent {
		explicit Quitter(const std::string& p_name) : Agent(p_name) {}
		void default_action() override {
			g_recorder.record("quit");
			stop();
		}
	};

	// Does nothing but keep an environment alive, until it reads "stop".
	struct Idle final : cam::Agent {
		explicit Idle(const std::string& p_name) : Agent(p_name) {}
		void action(const cam::MessagePointer& p_message) override {
			if (p_message->as_text() == "stop") {
				g_recorder.record(get_name() + ":stopped");
				stop();
			}
		}
	};

	// Records every text it receives as "<name>:<content>"; at "end", tells the idle agent to stop, then stops.
	struct Worker final : cam::Agent {
		explicit Worker(const std::string& p_name) : Agent(p_name) {}
		void setup() override { g_recorder.record(get_name() + ":setup"); }
		void action(const cam::MessagePointer& p_message) override {
			g_recorder.record(get_name() + ":" + p_message->as_text());
			if (p_message->as_text() == "end") {
				send_by_name("idle", std::string("stop"));
				stop();
			}
		}
	};

	// Stops once p_expected agents have quit.
	struct Waiter final : cam::Agent {
		Waiter(const std::string& p_name, const std::size_t p_expected) : Agent(p_name), m_expected(p_expected) {}
		void setup() override { g_recorder.record(get_name() + ":setup"); }
		void default_action() override {
			if (g_recorder.count_of("quit") == m_expected) {
				g_recorder.record(get_name() + ":end");
				stop();
			}
		}
	private:
		std::size_t m_expected;
	};

	// Records the address of the JSON value it receives.
	struct JsonReader final : cam::Agent {
		explicit JsonReader(const std::string& p_name) : Agent(p_name) {}
		void action(const cam::MessagePointer& p_message) override {
			static_assert(std::is_same_v<decltype(p_message->as_json()), const json&>, "a JSON content is read only");
			g_recorder.record("json:" + std::to_string(reinterpret_cast<std::uintptr_t>(&p_message->as_json())));
		}
	};

	// Broadcasts a JSON value at setup.
	struct JsonBroadcaster final : cam::Agent {
		explicit JsonBroadcaster(const std::string& p_name) : Agent(p_name) {}
		void setup() override { broadcast(json{{"value", 42}}); }
	};

	// Records "<name>:default:<turn>" or "<name>:<content>:<turn>"; the one named "b" sends "ping" to "a" at its first
	// action.
	struct Activated final : cam::Agent {
		explicit Activated(const std::string& p_name) : Agent(p_name) {}
		void default_action() override {
			g_recorder.record(get_name() + ":default:" + std::to_string(g_turn.load()));
			if (get_name() == "b" && !m_pinged) {
				m_pinged = true;
				send_by_name("a", std::string("ping"));
			}
		}
		void action(const cam::MessagePointer& p_message) override {
			g_recorder.record(get_name() + ":" + p_message->as_text() + ":" + std::to_string(g_turn.load()));
		}
	private:
		bool m_pinged = false;
	};

	// Observes the others and records "see:<turn>" at each perception.
	struct Observer final : cam::Agent {
		explicit Observer(const std::string& p_name) : Agent(p_name, true) {}
		void setup() override { (*m_observables)["name"] = get_name(); }
		[[nodiscard]] bool perception_filter(const cam::ObservablesPointer&) const override { return true; }
		void see(const std::vector<cam::ObservablesPointer>&) override { g_recorder.record("see:" + std::to_string(g_turn.load())); }
	};

	// Sends itself "self" at setup, and again from each of its first two readings; records the turn of each reading.
	struct SelfSender final : cam::Agent {
		explicit SelfSender(const std::string& p_name) : Agent(p_name) {}
		void setup() override { send(get_id(), std::string("self")); }
		void action(const cam::MessagePointer&) override {
			g_recorder.record("self:" + std::to_string(g_turn.load()));
			if (++m_readings < 3) {
				send(get_id(), std::string("self"));
			}
		}
	private:
		int m_readings = 0;
	};

	// The numbers each sender of "<sender>:<n>" entries of a collector sent, in their order of arrival
	std::map<std::string, std::vector<int>> per_sender(const std::string& p_collector) {
		std::map<std::string, std::vector<int>> l_numbers;
		for (const auto& l_entry: g_recorder.entries()) {
			const std::string l_prefix = p_collector + ":";
			if (l_entry.rfind(l_prefix, 0) != 0) {
				continue;
			}
			const std::string l_content = l_entry.substr(l_prefix.size());
			const auto l_colon = l_content.find(':');
			if (l_colon != std::string::npos) {
				l_numbers[l_content.substr(0, l_colon)].push_back(std::stoi(l_content.substr(l_colon + 1)));
			}
		}
		return l_numbers;
	}

} // namespace

TEST_SUITE("delivery") {

	TEST_CASE("the messages of one sender reach their receiver in the order they were sent, in every mode") {
		for (const auto& [l_mode, l_label]: {std::pair{Mode::Sequential, "Sequential"}, std::pair{Mode::SequentialRandom, "SequentialRandom"},
											  std::pair{Mode::Parallel, "Parallel"}}) {
			INFO(l_label);
			g_recorder.clear();
			cam::Environment l_env({.m_turns = 8, .m_mode = l_mode, .m_seed = 3});
			const cam::AgentId l_collector = l_env.add<Collector>("collector");
			l_env.add<Burst>("s1", l_collector, 50, 4);
			l_env.add<Burst>("s2", l_collector, 50, 4);
			l_env.start();
			const auto l_numbers = per_sender("collector");
			REQUIRE(l_numbers.size() == 2);
			for (const auto& [l_sender, l_received]: l_numbers) {
				INFO(l_sender);
				REQUIRE(l_received.size() == 200);
				for (int i = 0; i < 200; i++) {
					CHECK(l_received[i] == i);
				}
			}
		}
	}

	TEST_CASE("messages sent from another thread while a Parallel start runs are all taken in, in order") {
		g_recorder.clear();
		cam::Environment l_env({.m_turns = 0, .m_mode = Mode::Parallel});
		const cam::AgentId l_collector = l_env.add<Collector>("collector");
		std::thread l_runner([&l_env] { l_env.start(); });
		REQUIRE(cam::test::wait_until([] { return g_recorder.contains("collector:setup"); }, std::chrono::seconds(30)));
		for (int i = 0; i < 100; i++) {
			l_env.send_content("outside", l_collector, cam::MessageContent(std::string("outside:" + std::to_string(i))));
		}
		l_env.send_content("outside", l_collector, cam::MessageContent(std::string("end")));
		l_runner.join();
		const auto l_numbers = per_sender("collector");
		REQUIRE(l_numbers.contains("outside"));
		const auto& l_received = l_numbers.at("outside");
		std::string l_missing;
		for (int i = 0; i < 100; i++) {
			if (std::find(l_received.begin(), l_received.end(), i) == l_received.end()) {
				l_missing += " " + std::to_string(i);
			}
		}
		INFO("missing:" << l_missing);
		REQUIRE(l_received.size() == 100);
		for (int i = 0; i < 100; i++) {
			CHECK(l_received[i] == i);
		}
		CHECK(g_recorder.count_of("collector:end") == 1);
	}

	TEST_CASE("agents added from another thread while a Parallel start runs all run") {
		g_recorder.clear();
		cam::Environment l_env({.m_turns = 0, .m_mode = Mode::Parallel});
		l_env.add<Waiter>("waiter", 20);
		std::thread l_runner([&l_env] { l_env.start(); });
		REQUIRE(cam::test::wait_until([] { return g_recorder.contains("waiter:setup"); }, std::chrono::seconds(30)));
		for (int i = 0; i < 20; i++) {
			l_env.add<Quitter>("quitter");
		}
		// The waiter stops once the added agents have run: the environment ends once all are gone. Nothing is sent from
		// here once they start to leave: a send from another thread races with the turn boundary that erases them
		// (characterization probe outside_add_and_send_parallel).
		l_runner.join();
		CHECK(g_recorder.count_of("quit") == 20);
		CHECK(g_recorder.count_of("waiter:end") == 1);
	}

	TEST_CASE("an environment kept alive by an idle agent serves messages from outside, and ends once that agent stops") {
		g_recorder.clear();
		cam::Environment l_env({.m_turns = 0, .m_mode = Mode::Parallel});
		l_env.add<Idle>("idle");
		const cam::AgentId l_worker = l_env.add<Worker>("worker");
		std::thread l_runner([&l_env] { l_env.start(); });
		REQUIRE(cam::test::wait_until([] { return g_recorder.contains("worker:setup"); }, std::chrono::seconds(30)));
		for (int i = 0; i < 10; i++) {
			l_env.send_content("outside", l_worker, cam::MessageContent(std::string("outside:" + std::to_string(i))));
		}
		CHECK(cam::test::wait_until([] { return g_recorder.contains("worker:outside:9"); }, std::chrono::seconds(30)));
		// The worker stops the idle agent: a removal from another thread races with the turn boundary that erases it
		// (characterization probe outside_remove_and_send_parallel)
		l_env.send_content("outside", l_worker, cam::MessageContent(std::string("end")));
		l_runner.join();
		CHECK(per_sender("worker")["outside"].size() == 10);
		CHECK(g_recorder.count_of("worker:end") == 1);
		CHECK(g_recorder.count_of("idle:stopped") == 1);
		CHECK(l_env.agents_count() == 0);
	}

	TEST_CASE("a broadcast JSON value is one read-only value shared by its receivers") {
		g_recorder.clear();
		cam::Environment l_env({.m_turns = 3, .m_mode = Mode::Sequential});
		l_env.add<JsonBroadcaster>("broadcaster");
		for (int i = 0; i < 3; i++) {
			l_env.add<JsonReader>("reader");
		}
		l_env.start();
		std::vector<std::string> l_addresses;
		for (const auto& l_entry: g_recorder.entries()) {
			if (l_entry.rfind("json:", 0) == 0) {
				l_addresses.push_back(l_entry);
			}
		}
		REQUIRE(l_addresses.size() == 3);
		CHECK(l_addresses[1] == l_addresses[0]);
		CHECK(l_addresses[2] == l_addresses[0]);
	}

	TEST_CASE("the action of an agent is decided when it is activated: an empty mailbox gives its default action") {
		g_recorder.clear();
		g_turn = 0;
		TurnEnvironment l_env({.m_turns = 4, .m_mode = Mode::Sequential});
		l_env.add<Activated>("a");
		l_env.add<Activated>("b");
		l_env.start();
		// a acts before b in turn 1, with an empty mailbox; the ping b sends then is read by a in turn 2
		CHECK(g_recorder.contains("a:default:1"));
		CHECK(g_recorder.contains("b:default:1"));
		CHECK(g_recorder.contains("a:ping:2"));
		CHECK_FALSE(g_recorder.contains("a:default:2"));
		CHECK(g_recorder.contains("a:default:3"));
	}

	TEST_CASE("no agent perceives the others in the setup turn") {
		g_recorder.clear();
		g_turn = 0;
		TurnEnvironment l_env({.m_turns = 3, .m_mode = Mode::Sequential});
		l_env.add<Observer>("o1");
		l_env.add<Observer>("o2");
		l_env.start();
		CHECK_FALSE(g_recorder.contains("see:0"));
		CHECK(g_recorder.count_of("see:1") == 2);
		CHECK(g_recorder.count_of("see:2") == 2);
	}

	TEST_CASE("a message an agent sends itself while reading is read in the same activation") {
		g_recorder.clear();
		g_turn = 0;
		TurnEnvironment l_env({.m_turns = 3, .m_mode = Mode::Sequential});
		l_env.add<SelfSender>("self");
		l_env.start();
		CHECK(g_recorder.count_of("self:1") == 3);
		CHECK_FALSE(g_recorder.contains("self:2"));
	}

	TEST_CASE("the end hook runs at the end of each start and of each continue") {
		g_recorder.clear();
		TurnEnvironment l_env({.m_turns = 2, .m_mode = Mode::Sequential});
		l_env.add<Idle>("idle");
		l_env.start();
		CHECK(g_recorder.count_of("finished") == 1);
		l_env.continue_simulation(1);
		CHECK(g_recorder.count_of("finished") == 2);
	}
}
