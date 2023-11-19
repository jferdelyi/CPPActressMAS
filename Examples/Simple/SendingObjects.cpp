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

class ContentInfo {
	public:
		int m_number;
		std::string m_text;

		ContentInfo(int l_number, const std::string& l_text) : 
			m_number(l_number),
			m_text(l_text) {
		}

		ContentInfo(const json& p_serialized_data) {
			parse(p_serialized_data);
		}

		const json serialize() const {
			return {{"number", m_number}, {"text", m_text}};
		}

		void parse(const json& p_serialized_data) {
			m_number = p_serialized_data["number"];
			m_text = p_serialized_data["text"];
		}
};


class Agent1 : public cam::Agent {
	public:
		Agent1(const std::string& p_name) : 
			Agent(p_name) {
			s_name_id.emplace(m_name, m_id);
		}

		void setup() override {
			for (int i = 0; i < 10; i++) {
				std::cout << "[" + m_name + "]: setup " << std::to_string(i + 1) << std::endl;
				ContentInfo l_content(i + 1, "setup from a1*");
				send(s_name_id.at("a2"), l_content.serialize());
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
		}

		void action(const cam::MessagePointer& p_message) override {
			ContentInfo l_content(p_message->content());
			std::cout << "[" + m_name + "]: has received {" << l_content.m_text << ", " <<  std::to_string(l_content.m_number) << "}" << std::endl;
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
};

class Agent2 : public cam::Agent {
	public:
		Agent2(const std::string& p_name) : 
			Agent(p_name) {
			s_name_id.emplace(m_name, m_id);
		}

		void setup() override {
			for (int i = 0; i < 3; i++) {
				std::cout << "[" + m_name + "]: setup " << std::to_string(i + 1) << std::endl;
				ContentInfo l_content(i + 1, "setup from a2");
				send(s_name_id.at("a1*"), l_content.serialize());
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
		}

		void action(const cam::MessagePointer& p_message) override {
			ContentInfo l_content(p_message->content());
			std::cout << "[" + m_name + "]: has received {" << l_content.m_text << ", " <<  std::to_string(l_content.m_number) << "}" << std::endl;
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
};

int main() {
	cam::EnvironmentMas l_environment(10);

	l_environment.add(cam::AgentPointer(new Agent1("a1*")));
	l_environment.add(cam::AgentPointer(new Agent2("a2")));
	l_environment.start();

	return 0;
}
