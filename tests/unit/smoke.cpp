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

// Smoke tests of the public API on paths without a known defect. They also show the test pattern:
// agents record what happens in a thread-safe Recorder and never assert; the test checks the entries
// after start() returns.

#include <doctest/doctest.h>

#include <Environment.h>

#include "Recorder.hpp"

namespace {

	using Mode = cam::EnvironmentMasMode;

	cam::test::Recorder g_recorder;

	const std::uint8_t* raw(const std::string& p_text) {
		return reinterpret_cast<const std::uint8_t*>(p_text.data());
	}

	// Records every message it receives as "<name>:<content>".
	struct Receiver final : cam::Agent {
		explicit Receiver(const std::string& p_name) : Agent(p_name) {}
		void action(const cam::MessagePointer& p_message) override {
			g_recorder.record(get_name() + ":" + p_message->to_string());
		}
	};

	// Sends one message at setup, by id, to a given agent.
	struct Sender final : cam::Agent {
		Sender(const std::string& p_name, std::string p_target_id) : Agent(p_name), m_target_id(std::move(p_target_id)) {}
		void setup() override { send(m_target_id, json("hello")); }
	private:
		std::string m_target_id;
	};

	// Sends one message at setup through the addressing mode given by its name.
	struct Addresser final : cam::Agent {
		Addresser(const std::string& p_name, std::string p_mode, std::string p_target, const bool p_first_only) :
				Agent(p_name), m_mode(std::move(p_mode)), m_target(std::move(p_target)), m_first_only(p_first_only) {}
		void setup() override {
			if (m_mode == "name") {
				send_by_name(m_target, json("by name"), m_first_only);
			} else if (m_mode == "fragment") {
				send_by_fragment_name(m_target, json("by fragment"), m_first_only);
			} else if (m_mode == "broadcast") {
				broadcast(json("to all"));
			}
		}
	private:
		std::string m_mode;
		std::string m_target;
		bool m_first_only;
	};

	// Stops itself at its first action (turn 1).
	struct Quitter final : cam::Agent {
		explicit Quitter(const std::string& p_name) : Agent(p_name) {}
		void default_action() override {
			g_recorder.record(get_name() + ":quit");
			stop();
		}
	};

	// Environment that records its turn and simulation hooks.
	struct HookedEnvironment final : cam::Environment {
		using cam::Environment::Environment;
		void turn_finished(const int p_turn) override { g_recorder.record("turn:" + std::to_string(p_turn)); }
		void simulation_finished() override { g_recorder.record("finished"); }
	};

} // namespace

