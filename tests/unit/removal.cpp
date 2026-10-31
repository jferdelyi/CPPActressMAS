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

// Removal of agents: during a turn a removed agent is no longer counted, found by name nor sent to,
// and the turn boundary destroys it; in Parallel mode it still runs in the turn if it has not yet, as the C#;
// in the Sequential modes it is skipped if its turn has not come; outside a turn it is removed at once.
// Agents record into a Recorder and never assert; the tests check after start().

#include <doctest/doctest.h>

#include <atomic>
#include <cstddef>
#include <memory>
#include <string>
#include <thread>

#include <Environment.h>

#include "Recorder.hpp"

namespace {

	using Mode = cam::EnvironmentMasMode;

	cam::test::Recorder g_recorder;

	// Records each of its actions, and every message it receives.
	struct Victim final : cam::Agent {
		explicit Victim(const std::string& p_name) : Agent(p_name) {}
		void action(const cam::MessagePointer& p_message) override { g_recorder.record("victim:got:" + p_message->as_text()); }
		void default_action() override { g_recorder.record("victim:run"); }
	};

	// At its first action, removes every victim, sends each one a late message, then records what it sees.
	struct Killer final : cam::Agent {
		explicit Killer(const std::string& p_name) : Agent(p_name) {}
		void default_action() override {
			if (m_done) {
				return;
			}
			m_done = true;
			for (const cam::AgentId& l_id: get_agents_by_name("victim")) {
				get_environment()->remove(l_id);
				send(l_id, std::string("late"));
			}
			g_recorder.record("killer:count:" + std::to_string(agents_count()));
			g_recorder.record("killer:found:" + std::to_string(get_agents_by_name("victim").size()));
		}
	private:
		bool m_done = false;
	};

	// At its first action, removes itself, then goes on with its action.
	struct SelfRemover final : cam::Agent {
		explicit SelfRemover(const std::string& p_name) : Agent(p_name) {}
		void default_action() override {
			get_environment()->remove(get_id());
			g_recorder.record("self:after");
		}
	};

	// At its first action, removes itself twice and stops: once is enough.
	struct DoubleQuitter final : cam::Agent {
		explicit DoubleQuitter(const std::string& p_name) : Agent(p_name) {}
		void default_action() override {
			get_environment()->remove(get_id());
			get_environment()->remove(get_id());
			stop();
		}
	};

	// Stops at its first action, while the killer may remove it at the same time.
	struct StoppingVictim final : cam::Agent {
		explicit StoppingVictim(const std::string& p_name) : Agent(p_name) {}
		void default_action() override { stop(); }
	};

	// Does nothing: keeps the simulation going.
	struct Plain final : cam::Agent {
		explicit Plain(const std::string& p_name) : Agent(p_name) {}
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

	// At its first action, removes one agent.
	struct Remover final : cam::Agent {
		Remover(const std::string& p_name, const cam::AgentId p_removed) : Agent(p_name), m_removed(p_removed) {}
		void default_action() override {
			if (!m_done) {
				m_done = true;
				get_environment()->remove(m_removed);
			}
		}
	private:
		cam::AgentId m_removed;
		bool m_done = false;
	};

	std::atomic<int> g_destroyed{0};

