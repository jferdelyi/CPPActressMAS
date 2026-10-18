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

// C++ feature example (no C# original): round trips between two environments through a transport chosen by its URL.
// A ping agent of environment A and a pong agent of environment B exchange messages by id; the directory of the
// transport tells A where the pong agent lives. Without argument both environments share the in-process loopback;
// with URLs they go through the network, for instance:
//   TransportRoundTrip "zenoh://?listen=tcp/127.0.0.1:17449&ns=roundtrip" "zenoh://127.0.0.1:17449/roundtrip"
//   TransportRoundTrip "mqtt://127.0.0.1:1883/roundtrip"
// A third argument sets the number of round trips (1000 by default).

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
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

} // namespace

int main(int argc, char** argv) {
	const std::string l_url_a = argc > 1 ? argv[1] : "loopback://roundtrip";
	const std::string l_url_b = argc > 2 ? argv[2] : l_url_a;
	const long l_round_trips = argc > 3 ? std::atol(argv[3]) : 1000;

	cam::Environment l_environment_a({.m_mode = cam::EnvironmentMasMode::Sequential, .m_url = l_url_a, .m_endpoint = {.m_name = "A"}});
	cam::Environment l_environment_b({.m_mode = cam::EnvironmentMasMode::Sequential, .m_url = l_url_b, .m_endpoint = {.m_name = "B"}});
	// Connected before their threads start (A first: with Zenoh it is the peer that listens)
	for (auto* const l_environment: {&l_environment_a, &l_environment_b}) {
		if (const auto l_result = l_environment->connect(); !l_result) {
			std::cerr << "Cannot connect to " << l_environment->transport_url() << ": " << l_result.error().m_message << std::endl;
			return 1;
		}
	}

	const cam::AgentId l_pong = l_environment_b.add<PongAgent>("pong");
	l_environment_a.add<PingAgent>("ping", l_pong, l_round_trips);
	std::cout << "Round trips between two environments through " << l_url_a << std::endl;
	std::thread l_thread_b(&cam::Environment::start, &l_environment_b);
	std::thread l_thread_a(&cam::Environment::start, &l_environment_a);
	l_thread_a.join();
	l_thread_b.join();
	return 0;
}
