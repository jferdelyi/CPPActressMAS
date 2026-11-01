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

// Skynet benchmark, protocol of the C# v3.1 E30/Skynet example:
// a tree of 10 000 agents (10 children per node) created during the simulation, each leaf sends its
// identifier to its parent as text, each node sums and reports; expected "Sum = 49995000". Parallel
// mode without a turn limit; the stopwatch covers the creation of the environment, the root and
// start(). Parent addressed by name ("a<id>"), as the C# does.
// Options: --sequential, --agents N, --csv.

#include <iomanip>

#include "common.hpp"

namespace {

	constexpr int CHILD_COUNT = 10;
	int s_max_agents = 10000;

	class SkynetAgent final : public cam::Agent {
	private:
		long m_id;
		long m_parent_id;
		long m_sum;
		int m_messages_left;

	public:
		SkynetAgent(const std::string& p_name, const long p_id, const long p_parent_id) :
				Agent(p_name),
				m_id(p_id),
				m_parent_id(p_parent_id),
				m_sum(p_id),
				m_messages_left(0) {}

		void setup() override {
			for (int i = 1; i <= CHILD_COUNT; i++) {
				if (const long l_new_id = m_id * 10 + i; l_new_id < s_max_agents) {
					get_environment()->add<SkynetAgent>("a" + std::to_string(l_new_id), l_new_id, m_id);
					m_messages_left++;
				}
			}
			if (m_messages_left == 0) {
				report(m_id);
				stop();
			}
		}

		void action(const cam::MessagePointer& p_message) override {
			m_sum += std::stol(p_message->to_string());
			m_messages_left--;
			if (m_messages_left == 0) {
				if (get_name() == "a0") {
					std::cout << "Sum = " << m_sum << "\n";
				} else {
					report(m_sum);
				}
				stop();
			}
		}

	private:
		void report(const long p_value) const {
			const std::string l_text = std::to_string(p_value);
			send_by_name("a" + std::to_string(m_parent_id), bench::raw(l_text), l_text.size(), true);
		}
	};

} // namespace

int main(int argc, char** argv) {
	const bench::Options l_options = bench::parse(argc, argv, 10000);
	s_max_agents = l_options.m_agents;
	const auto l_mode = l_options.m_sequential ? cam::EnvironmentMasMode::Sequential : cam::EnvironmentMasMode::Parallel;

	const bench::Stopwatch l_stopwatch;
	{
		cam::Environment l_environment(0, l_mode);
		l_environment.add<SkynetAgent>("a0", 0, -1);
		l_environment.start();
	}
	const long long l_elapsed_us = l_stopwatch.elapsed_us();

	if (l_options.m_csv) {
		std::cout << "skynet," << s_max_agents << "," << l_elapsed_us << "\n";
	} else {
		std::cout << std::fixed << std::setprecision(3) << static_cast<double>(l_elapsed_us) / 1000.0 << " ms\n";
	}
	return 0;
}
