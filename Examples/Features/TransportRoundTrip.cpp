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

// C++ feature example (no C# original): round trips between two environments of this process.
// A ping agent of environment A and a pong agent of environment B exchange messages by id; the directory of the
// transport tells A where the pong agent lives. TransportRoundTrip [round trips [protocol [host [port]]]]: 1000 round
// trips by default; without protocol both environments meet on the loopback of the process, without configuration;
// with one they also connect to that network, for instance:
//   TransportRoundTrip 1000 zenoh 127.0.0.1 17449
//   TransportRoundTrip 1000 mqtt 127.0.0.1 1883
// Between the environments of this process, the traffic stays on the loopback: the network is crossed by two
// processes.

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <thread>

#include <Environment.h>

namespace {

	std::atomic<bool> g_done{false};

	// Answers every ping with a pong to its sender; stops with the ping agent.
	class PongAgent final : public cam::Agent {
		public:
			explicit PongAgent(const std::string& p_name) : Agent(p_name) {}

			void action(const cam::MessagePointer& p_message) override {
				send(p_message->get_sender(), std::string("pong"));
			}

			void default_action() override {
				if (g_done.load()) {
					stop();
				}
			}
	};

	// Sends a ping as soon as the directory knows the pong agent, then the next one at each pong.
	class PingAgent final : public cam::Agent {
		private:
			cam::AgentId m_pong_id;
			long m_round_trips;
			long m_count = 0;
			bool m_started = false;
			std::chrono::steady_clock::time_point m_start;

		public:
			PingAgent(const std::string& p_name, const cam::AgentId& p_pong_id, const long p_round_trips) :
					Agent(p_name), m_pong_id(p_pong_id), m_round_trips(p_round_trips) {}

			void action(const cam::MessagePointer&) override {
				if (++m_count < m_round_trips) {
					send(m_pong_id, std::string("ping"));
					return;
				}
				const double l_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - m_start).count();
				std::cout << m_count << " round trips in " << l_ms << " ms: " << l_ms * 1000.0 / static_cast<double>(m_count)
						  << " us per round trip, " << static_cast<double>(2 * m_count) / l_ms << " messages per ms" << std::endl;
				g_done.store(true);
				stop();
			}

			void default_action() override {
				if (!m_started && send(m_pong_id, std::string("ping"))) {  // refused until the directory knows the pong agent
					m_started = true;
					m_start = std::chrono::steady_clock::now();
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
		for (const auto& [l_name, l_value]: {std::pair{"loopback", cam::Protocol::Loopback}, std::pair{"zenoh", cam::Protocol::Zenoh},
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

} // namespace

int main(int argc, char** argv) {
	const long l_round_trips = argc > 1 ? std::atol(argv[1]) : 1000;
	const auto l_network = network_of(argc, argv, 2, "roundtrip");
	if (!l_network) {
		return 1;
	}

	cam::Environment l_environment_a({.m_mode = cam::EnvironmentMasMode::Sequential, .m_network = *l_network, .m_endpoint = {.m_name = "A"}});
	cam::Environment l_environment_b({.m_mode = cam::EnvironmentMasMode::Sequential, .m_network = *l_network, .m_endpoint = {.m_name = "B"}});
	// Connected before their threads start (A first: with Zenoh it is the peer that listens); without protocol, each
	// joins the loopback at its start, and the ping agent waits for the directory to know the pong agent
	if (l_network->m_protocol != cam::Protocol::None) {
		for (auto* const l_environment: {&l_environment_a, &l_environment_b}) {
			if (const auto l_result = l_environment->connect(); !l_result) {
				std::cerr << "Cannot connect: " << l_result.error().m_message << std::endl;
				return 1;
			}
		}
	}

	const cam::AgentId l_pong = l_environment_b.add<PongAgent>("pong");
	l_environment_a.add<PingAgent>("ping", l_pong, l_round_trips);
	std::cout << "Round trips between two environments through the loopback of the process" << std::endl;
	std::thread l_thread_b(&cam::Environment::start, &l_environment_b);
	std::thread l_thread_a(&cam::Environment::start, &l_environment_a);
	l_thread_a.join();
	l_thread_b.join();
	return 0;
}
