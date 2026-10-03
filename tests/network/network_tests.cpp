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

// Network behaviour that must survive the communication refactoring (mission 1): delivery to a remote
// agent, discovery of the other environments, migration there and back. Two environments of this
// process talk through an MQTT broker given by CAM_MQTT_HOST and CAM_MQTT_PORT; without them, or if the
// port does not answer, the whole executable skips with exit code 77 (CTest SKIP_RETURN_CODE).
// Every wait is a condition with a deadline, never a fixed sleep.

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <Environment.h>

#include <cereal/types/base_class.hpp>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#include "Recorder.hpp"
#include "tcp_probe.hpp"
#include "wait_until.hpp"

namespace {

	using namespace std::chrono_literals;
	using Mode = cam::EnvironmentMasMode;

	std::string g_host;
	int g_port = 0;
	cam::test::Recorder g_recorder;

	constexpr int TURN_DELAY_MS = 20;
	constexpr int MAX_TURNS = 400;  // 8 s of simulation at most per environment

	// Keeps its environment alive until the test raises the stop flag or MAX_TURNS turns elapsed, then
	// stops so that start() returns. The test never removes an agent from outside during a turn (C10, C12).
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

	// Sends one message at setup to an agent of another environment, by id; stops with the keepers.
	struct RemoteSender final : cam::Agent {
		RemoteSender(const std::string& p_name, std::string p_target_id) : Agent(p_name), m_target_id(std::move(p_target_id)) {}
		void setup() override { send(m_target_id, json("hello from afar")); }
		void default_action() override {
			if (g_stop_keepers.load()) {
				stop();
			}
		}
	private:
		std::string m_target_id;
	};

	// Travels to the other environment and back, carrying a counter, then reports.
	struct Traveler final : cam::Agent {
		Traveler(const std::string& p_name, std::string p_home_id, std::string p_away_id) :
				Agent(p_name), m_home_id(std::move(p_home_id)), m_away_id(std::move(p_away_id)) {}
		Traveler() : Agent() {}
		int m_hops = 0;
		int m_counter = 0;

		void setup() override { g_recorder.record(get_name() + ":setup@" + get_environment()->get_id()); }

		void default_action() override {
			++m_counter;
			if (m_hops == 0) {
				m_hops = 1;
				move<Traveler>(m_away_id, json("outbound"));
			} else if (m_hops == 1) {
				m_hops = 2;
				move<Traveler>(m_home_id, json("inbound"));
			} else {
				g_recorder.record(get_name() + ":done@" + get_environment()->get_id() + " counter=" + std::to_string(m_counter));
				stop();
			}
		}

		template<class Archive>
		void save(Archive& p_archive) const {
			p_archive(cereal::base_class<cam::Agent>(this), m_home_id, m_away_id, m_hops, m_counter);
		}
		template<class Archive>
		void load(Archive& p_archive) {
			p_archive(cereal::base_class<cam::Agent>(this), m_home_id, m_away_id, m_hops, m_counter);
		}

	private:
		std::string m_home_id;
		std::string m_away_id;
	};

	// Environment that rebuilds a Traveler on arrival (the library has no type registry, N7).
	struct TravelEnvironment final : cam::Environment {
		using cam::Environment::Environment;
	protected:
		void on_new_agent(const json& p_message) override {
			const std::string l_blob = p_message["agent"];
			std::stringstream l_stream(l_blob);
			add<Traveler>(l_stream);
		}
	};

	int connect(cam::Environment& p_env, const std::string& p_client_id) {
		return p_env.initialise_remote_connection(p_client_id, g_host, g_port);
	}

} // namespace

