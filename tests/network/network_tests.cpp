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

// Network behaviour through the MQTT transport: delivery to a remote agent, discovery of the other
// environments, migration there and back, isolation of two namespaces, reconnection. Each environment
// of this process is its own node (one connection) on the broker given by CAM_MQTT_HOST and
// CAM_MQTT_PORT; without them, if the port does not answer, or if this build has no MQTT transport, the
// whole executable skips with exit code 77 (CTest SKIP_RETURN_CODE). The reconnection test also needs
// CAM_MQTT_BROKER_CONTROL (path of a script accepting "stop" and "start"); it is skipped otherwise.
// Every wait is a condition with a deadline, never a fixed sleep.

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <Environment.h>
#include <transport/Facade.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "Recorder.hpp"
#include "tcp_probe.hpp"
#include "wait_until.hpp"

namespace {

	using namespace std::chrono_literals;
	using namespace cam::transport;
	using Mode = cam::EnvironmentMasMode;

	std::string g_url;  // mqtt://host:port
	std::string g_broker_control;
	cam::test::Recorder g_recorder;

	constexpr int TURN_DELAY_MS = 20;
	constexpr int MAX_TURNS = 600;  // 12 s of simulation at most per environment

	// Keeps its environment alive until the test raises the stop flag or MAX_TURNS turns elapsed, then
	// stops so that start() returns. The test never removes an agent from outside during a turn.
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

	// Records the messages it receives; stops with the keepers so that its environment ends.
	struct Receiver final : cam::Agent {
		explicit Receiver(const std::string& p_name) : Agent(p_name) {}
		void action(const cam::MessagePointer& p_message) override {
			g_recorder.record(get_name() + ":" + p_message->to_string());
		}
		void default_action() override {
			if (g_stop_keepers.load()) {
				stop();
			}
		}
	};

	// Sends one message by id to an agent of another environment, retrying every turn until the
	// directory knows the receiver; stops with the keepers.
	struct RemoteSender final : cam::Agent {
		RemoteSender(const std::string& p_name, cam::AgentId p_target_id) : Agent(p_name), m_target_id(p_target_id) {}
		void default_action() override {
			if (!m_sent && send(m_target_id, json("hello from afar"))) {
				m_sent = true;
				g_recorder.record(get_name() + ":sent");
			}
			if (g_stop_keepers.load()) {
				stop();
			}
		}
	private:
		cam::AgentId m_target_id;
		bool m_sent = false;
	};

	// Sends "o0", "o1"... p_total texts by id, in order: as many per turn as the transport takes, and from the first
	// refused one again at the next turn (the directory may not know the receiver yet); stops with the keepers.
	struct OrderedSender final : cam::Agent {
		OrderedSender(const std::string& p_name, cam::AgentId p_target_id, const int p_total) :
				Agent(p_name), m_target_id(p_target_id), m_total(p_total) {}
		void default_action() override {
			while (m_next < m_total && send(m_target_id, "o" + std::to_string(m_next))) {
				m_next++;
			}
			if (g_stop_keepers.load()) {
				stop();
			}
		}
	private:
		cam::AgentId m_target_id;
		int m_total;
		int m_next = 0;
	};

	// The texts a receiver recorded, in their order of arrival
	std::vector<std::string> received_by(const std::string& p_receiver) {
		std::vector<std::string> l_texts;
		for (const auto& l_entry: g_recorder.entries()) {
			if (l_entry.rfind(p_receiver + ":", 0) == 0) {
				l_texts.push_back(l_entry.substr(p_receiver.size() + 1));
			}
		}
		return l_texts;
	}

	// Sends "ping<n>" by id every 25 turns (reconnection test).
	struct Pinger final : cam::Agent {
		Pinger(const std::string& p_name, cam::AgentId p_target_id) : Agent(p_name), m_target_id(p_target_id) {}
		void default_action() override {
			if (++m_turns % 25 == 0) {
				(void) send(m_target_id, json("ping" + std::to_string(m_turns / 25)));
			}
			if (g_stop_keepers.load()) {
				stop();
			}
		}
	private:
		cam::AgentId m_target_id;
		int m_turns = 0;
	};

