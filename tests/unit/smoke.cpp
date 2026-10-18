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

#include <cstdint>
#include <type_traits>

#include <Environment.h>
#include <transport/codec/ContentCodec.h>

#include "Recorder.hpp"

namespace {

	using Mode = cam::EnvironmentMasMode;

	cam::test::Recorder g_recorder;

	std::vector<std::uint8_t> raw_bytes(const std::string& p_text) {
		return {p_text.begin(), p_text.end()};
	}

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

	// Records every message it receives as "<name>:<kind>:<content>".
	struct KindReceiver final : cam::Agent {
		explicit KindReceiver(const std::string& p_name) : Agent(p_name) {}
		void action(const cam::MessagePointer& p_message) override {
			const std::string l_kind = p_message->kind() == cam::ContentKind::Text ? "text"
									 : p_message->kind() == cam::ContentKind::Json ? "json"
									 : p_message->kind() == cam::ContentKind::Bytes ? "bytes" : "none";
			g_recorder.record(get_name() + ":" + l_kind + ":" + p_message->to_string());
		}
	};

	// Sends a text, bytes and a JSON value at setup, by name to every "worker", then by broadcast.
	struct KindSpeaker final : cam::Agent {
		explicit KindSpeaker(const std::string& p_name) : Agent(p_name) {}
		void setup() override {
			const std::string l_name_bytes = "name bytes";
			const std::string l_broadcast_bytes = "broadcast bytes";
			send_by_name("worker", std::string("name text"), false);
			send_by_name("worker", raw(l_name_bytes), l_name_bytes.size(), false);
			send_by_name("worker", json("name json"), false);
			broadcast(std::string("broadcast text"));
			broadcast(raw(l_broadcast_bytes), l_broadcast_bytes.size());
			broadcast(json("broadcast json"));
		}
	};

	// Records "<sender>|<receiver>|<origin>|<content>" for every message it receives.
	struct OriginReceiver final : cam::Agent {
		explicit OriginReceiver(const std::string& p_name) : Agent(p_name) {}
		void action(const cam::MessagePointer& p_message) override {
			g_recorder.record(p_message->get_sender() + "|" + p_message->get_receiver() + "|" +
							  p_message->get_sender_environment() + "|" + p_message->to_string());
		}
	};

	// Sends bytes, a text and a JSON value at setup, by id, to a given agent.
	struct IdSpeaker final : cam::Agent {
		IdSpeaker(const std::string& p_name, std::string p_target_id) : Agent(p_name), m_target_id(std::move(p_target_id)) {}
		void setup() override {
			const std::string l_bytes = "id bytes";
			send(m_target_id, raw(l_bytes), l_bytes.size());
			send(m_target_id, std::string("id text"));
			send(m_target_id, json("id json"));
		}
	private:
		std::string m_target_id;
	};

	// Records "<its turn>:<name>" at each action; at its first action it may stop, or add an agent named "<name>+".
	struct OrderedAgent final : cam::Agent {
		OrderedAgent(const std::string& p_name, const bool p_stop_first, const bool p_spawn_first) :
				Agent(p_name),
				m_stop_first(p_stop_first),
				m_spawn_first(p_spawn_first) {}
		void default_action() override {
			g_recorder.record(std::to_string(m_turn) + ":" + get_name());
			if (m_turn == 0 && m_spawn_first) {
				get_environment()->add<OrderedAgent>(get_name() + "+", false, false);
			}
			if (m_turn == 0 && m_stop_first) {
				stop();
			}
			m_turn++;
		}
	private:
		bool m_stop_first;
		bool m_spawn_first;
		int m_turn = 0;
	};

	// Stops itself at its first action (turn 1).
	struct Quitter final : cam::Agent {
		explicit Quitter(const std::string& p_name) : Agent(p_name) {}
		void default_action() override {
			g_recorder.record(get_name() + ":quit");
			stop();
		}
	};

	// Reports its number to its parent at its first turn and stops, so it is removed at the next turn boundary.
	struct Child final : cam::Agent {
		Child(const std::string& p_name, std::string p_parent_id, const int p_number) :
				Agent(p_name), m_parent_id(std::move(p_parent_id)), m_number(p_number) {}
		void setup() override {
			send(m_parent_id, json{{"child", m_number}});
			stop();
		}
	private:
		std::string m_parent_id;
		int m_number;
	};

