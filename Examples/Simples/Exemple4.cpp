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

// Port of the C# example E00 Simple Examples/Agents4 (ActressMAS v3.1): five agents write three messages per turn to a
// writer, then stop. Parallel, 5 turns, one second after each turn.
// WARNING: the C# stops an agent at its fourth action and still sends three messages afterwards (X3); "legacy", the
// default, keeps this bug, "fixed" stops without sending (D89). The writer prints the late messages it gets in the last
// turn, so the C# prints 45 to 60 lines; "fixed" prints 45.
// Arguments: [legacy|fixed].

#include <iostream>
#include <string>

#include <DotNet.hpp>
#include <Environment.h>

class MyAgent final : public cam::Agent {
	private:
		bool m_fixed;
		int m_turn = 1;

	public:
		MyAgent(const std::string& p_name, const bool p_fixed) :
				Agent(p_name),
				m_fixed(p_fixed) {}

		void default_action() override {
			if (m_turn > 3) {
				stop();
				if (m_fixed) {  // Fixed: a stopped agent sends nothing more
					return;
				}
				// Legacy: as the C#, the three messages below are still sent after the stop
			}
			for (int i = 1; i <= 3; i++) {
				send_by_name("writer", "Agent " + get_name() + " turn " + std::to_string(m_turn) + " index " + std::to_string(i));
			}
			m_turn++;
		}
};

class WriterAgent final : public cam::Agent {
	public:
		explicit WriterAgent(const std::string& p_name) : Agent(p_name) {}

		void action(const cam::MessagePointer& p_message) override {
			cam::dotnet::write_line(format(p_message));
		}
};

int main(int argc, char** argv) {
	const std::string l_mode = argc > 1 ? argv[1] : "legacy";
	if (l_mode != "legacy" && l_mode != "fixed") {
		std::cerr << "usage: Exemple4 [legacy|fixed]" << std::endl;
		return 2;
	}
	const bool l_fixed = l_mode == "fixed";
	if (!l_fixed) {
		std::cerr << "WARNING: legacy mode, the agents still send after stopping, as in the C# (see the README); \"fixed\" corrects it" << std::endl;
	}

	constexpr int l_no_agents = 5;
	cam::Environment l_environment({.m_turns = 5,
									.m_mode = cam::EnvironmentMasMode::Parallel,
									.m_delay_after_turn = 1000,
									.m_seed = static_cast<unsigned int>(cam::dotnet::next_site_seed("EnvironmentMas.cs:61")),
									.m_random = cam::RandomEngine::DotNet});

	l_environment.add<WriterAgent>("writer");
	for (int i = 1; i <= l_no_agents; i++) {
		l_environment.add<MyAgent>("a" + std::to_string(i), l_fixed);
	}

	l_environment.start();
	return 0;
}
