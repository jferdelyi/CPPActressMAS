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
// iso.MasMobileConsole.
// Port of the C# example E20 MasMobileConsole (ActressMAS v3.1): a mobile agent leaves Container1, visits the other
// containers, asks their static agents for information, then comes home and prints its report. The C# runs one process
// per container, each with its console, and a server between them; the server is not ported (the transport plays its
// part): the three containers are three environments of this process on the loopback transport, each named after its
// container, and their consoles are printed one after the other at the end. The C# containers never stop: they are
// stopped once the mobile agent has printed its report, as the processes of the oracle were.
// Arguments: [pause after each turn in ms], 0 by default as the C#.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <deque>
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

		Container(const std::string& p_name, const int p_pause) :
				Environment({.m_mode = cam::EnvironmentMasMode::Parallel,
							 .m_delay_after_turn = p_pause,
							 .m_url = "loopback://mas-mobile",
							 .m_endpoint = {.m_name = p_name}}),
				m_name(p_name) {}

		// Environment.ContainerName
		[[nodiscard]] const std::string& container_name() const { return m_name; }

		// Environment.AllContainers: the containers the server knows, this one included, in the order they registered
		// (Container1 to Container3 in the oracle); here the containers known through the transport, in the order of
		// their names
		[[nodiscard]] std::vector<std::string> all_containers() const {
			std::vector<std::string> l_names{m_name};
			for (const auto& l_info: environments()) {
				if (l_info.m_online) {
					l_names.push_back(l_info.m_name);
				}
			}
			std::sort(l_names.begin(), l_names.end());
			return l_names;
		}

		// The id of a container known through the transport, empty if unknown
		[[nodiscard]] std::string container_id(const std::string& p_name) const {
			for (const auto& l_info: environments()) {
				if (l_info.m_online && l_info.m_name == p_name) {
					return l_info.m_id;
				}
			}
			return {};
		}

		// Console.WriteLine in the process of this container
		void write_line(const std::string& p_text) {
			const std::lock_guard l_lock(m_console_mutex);
			m_console += p_text + "\n";
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

		void write_line(const std::string& p_text) const {
			container().write_line(p_text);
		}

		// Agent.CanMove
		[[nodiscard]] bool can_move(const std::string& p_destination) const {
			const std::vector<std::string> l_containers = container().all_containers();
			return !container().container_name().empty() && p_destination != container().container_name() &&
				   std::find(l_containers.begin(), l_containers.end(), p_destination) != l_containers.end();
		}

		// Agent.Move: to the container of that name
		void move_to(const std::string& p_destination) {
			if (const auto l_result = move(container().container_id(p_destination)); !l_result) {
				write_line("Move refused: " + l_result.error().m_message);
			}
		}

	public:
		explicit ContainerAgent(const std::string& p_name) : Agent(p_name) {}
};

class StaticAgent final : public ContainerAgent {
	private:
		std::string m_info;

	public:
		explicit StaticAgent(const std::string& p_name) : ContainerAgent(p_name) {}

		void setup() override {
			write_line("Static agent " + get_name() + " starting");
			m_info = "Info from agent " + get_name() + " in container " + container().container_name();
		}

		void action(const cam::MessagePointer& p_message) override {
			write_line("\t" + format(p_message));

			if (p_message->to_string() == "request-info") {
				send(p_message->get_sender(), m_info);
			}
		}
};

class MobileAgent final : public ContainerAgent {
	private:
		std::string m_log;
		std::deque<std::string> m_moves;
		bool m_first_start = true;
		int m_turns_to_wait_for_info = 0;

	public:
		explicit MobileAgent(const std::string& p_name) : ContainerAgent(p_name) {}

		void setup() override {
			if (m_first_start) {
				write_line("Mobile agent starting at home");

				m_first_start = false;

				m_log = "\r\nReporting:\r\n";

				m_moves.clear();

				for (const std::string& l_cn: container().all_containers()) {
					if (l_cn != container().container_name()) {  // home
						m_moves.push_back(l_cn);
					}
				}

				m_moves.push_back(container().container_name());  // return home, get local info and report

				for (const std::string& l_m: m_moves) {
					write_line(l_m);
				}
			} else {
				write_line("I have moved to " + container().container_name());
				m_log += "Arrived to " + container().container_name() + "\r\n";
				broadcast("request-info");
				m_turns_to_wait_for_info = 3;
			}
		}

