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

// Network behaviour through the ActressMAS transport: containers of this process on 127.0.0.1, through the relay of the
// tests, which follows the rules of the ContainerServer of the C# (support/ActressMasRelay.hpp). Presence from the list
// of the server, a message by name to one container or to all of them, the reply by identifier and the name of a
// remote sender as "name@container", a name already held, the loss of the server, what the protocol cannot carry, and
// the frames that cannot be read. Every wait is a condition with a deadline.

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <Environment.h>
#include <transport/Facade.h>
#include <transport/actressmas/ActressMasTransport.h>
#include <util/Identifier.h>

#include "ActressMasRelay.hpp"
#include "Recorder.hpp"
#include "wait_until.hpp"

namespace {

	using namespace std::chrono_literals;
	using namespace cam::transport;
	using Mode = cam::EnvironmentMasMode;

	cam::test::Recorder g_recorder;

	constexpr int TURN_DELAY_MS = 20;
	constexpr int MAX_TURNS = 600;  // 12 s of simulation at most per environment

	// Keeps its environment alive until the test raises the stop flag or MAX_TURNS turns elapsed.
	std::atomic<bool> g_stop_keepers{false};
	struct Keeper final : cam::Agent {
		explicit Keeper(const std::string& p_name) : Agent(p_name) {}
		int m_turns = 0;
		void default_action() override {
			if (g_stop_keepers.load() || ++m_turns >= MAX_TURNS) {
				stop();
			}
		}
	};

	// Records what it receives under the label of its container, and answers its sender by identifier.
	struct Responder final : cam::Agent {
		Responder(const std::string& p_name, std::string p_label) : Agent(p_name), m_label(std::move(p_label)) {}
		void action(const cam::MessagePointer& p_message) override {
			g_recorder.record(m_label + ":" + get_name() + ":received:" + p_message->to_string());
			g_recorder.record(m_label + ":format:" + format(p_message));
			(void) send(p_message->get_sender(), "reply from " + m_label);
		}
		void default_action() override {
			if (g_stop_keepers.load()) {
				stop();
			}
		}
	private:
		std::string m_label;
	};

	// Sends "hello" by name on the network once its environment knows p_containers other containers, then records the
	// replies.
	struct Pinger final : cam::Agent {
		Pinger(const std::string& p_name, std::string p_target, const std::size_t p_containers) :
				Agent(p_name), m_target(std::move(p_target)), m_containers(p_containers) {}
		void action(const cam::MessagePointer& p_message) override {
			g_recorder.record(get_name() + ":received:" + p_message->to_string());
			g_recorder.record(get_name() + ":format:" + format(p_message));
		}
		void default_action() override {
			if (!m_sent && get_environment()->environments().size() >= m_containers) {
				SendOptions l_options;
				l_options.m_scope = Scope::Network;
				m_sent = true;
				if (!send_by_name(m_target, std::string("hello"), true, l_options)) {
					g_recorder.record(get_name() + ":refused");
				}
			}
			if (g_stop_keepers.load()) {
				stop();
			}
		}
	private:
		std::string m_target;
		std::size_t m_containers;
		bool m_sent = false;
	};

	// Environment recording its transport hooks (simulation thread).
	struct TestEnvironment final : cam::Environment {
		explicit TestEnvironment(std::string p_label) : Environment(0, Mode::Sequential, TURN_DELAY_MS), m_label(std::move(p_label)) {}
	protected:
		void on_transport_event(const TransportEvent p_event) override { g_recorder.record(m_label + ":transport:" + to_string(p_event)); }
		void on_environment_joined(const EnvironmentInfo& p_info) override { g_recorder.record(m_label + ":joined:" + p_info.m_name); }
		void on_environment_left(const EnvironmentInfo& p_info) override { g_recorder.record(m_label + ":left:" + p_info.m_name); }
		void on_delivery_failure(const Error& p_error) override { g_recorder.record(m_label + ":failure:" + to_string(p_error.m_code)); }
	private:
		std::string m_label;
	};

	std::string url_of(const cam::test::ActressMasRelay& p_relay, const std::string& p_container) {
		return "actressmas://127.0.0.1:" + std::to_string(p_relay.port()) + "/" + p_container;
	}

	Result<void> connect(cam::Environment& p_environment, const cam::test::ActressMasRelay& p_relay, const std::string& p_container) {
		EndpointConfig l_config;
		l_config.m_name = p_container;
		return p_environment.connect(url_of(p_relay, p_container), l_config);
	}

