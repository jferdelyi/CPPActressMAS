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
// oracle by iso.FictitiousPlay.
// Port of the C# example E15 Fictitious Play (ActressMAS v3.1): two players repeat the prisoner's dilemma for 50
// rounds; each plays the best response to the frequencies of its opponent's past actions, and the game agent pays them.
// Parallel, no turn limit. A player draws only on equal expected payoffs, never reached (defection dominates); the
// generators come from the seeds of the C# oracle.
// Arguments: none for the example; "sequential" for its sequential variant (fixed order).

#include <cmath>
#include <exception>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <DotNet.hpp>
#include <Environment.h>
#include <util/DotnetRandom.h>

struct Game {
	// Prisoner's Dilemma:
	//                 Opponent
	//               C         D
	//        C    (3,3)     (0,5)
	// Me:
	//        D    (5,0)     (1,1)

	static inline const std::string s_action_a = "C";  // cooperate
	static inline const std::string s_action_b = "D";  // defect

	static inline const std::vector<std::string> s_actions = {s_action_a, s_action_b};

	static double payoff(const std::string& p_my_action, const std::string& p_other_action) {
		// Mutual cooperation: (C, C) -> 3
		if (p_my_action == s_action_a && p_other_action == s_action_a) {
			return 3.0;
		}

		// Mutual defection: (D, D) -> 1
		if (p_my_action == s_action_b && p_other_action == s_action_b) {
			return 1.0;
		}

		// I cooperate, opponent defects: (C, D) -> 0
		if (p_my_action == s_action_a && p_other_action == s_action_b) {
			return 0.0;
		}

		// I defect, opponent cooperates: (D, C) -> 5
		if (p_my_action == s_action_b && p_other_action == s_action_a) {
			return 5.0;
		}

		throw std::invalid_argument("Unknown actions.");
	}
};

class GameAgent final : public cam::Agent {
	private:
		std::string m_player1_name;
		std::string m_player2_name;
		int m_max_rounds;

		int m_current_round = 0;
		std::map<std::string, std::optional<std::string>> m_current_actions;

		double m_total_payoff1 = 0;
		double m_total_payoff2 = 0;

		void start_next_round() {
			m_current_round++;

			if (m_current_round > m_max_rounds) {
				cam::dotnet::write_line("");
				cam::dotnet::write_line("Game finished.");
				cam::dotnet::write_line("Total payoff for " + m_player1_name + ": " + cam::dotnet::format_fixed(m_total_payoff1, 2));
				cam::dotnet::write_line("Total payoff for " + m_player2_name + ": " + cam::dotnet::format_fixed(m_total_payoff2, 2));

				send_by_name(m_player1_name, "stop");
				send_by_name(m_player2_name, "stop");

				stop();
				return;
			}

			cam::dotnet::write_line("");
			cam::dotnet::write_line("--- Round " + std::to_string(m_current_round) + " ---");

			m_current_actions[m_player1_name] = std::nullopt;
			m_current_actions[m_player2_name] = std::nullopt;

			send_by_name(m_player1_name, "play " + std::to_string(m_current_round));
			send_by_name(m_player2_name, "play " + std::to_string(m_current_round));
		}

		void handle_action(const std::string& p_sender, const std::string& p_parameters) {
			const std::string l_action = cam::dotnet::trim(p_parameters);
			m_current_actions[p_sender] = l_action;

			if (m_current_actions[m_player1_name] && m_current_actions[m_player2_name]) {
				const std::string l_a1 = *m_current_actions[m_player1_name];
				const std::string l_a2 = *m_current_actions[m_player2_name];

				const double l_payoff1 = Game::payoff(l_a1, l_a2);
				const double l_payoff2 = Game::payoff(l_a2, l_a1);

				m_total_payoff1 += l_payoff1;
				m_total_payoff2 += l_payoff2;

				cam::dotnet::write_line("Joint action: " + m_player1_name + "=" + l_a1 + ", " + m_player2_name + "=" + l_a2 + " | payoffs: " +
										cam::dotnet::format_fixed(l_payoff1, 2) + ", " + cam::dotnet::format_fixed(l_payoff2, 2));

				send_by_name(m_player1_name, "result " + l_a2 + " " + cam::dotnet::to_string(l_payoff1));
				send_by_name(m_player2_name, "result " + l_a1 + " " + cam::dotnet::to_string(l_payoff2));

				start_next_round();
			}
		}