	// Broadcasts "wave" on the network at its second turn; stops with the keepers.
	struct Waver final : cam::Agent {
		explicit Waver(const std::string& p_name) : Agent(p_name) {}
		void default_action() override {
			if (!m_done) {
				m_done = true;
				(void) broadcast(json("wave"), SendOptions{.m_scope = Scope::Network});
			}
			if (g_stop_keepers.load()) {
				stop();
			}
		}
	private:
		bool m_done = false;
	};

	// Records every online environment it sees, once.
	struct Watcher final : cam::Agent {
		explicit Watcher(const std::string& p_name) : Agent(p_name) {}
		void default_action() override {
			for (const auto& l_environment: get_environment()->environments()) {
				if (l_environment.m_online && std::find(m_seen.begin(), m_seen.end(), l_environment.m_id) == m_seen.end()) {
					m_seen.push_back(l_environment.m_id);
					g_recorder.record(get_name() + ":seen:" + l_environment.m_name);
				}
			}
			if (g_stop_keepers.load()) {
				stop();
			}
		}
	private:
		std::vector<std::string> m_seen;
	};

	// Travels to the other environment and back, carrying a counter, then reports.
	struct Traveler final : cam::Agent {
		explicit Traveler(const std::string& p_name) : Agent(p_name) {}
		Traveler(const std::string& p_name, std::string p_home_id, std::string p_away_id) :
				Agent(p_name), m_home_id(std::move(p_home_id)), m_away_id(std::move(p_away_id)) {}

		void setup() override { g_recorder.record(get_name() + ":setup@" + get_environment()->get_id()); }

		void default_action() override {
			// The counter follows the agent: one per successful move, one at the end (attempts do not count).
			if (m_hops == 0) {
				if (move(m_away_id)) {
					m_hops = 1;
					++m_counter;
				}
			} else if (m_hops == 1) {
				if (move(m_home_id)) {
					m_hops = 2;
					++m_counter;
				}
			} else {
				++m_counter;
				g_recorder.record(get_name() + ":done@" + get_environment()->get_id() + " counter=" + std::to_string(m_counter));
				stop();
			}
		}

		[[nodiscard]] json save_state() const override {
			return {{"home", m_home_id}, {"away", m_away_id}, {"hops", m_hops}, {"counter", m_counter}};
		}

		void load_state(const json& p_state) override {
			m_home_id = p_state.at("home").get<std::string>();
			m_away_id = p_state.at("away").get<std::string>();
			m_hops = p_state.at("hops").get<int>();
			m_counter = p_state.at("counter").get<int>();
		}

	private:
		std::string m_home_id;
		std::string m_away_id;
		int m_hops = 0;
		int m_counter = 0;
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

	// The broker is shared: two test processes running at once (two machines, two presets) must not meet,
	// so every namespace of this process carries a random suffix.
	const std::string& process_suffix() {
		static const std::string s_suffix = [] {
			std::random_device l_device;
			return std::to_string(l_device() % 1000000);
		}();
		return s_suffix;
	}

