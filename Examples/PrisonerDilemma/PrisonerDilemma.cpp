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

// Port of the C# example E10 Iterated Prisoner Dilemma (ActressMAS v3.1): two prisoners play ten rounds against the
// police, which computes the outcome of each round; tit for tat cooperates first, then copies the other prisoner.
// The environment announces the rounds and tells the prisoners when the game ends.

#include <cstdint>
#include <exception>
#include <iostream>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include <DotNet.hpp>
#include <Environment.h>
#include <util/DotnetRandom.h>

class PrisonerAgent : public cam::Agent {
	private:
		void handle_outcome(const int p_outcome) {
			m_last_outcome = p_outcome;
			m_points += m_last_outcome;
		}

		void handle_turn() {
			send_by_name("police", "play " + choose_action(m_last_outcome));
		}

		void handle_end() {
			std::cout << "[" + get_name() + "]: I have " << m_points << " points" << std::endl;
			stop();
		}

	protected:
		int m_points = 0;
		int m_last_outcome = 0;

		virtual std::string choose_action(int p_last_outcome) = 0;

	public:
		explicit PrisonerAgent(const std::string& p_name) : Agent(p_name) {}

		void action(const cam::MessagePointer& p_message) override {
			try {
				std::cout << "\t" + format(p_message) << std::endl;
				const auto [l_action, l_parameters] = p_message->parse_rest();
				if (l_action == "outcome") {
					handle_outcome(cam::dotnet::to_int32(l_parameters));
				} else if (l_action == "end") {
					handle_end();
				}
			} catch (const std::exception& p_exception) {
				std::cout << p_exception.what() << std::endl;
			}
		}

		void default_action() override {
			handle_turn();
		}
};

class TitForTatPrisonerAgent final : public PrisonerAgent {
	protected:
		std::string choose_action(const int p_last_outcome) override {
			if (p_last_outcome == 0 || p_last_outcome == -1) {
				return "deny";  // initially, last outcome = 0 => cooperate first time
			}
			if (p_last_outcome == -3 || p_last_outcome == -5) {
				return "confess";
			}
			return "";
		}

	public:
		explicit TitForTatPrisonerAgent(const std::string& p_name) : PrisonerAgent(p_name) {}
};

class ConfessPrisonerAgent final : public PrisonerAgent {
	protected:
		std::string choose_action(int) override {
			return "confess";
		}

	public:
		explicit ConfessPrisonerAgent(const std::string& p_name) : PrisonerAgent(p_name) {}
};

class RandomPrisonerAgent final : public PrisonerAgent {
	private:
		// Unseeded in the C# (static Random)
		static inline cam::DotnetRandom s_random{static_cast<std::int32_t>(std::random_device()())};

	protected:
		std::string choose_action(int) override {
			return s_random.next_double() < 0.5 ? "confess" : "deny";
		}

	public:
		explicit RandomPrisonerAgent(const std::string& p_name) : PrisonerAgent(p_name) {}
};

class PoliceAgent final : public cam::Agent {
	private:
		// Plays of the round in their order of arrival (a C# Dictionary enumerates in insertion order)
		std::vector<std::pair<std::string, std::string>> m_responses;

		static void compute_outcome(const std::string& p_action1, const std::string& p_action2, int& p_outcome1, int& p_outcome2) {
			p_outcome1 = p_outcome2 = 0;
			if (p_action1 == "confess" && p_action2 == "confess") {
				p_outcome1 = p_outcome2 = -3;
			} else if (p_action1 == "deny" && p_action2 == "deny") {
				p_outcome1 = p_outcome2 = -1;
			} else if (p_action1 == "confess" && p_action2 == "deny") {
				p_outcome1 = 0;
				p_outcome2 = -5;
			} else if (p_action1 == "deny" && p_action2 == "confess") {
				p_outcome1 = -5;
				p_outcome2 = 0;
			}
		}

		void handle_play(const std::string& p_sender, const std::string& p_action) {
			m_responses.emplace_back(p_sender, p_action);
			if (m_responses.size() == 2) {
				const auto& [l_name1, l_action1] = m_responses[0];
				const auto& [l_name2, l_action2] = m_responses[1];
				int l_outcome1 = 0;
				int l_outcome2 = 0;
				compute_outcome(l_action1, l_action2, l_outcome1, l_outcome2);
				send_by_name(l_name1, "outcome " + std::to_string(l_outcome1));
				send_by_name(l_name2, "outcome " + std::to_string(l_outcome2));
				m_responses.clear();
			}
		}

	public:
		explicit PoliceAgent(const std::string& p_name) : Agent(p_name) {}

		void action(const cam::MessagePointer& p_message) override {
			try {
				std::cout << "\t" + format(p_message) << std::endl;
				const auto [l_play, l_chosen_action] = p_message->parse_one();  // l_play = "play"
				handle_play(get_agent_name(p_message->get_sender()).value_or(""), l_chosen_action);
			} catch (const std::exception& p_exception) {
				std::cout << p_exception.what() << std::endl;
			}
		}
};

// Two turns per round (the plays, then the outcomes), plus the setup and the end
class IPDEnvironment final : public cam::Environment {
	private:
		int m_round_turns;

	public:
		explicit IPDEnvironment(const int p_rounds) :
				Environment({.m_turns = p_rounds * 2 + 2, .m_mode = cam::EnvironmentMasMode::Sequential}),
				m_round_turns(p_rounds * 2) {}

		void turn_finished(const int p_turn) override {
			if (p_turn < m_round_turns && p_turn % 2 == 0) {
				std::cout << "Round " << p_turn / 2 + 1 << std::endl;
			}
			if (p_turn == m_round_turns - 1) {
				for (const auto& l_prisoner: get_filtered_agents("Prisoner")) {
					send_content("env", l_prisoner, std::string("end"));
				}
			}
		}
};

int main() {
	IPDEnvironment l_environment(10);
	l_environment.add<TitForTatPrisonerAgent>("p1-TitForTatPrisonerAgent");
	l_environment.add<TitForTatPrisonerAgent>("p2-TitForTatPrisonerAgent");
	// l_environment.add<ConfessPrisonerAgent>("p1-ConfessPrisonerAgent");
	// l_environment.add<RandomPrisonerAgent>("p2-RandomPrisonerAgent");
	l_environment.add<PoliceAgent>("police");
	l_environment.start();
	return 0;
}
