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

// PingPong benchmark, protocol of the C# v3.1 E30/PingPong example:
// 10 agents, 10 000 turns, 10 million messages at most, 5 trials, a new environment per trial, the
// stopwatch covers the creation of the environment, the agents and start(). At setup every agent
// sends "msg", a text like the C# string, by name to the 9 others and counts 9 messages; a reply goes
// back to the sender (by id: the C++ sender of a message is its identifier, the C# one its name). The counter is
// atomic (relaxed) and the throughput is computed from microseconds, so a trial cannot divide by zero.
// Options: --sequential (Sequential mode, fixed order, no pool), --turns, --trials, --agents, --csv.

#include <atomic>
#include <iomanip>

#include "common.hpp"

namespace {

	constexpr long MAX_MESSAGES = 10 * 1000 * 1000;
	const std::string MESSAGE = "msg";

	std::atomic<long> s_messages{0};
	int s_agents = 10;

	class PingPongAgent final : public cam::Agent {
	private:
		int m_index;

	public:
		PingPongAgent(const std::string& p_name, const int p_index) :
				Agent(p_name),
				m_index(p_index) {}

		void setup() override {
			for (int i = 0; i < s_agents; i++) {
				if (i != m_index) {
					send_by_name("a" + std::to_string(i), MESSAGE, true);
					s_messages.fetch_add(1, std::memory_order_relaxed);
				}
			}
		}

		void action(const cam::MessagePointer& p_message) override {
			if (s_messages.load(std::memory_order_relaxed) < MAX_MESSAGES) {
				send(p_message->get_sender(), MESSAGE);
				s_messages.fetch_add(1, std::memory_order_relaxed);
			}
		}
	};

} // namespace

int main(int argc, char** argv) {
	const bench::Options l_options = bench::parse(argc, argv, 10);
	s_agents = l_options.m_agents;
	const auto l_mode = l_options.m_sequential ? cam::EnvironmentMasMode::Sequential : cam::EnvironmentMasMode::Parallel;
	double l_sum = 0;

	for (int l_trial = 1; l_trial <= l_options.m_trials; l_trial++) {
		s_messages.store(0, std::memory_order_relaxed);
		if (!l_options.m_csv) {
			std::cout << "Trial " << l_trial << "\n";
		}

		const bench::Stopwatch l_stopwatch;
		{
			cam::Environment l_environment(l_options.m_turns, l_mode);
			for (int i = 0; i < s_agents; i++) {
				l_environment.add<PingPongAgent>("a" + std::to_string(i), i);
			}
			l_environment.start();
		}
		const long long l_elapsed_us = l_stopwatch.elapsed_us();

		const long l_messages = s_messages.load(std::memory_order_relaxed);
		const double l_throughput = static_cast<double>(l_messages) / (static_cast<double>(l_elapsed_us) / 1000.0);
		l_sum += l_throughput;
		if (l_options.m_csv) {
			std::cout << "pingpong," << l_trial << "," << l_messages << "," << l_elapsed_us << ","
					  << std::fixed << std::setprecision(3) << l_throughput << "\n";
		} else {
			std::cout << l_messages << " msg\n" << l_elapsed_us / 1000 << " ms\n\n";
		}
	}

	if (!l_options.m_csv) {
		std::cout << std::fixed << std::setprecision(3) << l_sum / l_options.m_trials << " msg/ms\n";
	}
	return 0;
}
