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

// Growth of a benchmark with its number of agents: a guard against a complexity regression, whatever the machine.
//
// Usage: cam_bench_scaling <max ratio> <runs> -- <command> [args...] --agents <N> [args...]
//
// The command runs <runs> times with N agents and <runs> times with 4N agents, alternately; each run prints "<time> ms".
// The first run of each size warms up and is dropped. The test passes when the median time with 4N agents is less
// than <max ratio> times the median with N: a linear cost gives about 4, a quadratic one about 16.
// Exit code: 0 within the ratio, 1 above it, 2 usage or a run that fails.

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "measure.hpp"

int main(int argc, char** argv) {
	const std::vector<std::string> l_arguments(argv + 1, argv + argc);
	std::size_t l_agents_index = 0;
	for (std::size_t i = 3; i + 1 < l_arguments.size(); i++) {
		if (l_arguments[i] == "--agents") {
			l_agents_index = i + 1;
		}
	}
	const double l_max_ratio = l_arguments.size() > 1 ? std::atof(l_arguments[0].c_str()) : 0;
	const int l_runs = l_arguments.size() > 1 ? std::atoi(l_arguments[1].c_str()) : 0;
	if (l_arguments.size() < 4 || l_arguments[2] != "--" || l_max_ratio <= 0 || l_runs < 2 || l_agents_index == 0 ||
		std::atoi(l_arguments[l_agents_index].c_str()) < 1) {
		std::cerr << "usage: cam_bench_scaling <max ratio> <runs, 2 at least> -- <command> [args...] --agents <N> [args...]\n";
		return 2;
	}
	const int l_agents = std::atoi(l_arguments[l_agents_index].c_str());

	// The command with N agents, then with 4N agents
	std::vector<std::string> l_commands;
	for (const int l_count: {l_agents, 4 * l_agents}) {
		std::string l_command;
		for (std::size_t i = 3; i < l_arguments.size(); i++) {
			l_command += (i == 3 ? "" : " ") + bench::quote(i == l_agents_index ? std::to_string(l_count) : l_arguments[i]);
		}
		l_commands.push_back(l_command);
	}

	std::vector<double> l_times[2];
	for (int l_run = 1; l_run <= l_runs; l_run++) {
		for (std::size_t l_size = 0; l_size < 2; l_size++) {
			int l_exit_code = 0;
			const std::string l_output = bench::run(l_commands[l_size], l_exit_code);
			const auto l_time = bench::value_of_run("ms", l_output);
			if (l_exit_code != 0 || !l_time) {
				std::cout << "FAIL run " << l_run << " exited with status " << l_exit_code << " or printed no time: " << l_commands[l_size]
						  << "\n" << l_output;
				return 2;
			}
			if (l_run > 1) {
				l_times[l_size].push_back(*l_time);
			}
		}
	}

	const double l_small = bench::median(l_times[0]);
	const double l_large = bench::median(l_times[1]);
	const double l_ratio = l_large / l_small;
	const bool l_within = l_ratio < l_max_ratio;
	std::cout << std::fixed << std::setprecision(3) << l_agents << " agents: median " << l_small << " ms; " << 4 * l_agents
			  << " agents: median " << l_large << " ms; " << std::setprecision(2) << "ratio " << l_ratio << " (at most "
			  << l_max_ratio << ") over " << l_times[0].size() << " runs each\n"
			  << (l_within ? "scaling within the ratio\n" : "FAIL the cost grows faster than the ratio\n");
	return l_within ? 0 : 1;
}