	// One node per environment, in the namespace of the test.
	Result<void> connect(cam::Environment& p_environment, const std::string& p_name, const std::string& p_namespace,
						 const std::string& p_query = {}) {
		EndpointConfig l_config;
		l_config.m_name = p_name;
		return p_environment.connect(g_url + "/" + p_namespace + "-" + process_suffix() + (p_query.empty() ? "" : "?" + p_query), l_config);
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

} // namespace

TEST_SUITE("mqtt") {

	TEST_CASE("a message sent by id to an agent of another environment is delivered through the broker") {
		g_recorder.clear();
		TestEnvironment l_env_a("A");
		TestEnvironment l_env_b("B");
		REQUIRE(connect(l_env_a, "A", "cam-test-delivery"));
		REQUIRE(connect(l_env_b, "B", "cam-test-delivery"));

		const cam::AgentId l_receiver = l_env_b.add<Receiver>("receiver");
		l_env_b.add<Keeper>("keeper-b");
		l_env_a.add<RemoteSender>("sender", l_receiver);
		l_env_a.add<Keeper>("keeper-a");

		const bool l_delivered = run_until({&l_env_b, &l_env_a}, [] { return g_recorder.contains("receiver:\"hello from afar\""); }, 15s);
		CHECK(l_delivered);
		CHECK(g_recorder.count_of("receiver:\"hello from afar\"") == 1);
		CHECK(g_recorder.contains("sender:sent"));
		CHECK(l_env_a.statistics().m_sent >= 1);
		CHECK(l_env_b.statistics().m_delivered >= 1);
		CHECK(l_env_b.statistics().m_rejected == 0);
		REQUIRE(l_env_a.disconnect());
		REQUIRE(l_env_b.disconnect());
	}

	TEST_CASE("the messages of one sender cross the broker in the order they were sent") {
		g_recorder.clear();
		TestEnvironment l_env_a("A");
		TestEnvironment l_env_b("B");
		REQUIRE(connect(l_env_a, "A", "cam-test-order"));
		REQUIRE(connect(l_env_b, "B", "cam-test-order"));

		const cam::AgentId l_receiver = l_env_b.add<Receiver>("receiver");
		l_env_b.add<Keeper>("keeper-b");
		l_env_a.add<OrderedSender>("sender", l_receiver, 200);
		l_env_a.add<Keeper>("keeper-a");

		const bool l_all = run_until({&l_env_b, &l_env_a}, [] { return received_by("receiver").size() >= 200; }, 30s);
		CHECK(l_all);
		const auto l_received = received_by("receiver");
		REQUIRE(l_received.size() == 200);
		for (int i = 0; i < 200; i++) {
			CHECK(l_received[i] == "o" + std::to_string(i));
		}
		REQUIRE(l_env_a.disconnect());
		REQUIRE(l_env_b.disconnect());
	}

	TEST_CASE("a transport is opened once and a closed port is refused") {
		const auto l_opened = cam::transport::open(g_url + "/cam-test-open-" + process_suffix());
		REQUIRE(l_opened);
		const auto l_again = l_opened.value()->open();
		CHECK_FALSE(l_again);
		CHECK(l_again.error().m_code == ErrorCode::AlreadyConnected);
		CHECK(l_opened.value()->close(2s));
		// Nothing listens on port 1: the connection is refused at once, no broker involved.
		const auto l_refused = cam::transport::open("mqtt://127.0.0.1:1/cam-test-open");
		CHECK_FALSE(l_refused);
	}

	TEST_CASE("the envelope codec is chosen by the URL, per namespace") {
		for (const std::string l_codec: {"compact", "json"}) {
			CAPTURE(l_codec);
			g_recorder.clear();
			TestEnvironment l_env_a("A");
			TestEnvironment l_env_b("B");
			REQUIRE(connect(l_env_a, "A", "cam-test-codec-" + l_codec, "codec=" + l_codec));
			REQUIRE(connect(l_env_b, "B", "cam-test-codec-" + l_codec, "codec=" + l_codec));
			const cam::AgentId l_receiver = l_env_b.add<Receiver>("receiver");
			l_env_b.add<Keeper>("keeper-b");
			l_env_a.add<RemoteSender>("sender", l_receiver);
			l_env_a.add<Keeper>("keeper-a");
			const bool l_delivered = run_until({&l_env_b, &l_env_a}, [] { return g_recorder.contains("receiver:\"hello from afar\""); }, 15s);
			CHECK(l_delivered);
			CHECK(l_env_b.statistics().m_rejected == 0);
			REQUIRE(l_env_a.disconnect());
			REQUIRE(l_env_b.disconnect());
		}
	}

	TEST_CASE("an environment discovers the other environments connected to the broker") {
		g_recorder.clear();
		TestEnvironment l_env_a("A");
		TestEnvironment l_env_b("B");
		REQUIRE(connect(l_env_a, "A", "cam-test-discovery"));
		REQUIRE(connect(l_env_b, "B", "cam-test-discovery"));
		l_env_a.add<Watcher>("watcher-a");
		l_env_b.add<Watcher>("watcher-b");

		const bool l_seen = run_until({&l_env_a, &l_env_b},
									  [] { return g_recorder.contains("watcher-a:seen:B") && g_recorder.contains("watcher-b:seen:A"); }, 15s);
		CHECK(l_seen);
		CHECK(is_canonical_uuid(l_env_a.get_id()));
		CHECK(l_env_a.get_id() != l_env_b.get_id());
		CHECK(g_recorder.contains("A:transport:connected"));
		REQUIRE(l_env_b.disconnect());
		// A learns the departure at its next boundary.
		l_env_a.add<Keeper>("keeper-a");
		const bool l_left = run_until({&l_env_a}, [] { return g_recorder.contains("A:left:B"); }, 10s);
		CHECK(l_left);
		REQUIRE(l_env_a.disconnect());
	}

	TEST_CASE("an agent migrates to another environment and back with its state") {
		g_recorder.clear();
		TestEnvironment l_home("home");
		TestEnvironment l_away("away");
		l_home.register_agent_type<Traveler>("Traveler");
		l_away.register_agent_type<Traveler>("Traveler");
		REQUIRE(connect(l_home, "home", "cam-test-migration"));
		REQUIRE(connect(l_away, "away", "cam-test-migration"));

		l_home.add<Traveler>("traveler", l_home.get_id(), l_away.get_id());
		l_home.add<Keeper>("keeper-home");
		l_away.add<Keeper>("keeper-away");

		const bool l_done = run_until({&l_away, &l_home}, [] { return recorded_prefix("traveler:done@"); }, 30s);
		CHECK(l_done);
		CHECK(g_recorder.count_of("traveler:setup@" + l_home.get_id()) == 2);  // departure and return
		CHECK(g_recorder.count_of("traveler:setup@" + l_away.get_id()) == 1);
		CHECK(g_recorder.count_of("traveler:done@" + l_home.get_id() + " counter=3") == 1);  // state carried over
		CHECK(l_home.statistics().m_migrations_completed == 2);
		CHECK(l_away.statistics().m_migrations_completed == 2);
		CHECK_FALSE(recorded_prefix("home:failure"));
		REQUIRE(l_home.disconnect());
		REQUIRE(l_away.disconnect());
	}

	TEST_CASE("two namespaces on the same broker never mix") {
		g_recorder.clear();
		TestEnvironment l_one_a("one-a");
		TestEnvironment l_one_b("one-b");
		TestEnvironment l_two("two");
		REQUIRE(connect(l_one_a, "one-a", "cam-test-ns-one"));
		REQUIRE(connect(l_one_b, "one-b", "cam-test-ns-one"));
		REQUIRE(connect(l_two, "two", "cam-test-ns-two"));
		l_one_a.add<Waver>("waver");
		l_one_a.add<Keeper>("keeper-1a");
		l_one_b.add<Receiver>("receiver-one");
		l_one_b.add<Watcher>("watcher-one");
		l_two.add<Receiver>("receiver-two");
		l_two.add<Watcher>("watcher-two");

		const bool l_waved = run_until({&l_one_a, &l_one_b, &l_two}, [] { return g_recorder.contains("receiver-one:\"wave\""); }, 15s);
		CHECK(l_waved);
		CHECK(g_recorder.count_of("receiver-two:\"wave\"") == 0);
		CHECK(g_recorder.contains("watcher-one:seen:one-a"));
		CHECK_FALSE(g_recorder.contains("watcher-two:seen:one-a"));
		CHECK_FALSE(g_recorder.contains("watcher-two:seen:one-b"));
		for (const auto& l_info: l_two.environments()) {
			MESSAGE("two knows: id=" << l_info.m_id << " name=" << l_info.m_name << " node=" << l_info.m_node << " online=" << l_info.m_online);
		}
		CHECK(l_two.environments().empty());
		REQUIRE(l_one_a.disconnect());
		REQUIRE(l_one_b.disconnect());
		REQUIRE(l_two.disconnect());
	}

	TEST_CASE("a lost connection is reported and recovered with its subscriptions") {
		if (g_broker_control.empty()) {
			MESSAGE("skipped: CAM_MQTT_BROKER_CONTROL not set (script accepting stop and start)");
			return;
		}
		g_recorder.clear();
		TestEnvironment l_env_a("A");
		TestEnvironment l_env_b("B");
		REQUIRE(connect(l_env_a, "A", "cam-test-reconnect"));
		REQUIRE(connect(l_env_b, "B", "cam-test-reconnect"));
		const cam::AgentId l_receiver = l_env_b.add<Receiver>("receiver");
		l_env_b.add<Keeper>("keeper-b");
		l_env_a.add<Pinger>("pinger", l_receiver);
		l_env_a.add<Keeper>("keeper-a");

		g_stop_keepers = false;
		std::thread l_thread_b(&cam::Environment::start, &l_env_b);
		std::thread l_thread_a(&cam::Environment::start, &l_env_a);
		const bool l_first = cam::test::wait_until([] { return g_recorder.contains("receiver:\"ping1\""); }, 15s);
		CHECK(l_first);

		CHECK(std::system((g_broker_control + " stop").c_str()) == 0);
		const bool l_lost = cam::test::wait_until([] { return g_recorder.contains("A:transport:disconnected"); }, 30s);
		CHECK(l_lost);
		CHECK(std::system((g_broker_control + " start").c_str()) == 0);
		const bool l_back = cam::test::wait_until([] { return g_recorder.contains("A:transport:reconnected") && g_recorder.contains("B:transport:reconnected"); }, 60s);
		CHECK(l_back);
		const std::size_t l_before = g_recorder.count();
		const bool l_again = cam::test::wait_until([l_before] {
			for (const auto& l_entry: g_recorder.entries()) {
				if (l_entry.rfind("receiver:\"ping", 0) == 0 && l_entry != "receiver:\"ping1\"") {
					return true;
				}
			}
			return g_recorder.count() > l_before + 1000;
		}, 30s);
		CHECK(l_again);
		if (!l_again) {
			for (const auto& l_entry: g_recorder.entries()) {
				MESSAGE(l_entry);
			}
			MESSAGE("A sent=" << l_env_a.statistics().m_sent << " lost=" << l_env_a.statistics().m_lost
					<< " B received=" << l_env_b.statistics().m_received << " delivered=" << l_env_b.statistics().m_delivered
					<< " rejected=" << l_env_b.statistics().m_rejected << " unknown=" << l_env_b.statistics().m_unknown_destination);
		}
		g_stop_keepers = true;
		l_thread_a.join();
		l_thread_b.join();
		REQUIRE(l_env_a.disconnect());
		REQUIRE(l_env_b.disconnect());
	}
}

int main(int argc, char** argv) {
	const char* l_host = std::getenv("CAM_MQTT_HOST");
	const char* l_port = std::getenv("CAM_MQTT_PORT");
	if (!l_host || !l_port) {
		std::cout << "SKIP: CAM_MQTT_HOST and CAM_MQTT_PORT are not set; the MQTT tests need a broker\n";
		return 77;
	}
	const auto l_schemes = cam::transport::schemes();
	if (std::find(l_schemes.begin(), l_schemes.end(), "mqtt") == l_schemes.end()) {
		std::cout << "SKIP: this build has no MQTT transport (CAM_TRANSPORT_MQTT)\n";
		return 77;
	}
	if (!cam::test::tcp_port_open(l_host, std::atoi(l_port), 2s)) {
		std::cout << "SKIP: no broker answers on " << l_host << ":" << l_port << "\n";
		return 77;
	}
	g_url = std::string("mqtt://") + l_host + ":" + l_port;
	if (const char* l_control = std::getenv("CAM_MQTT_BROKER_CONTROL"); l_control) {
		g_broker_control = l_control;
	}
	doctest::Context l_context(argc, argv);
	return l_context.run();
}
