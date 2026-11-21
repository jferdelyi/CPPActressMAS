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
#include <thread>

#include <Environment.h>
#include <transport/Facade.h>

// Port of ActressMAS E20 RemoteMessages: a ping agent asks a pong agent of another environment for
// information, by name over the network; the pong agent answers twice by id. Two environments of this
// process: MobilePingPong [protocol [host [port]]]; by default the loopback of the process, in memory. With
// Zenoh (pong listening on the meeting point, ping connecting to it) or MQTT, see main.

class PingAgent : public cam::Agent {
public:
	explicit PingAgent(const std::string& p_name) : Agent(p_name) {}

	void setup() override {
		std::cout << "[" << get_name() << "] Sending request" << std::endl;
		// The pong agent lives in another environment: network scope, asked explicitly.
		cam::transport::SendOptions l_options;
		l_options.m_scope = cam::transport::Scope::Network;
		send_by_name("PongAgent", {{"data", "request-info"}, {"from", get_name()}}, true, l_options);
	}

	void action(const cam::MessagePointer& p_message) override {
		std::cout << p_message->content() << std::endl;
	}
};

class PongAgent : public cam::Agent {
public:
	explicit PongAgent(const std::string& p_name) : Agent(p_name) {}

	void setup() override {
		std::cout << "[" << get_name() << "] Waiting for requests..." << std::endl;
	}

	void action(const cam::MessagePointer& p_message) override {
		std::cout << p_message->content() << std::endl;
		if (p_message->content()["data"] == "request-info") {
			std::string l_info = "Info 1 from agent " + get_name() + " in container " + get_environment()->get_id();
			send(p_message->get_sender(), l_info);

			l_info = "Info 2 from agent " + get_name() + " in container " + get_environment()->get_id();
			send(p_message->get_sender(), l_info);
		}
	}
};

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
	// Without argument the two environments connect to the loopback of the process, available in every build.
	// With a protocol they also connect to that network, the host and the port of the meeting point by default
	// 127.0.0.1 and the port of the protocol, for example:
	//   MobilePingPong zenoh 127.0.0.1 17448
	//   MobilePingPong mqtt 127.0.0.1 1883
	// Between the environments of this process, the traffic stays on the loopback.
	auto l_network = network_of(argc, argv, 1, "pingpong");
	if (!l_network) {
		return 1;
	}
	if (l_network->m_protocol == cam::Protocol::None) {
		// Connected explicitly: the ping agent sends its request at its setup, when the pong environment must be there
		l_network->m_protocol = cam::Protocol::Loopback;
	}

	// A remote message reaches its agent at the turn after its reception: a few turns are needed.
	cam::Environment l_environment_b({.m_turns = 10, .m_mode = cam::EnvironmentMasMode::Parallel, .m_delay_after_turn = 100, .m_network = *l_network});
	cam::Environment l_environment_a({.m_turns = 10, .m_mode = cam::EnvironmentMasMode::Parallel, .m_delay_after_turn = 100, .m_network = *l_network});
	// Connected before their threads start, so that each environment knows the other at its first turn
	for (auto* const l_environment : {&l_environment_b, &l_environment_a}) {
		if (const auto l_result = l_environment->connect(); !l_result) {
			std::cerr << "Cannot connect: " << l_result.error().m_message << std::endl;
			return 1;
		}
	}

	l_environment_b.add<PongAgent>("PongAgent");
	std::thread l_environment_1_thread(&cam::Environment::start, &l_environment_b);

	l_environment_a.add<PingAgent>("PingAgent");
	std::thread l_environment_2_thread(&cam::Environment::start, &l_environment_a);

	l_environment_1_thread.join();
	l_environment_2_thread.join();
}
