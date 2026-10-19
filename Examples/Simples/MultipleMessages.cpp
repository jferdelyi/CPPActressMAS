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

// Port of the C# example E00 Simple Examples/MultipleMessages (ActressMAS v3.1): a manager starts five workers; each
// reports to the manager and sends a request to another worker, drawn at random, which replies. Parallel, 20 turns.
// WARNING: the C# handles the reply under the label "request_reply", which no message carries (X1): the reply is never
// handled and the exchange stops there. "legacy", the default, keeps this bug; "fixed" handles the reply as the C#
// meant to (D89): the worker tells the manager, which prints "replying", and sends a new request, until the last turn.
// The workers draw from one generator, as from the static Random of the C#, with the seed of its oracle (D96), but
// under a lock: the C# draws without one, which C++ cannot do without undefined behaviour.
// Arguments: [legacy|fixed].

#include <cstdint>
#include <iostream>
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
		bool m_fixed;

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
		WorkerAgent(const std::string& p_name, const bool p_fixed) :
				Agent(p_name),
				m_fixed(p_fixed) {}

		void action(const cam::MessagePointer& p_message) override {
			cam::dotnet::write_line(format(p_message));
			const auto [l_action, l_parameters] = p_message->parse();

			if (l_action == "start") {
				send_by_name("Manager", "report");
				send_by_name(next_worker(), "request");
			} else if (l_action == "request") {
				send(p_message->get_sender(), "reply");
			} else if (l_action == (m_fixed ? "reply" : "request_reply")) {
				// Legacy: "request_reply", as the C#, never matches; fixed: the reply is handled
				send_by_name("Manager", "reply");
				send_by_name(next_worker(), "request");
			}
		}
};

int main(int argc, char** argv) {
	const std::string l_mode = argc > 1 ? argv[1] : "legacy";
	if (l_mode != "legacy" && l_mode != "fixed") {
		std::cerr << "usage: MultipleMessages [legacy|fixed]" << std::endl;
		return 2;
	}
	const bool l_fixed = l_mode == "fixed";
	if (!l_fixed) {
		std::cerr << "WARNING: legacy mode, the replies are never handled, as in the C# (see the README); \"fixed\" corrects it" << std::endl;
	}

	constexpr int l_no_workers = 5;
	cam::Environment l_environment({.m_turns = 20,
									.m_mode = cam::EnvironmentMasMode::Parallel,
									.m_seed = static_cast<unsigned int>(cam::dotnet::next_site_seed("EnvironmentMas.cs:61")),
									.m_random = cam::RandomEngine::DotNet});

	l_environment.add<ManagerAgent>("Manager");
	for (int i = 0; i < l_no_workers; i++) {
		l_environment.add<WorkerAgent>("Worker" + std::to_string(i), l_fixed);
	}

	l_environment.start();
	return 0;
}