	struct Tracked final : cam::Agent {
		explicit Tracked(const std::string& p_name) : Agent(p_name) {}
		~Tracked() override { ++g_destroyed; }
	};

} // namespace

TEST_SUITE("removal") {

	TEST_CASE("in Parallel mode a victim removed during a turn runs once in that turn, then never, and is gone at once") {
		g_recorder.clear();
		cam::Environment l_env(4, Mode::Parallel);
		l_env.add<Killer>("killer");
		for (int i = 0; i < 50; i++) {
			l_env.add<Victim>("victim");
		}
		l_env.start();  // turn 0: setup; turn 1: the killer removes the victims, each of them runs in this turn, as in the C#
		CHECK(g_recorder.count_of("victim:run") == 50);
		CHECK(g_recorder.count_of("victim:got:late") == 0);  // sent after the removal: dropped, as the C#
		CHECK(g_recorder.contains("killer:count:1"));
		CHECK(g_recorder.contains("killer:found:0"));
		CHECK(l_env.agents_count() == 1);
		CHECK(l_env.get_agents_by_name("victim").empty());
	}

	TEST_CASE("in Sequential mode a victim removed before its turn is skipped, after its turn it has run") {
		g_recorder.clear();
		{
			cam::Environment l_env(3, Mode::Sequential);
			l_env.add<Killer>("killer");
			l_env.add<Victim>("victim");
			l_env.start();
			CHECK(g_recorder.count_of("victim:run") == 0);  // removed earlier in turn 1: skipped, as the C# (ContainsKey)
			CHECK(l_env.agents_count() == 1);
		}
		g_recorder.clear();
		{
			cam::Environment l_env(3, Mode::Sequential);
			l_env.add<Victim>("victim");
			l_env.add<Killer>("killer");
			l_env.start();
			CHECK(g_recorder.count_of("victim:run") == 1);  // ran in turn 1 before the killer, never after
			CHECK(g_recorder.count_of("victim:got:late") == 0);
			CHECK(l_env.agents_count() == 1);
		}
	}

	TEST_CASE("an agent that removes itself finishes its action, in every mode") {
		for (const auto l_mode: {Mode::Sequential, Mode::SequentialRandom, Mode::Parallel}) {
			g_recorder.clear();
			cam::Environment l_env(4, l_mode);
			l_env.add<Plain>("plain");
			for (int i = 0; i < 5; i++) {
				l_env.add<SelfRemover>("self");
			}
			l_env.start();
			CHECK(g_recorder.count_of("self:after") == 5);
			CHECK(l_env.agents_count() == 1);
		}
	}

	TEST_CASE("an agent removed outside a turn is destroyed at once") {
		g_destroyed = 0;
		cam::Environment l_env(2, Mode::Sequential);
		const cam::AgentId l_id = l_env.add<Tracked>("tracked");
		l_env.add<Plain>("plain");
		l_env.start();
		REQUIRE(g_destroyed == 0);
		l_env.remove(l_id);
		CHECK(g_destroyed == 1);
		CHECK(l_env.agents_count() == 1);
		l_env.remove(l_id);  // already gone: ignored
		CHECK(l_env.agents_count() == 1);
	}

	TEST_CASE("removing one of two agents of the same name leaves the other reachable by that name") {
		// Each twin in turn is the removed one, outside a turn and during a turn: whatever the order of the name index, the
		// entry of the removed twin, and only it, leaves the index
		for (const bool l_during_turn: {false, true}) {
			for (const int l_removed: {0, 1}) {
				INFO((l_during_turn ? "during a turn" : "outside a turn") << ", twin" << l_removed << " removed");
				g_recorder.clear();
				cam::Environment l_env(3, Mode::Sequential);
				const cam::AgentId l_twin0 = l_env.add<Twin>("twin", 0);
				const cam::AgentId l_twin1 = l_env.add<Twin>("twin", 1);
				const cam::AgentId l_target = l_removed == 0 ? l_twin0 : l_twin1;
				if (l_during_turn) {
					l_env.add<Remover>("remover", l_target);
					l_env.start();  // turn 0: setup; turn 1: the remover removes; the boundary before turn 2 erases
				} else {
					l_env.start();
					l_env.remove(l_target);
				}
				l_env.send_content_by_name("ext", "twin", cam::MessageContent(std::string("hello")), false, false);
				l_env.continue_simulation(1);
				const std::string l_alive = "twin" + std::to_string(1 - l_removed);
				CHECK(g_recorder.count_of(l_alive + ":hello") == 1);
				CHECK_FALSE(g_recorder.contains("twin" + std::to_string(l_removed) + ":hello"));
				CHECK(l_env.get_agents_by_name("twin").size() == 1);
			}
		}
	}

	TEST_CASE("removing or stopping an agent twice counts it once, in every mode") {
		for (const auto l_mode: {Mode::Sequential, Mode::SequentialRandom, Mode::Parallel}) {
			g_recorder.clear();
			cam::Environment l_env(4, l_mode);
			l_env.add<Killer>("killer");
			for (int i = 0; i < 5; i++) {
				l_env.add<DoubleQuitter>("quitter");
				l_env.add<StoppingVictim>("victim");  // may stop itself while the killer removes it
			}
			l_env.start();
			CHECK(l_env.agents_count() == 1);
			CHECK(l_env.get_filtered_agents("i").size() == 1);  // only the killer
		}
	}

	TEST_CASE("an agent stopped from another thread while the boundary inserts it is counted once") {
		// This thread adds agents while the environment runs and stops each one at once, without any lock: the stop may
		// meet the insertion of the agent at a turn boundary. Each stopped agent must leave the count once, never twice nor
		// never.
		for (const auto l_mode: {Mode::Sequential, Mode::Parallel}) {
			INFO((l_mode == Mode::Sequential ? "Sequential" : "Parallel"));
			cam::Environment l_env(1000, l_mode);
			l_env.add<Plain>("plain");
			std::atomic<bool> l_done{false};
			std::thread l_runner([&l_env, &l_done] {
				l_env.start();
				l_done = true;
			});
			std::size_t l_added = 0;
			bool l_in_range = true;
			while (!l_done && l_added < 20000) {
				const auto l_agent = std::make_shared<Plain>("added");
				l_env.add(l_agent);
				l_agent->stop();
				++l_added;
				l_in_range = l_in_range && l_env.agents_count() <= l_added + 1;
			}
			l_runner.join();
			CHECK(l_in_range);
			l_env.continue_simulation(2);  // the last ones are inserted, then erased
			CHECK(l_env.agents_count() == 1);
			CHECK(l_env.get_filtered_agents("added").empty());
		}
	}
}
