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

class MyEnvironment final : public cam::EnvironmentMas {
  public:
	MyEnvironment() :
		EnvironmentMas(10, cam::EnvironmentMasMode::Parallel, 100) {}

	void turn_finished(const int p_turn) override {
		std::cout << std::endl
				  << "Turn " + std::to_string(p_turn + 1) + " finished" << std::endl
				  << std::endl;
	}

	void simulation_finished() override {
		std::cout << std::endl
				  << "Simulation finished" << std::endl
				  << std::endl;
	}
};

class MonitorAgent final : public cam::Agent {
	protected:
		int m_round{};
		int m_max_rounds{};
		std::map<std::string, bool> m_finished;
		std::vector<std::string> m_agents_id;

	public:
		explicit MonitorAgent(const std::string& p_name) : Agent(p_name) { }

		void setup() override {
			m_max_rounds = 3;
			m_round = 0;

			std::cout << "[" + m_name + "]: start round 1 in setup " << std::endl;

			// In C# version the order is alway 3,2,1,4
			// In C++ version the order is always 4,3,2,1
			// Probably explained by the type of container and
			// find algorithme. I think this is not really critical
			for (const auto& l_agent : m_environment->get_filtered_agents("agent")) {
				m_agents_id.push_back(l_agent);
				m_finished.emplace(l_agent, false);
			}

			for (const auto& l_agent : m_agents_id) {
				if (const auto& l_agent_name = m_environment->get_agent_name(l_agent); l_agent_name != std::nullopt) {
					std::cout << "[" + m_name + "]: sending to " << l_agent_name.value() << std::endl;
					send(l_agent,{{"data", "start"}, {"from", m_name}});
				}
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

				for (const auto& l_agent : m_agents_id) {
					m_finished[l_agent] = false;
				}

				std::cout << "[" + m_name + "]: start round " + std::to_string(m_round + 1) + " in action";

				for (const auto& l_agent : m_agents_id) {
					if (const auto& l_agent_name = m_environment->get_agent_name(l_agent); l_agent_name != std::nullopt) {
						std::cout << "[" + m_name + "]: sending to " << l_agent_name.value() << std::endl;
						send(l_agent, {{"data", "continue"}, {"from", m_name}});
					}
				}
			}
		}

	protected:
		[[nodiscard]] bool all_finished() const {
			return std::ranges::all_of(m_finished.cbegin(), m_finished.cend(),[](const auto& l_data) {
				return l_data.second;
			});
		}
};

class MyAgent : public cam::Agent {
	std::string m_monitor_id;
	public:
		explicit MyAgent(const std::string& p_name) : Agent(p_name) { }

		void setup() override {
			if (const auto& l_agent_id = m_environment->get_first_agent_by_name("monitor"); l_agent_id != std::nullopt) {
				m_monitor_id = l_agent_id.value();
			}
		}

		void action(const cam::MessagePointer& p_message) override {
			std::cout << "[" + m_name + "]: has received " << p_message->to_string() << std::endl;
			if (p_message->get_sender() == m_monitor_id && (p_message->content()["data"] == "start" || p_message->content()["data"] == "continue")) {
				send(m_monitor_id, {{"data", "done"}, {"from", m_name}});
			}
		}
};

int main() {
	MyEnvironment l_environment;

	for (int i = 1; i <= 4; i++) {
		l_environment.add<MyAgent>("agent" + std::to_string(i));
	}
	l_environment.add<MonitorAgent>("monitor");

	l_environment.start();
	return 0;
}