		void action(const cam::MessagePointer& p_message) override {
			try {
				write_line("\t" + format(p_message));
				m_log += "Received info: " + p_message->to_string() + "\r\n";  // info from static agents
			} catch (const std::exception& p_exception) {
				write_line(p_exception.what());
			}
		}

		void default_action() override {
			if (m_turns_to_wait_for_info-- > 0) {
				return;
			}

			if (!m_moves.empty()) {
				const std::string l_next_destination = m_moves.front();
				m_moves.pop_front();

				if (can_move(l_next_destination)) {
					write_line("I'm moving to " + l_next_destination);
					m_log += "Moving to " + l_next_destination + "\r\n";
					move_to(l_next_destination);
					return;
				}
			}

			m_log += "Stopping\r\n";
			write_line(m_log);
			stop();
		}

		[[nodiscard]] json save_state() const override {
			return {{"first_start", m_first_start}, {"log", m_log}, {"moves", m_moves}};
		}

		void load_state(const json& p_state) override {
			m_first_start = p_state.at("first_start").get<bool>();
			m_log = p_state.at("log").get<std::string>();
			m_moves = p_state.at("moves").get<std::deque<std::string>>();
		}
};

// MasSetup.Configure
void configure(Container& p_env, const std::string& p_container_name) {
	if (p_container_name.empty()) {
		p_env.write_line("Container not activated.");
		return;
	}

	p_env.write_line("This is " + p_container_name);

	if (p_container_name == "Container1") {
		p_env.add<MobileAgent>("mobile-k1");

		for (int i = 1; i <= 1; i++) {
			p_env.add<StaticAgent>("a" + std::to_string(i) + "-k1");
		}
	} else if (p_container_name == "Container2") {
		for (int i = 1; i <= 2; i++) {
			p_env.add<StaticAgent>("b" + std::to_string(i) + "-k1");
		}
	} else if (p_container_name == "Container3") {
		for (int i = 1; i <= 3; i++) {
			p_env.add<StaticAgent>("c" + std::to_string(i) + "-k3");
		}
	}
}

int main(int argc, char** argv) {
	const int l_pause = argc > 1 ? std::atoi(argv[1]) : 0;

	// Program.Main of each container, ENTER pressed at once: the console prompt, then the agents
	Container l_container1("Container1", l_pause);
	Container l_container2("Container2", l_pause);
	Container l_container3("Container3", l_pause);
	for (Container* const l_container: {&l_container1, &l_container2, &l_container3}) {
		l_container->write_line("Press ENTER to start MAS in container " + l_container->container_name());
		configure(*l_container, l_container->container_name());
	}

	// All registered before any runs its MAS, as the containers of the oracle with their server: each knows the others
	// at its first turn
	for (Container* const l_container: {&l_container1, &l_container2, &l_container3}) {
		if (const auto l_result = l_container->connect(); !l_result) {
			cam::dotnet::write_line("Cannot connect " + l_container->container_name() + ": " + l_result.error().m_message);
			return 1;
		}
	}

	// RunMas in each container
	std::thread l_thread1(&Container::start, &l_container1);
	std::thread l_thread2(&Container::start, &l_container2);
	std::thread l_thread3(&Container::start, &l_container3);

	// The scenario is over once the mobile agent has printed its report at home (at most 60 s)
	const auto l_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
	while (l_container1.console().find("Stopping") == std::string::npos && std::chrono::steady_clock::now() < l_deadline) {
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	Container::s_stop = true;
	l_thread1.join();
	l_thread2.join();
	l_thread3.join();

	// The console of each process
	for (const Container* const l_container: {&l_container1, &l_container2, &l_container3}) {
		cam::dotnet::write(l_container->console());
	}
	return 0;
}
