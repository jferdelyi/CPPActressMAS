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

// Text of a message as the C# reads it: Parse, Parse(out string) and Parse1P split the content at every
// white space as String.Split() does (Unicode spaces, empty words kept), and Format names the sender and the receiver.
// Agents record into a Recorder and never assert; the tests check after start().

#include <doctest/doctest.h>

#include <stdexcept>
#include <string>
#include <vector>

#include <Environment.h>

#include "Recorder.hpp"

namespace {

	using Mode = cam::EnvironmentMasMode;

	cam::test::Recorder g_recorder;

	// Records the parses and the format of every message it receives.
	struct Reader final : cam::Agent {
		explicit Reader(const std::string& p_name) : Agent(p_name) {}
		void action(const cam::MessagePointer& p_message) override {
			const auto [l_action, l_parameters] = p_message->parse();
			std::string l_list = l_action + ":";
			for (const auto& l_parameter: l_parameters) {
				l_list += "[" + l_parameter + "]";
			}
			g_recorder.record("list " + l_list);
			const auto [l_action_rest, l_rest] = p_message->parse_rest();
			g_recorder.record("rest " + l_action_rest + ":[" + l_rest + "]");
			try {
				const auto [l_action_one, l_parameter] = p_message->parse_one();
				g_recorder.record("one " + l_action_one + ":[" + l_parameter + "]");
			} catch (const std::out_of_range&) {
				g_recorder.record("one throws");
			}
			g_recorder.record("format " + format(p_message));
		}
	};

	// Sends one text to the reader at its setup.
	struct Writer final : cam::Agent {
		Writer(const std::string& p_name, std::string p_text) : Agent(p_name), m_text(std::move(p_text)) {}
		void setup() override { send_by_name("reader", m_text); }
	private:
		std::string m_text;
	};

	void exchange(const std::string& p_text) {
		g_recorder.clear();
		cam::Environment l_env(2, Mode::Sequential);
		l_env.add<Writer>("writer", p_text);
		l_env.add<Reader>("reader");
		l_env.start();
	}

} // namespace

TEST_SUITE("message_parse") {

	TEST_CASE("the words of a content split at single spaces") {
		exchange("move 3 4");
		CHECK(g_recorder.contains("list move:[3][4]"));
		CHECK(g_recorder.contains("rest move:[3 4]"));
		CHECK(g_recorder.contains("one move:[3]"));
		CHECK(g_recorder.contains("format [writer -> reader]: move 3 4"));
	}

	TEST_CASE("empty words between two spaces are kept, as String.Split()") {
		exchange("a  b ");
		CHECK(g_recorder.contains("list a:[][b][]"));
		CHECK(g_recorder.contains("rest a:[ b ]"));
		CHECK(g_recorder.contains("one a:[]"));
	}

	TEST_CASE("Unicode spaces and control spaces separate words, other characters do not") {
		// UTF-8 bytes written out, for compilers that do not read the source as UTF-8: NBSP, EM SPACE, an e acute, an arrow
		exchange("go\xC2\xA0north\tnow\xE2\x80\x83\xC3\xA9\xE2\x86\x92x");
		CHECK(g_recorder.contains("list go:[north][now][\xC3\xA9\xE2\x86\x92x]"));
	}

	TEST_CASE("a single word has no parameter, parse_one throws as Parse1P") {
		exchange("ping");
		CHECK(g_recorder.contains("list ping:"));
		CHECK(g_recorder.contains("rest ping:[]"));
		CHECK(g_recorder.contains("one throws"));
	}

	TEST_CASE("a message from outside the environment keeps the raw sender in its format") {
		g_recorder.clear();
		cam::Environment l_env(2, Mode::Sequential);
		const cam::AgentId l_reader = l_env.add<Reader>("reader");
		l_env.start();
		const std::string l_text = "end";
		l_env.send("env", l_reader, reinterpret_cast<const std::uint8_t*>(l_text.data()), l_text.size());
		l_env.continue_simulation(1);
		CHECK(g_recorder.contains("format [env -> reader]: end"));
	}
}
