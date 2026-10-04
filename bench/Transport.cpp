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

// End-to-end transport benchmark (docs/port/perf/PROTOCOLE.md): a ping agent in environment A and a
// pong agent in environment B, each environment its own node of the transport, exchange round trips by
// id through the directory. A round trip takes at least two turns (a remote message reaches its agent at
// the turn after its reception), so the figure measures the transport path, not the local one.
// Usage: bench_Transport [--transport loopback|mqtt|zenoh] [--url-a URL] [--url-b URL] [--pings N] [--trials N]
// Exit code 77 when the transport is not compiled in or the broker does not answer (skipped).

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#include <Environment.h>
#include <transport/Facade.h>
#include <transport/LoopbackTransport.h>

#include "common.hpp"

namespace {

	using namespace std::chrono_literals;

	struct Settings {
		std::string m_transport = "loopback";
		std::string m_url_a;
		std::string m_url_b;
		long m_pings = 2000;
		int m_trials = 5;
	};

	std::atomic<long> g_received{0};
	std::atomic<bool> g_done{false};

	// Answers every ping with a pong to its sender.
	class Pong final : public cam::Agent {
	public:
		explicit Pong(const std::string& p_name) : Agent(p_name) {}
		void action(const cam::MessagePointer& p_message) override {
			(void) send(p_message->get_sender(), p_message->get_binary_message().data(), p_message->get_binary_message().size());
		}
		void default_action() override {
			if (g_done.load()) stop();
		}
	};

	// Sends a ping, waits for the pong, sends the next one; stops after N round trips (or when asked).
	class Ping final : public cam::Agent {
	public:
		Ping(const std::string& p_name, std::string p_pong_id, const long p_pings) :
				Agent(p_name), m_pong_id(std::move(p_pong_id)), m_pings(p_pings) {}
		void action(const cam::MessagePointer&) override {
			g_received.fetch_add(1, std::memory_order_relaxed);
			if (++m_count >= m_pings || g_done.load()) {
				g_done.store(true);
				stop();
				return;
			}
			ping();
		}
		void default_action() override {
			if (g_done.load()) {
				stop();
				return;
			}
			if (!m_started && ping()) {
				m_started = true;  // the directory knows the pong agent: the first ping is out
			}
		}
	private:
		bool ping() {
			static const std::string s_payload = "ping";
			return static_cast<bool>(send(m_pong_id, reinterpret_cast<const std::uint8_t*>(s_payload.data()), s_payload.size()));
		}
		std::string m_pong_id;
		long m_pings;
		long m_count = 0;
		bool m_started = false;
	};

	// Opens the transport of one environment according to the settings; the loopback is shared.
	cam::transport::Result<void> connect(cam::Environment& p_environment, const Settings& p_settings, const std::string& p_url,
										 const std::shared_ptr<cam::transport::Transport>& p_loopback) {
		if (p_settings.m_transport == "loopback") {
			return p_environment.connect(p_loopback);
		}
		return cam::transport::connect(p_environment, p_url);
	}

	bool has_scheme(const std::string& p_scheme) {
		for (const auto& l_scheme: cam::transport::schemes()) {
			if (l_scheme == p_scheme) return true;
		}
		return false;
	}

} // namespace

