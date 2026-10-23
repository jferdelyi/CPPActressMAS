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

// PingPong by pairs, the variant that the ActressMAS article cites without its code ("pairs of agents, 100 messages"),
// with the same definition as its C# counterpart (tools/csharp-ref/ref/PingPong.Pairs): 500 pairs of agents, no turn
// limit, 5 trials, a new environment per trial, the stopwatch covers the creation of the environment, the agents and
// start(). In each pair, the first agent sends "msg" by name to the second at setup; each agent replies to every message
// (by id: the C++ sender of a message is its identifier, the C# one its name) until it has sent half of the messages of
// its pair, and stops once it has also received as many. The simulation ends when every agent has stopped, after
// exactly pairs * messages messages. The counter is atomic (relaxed); the time of a trial is printed to the microsecond,
// the workload being short.
// Options: --sequential (Sequential mode, fixed order, no pool), --pairs, --messages (per pair, even), --trials, --csv.

#include <atomic>
#include <iomanip>

#include "common.hpp"

namespace {

	const std::string MESSAGE = "msg";

	std::atomic<long> s_messages{0};
	int s_messages_per_agent = 50;

	class PairAgent final : public cam::Agent {
	private:
		int m_index;
		int m_sent = 0;
		int m_received = 0;

	public:
		PairAgent(const std::string& p_name, const int p_index) :
				Agent(p_name),
				m_index(p_index) {}

		void setup() override {
			if (m_index % 2 == 0) {
				send_by_name("a" + std::to_string(m_index + 1), MESSAGE, true);
				m_sent++;
				s_messages.fetch_add(1, std::memory_order_relaxed);
			}
		}

		void action(const cam::MessagePointer& p_message) override {
			m_received++;
			if (m_sent < s_messages_per_agent) {
				send(p_message->get_sender(), MESSAGE);
				m_sent++;
				s_messages.fetch_add(1, std::memory_order_relaxed);
			}
			if (m_sent == s_messages_per_agent && m_received == s_messages_per_agent) {
				stop();
			}
		}
	};

} // namespace

int main(int argc, char** argv) {
	const bench::Options l_options = bench::parse(argc, argv, 0);
	s_messages_per_agent = l_options.m_messages / 2;
	const auto l_mode = l_options.m_sequential ? cam::EnvironmentMasMode::Sequential : cam::EnvironmentMasMode::Parallel;
	double l_sum = 0;

	for (int l_trial = 1; l_trial <= l_options.m_trials; l_trial++) {
		s_messages.store(0, std::memory_order_relaxed);
		if (!l_options.m_csv) {
			std::cout << "Trial " << l_trial << "\n";
		}

		const bench::Stopwatch l_stopwatch;
		{
			cam::Environment l_environment(0, l_mode);
			for (int i = 0; i < 2 * l_options.m_pairs; i++) {
				l_environment.add<PairAgent>("a" + std::to_string(i), i);
			}
			l_environment.start();
		}
		const long long l_elapsed_us = l_stopwatch.elapsed_us();

		const long l_messages = s_messages.load(std::memory_order_relaxed);
		const double l_throughput = static_cast<double>(l_messages) / (static_cast<double>(l_elapsed_us) / 1000.0);
		l_sum += l_throughput;
		if (l_options.m_csv) {
			std::cout << "pingpongpairs," << l_trial << "," << l_messages << "," << l_elapsed_us << ","
					  << std::fixed << std::setprecision(3) << l_throughput << "\n";
		} else {
			std::cout << l_messages << " msg\n" << std::fixed << std::setprecision(3) << static_cast<double>(l_elapsed_us) / 1000.0
					  << " ms\n\n";
		}
	}

	if (!l_options.m_csv) {
		std::cout << std::fixed << std::setprecision(3) << l_sum / l_options.m_trials << " msg/ms\n";
	}
	return 0;
}
