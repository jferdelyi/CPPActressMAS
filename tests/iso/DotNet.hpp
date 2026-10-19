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
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>

/**
 * Number formatting and parsing of .NET, as the C# examples print and read numbers, so that a port prints the same text
 * as its original (D82). Invariant culture, checked against tests/iso/vectors/format.txt (FormatVectors, .NET 10).
 * Seeds of the C# oracles: a port draws from the same seeds as its oracle under the seed shim (D96).
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

	/**
	 * Lock of the console, shared by write and write_line.
	 * @return The lock
	 **/
	inline std::mutex& console_mutex() {
		static std::mutex s_mutex;
		return s_mutex;
	}

	/**
	 * As Console.Write: the text in one piece, never mixed with what another thread writes at the same time.
	 * @param p_text The text
	 **/
	inline void write(const std::string_view p_text) {
		const std::scoped_lock l_lock(console_mutex());
		std::cout << p_text << std::flush;
	}

	/**
	 * As Console.WriteLine: the text and the end of line in one piece, never mixed with what another thread writes at
	 * the same time, as the agents of a parallel turn print.
	 * @param p_text The text
	 **/
	inline void write_line(const std::string_view p_text) {
		const std::scoped_lock l_lock(console_mutex());
		std::cout << p_text << std::endl;
	}

	/**
	 * FNV-1a, 64 bits, of the bytes of a text (UTF-8 for a site name).
	 * @param p_text The text
	 * @return The hash
	 **/
	inline std::uint64_t fnv1a64(const std::string_view p_text) noexcept {
		std::uint64_t l_hash = 14695981039346656037ULL;
		for (const char l_char: p_text) {
			l_hash ^= static_cast<unsigned char>(l_char);
			l_hash *= 1099511628211ULL;
		}
		return l_hash;
	}

	/**
	 * One output of SplitMix64 from a state.
	 * @param p_state The state
	 * @return The mixed value
	 **/
	inline std::uint64_t splitmix64(const std::uint64_t p_state) noexcept {
		std::uint64_t l_z = p_state + 0x9E3779B97F4A7C15ULL;
		l_z = (l_z ^ (l_z >> 30)) * 0xBF58476D1CE4E5B9ULL;
		l_z = (l_z ^ (l_z >> 27)) * 0x94D049BB133111EBULL;
		return l_z ^ (l_z >> 31);
	}

	/**
	 * Seed that the seed shim of the oracles gives to the k-th System.Random created at a call site of the C#
	 * (tests/tools/csharp-ref/README.md): the low 31 bits of SplitMix64(SplitMix64(base ^ FNV-1a("File.cs:line")) + k).
	 * @param p_site The call site, file name and line ("EnvironmentMas.cs:61")
	 * @param p_instance The rank k of the instance at this site, from 0
	 * @param p_base The base, CAM_SEED of the oracle
	 * @return The seed
	 **/
	inline std::int32_t site_seed(const std::string_view p_site, const std::uint64_t p_instance, const std::uint64_t p_base) noexcept {
		return static_cast<std::int32_t>(splitmix64(splitmix64(p_base ^ fnv1a64(p_site)) + p_instance) & 0x7FFFFFFFULL);
	}

	/**
	 * The base of the seeds: CAM_SEED of the process environment, read once, as the seed shim reads it (an unsigned
	 * integer, white space and a leading '+' allowed), 0 when absent or not a number.
	 * @return The base
	 **/
	inline std::uint64_t cam_seed() {
		static const std::uint64_t s_seed = [] {
			const char* l_text = std::getenv("CAM_SEED");
			if (l_text == nullptr) {
				return std::uint64_t{0};
			}
			std::string_view l_view(l_text);
			const auto l_white = [](const char p_char) { return p_char == ' ' || (p_char >= '\t' && p_char <= '\r'); };
			while (!l_view.empty() && l_white(l_view.front())) {
				l_view.remove_prefix(1);
			}
			while (!l_view.empty() && l_white(l_view.back())) {
				l_view.remove_suffix(1);
			}
			if (!l_view.empty() && l_view.front() == '+') {
				l_view.remove_prefix(1);
			}
			std::uint64_t l_value = 0;
			const auto [l_end, l_error] = std::from_chars(l_view.data(), l_view.data() + l_view.size(), l_value);
			return l_error == std::errc() && l_end == l_view.data() + l_view.size() && !l_view.empty() ? l_value : 0;
		}();
		return s_seed;
	}

	/**
	 * Seed of the next System.Random created at a call site of the C#, as its oracle drew it: each site counts its
	 * instances in the process. With CAM_SEED set, the seed is reported on the error output as the shim does
	 * ("#camrng <site> <k> <seed>"), to compare with the .err file of the oracle.
	 * @param p_site The call site, file name and line ("Program.cs:90")
	 * @return The seed, for a cam::DotnetRandom
	 **/
	inline std::int32_t next_site_seed(const std::string_view p_site) {
		static std::mutex s_mutex;
		static std::map<std::string, std::uint64_t, std::less<>> s_counts;
		std::uint64_t l_instance;
		{
			const std::scoped_lock l_lock(s_mutex);
			l_instance = s_counts[std::string(p_site)]++;
		}
		const std::int32_t l_seed = site_seed(p_site, l_instance, cam_seed());
		if (std::getenv("CAM_SEED") != nullptr) {
			std::cerr << "#camrng " << p_site << " " << l_instance << " " << l_seed << std::endl;
		}
		return l_seed;
	}

} // namespace cam::dotnet
