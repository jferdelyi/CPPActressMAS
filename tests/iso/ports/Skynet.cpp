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
// iso.Skynet.
// Port of the C# benchmark E30 Benchmarks/Skynet (ActressMAS v3.1): each agent creates up to ten children, numbered
// id * 10 + i below 10 000, in its setup; the leaves send their number to their parent, which adds the numbers of its
// children to its own and sends the sum up; the root prints the sum of 0 to 9 999. Parallel, no turn limit.

#include <chrono>
#include <cstdint>
#include <string>

#include <DotNet.hpp>
#include <Environment.h>

struct Global {
	static constexpr std::int64_t s_max_agents = 10000;
	static constexpr int s_no_children = 10;
};

class MyAgent final : public cam::Agent {
	private:
		std::int64_t m_id;
		std::int64_t m_parent_id;
		std::int64_t m_sum;
		int m_messages_left = 0;

	public:
		MyAgent(const std::string& p_name, const std::int64_t p_id, const std::int64_t p_parent_id) :
				Agent(p_name),
				m_id(p_id),
				m_parent_id(p_parent_id),
				m_sum(p_id) {}

		void setup() override {
			for (int i = 1; i <= Global::s_no_children; i++) {
				const std::int64_t l_new_id = m_id * 10 + i;
				if (l_new_id < Global::s_max_agents) {
					get_environment()->add<MyAgent>("a" + std::to_string(l_new_id), l_new_id, m_id);
					m_messages_left++;
				}
			}
			if (m_messages_left == 0) {
				send_by_name("a" + std::to_string(m_parent_id), std::to_string(m_id));  // send id to parent
				stop();
			}
		}

		void action(const cam::MessagePointer& p_message) override {
			m_sum += std::stoll(p_message->to_string());
			m_messages_left--;
			if (m_messages_left == 0) {
				if (get_name() == "a0") {
					cam::dotnet::write_line("Sum = " + std::to_string(m_sum));  // root reporting
				} else {
					send_by_name("a" + std::to_string(m_parent_id), std::to_string(m_sum));  // send sum to parent
				}
				stop();
			}
		}
};

int main() {
	const auto l_start = std::chrono::steady_clock::now();
	cam::Environment l_environment({.m_turns = 0,
									.m_mode = cam::EnvironmentMasMode::Parallel,
									.m_seed = static_cast<unsigned int>(cam::dotnet::next_site_seed("EnvironmentMas.cs:61")),
									.m_random = cam::RandomEngine::DotNet});
	l_environment.add<MyAgent>("a0", 0, -1);  // the root agent
	l_environment.start();
	const auto l_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - l_start).count();
	cam::dotnet::write_line(cam::dotnet::format_fixed(static_cast<double>(l_elapsed), 3) + " ms");
	return 0;
}
