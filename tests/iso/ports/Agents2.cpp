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
// oracle by iso.Agents2.
// Port of the C# example E00 Simple Examples/Agents2 (ActressMAS v3.1): two agents exchange messages from their setup
// and their action. Sequential in a random order, 100 turns: the order of each turn comes from the seed of the C#
// oracle, so the output is the one of the C#.

#include <chrono>
#include <string>
#include <thread>

#include <DotNet.hpp>
#include <Environment.h>

class MyAgent1 final : public cam::Agent {
	public:
		explicit MyAgent1(const std::string& p_name) : Agent(p_name) {}

		void setup() override {
			send_by_name("agent2", "start from 1");

			for (int i = 0; i < 10; i++) {
				send_by_name("agent2", "in setup for: " + std::to_string(i));
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
		}

		void action(const cam::MessagePointer& p_message) override {
			cam::dotnet::write_line(format(p_message));

			for (int i = 0; i < 10; i++) {
				send_by_name("agent2", "in act for: " + std::to_string(i));
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
		}
};

class MyAgent2 final : public cam::Agent {
	public:
		explicit MyAgent2(const std::string& p_name) : Agent(p_name) {}

		void setup() override {
			send_by_name("agent1", "start from 2");
		}

		void action(const cam::MessagePointer& p_message) override {
			cam::dotnet::write_line(format(p_message));
		}
};

int main() {
	cam::Environment l_environment({.m_turns = 100,
									.m_mode = cam::EnvironmentMasMode::SequentialRandom,
									.m_seed = static_cast<unsigned int>(cam::dotnet::next_site_seed("EnvironmentMas.cs:61")),
									.m_random = cam::RandomEngine::DotNet});

	l_environment.add<MyAgent1>("agent1");
	l_environment.add<MyAgent2>("agent2");

	l_environment.start();
	return 0;
}