	public:
		GameAgent(const std::string& p_name, const std::string& p_player1_name, const std::string& p_player2_name, const int p_max_rounds) :
				Agent(p_name),
				m_player1_name(p_player1_name),
				m_player2_name(p_player2_name),
				m_max_rounds(p_max_rounds) {}

		void setup() override {
			m_current_round = 0;
			m_current_actions.clear();

			cam::dotnet::write_line("Starting fictitious play for " + std::to_string(m_max_rounds) + " rounds between " + m_player1_name + " and " +
									m_player2_name + ".");

			start_next_round();
		}

		void action(const cam::MessagePointer& p_message) override {
			try {
				cam::dotnet::write_line("\t" + format(p_message));
				const auto [l_action, l_parameters] = p_message->parse_rest();

				if (l_action == "action") {
					handle_action(get_agent_name(p_message->get_sender()).value_or(""), l_parameters);
				}
			} catch (const std::exception& p_exception) {
				cam::dotnet::write_line(p_exception.what());
			}
		}
};

class PlayerAgent final : public cam::Agent {
	private:
		std::string m_game_agent_name;
		std::string m_opponent_name;
		// Dictionary<string, int> of the C#: insertion order, A then B
		std::vector<std::pair<std::string, int>> m_opponent_action_counts;
		std::vector<std::pair<std::string, int>> m_my_action_counts;
		cam::DotnetRandom m_rand;
		int m_round = 0;

		static int& count(std::vector<std::pair<std::string, int>>& p_counts, const std::string& p_action) {
			for (auto& [l_action, l_count]: p_counts) {
				if (l_action == p_action) {
					return l_count;
				}
			}
			return p_counts.emplace_back(p_action, 0).second;
		}

		static int total(const std::vector<std::pair<std::string, int>>& p_counts) {
			int l_total = 0;
			for (const auto& [l_action, l_count]: p_counts) {
				l_total += l_count;
			}
			return l_total;
		}

		void handle_play([[maybe_unused]] const std::string& p_parameters) {
			m_round++;

			const std::string l_chosen_action = choose_action();

			count(m_my_action_counts, l_chosen_action)++;

			cam::dotnet::write_line(get_name() + " (round " + std::to_string(m_round) + ") plays " + l_chosen_action);

			send_by_name(m_game_agent_name, "action " + l_chosen_action);
		}

		void print_strategy_probabilities() {
			// Belief about opponent strategy (based on counts, includes Laplace smoothing)
			const int l_opp_total = total(m_opponent_action_counts);

			const double l_opp_pa = static_cast<double>(count(m_opponent_action_counts, Game::s_action_a)) / l_opp_total;
			const double l_opp_pb = static_cast<double>(count(m_opponent_action_counts, Game::s_action_b)) / l_opp_total;

			// Empirical strategy based on my own play history (does not include smoothing)
			const int l_my_total = total(m_my_action_counts);

			const double l_my_pa = l_my_total > 0 ? static_cast<double>(count(m_my_action_counts, Game::s_action_a)) / l_my_total : 0.0;
			const double l_my_pb = l_my_total > 0 ? static_cast<double>(count(m_my_action_counts, Game::s_action_b)) / l_my_total : 0.0;

			cam::dotnet::write_line(get_name() + ": belief about " + m_opponent_name + ": P(A)=" + cam::dotnet::format_fixed(l_opp_pa, 2) + ", P(B)=" +
									cam::dotnet::format_fixed(l_opp_pb, 2) + " [counts A=" + std::to_string(count(m_opponent_action_counts, Game::s_action_a)) +
									", B=" + std::to_string(count(m_opponent_action_counts, Game::s_action_b)) + "]");

			cam::dotnet::write_line(get_name() + ": empirical strategy: P(A)=" + cam::dotnet::format_fixed(l_my_pa, 2) + ", P(B)=" +
									cam::dotnet::format_fixed(l_my_pb, 2) + " [counts A=" + std::to_string(count(m_my_action_counts, Game::s_action_a)) +
									", B=" + std::to_string(count(m_my_action_counts, Game::s_action_b)) + ", rounds=" + std::to_string(l_my_total) + "]");
		}

