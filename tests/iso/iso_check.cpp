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

// ISO comparison of an example with its C# original: facts, never text.
//
// Usage: iso_check <rules> <oracle> -- <command> [args...]   runs the C++ example and compares
//        iso_check <rules> <oracle> --actual <file>          compares a saved output (fixtures)
//        iso_check --extract <rules> <file>                  prints the facts of an output
//
// The oracle is the raw output of the C# example (tests/iso/oracles/, produced by tests/tools/csharp-ref); lines starting
// with "#cam" are its header. A rules file holds one rule per line ('#' starts a comment):
//   fact NAME REGEX             each line matching REGEX (searched) gives a tuple: its groups, or the whole match
//   section REGEX               a line matching REGEX starts a new section; every tuple then begins with its section
//   same NAME bag|seq|set|count [by C,C...] [num C TOL] [first N]
//                               compares the tuples of NAME with the oracle: multiset, order (only within each group of
//                               the "by" columns), set or number; "num" compares column C as numbers within TOL; "first"
//                               keeps only the first N tuples of each output
//   check NAME AGG [by C,C...] OP VALUE
//                               invariant on both outputs: AGG is count, sum(C), min(C), max(C) or distinct(C); OP is
//                               ==, !=, <, <=, >, >= or in; VALUE is a number, [low,high] or OTHER.AGG
//   ignore REGEX                lines that are no fact
//   cover all                   every other line must match a fact or a section
//   optional NAME               the oracle may have no tuple of NAME (otherwise it is an error: a typo in a rule
//                               would compare two empty lists)
// Columns are numbered from 0; with sections, column 0 is the section. Exit code: 0 same behaviour, 1 different, 2 usage.

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <set>
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

	using Tuple = std::vector<std::string>;
	using Facts = std::map<std::string, std::vector<Tuple>>;

	struct Fact {
		std::string m_name;
		std::regex m_regex;
	};

	struct Same {
		std::string m_name;
		std::string m_mode;
		std::vector<std::size_t> m_by;
		int m_numeric_column = -1;
		double m_tolerance = 0;
		std::size_t m_first = 0;
	};

	struct Check {
		std::string m_name;
		std::string m_aggregate;
		std::vector<std::size_t> m_by;
		std::string m_operator;
		std::string m_value;
	};

	struct Rules {
		std::vector<Fact> m_facts;
		std::vector<std::regex> m_sections;
		std::vector<std::regex> m_ignored;
		bool m_cover = false;
		std::set<std::string> m_optional;
		std::vector<Same> m_same;
		std::vector<Check> m_checks;
	};

	// Thrown for a rules or usage error (exit code 2)
	struct UsageError final : std::runtime_error {
		using std::runtime_error::runtime_error;
	};

	std::string join(const Tuple& p_tuple) {
		std::string l_text;
		for (std::size_t i = 0; i < p_tuple.size(); i++) {
			l_text += (i == 0 ? "" : "|") + p_tuple[i];
		}
		return "(" + l_text + ")";
	}

	std::string trim(const std::string& p_text) {
		const auto l_first = p_text.find_first_not_of(" \t\r");
		const auto l_last = p_text.find_last_not_of(" \t\r");
		return l_first == std::string::npos ? std::string() : p_text.substr(l_first, l_last - l_first + 1);
	}

	std::vector<std::string> read_lines(const std::string& p_path) {
		std::ifstream l_file(p_path, std::ios::binary);
		if (!l_file) {
			throw UsageError("cannot read " + p_path);
		}
		std::vector<std::string> l_lines;
		for (std::string l_line; std::getline(l_file, l_line);) {
			if (!l_line.empty() && l_line.back() == '\r') {
				l_line.pop_back();
			}
			l_lines.push_back(l_line);
		}
		return l_lines;
	}

	std::vector<std::string> split_lines(const std::string& p_text) {
		std::vector<std::string> l_lines;
		std::stringstream l_stream(p_text);
		for (std::string l_line; std::getline(l_stream, l_line);) {
			if (!l_line.empty() && l_line.back() == '\r') {
				l_line.pop_back();
			}
			l_lines.push_back(l_line);
		}
		return l_lines;
	}

	std::vector<std::size_t> columns(const std::string& p_list) {
		std::vector<std::size_t> l_columns;
		std::stringstream l_stream(p_list);
		for (std::string l_column; std::getline(l_stream, l_column, ',');) {
			l_columns.push_back(std::stoul(l_column));
		}
		return l_columns;
	}

	// ---------------------------------------------------------------- rules

	Rules read_rules(const std::string& p_path) {
		Rules l_rules;
		for (const auto& l_raw: read_lines(p_path)) {
			const std::string l_line = trim(l_raw);
			if (l_line.empty() || l_line[0] == '#') {
				continue;
			}
			std::stringstream l_stream(l_line);
			std::string l_keyword;
			l_stream >> l_keyword;
			const auto l_rest = [&] {
				std::string l_text;
				std::getline(l_stream, l_text);
				return trim(l_text);
			};
			if (l_keyword == "fact") {
				std::string l_name;
				l_stream >> l_name;
				l_rules.m_facts.push_back({l_name, std::regex(l_rest())});
			} else if (l_keyword == "section") {
				l_rules.m_sections.emplace_back(l_rest());
			} else if (l_keyword == "ignore") {
				l_rules.m_ignored.emplace_back(l_rest());
			} else if (l_keyword == "cover") {
				l_rules.m_cover = true;
			} else if (l_keyword == "optional") {
				std::string l_name;
				l_stream >> l_name;
				l_rules.m_optional.insert(l_name);
			} else if (l_keyword == "same") {
				Same l_same;
				l_stream >> l_same.m_name >> l_same.m_mode;
				if (l_same.m_mode != "bag" && l_same.m_mode != "seq" && l_same.m_mode != "set" && l_same.m_mode != "count") {
					throw UsageError("unknown comparison \"" + l_same.m_mode + "\" in: " + l_line);
				}
				for (std::string l_word; l_stream >> l_word;) {
					if (l_word == "by") {
						l_stream >> l_word;
						l_same.m_by = columns(l_word);
					} else if (l_word == "num") {
						l_stream >> l_same.m_numeric_column >> l_same.m_tolerance;
					} else if (l_word == "first") {
						if (!(l_stream >> l_same.m_first) || l_same.m_first == 0) {
							throw UsageError("\"first\" needs a positive number in: " + l_line);
						}
					} else {
						throw UsageError("unexpected \"" + l_word + "\" in: " + l_line);
					}
				}
				l_rules.m_same.push_back(l_same);
			} else if (l_keyword == "check") {
				Check l_check;
				l_stream >> l_check.m_name >> l_check.m_aggregate;
				std::string l_word;
				l_stream >> l_word;
				if (l_word == "by") {
					l_stream >> l_word;
					l_check.m_by = columns(l_word);
					l_stream >> l_word;
				}
				l_check.m_operator = l_word;
				l_check.m_value = l_rest();
				l_rules.m_checks.push_back(l_check);
			} else {
				throw UsageError("unknown rule: " + l_line);
			}
		}
		return l_rules;
	}

	// ---------------------------------------------------------------- facts

	Facts extract(const Rules& p_rules, const std::vector<std::string>& p_lines, std::vector<std::string>& p_uncovered) {
		Facts l_facts;
		for (const auto& l_fact: p_rules.m_facts) {
			l_facts[l_fact.m_name];
		}
		int l_section = 0;
		for (const auto& l_line: p_lines) {
			if (l_line.rfind("#cam", 0) == 0) {
				continue;
			}
			bool l_covered = false;
			for (const auto& l_regex: p_rules.m_sections) {
				if (std::regex_search(l_line, l_regex)) {
					++l_section;
					l_covered = true;
				}
			}
			for (const auto& l_fact: p_rules.m_facts) {
				std::smatch l_match;
				if (!std::regex_search(l_line, l_match, l_fact.m_regex)) {
					continue;
				}
				Tuple l_tuple;
				if (!p_rules.m_sections.empty()) {
					l_tuple.push_back(std::to_string(l_section));
				}
				if (l_match.size() == 1) {
					l_tuple.push_back(l_match[0]);
				}
				for (std::size_t i = 1; i < l_match.size(); i++) {
					l_tuple.push_back(l_match[i]);
				}
				l_facts[l_fact.m_name].push_back(l_tuple);
				l_covered = true;
			}
			const bool l_ignored = std::any_of(p_rules.m_ignored.begin(), p_rules.m_ignored.end(),
											   [&](const std::regex& p_regex) { return std::regex_search(l_line, p_regex); });
			if (!l_covered && !l_ignored) {
				p_uncovered.push_back(l_line);
			}
		}
		return l_facts;
	}

	// ---------------------------------------------------------------- comparison

	bool equal(const Tuple& p_left, const Tuple& p_right, const Same& p_same) {
		if (p_left.size() != p_right.size()) {
			return false;
		}
		for (std::size_t i = 0; i < p_left.size(); i++) {
			if (static_cast<int>(i) == p_same.m_numeric_column) {
				try {
					if (std::fabs(std::stod(p_left[i]) - std::stod(p_right[i])) > p_same.m_tolerance) {
						return false;
					}
					continue;
				} catch (const std::exception&) {
					return false;
				}
			}
			if (p_left[i] != p_right[i]) {
				return false;
			}
		}
		return true;
	}

	// Empty when equal, otherwise the first difference
	std::string compare_lists(const std::vector<Tuple>& p_expected, const std::vector<Tuple>& p_actual, const Same& p_same) {
		const std::size_t l_count = std::min(p_expected.size(), p_actual.size());
		for (std::size_t i = 0; i < l_count; i++) {
			if (!equal(p_expected[i], p_actual[i], p_same)) {
				return "at #" + std::to_string(i) + ": C# " + join(p_expected[i]) + ", C++ " + join(p_actual[i]);
			}
		}
		if (p_expected.size() != p_actual.size()) {
			return std::to_string(p_expected.size()) + " tuples in C#, " + std::to_string(p_actual.size()) + " in C++; first extra: " +
				   join(p_expected.size() > l_count ? p_expected[l_count] : p_actual[l_count]);
		}
		return {};
	}

	Tuple key(const Tuple& p_tuple, const std::vector<std::size_t>& p_columns) {
		Tuple l_key;
		for (const auto l_column: p_columns) {
			l_key.push_back(l_column < p_tuple.size() ? p_tuple[l_column] : std::string());
		}
		return l_key;
	}

	std::string compare(const Same& p_same, std::vector<Tuple> p_expected, std::vector<Tuple> p_actual) {
		if (p_same.m_first > 0) {
			// Only the first tuples of each output, in their order
			p_expected.resize(std::min(p_expected.size(), p_same.m_first));
			p_actual.resize(std::min(p_actual.size(), p_same.m_first));
		}
		if (p_same.m_mode == "count") {
			return p_expected.size() == p_actual.size() ? std::string() :
				   std::to_string(p_expected.size()) + " in C#, " + std::to_string(p_actual.size()) + " in C++";
		}
		if (p_same.m_mode == "bag" || p_same.m_mode == "set") {
			std::sort(p_expected.begin(), p_expected.end());
			std::sort(p_actual.begin(), p_actual.end());
			if (p_same.m_mode == "set") {
				p_expected.erase(std::unique(p_expected.begin(), p_expected.end()), p_expected.end());
				p_actual.erase(std::unique(p_actual.begin(), p_actual.end()), p_actual.end());
			}
			return compare_lists(p_expected, p_actual, p_same);
		}
		// seq: order kept within each group of the "by" columns (a single group without "by")
		std::map<Tuple, std::pair<std::vector<Tuple>, std::vector<Tuple>>> l_groups;
		for (const auto& l_tuple: p_expected) {
			l_groups[key(l_tuple, p_same.m_by)].first.push_back(l_tuple);
		}
		for (const auto& l_tuple: p_actual) {
			l_groups[key(l_tuple, p_same.m_by)].second.push_back(l_tuple);
		}
		for (const auto& [l_key, l_lists]: l_groups) {
			if (const std::string l_difference = compare_lists(l_lists.first, l_lists.second, p_same); !l_difference.empty()) {
				return (p_same.m_by.empty() ? std::string() : "group " + join(l_key) + " ") + l_difference;
			}
		}
		return {};
	}

	// ---------------------------------------------------------------- invariants

	double aggregate(const std::vector<Tuple>& p_tuples, const std::string& p_aggregate) {
		if (p_aggregate == "count") {
			return static_cast<double>(p_tuples.size());
		}
		const auto l_open = p_aggregate.find('(');
		if (l_open == std::string::npos || p_aggregate.back() != ')') {
			throw UsageError("unknown aggregate " + p_aggregate);
		}
		const std::string l_function = p_aggregate.substr(0, l_open);
		const std::size_t l_column = std::stoul(p_aggregate.substr(l_open + 1, p_aggregate.size() - l_open - 2));
		std::vector<std::string> l_values;
		for (const auto& l_tuple: p_tuples) {
			if (l_column >= l_tuple.size()) {
				throw UsageError("column " + std::to_string(l_column) + " out of " + join(l_tuple));
			}
			l_values.push_back(l_tuple[l_column]);
		}
		if (l_function == "distinct") {
			return static_cast<double>(std::set<std::string>(l_values.begin(), l_values.end()).size());
		}
		std::vector<double> l_numbers;
		for (const auto& l_value: l_values) {
			l_numbers.push_back(std::stod(l_value));
		}
		if (l_function == "sum") {
			double l_sum = 0;
			for (const double l_number: l_numbers) {
				l_sum += l_number;
			}
			return l_sum;
		}
		if (l_numbers.empty()) {
			return std::nan("");
		}
		if (l_function == "min") {
			return *std::min_element(l_numbers.begin(), l_numbers.end());
		}
		if (l_function == "max") {
			return *std::max_element(l_numbers.begin(), l_numbers.end());
		}
		throw UsageError("unknown aggregate " + p_aggregate);
	}

	bool holds(const double p_value, const std::string& p_operator, const std::string& p_reference, const Facts& p_facts) {
		if (p_operator == "in") {
			const auto l_comma = p_reference.find(',');
			if (p_reference.front() != '[' || p_reference.back() != ']' || l_comma == std::string::npos) {
				throw UsageError("expected [low,high] after in, got " + p_reference);
			}
			return p_value >= std::stod(p_reference.substr(1, l_comma - 1)) && p_value <= std::stod(p_reference.substr(l_comma + 1));
		}
		double l_reference;
		if (const auto l_dot = p_reference.find('.'); l_dot != std::string::npos && !std::isdigit(static_cast<unsigned char>(p_reference[0])) &&
				p_reference[0] != '-') {
			const auto l_other = p_facts.find(p_reference.substr(0, l_dot));
			if (l_other == p_facts.end()) {
				throw UsageError("unknown fact in " + p_reference);
			}
			l_reference = aggregate(l_other->second, p_reference.substr(l_dot + 1));
		} else {
			l_reference = std::stod(p_reference);
		}
		if (p_operator == "==") return p_value == l_reference;
		if (p_operator == "!=") return p_value != l_reference;
		if (p_operator == "<") return p_value < l_reference;
		if (p_operator == "<=") return p_value <= l_reference;
		if (p_operator == ">") return p_value > l_reference;
		if (p_operator == ">=") return p_value >= l_reference;
		throw UsageError("unknown operator " + p_operator);
	}

	// Failures of one invariant on one output
	std::vector<std::string> verify(const Check& p_check, const Facts& p_facts) {
		std::vector<std::string> l_failures;
		std::map<Tuple, std::vector<Tuple>> l_groups;
		for (const auto& l_tuple: p_facts.at(p_check.m_name)) {
			l_groups[key(l_tuple, p_check.m_by)].push_back(l_tuple);
		}
		if (l_groups.empty()) {
			l_groups[Tuple()];
		}
		for (const auto& [l_key, l_tuples]: l_groups) {
			const double l_value = aggregate(l_tuples, p_check.m_aggregate);
			if (!holds(l_value, p_check.m_operator, p_check.m_value, p_facts)) {
				std::ostringstream l_text;
				l_text << (p_check.m_by.empty() ? std::string() : "group " + join(l_key) + " ") << p_check.m_aggregate << " = " << l_value;
				l_failures.push_back(l_text.str());
			}
		}
		return l_failures;
	}

	// ---------------------------------------------------------------- run

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

	int compare_outputs(const Rules& p_rules, const std::vector<std::string>& p_oracle, const std::vector<std::string>& p_actual) {
		std::vector<std::string> l_oracle_uncovered;
		std::vector<std::string> l_actual_uncovered;
		const Facts l_oracle = extract(p_rules, p_oracle, l_oracle_uncovered);
		const Facts l_actual = extract(p_rules, p_actual, l_actual_uncovered);
		int l_failures = 0;
		const auto l_report = [&](const bool p_ok, const std::string& p_rule, const std::string& p_detail) {
			std::cout << (p_ok ? "OK   " : "FAIL ") << p_rule << (p_detail.empty() ? "" : ": " + p_detail) << "\n";
			l_failures += p_ok ? 0 : 1;
		};
		for (const auto& [l_name, l_tuples]: l_oracle) {
			if (l_tuples.empty() && !p_rules.m_optional.contains(l_name)) {
				l_report(false, "fact " + l_name, "no tuple in the C# oracle (mark it optional if expected)");
			}
		}
		for (const auto& l_same: p_rules.m_same) {
			if (!l_oracle.contains(l_same.m_name)) {
				throw UsageError("same on unknown fact " + l_same.m_name);
			}
			const std::string l_difference = compare(l_same, l_oracle.at(l_same.m_name), l_actual.at(l_same.m_name));
			l_report(l_difference.empty(), "same " + l_same.m_name + " " + l_same.m_mode + " (" + std::to_string(l_oracle.at(l_same.m_name).size()) + " tuples)",
					 l_difference);
		}
		for (const auto& l_check: p_rules.m_checks) {
			if (!l_oracle.contains(l_check.m_name)) {
				throw UsageError("check on unknown fact " + l_check.m_name);
			}
			std::string l_by;
			for (const auto l_column: l_check.m_by) {
				l_by += (l_by.empty() ? " by " : ",") + std::to_string(l_column);
			}
			const std::string l_rule = "check " + l_check.m_name + " " + l_check.m_aggregate + l_by + " " + l_check.m_operator + " " + l_check.m_value;
			for (const auto& [l_side, l_facts]: {std::pair<std::string, const Facts*>{"C#", &l_oracle}, {"C++", &l_actual}}) {
				const auto l_failed = verify(l_check, *l_facts);
				l_report(l_failed.empty(), l_rule + " on " + l_side, l_failed.empty() ? std::string() : l_failed.front());
			}
		}
		if (p_rules.m_cover) {
			for (const auto& [l_side, l_lines]: {std::pair<std::string, const std::vector<std::string>*>{"C#", &l_oracle_uncovered},
												  {"C++", &l_actual_uncovered}}) {
				l_report(l_lines->empty(), "cover all on " + l_side,
						 l_lines->empty() ? std::string() : std::to_string(l_lines->size()) + " lines without fact, first: \"" + l_lines->front() + "\"");
			}
		}
		std::cout << (l_failures == 0 ? "same behaviour\n" : std::to_string(l_failures) + " difference(s)\n");
		return l_failures == 0 ? 0 : 1;
	}

	std::string quote(const std::string& p_argument) {
		return "\"" + p_argument + "\"";
	}

} // namespace

