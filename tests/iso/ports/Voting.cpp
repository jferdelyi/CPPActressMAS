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
// oracle by iso.Voting.
// Port of the C# example E14 Voting (ActressMAS v3.1): 99 voters with a random, complete or partial, preference order
// over 5 candidates vote; the teller prints every vote, then the Condorcet winner if there is one (Copeland's method)
// and the winner of the Borda count. Sequential, fixed order, no turn limit; the preferences come from the seed of the
// C# oracle, drawn from one generator shared by the voters, as the static Random of the C#.

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <string>
#include <vector>

#include <DotNet.hpp>
#include <Environment.h>
#include <util/DotnetRandom.h>

class VoterAgent final : public cam::Agent {
	private:
		std::string m_preference_order;
		cam::DotnetRandom& m_rand;

		// As Enumerable.Range(0, n).OrderBy(x => _rand.Next()): one key drawn per element in order, then a stable sort
		std::vector<int> random_order_permutation(const int p_n) {
			std::vector<std::int32_t> l_keys(static_cast<std::size_t>(p_n));
			for (auto& l_key: l_keys) {
				l_key = m_rand.next();
			}
			std::vector<int> l_order(static_cast<std::size_t>(p_n));
			std::iota(l_order.begin(), l_order.end(), 0);
			std::stable_sort(l_order.begin(), l_order.end(), [&l_keys](const int p_a, const int p_b) {
				return l_keys[static_cast<std::size_t>(p_a)] < l_keys[static_cast<std::size_t>(p_b)];
			});
			return l_order;
		}

		void handle_start() {
			send_by_name("teller", "vote " + m_preference_order);

			stop();
		}

	public:
		VoterAgent(const std::string& p_name, cam::DotnetRandom& p_rand) : Agent(p_name), m_rand(p_rand) {}

		void setup() override {
			const int l_no_candidates = get_environment()->get_global_data().at("NoCandidates").get<int>();

			const auto l_pref = random_order_permutation(l_no_candidates);

			std::string l_sb;

			if (m_rand.next_double() < 0.5) {
				// complete preference order
				for (int i = 0; i < l_no_candidates; i++) {
					l_sb += std::to_string(l_pref[static_cast<std::size_t>(i)]) + " ";
				}
			} else {
				// incomplete preference order
				const int l_no_preferences = m_rand.next(l_no_candidates - 1) + 1;
				for (int i = 0; i < l_no_preferences; i++) {
					l_sb += std::to_string(l_pref[static_cast<std::size_t>(i)]) + " ";
				}
			}

			m_preference_order = cam::dotnet::trim(l_sb);
		}

		void action(const cam::MessagePointer& p_message) override {
			[[maybe_unused]] const auto l_parsed = p_message->parse_rest();  // action == "start"
			handle_start();
		}
};

class TellerAgent final : public cam::Agent {
	private:
		int m_turns_to_wait = 0;
		int m_no_candidates = 0;
		std::vector<std::string> m_votes;

		void handle_vote(const std::string& p_preference_order) {
			cam::dotnet::write_line(p_preference_order);
			m_votes.push_back(p_preference_order);  // optimization: use int[][] instead of string[] to store the votes
		}

		void handle_results() {
			bool l_is_condorcet = false;
			int l_winner = -1;
			copeland(l_is_condorcet, l_winner);

			if (l_is_condorcet) {
				cam::dotnet::write_line("Candidate " + std::to_string(l_winner) + " is the Condorcet winner");
			} else {
				cam::dotnet::write_line("There is no Condorcet winner");
			}

			cam::dotnet::write_line("Using Copeland's method, candidate " + std::to_string(l_winner) + " is the winner");
			l_winner = borda();
			cam::dotnet::write_line("Using Borda count, candidate " + std::to_string(l_winner) + " is the winner");

			stop();
		}

		void copeland(bool& p_is_condorcet, int& p_winner) const {
			p_is_condorcet = false;
			p_winner = -1;

			const auto l_n = static_cast<std::size_t>(m_no_candidates);
			std::vector<std::vector<int>> l_pair_wise_wins(l_n, std::vector<int>(l_n, 0));

			for (int i = 0; i < m_no_candidates - 1; i++) {
				for (int j = i + 1; j < m_no_candidates; j++) {
					int l_i_wins = 0;
					int l_j_wins = 0;

					for (const auto& l_vote: m_votes) {
						bool l_iw = false;
						bool l_jw = false;
						check_winner(i, j, l_vote, l_iw, l_jw);

						if (l_iw) {
							l_i_wins++;
						}
						if (l_jw) {
							l_j_wins++;
						}
					}

					if (l_i_wins > l_j_wins) {
						l_pair_wise_wins[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)]++;
					} else if (l_i_wins < l_j_wins) {
						l_pair_wise_wins[static_cast<std::size_t>(j)][static_cast<std::size_t>(i)]++;
					}
				}
			}

