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

class Agent1 : public cam::Agent {
	public:
		Agent1(const std::string& p_name): Agent(p_name) {
			s_name_id.emplace(m_name, m_id);
		}

		void setup() override {
			for (int i = 0; i < 10; i++)  {
				std::cout << "[" + m_name + "]: setup " + std::to_string(i + 1) << std::endl;
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
			send(s_name_id.at("a2"), "msg");
		}

		void action(const cam::MessagePointer& p_message) override {
			for (int i = 0; i < 3; i++) {
				std::cout << "[" + m_name + "]: action " + std::to_string(i + 1) << std::endl;
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
		}

		void default_action() override {
			std::cout << "[" + m_name + "]: no messages" << std::endl;
			stop();
		}
};

class Agent2 : public cam::Agent {
	public:
		Agent2(const std::string& p_name): Agent(p_name) {
			s_name_id.emplace(m_name, m_id);
		}

		void setup() override {
			for (int i = 0; i < 3; i++) {
				std::cout << "[" + m_name + "]: setup " + std::to_string(i + 1) << std::endl;
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}

			send(s_name_id.at("a1*"), "msg");
		}

		void action(const cam::MessagePointer& p_message) override {
			for (int i = 0; i < 10; i++) {
				std::cout << "[" + m_name + "]: action " + std::to_string(i + 1) << std::endl;
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
		}

		void default_action() override {
			std::cout << "[" + m_name + "]: no messages" << std::endl;
			stop();
		}
};


int main() {
	cam::EnvironmentMas l_environment;

	l_environment.add(cam::AgentPointer(new Agent1("a1*")));
	l_environment.add(cam::AgentPointer(new Agent2("a2")));
	l_environment.start();

	return 0;
}
