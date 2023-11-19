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

static std::unordered_map<std::string, std::string> s_name_id;

class MyAgent final : public cam::Agent {
	public:
		explicit MyAgent(const std::string& p_name) : Agent(p_name) {
			s_name_id.emplace(m_name, m_id);
		}

		void setup() override {
			std::cout << "[" + m_name + "]: starting" << std::endl;

			std::random_device l_rd;
			std::uniform_int_distribution l_dist(1, 1000);

			for (int i = 1; i <= 10; i++) {
				std::cout << "[" + m_name + "]: sending " << i << std::endl;
				send(s_name_id.at("monitor"), {{"data", i}, {"from", m_name}});
				std::this_thread::sleep_for(std::chrono::milliseconds(l_dist(l_rd)));
			}
		}
};

class MonitorAgent final : public cam::Agent  {
	public:
		explicit MonitorAgent(const std::string& p_name) : Agent(p_name) {
			s_name_id.emplace(m_name, m_id);
		}

		void action(const cam::MessagePointer& p_message) override {
			std::cout << "[" + m_name + "]: has received " << p_message->to_string() << std::endl;
		}
};

int main() {
	cam::EnvironmentMas l_environment(100);
	l_environment.add(cam::AgentPointer(new MyAgent("agent1")));
	l_environment.add(cam::AgentPointer(new MyAgent("agent2")));
	l_environment.add(cam::AgentPointer(new MonitorAgent("monitor")));
	l_environment.start();
	return 0;
}
