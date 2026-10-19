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

#include "Identifier.h"

#include <cstdint>
#include <random>
#include <string_view>

namespace {

	constexpr std::string_view HEX_DIGITS = "0123456789abcdef";
	constexpr std::string_view BASE62 = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

	// One generator per thread, seeded once from the system entropy (no random_device per character)
	std::mt19937_64& generator() {
		thread_local std::mt19937_64 s_generator = [] {
			std::random_device l_device;
			std::seed_seq l_seed{l_device(), l_device(), l_device(), l_device(), l_device(), l_device(), l_device(), l_device()};
			return std::mt19937_64(l_seed);
		}();
		return s_generator;
	}

} // namespace

std::string cam::generate_uuid() {
	auto& l_generator = generator();
	// Version 4 in the high nibble of the third group, variant 10xx at the top of the fourth (RFC 9562)
	const std::uint64_t l_high = (l_generator() & ~0xF000ULL) | 0x4000ULL;
	const std::uint64_t l_low = (l_generator() & ~(3ULL << 62)) | (2ULL << 62);
	std::string l_uuid;
	l_uuid.reserve(36);
	for (int i = 0; i < 32; i++) {
		if (i == 8 || i == 12 || i == 16 || i == 20) {
			l_uuid.push_back('-');
		}
		const std::uint64_t l_word = i < 16 ? l_high : l_low;
		l_uuid.push_back(HEX_DIGITS[(l_word >> (60 - 4 * (i % 16))) & 0xF]);
	}
	return l_uuid;
}

std::string cam::generate_agent_id() {
	auto& l_generator = generator();
	std::string l_id;
	l_id.reserve(AGENT_ID_LENGTH);
	while (l_id.size() < AGENT_ID_LENGTH) {
		std::uint64_t l_bits = l_generator();
		for (int i = 0; i < 10 && l_id.size() < AGENT_ID_LENGTH; i++) {
			// Six bits give 0 to 63: 62 and 63 are dropped so that every character is equally likely
			const auto l_value = static_cast<std::size_t>(l_bits & 63);
			l_bits >>= 6;
			if (l_value < BASE62.size()) {
				l_id.push_back(BASE62[l_value]);
			}
		}
	}
	return l_id;
}
