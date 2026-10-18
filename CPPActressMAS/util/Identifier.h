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

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>

/**
 * CPPActressMAS: identifiers of environments, nodes, transfers and agents.
 **/
namespace cam {

	/**
	 * Length of an agent identifier: short enough to stay in the internal buffer of std::string with every
	 * standard library (15 characters at least), so that copying it never allocates.
	 **/
	constexpr std::size_t AGENT_ID_LENGTH = 15;

	/**
	 * Generate a random UUID of version 4 (RFC 9562): 36 lowercase hexadecimal digits and dashes. Used for
	 * environments, nodes and transfers. Thread-safe: one generator per thread, seeded once.
	 * @return The UUID
	 **/
	[[nodiscard]] std::string generate_uuid();

	/**
	 * Generate the identifier of an agent: AGENT_ID_LENGTH base62 characters, about 89 random bits.
	 * Thread-safe: one generator per thread, seeded once.
	 * @return The identifier
	 **/
	[[nodiscard]] std::string generate_agent_id();

	/**
	 * Equality of the agent identifiers (P14): two identifiers of AGENT_ID_LENGTH characters are compared as two
	 * overlapping 8-byte words; any other keys go to the string comparison.
	 **/
	struct AgentIdEqual {
		/**
		 * Compare two keys.
		 * @param p_left A key, an agent identifier or any string
		 * @param p_right Another key
		 * @return True if both keys are equal
		 **/
		[[nodiscard]] bool operator()(const std::string& p_left, const std::string& p_right) const noexcept {
			if (p_left.size() != AGENT_ID_LENGTH || p_right.size() != AGENT_ID_LENGTH) {
				return p_left == p_right;
			}
			std::uint64_t l_left[2];
			std::uint64_t l_right[2];
			std::memcpy(&l_left[0], p_left.data(), sizeof(std::uint64_t));
			std::memcpy(&l_left[1], p_left.data() + AGENT_ID_LENGTH - sizeof(std::uint64_t), sizeof(std::uint64_t));
			std::memcpy(&l_right[0], p_right.data(), sizeof(std::uint64_t));
			std::memcpy(&l_right[1], p_right.data() + AGENT_ID_LENGTH - sizeof(std::uint64_t), sizeof(std::uint64_t));
			return l_left[0] == l_right[0] && l_left[1] == l_right[1];
		}
	};

	/**
	 * Hash of the agent identifiers (P14). An identifier of AGENT_ID_LENGTH characters is read as two overlapping
	 * 8-byte words, mixed by multiplications and shifts so that every bit reaches the low bits the buckets use; any
	 * other key goes to std::hash.
	 **/
	struct AgentIdHash {
		/**
		 * Hash a key.
		 * @param p_id The key, an agent identifier or any string
		 * @return The hash
		 **/
		[[nodiscard]] std::size_t operator()(const std::string& p_id) const noexcept {
			if (p_id.size() != AGENT_ID_LENGTH) {
				return std::hash<std::string>{}(p_id);
			}
			std::uint64_t l_low;
			std::uint64_t l_high;
			std::memcpy(&l_low, p_id.data(), sizeof(l_low));
			std::memcpy(&l_high, p_id.data() + AGENT_ID_LENGTH - sizeof(l_high), sizeof(l_high));
			std::uint64_t l_hash = l_low ^ (l_high * 0x9E3779B97F4A7C15ULL);
			l_hash ^= l_hash >> 32;
			l_hash *= 0xD6E8FEB86659FD93ULL;
			l_hash ^= l_hash >> 32;
			return static_cast<std::size_t>(l_hash);
		}
	};

} // namespace cam
