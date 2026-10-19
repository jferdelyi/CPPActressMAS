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

// Harness program of the oracle tests (tests/iso): the C# original as it is, its bugs included, compared with its
// oracle by iso.Agents3.
// Port of the C# example E00 Simple Examples/Agents3 (ActressMAS v3.1): a monitor runs three start/done rounds with
// four agents; the environment reports the end of each turn and of the simulation. Parallel, 10 turns, 100 ms after
// each turn. The monitor addresses the agents in the order of the environment, the order of insertion in C++ (the C#
// follows its ConcurrentDictionary).

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include <DotNet.hpp>
#include <Environment.h>

class MyEnvironment final : public cam::Environment {
	public:
		MyEnvironment() :
				Environment({.m_turns = 10,
							 .m_mode = cam::EnvironmentMasMode::Parallel,
							 .m_delay_after_turn = 100,
							 .m_seed = static_cast<unsigned int>(cam::dotnet::next_site_seed("EnvironmentMas.cs:61")),
							 .m_random = cam::RandomEngine::DotNet}) {}

		void turn_finished(const int p_turn) override {
			cam::dotnet::write_line("\nTurn " + std::to_string(p_turn + 1) + " finished\n");
		}

		void simulation_finished() override {
			cam::dotnet::write_line("\nSimulation finished\n");
		}
};

class MyAgent final : public cam::Agent {
	public:
		explicit MyAgent(const std::string& p_name) : Agent(p_name) {}

		void action(const cam::MessagePointer& p_message) override {
			cam::dotnet::write_line("\t" + format(p_message));
			const std::string l_content = p_message->to_string();
			if (get_agent_name(p_message->get_sender()) == "monitor" && (l_content == "start" || l_content == "continue")) {
				send_by_name("monitor", "done");
			}
		}
};

class MonitorAgent final : public cam::Agent {
	private:
		int m_round = 0;
		int m_max_rounds = 0;
		std::map<std::string, bool> m_finished;
		std::vector<std::string> m_agent_names;

		[[nodiscard]] bool all_finished() const {
			return std::ranges::all_of(m_finished, [](const auto& p_entry) { return p_entry.second; });
		}

	public:
		explicit MonitorAgent(const std::string& p_name) : Agent(p_name) {}

		void setup() override {
			m_max_rounds = 3;
			m_round = 0;

			cam::dotnet::write_line("monitor: start round 1 in setup");

			for (const auto& l_agent: get_filtered_agents("agent")) {
				const std::string l_name = get_agent_name(l_agent).value_or("");
				m_agent_names.push_back(l_name);
				m_finished.emplace(l_name, false);
			}

			for (const auto& l_name: m_agent_names) {
				cam::dotnet::write_line("-> sending to " + l_name);
				send_by_name(l_name, "start");
			}
		}

		void action(const cam::MessagePointer& p_message) override {
			cam::dotnet::write_line("\t" + format(p_message));

			if (p_message->to_string() == "done") {
				m_finished[get_agent_name(p_message->get_sender()).value_or("")] = true;
			}

			if (all_finished()) {
				if (++m_round >= m_max_rounds) {
					return;
				}

				for (const auto& l_name: m_agent_names) {
					m_finished[l_name] = false;
				}

				cam::dotnet::write_line("\r\nmonitor: start round " + std::to_string(m_round + 1) + " in act");
				for (const auto& l_name: m_agent_names) {
					cam::dotnet::write_line("-> sending to " + l_name);
					send_by_name(l_name, "continue");
				}
			}
		}
};

int main() {
	MyEnvironment l_environment;

	constexpr int l_no_agents = 4;
	for (int i = 1; i <= l_no_agents; i++) {
		l_environment.add<MyAgent>("agent" + std::to_string(i));
	}
	l_environment.add<MonitorAgent>("monitor");

	l_environment.start();
	return 0;
}
