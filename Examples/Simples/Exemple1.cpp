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

#include <random>

#include <Environment.h>

#include <Agent.h>

class MyAgent : public cam::Agent {
	public:
		explicit MyAgent(const std::string& p_name) : Agent(p_name) { }

		void setup() override {
			std::cout << "[" + get_name() + "]: starting" << std::endl;

			std::random_device l_rd;
			std::uniform_int_distribution l_dist(10, 100);

			for (int i = 1; i <= 10; i++) {
				std::cout << "[" + get_name() + "]: sending " << i << std::endl;
				send_by_name("monitor",{{"data", i}, {"from", get_name()}});
				std::this_thread::sleep_for(std::chrono::milliseconds(l_dist(l_rd)));
			}
		}
};

class MonitorAgent final : public cam::Agent {
	public:
		explicit MonitorAgent(const std::string& p_name) : Agent(p_name) { }

		void action(const cam::MessagePointer& p_message) override {
			std::cout << "[" + get_name() + "]: has received " << p_message->to_string() << std::endl;
		}
};

int main() {
	cam::Environment l_environment(100, cam::EnvironmentMasMode::Parallel);
	l_environment.add<MyAgent>("agent1");
	l_environment.add<MyAgent>("agent2");
	l_environment.add<MonitorAgent>("monitor");
	l_environment.start();
	return 0;
}
