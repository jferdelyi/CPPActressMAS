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
#include <queue>
#include <thread>

#include <Environment.h>
#include <transport/Facade.h>

// Port of ActressMAS E20 MasMobileConsole: a mobile agent visits every other environment, asks the
// static agents there for information and reports once home. Three environments of this process, each
// connected to the broker as its own node (Mobile [host [port]], default 127.0.0.1 1883).

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

	void setup() override {
		if (m_firstStart) {
			std::cout << "Mobile agent starting at home" << std::endl;

			m_firstStart = false;

			m_log = "\nReporting:\n";

			const auto& l_home_id = get_environment()->get_id();

			for (const auto& l_container : get_environment()->environments()) {
				if (l_container.m_online && l_container.m_id != l_home_id) {	// home
					m_moves.push(l_container.m_id);
					std::cout << l_container.m_id << std::endl;
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
			if (const auto l_result = move(l_next_destination); !l_result) {
				std::cout << "Move refused: " << l_result.error().m_message << std::endl;
			}
			return;
		}

		m_log += "Stopping\n";
		std::cout << m_log << std::endl;
		// The static agents of every environment stop: network scope, asked explicitly.
		cam::transport::SendOptions l_options;
		l_options.m_scope = cam::transport::Scope::Network;
		broadcast("stop", l_options);
		stop();
	}

	[[nodiscard]] json save_state() const override {
		json l_moves = json::array();
		std::queue<std::string> l_copy = m_moves;
		while (!l_copy.empty()) {
			l_moves.push_back(l_copy.front());
			l_copy.pop();
		}
		return {{"first_start", m_firstStart}, {"log", m_log}, {"moves", l_moves}};
	}

	void load_state(const json& p_state) override {
		m_firstStart = p_state.at("first_start").get<bool>();
		m_log = p_state.at("log").get<std::string>();
		m_moves = {};
		for (const auto& l_move : p_state.at("moves")) {
			m_moves.push(l_move.get<std::string>());
		}
	}
};

int main(int argc, char** argv) {
	const std::string l_host = argc > 1 ? argv[1] : "127.0.0.1";
	const std::string l_port = argc > 2 ? argv[2] : "1883";
	const std::string l_url = "mqtt://" + l_host + ":" + l_port + "/mobile";

	cam::Environment l_environment_c;
	cam::Environment l_environment_b;
	cam::Environment l_environment_a;
	for (cam::Environment* l_environment : {&l_environment_c, &l_environment_b, &l_environment_a}) {
		l_environment->register_agent_type<MobileAgent>("MobileAgent");
		if (const auto l_result = cam::transport::connect(*l_environment, l_url); !l_result) {
			std::cerr << "Cannot connect to " << l_url << ": " << l_result.error().m_message << std::endl;
			return 1;
		}
	}

	l_environment_c.add<StaticAgent>("StaticAgentC1");
	l_environment_c.add<StaticAgent>("StaticAgentC2");
	l_environment_c.add<StaticAgent>("StaticAgentC3");

	l_environment_b.add<StaticAgent>("StaticAgentB1");
	l_environment_b.add<StaticAgent>("StaticAgentB2");

	l_environment_a.add<StaticAgent>("StaticAgentA1");
	l_environment_a.add<MobileAgent>("MobileAgentA1");

	std::thread l_environment_c_thread(&cam::Environment::start, &l_environment_c);
	std::thread l_environment_b_thread(&cam::Environment::start, &l_environment_b);
	std::thread l_environment_a_thread(&cam::Environment::start, &l_environment_a);

	l_environment_a_thread.join();
	l_environment_b_thread.join();
	l_environment_c_thread.join();
}
