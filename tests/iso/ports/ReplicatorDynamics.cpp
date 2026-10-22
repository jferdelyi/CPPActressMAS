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
// oracle by iso.ReplicatorDynamics.
// Port of the C# example E16 Replicator Dynamics (ActressMAS v3.1): each round, the game agent collects the proportion
// of each strategy, prints the expected payoffs, and each strategy agent updates its proportion by the discrete
// replicator rule. A 2x2 coordination game, 50 rounds. Parallel, no turn limit.
// Arguments: none for the example; "sequential" for its sequential variant (fixed order).

#include <cmath>
#include <exception>
#include <limits>
#include <map>
#include <string>
#include <vector>

#include <DotNet.hpp>
#include <Environment.h>

struct Game {
	// Symmetric normal-form game payoff matrix A, where A[i,j] is the payoff of pure strategy i against pure strategy j.
	// Must be square and have strictly positive payoffs for the basic x_i' = x_i * (u_i / u_bar) replicator rule.
	static inline std::vector<std::vector<double>> s_payoff_matrix;

	static int no_strategies() { return static_cast<int>(s_payoff_matrix.size()); }

	static std::vector<double> compute_expected_payoffs(const std::vector<double>& p_x) {
		const auto l_n = static_cast<std::size_t>(no_strategies());
		std::vector<double> l_u(l_n);
		for (std::size_t i = 0; i < l_n; i++) {
			double l_ui = 0.0;
			for (std::size_t j = 0; j < l_n; j++) {
				l_ui += s_payoff_matrix[i][j] * p_x[j];
			}
			l_u[i] = l_ui;
		}
		return l_u;
	}

	static double compute_average_payoff(const std::vector<double>& p_x, const std::vector<double>& p_u) {
		double l_u_bar = 0.0;
		for (std::size_t i = 0; i < p_x.size(); i++) {
			l_u_bar += p_x[i] * p_u[i];
		}
		return l_u_bar;
	}
};

class GameAgent final : public cam::Agent {
	private:
		std::vector<std::string> m_strategy_names;
		int m_no_strategies;
		int m_max_rounds;
		std::map<std::string, double> m_current_proportions;
		int m_current_round = 0;

		void start_next_round() {
			m_current_round++;

			if (m_current_round > m_max_rounds) {
				cam::dotnet::write_line("");
				cam::dotnet::write_line("Simulation finished.");

				for (const auto& l_name: m_strategy_names) {
					send_by_name(l_name, "stop");
				}

				stop();
				return;
			}

			cam::dotnet::write_line("");
			cam::dotnet::write_line("--- Round " + std::to_string(m_current_round) + " ---");

			for (const auto& l_name: m_strategy_names) {
				m_current_proportions[l_name] = std::numeric_limits<double>::quiet_NaN();
			}

			for (const auto& l_name: m_strategy_names) {
				send_by_name(l_name, "tick " + std::to_string(m_current_round));
			}
		}

		void handle_state(const std::string& p_sender, const std::string& p_parameters) {
			const double l_xi = cam::dotnet::to_double(cam::dotnet::trim(p_parameters));
			m_current_proportions[p_sender] = l_xi;

			if (!all_proportions_received()) {
				return;
			}

			std::vector<double> l_x(static_cast<std::size_t>(m_no_strategies));
			for (int i = 0; i < m_no_strategies; i++) {
				const std::string& l_name = m_strategy_names[static_cast<std::size_t>(i)];
				l_x[static_cast<std::size_t>(i)] = m_current_proportions[l_name];
			}

			double l_sum = 0.0;  // x.Sum()
			for (const double l_value: l_x) {
				l_sum += l_value;
			}

			if (l_sum <= 0.0) {
				cam::dotnet::write_line("Population proportions sum to non-positive value.");
				for (const auto& l_name: m_strategy_names) {
					send_by_name(l_name, "stop");
				}
				stop();
				return;
			}

			if (std::abs(l_sum - 1.0) > 1e-6) {
				for (auto& l_value: l_x) {
					l_value /= l_sum;
				}
				// The C# format "F2)" is a custom one, not F2: never reached here, the proportions keep summing to 1
				cam::dotnet::write_line("State normalized: sum before normalization = " + cam::dotnet::format_custom_f2_paren(l_sum));
			}

			const auto l_u = Game::compute_expected_payoffs(l_x);
			const double l_u_bar = Game::compute_average_payoff(l_x, l_u);

			cam::dotnet::write("x = [");
			for (int i = 0; i < m_no_strategies; i++) {
				cam::dotnet::write(cam::dotnet::format_fixed(l_x[static_cast<std::size_t>(i)], 2));
				if (i < m_no_strategies - 1) {
					cam::dotnet::write(", ");
				}
			}
			cam::dotnet::write_line("]");

			cam::dotnet::write("u = [");
			for (int i = 0; i < m_no_strategies; i++) {
				cam::dotnet::write(cam::dotnet::format_fixed(l_u[static_cast<std::size_t>(i)], 2));
				if (i < m_no_strategies - 1) {
					cam::dotnet::write(", ");
				}
			}
			cam::dotnet::write_line("]");
			cam::dotnet::write_line("ubar = " + cam::dotnet::format_fixed(l_u_bar, 2));

			for (int i = 0; i < m_no_strategies; i++) {
				const std::string& l_name = m_strategy_names[static_cast<std::size_t>(i)];
				const std::string l_msg = "payoff " + cam::dotnet::to_string(l_u[static_cast<std::size_t>(i)]) + " " + cam::dotnet::to_string(l_u_bar);
				send_by_name(l_name, l_msg);
			}

			start_next_round();
		}