	// Adds a child at each action, five in all, and records the JSON reports of its children.
	struct Parent final : cam::Agent {
		explicit Parent(const std::string& p_name) : Agent(p_name) {}
		void action(const cam::MessagePointer& p_message) override {
			g_recorder.record("report:" + std::to_string(p_message->as_json().at("child").get<int>()));
			spawn();
		}
		void default_action() override { spawn(); }
	private:
		void spawn() {
			if (m_children < 5) {
				m_children++;
				get_environment()->add<Child>("child", get_id(), m_children);
			}
		}
		int m_children = 0;
	};

	// Needs more than the default alignment of new; records whether it got it.
	struct alignas(64) Aligned final : cam::Agent {
		explicit Aligned(const std::string& p_name) : Agent(p_name) {}
		void setup() override {
			g_recorder.record(reinterpret_cast<std::uintptr_t>(this) % 64 == 0 ? "aligned" : "misaligned");
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

	TEST_CASE("a local message carries its sender, receiver and content, no origin, whatever the content (P8)") {
		// Only the environment can build a message in place
		static_assert(!std::is_default_constructible_v<cam::Message::Key>);

		g_recorder.clear();
		cam::Environment l_env(2, Mode::Sequential);
		const std::string l_receiver = l_env.add<OriginReceiver>("receiver");
		const std::string l_sender = l_env.add<IdSpeaker>("speaker", l_receiver);
		l_env.start();

		const std::string l_prefix = l_sender + "|" + l_receiver + "||";
		CHECK(g_recorder.count_of(l_prefix + "id bytes") == 1);
		CHECK(g_recorder.count_of(l_prefix + "id text") == 1);
		CHECK(g_recorder.count_of(l_prefix + "\"id json\"") == 1);
		CHECK(g_recorder.count() == 3);
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

	TEST_CASE("send by name reaches its agents among many, by key, and by fragment (P13)") {
		g_recorder.clear();
		cam::Environment l_env(2, Mode::Sequential);
		for (int i = 0; i < 1000; i++) {
			l_env.add<Receiver>("n" + std::to_string(i));
		}
		for (int i = 0; i < 3; i++) {
			l_env.add<Receiver>("target");
		}
		l_env.add<Addresser>("all", "name", "target", false);
		l_env.add<Addresser>("one", "name", "target", true);
		l_env.add<Addresser>("part", "fragment", "targ", false);
		l_env.start();
		CHECK(g_recorder.count_of("target:\"by name\"") == 4);
		CHECK(g_recorder.count_of("target:\"by fragment\"") == 3);
		CHECK(g_recorder.count() == 7);
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

	TEST_CASE("every receiver of a send by name or a broadcast gets the whole content, text, JSON or bytes (P12)") {
		// The content is copied for every receiver but the last, which takes it: none may get a moved-from value
		g_recorder.clear();
		cam::Environment l_env(2, Mode::Sequential);
		for (int i = 0; i < 3; i++) {
			l_env.add<KindReceiver>("worker");
		}
		l_env.add<KindSpeaker>("speaker");
		l_env.start();
		for (const std::string l_mode: {"name", "broadcast"}) {
			CAPTURE(l_mode);
			CHECK(g_recorder.count_of("worker:text:" + l_mode + " text") == 3);
			CHECK(g_recorder.count_of("worker:bytes:" + l_mode + " bytes") == 3);
			CHECK(g_recorder.count_of("worker:json:\"" + l_mode + " json\"") == 3);
		}
		CHECK(g_recorder.count() == 18);
	}

	TEST_CASE("agents born and removed over the turns exchange JSON, and an over-aligned agent keeps its alignment (P8)") {
		g_recorder.clear();
		cam::Environment l_env(20, Mode::Sequential);
		l_env.add<Parent>("parent");
		l_env.add<Aligned>("aligned");
		l_env.start();

		for (int i = 1; i <= 5; i++) {
			CHECK(g_recorder.count_of("report:" + std::to_string(i)) == 1);
		}
		CHECK(g_recorder.count_of("aligned") == 1);
		CHECK(g_recorder.count_of("misaligned") == 0);
		CHECK(l_env.agents_count() == 2);  // the children stopped and were removed
	}

	TEST_CASE("in Sequential mode the agents run in insertion order, after removals and additions (P9)") {
		// Locks the order kept by the one-pass removal of the dead agents
		g_recorder.clear();
		cam::Environment l_env(4, Mode::Sequential);
		l_env.add<OrderedAgent>("a", false, true);
		l_env.add<OrderedAgent>("b", true, false);
		l_env.add<OrderedAgent>("c", true, false);
		l_env.add<OrderedAgent>("d", false, false);
		l_env.start();
		const std::vector<std::string> l_expected = {"0:a", "0:b", "0:c", "0:d", "1:a", "1:d", "2:a", "2:d", "0:a+"};
		CHECK(g_recorder.entries() == l_expected);
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

	TEST_CASE("in Parallel mode every agent runs exactly once per turn (P2)") {
		g_recorder.clear();
		cam::Environment l_env(6, Mode::Parallel);
		for (int i = 0; i < 50; i++) {
			l_env.add<OrderedAgent>("p" + std::to_string(i), false, false);
		}
		l_env.start();
		// Turn 0 runs the setups, turns 1 to 5 one action each
		CHECK(g_recorder.count() == 250);
		for (int i = 0; i < 50; i++) {
			CHECK(g_recorder.count_of("4:p" + std::to_string(i)) == 1);
		}
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

	TEST_CASE("message contents stay typed locally and round-trip through the wire encodings") {
		const json l_content = {{"key", "value"}, {"n", 3}};
		const cam::Message l_message("sender", "receiver", l_content);
		CHECK(l_message.kind() == cam::ContentKind::Json);
		CHECK(l_message.as_json() == l_content);
		CHECK(l_message.content() == l_content);
		CHECK(l_message.get_sender() == "sender");
		CHECK(l_message.get_receiver() == "receiver");
		CHECK(l_message.format() == "[sender -> receiver]: " + l_content.dump());

		const std::string l_text = "plain text";
		const cam::Message l_textual("s", "r", l_text);
		CHECK(l_textual.kind() == cam::ContentKind::Text);
		CHECK(l_textual.as_text() == l_text);
		CHECK(l_textual.content() == json(l_text));
		CHECK(l_textual.to_string() == l_text);

		const cam::Message l_raw("s", "r", raw(l_text), l_text.size());
		CHECK(l_raw.kind() == cam::ContentKind::Bytes);
		CHECK(l_raw.as_bytes().size() == l_text.size());
		CHECK(l_raw.to_string() == l_text);
		CHECK(l_raw.as_text().empty());

		// On the wire: text as is, bytes as is, a JSON value in the encoding of the options.
		using cam::transport::decode_content;
		using cam::transport::encode_content;
		using cam::transport::wire_encoding;
		CHECK(wire_encoding(l_textual.get_content(), {}).value() == cam::MessageBinaryFormat::Text);
		CHECK(wire_encoding(l_raw.get_content(), {}).value() == cam::MessageBinaryFormat::RAW);
		CHECK(wire_encoding(l_message.get_content(), {}).value() == cam::MessageBinaryFormat::MessagePack);
		const auto l_refused = wire_encoding(l_message.get_content(), cam::transport::SendOptions{.m_encoding = cam::MessageBinaryFormat::RAW});
		REQUIRE_FALSE(l_refused);
		CHECK(l_refused.error().m_code == cam::transport::ErrorCode::InvalidEnvelope);
		for (const auto l_format: {cam::MessageBinaryFormat::MessagePack, cam::MessageBinaryFormat::CBOR, cam::MessageBinaryFormat::BSON,
								   cam::MessageBinaryFormat::UBJSON, cam::MessageBinaryFormat::BJData, cam::MessageBinaryFormat::JSON}) {
			const auto l_bytes = encode_content(l_message.get_content(), l_format);
			REQUIRE(l_bytes);
			const auto l_back = decode_content(l_bytes.value(), l_format);
			REQUIRE(l_back);
			CHECK(*std::get<std::shared_ptr<const json>>(l_back.value()) == l_content);
		}
		const auto l_text_back = decode_content(encode_content(l_textual.get_content(), cam::MessageBinaryFormat::Text).value(),
												cam::MessageBinaryFormat::Text);
		REQUIRE(l_text_back);
		CHECK(std::get<std::string>(l_text_back.value()) == l_text);
		const auto l_garbage = decode_content(raw_bytes("not msgpack"), cam::MessageBinaryFormat::MessagePack);
		CHECK_FALSE(l_garbage);
	}
}
