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

#include <Environment.h>

static bool s_serialization_sent = false;

class SerialAgentOut : public cam::Agent {
private:
	std::string m_string;

public:
	explicit SerialAgentOut(const std::string& p_name) : Agent(p_name) {}
	explicit SerialAgentOut() : Agent() {}

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

		// Send serial data to "SerialAgentIn"
		if (!s_serialization_sent) {
			// Serialize itself
			m_string  = "my string OUT";
			std::stringstream l_stream = serialize_to_stream<SerialAgentOut>();
			send_by_name("SerialAgentIn", {{"data", l_stream.str()}, {"from", get_name()}});
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

	template <class Archive>
	void save(Archive& p_archive) const {
		p_archive(cereal::base_class<Agent>(this), m_string);
	}

	template <class Archive>
	void load(Archive& p_archive) {
		p_archive(cereal::base_class<Agent>(this), m_string);
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
		std::stringstream l_stream = std::stringstream(p_message->content()["data"].get<std::string>());
		std::cout << "id " << get_environment()->add<SerialAgentOut>(l_stream);
		std::cout << std::endl;
	}

	void default_action() override {
		std::cout << "Default" << std::endl;
		std::cout << get_name() << std::endl;
		std::cout << get_id() << std::endl;
		std::cout << "string " << m_string << std::endl;
		std::cout << std::endl;
	}

	template <class Archive>
	void save(Archive& p_archive) const {
		p_archive(cereal::base_class<Agent>( this), m_string);
	}

	template <class Archive>
	void load(Archive& p_archive) const {
		p_archive(cereal::base_class<Agent>( this), m_string);
	}
};

int main() {
	cam::Environment l_environment(5, cam::EnvironmentMasMode::Sequential);

	l_environment.add<SerialAgentOut>("SerialAgentOut");
	l_environment.add<SerialAgentIn>("SerialAgentIn");

	l_environment.start();
}