		[[nodiscard]] bool all_proportions_received() const {
			for (const auto& [l_name, l_value]: m_current_proportions) {
				if (std::isnan(l_value)) {
					return false;
				}
			}
			return true;
		}

	public:
		GameAgent(const std::string& p_name, const std::vector<std::string>& p_strategy_names, const int p_max_rounds) :
				Agent(p_name),
				m_strategy_names(p_strategy_names),
				m_no_strategies(static_cast<int>(p_strategy_names.size())),
				m_max_rounds(p_max_rounds) {}

		void setup() override {
			m_current_round = 0;
			m_current_proportions.clear();
			for (const auto& l_name: m_strategy_names) {
				m_current_proportions[l_name] = std::numeric_limits<double>::quiet_NaN();
			}

			cam::dotnet::write_line("Replicator dynamics with " + std::to_string(m_no_strategies) + " strategies for " + std::to_string(m_max_rounds) +
									" rounds.");

			start_next_round();
		}

		void action(const cam::MessagePointer& p_message) override {
			try {
				const auto [l_action, l_parameters] = p_message->parse_rest();

				if (l_action == "state") {
					handle_state(get_agent_name(p_message->get_sender()).value_or(""), l_parameters);
				}
			} catch (const std::exception& p_exception) {
				cam::dotnet::write_line(std::string("GameAgent error: ") + p_exception.what());
			}
		}
};

class StrategyAgent final : public cam::Agent {
	private:
		std::string m_coordinator_name;
		int m_index;
		double m_proportion;

		void handle_tick([[maybe_unused]] const std::string& p_parameters) const {
			// parameters: time step (optional, not used here)
			send_by_name(m_coordinator_name, "state " + cam::dotnet::format_fixed(m_proportion, 4));
		}

		void handle_payoff(const std::string& p_parameters) {
			// parameters: "<ui> <ubar>"
			const auto l_toks = cam::dotnet::split(cam::dotnet::trim(p_parameters));
			if (l_toks.size() < 2) {
				cam::dotnet::write_line(get_name() + ": payoff message malformed: '" + p_parameters + "'");
				return;
			}

			const double l_ui = cam::dotnet::to_double(l_toks[0]);
			const double l_u_bar = cam::dotnet::to_double(l_toks[1]);

			if (l_u_bar <= 0.0) {
				cam::dotnet::write_line(get_name() + ": average payoff non-positive (" + cam::dotnet::to_string(l_u_bar) + "), proportion not updated.");
				return;
			}

			const double l_old_proportion = m_proportion;

			// Discrete-time replicator update: x_i' = x_i * (u_i / u_bar)
			m_proportion = m_proportion * (l_ui / l_u_bar);

			cam::dotnet::write_line(get_name() + ": ui = " + cam::dotnet::format_fixed(l_ui, 2) + ", ubar = " + cam::dotnet::format_fixed(l_u_bar, 2) +
									", x(old) = " + cam::dotnet::format_fixed(l_old_proportion, 4) + ", x(new) = " +
									cam::dotnet::format_fixed(m_proportion, 2));
		}

		void handle_stop() {
			cam::dotnet::write_line("Final proportion for " + get_name() + ": " + cam::dotnet::format_fixed(m_proportion, 2));
			stop();
		}

	public:
		StrategyAgent(const std::string& p_name, const std::string& p_coordinator_name, const int p_index, const double p_initial_proportion) :
				Agent(p_name),
				m_coordinator_name(p_coordinator_name),
				m_index(p_index),
				m_proportion(p_initial_proportion) {}

		void setup() override {
			cam::dotnet::write_line(get_name() + " (strategy " + std::to_string(m_index) + ") initial proportion = " +
									cam::dotnet::format_fixed(m_proportion, 4));
		}

		void action(const cam::MessagePointer& p_message) override {
			try {
				const auto [l_action, l_parameters] = p_message->parse_rest();

				if (l_action == "tick") {
					handle_tick(l_parameters);
				} else if (l_action == "payoff") {
					handle_payoff(l_parameters);
				} else if (l_action == "stop") {
					handle_stop();
				}
			} catch (const std::exception& p_exception) {
				cam::dotnet::write_line(get_name() + " error: " + p_exception.what());
			}
		}
};

int main(int argc, char** argv) {
	// 2x2 coordination game
	//          A     B
	//   A    3.0   0.0
	//   B    0.0   3.0
	//
	// Payoffs are strictly positive, which matches the simple replicator rule.
	Game::s_payoff_matrix = {{3.0, 0.0}, {0.0, 3.0}};

	const int l_n = Game::no_strategies();

	// Initial distribution over strategies; must sum to 1.0
	const std::vector<double> l_initial_proportions = {0.5, 0.5};

	cam::Environment l_environment({.m_turns = 0,
									.m_mode = cam::dotnet::scenario_mode(argc, argv, cam::EnvironmentMasMode::Parallel),
									.m_seed = static_cast<unsigned int>(cam::dotnet::next_site_seed("EnvironmentMas.cs:61")),
									.m_random = cam::dotnet::random_engine()});

	std::vector<std::string> l_strategy_names(static_cast<std::size_t>(l_n));
	for (int i = 0; i < l_n; i++) {
		l_strategy_names[static_cast<std::size_t>(i)] = "S" + std::to_string(i);
	}

	constexpr int l_max_rounds = 50;

	l_environment.add<GameAgent>("game", l_strategy_names, l_max_rounds);

	for (int i = 0; i < l_n; i++) {
		l_environment.add<StrategyAgent>(l_strategy_names[static_cast<std::size_t>(i)], "game", i, l_initial_proportions[static_cast<std::size_t>(i)]);
	}

	l_environment.start();
	return 0;
}
