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

// Identifier of an agent: 16 bytes, at most 15 characters, copied as bytes, compared and hashed as two words.

#include <doctest/doctest.h>

#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_set>

#include <nlohmann/json.hpp>

#include <AgentId.h>
#include <util/Identifier.h>

TEST_SUITE("unit") {

	TEST_CASE("an agent identifier is two machine words, copied as bytes") {
		static_assert(sizeof(cam::AgentId) == 16);
		static_assert(std::is_trivially_copyable_v<cam::AgentId>);
		const cam::AgentId l_empty;
		CHECK(l_empty.empty());
		CHECK(l_empty.size() == 0);
		CHECK(l_empty == cam::AgentId(""));
	}

	TEST_CASE("an agent identifier holds at most 15 characters") {
		const std::string l_fifteen(15, 'a');
		CHECK(cam::AgentId(l_fifteen).str() == l_fifteen);
		CHECK_THROWS_AS(cam::AgentId(std::string(16, 'a')), std::invalid_argument);
		CHECK(cam::AgentId::parse(l_fifteen).has_value());
		CHECK_FALSE(cam::AgentId::parse(std::string(16, 'a')).has_value());
		CHECK_FALSE(cam::AgentId::parse(cam::generate_uuid()).has_value());  // 36 characters
		const cam::AgentId l_literal = "ext";  // a literal is checked when compiling
		CHECK(l_literal.view() == "ext");
		CHECK(l_literal.size() == 3);
	}

	TEST_CASE("a generated identifier has 15 base62 characters and identifiers are unique") {
		std::unordered_set<cam::AgentId> l_ids;
		for (int i = 0; i < 10000; i++) {
			const cam::AgentId l_id = cam::AgentId::generate();
			CHECK(l_id.size() == cam::AgentId::CAPACITY);
			l_ids.insert(l_id);
		}
		CHECK(l_ids.size() == 10000);
	}

	TEST_CASE("agent identifiers compare and order like their characters") {
		CHECK(cam::AgentId("abc") == cam::AgentId("abc"));
		CHECK(cam::AgentId("abc") != cam::AgentId("abd"));
		CHECK(cam::AgentId("abc") < cam::AgentId("abd"));
		CHECK(cam::AgentId("ab") < cam::AgentId("abc"));  // a prefix comes first, as with strings
		CHECK(cam::AgentId("abc") == "abc");
		CHECK(cam::AgentId("abc") == std::string_view("abc"));
		CHECK_FALSE(cam::AgentId("abc") == std::string("abcd"));
		CHECK(std::hash<cam::AgentId>{}(cam::AgentId("abc")) == std::hash<cam::AgentId>{}(cam::AgentId("abc")));
	}

	TEST_CASE("an agent identifier is written and read as its characters") {
		const cam::AgentId l_id = cam::AgentId::generate();
		std::ostringstream l_stream;
		l_stream << l_id;
		CHECK(l_stream.str() == l_id.str());

		const nlohmann::json l_json = l_id;
		CHECK(l_json.get<std::string>() == l_id.str());
		CHECK(l_json.get<cam::AgentId>() == l_id);
		CHECK_THROWS_AS(nlohmann::json(std::string(16, 'a')).get<cam::AgentId>(), std::invalid_argument);
	}
}
