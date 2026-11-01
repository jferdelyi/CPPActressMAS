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
//   same NAME bag|seq|set|count [by C,C...] [num C TOL] [first N] [alias C REGEX]
//                               compares the tuples of NAME with the oracle: multiset, order (only within each group of
//                               the "by" columns), set or number; "num" compares column C as numbers within TOL; "first"
//                               keeps only the first N tuples of each output; "alias" (bag or set): once the equal tuples
//                               are paired, a C++ tuple whose column C matches REGEX is paired with a C# tuple equal on
//                               every other column (a known divergence, such as a name shown as an identifier)
//   same NAME aggregate AGG [tol T]
//                               compares an aggregate of NAME (as for check) between the C# and the C++, within T
//   expect NAME V0|V1|...       a published value: each output has a tuple of NAME equal to (V0, V1, ...)
//   check NAME AGG [by C,C...] OP VALUE
//                               invariant on both outputs: AGG is count, sum(C), min(C), max(C) or distinct(C); OP is
//                               ==, !=, <, <=, >, >= or in; VALUE is a number, [low,high] or OTHER.AGG
//   check NAME each [by C,C...] EXPR
//                               predicate on every tuple of both outputs: $N is column N, prev$N column N of the
//                               previous tuple of the same group (the first tuple of a group is skipped when EXPR uses
//                               it); numbers, + - * /, abs(), == != < <= > >=, and, or, parentheses
//   ignore REGEX                lines that are no fact
//   cover all                   every other line must match a fact or a section
//   optional NAME               the oracle may have no tuple of NAME (otherwise it is an error: a typo in a rule
//                               would compare two empty lists)
// Columns are numbered from 0; with sections, column 0 is the section. Exit code: 0 same behaviour, 1 different, 2 usage.
// With CAM_ISO_SAVE naming a folder, the output of the program is also written there, as <oracle>.out (<oracle>.seed<n>.out
// with CAM_SEED): the input of tests/tools/iso-mutate.py.

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
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
		int m_alias_column = -1;
		std::regex m_alias;
		std::string m_aggregate;
	};

	// Node of the expression of "check NAME each": a number, a column of the tuple or of the previous one, or an
	// operator on its operands
	struct Expression {
		enum class Kind {
			Number,
			Column,
			Previous,
			Negate,
			Abs,
			Binary
		};

		Kind m_kind = Kind::Number;
		double m_number = 0;
		std::size_t m_column = 0;
		std::string m_operator;
		std::vector<Expression> m_operands;
	};

	struct Check {
		std::string m_name;
		std::string m_aggregate;
		std::vector<std::size_t> m_by;
		std::string m_operator;
		std::string m_value;
		std::shared_ptr<Expression> m_each;
		bool m_uses_previous = false;
	};

	struct Expect {
		std::string m_name;
		std::vector<std::string> m_values;
	};

	struct Rules {
		std::vector<Fact> m_facts;
		std::vector<std::regex> m_sections;
		std::vector<std::regex> m_ignored;
		bool m_cover = false;
		std::set<std::string> m_optional;
		std::vector<Same> m_same;
		std::vector<Check> m_checks;
		std::vector<Expect> m_expects;
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

	// ---------------------------------------------------------------- expressions

	// Recursive descent parser of the expressions of "check NAME each"
	class ExpressionParser {
		private:
			std::vector<std::string> m_tokens;
			std::size_t m_position = 0;
			bool m_uses_previous = false;

			static std::vector<std::string> tokenize(const std::string& p_text) {
				std::vector<std::string> l_tokens;
				for (std::size_t i = 0; i < p_text.size();) {
					const char l_char = p_text[i];
					if (std::isspace(static_cast<unsigned char>(l_char))) {
						i++;
					} else if (std::isdigit(static_cast<unsigned char>(l_char)) || l_char == '.') {
						const std::size_t l_start = i;
						while (i < p_text.size() && (std::isdigit(static_cast<unsigned char>(p_text[i])) || p_text[i] == '.')) {
							i++;
						}
						l_tokens.push_back(p_text.substr(l_start, i - l_start));
					} else if (std::isalpha(static_cast<unsigned char>(l_char)) || l_char == '$') {
						// a word, $N, or prev$N
						const std::size_t l_start = i;
						while (i < p_text.size() && (std::isalnum(static_cast<unsigned char>(p_text[i])) || p_text[i] == '$')) {
							i++;
						}
						l_tokens.push_back(p_text.substr(l_start, i - l_start));
					} else if (i + 1 < p_text.size() && (p_text.compare(i, 2, "==") == 0 || p_text.compare(i, 2, "!=") == 0 ||
														   p_text.compare(i, 2, "<=") == 0 || p_text.compare(i, 2, ">=") == 0)) {
						l_tokens.push_back(p_text.substr(i, 2));
						i += 2;
					} else if (std::string("+-*/()<>").find(l_char) != std::string::npos) {
						l_tokens.emplace_back(1, l_char);
						i++;
					} else {
						throw UsageError("unexpected \"" + std::string(1, l_char) + "\" in the expression: " + p_text);
					}
				}
				return l_tokens;
			}

			[[nodiscard]] const std::string& peek() const {
				static const std::string s_end;
				return m_position < m_tokens.size() ? m_tokens[m_position] : s_end;
			}

			std::string next() {
				if (m_position >= m_tokens.size()) {
					throw UsageError("unexpected end of the expression");
				}
				return m_tokens[m_position++];
			}

			static Expression binary(const std::string& p_operator, Expression p_left, Expression p_right) {
				Expression l_node;
				l_node.m_kind = Expression::Kind::Binary;
				l_node.m_operator = p_operator;
				l_node.m_operands.push_back(std::move(p_left));
				l_node.m_operands.push_back(std::move(p_right));
				return l_node;
			}

			Expression parse_or() {
				Expression l_left = parse_and();
				while (peek() == "or") {
					next();
					l_left = binary("or", std::move(l_left), parse_and());
				}
				return l_left;
			}

			Expression parse_and() {
				Expression l_left = parse_comparison();
				while (peek() == "and") {
					next();
					l_left = binary("and", std::move(l_left), parse_comparison());
				}
				return l_left;
			}

			Expression parse_comparison() {
				Expression l_left = parse_sum();
				const std::string& l_operator = peek();
				if (l_operator == "==" || l_operator == "!=" || l_operator == "<" || l_operator == "<=" || l_operator == ">" || l_operator == ">=") {
					const std::string l_name = next();
					return binary(l_name, std::move(l_left), parse_sum());
				}
				return l_left;
			}

			Expression parse_sum() {
				Expression l_left = parse_term();
				while (peek() == "+" || peek() == "-") {
					const std::string l_name = next();
					l_left = binary(l_name, std::move(l_left), parse_term());
				}
				return l_left;
			}

			Expression parse_term() {
				Expression l_left = parse_unary();
				while (peek() == "*" || peek() == "/") {
					const std::string l_name = next();
					l_left = binary(l_name, std::move(l_left), parse_unary());
				}
				return l_left;
			}

			Expression parse_unary() {
				if (peek() == "-") {
					next();
					Expression l_node;
					l_node.m_kind = Expression::Kind::Negate;
					l_node.m_operands.push_back(parse_unary());
					return l_node;
				}
				return parse_primary();
			}

			Expression parse_primary() {
				const std::string l_token = next();
				Expression l_node;
				if (l_token == "(") {
					l_node = parse_or();
					if (next() != ")") {
						throw UsageError("\")\" expected in the expression");
					}
				} else if (l_token == "abs") {
					if (next() != "(") {
						throw UsageError("\"(\" expected after abs");
					}
					l_node.m_kind = Expression::Kind::Abs;
					l_node.m_operands.push_back(parse_or());
					if (next() != ")") {
						throw UsageError("\")\" expected after the argument of abs");
					}
				} else if (l_token.rfind("prev$", 0) == 0) {
					l_node.m_kind = Expression::Kind::Previous;
					l_node.m_column = std::stoul(l_token.substr(5));
					m_uses_previous = true;
				} else if (l_token[0] == '$') {
					l_node.m_kind = Expression::Kind::Column;
					l_node.m_column = std::stoul(l_token.substr(1));
				} else if (std::isdigit(static_cast<unsigned char>(l_token[0])) || l_token[0] == '.') {
					l_node.m_number = std::stod(l_token);
				} else {
					throw UsageError("unexpected \"" + l_token + "\" in the expression");
				}
				return l_node;
			}

		public:
			explicit ExpressionParser(const std::string& p_text) : m_tokens(tokenize(p_text)) {}

			Expression parse() {
				Expression l_expression = parse_or();
				if (m_position != m_tokens.size()) {
					throw UsageError("unexpected \"" + m_tokens[m_position] + "\" in the expression");
				}
				return l_expression;
			}

			[[nodiscard]] bool uses_previous() const {
				return m_uses_previous;
			}
	};

	double column_value(const Tuple& p_tuple, const std::size_t p_column) {
		if (p_column >= p_tuple.size()) {
			throw std::runtime_error("column " + std::to_string(p_column) + " out of " + join(p_tuple));
		}
		try {
			return std::stod(p_tuple[p_column]);
		} catch (const std::exception&) {
			throw std::runtime_error("column " + std::to_string(p_column) + " is not a number in " + join(p_tuple));
		}
	}

	double evaluate(const Expression& p_node, const Tuple& p_tuple, const Tuple* p_previous) {
		switch (p_node.m_kind) {
			case Expression::Kind::Number:
				return p_node.m_number;
			case Expression::Kind::Column:
				return column_value(p_tuple, p_node.m_column);
			case Expression::Kind::Previous:
				return column_value(*p_previous, p_node.m_column);
			case Expression::Kind::Negate:
				return -evaluate(p_node.m_operands[0], p_tuple, p_previous);
			case Expression::Kind::Abs:
				return std::fabs(evaluate(p_node.m_operands[0], p_tuple, p_previous));
			case Expression::Kind::Binary:
				break;
		}
		const double l_left = evaluate(p_node.m_operands[0], p_tuple, p_previous);
		const double l_right = evaluate(p_node.m_operands[1], p_tuple, p_previous);
		const std::string& l_operator = p_node.m_operator;
		if (l_operator == "+") {
			return l_left + l_right;
		}
		if (l_operator == "-") {
			return l_left - l_right;
		}
		if (l_operator == "*") {
			return l_left * l_right;
		}
		if (l_operator == "/") {
			return l_left / l_right;
		}
		if (l_operator == "==") {
			return l_left == l_right ? 1 : 0;
		}
		if (l_operator == "!=") {
			return l_left != l_right ? 1 : 0;
		}
		if (l_operator == "<") {
			return l_left < l_right ? 1 : 0;
		}
		if (l_operator == "<=") {
			return l_left <= l_right ? 1 : 0;
		}
		if (l_operator == ">") {
			return l_left > l_right ? 1 : 0;
		}
		if (l_operator == ">=") {
			return l_left >= l_right ? 1 : 0;
		}
		if (l_operator == "and") {
			return l_left != 0 && l_right != 0 ? 1 : 0;
		}
		if (l_operator == "or") {
			return l_left != 0 || l_right != 0 ? 1 : 0;
		}
		throw UsageError("unknown operator " + l_operator);
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
				if (l_same.m_mode == "aggregate") {
					l_stream >> l_same.m_aggregate;
					for (std::string l_word; l_stream >> l_word;) {
						if (l_word != "tol" || !(l_stream >> l_same.m_tolerance)) {
							throw UsageError("unexpected \"" + l_word + "\" in: " + l_line);
						}
					}
					l_rules.m_same.push_back(l_same);
					continue;
				}
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
					} else if (l_word == "alias") {
						if (l_same.m_mode != "bag" && l_same.m_mode != "set") {
							throw UsageError("\"alias\" applies to bag or set in: " + l_line);
						}
						if (!(l_stream >> l_same.m_alias_column) || l_same.m_alias_column < 0) {
							throw UsageError("\"alias\" needs a column and a regex in: " + l_line);
						}
						const std::string l_regex = l_rest();
						if (l_regex.empty()) {
							throw UsageError("\"alias\" needs a column and a regex in: " + l_line);
						}
						l_same.m_alias = std::regex(l_regex);
					} else {
						throw UsageError("unexpected \"" + l_word + "\" in: " + l_line);
					}
				}
				if (l_same.m_alias_column >= 0 && l_same.m_numeric_column >= 0) {
					throw UsageError("\"alias\" and \"num\" cannot be combined in: " + l_line);
				}
				l_rules.m_same.push_back(l_same);
			} else if (l_keyword == "expect") {
				Expect l_expect;
				l_stream >> l_expect.m_name;
				std::stringstream l_values(l_rest());
				for (std::string l_value; std::getline(l_values, l_value, '|');) {
					l_expect.m_values.push_back(l_value);
				}
				if (l_expect.m_values.empty()) {
					throw UsageError("\"expect\" needs values in: " + l_line);
				}
				l_rules.m_expects.push_back(l_expect);
			} else if (l_keyword == "check") {
				Check l_check;
				l_stream >> l_check.m_name >> l_check.m_aggregate;
				if (l_check.m_aggregate == "each") {
					std::string l_text = l_rest();
					if (l_text.rfind("by ", 0) == 0) {
						std::stringstream l_by(l_text.substr(3));
						std::string l_list;
						l_by >> l_list;
						l_check.m_by = columns(l_list);
						std::getline(l_by, l_text);
						l_text = trim(l_text);
					}
					ExpressionParser l_parser(l_text);
					l_check.m_each = std::make_shared<Expression>(l_parser.parse());
					l_check.m_uses_previous = l_parser.uses_previous();
					l_check.m_value = l_text;
					l_rules.m_checks.push_back(l_check);
					continue;
				}
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
				return "at #" + std::to_string(i) + ": C# " + join(p_expected[i]) + ", C++ " + join(p_actual[i]) + " (" +
					   std::to_string(p_expected.size()) + " tuples in C#, " + std::to_string(p_actual.size()) + " in C++)";
			}
		}
		if (p_expected.size() != p_actual.size()) {
			return std::to_string(p_expected.size()) + " tuples in C#, " + std::to_string(p_actual.size()) + " in C++; first extra: " +
				   join(p_expected.size() > l_count ? p_expected[l_count] : p_actual[l_count]);
		}
		return {};
	}

	// The first tuples of a list, for a report
	std::string first_tuples(const std::vector<Tuple>& p_tuples) {
		std::string l_text;
		for (std::size_t i = 0; i < std::min<std::size_t>(p_tuples.size(), 3); i++) {
			l_text += (i == 0 ? "" : " ") + join(p_tuples[i]);
		}
		return p_tuples.size() > 3 ? l_text + " ..." : l_text;
	}

	// Exact multisets (sets without duplicates): the tuples left on each side once the equal ones are paired, then the
	// pairs made by the alias of the rule; p_note tells how many tuples the alias paired
	std::string compare_multisets(std::vector<Tuple> p_expected, std::vector<Tuple> p_actual, const Same& p_same, std::string& p_note) {
		if (p_same.m_mode == "set") {
			for (auto* l_tuples: {&p_expected, &p_actual}) {
				std::sort(l_tuples->begin(), l_tuples->end());
				l_tuples->erase(std::unique(l_tuples->begin(), l_tuples->end()), l_tuples->end());
			}
		}
		std::map<Tuple, long> l_balance;
		for (const auto& l_tuple: p_expected) {
			++l_balance[l_tuple];
		}
		for (const auto& l_tuple: p_actual) {
			--l_balance[l_tuple];
		}
		std::vector<Tuple> l_only_expected;
		std::vector<Tuple> l_only_actual;
		for (const auto& [l_tuple, l_count]: l_balance) {
			for (long i = 0; i < l_count; i++) {
				l_only_expected.push_back(l_tuple);
			}
			for (long i = 0; i < -l_count; i++) {
				l_only_actual.push_back(l_tuple);
			}
		}
		if (p_same.m_alias_column >= 0) {
			const auto l_column = static_cast<std::size_t>(p_same.m_alias_column);
			std::size_t l_paired = 0;
			std::vector<Tuple> l_left_actual;
			for (const auto& l_tuple: l_only_actual) {
				auto l_match = l_only_expected.end();
				if (l_column < l_tuple.size() && std::regex_match(l_tuple[l_column], p_same.m_alias)) {
					l_match = std::find_if(l_only_expected.begin(), l_only_expected.end(), [&](const Tuple& p_expected_tuple) {
						if (p_expected_tuple.size() != l_tuple.size()) {
							return false;
						}
						for (std::size_t i = 0; i < l_tuple.size(); i++) {
							if (i != l_column && p_expected_tuple[i] != l_tuple[i]) {
								return false;
							}
						}
						return true;
					});
				}
				if (l_match != l_only_expected.end()) {
					l_only_expected.erase(l_match);
					++l_paired;
				} else {
					l_left_actual.push_back(l_tuple);
				}
			}
			l_only_actual = l_left_actual;
			if (l_paired > 0) {
				p_note = std::to_string(l_paired) + " paired by the alias of column " + std::to_string(l_column);
			}
		}
		if (l_only_expected.empty() && l_only_actual.empty()) {
			return {};
		}
		return std::to_string(l_only_expected.size()) + " tuples only in C#" + (l_only_expected.empty() ? "" : " " + first_tuples(l_only_expected)) +
			   ", " + std::to_string(l_only_actual.size()) + " only in C++" + (l_only_actual.empty() ? "" : " " + first_tuples(l_only_actual));
	}

	Tuple key(const Tuple& p_tuple, const std::vector<std::size_t>& p_columns) {
		Tuple l_key;
		for (const auto l_column: p_columns) {
			l_key.push_back(l_column < p_tuple.size() ? p_tuple[l_column] : std::string());
		}
		return l_key;
	}

	double aggregate(const std::vector<Tuple>& p_tuples, const std::string& p_aggregate);

	std::string compare(const Same& p_same, std::vector<Tuple> p_expected, std::vector<Tuple> p_actual, std::string& p_note) {
		if (p_same.m_first > 0) {
			// Only the first tuples of each output, in their order
			p_expected.resize(std::min(p_expected.size(), p_same.m_first));
			p_actual.resize(std::min(p_actual.size(), p_same.m_first));
		}
		if (p_same.m_mode == "count") {
			return p_expected.size() == p_actual.size() ? std::string() :
				   std::to_string(p_expected.size()) + " in C#, " + std::to_string(p_actual.size()) + " in C++";
		}
		if (p_same.m_mode == "aggregate") {
			const double l_expected = aggregate(p_expected, p_same.m_aggregate);
			const double l_actual = aggregate(p_actual, p_same.m_aggregate);
			std::ostringstream l_text;
			l_text << p_same.m_aggregate << " " << l_expected << " in C#, " << l_actual << " in C++";
			if (std::fabs(l_expected - l_actual) <= p_same.m_tolerance) {
				p_note = l_text.str();
				return {};
			}
			return l_text.str();
		}
		if (p_same.m_mode == "bag" || p_same.m_mode == "set") {
			if (p_same.m_numeric_column < 0) {
				return compare_multisets(p_expected, p_actual, p_same, p_note);
			}
			// With a numeric column, the sorted lists are compared tuple by tuple within the tolerance
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
		if (p_operator == "==") {
			return p_value == l_reference;
		}
		if (p_operator == "!=") {
			return p_value != l_reference;
		}
		if (p_operator == "<") {
			return p_value < l_reference;
		}
		if (p_operator == "<=") {
			return p_value <= l_reference;
		}
		if (p_operator == ">") {
			return p_value > l_reference;
		}
		if (p_operator == ">=") {
			return p_value >= l_reference;
		}
		throw UsageError("unknown operator " + p_operator);
	}

	// Failures of "check NAME each" on one output: the first failing tuple of each group
	std::vector<std::string> verify_each(const Check& p_check, const Facts& p_facts) {
		std::vector<std::string> l_failures;
		std::map<Tuple, std::vector<Tuple>> l_groups;
		for (const auto& l_tuple: p_facts.at(p_check.m_name)) {
			l_groups[key(l_tuple, p_check.m_by)].push_back(l_tuple);
		}
		for (const auto& [l_key, l_tuples]: l_groups) {
			const Tuple* l_previous = nullptr;
			for (const auto& l_tuple: l_tuples) {
				if (!p_check.m_uses_previous || l_previous != nullptr) {
					try {
						if (evaluate(*p_check.m_each, l_tuple, l_previous) == 0) {
							l_failures.push_back("fails on " + join(l_tuple) + (l_previous != nullptr ? " after " + join(*l_previous) : std::string()));
							break;
						}
					} catch (const std::runtime_error& p_error) {
						l_failures.push_back(p_error.what());
						break;
					}
				}
				l_previous = &l_tuple;
			}
		}
		return l_failures;
	}

	// Failures of one invariant on one output
	std::vector<std::string> verify(const Check& p_check, const Facts& p_facts) {
		if (p_check.m_each) {
			return verify_each(p_check, p_facts);
		}
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
			std::string l_note;
			const std::string l_difference = compare(l_same, l_oracle.at(l_same.m_name), l_actual.at(l_same.m_name), l_note);
			l_report(l_difference.empty(), "same " + l_same.m_name + " " + l_same.m_mode + " (" + std::to_string(l_oracle.at(l_same.m_name).size()) + " tuples)",
					 l_difference.empty() ? l_note : l_difference);
		}
		for (const auto& l_expect: p_rules.m_expects) {
			if (!l_oracle.contains(l_expect.m_name)) {
				throw UsageError("expect on unknown fact " + l_expect.m_name);
			}
			for (const auto& [l_side, l_facts]: {std::pair<std::string, const Facts*>{"C#", &l_oracle}, {"C++", &l_actual}}) {
				const auto& l_tuples = l_facts->at(l_expect.m_name);
				const bool l_found = std::find(l_tuples.begin(), l_tuples.end(), l_expect.m_values) != l_tuples.end();
				l_report(l_found, "expect " + l_expect.m_name + " " + join(l_expect.m_values) + " on " + l_side, std::string());
			}
		}
		for (const auto& l_check: p_rules.m_checks) {
			if (!l_oracle.contains(l_check.m_name)) {
				throw UsageError("check on unknown fact " + l_check.m_name);
			}
			std::string l_by;
			for (const auto l_column: l_check.m_by) {
				l_by += (l_by.empty() ? " by " : ",") + std::to_string(l_column);
			}
			const std::string l_rule = l_check.m_each ? "check " + l_check.m_name + " each" + l_by + " " + l_check.m_value :
							   "check " + l_check.m_name + " " + l_check.m_aggregate + l_by + " " + l_check.m_operator + " " + l_check.m_value;
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

	// The output of the program, kept in the folder of CAM_ISO_SAVE when it is set
	void save_output(const std::string& p_oracle, const std::string& p_output) {
		const char* l_folder = std::getenv("CAM_ISO_SAVE");
		if (l_folder == nullptr || *l_folder == '\0') {
			return;
		}
		const char* l_seed = std::getenv("CAM_SEED");
		const std::string l_name = std::filesystem::path(p_oracle).stem().string() + (l_seed != nullptr && *l_seed != '\0' ? ".seed" + std::string(l_seed) : "") + ".out";
		std::filesystem::create_directories(l_folder);
		std::ofstream(std::filesystem::path(l_folder) / l_name, std::ios::binary) << p_output;
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
			save_output(l_arguments[1], l_output);
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
