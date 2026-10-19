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

// Harness program of the oracle tests (tests/iso): the C# original as it is, its bugs included, compared with its
// oracle by iso.MultipleMessages.
// Port of the C# example E00 Simple Examples/MultipleMessages (ActressMAS v3.1): a manager starts five workers; each
// reports to the manager and sends a request to another worker, drawn at random, which replies. Parallel, 20 turns.
// As the C#, the reply is handled under the label "request_reply", which no message carries: the reply is never
// handled and the exchange stops there.
// The workers draw from one generator, as from the static Random of the C#, with the seed of its oracle, but
// under a lock: the C# draws without one, which C++ cannot do without undefined behaviour.

#include <cstdint>
#include <mutex>
#include <string>

#include <DotNet.hpp>
#include <Environment.h>
#include <util/DotnetRandom.h>

class ManagerAgent final : public cam::Agent {
	public:
		explicit ManagerAgent(const std::string& p_name) : Agent(p_name) {}

		void setup() override {
			broadcast("start");
		}

		void action(const cam::MessagePointer& p_message) override {
			cam::dotnet::write_line(format(p_message));
			const auto [l_action, l_parameters] = p_message->parse();
			const std::string l_sender = get_agent_name(p_message->get_sender()).value_or("");

			if (l_action == "report") {
				cam::dotnet::write_line("\t[" + get_name() + "]: " + l_sender + " reporting");
			} else if (l_action == "reply") {
				cam::dotnet::write_line("\t[" + get_name() + "]: " + l_sender + " replying");
			}
		}
};

class WorkerAgent final : public cam::Agent {
	private:
		// Another worker, drawn from the generator shared by the workers, as the static Random of the C#
		[[nodiscard]] std::string next_worker() const {
			static std::mutex s_mutex;
			static cam::DotnetRandom s_rand(cam::dotnet::next_site_seed("WorkerAgent.cs:9"));
			constexpr std::int32_t l_no_workers = 5;
			std::string l_name = get_name();
			const std::scoped_lock l_lock(s_mutex);
			while (l_name == get_name()) {
				l_name = "Worker" + std::to_string(s_rand.next(l_no_workers));
			}
			return l_name;
		}

	public:
		explicit WorkerAgent(const std::string& p_name) : Agent(p_name) {}

		void action(const cam::MessagePointer& p_message) override {
			cam::dotnet::write_line(format(p_message));
			const auto [l_action, l_parameters] = p_message->parse();

			if (l_action == "start") {
				send_by_name("Manager", "report");
				send_by_name(next_worker(), "request");
			} else if (l_action == "request") {
				send(p_message->get_sender(), "reply");
			} else if (l_action == "request_reply") {  // as the C#: no message carries this label
				send_by_name("Manager", "reply");
				send_by_name(next_worker(), "request");
			}
		}
};

int main() {
	constexpr int l_no_workers = 5;
	cam::Environment l_environment({.m_turns = 20,
									.m_mode = cam::EnvironmentMasMode::Parallel,
									.m_seed = static_cast<unsigned int>(cam::dotnet::next_site_seed("EnvironmentMas.cs:61")),
									.m_random = cam::RandomEngine::DotNet});

	l_environment.add<ManagerAgent>("Manager");
	for (int i = 0; i < l_no_workers; i++) {
		l_environment.add<WorkerAgent>("Worker" + std::to_string(i));
	}

	l_environment.start();
	return 0;
}
