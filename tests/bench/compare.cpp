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

// Comparison of a benchmark with its C# original measured on the same machine: "faster than the C#".
//
// Usage: cam_bench_compare <benchmark> <runs> -- <command> [args...]
//
// The reference is the file named by CAM_BENCH_REFERENCE, written by tests/tools/csharp-ref (action benchref) on the
// machine that runs the test; one line per benchmark, '#' starts a comment:
//   <benchmark> ms <median>          lower is better: each run prints "<time> ms"; the first run warms up and is dropped
//   <benchmark> msg_per_ms <median>  higher is better: each trial prints "<messages> msg" then "<time> ms"; the first
//                                    trial of each run warms up and is dropped, a run gives the mean msg/ms of the others
// The command runs <runs> times (CAM_BENCH_RUNS in the environment, if set, as on the C# side); the test passes when the
// median of the C++ runs is faster than the C# median: a speedup above CAM_BENCH_MIN_SPEEDUP (1 when not set).
// Exit code: 0 faster, 1 not faster, 2 usage or a run that fails, 77 no reference for this benchmark (skipped).

#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "measure.hpp"

namespace {

	struct Reference {
		std::string m_metric;
		double m_median = 0;
	};

	std::optional<Reference> read_reference(const std::string& p_benchmark) {
		const char* l_path = std::getenv("CAM_BENCH_REFERENCE");
		if (!l_path || !*l_path) {
			return std::nullopt;
		}
		std::ifstream l_file(l_path);
		for (std::string l_line; std::getline(l_file, l_line);) {
			if (l_line.empty() || l_line[0] == '#') {
				continue;
			}
			std::istringstream l_stream(l_line);
			std::string l_name;
			Reference l_reference;
			if (l_stream >> l_name >> l_reference.m_metric >> l_reference.m_median && l_name == p_benchmark) {
				return l_reference;
			}
		}
		return std::nullopt;
	}

} // namespace

int main(int argc, char** argv) {
	const std::vector<std::string> l_arguments(argv + 1, argv + argc);
	if (l_arguments.size() < 4 || l_arguments[2] != "--" || std::atoi(l_arguments[1].c_str()) < 2) {
		std::cerr << "usage: cam_bench_compare <benchmark> <runs, 2 at least> -- <command> [args...]\n";
		return 2;
	}
	const std::string& l_benchmark = l_arguments[0];
	const char* l_runs_override = std::getenv("CAM_BENCH_RUNS");
	const int l_runs = l_runs_override && std::atoi(l_runs_override) >= 2 ? std::atoi(l_runs_override) : std::atoi(l_arguments[1].c_str());
	const auto l_reference = read_reference(l_benchmark);
	if (!l_reference) {
		std::cout << "SKIP no C# reference for " << l_benchmark << " (CAM_BENCH_REFERENCE, tests/bench/README.md)\n";
		return 77;
	}
	if (l_reference->m_metric != "ms" && l_reference->m_metric != "msg_per_ms") {
		std::cerr << "cam_bench_compare: unknown metric " << l_reference->m_metric << "\n";
		return 2;
	}
	std::string l_command = bench::quote(l_arguments[3]);
	for (std::size_t i = 4; i < l_arguments.size(); i++) {
		l_command += " " + bench::quote(l_arguments[i]);
	}

	std::vector<double> l_values;
	for (int l_run = 1; l_run <= l_runs; l_run++) {
		int l_exit_code = 0;
		const std::string l_output = bench::run(l_command, l_exit_code);
		const auto l_value = bench::value_of_run(l_reference->m_metric, l_output);
		if (l_exit_code != 0 || !l_value) {
			std::cout << "FAIL run " << l_run << " exited with status " << l_exit_code << " or printed no " << l_reference->m_metric
					  << ": " << l_command << "\n" << l_output;
			return 2;
		}
		// The first run of a time warms up, as on the C# side
		if (l_reference->m_metric == "msg_per_ms" || l_run > 1) {
			l_values.push_back(*l_value);
		}
	}

	const double l_median = bench::median(l_values);
	const bool l_lower_is_better = l_reference->m_metric == "ms";
	const double l_speedup = l_lower_is_better ? l_reference->m_median / l_median : l_median / l_reference->m_median;
	const char* l_minimum_text = std::getenv("CAM_BENCH_MIN_SPEEDUP");
	const double l_minimum = l_minimum_text && *l_minimum_text ? std::atof(l_minimum_text) : 1.0;
	const bool l_faster = l_speedup > l_minimum;
	std::cout << std::fixed << std::setprecision(3) << l_benchmark << ": C++ median " << l_median << " " << l_reference->m_metric
			  << " over " << l_values.size() << " runs, C# median " << l_reference->m_median << ", C++ " << std::setprecision(2)
			  << l_speedup << "x the C#\n"
			  << (l_faster ? "faster than the C#\n" : "FAIL not faster than the C#\n");
	return l_faster ? 0 : 1;
}
