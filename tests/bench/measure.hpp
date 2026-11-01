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

// Running a benchmark program and reading what it prints, for the tools that measure (compare.cpp, scaling.cpp).

#pragma once

#include <algorithm>
#include <array>
#include <cstdio>
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

namespace bench {

	/**
	 * Run a command through the shell and read its output.
	 * @param p_command The command, its arguments quoted
	 * @param p_exit_code Set to its exit status (-1 when it cannot run)
	 * @return Its standard output
	 **/
	inline std::string run(const std::string& p_command, int& p_exit_code) {
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

	/**
	 * The value of one run: its last time for "ms", the mean msg/ms of its trials but the first for "msg_per_ms".
	 * @param p_metric "ms" or "msg_per_ms"
	 * @param p_output What the run printed: "<time> ms" lines, each after a "<messages> msg" line for msg_per_ms
	 * @return The value, or nothing when the output has none
	 **/
	inline std::optional<double> value_of_run(const std::string& p_metric, const std::string& p_output) {
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

	/**
	 * Median of values.
	 * @param p_values The values, at least one
	 * @return The median
	 **/
	inline double median(std::vector<double> p_values) {
		std::sort(p_values.begin(), p_values.end());
		const std::size_t l_middle = p_values.size() / 2;
		return p_values.size() % 2 == 1 ? p_values[l_middle] : (p_values[l_middle - 1] + p_values[l_middle]) / 2;
	}

	/**
	 * Quote an argument for the shell of run().
	 * @param p_argument The argument
	 * @return The argument between double quotes
	 **/
	inline std::string quote(const std::string& p_argument) {
		return "\"" + p_argument + "\"";
	}

} // namespace bench
