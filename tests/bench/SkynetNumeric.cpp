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

// SkynetNumeric benchmark, protocol of the C# v3.1 E30/SkynetNumeric example:
// Skynet with 20 000 agents, sequential random order, numeric message
// contents (a JSON integer stands for the C# ContentObj long); expected "Long Sum = 199990000".
// Options: --parallel, --sequential (fixed order), --agents N, --csv.

#include <iomanip>

#include "common.hpp"

namespace {

	constexpr int CHILD_COUNT = 10;
	int s_max_agents = 20000;

	class SkynetNumericAgent final : public cam::Agent {
	private:
		long m_id;
		long m_parent_id;
		long m_sum;
		int m_messages_left;

	public:
		SkynetNumericAgent(const std::string& p_name, const long p_id, const long p_parent_id) :
				Agent(p_name),
				m_id(p_id),
				m_parent_id(p_parent_id),
				m_sum(p_id),
				m_messages_left(0) {}

		void setup() override {
			for (int i = 1; i <= CHILD_COUNT; i++) {
				if (const long l_new_id = m_id * 10 + i; l_new_id < s_max_agents) {
					get_environment()->add<SkynetNumericAgent>("a" + std::to_string(l_new_id), l_new_id, m_id);
					m_messages_left++;
				}
			}
			if (m_messages_left == 0) {
				report(m_id);
				stop();
			}
		}

		void action(const cam::MessagePointer& p_message) override {
			m_sum += p_message->content().get<long>();
			m_messages_left--;
			if (m_messages_left == 0) {
				if (get_name() == "a0") {
					std::cout << "Long Sum = " << m_sum << "\n";
				} else {
					report(m_sum);
				}
				stop();
			}
		}

	private:
		void report(const long p_value) const {
			send_by_name("a" + std::to_string(m_parent_id), json(p_value), true);
		}
	};

} // namespace

int main(int argc, char** argv) {
	const bench::Options l_options = bench::parse(argc, argv, 20000);
	s_max_agents = l_options.m_agents;
	auto l_mode = cam::EnvironmentMasMode::SequentialRandom;
	if (l_options.m_parallel) {
		l_mode = cam::EnvironmentMasMode::Parallel;
	} else if (l_options.m_sequential) {
		l_mode = cam::EnvironmentMasMode::Sequential;
	}

	const bench::Stopwatch l_stopwatch;
	{
		cam::Environment l_environment(0, l_mode);
		l_environment.add<SkynetNumericAgent>("a0", 0, -1);
		l_environment.start();
	}
	const long long l_elapsed_us = l_stopwatch.elapsed_us();

	if (l_options.m_csv) {
		std::cout << "skynetnumeric," << s_max_agents << "," << l_elapsed_us << "\n";
	} else {
		std::cout << std::fixed << std::setprecision(3) << static_cast<double>(l_elapsed_us) / 1000.0 << " ms\n";
	}
	return 0;
}