	// A transport of its own, with no environment, on the relay
	std::shared_ptr<ActressMasTransport> open_transport(const cam::test::ActressMasRelay& p_relay, const std::string& p_container) {
		TransportConfig l_config;
		l_config.m_endpoint = "127.0.0.1:" + std::to_string(p_relay.port());
		auto l_transport = std::make_shared<ActressMasTransport>(l_config, ActressMasOptions{p_container});
		if (!l_transport->open()) {
			return nullptr;
		}
		return l_transport;
	}

	// Runs the environments in threads until the condition holds or the deadline passes.
	template<typename Predicate>
	bool run_until(std::vector<cam::Environment*> p_environments, Predicate p_predicate, const std::chrono::milliseconds p_timeout) {
		g_stop_keepers = false;
		std::vector<std::thread> l_threads;
		for (cam::Environment* l_environment: p_environments) {
			l_threads.emplace_back(&cam::Environment::start, l_environment);
		}
		const bool l_result = cam::test::wait_until(p_predicate, p_timeout);
		g_stop_keepers = true;
		for (auto& l_thread: l_threads) {
			l_thread.join();
		}
		return l_result;
	}

	bool recorded_prefix(const std::string& p_prefix) {
		for (const auto& l_entry: g_recorder.entries()) {
			if (l_entry.rfind(p_prefix, 0) == 0) {
				return true;
			}
		}
		return false;
	}

	std::size_t count_prefix(const std::string& p_prefix) {
		std::size_t l_count = 0;
		for (const auto& l_entry: g_recorder.entries()) {
			if (l_entry.rfind(p_prefix, 0) == 0) {
				l_count++;
			}
		}
		return l_count;
	}

	// A frame whose header is valid and whose body is not: its base64 decodes to zeros, which no record starts with
	std::vector<std::uint8_t> unreadable_frame() {
		auto l_frame = ActressMasCodec::encode_frame({"Server", "Solo", std::string(ActressMasCodec::INFORM_CONTAINERS), ""});
		for (std::size_t i = ActressMasCodec::HEADER_SIZE; i < l_frame.size(); i++) {
			l_frame[i] = (i - ActressMasCodec::HEADER_SIZE) % 2 == 0 ? 0x41 : 0x00;
		}
		return l_frame;
	}

} // namespace

