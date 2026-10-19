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

// Random order of the sequential modes: with RandomEngine::DotNet an environment runs its agents in the order of
// the C# turn by turn, against the traces of tests/unit/vectors/rng/trace-order.txt (TraceOrder of tests/tools/csharp-ref, .NET 10);
// with RandomEngine::Mt19937 the order of a seed is the same with every standard library. The environments are
// sequential: the agents write into a plain log on the calling thread.

#include <doctest/doctest.h>

#include <fstream>
#include <map>
#include <string>
#include <vector>

#include <Environment.h>
#include <util/DotnetRandom.h>

namespace {

	using Mode = cam::EnvironmentMasMode;

	// What ran in the current turn ("X(setup)" or "X"), and one line per finished turn
	std::vector<std::string> g_turn;
	std::vector<std::string> g_turns;

	struct TraceEnvironment final : cam::Environment {
		using cam::Environment::Environment;
		void turn_finished(const int p_turn) override {
			std::string l_line = "turn " + std::to_string(p_turn) + ":";
			for (const auto& l_entry: g_turn) {
				l_line += " " + l_entry;
			}
			g_turns.push_back(l_line);
			g_turn.clear();
		}
	};

	struct NamedAgent : cam::Agent {
		explicit NamedAgent(const std::string& p_name) : Agent(p_name) {}
		void setup() override { g_turn.push_back(get_name() + "(setup)"); }
		void default_action() override { g_turn.push_back(get_name()); }
	};

	// B stops in its first action (turn 1), as StoppingAgent of TraceOrder
	struct StoppingAgent final : NamedAgent {
		explicit StoppingAgent(const std::string& p_name) : NamedAgent(p_name) {}
		void default_action() override {
			NamedAgent::default_action();
			stop();
		}
	};

	// E adds F in its first action (turn 1), as SpawningAgent of TraceOrder
	struct SpawningAgent final : NamedAgent {
		explicit SpawningAgent(const std::string& p_name) : NamedAgent(p_name) {}
		void default_action() override {
			NamedAgent::default_action();
			if (!m_spawned) {
				m_spawned = true;
				get_environment()->add<NamedAgent>("F");
			}
		}
	private:
		bool m_spawned = false;
	};

	// The turn lines of each scenario of the C# traces, without their "| keys after turn" part
	std::map<std::string, std::vector<std::string>> read_traces() {
		std::ifstream l_file(std::string(CAM_VECTORS_DIR) + "/rng/trace-order.txt");
		REQUIRE(l_file.good());
		std::map<std::string, std::vector<std::string>> l_traces;
		std::string l_scenario;
		for (std::string l_line; std::getline(l_file, l_line);) {
			if (l_line.empty() || l_line[0] == '#') {
				continue;
			}
			if (l_line[0] != ' ') {
				l_scenario = l_line.substr(0, l_line.find(' '));
				continue;
			}
			std::string l_turn = l_line.substr(l_line.find_first_not_of(' '));
			l_turn = l_turn.substr(0, l_turn.find("   |"));
			l_traces[l_scenario].push_back(l_turn);
		}
		return l_traces;
	}

	std::vector<std::string> run(cam::EnvironmentConfig p_config, const bool p_stop_and_spawn) {
		g_turn.clear();
		g_turns.clear();
		TraceEnvironment l_env(p_config);
		for (const char* l_name: {"A", "B", "C", "D", "E"}) {
			const std::string l_text = l_name;
			if (p_stop_and_spawn && l_text == "B") {
				l_env.add<StoppingAgent>(l_text);
			} else if (p_stop_and_spawn && l_text == "E") {
				l_env.add<SpawningAgent>(l_text);
			} else {
				l_env.add<NamedAgent>(l_text);
			}
		}
		l_env.start();
		return g_turns;
	}

} // namespace

