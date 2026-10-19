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

// Harness program of the oracle tests (tests/iso, D98): the C# original as it is, its bugs included, compared with its
// oracle by iso.SendingObjects.
// Port of the C# example E00 Simple Examples/SendingObjects (ActressMAS v3.1): two agents send objects to each other
// during their setup and print the objects they receive. Parallel, 10 turns. The C# sends the object itself
// (ContentObj); here it travels as a JSON object, read back on arrival (as_json).

#include <chrono>
#include <string>
#include <thread>
#include <utility>

#include <DotNet.hpp>
#include <Environment.h>

class ContentInfo {
	public:
		int m_number;
		std::string m_text;

		explicit ContentInfo(const int p_number = 0, std::string p_text = "") :
			m_number(p_number),
			m_text(std::move(p_text)) {}
};

// A content info travels as a JSON object: the receiver gets it as sent, without serialization while it stays local
void to_json(json& p_json, const ContentInfo& p_content) {
	p_json = json{{"number", p_content.m_number}, {"text", p_content.m_text}};
}

void from_json(const json& p_json, ContentInfo& p_content) {
	p_json.at("number").get_to(p_content.m_number);
	p_json.at("text").get_to(p_content.m_text);
}

class Agent1 final : public cam::Agent {
	public:
		explicit Agent1(const std::string& p_name) : Agent(p_name) { }

		void setup() override {
			for (int i = 0; i < 10; i++) {
				cam::dotnet::write_line("a1* sending: setup " + std::to_string(i + 1) + " from a1*");
				send_by_name("a2", json(ContentInfo(i + 1, "setup a1*")));
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
		}

		void action(const cam::MessagePointer& p_message) override {
			const auto l_content = p_message->as_json().get<ContentInfo>();
			cam::dotnet::write_line("a1* receiving: " + l_content.m_text + " - " + std::to_string(l_content.m_number));
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
};

class Agent2 final : public cam::Agent {
	public:
		explicit Agent2(const std::string& p_name) : Agent(p_name) { }

		void setup() override {
			for (int i = 0; i < 3; i++) {
				cam::dotnet::write_line("a2 sending: setup: " + std::to_string(i + 1) + " from a2");
				// By identifier here, where the C# sends by name: same receiver
				send(get_first_agent_by_name("a1").value(), json(ContentInfo(i + 1, "setup a2")));
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
		}

		void action(const cam::MessagePointer& p_message) override {
			const auto l_content = p_message->as_json().get<ContentInfo>();
			cam::dotnet::write_line("a2 receiving: " + l_content.m_text + " - " + std::to_string(l_content.m_number));
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
};

int main() {
	cam::Environment l_environment({.m_turns = 10,
									.m_mode = cam::EnvironmentMasMode::Parallel,
									.m_seed = static_cast<unsigned int>(cam::dotnet::next_site_seed("EnvironmentMas.cs:61")),
									.m_random = cam::RandomEngine::DotNet});

	l_environment.add<Agent1>("a1");
	l_environment.add<Agent2>("a2");
	l_environment.start();

	return 0;
}
