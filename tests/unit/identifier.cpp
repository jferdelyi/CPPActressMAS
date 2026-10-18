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

// Identifiers (D58): version 4 UUIDs for environments, nodes and transfers; 15 base62 characters for agents,
// short enough for the internal buffer of std::string. Unique over many draws and from several threads.

#include <doctest/doctest.h>

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include <Environment.h>
#include <util/Identifier.h>
#include <transport/Types.h>
#include <tsl/ordered_map.h>

namespace {

	constexpr int DRAWS = 100000;

	bool is_base62(const std::string& p_text) {
		for (const char l_char: p_text) {
			const bool l_digit = l_char >= '0' && l_char <= '9';
			const bool l_upper = l_char >= 'A' && l_char <= 'Z';
			const bool l_lower = l_char >= 'a' && l_char <= 'z';
			if (!l_digit && !l_upper && !l_lower) {
				return false;
			}
		}
		return true;
	}

	// Does nothing: only its identifier matters
	struct Idle final : cam::Agent {
		explicit Idle(const std::string& p_name) : Agent(p_name) {}
	};

} // namespace

TEST_SUITE("unit") {

	TEST_CASE("a generated UUID is a canonical version 4 UUID of RFC 9562") {
		for (int i = 0; i < 1000; i++) {
			const std::string l_uuid = cam::generate_uuid();
			REQUIRE(cam::transport::is_canonical_uuid(l_uuid));
			CHECK(l_uuid[14] == '4');
			CHECK(std::string("89ab").find(l_uuid[19]) != std::string::npos);
		}
	}

	TEST_CASE("an agent identifier is made of 15 base62 characters and fits the internal buffer of std::string") {
		for (int i = 0; i < 1000; i++) {
			const std::string l_id = cam::generate_agent_id();
			REQUIRE(l_id.size() == cam::AGENT_ID_LENGTH);
			CHECK(is_base62(l_id));
		}
		// No allocation when copied: the copy keeps the characters inside the string object
		const std::string l_id = cam::generate_agent_id();
		const std::string l_copy = l_id;
		const auto l_object = reinterpret_cast<std::uintptr_t>(&l_copy);
		const auto l_characters = reinterpret_cast<std::uintptr_t>(l_copy.data());
		CHECK(l_characters >= l_object);
		CHECK(l_characters < l_object + sizeof(std::string));
	}

	TEST_CASE("generated identifiers are unique") {
		std::unordered_set<std::string> l_uuids;
		std::unordered_set<std::string> l_ids;
		for (int i = 0; i < DRAWS; i++) {
			l_uuids.insert(cam::generate_uuid());
			l_ids.insert(cam::generate_agent_id());
		}
		CHECK(l_uuids.size() == DRAWS);
		CHECK(l_ids.size() == DRAWS);
	}

	TEST_CASE("identifiers drawn from several threads are unique") {
		constexpr int THREADS = 4;
		std::mutex l_mutex;
		std::unordered_set<std::string> l_ids;
		std::vector<std::thread> l_threads;
		for (int t = 0; t < THREADS; t++) {
			l_threads.emplace_back([&l_mutex, &l_ids] {
				std::vector<std::string> l_drawn;
				for (int i = 0; i < DRAWS / THREADS; i++) {
					l_drawn.push_back(cam::generate_agent_id());
					l_drawn.push_back(cam::generate_uuid());
				}
				const std::lock_guard<std::mutex> l_lock(l_mutex);
				l_ids.insert(l_drawn.begin(), l_drawn.end());
			});
		}
		for (auto& l_thread: l_threads) {
			l_thread.join();
		}
		CHECK(l_ids.size() == 2 * DRAWS);
	}

	TEST_CASE("agents are found by identifier through their hash, which spreads over the low bits (P14)") {
		const cam::AgentIdHash l_hash;
		tsl::ordered_map<std::string, int, cam::AgentIdHash> l_agents;
		std::vector<std::string> l_ids;
		std::unordered_set<std::size_t> l_hashes;
		std::unordered_set<std::size_t> l_low_bits;
		for (int i = 0; i < DRAWS; i++) {
			l_ids.push_back(cam::generate_agent_id());
			l_agents.emplace(l_ids.back(), i);
			l_hashes.insert(l_hash(l_ids.back()));
			l_low_bits.insert(l_hash(l_ids.back()) & 0xFFFF);
		}
		bool l_found = true;
		for (int i = 0; i < DRAWS; i++) {
			const auto l_it = l_agents.find(l_ids[i]);
			l_found = l_found && l_it != l_agents.end() && l_it->second == i;
		}
		CHECK(l_found);
		CHECK(l_hashes.size() >= DRAWS - 2);
		// 100 000 random keys over 65 536 values cover about 51 500 of them; a weak mix of the low bits covers far fewer
		CHECK(l_low_bits.size() > 48000);

		// A key of another length is hashed like any string, and still found
		const std::string l_uuid = cam::generate_uuid();
		CHECK(l_hash(l_uuid) == std::hash<std::string>{}(l_uuid));
		l_agents.emplace(l_uuid, -1);
		CHECK(l_agents.at(l_uuid) == -1);
	}

	TEST_CASE("an agent gets a short identifier, its environment a UUID") {
		cam::Environment l_env(1, cam::EnvironmentMasMode::Sequential);
		const std::string l_id = l_env.add<Idle>("idle");
		CHECK(l_id.size() == cam::AGENT_ID_LENGTH);
		CHECK(is_base62(l_id));
		CHECK(cam::transport::is_canonical_uuid(l_env.get_id()));
	}
}
