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

// Harness program of the oracle tests (tests/iso): the C# original as it is, compared with its oracle by
// iso.RemoteMessages.
// Port of the C# example E20 RemoteMessages (ActressMAS v3.1): the ping agent of Container1 asks the pong agent of
// Container2 for information, by name, and the pong agent answers twice. The C# runs one process per container, each
// with its console, and a server between them; the server is not ported (the transport plays its part): the two
// containers are two environments of this process on the loopback transport, each named after its container, and
// their consoles are printed one after the other at the end. The C# containers never stop: they are stopped once the
// ping agent has its answers, as the processes of the oracle were.
// Arguments: [pause after each turn in ms] [transport URL] [container], by default 0 as the C#, the loopback transport
// and both containers. With an actressmas:// URL (a server, without container name), each container connects under its
// own name, as the C# containers do; a container named alone runs alone, against containers of other processes.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <DotNet.hpp>
#include <Environment.h>

// A container of the C#: an environment named after it, with the console of its process
class Container final : public cam::Environment {
	private:
		std::string m_name;
		mutable std::mutex m_console_mutex;
		std::string m_console;

	public:
		// Set once the scenario is over: the processes of the oracle are stopped from outside
		static inline std::atomic<bool> s_stop = false;

		Container(const std::string& p_name, const int p_pause, const std::string& p_url) :
				Environment({.m_mode = cam::EnvironmentMasMode::Parallel,
							 .m_delay_after_turn = p_pause,
							 .m_url = p_url,
							 .m_endpoint = {.m_name = p_name}}),
				m_name(p_name) {}

		// Environment.ContainerName
		[[nodiscard]] const std::string& container_name() const { return m_name; }

		// Console.Write in the process of this container
		void write(const std::string& p_text) {
			const std::lock_guard l_lock(m_console_mutex);
			m_console += p_text;
		}

		[[nodiscard]] std::string console() const {
			const std::lock_guard l_lock(m_console_mutex);
			return m_console;
		}

		// Once stopped, every agent leaves at the turn boundary and the run ends, without any output
		void turn_finished(int) override {
			if (s_stop) {
				for (const auto& l_id: get_filtered_agents("")) {
					remove(l_id);
				}
			}
		}
};

// The agents of a container write to its console
class ContainerAgent : public cam::Agent {
	protected:
		[[nodiscard]] Container& container() const {
			return *static_cast<Container*>(get_environment());
		}

		// Global.Rtb.AppendText: the text box of the form, the console of the reference without WinForms
		void append_text(const std::string& p_text) const {
			container().write(p_text);
		}

	public:
		explicit ContainerAgent(const std::string& p_name) : Agent(p_name) {}
};

class PingAgent final : public ContainerAgent {
	public:
		explicit PingAgent(const std::string& p_name) : ContainerAgent(p_name) {}

		void setup() override {
			append_text("[" + get_name() + "] Sending request\r\n");
			// "pong-agent@Container2": the agent of that name in the other environments
			cam::transport::SendOptions l_options;
			l_options.m_scope = cam::transport::Scope::Network;
			send_by_name("pong-agent", "request-info", true, l_options);
		}

		void action(const cam::MessagePointer& p_message) override {
			try {
				append_text("\t" + format(p_message) + "\r\n");
			} catch (const std::exception& p_exception) {
				append_text(std::string(p_exception.what()) + "\r\n");
			}
		}
};

class PongAgent final : public ContainerAgent {
	public:
		explicit PongAgent(const std::string& p_name) : ContainerAgent(p_name) {}

		void setup() override {
			append_text("[" + get_name() + "] Waiting for requests...\r\n");
		}

		void action(const cam::MessagePointer& p_message) override {
			try {
				append_text("\t" + format(p_message) + "\r\n");

				if (p_message->to_string() == "request-info") {
					std::string l_info = "Info 1 from agent " + get_name() + " in container " + container().container_name();
					send(p_message->get_sender(), l_info);

					l_info = "Info 2 from agent " + get_name() + " in container " + container().container_name();
					send(p_message->get_sender(), l_info);
				}
			} catch (const std::exception& p_exception) {
				append_text(std::string(p_exception.what()) + "\r\n");
			}
		}
};

// MasSetup.Configure
void configure(Container& p_env, const std::string& p_home) {
	if (p_home.empty()) {
		p_env.write("Container not activated.\r\n");
		return;
	}

	p_env.write("This is " + p_home + "\r\n");

	if (p_home == "Container1") {
		p_env.add<PingAgent>("ping-agent");
	} else if (p_home == "Container2") {
		p_env.add<PongAgent>("pong-agent");
	}
}

// The URL of a container: an ActressMAS server takes the name of the container, as the C# Container does
std::string url_of(const std::string& p_url, const std::string& p_container) {
	return p_url.rfind("actressmas://", 0) == 0 ? p_url + "/" + p_container : p_url;
}

int main(int argc, char** argv) {
	const int l_pause = argc > 1 ? std::atoi(argv[1]) : 0;
	const std::string l_url = argc > 2 ? argv[2] : "loopback://remote-messages";
	const std::string l_only = argc > 3 ? argv[3] : "";

	Container l_container1("Container1", l_pause, url_of(l_url, "Container1"));
	Container l_container2("Container2", l_pause, url_of(l_url, "Container2"));
	std::vector<Container*> l_running;  // Container2 first, as in the oracle
	for (Container* const l_container: {&l_container2, &l_container1}) {
		if (l_only.empty() || l_only == l_container->container_name()) {
			l_running.push_back(l_container);
		}
	}
	for (Container* const l_container: {&l_container1, &l_container2}) {
		if (std::find(l_running.begin(), l_running.end(), l_container) != l_running.end()) {
			configure(*l_container, l_container->container_name());
		}
	}

	// Both registered before either runs its MAS, as the containers of the oracle with their server
	for (Container* const l_container: l_running) {
		if (const auto l_result = l_container->connect(); !l_result) {
			cam::dotnet::write_line("Cannot connect " + l_container->container_name() + ": " + l_result.error().m_message);
			return 1;
		}
	}

	// RunMas in each container: Container2 first, as in the oracle
	std::vector<std::thread> l_threads;
	for (Container* const l_container: l_running) {
		l_threads.emplace_back(&Container::start, l_container);
	}

	// The scenario is over once the ping agent has both answers, in either order (the C# server may swap them), or, for
	// Container2 alone, once it has answered (at most 60 s)
	const bool l_ping_here = l_only.empty() || l_only == "Container1";
	const auto l_over = [&]() {
		if (!l_ping_here) {
			return l_container2.console().find("request-info") != std::string::npos;
		}
		const std::string l_console = l_container1.console();
		return l_console.find("Info 1 from") != std::string::npos && l_console.find("Info 2 from") != std::string::npos;
	};
	const auto l_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
	while (!l_over() && std::chrono::steady_clock::now() < l_deadline) {
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	Container::s_stop = true;
	for (auto& l_thread: l_threads) {
		l_thread.join();
	}

	// The console of each process
	for (const Container* const l_container: {&l_container1, &l_container2}) {
		if (std::find(l_running.begin(), l_running.end(), l_container) != l_running.end()) {
			cam::dotnet::write(l_container->console());
		}
	}
	return 0;
}