			std::vector<int> l_score(l_n, 0);
			int l_max_score = 0;

			for (int i = 0; i < m_no_candidates; i++) {
				int l_no_wins = 0;
				int l_no_losses = 0;
				no_wins_losses(i, l_pair_wise_wins, l_no_wins, l_no_losses);
				l_score[static_cast<std::size_t>(i)] = l_no_wins - l_no_losses;

				if (l_score[static_cast<std::size_t>(i)] > l_max_score) {
					l_max_score = l_score[static_cast<std::size_t>(i)];
					p_winner = i;
				}
			}

			if (l_max_score == m_no_candidates - 1) {
				p_is_condorcet = true;
			}
		}

		void no_wins_losses(const int p_i, const std::vector<std::vector<int>>& p_pair_wise_wins, int& p_no_wins, int& p_no_losses) const {
			p_no_wins = 0;
			p_no_losses = 0;

			for (int a = 0; a < m_no_candidates; a++) {
				if (a == p_i) {
					continue;
				}

				if (p_pair_wise_wins[static_cast<std::size_t>(p_i)][static_cast<std::size_t>(a)] > 0) {
					p_no_wins++;
				} else if (p_pair_wise_wins[static_cast<std::size_t>(a)][static_cast<std::size_t>(p_i)] > 0) {
					p_no_losses++;
				}
			}
		}

		void check_winner(const int p_i, const int p_j, const std::string& p_vote, bool& p_i_wins, bool& p_j_wins) const {
			const int l_pos_i = get_position(p_i, p_vote);
			const int l_pos_j = get_position(p_j, p_vote);

			if (l_pos_i < l_pos_j) {
				p_i_wins = true;
				p_j_wins = false;
			} else if (l_pos_i > l_pos_j) {
				p_i_wins = false;
				p_j_wins = true;
			} else {  // posI == posJ
				p_i_wins = false;
				p_j_wins = false;
			}
		}

		[[nodiscard]] int get_position(const int p_i, const std::string& p_vote) const {
			const auto l_toks = cam::dotnet::split(p_vote);
			for (std::size_t a = 0; a < l_toks.size(); a++) {
				if (p_i == cam::dotnet::to_int32(l_toks[a])) {
					return static_cast<int>(a);
				}
			}

			return m_no_candidates;  // i is not in the preference order v
		}

		[[nodiscard]] int borda() const {
			std::vector<int> l_score(static_cast<std::size_t>(m_no_candidates), 0);

			for (const auto& l_vote: m_votes) {
				for (int i = 0; i < m_no_candidates; i++) {
					const int l_pos = get_position(i, l_vote);
					l_score[static_cast<std::size_t>(i)] += m_no_candidates - l_pos;  // last ranked: 1 point, unranked: 0 points
				}
			}

			int l_winner = -1;
			int l_max_score = 0;

			for (int i = 0; i < m_no_candidates; i++) {
				if (l_score[static_cast<std::size_t>(i)] > l_max_score) {
					l_max_score = l_score[static_cast<std::size_t>(i)];
					l_winner = i;
				}
			}

			return l_winner;
		}

	public:
		explicit TellerAgent(const std::string& p_name) : Agent(p_name) {}

		void setup() override {
			m_no_candidates = get_environment()->get_global_data().at("NoCandidates").get<int>();
			m_turns_to_wait = 2;
			broadcast("start");
		}

		void action(const cam::MessagePointer& p_message) override {
			const auto [l_action, l_parameters] = p_message->parse_rest();  // action == "vote"
			handle_vote(l_parameters);
		}

		void default_action() override {
			if (--m_turns_to_wait <= 0) {
				handle_results();
			}
		}
};

int main() {
	// select the Condorcet winner if one exists, otherwise use Borda count
	cam::DotnetRandom l_rand(cam::dotnet::next_site_seed("Program.cs:23"));

	cam::Environment l_environment({.m_turns = 0,
									.m_mode = cam::EnvironmentMasMode::Sequential,
									.m_seed = static_cast<unsigned int>(cam::dotnet::next_site_seed("EnvironmentMas.cs:61")),
									.m_random = cam::dotnet::random_engine()});

	l_environment.add<TellerAgent>("teller");

	l_environment.set_global_data("NoCandidates", 5);

	constexpr int l_no_voters = 99;
	for (int i = 1; i <= l_no_voters; i++) {
		l_environment.add<VoterAgent>((i < 10 ? "voter0" : "voter") + std::to_string(i), l_rand);
	}

	l_environment.start();
	return 0;
}
