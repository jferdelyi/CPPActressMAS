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

// Pointer to a message with the counter inside the message: copies share, moves leave the source null, a kept
// message stays valid after the action, and messages dropped by other threads than their sender's are released once.
// Agents record into a Recorder and never assert; the tests check after start().

#include <doctest/doctest.h>

#include <string>
#include <vector>

#include <Environment.h>

#include "Recorder.hpp"

namespace {

	using Mode = cam::EnvironmentMasMode;

	cam::test::Recorder g_recorder;

	// Sends three texts by name at setup.
	struct Speaker final : cam::Agent {
		explicit Speaker(const std::string& p_name) : Agent(p_name) {}
		void setup() override {
			for (int i = 1; i <= 3; i++) {
				send_by_name("keeper", "text " + std::to_string(i));
			}
		}
	};

	// Keeps every message it receives, through a copy then a move, and reads them back at its next empty turn.
	struct Keeper final : cam::Agent {
		explicit Keeper(const std::string& p_name) : Agent(p_name) {}
		void action(const cam::MessagePointer& p_message) override {
			cam::MessagePointer l_copy = p_message;
			g_recorder.record(l_copy == p_message ? "copy shares" : "copy differs");
			cam::MessagePointer l_moved = std::move(l_copy);
			g_recorder.record(l_copy == nullptr && !l_copy ? "moved-from is null" : "moved-from is not null");
			m_kept.push_back(std::move(l_moved));
		}
		void default_action() override {
			for (const auto& l_message: m_kept) {
				g_recorder.record("kept " + l_message->as_text());
			}
			m_kept.clear();
		}
	private:
		std::vector<cam::MessagePointer> m_kept;
	};

	// Keeps its last messages and drops the older ones, on whatever thread runs it at that turn.
	struct Echo final : cam::Agent {
		explicit Echo(const std::string& p_name) : Agent(p_name) {}
		void setup() override { broadcast("ping"); }
		void action(const cam::MessagePointer& p_message) override {
			m_kept.push_back(p_message);  // dropped at the next action, on another thread maybe
			if (m_kept.size() > 4) {
				m_kept.erase(m_kept.begin());
			}
			g_recorder.record("echo");
		}
	private:
		std::vector<cam::MessagePointer> m_kept;
	};

} // namespace

TEST_SUITE("unit") {

	TEST_CASE("a null message pointer") {
		cam::MessagePointer l_null;
		CHECK_FALSE(l_null);
		CHECK(l_null == nullptr);
		CHECK(l_null.get() == nullptr);
		l_null.reset();
		cam::MessagePointer l_other = nullptr;
		l_null.swap(l_other);
		CHECK(l_null == l_other);
	}

	TEST_CASE("message pointers copied, moved and kept after the action stay valid") {
		g_recorder.clear();
		cam::Environment l_env(4, Mode::Sequential);
		l_env.add<Keeper>("keeper");
		l_env.add<Speaker>("speaker");
		l_env.start();
		CHECK(g_recorder.count_of("copy shares") == 3);
		CHECK(g_recorder.count_of("moved-from is null") == 3);
		for (int i = 1; i <= 3; i++) {
			CHECK(g_recorder.count_of("kept text " + std::to_string(i)) == 1);
		}
	}

	TEST_CASE("messages kept and dropped across the threads of a Parallel environment are released once") {
		g_recorder.clear();
		{
			cam::Environment l_env(20, Mode::Parallel);
			for (int i = 0; i < 8; i++) {
				l_env.add<Echo>("echo");
			}
			l_env.start();
		}  // every kept pointer is dropped with its agent: no leak under AddressSanitizer, no race under ThreadSanitizer
		CHECK(g_recorder.count_of("echo") == 8 * 7);
	}
}
