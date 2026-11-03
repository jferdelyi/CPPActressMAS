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

// Harness program of the oracle tests (tests/iso): the C# original as it is, its bugs included, compared with its
// oracle by iso.Agents1.
// Port of the C# example E00 Simple Examples/Agents1 (ActressMAS v3.1): two agents send ten messages each to a
// monitor during their setup, with random pauses of 10 to 99 ms; the monitor prints what it receives. Parallel,
// 100 turns. The pauses come from the seed of the C# oracle; both agents draw from one generator, as from the
// static Random of the C#, but under a lock: the C# draws without one, which C++ cannot do without undefined behaviour.
// Arguments: none for the example; "sequential" for its sequential variant (fixed order).

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

#include <DotNet.hpp>
#include <Environment.h>
#include <util/DotnetRandom.h>

class MyAgent final : public cam::Agent {
	private:
		// Shared by the agents, as the static Random of the C#
		static std::int32_t next_pause() {
			static std::mutex s_mutex;
			static cam::DotnetRandom s_rand(cam::dotnet::next_site_seed("Program.cs:36"));
			const std::scoped_lock l_lock(s_mutex);
			return 10 + s_rand.next(90);
		}

	public:
		explicit MyAgent(const std::string& p_name) : Agent(p_name) {}

		void setup() override {
			cam::dotnet::write_line(get_name() + " starting");
			for (int i = 1; i <= 10; i++) {
				cam::dotnet::write_line(get_name() + " sending " + std::to_string(i));
				send_by_name("monitor", std::to_string(i));
				const std::int32_t l_dt = next_pause();
				std::this_thread::sleep_for(std::chrono::milliseconds(l_dt));  // milliseconds
			}
		}
};

class MonitorAgent final : public cam::Agent {
	public:
		explicit MonitorAgent(const std::string& p_name) : Agent(p_name) {}

		void action(const cam::MessagePointer& p_message) override {
			cam::dotnet::write_line(get_name() + " has received " + format(p_message));
		}
};

int main(int argc, char** argv) {
	cam::Environment l_environment({.m_turns = 100,
									.m_mode = cam::dotnet::scenario_mode(argc, argv, cam::EnvironmentMasMode::Parallel),
									.m_seed = static_cast<unsigned int>(cam::dotnet::next_site_seed("EnvironmentMas.cs:61")),
									.m_random = cam::dotnet::random_engine()});

	l_environment.add<MyAgent>("agent1");
	l_environment.add<MyAgent>("agent2");
	l_environment.add<MonitorAgent>("monitor");

	l_environment.start();
	return 0;
}
