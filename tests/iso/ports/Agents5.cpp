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
// oracle by iso.Agents5.
// Port of the C# example E00 Simple Examples/Agents5 (ActressMAS v3.1): long setups, then actions, and each agent stops
// as soon as its mailbox is empty. Parallel, no turn limit: the simulation ends when no agent is left.

#include <chrono>
#include <string>
#include <thread>

#include <DotNet.hpp>
#include <Environment.h>

class Agent1 final : public cam::Agent {
	public:
		explicit Agent1(const std::string& p_name) : Agent(p_name) {}

		void setup() override {
			for (int i = 0; i < 10; i++) {
				cam::dotnet::write_line("Setup: " + std::to_string(i + 1) + " from a1*");
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
			send_by_name("a2", "msg");
		}

		void action(const cam::MessagePointer&) override {
			for (int i = 0; i < 3; i++) {
				cam::dotnet::write_line("Act: " + std::to_string(i + 1) + " from a1*");
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
		}

		void default_action() override {
			cam::dotnet::write_line("a1: No messages");
			stop();
		}
};

class Agent2 final : public cam::Agent {
	public:
		explicit Agent2(const std::string& p_name) : Agent(p_name) {}

		void setup() override {
			for (int i = 0; i < 3; i++) {
				cam::dotnet::write_line("Setup: " + std::to_string(i + 1) + " from a2");
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
			send_by_name("a1", "msg");
		}

		void action(const cam::MessagePointer&) override {
			for (int i = 0; i < 10; i++) {
				cam::dotnet::write_line("Act: " + std::to_string(i + 1) + " from a2");
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
		}

		void default_action() override {
			cam::dotnet::write_line("a2: No messages");
			stop();
		}
};

int main() {
	cam::Environment l_environment({.m_turns = 0,
									.m_mode = cam::EnvironmentMasMode::Parallel,
									.m_seed = static_cast<unsigned int>(cam::dotnet::next_site_seed("EnvironmentMas.cs:61")),
									.m_random = cam::RandomEngine::DotNet});

	l_environment.add<Agent1>("a1");
	l_environment.add<Agent2>("a2");

	l_environment.start();
	return 0;
}
