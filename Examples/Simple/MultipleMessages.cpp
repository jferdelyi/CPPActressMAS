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

class ManagerAgent final : public cam::Agent {
	public:
		explicit ManagerAgent(const std::string& p_name) :
			Agent(p_name) {
			s_name_id.emplace(m_name, m_id);
		}

		void setup() override {
			broadcast({{"action", "start"}});
		}

		void action(const cam::MessagePointer& p_message) override {

			// const std::vector<std::string>& l_parameters = p_message->content()["parameters"];

			if (const std::string& l_action = p_message->content()["action"]; l_action == "report") {
				std::cout << "[" + m_name + "]: reporting from " << p_message->content()["from"] << std::endl;
			} else if (l_action == "reply") {
				std::cout << "[" + m_name + "]: replaying from " << p_message->content()["from"] << std::endl;
			}
		}
};

class WorkerAgent final : public cam::Agent {

	protected:
		[[nodiscard]] std::string next_worker() const {
			std::string l_name = m_name;

			std::random_device l_rd;
			std::uniform_int_distribution l_dist(1, 5);

			while (l_name == m_name) {
				l_name = "Worker" + std::to_string(l_dist(l_rd));
			}
			return l_name;
		}

	public:
		explicit WorkerAgent(const std::string& p_name) : Agent(p_name) {
			s_name_id.emplace(m_name, m_id);
		}

		void action(const cam::MessagePointer& p_message) override {
			std::cout << "[" + m_name + "]: has received " << p_message->to_string() << std::endl;
			// const std::vector<std::string>& l_parameters = p_message->content()["parameters"];

			if (const std::string& l_action = p_message->content()["action"];
				l_action == "start") {
				send(s_name_id.at("Manager"), {{"action", "report"}, {"from", m_name}});
				send(s_name_id.at(next_worker()), {{"action", "request"}, {"from", m_name}});
			} else if (l_action == "request") {
				send(p_message->get_sender(), {{"action", "reply"}, {"from", m_name}});
			} else if (l_action == "request_reply") {
				send(s_name_id.at("Manager"), {{"action", "reply"}, {"from", m_name}});
				send(s_name_id.at(next_worker()), {{"action", "request"}, {"from", m_name}});
			}
		}
};

int main() {
	cam::EnvironmentMas l_environment(20);

	l_environment.add(cam::AgentPointer(new ManagerAgent("Manager")));
	for (int i = 1; i <= 5; i++) {
		l_environment.add(cam::AgentPointer(new WorkerAgent("Worker" + std::to_string(i))));
	}
	l_environment.start();

	return 0;
}
