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


class ManagerAgent : public cam::Agent {
	public:
		ManagerAgent(const std::string& p_name): Agent(p_name) {}

		void setup() override {
			broadcast({{"action","start"}});
		}

		void action(const cam::MessagePointer& p_message) override {
			std::cout << p_message->format() << std::endl;
			const std::string& l_action = p_message->content()["action"];
			//const std::vector<std::string>& l_parameters = p_message->content()["parameters"];

			if (l_action == "report") {
				std::cout << "\t[" + m_name + "]: " + p_message->get_sender() + " reporting" << std::endl;	
			} else if (l_action == "reply") {
				std::cout << "\t[" + m_name + "]: " + p_message->get_sender() + " replying" << std::endl;
			}
		}
};

class WorkerAgent : public cam::Agent {

	protected:
		inline const std::string next_worker() const {
			int l_workers_count = 5;
			std::string l_name = m_name;
			while (l_name == m_name) {
				l_name = "Worker" + std::to_string(rand() % l_workers_count);
			}
			return l_name;
		}

	public:
		WorkerAgent(const std::string& p_name): Agent(p_name) {}

		void action(const cam::MessagePointer& p_message) override {
			std::cout << p_message->format() << std::endl;
			const std::string& l_action = p_message->content()["action"];
			//const std::vector<std::string>& l_parameters = p_message->content()["parameters"];

			if (l_action == "start") {
				send("Manager", {{"action","report"}});
				send(next_worker(), {{"action","request"}});			
			} else if (l_action == "request") {
				send(p_message->get_sender(), {{"action","reply"}});
			} else if (l_action == "request_reply") {
				send("Manager", {{"action","reply"}});
				send(next_worker(), {{"action","request"}});
			}
		}
};

int main() {
	cam::EnvironmentMas l_environment(20);

	l_environment.add(cam::AgentPointer(new ManagerAgent("Manager")));
	for (int i = 1; i <= 5; i++) {
		l_environment.add(cam::AgentPointer(new WorkerAgent("Worker" + std::to_string(i))));
	}	l_environment.start();

	return 0;
}
