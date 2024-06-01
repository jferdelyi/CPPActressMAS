
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

class StaticAgent : public cam::Agent {
public:
	explicit StaticAgent(const std::string& p_name) : Agent(p_name) {}

	void setup() override {
		std::cout << "Static agent " << get_name() << " starting" << std::endl;
	}

	void action(const cam::MessagePointer& p_message) override {
		std::cout << p_message->content() << std::endl;
		if (p_message->content() == "request-info") {
			const std::string l_info = "Info from agent " + get_name() + " in container " + get_environment()->get_id();
			send(p_message->get_sender(), l_info);
		} else if (p_message->content() == "stop") {
			stop();
		}
	}
};

class MobileAgent : public cam::Agent {
private:
	bool m_firstStart;
	int m_turns_to_wait_for_info;
	std::string m_log;
	std::queue<std::string> m_moves;

public:
	explicit MobileAgent(const std::string& p_name) :
		Agent(p_name),
		m_firstStart(true),
		m_turns_to_wait_for_info(0),
		m_log(),
		m_moves() {}
	explicit MobileAgent() : Agent() {}

	void setup() override {
		if (m_firstStart) {
			std::cout << "Mobile agent starting at home" << std::endl;

			m_firstStart = false;

			m_log = "\nReporting:\n";

			const auto& l_home_id = get_environment()->get_id();

			for (const auto& l_container_id : get_environment()->get_containers()) {
				if (l_container_id != l_home_id) {	// home
					m_moves.push(l_container_id);
					std::cout << l_container_id << std::endl;
				}
			}

			m_moves.push(l_home_id); // return home, get local info and report
			std::cout << l_home_id << std::endl;

		} else {
			std::cout << "I have moved to " + get_environment()->get_id() << std::endl;
			m_log += "Arrived to " + get_environment()->get_id() + "\n";
			broadcast("request-info");
			m_turns_to_wait_for_info = 3;
		}
	}

	void action(const cam::MessagePointer& p_message) override {
		std::cout << p_message->content() << std::endl;
		m_log += "Received info: " + p_message->content().get<std::string>() + "\n";  // info from static agents
	}

	void default_action() override {
		if (m_turns_to_wait_for_info-- > 0) {
			return;
		}

		if (!m_moves.empty()) {
			std::string l_next_destination = m_moves.front();
			m_moves.pop();
			std::cout << "I'm moving to " << l_next_destination << std::endl;
			m_log += "Moving to " + l_next_destination + "\n";
			move<MobileAgent>(l_next_destination, {{ "agent_type", "MobileAgent" }});
			return;
		}

		m_log += "Stopping\n";
		std::cout << m_log << std::endl;
		broadcast("stop");
		stop();
	}

	template <class Archive>
	void save(Archive& p_archive) const {
		p_archive(cereal::base_class<Agent>(this), m_firstStart, m_log, m_moves);
	}

	template <class Archive>
	void load(Archive& p_archive) {
		p_archive(cereal::base_class<Agent>(this), m_firstStart, m_log, m_moves);
	}
};

class MyEnvironment : public cam::Environment {
public:
	explicit MyEnvironment():
		Environment() {}

	void on_new_agent(const json& p_json_message) override {
		const std::string l_message = p_json_message["agent"];
		std::cout << "Move message " << p_json_message["message"] << std::endl;
		std::stringstream l_stream = std::stringstream(l_message);
		add<MobileAgent>(l_stream);
	}
};

int main() {
	MyEnvironment l_environment_c;
	l_environment_c.initialise_remote_connection("EnvironmentC");
	l_environment_c.add<StaticAgent>("StaticAgentC1");
	l_environment_c.add<StaticAgent>("StaticAgentC2");
	l_environment_c.add<StaticAgent>("StaticAgentC3");
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	MyEnvironment l_environment_b;
	l_environment_b.initialise_remote_connection("EnvironmentB");
	l_environment_b.add<StaticAgent>("StaticAgentB1");
	l_environment_b.add<StaticAgent>("StaticAgentB2");
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	MyEnvironment l_environment_a;
	l_environment_a.initialise_remote_connection("EnvironmentA");
	l_environment_a.add<StaticAgent>("StaticAgentA1");
	l_environment_a.add<MobileAgent>("MobileAgentA1");
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	std::thread l_environment_c_thread(&cam::Environment::start, &l_environment_c);
	std::thread l_environment_b_thread(&cam::Environment::start, &l_environment_b);
	std::thread l_environment_a_thread(&cam::Environment::start, &l_environment_a);

	l_environment_a_thread.join();
	l_environment_b_thread.join();
	l_environment_c_thread.join();
}