TEST_SUITE("mqtt") {

	TEST_CASE("a message sent by id to an agent of another environment is delivered through the broker") {
		g_recorder.clear();
		cam::Environment l_env_a(0, Mode::Sequential, TURN_DELAY_MS);
		cam::Environment l_env_b(0, Mode::Sequential, TURN_DELAY_MS);
		REQUIRE(connect(l_env_a, "cam-test-delivery-a") == 0);
		REQUIRE(connect(l_env_b, "cam-test-delivery-b") == 0);

		const std::string l_receiver = l_env_b.add<Receiver>("receiver");
		l_env_b.add<Keeper>("keeper-b");
		l_env_a.add<RemoteSender>("sender", l_receiver);
		l_env_a.add<Keeper>("keeper-a");

		g_stop_keepers = false;
		std::thread l_thread_b(&cam::Environment::start, &l_env_b);
		std::thread l_thread_a(&cam::Environment::start, &l_env_a);
		const bool l_delivered = cam::test::wait_until([] { return g_recorder.count() >= 1; }, 10s);
		g_stop_keepers = true;
		l_thread_a.join();
		l_thread_b.join();

		CHECK(l_delivered);
		CHECK(g_recorder.count_of("receiver:\"hello from afar\"") == 1);
	}

	TEST_CASE("an environment discovers the other environments connected to the broker") {
		cam::Environment l_env_a(0, Mode::Sequential, TURN_DELAY_MS);
		cam::Environment l_env_b(0, Mode::Sequential, TURN_DELAY_MS);
		REQUIRE(connect(l_env_a, "cam-test-discovery-a") == 0);
		REQUIRE(connect(l_env_b, "cam-test-discovery-b") == 0);

		const bool l_seen_by_a = cam::test::wait_until([&] { return l_env_a.get_containers().count(l_env_b.get_id()) == 1; }, 10s);
		const bool l_seen_by_b = cam::test::wait_until([&] { return l_env_b.get_containers().count(l_env_a.get_id()) == 1; }, 10s);
		CHECK(l_seen_by_a);
		CHECK(l_seen_by_b);
		CHECK_FALSE(l_env_a.get_id().empty());
		CHECK(l_env_a.get_id() != l_env_b.get_id());
	}

	TEST_CASE("an agent migrates to another environment and back with its state") {
		g_recorder.clear();
		TravelEnvironment l_home(0, Mode::Sequential, TURN_DELAY_MS);
		TravelEnvironment l_away(0, Mode::Sequential, TURN_DELAY_MS);
		REQUIRE(connect(l_home, "cam-test-migration-home") == 0);
		REQUIRE(connect(l_away, "cam-test-migration-away") == 0);
		REQUIRE(cam::test::wait_until([&] { return l_home.get_containers().count(l_away.get_id()) == 1; }, 10s));

		l_home.add<Traveler>("traveler", l_home.get_id(), l_away.get_id());
		l_home.add<Keeper>("keeper-home");
		l_away.add<Keeper>("keeper-away");

		g_stop_keepers = false;
		std::thread l_thread_away(&cam::Environment::start, &l_away);
		std::thread l_thread_home(&cam::Environment::start, &l_home);
		const bool l_done = cam::test::wait_until([] {
			for (const auto& l_entry: g_recorder.entries()) {
				if (l_entry.rfind("traveler:done@", 0) == 0) {
					return true;
				}
			}
			return false;
		}, 20s);
		g_stop_keepers = true;
		l_thread_home.join();
		l_thread_away.join();

		CHECK(l_done);
		CHECK(g_recorder.count_of("traveler:setup@" + l_home.get_id()) == 2);  // departure and return
		CHECK(g_recorder.count_of("traveler:setup@" + l_away.get_id()) == 1);
		CHECK(g_recorder.count_of("traveler:done@" + l_home.get_id() + " counter=3") == 1);  // state carried over
	}
}

int main(int argc, char** argv) {
	const char* l_host = std::getenv("CAM_MQTT_HOST");
	const char* l_port = std::getenv("CAM_MQTT_PORT");
	if (!l_host || !l_port) {
		std::cout << "SKIP: CAM_MQTT_HOST and CAM_MQTT_PORT are not set; the MQTT tests need a broker\n";
		return 77;
	}
	g_host = l_host;
	g_port = std::atoi(l_port);
	if (!cam::test::tcp_port_open(g_host, g_port, 2s)) {
		std::cout << "SKIP: no broker answers on " << g_host << ":" << g_port << "\n";
		return 77;
	}
	doctest::Context l_context(argc, argv);
	return l_context.run();
}
