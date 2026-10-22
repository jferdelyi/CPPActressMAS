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

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#define CAM_POPEN _popen
#define CAM_PCLOSE _pclose
#else
#define CAM_POPEN popen
#define CAM_PCLOSE pclose
#endif

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

	std::string run(const std::string& p_command, int& p_exit_code) {
		std::string l_output;
		FILE* l_pipe = CAM_POPEN(p_command.c_str(), "r");
		if (!l_pipe) {
			p_exit_code = -1;
			return l_output;
		}
		std::array<char, 4096> l_buffer{};
		for (std::size_t l_read; (l_read = std::fread(l_buffer.data(), 1, l_buffer.size(), l_pipe)) > 0;) {
			l_output.append(l_buffer.data(), l_read);
		}
		p_exit_code = CAM_PCLOSE(l_pipe);
		return l_output;
	}

	// The value of one run: its last time for "ms", the mean msg/ms of its trials but the first for "msg_per_ms"
	std::optional<double> value_of_run(const std::string& p_metric, const std::string& p_output) {
		static const std::regex s_time(R"(^\s*([0-9]+(\.[0-9]+)?) ms\s*$)");
		static const std::regex s_messages(R"(^\s*([0-9]+) msg\s*$)");
		std::istringstream l_stream(p_output);
		std::optional<double> l_time;
		std::optional<double> l_messages;
		std::vector<double> l_throughputs;
		for (std::string l_line; std::getline(l_stream, l_line);) {
			std::smatch l_match;
			if (std::regex_match(l_line, l_match, s_messages)) {
				l_messages = std::stod(l_match[1].str());
			} else if (std::regex_match(l_line, l_match, s_time)) {
				l_time = std::stod(l_match[1].str());
				if (l_messages && *l_time > 0) {
					l_throughputs.push_back(*l_messages / *l_time);
				}
				l_messages.reset();
			}
		}
		if (p_metric == "ms") {
			return l_time;
		}
		if (l_throughputs.size() < 2) {
			return std::nullopt;
		}
		double l_sum = 0;
		for (std::size_t i = 1; i < l_throughputs.size(); i++) {
			l_sum += l_throughputs[i];
		}
		return l_sum / static_cast<double>(l_throughputs.size() - 1);
	}

	double median(std::vector<double> p_values) {
		std::sort(p_values.begin(), p_values.end());
		const std::size_t l_middle = p_values.size() / 2;
		return p_values.size() % 2 == 1 ? p_values[l_middle] : (p_values[l_middle - 1] + p_values[l_middle]) / 2;
	}

	std::string quote(const std::string& p_argument) {
		return "\"" + p_argument + "\"";
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
	std::string l_command = quote(l_arguments[3]);
	for (std::size_t i = 4; i < l_arguments.size(); i++) {
		l_command += " " + quote(l_arguments[i]);
	}

	std::vector<double> l_values;
	for (int l_run = 1; l_run <= l_runs; l_run++) {
		int l_exit_code = 0;
		const std::string l_output = run(l_command, l_exit_code);
		const auto l_value = value_of_run(l_reference->m_metric, l_output);
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

	const double l_median = median(l_values);
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
