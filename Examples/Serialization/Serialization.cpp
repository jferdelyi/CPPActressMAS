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

#include <Environment.h>

// An agent saves its state (save_state), sends it in a message, and the receiver builds a copy of it
// from that state (load_state). The copy is a new agent: it gets a new id and runs setup() again.

static bool s_serialization_sent = false;

class SerialAgentOut : public cam::Agent {
private:
	std::string m_string;

public:
	explicit SerialAgentOut(const std::string& p_name) : Agent(p_name) {}

	void setup() override {
		// Set observables
		m_observables->emplace(std::make_pair("number", 42));
		m_observables->emplace(std::make_pair("color", "blue"));

		std::cout << "Setup" << std::endl;
		std::cout << get_name() << std::endl;
		std::cout << get_id() << std::endl;
		std::cout << "number " << get_observables()->at("number") << std::endl;
		std::cout << "color " << get_observables()->at("color") << std::endl;
		std::cout << "string " << m_string << std::endl;
		std::cout << std::endl;

		// Send its state to "SerialAgentIn"
		if (!s_serialization_sent) {
			m_string  = "my string OUT";
			send_by_name("SerialAgentIn", {{"data", save_state()}, {"from", get_name()}});
			stop();
			s_serialization_sent = true;
		}
	}

	void default_action() override {
		std::cout << "Default" << std::endl;
		std::cout << get_name() << std::endl;
		std::cout << get_id() << std::endl;
		std::cout << "number " << get_observables()->at("number") << std::endl;
		std::cout << "color " << get_observables()->at("color") << std::endl;
		std::cout << "string " << m_string << std::endl;
		std::cout << std::endl;
	}

	[[nodiscard]] json save_state() const override {
		return {{"string", m_string}};
	}

	void load_state(const json& p_state) override {
		m_string = p_state.at("string").get<std::string>();
	}
};

class SerialAgentIn : public cam::Agent {
private:
	std::string m_string = "my string IN";

public:
	explicit SerialAgentIn(const std::string& p_name) : Agent(p_name) {}

	void action(const cam::MessagePointer& p_message) override {
		std::cout << "Message arrived" << std::endl;
		std::cout << get_name() << std::endl;
		std::cout << get_id() << std::endl;
		std::cout << "from " << p_message->content()["from"] << std::endl;
		std::cout << "string " << m_string << std::endl;
		// A copy of the sender, with its state and a new id
		auto l_copy = std::make_shared<SerialAgentOut>(p_message->content()["from"].get<std::string>());
		l_copy->load_state(p_message->content()["data"]);
		std::cout << "id " << get_environment()->add(l_copy);
		std::cout << std::endl;
	}

	void default_action() override {
		std::cout << "Default" << std::endl;
		std::cout << get_name() << std::endl;
		std::cout << get_id() << std::endl;
		std::cout << "string " << m_string << std::endl;
		std::cout << std::endl;
	}
};

int main() {
	cam::Environment l_environment(5, cam::EnvironmentMasMode::Sequential);

	l_environment.add<SerialAgentOut>("SerialAgentOut");
	l_environment.add<SerialAgentIn>("SerialAgentIn");

	l_environment.start();
}