int main(int argc, char** argv) {
	try {
		const std::vector<std::string> l_arguments(argv + 1, argv + argc);
		if (l_arguments.size() == 3 && l_arguments[0] == "--extract") {
			std::vector<std::string> l_uncovered;
			const Rules l_rules = read_rules(l_arguments[1]);
			for (const auto& [l_name, l_tuples]: extract(l_rules, read_lines(l_arguments[2]), l_uncovered)) {
				for (const auto& l_tuple: l_tuples) {
					std::cout << l_name << " " << join(l_tuple) << "\n";
				}
			}
			for (const auto& l_line: l_uncovered) {
				std::cout << "uncovered \"" << l_line << "\"\n";
			}
			return 0;
		}
		if (l_arguments.size() == 4 && l_arguments[2] == "--actual") {
			return compare_outputs(read_rules(l_arguments[0]), read_lines(l_arguments[1]), read_lines(l_arguments[3]));
		}
		if (l_arguments.size() >= 4 && l_arguments[2] == "--") {
			std::string l_command = quote(l_arguments[3]);
			for (std::size_t i = 4; i < l_arguments.size(); i++) {
				l_command += " " + quote(l_arguments[i]);
			}
			int l_exit_code = 0;
			const std::string l_output = run(l_command, l_exit_code);
			if (l_exit_code != 0) {
				std::cout << "FAIL command exited with status " << l_exit_code << ": " << l_command << "\n" << l_output;
				return 1;
			}
			return compare_outputs(read_rules(l_arguments[0]), read_lines(l_arguments[1]), split_lines(l_output));
		}
		std::cerr << "usage: iso_check <rules> <oracle> -- <command> [args...]\n"
				  << "       iso_check <rules> <oracle> --actual <file>\n"
				  << "       iso_check --extract <rules> <file>\n";
		return 2;
	} catch (const UsageError& p_error) {
		std::cerr << "iso_check: " << p_error.what() << "\n";
		return 2;
	} catch (const std::exception& p_error) {
		std::cerr << "iso_check: " << p_error.what() << "\n";
		return 2;
	}
}
