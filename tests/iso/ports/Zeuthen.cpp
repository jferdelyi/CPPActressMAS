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
// oracle by iso.Zeuthen.
// Port of the C# example E07 Zeuthen Strategy (ActressMAS v3.1): two agents bargain over a deal between 0.1 and 4.9;
// each turn, the one with the smaller risk concedes, until a proposal is accepted. Sequential, fixed order, no turn
// limit. The agents draw (on equal risks only) from the seed of the C# oracle.

#include <cmath>
#include <functional>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>

#include <DotNet.hpp>
#include <Environment.h>
#include <util/DotnetRandom.h>

// The C# keeps the utility functions in the memory of the environment, a dictionary of any objects; the global data of
// the C++ environment is JSON, so the functions stay in this table, under the same names
std::map<std::string, std::function<double(double)>> g_functions;

class BargainingAgent final : public cam::Agent {
	private:
		std::function<double(double)> m_my_utility_function;
		std::function<double(double)> m_others_utility_function;
		double m_my_proposal = 0;
		double m_others_proposal = 0;
		std::string m_others_name;
		std::optional<cam::DotnetRandom> m_rand;

		[[nodiscard]] double eps() const { return get_environment()->get_global_data().at("Eps").get<double>(); }

		double new_proposal(const double p_d1, const double p_d2) const {
			// the new proposal is computed iteratively, so that the procedure is general and can be applied for any pair of utility functions
			// for our example, with linear utility functions, the new proposal can be computed analytically
			if (get_name() == "agent1") {
				for (double d = p_d1; d <= p_d2; d += eps()) {
					if (m_my_utility_function(d) * m_others_utility_function(d) > m_my_utility_function(p_d2) * m_others_utility_function(p_d2)) {
						// this inequality is equivalent to: risk1(d) > risk2(d2)
						return d;
					}
				}
				return p_d2;
			}
			for (double d = p_d1; d >= p_d2; d -= eps()) {
				if (m_my_utility_function(d) * m_others_utility_function(d) > m_my_utility_function(p_d2) * m_others_utility_function(p_d2)) {
					return d;
				}
			}
			return p_d1;
		}

		static double risk(const std::function<double(double)>& p_utility, const double p_d1, const double p_d2) {
			return 1.0 - p_utility(p_d2) / p_utility(p_d1);
		}

		void handle_propose(const double p_proposal) {
			m_others_proposal = p_proposal;

			if (m_my_utility_function(m_others_proposal) >= m_my_utility_function(m_my_proposal)) {
				send_by_name(m_others_name, "accept");
				return;
			}

			if (m_my_utility_function(m_others_proposal) < 0) {
				send_by_name(m_others_name, "continue");
				return;
			}

			const double l_my_risk = risk(m_my_utility_function, m_my_proposal, m_others_proposal);
			const double l_others_risk = risk(m_others_utility_function, m_others_proposal, m_my_proposal);

			cam::dotnet::write_line("[" + get_name() + "]: My risk is " + cam::dotnet::format_fixed(l_my_risk, 4) + " and the other's risk is " +
									cam::dotnet::format_fixed(l_others_risk, 4));

			if ((l_my_risk < l_others_risk) || (are_equal(l_my_risk, l_others_risk) && m_rand->next_double() < 0.5)) {  // on equality, concede randomly
				m_my_proposal = new_proposal(m_my_proposal, m_others_proposal);
				cam::dotnet::write_line("[" + get_name() + "]: I will concede with new proposal " + cam::dotnet::format_fixed(m_my_proposal, 1));
				send_by_name(m_others_name, "propose " + cam::dotnet::to_string(m_my_proposal));
			} else {
				cam::dotnet::write_line("[" + get_name() + "]: I will not concede");

				const double l_eps = eps();
				get_environment()->set_global_data("Eps", l_eps / 10.0);  // to avoid a blockage when finishing with equal risks

				send_by_name(m_others_name, "continue");
			}
		}

		void handle_continue() {
			m_my_proposal = new_proposal(m_my_proposal, m_others_proposal);
			send_by_name(m_others_name, "propose " + cam::dotnet::to_string(m_my_proposal));
		}

		void handle_accept() {
			cam::dotnet::write_line("[" + get_name() + "]: I accept " + cam::dotnet::format_fixed(m_my_proposal, 1));
			send_by_name(m_others_name, "accept");
			stop();
		}

		static bool are_equal(const double p_x, const double p_y) {
			return std::abs(p_x - p_y) < 1e-10;
		}

	public:
		explicit BargainingAgent(const std::string& p_name) : Agent(p_name) {}

		void setup() override {
			if (get_name() == "agent1") {
				m_my_utility_function = g_functions["Utility1"];
				m_others_utility_function = g_functions["Utility2"];

				m_others_name = "agent2";
				m_my_proposal = 0.1;
				send_by_name(m_others_name, "propose " + cam::dotnet::to_string(m_my_proposal));
			} else {
				m_my_utility_function = g_functions["Utility2"];
				m_others_utility_function = g_functions["Utility1"];

				m_others_name = "agent1";
				m_my_proposal = 4.9;
			}

			m_rand.emplace(cam::dotnet::next_site_seed("BargainingAgent.cs:48"));
		}

		void action(const cam::MessagePointer& p_message) override {
			try {
				cam::dotnet::write_line("\t" + format(p_message));
				const auto [l_action, l_parameters] = p_message->parse_rest();

				if (l_action == "propose") {
					handle_propose(cam::dotnet::to_double(l_parameters));
				} else if (l_action == "continue") {
					handle_continue();
				} else if (l_action == "accept") {
					handle_accept();
				}
			} catch (const std::exception& p_exception) {
				cam::dotnet::write_line(p_exception.what());
			}
		}
};

int main() {
	cam::Environment l_environment({.m_turns = 0,
									.m_mode = cam::EnvironmentMasMode::Sequential,
									.m_seed = static_cast<unsigned int>(cam::dotnet::next_site_seed("EnvironmentMas.cs:61")),
									.m_random = cam::dotnet::random_engine()});
	l_environment.add<BargainingAgent>("agent1");
	l_environment.add<BargainingAgent>("agent2");

	l_environment.set_global_data("Eps", 0.1);
	g_functions["Utility1"] = [](const double p_deal) { return 5.0 - p_deal; };
	g_functions["Utility2"] = [](const double p_deal) { return 2.0 / 3.0 * p_deal; };

	l_environment.start();
	return 0;
}
