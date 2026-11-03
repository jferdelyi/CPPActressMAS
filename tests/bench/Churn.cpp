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

// Churn benchmark: agents added and removed during the turns, the guard of their cost against a complexity
// regression (a search of the agents added in the turn at every removal once made the removals quadratic). N spawners in
// Sequential mode; at each action, a spawner removes the child it added at its previous action, inserted since, then
// adds a new one: each turn adds N agents, which wait for their insertion, and removes N inserted ones. The stopwatch
// covers the creation of the environment, the agents and start().
// Options: --agents N (spawners, 2 000 by default), --turns T (20 by default, the setup turn included).

#include <cstring>
#include <iomanip>
#include <optional>

#include "common.hpp"

namespace {

	class Child final : public cam::Agent {
	public:
		explicit Child(const std::string& p_name) : Agent(p_name) {}
	};

	class Spawner final : public cam::Agent {
	private:
		std::optional<cam::AgentId> m_child;

	public:
		explicit Spawner(const std::string& p_name) : Agent(p_name) {}

		void default_action() override {
			if (m_child) {
				get_environment()->remove(*m_child);
			}
			m_child = get_environment()->add<Child>(get_name() + "-child");
		}
	};

} // namespace

int main(int argc, char** argv) {
	const bench::Options l_options = bench::parse(argc, argv, 2000);
	// The default of --turns is the one of PingPong: 20 here unless it is given
	int l_turns = 20;
	for (int i = 1; i < argc; i++) {
		if (std::strcmp(argv[i], "--turns") == 0) {
			l_turns = l_options.m_turns;
		}
	}

	const bench::Stopwatch l_stopwatch;
	std::size_t l_agents = 0;
	{
		cam::Environment l_environment(l_turns, cam::EnvironmentMasMode::Sequential);
		for (int i = 0; i < l_options.m_agents; i++) {
			l_environment.add<Spawner>("s" + std::to_string(i));
		}
		l_environment.start();
		l_agents = l_environment.agents_count();
	}
	const long long l_elapsed_us = l_stopwatch.elapsed_us();

	std::cout << l_agents << " agents\n" << std::fixed << std::setprecision(3) << static_cast<double>(l_elapsed_us) / 1000.0 << " ms\n";
	return 0;
}