		void handle_result(const std::string& p_parameters) {
			// parameters: "<opponentAction> <myPayoff>"
			const auto l_toks = cam::dotnet::split(cam::dotnet::trim(p_parameters));

			if (!l_toks.empty()) {
				const std::string& l_opponent_action = l_toks[0];

				count(m_opponent_action_counts, l_opponent_action)++;

				double l_payoff = 0.0;
				if (l_toks.size() >= 2) {
					l_payoff = cam::dotnet::to_double(l_toks[1]);
				}

				cam::dotnet::write_line(get_name() + " observes opponent played " + l_opponent_action + ", payoff = " + cam::dotnet::format_fixed(l_payoff, 2));
			}
		}

		std::string choose_action() {
			const int l_total = total(m_opponent_action_counts);

			const double l_pa = static_cast<double>(count(m_opponent_action_counts, Game::s_action_a)) / l_total;
			const double l_pb = static_cast<double>(count(m_opponent_action_counts, Game::s_action_b)) / l_total;

			const double l_expected_a = l_pa * Game::payoff(Game::s_action_a, Game::s_action_a) + l_pb * Game::payoff(Game::s_action_a, Game::s_action_b);
			const double l_expected_b = l_pa * Game::payoff(Game::s_action_b, Game::s_action_a) + l_pb * Game::payoff(Game::s_action_b, Game::s_action_b);

			if (std::abs(l_expected_a - l_expected_b) < 1e-6) {  // tie: randomize
				return m_rand.next_double() < 0.5 ? Game::s_action_a : Game::s_action_b;
			}

			return l_expected_a > l_expected_b ? Game::s_action_a : Game::s_action_b;
		}

	public:
		PlayerAgent(const std::string& p_name, const std::string& p_game_agent_name, const std::string& p_opponent_name) :
				Agent(p_name),
				m_game_agent_name(p_game_agent_name),
				m_opponent_name(p_opponent_name),
				m_rand(cam::dotnet::next_site_seed("PlayerAgent.cs:36")) {}

		void setup() override {
			m_opponent_action_counts.clear();
			for (const auto& l_action: Game::s_actions) {
				count(m_opponent_action_counts, l_action) = 1;  // Laplace smoothing
			}

			m_my_action_counts.clear();
			for (const auto& l_action: Game::s_actions) {
				count(m_my_action_counts, l_action) = 0;
			}

			m_round = 0;
		}

		void action(const cam::MessagePointer& p_message) override {
			try {
				cam::dotnet::write_line("\t" + format(p_message));
				const auto [l_action, l_parameters] = p_message->parse_rest();

				if (l_action == "play") {
					handle_play(l_parameters);
				} else if (l_action == "result") {
					handle_result(l_parameters);
				} else if (l_action == "stop") {
					cam::dotnet::write_line(get_name() + ": stopping.");
					print_strategy_probabilities();
					stop();
				}
			} catch (const std::exception& p_exception) {
				cam::dotnet::write_line(p_exception.what());
			}
		}
};

int main(int argc, char** argv) {
	cam::Environment l_environment({.m_turns = 0,
									.m_mode = cam::dotnet::scenario_mode(argc, argv, cam::EnvironmentMasMode::Parallel),
									.m_seed = static_cast<unsigned int>(cam::dotnet::next_site_seed("EnvironmentMas.cs:61")),
									.m_random = cam::dotnet::random_engine()});

	const std::string l_player1_name = "player1";
	const std::string l_player2_name = "player2";
	const std::string l_game_name = "game";

	constexpr int l_rounds = 50;

	l_environment.add<GameAgent>(l_game_name, l_player1_name, l_player2_name, l_rounds);
	l_environment.add<PlayerAgent>(l_player1_name, l_game_name, l_player2_name);
	l_environment.add<PlayerAgent>(l_player2_name, l_game_name, l_player1_name);

	l_environment.start();
	return 0;
}
