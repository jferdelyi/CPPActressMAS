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

// Number formatting and parsing of .NET for the harness programs (tests/iso/DotNet.hpp): every case of
// tests/iso/vectors/format.txt, produced by tests/tools/csharp-ref (FormatVectors, .NET 10), is computed again.

#include <doctest/doctest.h>

#include <bit>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <DotNet.hpp>

namespace {

	std::vector<std::string> fields(const std::string& p_line) {
		std::vector<std::string> l_fields;
		std::stringstream l_stream(p_line);
		for (std::string l_field; std::getline(l_stream, l_field, '|');) {
			l_fields.push_back(l_field);
		}
		if (!p_line.empty() && p_line.back() == '|') {
			l_fields.emplace_back();
		}
		return l_fields;
	}

	// UTF-16 code units (hex, space separated) to UTF-8
	std::string utf8(const std::string& p_units) {
		std::string l_text;
		std::stringstream l_stream(p_units);
		for (std::string l_unit; l_stream >> l_unit;) {
			const auto l_code = static_cast<std::uint32_t>(std::stoul(l_unit, nullptr, 16));
			if (l_code < 0x80) {
				l_text += static_cast<char>(l_code);
			} else if (l_code < 0x800) {
				l_text += static_cast<char>(0xC0 | (l_code >> 6));
				l_text += static_cast<char>(0x80 | (l_code & 0x3F));
			} else {
				l_text += static_cast<char>(0xE0 | (l_code >> 12));
				l_text += static_cast<char>(0x80 | ((l_code >> 6) & 0x3F));
				l_text += static_cast<char>(0x80 | (l_code & 0x3F));
			}
		}
		return l_text;
	}

	std::vector<std::string> lines(const char p_kind) {
		std::ifstream l_file(std::string(CAM_VECTORS_DIR) + "/format.txt");
		REQUIRE(l_file.good());
		std::vector<std::string> l_lines;
		for (std::string l_line; std::getline(l_file, l_line);) {
			if (!l_line.empty() && l_line[0] == p_kind) {
				l_lines.push_back(l_line);
			}
		}
		return l_lines;
	}

} // namespace