TEST_SUITE("unit") {

	TEST_CASE("a message sent by id is delivered at the next turn, hooks run once per turn and once at the end") {
		g_recorder.clear();
		HookedEnvironment l_env(3, Mode::Sequential);
		const std::string l_receiver = l_env.add<Receiver>("receiver");
		l_env.add<Sender>("sender", l_receiver);
		l_env.start();

		CHECK(g_recorder.count_of("receiver:\"hello\"") == 1);
		CHECK(g_recorder.count_of("turn:0") == 1);
		CHECK(g_recorder.count_of("turn:1") == 1);
		CHECK(g_recorder.count_of("turn:2") == 1);
		CHECK(g_recorder.count_of("finished") == 1);
		CHECK(l_env.agents_count() == 2);
	}

	TEST_CASE("send_by_name reaches every agent of that name, or the first one only") {
		g_recorder.clear();
		cam::Environment l_env(2, Mode::Sequential);
		l_env.add<Receiver>("worker");
		l_env.add<Receiver>("worker");
		l_env.add<Receiver>("other");
		l_env.add<Addresser>("all", "name", "worker", false);
		l_env.start();
		CHECK(g_recorder.count_of("worker:\"by name\"") == 2);
		CHECK(g_recorder.count_of("other:\"by name\"") == 0);

		g_recorder.clear();
		cam::Environment l_env_first(2, Mode::Sequential);
		l_env_first.add<Receiver>("worker");
		l_env_first.add<Receiver>("worker");
		l_env_first.add<Addresser>("one", "name", "worker", true);
		l_env_first.start();
		CHECK(g_recorder.count_of("worker:\"by name\"") == 1);
	}

	TEST_CASE("send_by_fragment_name reaches the agents whose name contains the fragment") {
		g_recorder.clear();
		cam::Environment l_env(2, Mode::Sequential);
		l_env.add<Receiver>("worker-1");
		l_env.add<Receiver>("worker-2");
		l_env.add<Receiver>("manager");
		l_env.add<Addresser>("boss", "fragment", "worker", false);
		l_env.start();
		CHECK(g_recorder.count_of("worker-1:\"by fragment\"") == 1);
		CHECK(g_recorder.count_of("worker-2:\"by fragment\"") == 1);
		CHECK(g_recorder.count_of("manager:\"by fragment\"") == 0);
	}

	TEST_CASE("broadcast reaches every other agent but not the sender") {
		g_recorder.clear();
		cam::Environment l_env(2, Mode::Sequential);
		l_env.add<Receiver>("r1");
		l_env.add<Receiver>("r2");
		l_env.add<Addresser>("speaker", "broadcast", "", false);
		l_env.start();
		CHECK(g_recorder.count_of("r1:\"to all\"") == 1);
		CHECK(g_recorder.count_of("r2:\"to all\"") == 1);
		CHECK(g_recorder.count() == 2);
	}

	TEST_CASE("lookups by name, fragment and id after the first turn") {
		cam::Environment l_env(1, Mode::Sequential);
		const std::string l_a1 = l_env.add<Receiver>("agent-1");
		const std::string l_a2 = l_env.add<Receiver>("agent-2");
		l_env.add<Receiver>("monitor");
		l_env.start();

		CHECK(l_env.agents_count() == 3);
		CHECK(l_env.get_agents_by_name("agent-1") == std::vector<std::string>{l_a1});
		CHECK(l_env.get_first_agent_by_name("agent-2") == l_a2);
		CHECK_FALSE(l_env.get_first_agent_by_name("nobody").has_value());
		CHECK(l_env.get_filtered_agents("agent-").size() == 2);
		CHECK(l_env.get_filtered_agents("agent-", true).size() == 1);
		CHECK(l_env.get_agent_name(l_a1) == "agent-1");
		CHECK_FALSE(l_env.get_agent_name("unknown").has_value());
	}

	TEST_CASE("an agent that stops before the last turn is removed and the simulation ends when empty") {
		g_recorder.clear();
		HookedEnvironment l_env(10, Mode::Sequential);
		l_env.add<Quitter>("q");
		l_env.start();  // turn 0: setup; turn 1: quit; removed at the end of turn 1; no agent left: stop
		CHECK(g_recorder.count_of("q:quit") == 1);
		CHECK(g_recorder.count_of("turn:1") == 1);
		CHECK(g_recorder.count_of("turn:2") == 0);
		CHECK(g_recorder.count_of("finished") == 1);
		CHECK(l_env.agents_count() == 0);
	}

	TEST_CASE("an empty environment runs one empty turn and finishes once, in every mode (C1 fixed in 1A)") {
		for (const auto l_mode: {Mode::Sequential, Mode::SequentialRandom, Mode::Parallel}) {
			g_recorder.clear();
			HookedEnvironment l_env(3, l_mode);
			l_env.start();
			CHECK(g_recorder.count_of("turn:0") == 1);
			CHECK(g_recorder.count_of("turn:1") == 0);
			CHECK(g_recorder.count_of("finished") == 1);
			CHECK(l_env.agents_count() == 0);
		}
	}

	TEST_CASE("a removed agent is gone from the name index, send_by_name delivers nothing (C6 fixed in 1A)") {
		g_recorder.clear();
		cam::Environment l_env(2, Mode::Sequential);
		const std::string l_id = l_env.add<Receiver>("target");
		l_env.start();
		REQUIRE(l_env.agents_count() == 1);
		l_env.remove(l_id);
		CHECK(l_env.agents_count() == 0);
		CHECK(l_env.get_agents_by_name("target").empty());
		const std::string l_text = "x";
		l_env.send_by_name("ext", "target", raw(l_text), l_text.size(), false, false);
		l_env.continue_simulation(1);
		CHECK(g_recorder.count() == 0);
	}

	TEST_CASE("messages sent by id in Parallel mode are all delivered") {
		g_recorder.clear();
		cam::Environment l_env(3, Mode::Parallel);
		const std::string l_receiver = l_env.add<Receiver>("receiver");
		for (int i = 0; i < 8; i++) {
			l_env.add<Sender>("sender-" + std::to_string(i), l_receiver);
		}
		l_env.start();
		CHECK(g_recorder.count_of("receiver:\"hello\"") == 8);
	}

	TEST_CASE("message contents round-trip through the binary formats") {
		const json l_content = {{"key", "value"}, {"n", 3}};
		const cam::Message l_message("sender", "receiver", l_content);
		CHECK(l_message.get_binary_format() == cam::MessageBinaryFormat::MessagePack);
		CHECK(l_message.content() == l_content);
		CHECK(l_message.get_sender() == "sender");
		CHECK(l_message.get_receiver() == "receiver");
		CHECK(l_message.format() == "[sender -> receiver]: " + l_content.dump());

		const std::string l_text = "plain text";
		const cam::Message l_raw("s", "r", raw(l_text), l_text.size());
		CHECK(l_raw.get_binary_format() == cam::MessageBinaryFormat::RAW);
		CHECK(l_raw.to_string() == l_text);

		for (const auto l_format: {cam::MessageBinaryFormat::CBOR, cam::MessageBinaryFormat::BSON,
								   cam::MessageBinaryFormat::UBJSON, cam::MessageBinaryFormat::BJData}) {
			CHECK(cam::Message::to_json(cam::Message::to_binary(l_content, l_format), l_format) == l_content);
		}
	}
}