TEST_SUITE("actressmas") {

	TEST_CASE("a container sees the others from the list of the server, and sees them leave") {
		g_recorder.clear();
		cam::test::ActressMasRelay l_relay;
		REQUIRE(l_relay.start());
		TestEnvironment l_env_1("C1");
		TestEnvironment l_env_2("C2");
		REQUIRE(connect(l_env_1, l_relay, "Container1"));
		REQUIRE(connect(l_env_2, l_relay, "Container2"));
		l_env_1.add<Keeper>("keeper-1");
		l_env_2.add<Keeper>("keeper-2");

		const bool l_joined = run_until({&l_env_1, &l_env_2}, [] {
			return g_recorder.contains("C1:joined:Container2") && g_recorder.contains("C2:joined:Container1");
		}, 10s);
		CHECK(l_joined);
		CHECK(g_recorder.contains("C1:transport:connected"));

		REQUIRE(l_env_2.disconnect());
		l_env_1.add<Keeper>("keeper-1b");
		const bool l_left = run_until({&l_env_1}, [] { return g_recorder.contains("C1:left:Container2"); }, 10s);
		CHECK(l_left);
		REQUIRE(l_env_1.disconnect());
	}

	TEST_CASE("a message by name@container reaches that container only, and the reply by identifier comes back") {
		g_recorder.clear();
		cam::test::ActressMasRelay l_relay;
		REQUIRE(l_relay.start());
		TestEnvironment l_env_1("C1");
		TestEnvironment l_env_2("C2");
		TestEnvironment l_env_3("C3");
		REQUIRE(connect(l_env_1, l_relay, "Container1"));
		REQUIRE(connect(l_env_2, l_relay, "Container2"));
		REQUIRE(connect(l_env_3, l_relay, "Container3"));
		l_env_1.add<Pinger>("ping", "pong@Container2", 0);
		l_env_1.add<Keeper>("keeper-1");
		l_env_2.add<Responder>("pong", "C2");
		l_env_2.add<Keeper>("keeper-2");
		l_env_3.add<Responder>("pong", "C3");
		l_env_3.add<Keeper>("keeper-3");

		const bool l_answered = run_until({&l_env_1, &l_env_2, &l_env_3}, [] { return recorded_prefix("ping:received:"); }, 10s);
		CHECK(l_answered);
		CHECK(count_prefix("C2:pong:received:") == 1);
		CHECK(count_prefix("C3:pong:received:") == 0);
		CHECK(g_recorder.contains("C2:pong:received:hello"));
		CHECK(g_recorder.contains("ping:received:reply from C2"));
		// A remote sender is "name@container", as in C#, both ways
		CHECK(g_recorder.contains("C2:format:[ping@Container1 -> pong]: hello"));
		CHECK(g_recorder.contains("ping:format:[pong@Container2 -> ping]: reply from C2"));
		CHECK(l_env_1.statistics().m_rejected == 0);
		CHECK(l_env_2.statistics().m_rejected == 0);
		REQUIRE(l_env_1.disconnect());
		REQUIRE(l_env_2.disconnect());
		REQUIRE(l_env_3.disconnect());
	}

	TEST_CASE("a message by name without a container goes to every other container, and only the one with that agent delivers it") {
		g_recorder.clear();
		cam::test::ActressMasRelay l_relay;
		REQUIRE(l_relay.start());
		TestEnvironment l_env_1("C1");
		TestEnvironment l_env_2("C2");
		TestEnvironment l_env_3("C3");
		REQUIRE(connect(l_env_1, l_relay, "Container1"));
		REQUIRE(connect(l_env_2, l_relay, "Container2"));
		REQUIRE(connect(l_env_3, l_relay, "Container3"));
		l_env_1.add<Pinger>("ping", "pong", 2);
		l_env_1.add<Keeper>("keeper-1");
		l_env_2.add<Responder>("pong", "C2");
		l_env_2.add<Keeper>("keeper-2");
		l_env_3.add<Responder>("other", "C3");
		l_env_3.add<Keeper>("keeper-3");

		const bool l_answered = run_until({&l_env_1, &l_env_2, &l_env_3}, [] { return recorded_prefix("ping:received:"); }, 10s);
		CHECK(l_answered);
		CHECK(count_prefix("C2:pong:received:") == 1);
		CHECK_FALSE(recorded_prefix("C3:other:received:"));
		CHECK_FALSE(recorded_prefix("C3:format:"));
		std::vector<std::string> l_receivers;
		for (const auto& l_message: l_relay.relayed()) {
			if (l_message.m_sender == "Container1") {
				l_receivers.push_back(l_message.m_receiver);
			}
		}
		std::sort(l_receivers.begin(), l_receivers.end());
		CHECK(l_receivers == std::vector<std::string>{"Container2", "Container3"});
		REQUIRE(l_env_1.disconnect());
		REQUIRE(l_env_2.disconnect());
		REQUIRE(l_env_3.disconnect());
	}

	TEST_CASE("a name the server already holds is refused when connecting") {
		cam::test::ActressMasRelay l_relay;
		REQUIRE(l_relay.start());
		TestEnvironment l_env_1("C1");
		TestEnvironment l_env_2("C1bis");
		REQUIRE(connect(l_env_1, l_relay, "Container1"));
		const auto l_refused = connect(l_env_2, l_relay, "Container1");
		REQUIRE_FALSE(l_refused);
		CHECK(l_refused.error().m_code == ErrorCode::InvalidConfiguration);
		REQUIRE(l_env_1.disconnect());
	}

	TEST_CASE("a URL without a container name is refused before anything opens") {
		TestEnvironment l_env("C");
		const auto l_result = l_env.connect("actressmas://127.0.0.1:5000");
		REQUIRE_FALSE(l_result);
		CHECK(l_result.error().m_code == ErrorCode::InvalidConfiguration);
	}

	TEST_CASE("the loss of the server disconnects the container, and the publications that follow are refused") {
		cam::test::ActressMasRelay l_relay;
		REQUIRE(l_relay.start());
		auto l_transport = open_transport(l_relay, "Solo");
		REQUIRE(l_transport);
		Endpoint l_endpoint(cam::generate_uuid(), EndpointConfig{});
		REQUIRE(l_endpoint.attach(l_transport));
		CHECK(l_transport->is_connected());

		l_relay.stop();
		CHECK(cam::test::wait_until([&] { return !l_transport->is_connected(); }, 5s));
		const auto l_events = l_endpoint.take_events();
		CHECK(std::find(l_events.begin(), l_events.end(), TransportEvent::Disconnected) != l_events.end());
		Envelope l_message = l_endpoint.make_envelope(EnvelopeKind::Message);
		l_message.m_target_kind = TargetKind::Name;
		l_message.m_target_name = "pong@Container2";
		const auto l_result = l_transport->publish(l_endpoint, l_endpoint.address(AddressKind::AllIn), l_message);
		REQUIRE_FALSE(l_result);
		CHECK(l_result.error().m_code == ErrorCode::NotConnected);
		REQUIRE(l_endpoint.detach());
	}

	TEST_CASE("what the protocol cannot carry is refused, and the presence of the environment is absorbed") {
		cam::test::ActressMasRelay l_relay;
		REQUIRE(l_relay.start());
		auto l_transport = open_transport(l_relay, "Solo");
		REQUIRE(l_transport);
		Endpoint l_endpoint(cam::generate_uuid(), EndpointConfig{});
		REQUIRE(l_endpoint.attach(l_transport));
		const Address l_all = l_endpoint.address(AddressKind::AllIn);

		// A second environment on the same container
		Endpoint l_second(cam::generate_uuid(), EndpointConfig{});
		const auto l_second_attach = l_second.attach(l_transport);
		REQUIRE_FALSE(l_second_attach);
		CHECK(l_second_attach.error().m_code == ErrorCode::NotSupported);

		// The presence of the environment: the server keeps the list
		Envelope l_presence = l_endpoint.make_envelope(EnvelopeKind::Presence);
		CHECK(l_transport->publish(l_endpoint, l_endpoint.address(AddressKind::EnvPresence, l_endpoint.environment_id()), l_presence));

		// Migration
		Envelope l_offer = l_endpoint.make_envelope(EnvelopeKind::AgentOffer);
		const auto l_moved = l_transport->publish(l_endpoint, l_all, l_offer);
		REQUIRE_FALSE(l_moved);
		CHECK(l_moved.error().m_code == ErrorCode::NotSupported);

		// Broadcast and fragment of a name
		for (const TargetKind l_kind: {TargetKind::Broadcast, TargetKind::Fragment}) {
			Envelope l_message = l_endpoint.make_envelope(EnvelopeKind::Message);
			l_message.m_target_kind = l_kind;
			l_message.m_target_name = "pong";
			const auto l_result = l_transport->publish(l_endpoint, l_all, l_message);
			REQUIRE_FALSE(l_result);
			CHECK(l_result.error().m_code == ErrorCode::NotSupported);
		}

		// A name without container, while no other container is there; an identifier never met
		Envelope l_by_name = l_endpoint.make_envelope(EnvelopeKind::Message);
		l_by_name.m_target_kind = TargetKind::Name;
		l_by_name.m_target_name = "pong";
		const auto l_nobody = l_transport->publish(l_endpoint, l_all, l_by_name);
		REQUIRE_FALSE(l_nobody);
		CHECK(l_nobody.error().m_code == ErrorCode::UnknownDestination);
		Envelope l_by_id = l_endpoint.make_envelope(EnvelopeKind::Message);
		l_by_id.m_target_kind = TargetKind::Id;
		l_by_id.m_target_id = "0123456789abcde";
		const auto l_unknown = l_transport->publish(l_endpoint, l_endpoint.address(AddressKind::EnvIn, cam::generate_uuid()), l_by_id);
		REQUIRE_FALSE(l_unknown);
		CHECK(l_unknown.error().m_code == ErrorCode::UnknownDestination);
		REQUIRE(l_endpoint.detach());
		REQUIRE(l_transport->close(1s));
	}

	TEST_CASE("an unreadable frame is counted and skipped; a broken header closes the connection") {
		cam::test::ActressMasRelay l_relay;
		REQUIRE(l_relay.start());
		auto l_transport = open_transport(l_relay, "Solo");
		REQUIRE(l_transport);

		REQUIRE(l_relay.send_raw("Solo", unreadable_frame()));
		CHECK(cam::test::wait_until([&] { return l_transport->malformed() == 1; }, 5s));
		CHECK(l_transport->is_connected());

		// An agent of the C# cannot run here: it is refused and counted
		const auto l_move = ActressMasCodec::encode_frame({"Other", "Solo", std::string(ActressMasCodec::REQUEST_MOVE_AGENT), "AAAA"});
		REQUIRE(l_relay.send_raw("Solo", l_move));
		CHECK(cam::test::wait_until([&] { return l_transport->refused_moves() == 1; }, 5s));
		CHECK(l_transport->is_connected());

		auto l_broken = unreadable_frame();
		l_broken[1] = 0xFE;  // not the marker 01 FF
		REQUIRE(l_relay.send_raw("Solo", l_broken));
		CHECK(cam::test::wait_until([&] { return !l_transport->is_connected(); }, 5s));
		CHECK(l_transport->malformed() == 2);
		REQUIRE(l_transport->close(1s));
	}

	TEST_CASE("close keeps to its deadline") {
		cam::test::ActressMasRelay l_relay;
		REQUIRE(l_relay.start());
		auto l_transport = open_transport(l_relay, "Solo");
		REQUIRE(l_transport);
		const auto l_start = std::chrono::steady_clock::now();
		REQUIRE(l_transport->close(200ms));
		CHECK(std::chrono::steady_clock::now() - l_start < 2s);
		CHECK_FALSE(l_transport->is_connected());
	}

}

int main(int argc, char** argv) {
	doctest::Context l_context(argc, argv);
	return l_context.run();
}
