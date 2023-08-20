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

#include <iostream>

#include <EnvironmentMas.h>

class MyEnvironment : public cam::EnvironmentMas {
	public:
		MyEnvironment() : EnvironmentMas(10, cam::EnvironmentMasMode::Parallel, 100) {}

		void turn_finished(int p_turn) override {
			std::cout << std::endl << "Turn " + std::to_string(p_turn + 1) + " finished" << std::endl << std::endl;
		}

		void simulation_finished() override {
			std::cout << std::endl << "Simulation finished" << std::endl << std::endl;
		}
};

class MonitorAgent : public cam::Agent {
	protected:
		int m_round;
		int m_max_rounds;
		std::map<std::string, bool> m_finished;
		std::vector<std::string> m_agent_names;

	public:
		MonitorAgent(const std::string& p_name): Agent(p_name) {}

		void setup() override {
			m_max_rounds = 3;
			m_round = 0;

			std::cout << "monitor: start round 1 in setup " << std::endl;

			// In C# version the order is alway 3,2,1,4
			// In C++ version the order is always 1,2,3,4
			// Probably explained by the type of container and 
			// find algorithme. I think this is not really critical
			for (const auto& l_agent : m_environment->filtered_agents("agent")) {
				m_agent_names.push_back(l_agent->get_name());
				m_finished.emplace(l_agent->get_name(), false);
			}

			for (int i = 0; i < m_agent_names.size(); i++) {
				std::cout << "-> sending to " << m_agent_names[i] << std::endl;
				send(m_agent_names[i], "start");
			}
		}

		void action(const cam::MessagePointer& p_message) override {
			std::cout << "\t" << p_message->format() << std::endl;

			if (p_message->content() == "done") {
				m_finished[p_message->get_sender()] = true;
			}

			if (all_finished()) {
				if (++m_round >= m_max_rounds) {
					return;
				}

				for (int i = 0; i < m_agent_names.size(); i++) {
					m_finished[m_agent_names[i]] = false;
				}

				std::cout << "monitor: start round " + std::to_string(m_round + 1) + " in act";

				for (int i = 0; i < m_agent_names.size(); i++) {
					std::cout << "-> sending to " << m_agent_names[i] << std::endl;
					send(m_agent_names[i], "continue");
				}
			}
		}

	protected:
		inline bool all_finished() const {
			for (const auto& l_finished : m_finished) {
				if (!l_finished.second) {
					return false;
				}
			}
			return true;
		}
};

class MyAgent : public cam::Agent {
	public:
		MyAgent(const std::string& p_name): Agent(p_name) {}

		void action(const cam::MessagePointer& p_message) override {
			std::cout << "\t" << p_message->format() << std::endl;
			if (p_message->get_sender() == "monitor" && (p_message->content() == "start" || p_message->content() == "continue")) {
				send("monitor", "done");
			}
		}
};

int main() {
	MyEnvironment l_environment;

	for (int i = 1; i <= 4; i++) {
		l_environment.add(cam::AgentPointer(new MyAgent("agent" + std::to_string(i))));
	}
	l_environment.add(cam::AgentPointer(new MonitorAgent("monitor")));

	l_environment.start();
	return 0;
}