TEST_SUITE("random_order") {

	TEST_CASE("with the DotNet generator the random order follows the C# turn by turn (S1)") {
		const auto l_traces = read_traces();
		CHECK(run({.m_turns = 3, .m_mode = Mode::SequentialRandom, .m_seed = 1, .m_random = cam::RandomEngine::DotNet}, false)
			  == l_traces.at("S1"));
	}

	TEST_CASE("the fixed order follows the C# (S2)") {
		const auto l_traces = read_traces();
		CHECK(run({.m_turns = 3, .m_mode = Mode::Sequential}, false) == l_traces.at("S2"));
	}

	TEST_CASE("an agent stopped and an agent added during a random turn keep the C# order (S4)") {
		const auto l_traces = read_traces();
		// S3 is not replayed: after a removal the C# Dictionary reuses the freed slot, which the port does not reproduce
		CHECK(run({.m_turns = 4, .m_mode = Mode::SequentialRandom, .m_seed = 1, .m_random = cam::RandomEngine::DotNet}, true)
			  == l_traces.at("S4"));
	}

	TEST_CASE("with the Mt19937 generator a seed gives the same order with every standard library") {
		// Pinned with libc++ (macOS); the same with libstdc++ (Linux) since the bounded draw no longer depends on it
		const std::vector<std::string> l_expected = {
				"turn 0: E(setup) A(setup) C(setup) D(setup) B(setup)",
				"turn 1: A C B E D",
				"turn 2: D C E B A",
				"turn 3: D E C B A"};
		CHECK(run({.m_turns = 4, .m_mode = Mode::SequentialRandom, .m_seed = 42}, false) == l_expected);
	}

	TEST_CASE("random_agent draws once over the agents in their order, as RandomAgent of the C#") {
		cam::Environment l_env({.m_turns = 1, .m_mode = Mode::Sequential, .m_seed = 5, .m_random = cam::RandomEngine::DotNet});
		std::vector<cam::AgentId> l_ids;
		for (const char* l_name: {"A", "B", "C", "D", "E"}) {
			l_ids.push_back(l_env.add<NamedAgent>(l_name));
		}
		l_env.start();  // inserted; the fixed order draws nothing
		cam::DotnetRandom l_witness(5);
		for (int i = 0; i < 20; i++) {
			CHECK(l_env.random_agent() == l_ids[static_cast<std::size_t>(l_witness.next(5))]);
		}
	}

	TEST_CASE("random_agent returns nothing when no agent is left, after one draw as the C#") {
		cam::Environment l_env({.m_turns = 1, .m_mode = Mode::Sequential, .m_seed = 5, .m_random = cam::RandomEngine::DotNet});
		CHECK_FALSE(l_env.random_agent().has_value());
		const cam::AgentId l_only = l_env.add<NamedAgent>("A");
		l_env.start();
		cam::DotnetRandom l_witness(5);
		CHECK(l_witness.next(0) == 0);  // the draw of the empty environment
		CHECK(l_env.random_agent() == l_only);
		CHECK(l_witness.next(1) == 0);
	}

	TEST_CASE("random_agent never returns a stopped agent") {
		g_turn.clear();
		g_turns.clear();
		TraceEnvironment l_env(2, Mode::Sequential);
		l_env.add<NamedAgent>("A");
		const cam::AgentId l_stopped = l_env.add<StoppingAgent>("B");
		l_env.start();  // B stops at the last turn: still in the collection, no longer counted
		for (int i = 0; i < 50; i++) {
			const auto l_picked = l_env.random_agent();
			REQUIRE(l_picked.has_value());
			CHECK(*l_picked != l_stopped);
		}
	}

	TEST_CASE("the seed in use is the given one, or a drawn one") {
		CHECK(cam::Environment({.m_seed = 7}).seed() == 7);
		CHECK(cam::Environment(1, Mode::Sequential, 0, 9).seed() == 9);
		CHECK(cam::Environment({.m_turns = 1}).seed() != cam::Environment({.m_turns = 1}).seed());  // drawn: equal once in 2^32
	}
}