TEST_SUITE("dotnet_format") {

	TEST_CASE("F2, F3, F4 and ToString of every double print as in .NET") {
		const auto l_lines = lines('D');
		REQUIRE(l_lines.size() > 40);
		for (const auto& l_line: l_lines) {
			CAPTURE(l_line);
			const auto l_fields = fields(l_line);
			REQUIRE(l_fields.size() == 6);
			const double l_value = std::bit_cast<double>(static_cast<std::uint64_t>(std::stoull(l_fields[1], nullptr, 16)));
			CHECK(cam::dotnet::format_fixed(l_value, 2) == l_fields[2]);
			CHECK(cam::dotnet::format_fixed(l_value, 3) == l_fields[3]);
			CHECK(cam::dotnet::format_fixed(l_value, 4) == l_fields[4]);
			CHECK(cam::dotnet::to_string(l_value) == l_fields[5]);
		}
	}

	TEST_CASE("F0, F1, F5 and F6 of every double print as in .NET") {
		const auto l_lines = lines('X');
		REQUIRE(l_lines.size() > 40);
		for (const auto& l_line: l_lines) {
			CAPTURE(l_line);
			const auto l_fields = fields(l_line);
			REQUIRE(l_fields.size() == 6);
			const double l_value = std::bit_cast<double>(static_cast<std::uint64_t>(std::stoull(l_fields[1], nullptr, 16)));
			CHECK(cam::dotnet::format_fixed(l_value, 0) == l_fields[2]);
			CHECK(cam::dotnet::format_fixed(l_value, 1) == l_fields[3]);
			CHECK(cam::dotnet::format_fixed(l_value, 5) == l_fields[4]);
			CHECK(cam::dotnet::format_fixed(l_value, 6) == l_fields[5]);
		}
	}

	TEST_CASE("the custom format F2) of every double prints as in .NET") {
		const auto l_lines = lines('C');
		REQUIRE(l_lines.size() > 40);
		for (const auto& l_line: l_lines) {
			CAPTURE(l_line);
			const auto l_fields = fields(l_line);
			REQUIRE(l_fields.size() == 3);
			const double l_value = std::bit_cast<double>(static_cast<std::uint64_t>(std::stoull(l_fields[1], nullptr, 16)));
			CHECK(cam::dotnet::format_custom_f2_paren(l_value) == l_fields[2]);
		}
	}

	TEST_CASE("to_double reads, or refuses, every text as Convert.ToDouble") {
		const auto l_lines = lines('R');
		REQUIRE(l_lines.size() == 35);
		for (const auto& l_line: l_lines) {
			CAPTURE(l_line);
			const auto l_fields = fields(l_line);
			REQUIRE(l_fields.size() == 3);
			const std::string l_text = utf8(l_fields[1]);
			if (l_fields[2] == "FormatException") {
				CHECK_THROWS_AS(cam::dotnet::to_double(l_text), std::invalid_argument);
			} else {
				const auto l_expected = static_cast<std::uint64_t>(std::stoull(l_fields[2], nullptr, 16));
				const double l_value = cam::dotnet::to_double(l_text);
				// Every NaN is the same NaN for .NET, whatever its bits
				if (std::isnan(l_value)) {
					CHECK(std::isnan(std::bit_cast<double>(l_expected)));
				} else {
					CHECK(std::bit_cast<std::uint64_t>(l_value) == l_expected);
				}
			}
		}
	}

	TEST_CASE("split cuts every text into the words of String.Split()") {
		const auto l_lines = lines('S');
		REQUIRE(l_lines.size() == 12);
		for (const auto& l_line: l_lines) {
			CAPTURE(l_line);
			const auto l_fields = fields(l_line);
			REQUIRE(l_fields.size() == 4);
			std::vector<std::string> l_expected(1);
			for (const char l_char: l_fields[3]) {
				if (l_char == ';') {
					l_expected.emplace_back();
				} else {
					l_expected.back() += l_char;
				}
			}
			for (auto& l_word: l_expected) {
				l_word = utf8(l_word);
			}
			REQUIRE(l_expected.size() == std::stoul(l_fields[2]));
			CHECK(cam::dotnet::split(utf8(l_fields[1])) == l_expected);
		}
	}

	TEST_CASE("trim removes the white spaces around every text as String.Trim()") {
		const auto l_lines = lines('T');
		REQUIRE(l_lines.size() == 12);
		for (const auto& l_line: l_lines) {
			CAPTURE(l_line);
			const auto l_fields = fields(l_line);
			REQUIRE(l_fields.size() == 3);
			CHECK(cam::dotnet::trim(utf8(l_fields[1])) == utf8(l_fields[2]));
		}
	}

	TEST_CASE("list_sort leaves equal keys where List<T>.Sort() of .NET leaves them") {
		const auto l_lines = lines('L');
		REQUIRE(l_lines.size() == 44);
		for (const auto& l_line: l_lines) {
			CAPTURE(l_line);
			const auto l_fields = fields(l_line);
			REQUIRE(l_fields.size() == 4);
			std::vector<std::pair<int, int>> l_items;
			std::stringstream l_keys(l_fields[2]);
			for (int l_key; l_keys >> l_key;) {
				l_items.emplace_back(l_key, static_cast<int>(l_items.size()));
			}
			REQUIRE(l_items.size() == std::stoul(l_fields[1]));
			cam::dotnet::list_sort(l_items, [](const std::pair<int, int>& p_a, const std::pair<int, int>& p_b) {
				return p_a.first < p_b.first ? -1 : (p_a.first > p_b.first ? 1 : 0);
			});
			std::string l_tags;
			for (const auto& [l_key, l_tag]: l_items) {
				l_tags += (l_tags.empty() ? "" : " ") + std::to_string(l_tag);
			}
			CHECK(l_tags == l_fields[3]);
		}
	}

	TEST_CASE("to_int32 reads, or refuses, every text as Convert.ToInt32") {
		const auto l_lines = lines('I');
		REQUIRE(l_lines.size() == 20);
		for (const auto& l_line: l_lines) {
			CAPTURE(l_line);
			const auto l_fields = fields(l_line);
			REQUIRE(l_fields.size() == 3);
			const std::string l_text = utf8(l_fields[1]);
			if (l_fields[2] == "FormatException") {
				CHECK_THROWS_AS(cam::dotnet::to_int32(l_text), std::invalid_argument);
			} else if (l_fields[2] == "OverflowException") {
				CHECK_THROWS_AS(cam::dotnet::to_int32(l_text), std::out_of_range);
			} else {
				CHECK(cam::dotnet::to_int32(l_text) == std::stoi(l_fields[2]));
			}
		}
	}
}
