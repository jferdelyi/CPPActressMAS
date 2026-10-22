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

// Harness program of the oracle tests (tests/iso): the C# original as it is, compared with its oracle by
// iso.PingPong.
// Port of the C# benchmark E30 Benchmarks/PingPong (ActressMAS v3.1): ten agents send a message to every other one, then
// answer every message they receive, until 10 000 000 messages; five trials, a new environment for each. Parallel,
// 10 000 turns. The C# counts the messages in a volatile int, which parallel agents increment without atomicity: here
// the counter is atomic (ISO), and the C# check-then-send is kept (a few messages over the limit).
// Arguments: none for the example; "sequential-random" for the variant commented in its Program.cs (100 000 turns).

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>

#include <DotNet.hpp>
#include <Environment.h>

struct Global {
	static inline std::atomic<std::int64_t> s_no_messages{0};
	static constexpr std::int64_t s_max_messages = 10 * 1000 * 1000;
	static constexpr int s_no_agents = 10;
};

class MyAgent final : public cam::Agent {
	private:
		int m_id;

	public:
		MyAgent(const std::string& p_name, const int p_id) :
				Agent(p_name),
				m_id(p_id) {}

		void setup() override {
			for (int i = 0; i < Global::s_no_agents; i++) {
				if (i != m_id) {
					send_by_name("a" + std::to_string(i), "msg");
					Global::s_no_messages++;
				}
			}
		}

		void action(const cam::MessagePointer& p_message) override {
			if (Global::s_no_messages < Global::s_max_messages) {
				send(p_message->get_sender(), "msg");
				Global::s_no_messages++;
			}
		}
};

int main(int argc, char** argv) {
	constexpr int l_n = 5;
	const cam::EnvironmentMasMode l_mode = cam::dotnet::scenario_mode(argc, argv, cam::EnvironmentMasMode::Parallel);
	double l_sum = 0;
	for (int t = 0; t < l_n; t++) {
		cam::dotnet::write_line("Trial " + std::to_string(t + 1));
		Global::s_no_messages = 0;
		const auto l_start = std::chrono::steady_clock::now();

		cam::Environment l_environment({.m_turns = l_mode == cam::EnvironmentMasMode::Parallel ? 10000 : 100000,
										.m_mode = l_mode,
										.m_seed = static_cast<unsigned int>(cam::dotnet::next_site_seed("EnvironmentMas.cs:61")),
										.m_random = cam::dotnet::random_engine()});
		for (int i = 0; i < Global::s_no_agents; i++) {
			l_environment.add<MyAgent>("a" + std::to_string(i), i);
		}
		l_environment.start();

		const auto l_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - l_start).count();
		cam::dotnet::write_line(std::to_string(Global::s_no_messages.load()) + " msg");
		cam::dotnet::write_line(std::to_string(l_elapsed) + " ms\n");
		l_sum += static_cast<double>(Global::s_no_messages.load()) / static_cast<double>(l_elapsed);
	}
	cam::dotnet::write_line(cam::dotnet::format_fixed(l_sum / l_n, 3) + " msg/ms");
	return 0;
}
