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

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <Environment.h>

/**
 * Number formatting and parsing of .NET, as the C# examples print and read numbers, so that a port prints the same text
 * as its original. Invariant culture, checked against tests/iso/vectors/format.txt (FormatVectors, .NET 10).
 * Seeds of the C# oracles: a port draws from the same seeds as its oracle under the seed shim.
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
	 * As Double.ToString("F2)"), the custom format a C# example writes for F2 ("{sum:F2)}"): no digit placeholder, so
	 * .NET prints the text "F2)" with the sign of the value only; NaN, Infinity and -Infinity named.
	 * @param p_value The value
	 * @return The text
	 **/
	inline std::string format_custom_f2_paren(const double p_value) {
		if (std::isnan(p_value)) {
			return "NaN";
		}
		if (std::isinf(p_value)) {
			return p_value > 0 ? "Infinity" : "-Infinity";
		}
		return std::signbit(p_value) ? "-F2)" : "F2)";
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
	 * As Char.IsWhiteSpace: the Unicode white spaces.
	 * @param p_code The code point
	 * @return True for a white space
	 **/
	inline bool is_white_space(const char32_t p_code) noexcept {
		return (p_code >= 0x09 && p_code <= 0x0D) || p_code == 0x20 || p_code == 0x85 || p_code == 0xA0 || p_code == 0x1680 ||
			   (p_code >= 0x2000 && p_code <= 0x200A) || p_code == 0x2028 || p_code == 0x2029 || p_code == 0x202F || p_code == 0x205F ||
			   p_code == 0x3000;
	}

	/**
	 * As String.Trim(): the UTF-8 text without the Unicode white spaces at both ends.
	 * @param p_text The text
	 * @return The trimmed text
	 **/
	inline std::string trim(const std::string_view p_text) {
		// Code points of the text with their byte ranges, decoded as in split
		std::size_t l_begin = p_text.size();
		std::size_t l_end = 0;
		std::size_t i = 0;
		while (i < p_text.size()) {
			const auto l_byte = static_cast<unsigned char>(p_text[i]);
			std::size_t l_length = l_byte < 0x80 ? 1 : (l_byte >> 5) == 0x6 ? 2 : (l_byte >> 4) == 0xE ? 3 : (l_byte >> 3) == 0x1E ? 4 : 1;
			if (i + l_length > p_text.size()) {
				l_length = 1;
			}
			char32_t l_code = l_length == 1 ? l_byte : l_byte & (0x7F >> l_length);
			for (std::size_t k = 1; k < l_length; k++) {
				l_code = (l_code << 6) | (static_cast<unsigned char>(p_text[i + k]) & 0x3F);
			}
			if (!is_white_space(l_code)) {
				l_begin = std::min(l_begin, i);
				l_end = i + l_length;
			}
			i += l_length;
		}
		return l_begin < l_end ? std::string(p_text.substr(l_begin, l_end - l_begin)) : std::string();
	}

	/**
	 * As String.Split() without argument: the UTF-8 text cut at every Unicode white space, empty words kept.
	 * @param p_text The text
	 * @return The words, one at least
	 **/
	inline std::vector<std::string> split(const std::string_view p_text) {
		std::vector<std::string> l_words(1);
		std::size_t i = 0;
		while (i < p_text.size()) {
			const auto l_byte = static_cast<unsigned char>(p_text[i]);
			std::size_t l_length = l_byte < 0x80 ? 1 : (l_byte >> 5) == 0x6 ? 2 : (l_byte >> 4) == 0xE ? 3 : (l_byte >> 3) == 0x1E ? 4 : 1;
			if (i + l_length > p_text.size()) {
				l_length = 1;
			}
			char32_t l_code = l_length == 1 ? l_byte : l_byte & (0x7F >> l_length);
			for (std::size_t k = 1; k < l_length; k++) {
				l_code = (l_code << 6) | (static_cast<unsigned char>(p_text[i + k]) & 0x3F);
			}
			if (is_white_space(l_code)) {
				l_words.emplace_back();
			} else {
				l_words.back().append(p_text.substr(i, l_length));
			}
			i += l_length;
		}
		return l_words;
	}

	/**
	 * As Convert.ToDouble(string), invariant culture: ASCII white space around, one sign, digits with ',' between the
	 * digits of the integer part, an optional '.' and fraction, an optional exponent; "NaN" and "Infinity" in any
	 * case (not the sign ∞). A value too large is an infinity and a value too small a zero, as since .NET Core 3.0.
	 * @param p_text The text
	 * @return The value
	 * @throw std::invalid_argument Where .NET throws FormatException
	 **/
	inline double to_double(const std::string_view p_text) {
		const auto l_white = [](const char p_char) { return p_char == ' ' || (p_char >= '\t' && p_char <= '\r'); };
		const auto l_digit = [](const char p_char) { return p_char >= '0' && p_char <= '9'; };
		const auto l_fail = [&p_text]() { return std::invalid_argument("to_double: not a number: \"" + std::string(p_text) + "\""); };
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
		const std::string_view l_rest = p_text.substr(l_begin, l_end - l_begin);
		std::string l_lower;
		for (const char l_char: l_rest) {
			l_lower += (l_char >= 'A' && l_char <= 'Z') ? static_cast<char>(l_char - 'A' + 'a') : l_char;
		}
		if (l_lower == "infinity") {
			return l_negative ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity();
		}
		if (l_lower == "nan") {
			return std::numeric_limits<double>::quiet_NaN();
		}
		// The number without its group separators, for from_chars
		std::string l_number;
		std::size_t i = 0;
		bool l_digits = false;
		while (i < l_rest.size() && (l_digit(l_rest[i]) || (l_rest[i] == ',' && l_digits))) {
			if (l_rest[i] != ',') {
				l_number += l_rest[i];
				l_digits = true;
			}
			i++;
		}
		if (i < l_rest.size() && l_rest[i] == '.') {
			l_number += l_rest[i++];
			while (i < l_rest.size() && l_digit(l_rest[i])) {
				l_number += l_rest[i++];
				l_digits = true;
			}
		}
		if (!l_digits) {
			throw l_fail();
		}
		if (i < l_rest.size() && (l_rest[i] == 'e' || l_rest[i] == 'E')) {
			l_number += l_rest[i++];
			if (i < l_rest.size() && (l_rest[i] == '+' || l_rest[i] == '-')) {
				l_number += l_rest[i++];
			}
			const std::size_t l_exponent = i;
			while (i < l_rest.size() && l_digit(l_rest[i])) {
				l_number += l_rest[i++];
			}
			if (i == l_exponent) {
				throw l_fail();
			}
		}
		if (i != l_rest.size()) {
			throw l_fail();
		}
		double l_value = 0;
		if (std::from_chars(l_number.data(), l_number.data() + l_number.size(), l_value).ec == std::errc::result_out_of_range) {
			// Too large or too small: strtod of the C locale gives the infinity or the zero
			l_value = std::strtod(l_number.c_str(), nullptr);
		}
		return l_negative ? -l_value : l_value;
	}

	namespace detail {

		template<typename T, typename Compare>
		void swap_if_greater(T* p_keys, const Compare& p_compare, const int p_i, const int p_j) {
			if (p_i != p_j && p_compare(p_keys[p_i], p_keys[p_j]) > 0) {
				std::swap(p_keys[p_i], p_keys[p_j]);
			}
		}

		template<typename T, typename Compare>
		void insertion_sort(T* p_keys, const int p_length, const Compare& p_compare) {
			for (int i = 0; i < p_length - 1; i++) {
				T l_t = std::move(p_keys[i + 1]);
				int j = i;
				while (j >= 0 && p_compare(l_t, p_keys[j]) < 0) {
					p_keys[j + 1] = std::move(p_keys[j]);
					j--;
				}
				p_keys[j + 1] = std::move(l_t);
			}
		}

		template<typename T, typename Compare>
		void down_heap(T* p_keys, int p_i, const int p_n, const Compare& p_compare) {
			T l_d = std::move(p_keys[p_i - 1]);
			while (p_i <= p_n >> 1) {
				int l_child = 2 * p_i;
				if (l_child < p_n && p_compare(p_keys[l_child - 1], p_keys[l_child]) < 0) {
					l_child++;
				}
				if (!(p_compare(l_d, p_keys[l_child - 1]) < 0)) {
					break;
				}
				p_keys[p_i - 1] = std::move(p_keys[l_child - 1]);
				p_i = l_child;
			}
			p_keys[p_i - 1] = std::move(l_d);
		}

		template<typename T, typename Compare>
		void heap_sort(T* p_keys, const int p_n, const Compare& p_compare) {
			for (int i = p_n >> 1; i >= 1; i--) {
				down_heap(p_keys, i, p_n, p_compare);
			}
			for (int i = p_n; i > 1; i--) {
				std::swap(p_keys[0], p_keys[i - 1]);
				down_heap(p_keys, 1, i - 1, p_compare);
			}
		}

		template<typename T, typename Compare>
		int pick_pivot_and_partition(T* p_keys, const int p_length, const Compare& p_compare) {
			const int l_hi = p_length - 1;
			const int l_middle = l_hi >> 1;
			swap_if_greater(p_keys, p_compare, 0, l_middle);
			swap_if_greater(p_keys, p_compare, 0, l_hi);
			swap_if_greater(p_keys, p_compare, l_middle, l_hi);
			T l_pivot = p_keys[l_middle];
			std::swap(p_keys[l_middle], p_keys[l_hi - 1]);
			int l_left = 0;
			int l_right = l_hi - 1;
			while (l_left < l_right) {
				while (p_compare(p_keys[++l_left], l_pivot) < 0) {
				}
				while (p_compare(l_pivot, p_keys[--l_right]) < 0) {
				}
				if (l_left >= l_right) {
					break;
				}
				std::swap(p_keys[l_left], p_keys[l_right]);
			}
			if (l_left != l_hi - 1) {
				std::swap(p_keys[l_left], p_keys[l_hi - 1]);
			}
			return l_left;
		}

		template<typename T, typename Compare>
		void intro_sort(T* p_keys, const int p_length, int p_depth_limit, const Compare& p_compare) {
			int l_partition_size = p_length;
			while (l_partition_size > 1) {
				if (l_partition_size <= 16) {
					if (l_partition_size == 2) {
						swap_if_greater(p_keys, p_compare, 0, 1);
						return;
					}
					if (l_partition_size == 3) {
						swap_if_greater(p_keys, p_compare, 0, 1);
						swap_if_greater(p_keys, p_compare, 0, 2);
						swap_if_greater(p_keys, p_compare, 1, 2);
						return;
					}
					insertion_sort(p_keys, l_partition_size, p_compare);
					return;
				}
				if (p_depth_limit == 0) {
					heap_sort(p_keys, l_partition_size, p_compare);
					return;
				}
				p_depth_limit--;
				const int l_p = pick_pivot_and_partition(p_keys, l_partition_size, p_compare);
				intro_sort(p_keys + l_p + 1, l_partition_size - (l_p + 1), p_depth_limit, p_compare);
				l_partition_size = l_p;
			}
		}

	} // namespace detail

	/**
	 * As List<T>.Sort() of .NET: the introspective sort of ArraySortHelper (insertion sort up to 16 elements, median of
	 * three partitions, heap sort past the depth limit), not stable: equal elements end where .NET leaves them.
	 * @param p_items The items, sorted in place
	 * @param p_compare As CompareTo: negative, zero or positive
	 **/
	template<typename T, typename Compare>
	void list_sort(std::vector<T>& p_items, const Compare& p_compare) {
		const auto l_length = static_cast<int>(p_items.size());
		if (l_length > 1) {
			int l_log2 = 0;
			while ((1u << (l_log2 + 1)) <= static_cast<unsigned>(l_length)) {
				l_log2++;
			}
			detail::intro_sort(p_items.data(), l_length, 2 * (l_log2 + 1), p_compare);
		}
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

	/**
	 * Generator of the environment of a port: DotNet, the System.Random of its oracle, unless CAM_RANDOM_ENGINE is
	 * "mt19937", the default generator of the library (the distribution tests run the ports with both).
	 * @return The generator
	 **/
	inline cam::RandomEngine random_engine() {
		const char* l_text = std::getenv("CAM_RANDOM_ENGINE");
		return l_text != nullptr && std::string_view(l_text) == "mt19937" ? cam::RandomEngine::Mt19937 : cam::RandomEngine::DotNet;
	}

	/**
	 * Mode of a port: the mode of its C# original, unless the arguments name the mode of a variant of its oracle
	 * ("sequential" in a fixed order, "sequential-random", "parallel").
	 * @param p_argc Number of arguments
	 * @param p_argv The arguments
	 * @param p_mode The mode of the C# original
	 * @return The mode
	 **/
	inline cam::EnvironmentMasMode scenario_mode(const int p_argc, char** p_argv, const cam::EnvironmentMasMode p_mode) {
		for (int i = 1; i < p_argc; i++) {
			const std::string_view l_argument(p_argv[i]);
			if (l_argument == "sequential") {
				return cam::EnvironmentMasMode::Sequential;
			}
			if (l_argument == "sequential-random") {
				return cam::EnvironmentMasMode::SequentialRandom;
			}
			if (l_argument == "parallel") {
				return cam::EnvironmentMasMode::Parallel;
			}
		}
		return p_mode;
	}

	/**
	 * Whether the arguments of a port name a scenario of its oracle ("confess" for a prisoner that always confesses...).
	 * @param p_argc Number of arguments
	 * @param p_argv The arguments
	 * @param p_name The scenario
	 * @return True if one argument is p_name
	 **/
	inline bool has_scenario(const int p_argc, char** p_argv, const std::string_view p_name) {
		for (int i = 1; i < p_argc; i++) {
			if (std::string_view(p_argv[i]) == p_name) {
				return true;
			}
		}
		return false;
	}

} // namespace cam::dotnet
