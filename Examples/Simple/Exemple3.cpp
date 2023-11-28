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

// For convenience only to be closest as possible to the C# version
static std::unordered_map<std::string, std::string> s_name_id;

class MyEnvironment final : public cam::EnvironmentMas {
	public:
		MyEnvironment() : EnvironmentMas(10, cam::EnvironmentMasMode::Parallel, 100) {}

		void turn_finished(const int p_turn) override {
			std::cout << std::endl << "Turn " + std::to_string(p_turn + 1) + " finished" << std::endl << std::endl;
		}

		void simulation_finished() override {
			std::cout << std::endl << "Simulation finished" << std::endl << std::endl;
		}
};

class MonitorAgent final : public cam::Agent {
	protected:
		int m_round{};
		int m_max_rounds{};
		std::map<std::string, bool> m_finished;
		std::vector<std::string> m_agent_names;

	public:
		explicit MonitorAgent(const std::string& p_name) : Agent(p_name) {
			s_name_id.emplace(m_name, m_id);
		}

		void setup() override {
			m_max_rounds = 3;
			m_round = 0;

			std::cout << "[" + m_name + "]: start round 1 in setup " << std::endl;

			// In C# version the order is alway 3,2,1,4
			// In C++ version the order is always 4,3,2,1
			// Probably explained by the type of container and 
			// find algorithme. I think this is not really critical
			for (const auto& [l_agent_name, l_agent_id] : s_name_id) {
				if (l_agent_name.find("agent") != std::string::npos) {
					const auto& l_agent = m_environment->get(l_agent_id);
					m_agent_names.push_back(l_agent->get_name());
					m_finished.emplace(l_agent->get_id(), false);
				}
			}

			for (const auto & m_agent_name : m_agent_names) {
				std::cout << "[" + m_name + "]: sending to " << m_agent_name << std::endl;
				send(s_name_id.at(m_agent_name), {{"data", "start"},{"from", m_name}});
			}
		}

		void action(const cam::MessagePointer& p_message) override {
			std::cout << "[" + m_name + "]: has received " << p_message->to_string() << std::endl;

			if (p_message->content()["data"] == "done") {
				m_finished[p_message->get_sender()] = true;
			}

			if (all_finished()) {
				if (++m_round >= m_max_rounds) {
					return;
				}

				for (const auto & m_agent_name : m_agent_names) {
					m_finished[s_name_id.at(m_agent_name)] = false;
				}

				std::cout << "[" + m_name + "]: start round " + std::to_string(m_round + 1) + " in action";

				for (const auto & m_agent_name : m_agent_names) {
					std::cout << "[" + m_name + "]: sending to " << m_agent_name << std::endl;
					send(s_name_id.at(m_agent_name), {{"data", "continue"},{"from", m_name}});
				}
			}
		}

	protected:
		bool all_finished() const {
			return std::ranges::all_of(m_finished.cbegin(), m_finished.cend(), [](const auto& l_data) { return l_data.second; });
		}
};

class MyAgent final : public cam::Agent {
	public:
		explicit MyAgent(const std::string& p_name) : Agent(p_name) {
			s_name_id.emplace(m_name, m_id);
		}

		void action(const cam::MessagePointer& p_message) override {
			std::cout << "[" + m_name + "]: has received " << p_message->to_string() << std::endl;
			if (p_message->get_sender() == s_name_id.at("monitor") && (p_message->content()["data"] == "start" || p_message->content()["data"] == "continue")) {
				send(s_name_id.at("monitor"), {{"data", "done"},{"from", m_name}});
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
