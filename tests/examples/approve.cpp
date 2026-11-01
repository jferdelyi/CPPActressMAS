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

// Approval test runner.
//
// Usage: cam_approve [--sort] [--update] <expected file> <actual file> <command> [args...]
//
// Runs the command, captures its standard output, normalises it (UUIDs and agent identifiers become <uuid>, durations such as
// "245ms" or "245 ms" become <ms>, lines are sorted with --sort for programs whose line order depends on
// the scheduling), writes the normalised output to <actual file> and compares it with <expected file>.
// With --update the expected file is (re)written instead of compared. The expected files pin the
// current C++ behaviour; they are not an oracle of the C# reference.

#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
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

	std::string quote(const std::string& p_argument) {
		return "\"" + p_argument + "\"";
	}

	std::string run(const std::string& p_command, int& p_exit_code) {
		std::string l_output;
		FILE* l_pipe = CAM_POPEN(p_command.c_str(), "r");
		if (!l_pipe) {
			p_exit_code = -1;
			return l_output;
		}
		std::array<char, 4096> l_buffer{};
		while (const std::size_t l_read = std::fread(l_buffer.data(), 1, l_buffer.size(), l_pipe)) {
			l_output.append(l_buffer.data(), l_read);
		}
		p_exit_code = CAM_PCLOSE(l_pipe);
		return l_output;
	}

	std::string normalise(const std::string& p_text, const bool p_sort) {
		static const std::regex s_uuid("[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}");
		// Agent identifiers are 15 base62 characters; they read as identifiers too
		static const std::regex s_agent_id("\\b[0-9A-Za-z]{15}\\b");
		static const std::regex s_duration("[0-9]+ ?ms\\b");
		std::string l_text = std::regex_replace(p_text, s_uuid, "<uuid>");
		l_text = std::regex_replace(l_text, s_agent_id, "<uuid>");
		l_text = std::regex_replace(l_text, s_duration, "<ms>");

		std::vector<std::string> l_lines;
		std::istringstream l_stream(l_text);
		for (std::string l_line; std::getline(l_stream, l_line);) {
			if (!l_line.empty() && l_line.back() == '\r') {
				l_line.pop_back();
			}
			l_lines.push_back(l_line);
		}
		if (p_sort) {
			std::sort(l_lines.begin(), l_lines.end());
		}
		std::string l_result;
		for (const auto& l_line: l_lines) {
			l_result += l_line;
			l_result += '\n';
		}
		return l_result;
	}

	std::string read_file(const std::string& p_path, bool& p_exists) {
		std::ifstream l_file(p_path, std::ios::binary);
		p_exists = l_file.good();
		std::ostringstream l_content;
		l_content << l_file.rdbuf();
		return l_content.str();
	}

	void write_file(const std::string& p_path, const std::string& p_content) {
		std::ofstream l_file(p_path, std::ios::binary | std::ios::trunc);
		l_file << p_content;
	}

	void report_first_difference(const std::string& p_expected, const std::string& p_actual) {
		std::istringstream l_expected(p_expected);
		std::istringstream l_actual(p_actual);
		std::string l_expected_line;
		std::string l_actual_line;
		int l_number = 0;
		while (true) {
			const bool l_has_expected = static_cast<bool>(std::getline(l_expected, l_expected_line));
			const bool l_has_actual = static_cast<bool>(std::getline(l_actual, l_actual_line));
			++l_number;
			if (!l_has_expected && !l_has_actual) {
				return;
			}
			if (l_has_expected != l_has_actual || l_expected_line != l_actual_line) {
				std::cerr << "first difference at line " << l_number << "\n"
						  << "  expected: " << (l_has_expected ? l_expected_line : std::string("<end of file>")) << "\n"
						  << "  actual:   " << (l_has_actual ? l_actual_line : std::string("<end of file>")) << "\n";
				return;
			}
		}
	}

} // namespace

int main(int argc, char** argv) {
	bool l_sort = false;
	bool l_update = false;
	int l_index = 1;
	for (; l_index < argc; ++l_index) {
		const std::string l_argument = argv[l_index];
		if (l_argument == "--sort") {
			l_sort = true;
		} else if (l_argument == "--update") {
			l_update = true;
		} else {
			break;
		}
	}
	if (argc - l_index < 3) {
		std::cerr << "usage: cam_approve [--sort] [--update] <expected file> <actual file> <command> [args...]\n";
		return 2;
	}
	const std::string l_expected_path = argv[l_index++];
	const std::string l_actual_path = argv[l_index++];
	std::string l_command = quote(argv[l_index++]);
	for (; l_index < argc; ++l_index) {
		l_command += " " + quote(argv[l_index]);
	}

	int l_exit_code = 0;
	const std::string l_raw = run(l_command, l_exit_code);
	if (l_exit_code != 0) {
		std::cerr << "command failed (status " << l_exit_code << "): " << l_command << "\n" << l_raw;
		return 1;
	}
	const std::string l_actual = normalise(l_raw, l_sort);
	write_file(l_actual_path, l_actual);

	if (l_update) {
		write_file(l_expected_path, l_actual);
		std::cout << "expected output written to " << l_expected_path << "\n";
		return 0;
	}

	bool l_exists = false;
	const std::string l_expected = read_file(l_expected_path, l_exists);
	if (!l_exists) {
		std::cerr << "missing expected file " << l_expected_path << " (run with --update to create it)\n";
		return 1;
	}
	if (l_expected != l_actual) {
		std::cerr << "output differs from " << l_expected_path << "; actual output saved in " << l_actual_path << "\n";
		report_first_difference(l_expected, l_actual);
		return 1;
	}
	std::cout << "approved: " << l_expected_path << "\n";
	return 0;
}
