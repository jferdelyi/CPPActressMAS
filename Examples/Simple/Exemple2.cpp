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

class MyAgent1 : public cam::Agent {
	public:
		MyAgent1(const std::string& p_name): Agent(p_name) {}

		void setup() override {
			send("agent2", "start from 1");
			for (int i = 0; i < 10; i++) {
				send("agent2", "in setup for: " + std::to_string(i));
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
		}

		void action(const cam::MessagePointer& p_message) override {
			std::cout << m_name << " has received " << p_message->format() << std::endl;
			for (int i = 0; i < 10; i++) {
				send("agent2", "in act for: " + std::to_string(i));
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
		}
};

class MyAgent2 : public cam::Agent {
	public:
		MyAgent2(const std::string& p_name): Agent(p_name) {}

		void setup() override {
			send("agent1", "start from 2");
		}

		void action(const cam::MessagePointer& p_message) override {
			std::cout << p_message->format() << std::endl;
		}
};

int main() {
	cam::EnvironmentMas l_environment(100, cam::EnvironmentMasMode::SequentialRandom);

	l_environment.add(cam::AgentPointer(new MyAgent1("agent1")));
	l_environment.add(cam::AgentPointer(new MyAgent2("agent2")));

	l_environment.start();
	return 0;
}
