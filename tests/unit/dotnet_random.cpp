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

// Generator compatible with the seeded System.Random of .NET (D82): every series of tests/vectors/rng/rng-vectors.txt,
// produced by tools/csharp-ref (RngVectors, .NET 10), is drawn again and compared value by value.

#include <doctest/doctest.h>

#include <bit>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <functional>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <util/DotnetRandom.h>

namespace {

	// Every line "label: values" of the vectors, and the lines "  [i] value | 0xBITS" of NextDouble in order
	struct Vectors {
		std::map<std::string, std::string> m_series;
		std::vector<std::string> m_double_bits;
	};

	std::string trim(const std::string& p_text) {
		const auto l_first = p_text.find_first_not_of(' ');
		const auto l_last = p_text.find_last_not_of(' ');
		return l_first == std::string::npos ? std::string() : p_text.substr(l_first, l_last - l_first + 1);
	}

	Vectors read_vectors() {
		std::ifstream l_file(std::string(CAM_VECTORS_DIR) + "/rng/rng-vectors.txt");
		REQUIRE(l_file.good());
		Vectors l_vectors;
		for (std::string l_line; std::getline(l_file, l_line);) {
			if (l_line.empty() || l_line[0] == '#') {
				continue;
			}
			if (l_line.rfind("  [", 0) == 0) {
				l_vectors.m_double_bits.push_back(trim(l_line.substr(l_line.find('|') + 1)));
				continue;
			}
			const auto l_colon = l_line.rfind(": ");
			l_vectors.m_series[trim(l_line.substr(0, l_colon))] = trim(l_line.substr(l_colon + 1));
		}
		return l_vectors;
	}

	std::string draw(const std::function<std::int64_t()>& p_next, const int p_count) {
		std::string l_text;
		for (int i = 0; i < p_count; i++) {
			l_text += (i == 0 ? "" : " ") + std::to_string(p_next());
		}
		return l_text;
	}

	std::string hex_bits(const double p_value) {
		char l_text[32];
		std::snprintf(l_text, sizeof(l_text), "0x%016llX", static_cast<unsigned long long>(std::bit_cast<std::uint64_t>(p_value)));
		return l_text;
	}

} // namespace

TEST_SUITE("dotnet_random") {

	TEST_CASE("every series of the C# vectors is drawn again identically") {
		const Vectors l_vectors = read_vectors();
		const auto l_check = [&](const std::string& p_label, const std::string& p_drawn) {
			CAPTURE(p_label);
			REQUIRE(l_vectors.m_series.contains(p_label));
			CHECK(l_vectors.m_series.at(p_label) == p_drawn);
		};

		cam::DotnetRandom l_random(42);
		l_check("Random(42).Next()", draw([&] { return l_random.next(); }, 10));
		l_random = cam::DotnetRandom(42);
		l_check("Random(42).Next(100)", draw([&] { return l_random.next(100); }, 10));
		l_random = cam::DotnetRandom(1);
		l_check("Random(1).Next(5)", draw([&] { return l_random.next(5); }, 10));
		for (const std::int32_t l_seed: {0, 1, -1, 42, INT32_MAX, INT32_MIN}) {
			l_random = cam::DotnetRandom(l_seed);
			l_check("Random(" + std::to_string(l_seed) + ").Next() x5", draw([&] { return l_random.next(); }, 5));
		}
		l_random = cam::DotnetRandom(42);
		l_check("Random(42).Next(-10,10)", draw([&] { return l_random.next(-10, 10); }, 10));
		l_random = cam::DotnetRandom(42);
		l_check("Random(42).Next(int.MinValue,int.MaxValue) [large-range path]",
				draw([&] { return l_random.next(INT32_MIN, INT32_MAX); }, 5));
		l_random = cam::DotnetRandom(1);
		std::string l_permutation;
		for (const int l_n: {5, 4, 3, 2}) {
			l_permutation += (l_permutation.empty() ? "" : " ") + std::to_string(l_random.next(l_n));
		}
		l_check("Random(1).Next(n) n=5,4,3,2 (one RandomPermutation(5))", l_permutation);
	}

	TEST_CASE("next_double gives the same IEEE-754 bits as NextDouble") {
		const Vectors l_vectors = read_vectors();
		REQUIRE(l_vectors.m_double_bits.size() == 10);
		cam::DotnetRandom l_random(42);
		for (const auto& l_bits: l_vectors.m_double_bits) {
			CHECK(hex_bits(l_random.next_double()) == l_bits);
		}
	}

	TEST_CASE("invalid bounds throw, as ArgumentOutOfRangeException, and empty ranges still draw") {
		cam::DotnetRandom l_random(7);
		CHECK_THROWS_AS(l_random.next(-1), std::out_of_range);
		CHECK_THROWS_AS(l_random.next(5, 4), std::out_of_range);
		cam::DotnetRandom l_witness(7);
		CHECK(l_random.next(0) == 0);
		CHECK(l_random.next(3, 3) == 3);
		l_witness.next();
		l_witness.next();
		CHECK(l_random.next() == l_witness.next());  // both empty ranges consumed a draw
	}
}
