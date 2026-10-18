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

#pragma once

#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>

/**
 * Number formatting and parsing of .NET, as the C# examples print and read numbers, so that a port prints the same text
 * as its original (D82). Invariant culture, checked against tests/vectors/dotnet/format.txt (FormatVectors, .NET 10).
 **/
namespace cam::dotnet {

	/**
	 * As Double.ToString("F<digits>"): correctly rounded, a tie to the even digit, the sign of a negative value kept
	 * even when it rounds to zero ("-0.00"), NaN, Infinity and -Infinity named.
	 * @param p_value The value
	 * @param p_digits Number of digits after the point
	 * @return The text
	 **/
	inline std::string format_fixed(const double p_value, const int p_digits) {
		if (std::isnan(p_value)) {
			return "NaN";
		}
		if (std::isinf(p_value)) {
			return p_value < 0 ? "-Infinity" : "Infinity";
		}
		char l_text[512];  // 309 integer digits at most, a sign, a point and the digits
		const auto l_result = std::to_chars(std::begin(l_text), std::end(l_text), p_value, std::chars_format::fixed, p_digits);
		return std::string(l_text, l_result.ptr);
	}

	/**
	 * As Double.ToString(): the shortest text that reads back the same value, in scientific notation ("1E+21",
	 * "9.999E-05") when the point would fall more than 17 places after the first digit or more than 3 places before it.
	 * @param p_value The value
	 * @return The text
	 **/
	inline std::string to_string(const double p_value) {
		if (std::isnan(p_value)) {
			return "NaN";
		}
		if (std::isinf(p_value)) {
			return p_value < 0 ? "-Infinity" : "Infinity";
		}
		if (p_value == 0) {
			return std::signbit(p_value) ? "-0" : "0";
		}
		char l_text[64];
		const auto l_result = std::to_chars(std::begin(l_text), std::end(l_text), p_value, std::chars_format::scientific);
		const std::string_view l_scientific(l_text, static_cast<std::size_t>(l_result.ptr - l_text));
		const bool l_negative = l_scientific.front() == '-';
		const auto l_e = l_scientific.find('e');
		std::string l_digits;
		for (const char l_char: l_scientific.substr(l_negative ? 1 : 0, l_e - (l_negative ? 1 : 0))) {
			if (l_char != '.') {
				l_digits += l_char;
			}
		}
		const int l_exponent = std::atoi(std::string(l_scientific.substr(l_e + 1)).c_str());
		const int l_point = l_exponent + 1;  // place of the point after the first digit
		std::string l_out = l_negative ? "-" : "";
		if (l_point > 17 || l_point < -3) {
			l_out += l_digits.front();
			if (l_digits.size() > 1) {
				l_out += '.';
				l_out += l_digits.substr(1);
			}
			const std::string l_power = std::to_string(std::abs(l_exponent));
			l_out += l_exponent < 0 ? "E-" : "E+";
			l_out += (l_power.size() < 2 ? "0" : "") + l_power;
		} else if (l_point <= 0) {
			l_out += "0." + std::string(static_cast<std::size_t>(-l_point), '0') + l_digits;
		} else if (static_cast<std::size_t>(l_point) >= l_digits.size()) {
			l_out += l_digits + std::string(static_cast<std::size_t>(l_point) - l_digits.size(), '0');
		} else {
			l_out += l_digits.substr(0, static_cast<std::size_t>(l_point)) + "." + l_digits.substr(static_cast<std::size_t>(l_point));
		}
		return l_out;
	}

	/**
	 * As Convert.ToInt32(string): ASCII white space around (tab to carriage return, space), one sign right before the
	 * digits, ASCII digits only.
	 * @param p_text The text
	 * @return The value
	 * @throw std::invalid_argument Where .NET throws FormatException
	 * @throw std::out_of_range Where .NET throws OverflowException
	 **/
	inline std::int32_t to_int32(const std::string_view p_text) {
		const auto l_white = [](const char p_char) { return p_char == ' ' || (p_char >= '\t' && p_char <= '\r'); };
		std::size_t l_begin = 0;
		std::size_t l_end = p_text.size();
		while (l_begin < l_end && l_white(p_text[l_begin])) {
			l_begin++;
		}
		while (l_end > l_begin && l_white(p_text[l_end - 1])) {
			l_end--;
		}
		bool l_negative = false;
		if (l_begin < l_end && (p_text[l_begin] == '+' || p_text[l_begin] == '-')) {
			l_negative = p_text[l_begin] == '-';
			l_begin++;
		}
		if (l_begin == l_end) {
			throw std::invalid_argument("to_int32: no digit in \"" + std::string(p_text) + "\"");
		}
		// A misplaced character is a format error even after an overflow, as .NET reports it
		std::int64_t l_value = 0;
		bool l_overflow = false;
		for (std::size_t i = l_begin; i < l_end; i++) {
			if (p_text[i] < '0' || p_text[i] > '9') {
				throw std::invalid_argument("to_int32: not an integer: \"" + std::string(p_text) + "\"");
			}
			if (!l_overflow) {
				l_value = l_value * 10 + (p_text[i] - '0');
				l_overflow = l_value > 2147483648LL;
			}
		}
		if (l_overflow || l_value > (l_negative ? 2147483648LL : 2147483647LL)) {
			throw std::out_of_range("to_int32: out of the int range: \"" + std::string(p_text) + "\"");
		}
		return static_cast<std::int32_t>(l_negative ? -l_value : l_value);
	}

} // namespace cam::dotnet