int main(int argc, char** argv) {
	Settings l_settings;
	for (int i = 1; i < argc; i++) {
		const std::string l_argument = argv[i];
		const char* l_value = i + 1 < argc ? argv[i + 1] : nullptr;
		if (l_argument == "--transport" && l_value) l_settings.m_transport = argv[++i];
		else if (l_argument == "--url-a" && l_value) l_settings.m_url_a = argv[++i];
		else if (l_argument == "--url-b" && l_value) l_settings.m_url_b = argv[++i];
		else if (l_argument == "--pings" && l_value) l_settings.m_pings = std::atol(argv[++i]);
		else if (l_argument == "--trials" && l_value) l_settings.m_trials = std::atoi(argv[++i]);
		else {
			std::cerr << "usage: bench_Transport [--transport loopback|mqtt|zenoh] [--url-a URL] [--url-b URL] [--pings N] [--trials N]\n";
			return 2;
		}
	}
	if (l_settings.m_transport == "mqtt" && l_settings.m_url_a.empty()) {
		const char* l_host = std::getenv("CAM_MQTT_HOST");
		const char* l_port = std::getenv("CAM_MQTT_PORT");
		l_settings.m_url_a = std::string("mqtt://") + (l_host ? l_host : "127.0.0.1") + ":" + (l_port ? l_port : "1883") + "/cam-bench";
		l_settings.m_url_b = l_settings.m_url_a;
	}
	if (l_settings.m_transport == "zenoh" && l_settings.m_url_a.empty()) {
		l_settings.m_url_a = "zenoh://?listen=tcp/127.0.0.1:17460&ns=cam-bench";
		l_settings.m_url_b = "zenoh://127.0.0.1:17460/cam-bench";
	}
	if (l_settings.m_url_b.empty()) {
		l_settings.m_url_b = l_settings.m_url_a;
	}
	if (l_settings.m_transport != "loopback" && !has_scheme(l_settings.m_transport)) {
		std::cout << "skipped: this build has no " << l_settings.m_transport << " transport\n";
		return 77;
	}

	std::cout << "Transport " << l_settings.m_transport << ", " << l_settings.m_pings << " round trips per trial" << std::endl;
	double l_total_rate = 0.0;
	for (int l_trial = 1; l_trial <= l_settings.m_trials; l_trial++) {
		g_received = 0;
		g_done = false;
		// One transport instance per environment (two nodes), opened outside the timed zone.
		std::shared_ptr<cam::transport::Transport> l_loopback;
		if (l_settings.m_transport == "loopback") {
			cam::transport::LoopbackOptions l_options;
			l_options.m_serialize = true;
			l_loopback = std::make_shared<cam::transport::LoopbackTransport>(cam::transport::TransportConfig{}, l_options);
			if (!l_loopback->open()) return 1;
		}
		cam::Environment l_environment_a(0, cam::EnvironmentMasMode::Sequential);
		cam::Environment l_environment_b(0, cam::EnvironmentMasMode::Sequential);
		// A first: with Zenoh it is the listening peer B connects to.
		if (const auto l_result = connect(l_environment_a, l_settings, l_settings.m_url_a, l_loopback); !l_result) {
			std::cout << "skipped: " << l_result.error().m_message << std::endl;
			return 77;
		}
		if (const auto l_result = connect(l_environment_b, l_settings, l_settings.m_url_b, l_loopback); !l_result) {
			std::cout << "skipped: " << l_result.error().m_message << std::endl;
			return 77;
		}
		const std::string l_pong = l_environment_b.add<Pong>("pong");
		l_environment_a.add<Ping>("ping", l_pong, l_settings.m_pings);

		// Watchdog: a trial that does not finish within 300 s is abandoned (the keepers stop the environments).
		std::atomic<bool> l_finished{false};
		std::thread l_watchdog([&] {
			for (int i = 0; i < 3000 && !l_finished.load(); i++) {
				std::this_thread::sleep_for(100ms);
			}
			if (!l_finished.load()) {
				g_done.store(true);
			}
		});
		const bench::Stopwatch l_stopwatch;
		std::thread l_thread_b(&cam::Environment::start, &l_environment_b);
		std::thread l_thread_a(&cam::Environment::start, &l_environment_a);
		l_thread_a.join();
		l_thread_b.join();
		const double l_milliseconds = static_cast<double>(l_stopwatch.elapsed_us()) / 1000.0;
		l_finished.store(true);
		l_watchdog.join();
		if (g_received.load() < l_settings.m_pings) {
			std::cout << "skipped: only " << g_received.load() << " round trips within 300 s" << std::endl;
			return 77;
		}

		const long l_messages = g_received.load() * 2;  // a ping and a pong per round trip
		const double l_rate = l_milliseconds > 0.0 ? static_cast<double>(l_messages) / l_milliseconds : 0.0;
		l_total_rate += l_rate;
		std::cout << "Trial " << l_trial << "\n" << l_messages << " msg\n" << static_cast<long>(l_milliseconds) << " ms\n"
				  << (l_milliseconds * 1000.0 / static_cast<double>(g_received.load() > 0 ? g_received.load() : 1)) << " us per round trip\n" << std::endl;
		(void) l_environment_a.disconnect();
		(void) l_environment_b.disconnect();
	}
	std::cout << (l_total_rate / l_settings.m_trials) << " msg/ms\n";
	return 0;
}
