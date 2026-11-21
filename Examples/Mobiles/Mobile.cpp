/**************************************************************************
 *                                                                        *
 *  Description: CPPActressMas multi-agent framework                      *
 *  Website:     https://github.com/jferdelyi/CPPActressMAS               *
 *  Copyright:   (c) 2023-Today, Jean-François Erdelyi                    *
 *                                                                        *
 *  CPP version of ActressMAS by Florin Leon                              *
 *  https://github.com/florinleon/ActressMas                              *
 *                                                                        *
 *  This program and the accompanying materials are made available under  *
 *  the terms of the Eclipse Public License 2.0, which is available at    *
 *  https://www.eclipse.org/legal/epl-2.0/ and in the LICENSE file.       *
 *                                                                        *
 *  SPDX-License-Identifier: EPL-2.0                                      *
 *                                                                        *
 **************************************************************************/

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <queue>
#include <thread>

#include <Environment.h>
#include <transport/Facade.h>

// Port of ActressMAS E20 MasMobileConsole: a mobile agent visits every other environment, asks the
// static agents there for information and reports once home. Three environments of this process:
// Mobile [protocol [host [port]]]; by default the loopback of the process, in memory, without configuration.
// With Zenoh (C listening on the meeting point, B and A connecting to it, no router) or MQTT, see main.

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
	bool m_firstStart;
	bool m_planning;
	int m_turns_to_wait_for_info;
	std::string m_log;
	std::queue<std::string> m_moves;

public:
	explicit MobileAgent(const std::string& p_name) :
		Agent(p_name),
		m_firstStart(true),
		m_planning(false),
		m_turns_to_wait_for_info(0),
		m_moves() {}

	void setup() override {
		if (m_firstStart) {
			std::cout << "Mobile agent starting at home" << std::endl;

			m_firstStart = false;
			m_planning = true;

			m_log = "\nReporting:\n";

			// The other environments announce themselves over the network: give them a few turns before
			// planning the trip (the C# reads the list the server already holds).
			m_turns_to_wait_for_info = 5;

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

		if (m_planning) {
			m_planning = false;
			const auto& l_home_id = get_environment()->get_id();
			for (const auto& l_container : get_environment()->environments()) {
				if (l_container.m_online && l_container.m_id != l_home_id) {	// home
					m_moves.push(l_container.m_id);
					std::cout << l_container.m_id << std::endl;
				}
			}
			m_moves.push(l_home_id); // return home, get local info and report
			std::cout << l_home_id << std::endl;
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
CAM_AGENT_TYPE(MobileAgent);  // its wire name, for programs that only receive it

// The network named on the command line: <protocol> [host] [port], from p_first; none without protocol.
std::optional<cam::NetworkConfig> network_of(const int p_argc, char** p_argv, const int p_first, const std::string& p_namespace) {
	cam::NetworkConfig l_network{.m_namespace = p_namespace};
	if (p_argc <= p_first) {
		return l_network;
	}
	const std::string l_protocol = p_argv[p_first];
	for (const auto& [l_name, l_value] : {std::pair{"loopback", cam::Protocol::Loopback}, std::pair{"zenoh", cam::Protocol::Zenoh},
										   std::pair{"mqtt", cam::Protocol::Mqtt}, std::pair{"mqtts", cam::Protocol::Mqtts}}) {
		if (l_protocol == l_name) {
			l_network.m_protocol = l_value;
		}
	}
	if (l_network.m_protocol == cam::Protocol::None) {
		std::cerr << "Unknown protocol " << l_protocol << ": loopback, zenoh, mqtt or mqtts" << std::endl;
		return std::nullopt;
	}
	if (p_argc > p_first + 1) {
		l_network.m_host = p_argv[p_first + 1];
	}
	if (p_argc > p_first + 2) {
		l_network.m_port = static_cast<std::uint16_t>(std::atoi(p_argv[p_first + 2]));
	}
	return l_network;
}

int main(int argc, char** argv) {
	// Without argument the three environments meet on the loopback of the process, available in every build.
	// With a protocol they also connect to that network, the host and the port of the meeting point by default
	// 127.0.0.1 and the port of the protocol, for example:
	//   Mobile zenoh 127.0.0.1 17447
	//   Mobile mqtt 127.0.0.1 1883
	// Between the environments of this process, the traffic stays on the loopback.
	const auto l_network = network_of(argc, argv, 1, "mobile");
	if (!l_network) {
		return 1;
	}

	// A turn every 100 ms: the peers have time to announce themselves and the trip stays readable.
	cam::Environment l_environment_c({.m_mode = cam::EnvironmentMasMode::Parallel, .m_delay_after_turn = 100, .m_network = *l_network});
	cam::Environment l_environment_b({.m_mode = cam::EnvironmentMasMode::Parallel, .m_delay_after_turn = 100, .m_network = *l_network});
	cam::Environment l_environment_a({.m_mode = cam::EnvironmentMasMode::Parallel, .m_delay_after_turn = 100, .m_network = *l_network});
	// Connected before their threads start (C first: with Zenoh it is the peer that listens); without protocol, each
	// joins the loopback at its start, and the agent waits a few turns before planning its trip
	if (l_network->m_protocol != cam::Protocol::None) {
		for (auto* const l_environment : {&l_environment_c, &l_environment_b, &l_environment_a}) {
			if (const auto l_result = l_environment->connect(); !l_result) {
				std::cerr << "Cannot connect: " << l_result.error().m_message << std::endl;
				return 1;
			}
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
