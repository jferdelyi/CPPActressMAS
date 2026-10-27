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
#include <cstdint>
#include <map>
#include <memory>
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

	// Sends one text by id, by name, by fragment and by broadcast at its first action, then stops: its messages are
	// read once it is gone.
	struct Farewell final : cam::Agent {
		Farewell(const std::string& p_name, cam::AgentId p_reader_id) : Agent(p_name), m_reader_id(p_reader_id) {}
		void default_action() override {
			send(m_reader_id, std::string("by id"));
			send_by_name("reader", std::string("by name"));
			send_by_fragment_name("read", std::string("by fragment"));
			broadcast(std::string("by broadcast"));
			stop();
		}
	private:
		cam::AgentId m_reader_id;
	};

	// Records the format of every message it reads, and the name of its sender ("none" when unknown).
	struct Reader final : cam::Agent {
		explicit Reader(const std::string& p_name) : Agent(p_name) {}
		void action(const cam::MessagePointer& p_message) override {
			g_recorder.record(format(p_message));
			const std::string& l_name = p_message->get_sender_name();
			g_recorder.record("sender name:" + (l_name.empty() ? std::string("none") : l_name));
		}
	};

	// At its first action, adds "newcomer", then records what the searches see and reaches it by id, by name, by fragment
	// and by broadcast; with p_remove, removes it at once instead, then sends to it.
	struct Adder final : cam::Agent {
		Adder(const std::string& p_name, const bool p_remove) : Agent(p_name), m_remove(p_remove) {}
		void default_action() override {
			if (m_done) {
				return;
			}
			m_done = true;
			const cam::AgentId l_id = get_environment()->add<Collector>("newcomer");
			if (m_remove) {
				get_environment()->remove(l_id);
				send(l_id, std::string("after removal"));
				g_recorder.record("count after removal:" + std::to_string(agents_count()));
				return;
			}
			g_recorder.record("count:" + std::to_string(agents_count()));
			g_recorder.record("by name:" + std::to_string(get_agents_by_name("newcomer").size()));
			g_recorder.record("name:" + get_agent_name(l_id).value_or("none"));
			send(l_id, std::string("by id"));
			send_by_name("newcomer", std::string("by name"));
			send_by_fragment_name("newc", std::string("by fragment"));
			broadcast(std::string("by broadcast"));
		}
	private:
		bool m_remove;
		bool m_done = false;
	};

	// At its first action, adds "newcomer", then removes the inserted agent "victim" while the newcomer waits for its
	// insertion, and sends to the victim.
	struct Culler final : cam::Agent {
		explicit Culler(const std::string& p_name) : Agent(p_name) {}
		void default_action() override {
			if (m_done) {
				return;
			}
			m_done = true;
			get_environment()->add<Collector>("newcomer");
			const auto l_victims = get_agents_by_name("victim");
			if (l_victims.size() != 1) {
				g_recorder.record("victims:" + std::to_string(l_victims.size()));
				return;
			}
			get_environment()->remove(l_victims.front());
			send(l_victims.front(), std::string("after removal"));
			g_recorder.record("count after cull:" + std::to_string(agents_count()));
		}
	private:
		bool m_done = false;
	};

	// One of two agents of the same name: records "twin<index>:<text>" for every text it receives.
	struct Twin final : cam::Agent {
		Twin(const std::string& p_name, const int p_index) : Agent(p_name), m_index(p_index) {}
		void action(const cam::MessagePointer& p_message) override {
			g_recorder.record("twin" + std::to_string(m_index) + ":" + p_message->as_text());
		}
	private:
		int m_index;
	};

	// At its first action, removes one agent, then sends to the first agent of the name "twin" and to the first agent
	// whose name holds "wi".
	struct Picker final : cam::Agent {
		Picker(const std::string& p_name, const cam::AgentId p_removed) : Agent(p_name), m_removed(p_removed) {}
		void default_action() override {
			if (m_done) {
				return;
			}
			m_done = true;
			get_environment()->remove(m_removed);
			send_by_name("twin", std::string("by name"), true);
			send_by_fragment_name("wi", std::string("by fragment"), true);
		}
	private:
		cam::AgentId m_removed;
		bool m_done = false;
	};

	// Forwards every message it reads, without a copy, to an agent it holds: post() takes a message already built.
	struct Forwarder final : cam::Agent {
		Forwarder(const std::string& p_name, cam::AgentPointer p_target) : Agent(p_name), m_target(std::move(p_target)) {}
		void action(const cam::MessagePointer& p_message) override { m_target->post(p_message); }
	private:
		cam::AgentPointer m_target;
	};

	// The position of an entry in the recorder, or the number of entries when absent
	std::size_t position_of(const std::string& p_entry) {
		const auto l_entries = g_recorder.entries();
		return static_cast<std::size_t>(std::find(l_entries.begin(), l_entries.end(), p_entry) - l_entries.begin());
	}

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
		// The waiter stops once the added agents have run: the environment ends once all are gone
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
		// The worker stops the idle agent from inside (the case below removes it from another thread)
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

	TEST_CASE("a message names its sender once the sender is gone, whatever the mode and the way it was sent") {
		for (const auto& [l_mode, l_label]: {std::pair{Mode::Sequential, "Sequential"}, std::pair{Mode::SequentialRandom, "SequentialRandom"},
											  std::pair{Mode::Parallel, "Parallel"}}) {
			INFO(l_label);
			g_recorder.clear();
			cam::Environment l_env({.m_turns = 4, .m_mode = l_mode, .m_seed = 5});
			const cam::AgentId l_reader = l_env.add<Reader>("reader");
			l_env.add<Reader>("other");
			l_env.add<Farewell>("farewell", l_reader);
			l_env.start();
			CHECK(g_recorder.count_of("[farewell -> reader]: by id") == 1);
			CHECK(g_recorder.count_of("[farewell -> reader]: by name") == 1);
			CHECK(g_recorder.count_of("[farewell -> reader]: by fragment") == 1);
			CHECK(g_recorder.count_of("[farewell -> reader]: by broadcast") == 1);
			CHECK(g_recorder.count_of("[farewell -> other]: by broadcast") == 1);
			// Every message keeps the name of its sender
			CHECK(g_recorder.count_of("sender name:farewell") == 5);
		}
	}

	TEST_CASE("a message names no sender when it is built by hand or sent from outside the agents") {
		const cam::Message l_message(cam::AgentId("a"), cam::AgentId("b"), std::string("by hand"));
		CHECK(l_message.get_sender_name().empty());
		g_recorder.clear();
		cam::Environment l_env({.m_turns = 1, .m_mode = Mode::Sequential});
		const cam::AgentId l_reader = l_env.add<Reader>("reader");
		l_env.start();  // setup turn: the reader is inserted
		l_env.send_content("outside", l_reader, cam::MessageContent(std::string("from outside")));
		l_env.continue_simulation(1);
		CHECK(g_recorder.count_of("[outside -> reader]: from outside") == 1);  // the raw id, as before
		CHECK(g_recorder.count_of("sender name:none") == 1);
	}

	TEST_CASE("an agent added during a turn is reached at once by every send and search, and runs from the next turn") {
		for (const auto& [l_mode, l_label]: {std::pair{Mode::Sequential, "Sequential"}, std::pair{Mode::SequentialRandom, "SequentialRandom"},
											  std::pair{Mode::Parallel, "Parallel"}}) {
			INFO(l_label);
			g_recorder.clear();
			cam::Environment l_env({.m_turns = 4, .m_mode = l_mode, .m_seed = 5});
			l_env.add<Adder>("adder", false);
			l_env.start();
			CHECK(g_recorder.count_of("count:2") == 1);  // C#: Add is immediate
			CHECK(g_recorder.count_of("by name:1") == 1);
			CHECK(g_recorder.count_of("name:newcomer") == 1);
			for (const char* l_text: {"newcomer:by id", "newcomer:by name", "newcomer:by fragment", "newcomer:by broadcast"}) {
				INFO(l_text);
				CHECK(g_recorder.count_of(l_text) == 1);
			}
			// The newcomer did not run in the turn of its addition
			CHECK(position_of("count:2") < position_of("newcomer:setup"));
		}
	}

	TEST_CASE("an agent removed before its insertion leaves at once, and never runs") {
		for (const auto& [l_mode, l_label]: {std::pair{Mode::Sequential, "Sequential"}, std::pair{Mode::Parallel, "Parallel"}}) {
			INFO(l_label);
			g_recorder.clear();
			cam::Environment l_env({.m_turns = 4, .m_mode = l_mode, .m_seed = 5});
			l_env.add<Adder>("adder", true);
			l_env.start();
			CHECK(g_recorder.count_of("count after removal:1") == 1);  // C#: Remove is immediate
			CHECK_FALSE(g_recorder.contains("newcomer:setup"));
			CHECK_FALSE(g_recorder.contains("newcomer:after removal"));
			CHECK(l_env.agents_count() == 1);
		}
	}

	TEST_CASE("an inserted agent removed while added agents wait for their insertion leaves at once, and the added ones stay") {
		for (const auto& [l_mode, l_label]: {std::pair{Mode::Sequential, "Sequential"}, std::pair{Mode::SequentialRandom, "SequentialRandom"},
											  std::pair{Mode::Parallel, "Parallel"}}) {
			INFO(l_label);
			g_recorder.clear();
			cam::Environment l_env({.m_turns = 4, .m_mode = l_mode, .m_seed = 5});
			l_env.add<Collector>("victim");
			l_env.add<Culler>("culler");
			l_env.start();
			CHECK(g_recorder.count_of("count after cull:2") == 1);  // C#: Remove is immediate
			CHECK_FALSE(g_recorder.contains("victim:after removal"));
			CHECK(g_recorder.count_of("newcomer:setup") == 1);
			CHECK(l_env.agents_count() == 2);
			CHECK(l_env.get_agents_by_name("victim").empty());
		}
	}

	TEST_CASE("a message posted to another agent is read by it, with its first sender") {
		g_recorder.clear();
		cam::Environment l_env(3, Mode::Sequential);
		const auto l_reader = std::make_shared<Reader>("reader");  // built here, so that the forwarder holds it
		l_env.add(l_reader);
		const cam::AgentId l_forwarder = l_env.add<Forwarder>("forwarder", l_reader);
		l_env.start();  // turn 0: setup; then the forwarder reads the message below and posts it to the reader
		l_env.send_content("ext", l_forwarder, cam::MessageContent(std::string("posted")));
		l_env.continue_simulation(2);
		CHECK(g_recorder.count_of("[ext -> forwarder]: posted") == 1);  // the message itself: its receiver stays the forwarder
		CHECK(g_recorder.count_of("sender name:none") == 1);
	}

	TEST_CASE("a fragment is found anywhere in a name, by a search and by a send") {
		g_recorder.clear();
		cam::Environment l_env(1, Mode::Sequential);
		const cam::AgentId l_worker = l_env.add<Collector>("worker-3");
		l_env.add<Collector>("boss");
		l_env.start();  // setup turn
		const auto l_found = l_env.get_filtered_agents("ker");
		REQUIRE(l_found.size() == 1);
		CHECK(l_found[0] == l_worker);
		l_env.send_content_by_name("ext", "ker", cam::MessageContent(std::string("inside")), true, false);
		l_env.continue_simulation(1);
		CHECK(g_recorder.count_of("worker-3:inside") == 1);
		CHECK_FALSE(g_recorder.contains("boss:inside"));
	}

	TEST_CASE("a send to the first agent of a name or a fragment skips an agent removed in the turn, in every mode") {
		// Each twin in turn is the removed one: whatever the order of the name index, the removed twin comes first once
		for (const auto& [l_mode, l_label]: {std::pair{Mode::Sequential, "Sequential"}, std::pair{Mode::SequentialRandom, "SequentialRandom"},
											  std::pair{Mode::Parallel, "Parallel"}}) {
			for (const int l_removed: {0, 1}) {
				INFO(l_label << ", twin" << l_removed << " removed");
				g_recorder.clear();
				cam::Environment l_env({.m_turns = 3, .m_mode = l_mode, .m_seed = 5});
				const cam::AgentId l_twin0 = l_env.add<Twin>("twin", 0);
				const cam::AgentId l_twin1 = l_env.add<Twin>("twin", 1);
				l_env.add<Picker>("picker", l_removed == 0 ? l_twin0 : l_twin1);
				l_env.start();
				const std::string l_alive = "twin" + std::to_string(1 - l_removed);
				const std::string l_gone = "twin" + std::to_string(l_removed);
				CHECK(g_recorder.count_of(l_alive + ":by name") == 1);  // C#: Remove is immediate
				CHECK(g_recorder.count_of(l_alive + ":by fragment") == 1);
				CHECK_FALSE(g_recorder.contains(l_gone + ":by name"));
				CHECK_FALSE(g_recorder.contains(l_gone + ":by fragment"));
			}
		}
	}

	TEST_CASE("an agent added in the last turn of start is there after start, and runs at the next continue") {
		g_recorder.clear();
		cam::Environment l_env({.m_turns = 2, .m_mode = Mode::Sequential});  // turn 0: setup; turn 1: the adder adds
		l_env.add<Adder>("adder", false);
		l_env.start();
		CHECK(l_env.agents_count() == 2);
		CHECK(l_env.get_agents_by_name("newcomer").size() == 1);
		CHECK_FALSE(g_recorder.contains("newcomer:setup"));
		l_env.continue_simulation(2);
		CHECK(g_recorder.count_of("newcomer:setup") == 1);
	}

	TEST_CASE("agents added and messages sent from another thread while a Parallel start runs are all taken in, in order") {
		g_recorder.clear();
		cam::Environment l_env({.m_turns = 0, .m_mode = Mode::Parallel});
		const cam::AgentId l_collector = l_env.add<Collector>("collector");
		std::thread l_runner([&l_env] { l_env.start(); });
		REQUIRE(cam::test::wait_until([] { return g_recorder.contains("collector:setup"); }, std::chrono::seconds(30)));
		for (int i = 0; i < 100; i++) {
			l_env.add<Quitter>("quitter");
			l_env.send_content("outside", l_collector, cam::MessageContent(std::string("outside:" + std::to_string(i))));
		}
		l_env.send_content("outside", l_collector, cam::MessageContent(std::string("end")));
		l_runner.join();
		const auto l_numbers = per_sender("collector");
		REQUIRE(l_numbers.contains("outside"));
		const auto& l_received = l_numbers.at("outside");
		REQUIRE(l_received.size() == 100);
		for (int i = 0; i < 100; i++) {
			CHECK(l_received[i] == i);
		}
		CHECK(g_recorder.count_of("collector:end") == 1);
		CHECK(g_recorder.count_of("quit") == 100);
	}

	TEST_CASE("an environment kept alive by an idle agent ends once another thread removes that agent") {
		g_recorder.clear();
		cam::Environment l_env({.m_turns = 0, .m_mode = Mode::Parallel});
		const cam::AgentId l_idle = l_env.add<Idle>("idle");
		const cam::AgentId l_worker = l_env.add<Collector>("worker");
		std::thread l_runner([&l_env] { l_env.start(); });
		REQUIRE(cam::test::wait_until([] { return g_recorder.contains("worker:setup"); }, std::chrono::seconds(30)));
		for (int i = 0; i < 10; i++) {
			l_env.send_content("outside", l_worker, cam::MessageContent(std::string("outside:" + std::to_string(i))));
		}
		CHECK(cam::test::wait_until([] { return g_recorder.contains("worker:outside:9"); }, std::chrono::seconds(30)));
		l_env.remove(l_idle);
		l_env.send_content("outside", l_worker, cam::MessageContent(std::string("end")));
		l_runner.join();
		CHECK(per_sender("worker")["outside"].size() == 10);
		CHECK(g_recorder.count_of("worker:end") == 1);
		CHECK(l_env.agents_count() == 0);
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
