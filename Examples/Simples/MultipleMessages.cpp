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

class ManagerAgent final : public cam::Agent {
	public:
		explicit ManagerAgent(const std::string& p_name) : Agent(p_name) { }

		void setup() override {
			broadcast({{"action", "start"}});
		}

		void action(const cam::MessagePointer& p_message) override {
			if (const std::string& l_action = p_message->content()["action"]; l_action == "report") {
				std::cout << "[" + get_name() + "]: reporting from " << p_message->content()["from"] << std::endl;
			} else if (l_action == "reply") {
				std::cout << "[" + get_name() + "]: replaying from " << p_message->content()["from"] << std::endl;
			}
		}
};

class WorkerAgent final : public cam::Agent {

	protected:
		[[nodiscard]] std::string next_worker() const {
			std::string l_name = get_name();

			std::random_device l_rd;
			std::uniform_int_distribution l_dist(1, 5);

			while (l_name == get_name()) {
				l_name = "Worker" + std::to_string(l_dist(l_rd));
			}
			return l_name;
		}

	public:
		explicit WorkerAgent(const std::string& p_name) : Agent(p_name) {}

		void action(const cam::MessagePointer& p_message) override {
			std::cout << "[" + get_name() + "]: has received " << p_message->to_string() << std::endl;

			if (const std::string& l_action = p_message->content()["action"];
				l_action == "start") {
				send_by_name("Manager", {{"action", "report"}, {"from", get_name()}});
				send_by_name(next_worker(), {{"action", "request"}, {"from", get_name()}});
			} else if (l_action == "request") {
				send(p_message->get_sender(), {{"action", "reply"}, {"from", get_name()}});
			} else if (l_action == "reply") {
				send_by_name("Manager", {{"action", "reply"}, {"from", get_name()}});
				send_by_name(next_worker(), {{"action", "request"}, {"from", get_name()}});
			}
		}
};

int main() {
	cam::EnvironmentMas l_environment(20);

	l_environment.add<ManagerAgent>("Manager");
	for (int i = 1; i <= 5; i++) {
		l_environment.add<WorkerAgent>("Worker" + std::to_string(i));
	}
	l_environment.start();

	return 0;
}
