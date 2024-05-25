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


class MyAgent1 final : public cam::Agent {
	public:
		explicit MyAgent1(const std::string& p_name) : Agent(p_name) { }

		void setup() override {
			send_by_name("agent2",{{"data", "start"}, {"from", get_name()}});
			for (int i = 0; i < 10; i++) {
				send_by_name("agent2",{{"data", "in setup #" + std::to_string(i)}, {"from", get_name()}});
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
		}

		void action(const cam::MessagePointer& p_message) override {
			std::cout << "[" + get_name() + "]: has received " << p_message->to_string() << std::endl;
			for (int i = 0; i < 10; i++) {
				send_by_name("agent2",{{"data", "in action #" + std::to_string(i)}, {"from", get_name()}});
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
		}
};

class MyAgent2 final : public cam::Agent {
  public:
	explicit MyAgent2(const std::string& p_name) : Agent(p_name) { }

	void setup() override {
		send_by_name("agent1",{{"data", "start"}, {"from", get_name()}});
	}

	void action(const cam::MessagePointer& p_message) override {
		std::cout << "[" + get_name() + "]: has received " << p_message->to_string() << std::endl;
	}
};

int main() {
	cam::EnvironmentMas l_environment(100, cam::EnvironmentMasMode::SequentialRandom);

	l_environment.add<MyAgent1>("agent1");
	l_environment.add<MyAgent2>("agent2");

	l_environment.start();
	return 0;
}
