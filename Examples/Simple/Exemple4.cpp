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

class MyAgent : public cam::Agent {
	protected:
		int m_turn = 1;

	public:
		MyAgent(const std::string& p_name) : 
			Agent(p_name) {
			s_name_id.emplace(m_name, m_id);
		}

		void default_action() override {
			if (m_turn > 3) {
				stop();
				return; // Diference from the C# version 
			}

			for (int i = 1; i <= 3; i++) {
				send(s_name_id.at("writer"), {{"turn", m_turn}, {"index", i}, {"from", m_name}});
			}

			m_turn++;
		}
};

class WriterAgent : public cam::Agent {
	public:
		WriterAgent(const std::string& p_name) : 
			Agent(p_name) {
			s_name_id.emplace(m_name, m_id);
		}

		void action(const cam::MessagePointer& p_message) override {
			std::cout << "[" + m_name + "]: has received " << p_message->to_string() << std::endl;
		}
};

int main() {
	cam::EnvironmentMas l_environment(5, cam::EnvironmentMasMode::Parallel, 1000);

	l_environment.add(cam::AgentPointer(new WriterAgent("writer")));
	for (int i = 1; i <= 5; i++) {
		l_environment.add(cam::AgentPointer(new MyAgent("a" + std::to_string(i))));
	}
	l_environment.start();
	return 0;
}
