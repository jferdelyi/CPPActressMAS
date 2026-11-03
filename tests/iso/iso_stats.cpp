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

// Distribution test of an example with its C# original, where the C# itself is not deterministic: statistics of runs
// at many seeds, the C# sample against the C++ one, by a two-sample Kolmogorov-Smirnov test (Edmonds and Hales, 2003).
//
// Usage: iso_stats --self-test                             the test on known samples
//        iso_stats --extract <spec> <folder> <seeds>        statistics of the C# outputs <folder>/seed1.txt ... (the
//                                                            file to version in tests/iso/distributions/)
//        iso_stats <spec> <statistics> <seeds> -- <command> [args...]
//                                                            runs the C++ example at CAM_SEED 1 to <seeds> and compares
// A spec file holds one statistic per line ('#' starts a comment):
//   stat NAME value REGEX       the number of the first group of the last line that matches REGEX
//   stat NAME count REGEX       the number of lines that match REGEX
//   stat NAME sum REGEX         the sum of the first group over the lines that match REGEX
//   stat NAME max REGEX         the largest first group over the lines that match REGEX
//   alpha A                     the significance level of the test, 0.001 by default
// A statistic is the same distribution on both sides when the p-value of the test is above alpha.
// Exit code: 0 same distributions, 1 different, 2 usage or a run that fails.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>
#include <stdexcept>
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

	struct Statistic {
		std::string m_name;
		std::string m_kind;
		std::regex m_regex;
	};

	struct Spec {
		std::vector<Statistic> m_statistics;
		double m_alpha = 0.001;
	};

	// Thrown for a spec or usage error (exit code 2)
	struct UsageError final : std::runtime_error {
		using std::runtime_error::runtime_error;
	};

	std::string trim(const std::string& p_text) {
		const auto l_first = p_text.find_first_not_of(" \t\r");
		const auto l_last = p_text.find_last_not_of(" \t\r");
		return l_first == std::string::npos ? std::string() : p_text.substr(l_first, l_last - l_first + 1);
	}

	std::vector<std::string> lines_of(std::istream& p_stream) {
		std::vector<std::string> l_lines;
		for (std::string l_line; std::getline(p_stream, l_line);) {
			if (!l_line.empty() && l_line.back() == '\r') {
				l_line.pop_back();
			}
			l_lines.push_back(l_line);
		}
		return l_lines;
	}

	Spec read_spec(const std::string& p_path) {
		std::ifstream l_file(p_path);
		if (!l_file) {
			throw UsageError("cannot read " + p_path);
		}
		Spec l_spec;
		for (const auto& l_raw: lines_of(l_file)) {
			const std::string l_line = trim(l_raw);
			if (l_line.empty() || l_line[0] == '#') {
				continue;
			}
			std::istringstream l_stream(l_line);
			std::string l_keyword;
			l_stream >> l_keyword;
			if (l_keyword == "alpha") {
				l_stream >> l_spec.m_alpha;
			} else if (l_keyword == "stat") {
				Statistic l_statistic;
				l_stream >> l_statistic.m_name >> l_statistic.m_kind;
				if (l_statistic.m_kind != "value" && l_statistic.m_kind != "count" && l_statistic.m_kind != "sum" && l_statistic.m_kind != "max") {
					throw UsageError("unknown statistic \"" + l_statistic.m_kind + "\" in: " + l_line);
				}
				std::string l_regex;
				std::getline(l_stream, l_regex);
				l_statistic.m_regex = std::regex(trim(l_regex));
				l_spec.m_statistics.push_back(l_statistic);
			} else {
				throw UsageError("unknown rule: " + l_line);
			}
		}
		if (l_spec.m_statistics.empty()) {
			throw UsageError("no statistic in " + p_path);
		}
		return l_spec;
	}

	// The statistics of one output, in the order of the spec; NaN for a value that no line gives
	std::vector<double> measure(const Spec& p_spec, const std::vector<std::string>& p_lines) {
		std::vector<double> l_values;
		for (const auto& l_statistic: p_spec.m_statistics) {
			double l_value = l_statistic.m_kind == "value" || l_statistic.m_kind == "max" ? std::nan("") : 0;
			for (const auto& l_line: p_lines) {
				std::smatch l_match;
				if (!std::regex_search(l_line, l_match, l_statistic.m_regex)) {
					continue;
				}
				if (l_statistic.m_kind == "count") {
					l_value += 1;
					continue;
				}
				if (l_match.size() < 2) {
					throw UsageError("the regex of " + l_statistic.m_name + " has no group");
				}
				const double l_number = std::stod(l_match[1].str());
				if (l_statistic.m_kind == "value") {
					l_value = l_number;
				} else if (l_statistic.m_kind == "sum") {
					l_value += l_number;
				} else {
					l_value = std::isnan(l_value) ? l_number : std::max(l_value, l_number);
				}
			}
			l_values.push_back(l_value);
		}
		return l_values;
	}

	// P-value of the two-sample Kolmogorov-Smirnov statistic p_d for samples of p_n1 and p_n2 values (asymptotic
	// distribution with the correction of Stephens, as in Numerical Recipes)
	double ks_p_value(const double p_d, const std::size_t p_n1, const std::size_t p_n2) {
		const double l_n = static_cast<double>(p_n1 * p_n2) / static_cast<double>(p_n1 + p_n2);
		const double l_lambda = (std::sqrt(l_n) + 0.12 + 0.11 / std::sqrt(l_n)) * p_d;
		if (l_lambda < 1e-6) {
			return 1;
		}
		double l_sum = 0;
		for (int k = 1; k <= 100; k++) {
			const double l_term = 2 * ((k % 2 == 1) ? 1 : -1) * std::exp(-2 * k * k * l_lambda * l_lambda);
			l_sum += l_term;
			if (std::fabs(l_term) < 1e-12) {
				break;
			}
		}
		return std::clamp(l_sum, 0.0, 1.0);
	}

	// Largest distance between the empirical distribution functions of two samples
	double ks_distance(std::vector<double> p_first, std::vector<double> p_second) {
		std::sort(p_first.begin(), p_first.end());
		std::sort(p_second.begin(), p_second.end());
		std::size_t i = 0;
		std::size_t j = 0;
		double l_distance = 0;
		while (i < p_first.size() && j < p_second.size()) {
			const double l_value = std::min(p_first[i], p_second[j]);
			while (i < p_first.size() && p_first[i] <= l_value) {
				i++;
			}
			while (j < p_second.size() && p_second[j] <= l_value) {
				j++;
			}
			l_distance = std::max(l_distance, std::fabs(static_cast<double>(i) / static_cast<double>(p_first.size()) -
														 static_cast<double>(j) / static_cast<double>(p_second.size())));
		}
		return l_distance;
	}

	double mean(const std::vector<double>& p_values) {
		double l_sum = 0;
		for (const double l_value: p_values) {
			l_sum += l_value;
		}
		return p_values.empty() ? std::nan("") : l_sum / static_cast<double>(p_values.size());
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

	void set_seed(const int p_seed) {
#if defined(_WIN32)
		_putenv_s("CAM_SEED", std::to_string(p_seed).c_str());
#else
		setenv("CAM_SEED", std::to_string(p_seed).c_str(), 1);
#endif
	}

	std::string quote(const std::string& p_argument) {
		return "\"" + p_argument + "\"";
	}

	// The statistics of the C# outputs <folder>/seed<n>.txt: a header with the names, then one line per seed
	int extract(const Spec& p_spec, const std::string& p_folder, const int p_seeds) {
		std::cout << "# seed";
		for (const auto& l_statistic: p_spec.m_statistics) {
			std::cout << " " << l_statistic.m_name;
		}
		std::cout << "\n" << std::setprecision(17);
		for (int l_seed = 1; l_seed <= p_seeds; l_seed++) {
			std::ifstream l_file(p_folder + "/seed" + std::to_string(l_seed) + ".txt");
			if (!l_file) {
				throw UsageError("cannot read " + p_folder + "/seed" + std::to_string(l_seed) + ".txt");
			}
			std::cout << l_seed;
			for (const double l_value: measure(p_spec, lines_of(l_file))) {
				std::cout << " " << l_value;
			}
			std::cout << "\n";
		}
		return 0;
	}

	// The C# sample of each statistic, from the file written by extract
	std::vector<std::vector<double>> read_statistics(const std::string& p_path, const std::size_t p_count) {
		std::ifstream l_file(p_path);
		if (!l_file) {
			throw UsageError("cannot read " + p_path);
		}
		std::vector<std::vector<double>> l_samples(p_count);
		for (const auto& l_line: lines_of(l_file)) {
			if (l_line.empty() || l_line[0] == '#') {
				continue;
			}
			std::istringstream l_stream(l_line);
			int l_seed = 0;
			l_stream >> l_seed;
			for (std::size_t i = 0; i < p_count; i++) {
				std::string l_text;
				if (!(l_stream >> l_text)) {
					throw UsageError("a line of " + p_path + " has less than " + std::to_string(p_count) + " values: " + l_line);
				}
				l_samples[i].push_back(std::stod(l_text));
			}
		}
		return l_samples;
	}

	int compare(const Spec& p_spec, const std::string& p_statistics, const int p_seeds, const std::string& p_command) {
		const auto l_csharp = read_statistics(p_statistics, p_spec.m_statistics.size());
		std::vector<std::vector<double>> l_cpp(p_spec.m_statistics.size());
		for (int l_seed = 1; l_seed <= p_seeds; l_seed++) {
			set_seed(l_seed);
			int l_exit_code = 0;
			const std::string l_output = run(p_command, l_exit_code);
			if (l_exit_code != 0) {
				std::cout << "FAIL command exited with status " << l_exit_code << " at seed " << l_seed << ": " << p_command << "\n";
				return 2;
			}
			std::istringstream l_stream(l_output);
			const auto l_values = measure(p_spec, lines_of(l_stream));
			for (std::size_t i = 0; i < l_values.size(); i++) {
				l_cpp[i].push_back(l_values[i]);
			}
		}
		int l_failures = 0;
		for (std::size_t i = 0; i < p_spec.m_statistics.size(); i++) {
			const auto& l_name = p_spec.m_statistics[i].m_name;
			const bool l_missing = std::any_of(l_csharp[i].begin(), l_csharp[i].end(), [](const double p_value) { return std::isnan(p_value); }) ||
								   std::any_of(l_cpp[i].begin(), l_cpp[i].end(), [](const double p_value) { return std::isnan(p_value); });
			const double l_distance = l_missing ? 1 : ks_distance(l_csharp[i], l_cpp[i]);
			const double l_p_value = l_missing ? 0 : ks_p_value(l_distance, l_csharp[i].size(), l_cpp[i].size());
			const bool l_ok = l_p_value > p_spec.m_alpha;
			std::cout << (l_ok ? "OK   " : "FAIL ") << "stat " << l_name << ": mean " << std::setprecision(6) << mean(l_csharp[i]) << " in C# ("
					  << l_csharp[i].size() << " runs), " << mean(l_cpp[i]) << " in C++ (" << l_cpp[i].size() << " runs), D = " << l_distance
					  << ", p = " << l_p_value << (l_missing ? " (a run gives no value)" : "") << "\n";
			l_failures += l_ok ? 0 : 1;
		}
		std::cout << (l_failures == 0 ? "same distributions\n" : std::to_string(l_failures) + " difference(s)\n");
		return l_failures == 0 ? 0 : 1;
	}

	// The test on known samples: equal samples, disjoint ones, and the textbook distance of two shifted samples
	int self_test() {
		const std::vector<double> l_a{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
		const std::vector<double> l_b{11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
		const std::vector<double> l_c{3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
		const bool l_equal = ks_distance(l_a, l_a) == 0 && ks_p_value(0, 10, 10) == 1;
		const bool l_disjoint = ks_distance(l_a, l_b) == 1 && ks_p_value(1, 10, 10) < 0.001;
		const bool l_shifted = std::fabs(ks_distance(l_a, l_c) - 0.2) < 1e-12 && ks_p_value(0.2, 10, 10) > 0.9;
		std::cout << "equal " << l_equal << ", disjoint " << l_disjoint << ", shifted " << l_shifted << "\n";
		std::cout << (l_equal && l_disjoint && l_shifted ? "self-test ok\n" : "FAIL self-test\n");
		return l_equal && l_disjoint && l_shifted ? 0 : 1;
	}

} // namespace

int main(int argc, char** argv) {
	try {
		const std::vector<std::string> l_arguments(argv + 1, argv + argc);
		if (l_arguments.size() == 1 && l_arguments[0] == "--self-test") {
			return self_test();
		}
		if (l_arguments.size() == 4 && l_arguments[0] == "--extract") {
			return extract(read_spec(l_arguments[1]), l_arguments[2], std::stoi(l_arguments[3]));
		}
		if (l_arguments.size() >= 5 && l_arguments[3] == "--") {
			std::string l_command = quote(l_arguments[4]);
			for (std::size_t i = 5; i < l_arguments.size(); i++) {
				l_command += " " + quote(l_arguments[i]);
			}
			return compare(read_spec(l_arguments[0]), l_arguments[1], std::stoi(l_arguments[2]), l_command);
		}
		std::cerr << "usage: iso_stats --extract <spec> <folder> <seeds>\n"
				  << "       iso_stats <spec> <statistics> <seeds> -- <command> [args...]\n";
		return 2;
	} catch (const std::exception& p_error) {
		std::cerr << "iso_stats: " << p_error.what() << "\n";
		return 2;
	}
}
