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

// Network behaviour through the Zenoh transport: two peers of this process on 127.0.0.1 with explicit
// endpoints (one listens, the other connects), no multicast scouting, no router. Delivery to a remote
// agent, discovery and presence by liveliness, a late joiner that learns the agents at once, migration
// there and back, isolation of two namespaces, clean departure. Skips with exit code 77 when this build
// has no Zenoh transport (CTest SKIP_RETURN_CODE). Every wait is a condition with a deadline.

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <Environment.h>
#include <transport/Facade.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

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

	// Records the messages it receives; stops with the keepers.
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

	// Sends one message by id to a remote agent, retrying every turn until the directory knows it, and
	// records the number of attempts.
	struct RemoteSender final : cam::Agent {
		RemoteSender(const std::string& p_name, std::string p_target_id) : Agent(p_name), m_target_id(std::move(p_target_id)) {}
		void default_action() override {
			if (!m_sent) {
				++m_attempts;
				if (send(m_target_id, json("hello from afar"))) {
					m_sent = true;
					g_recorder.record(get_name() + ":sent:" + std::to_string(m_attempts));
				}
			}
			if (g_stop_keepers.load()) {
				stop();
			}
		}
	private:
		std::string m_target_id;
		bool m_sent = false;
		int m_attempts = 0;
	};

	// Records "<name>:ready" at its second turn: the agents added with it are inserted and announced.
	struct Beacon final : cam::Agent {
		explicit Beacon(const std::string& p_name) : Agent(p_name) {}
		void default_action() override {
			if (++m_turns == 2) {
				g_recorder.record(get_name() + ":ready");
			}
			if (g_stop_keepers.load()) {
				stop();
			}
		}
	private:
		int m_turns = 0;
	};

	// Broadcasts "wave" on the network at its second turn.
	struct Waver final : cam::Agent {
		explicit Waver(const std::string& p_name) : Agent(p_name) {}
		void default_action() override {
			if (!m_done) {
				m_done = true;
				SendOptions l_options;
				l_options.m_scope = Scope::Network;
				(void) broadcast(json("wave"), l_options);
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
				if (move(m_away_id)) { m_hops = 1; ++m_counter; }
			} else if (m_hops == 1) {
				if (move(m_home_id)) { m_hops = 2; ++m_counter; }
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

	// One peer per environment: the first listens on a port of 127.0.0.1, the others connect to it.
	std::string listen_url(const int p_port, const std::string& p_namespace) {
		return "zenoh://?listen=tcp/127.0.0.1:" + std::to_string(p_port) + "&ns=" + p_namespace;
	}

	std::string connect_url(const int p_port, const std::string& p_namespace) {
		return "zenoh://127.0.0.1:" + std::to_string(p_port) + "/" + p_namespace;
	}

	Result<void> connect(cam::Environment& p_environment, const std::string& p_name, const std::string& p_url) {
		EndpointConfig l_config;
		l_config.m_name = p_name;
		return cam::transport::connect(p_environment, p_url, l_config);
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

	int attempts_of(const std::string& p_name) {
		for (const auto& l_entry: g_recorder.entries()) {
			const std::string l_prefix = p_name + ":sent:";
			if (l_entry.rfind(l_prefix, 0) == 0) {
				return std::stoi(l_entry.substr(l_prefix.size()));
			}
		}
		return -1;
	}

} // namespace

TEST_SUITE("zenoh") {

	TEST_CASE("a message sent by id to an agent of another peer is delivered") {
		g_recorder.clear();
		TestEnvironment l_env_a("A");
		TestEnvironment l_env_b("B");
		REQUIRE(connect(l_env_a, "A", listen_url(17447, "cam-test-delivery")));
		REQUIRE(connect(l_env_b, "B", connect_url(17447, "cam-test-delivery")));

		const std::string l_receiver = l_env_b.add<Receiver>("receiver");
		l_env_b.add<Keeper>("keeper-b");
		l_env_a.add<RemoteSender>("sender", l_receiver);
		l_env_a.add<Keeper>("keeper-a");

		const bool l_delivered = run_until({&l_env_b, &l_env_a}, [] { return g_recorder.contains("receiver:\"hello from afar\""); }, 15s);
		CHECK(l_delivered);
		CHECK(g_recorder.count_of("receiver:\"hello from afar\"") == 1);
		CHECK(l_env_a.statistics().m_sent >= 1);
		CHECK(l_env_b.statistics().m_delivered >= 1);
		CHECK(l_env_b.statistics().m_rejected == 0);
		CHECK(g_recorder.contains("A:transport:connected"));
		REQUIRE(l_env_a.disconnect());
		REQUIRE(l_env_b.disconnect());
	}

	TEST_CASE("peers discover each other by liveliness and see a clean departure") {
		g_recorder.clear();
		TestEnvironment l_env_a("A");
		TestEnvironment l_env_b("B");
		REQUIRE(connect(l_env_a, "A", listen_url(17448, "cam-test-discovery")));
		REQUIRE(connect(l_env_b, "B", connect_url(17448, "cam-test-discovery")));
		l_env_a.add<Watcher>("watcher-a");
		l_env_b.add<Watcher>("watcher-b");

		const bool l_seen = run_until({&l_env_a, &l_env_b},
									  [] { return g_recorder.contains("watcher-a:seen:B") && g_recorder.contains("watcher-b:seen:A"); }, 15s);
		CHECK(l_seen);
		CHECK(g_recorder.contains("A:joined:B"));
		CHECK(g_recorder.contains("B:joined:A"));
		REQUIRE(l_env_b.disconnect());  // its liveliness token goes
		l_env_a.add<Keeper>("keeper-a");
		const bool l_left = run_until({&l_env_a}, [] { return g_recorder.contains("A:left:B"); }, 15s);
		CHECK(l_left);
		REQUIRE(l_env_a.disconnect());
	}

	TEST_CASE("a late joiner learns the agents of the peers at once through the presence query") {
		g_recorder.clear();
		TestEnvironment l_env_a("A");
		TestEnvironment l_env_c("C");
		REQUIRE(connect(l_env_a, "A", listen_url(17449, "cam-test-late")));
		const std::string l_receiver = l_env_a.add<Receiver>("receiver");
		l_env_a.add<Beacon>("beacon");
		// A runs alone first: its presence, with the receiver, is what the newcomer must get.
		g_stop_keepers = false;
		std::thread l_thread_a(&cam::Environment::start, &l_env_a);
		const bool l_alone = cam::test::wait_until([] { return g_recorder.contains("beacon:ready"); }, 5s);
		CHECK(l_alone);

		REQUIRE(connect(l_env_c, "C", connect_url(17449, "cam-test-late")));
		l_env_c.add<RemoteSender>("sender", l_receiver);
		l_env_c.add<Keeper>("keeper-c");
		std::thread l_thread_c(&cam::Environment::start, &l_env_c);
		const bool l_delivered = cam::test::wait_until([] { return g_recorder.contains("receiver:\"hello from afar\""); }, 15s);
		g_stop_keepers = true;
		l_thread_c.join();
		l_thread_a.join();
		CHECK(l_delivered);
		// The directory came from the presence answered by A, not from a directory event (none was sent).
		CHECK(attempts_of("sender") >= 1);
		CHECK(attempts_of("sender") <= 10);
		CHECK(g_recorder.contains("C:joined:A"));
		REQUIRE(l_env_a.disconnect());
		REQUIRE(l_env_c.disconnect());
	}

	TEST_CASE("an agent migrates to another peer and back with its state") {
		g_recorder.clear();
		TestEnvironment l_home("home");
		TestEnvironment l_away("away");
		l_home.register_agent_type<Traveler>("Traveler");
		l_away.register_agent_type<Traveler>("Traveler");
		REQUIRE(connect(l_home, "home", listen_url(17450, "cam-test-migration")));
		REQUIRE(connect(l_away, "away", connect_url(17450, "cam-test-migration")));

		l_home.add<Traveler>("traveler", l_home.get_id(), l_away.get_id());
		l_home.add<Keeper>("keeper-home");
		l_away.add<Keeper>("keeper-away");

		const bool l_done = run_until({&l_away, &l_home}, [] { return recorded_prefix("traveler:done@"); }, 30s);
		CHECK(l_done);
		CHECK(g_recorder.count_of("traveler:setup@" + l_home.get_id()) == 2);
		CHECK(g_recorder.count_of("traveler:setup@" + l_away.get_id()) == 1);
		CHECK(g_recorder.count_of("traveler:done@" + l_home.get_id() + " counter=3") == 1);
		CHECK(l_home.statistics().m_migrations_completed == 2);
		CHECK(l_away.statistics().m_migrations_completed == 2);
		CHECK_FALSE(recorded_prefix("home:failure"));
		REQUIRE(l_home.disconnect());
		REQUIRE(l_away.disconnect());
	}

	TEST_CASE("two namespaces on the same peers never mix") {
		g_recorder.clear();
		TestEnvironment l_one_a("one-a");
		TestEnvironment l_one_b("one-b");
		TestEnvironment l_two("two");
		// Three sessions: one-a listens, the two others connect to it; "two" shares the links, not the namespace.
		REQUIRE(connect(l_one_a, "one-a", listen_url(17451, "cam-test-ns-one")));
		REQUIRE(connect(l_one_b, "one-b", connect_url(17451, "cam-test-ns-one")));
		REQUIRE(connect(l_two, "two", connect_url(17451, "cam-test-ns-two")));
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
		CHECK(l_two.environments().empty());
		REQUIRE(l_one_a.disconnect());
		REQUIRE(l_one_b.disconnect());
		REQUIRE(l_two.disconnect());
	}

	TEST_CASE("the facade refuses a zenoh URL without any endpoint") {
		const auto l_result = cam::transport::open("zenoh:///cam-test-none");
		REQUIRE_FALSE(l_result);
		CHECK(l_result.error().m_code == ErrorCode::InvalidConfiguration);
		const auto l_parsed = parse_url("zenoh://127.0.0.1:7447/lab?mode=client&listen=tcp/127.0.0.1:7448,tcp/127.0.0.1:7449");
		REQUIRE(l_parsed);
		CHECK(l_parsed.value().m_config.m_endpoint == "tcp/127.0.0.1:7447");
		CHECK(l_parsed.value().m_config.m_namespace == "lab");
		CHECK(l_parsed.value().m_options.size() == 2);
	}
}

int main(int argc, char** argv) {
	const auto l_schemes = cam::transport::schemes();
	if (std::find(l_schemes.begin(), l_schemes.end(), "zenoh") == l_schemes.end()) {
		std::cout << "SKIP: this build has no Zenoh transport (CAM_TRANSPORT_ZENOH)\n";
		return 77;
	}
	doctest::Context l_context(argc, argv);
	return l_context.run();
}
