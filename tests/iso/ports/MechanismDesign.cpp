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

// Harness program of the oracle tests (tests/iso, D98): the C# original as it is, its bugs included, compared with its
// oracle by iso.MechanismDesign.
// Port of the C# example E09 Mechanism Design (ActressMAS v3.1): a decision maker chooses how many street lights to
// install from the net benefits the beneficiaries report, then taxes the pivotal ones (Clarke tax). The second run
// shows that lying does not pay: beneficiary a over-reports its benefit for 3 lights and ends with less.

#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include <DotNet.hpp>
#include <Environment.h>

// Settings of the example (Settings.cs)
namespace settings {

	constexpr int NO_BENEFICIARIES = 3;
	constexpr int NO_OPTIONS = 4;  // 0, 1, 2, 3 street lights
	constexpr int UNIT_COST = 120;
	bool s_a_is_lying = true;

} // namespace settings

class BeneficiaryAgent final : public cam::Agent {
	private:
		std::vector<int> m_net_benefits;
		std::string m_net_benefits_text;

		void handle_report() {
			if (settings::s_a_is_lying && get_name() == "a") {
				std::cout << "[a]: My *reported* net benefits are (0 20 10 70)" << std::endl;
				send_by_name("dm", "benefits 0 20 10 70");
			} else {
				send_by_name("dm", "benefits " + m_net_benefits_text);
			}
		}

		void handle_result(const std::vector<std::string>& p_parameters) {
			const int l_lights = cam::dotnet::to_int32(p_parameters.at(0));
			const int l_tax = cam::dotnet::to_int32(p_parameters.at(1));
			std::cout << "[" + get_name() + "]: My net benefit after tax is " << m_net_benefits.at(static_cast<std::size_t>(l_lights)) - l_tax
					  << std::endl;
			stop();
		}

	public:
		explicit BeneficiaryAgent(const std::string& p_name) : Agent(p_name) {}

		void setup() override {
			std::vector<int> l_total_benefits;
			if (get_name() == "a") {
				l_total_benefits = {0, 60, 90, 155};
			} else if (get_name() == "b") {
				l_total_benefits = {0, 80, 120, 140};
			} else if (get_name() == "c") {
				l_total_benefits = {0, 120, 200, 220};
			}
			m_net_benefits.clear();
			m_net_benefits_text.clear();
			for (int i = 0; i < settings::NO_OPTIONS; i++) {
				m_net_benefits.push_back(l_total_benefits.at(static_cast<std::size_t>(i)) - i * settings::UNIT_COST / settings::NO_BENEFICIARIES);
				m_net_benefits_text += (i == 0 ? "" : " ") + std::to_string(m_net_benefits.back());
			}
			std::cout << "[" + get_name() + "]: My net benefits are (" + m_net_benefits_text + ")" << std::endl;
		}

		void action(const cam::MessagePointer& p_message) override {
			try {
				std::cout << "\t" + format(p_message) << std::endl;
				const auto [l_action, l_parameters] = p_message->parse();
				if (l_action == "report") {
					handle_report();
				} else if (l_action == "result") {
					handle_result(l_parameters);
				}
			} catch (const std::exception& p_exception) {
				std::cout << p_exception.what() << std::endl;
			}
		}
};

class DecisionMakerAgent final : public cam::Agent {
	private:
		std::vector<std::vector<int>> m_net_benefits;
		std::vector<int> m_taxes;
		int m_answers_received = 0;

		void handle_benefits(const std::string& p_sender, const std::vector<std::string>& p_benefits) {
			const auto l_agent_index = static_cast<std::size_t>(p_sender.at(0) - 'a');
			for (std::size_t i = 0; i < static_cast<std::size_t>(settings::NO_OPTIONS); i++) {
				m_net_benefits.at(l_agent_index).at(i) = cam::dotnet::to_int32(p_benefits.at(i));
			}
			m_answers_received++;
			if (m_answers_received == settings::NO_BENEFICIARIES) {
				compute_taxes();
			}
		}

		void compute_taxes() {
			int l_best_option = -1;
			int l_largest_net_social_benefit = -1;
			for (int i = 0; i < settings::NO_OPTIONS; i++) {
				int l_sum = 0;
				for (int j = 0; j < settings::NO_BENEFICIARIES; j++) {
					l_sum += m_net_benefits[static_cast<std::size_t>(j)][static_cast<std::size_t>(i)];
				}
				if (l_sum > l_largest_net_social_benefit) {
					l_largest_net_social_benefit = l_sum;
					l_best_option = i;
				}
			}
			std::cout << "[" + get_name() + "]: We will install " << l_best_option << " lights" << std::endl;

			for (int k = 0; k < settings::NO_BENEFICIARIES; k++) {  // without k
				int l_best_option_without_k = -1;
				int l_largest_without_k = -1;
				int l_previous_largest_without_k = 0;
				for (int i = 0; i < settings::NO_OPTIONS; i++) {
					int l_sum = 0;
					for (int j = 0; j < settings::NO_BENEFICIARIES; j++) {
						if (j != k) {
							l_sum += m_net_benefits[static_cast<std::size_t>(j)][static_cast<std::size_t>(i)];
						}
					}
					if (i == l_best_option) {
						l_previous_largest_without_k = l_sum;
					}
					if (l_sum > l_largest_without_k) {
						l_largest_without_k = l_sum;
						l_best_option_without_k = i;
					}
				}
				const char l_beneficiary = static_cast<char>('a' + k);
				if (l_best_option != l_best_option_without_k) {
					m_taxes[static_cast<std::size_t>(k)] = l_largest_without_k - l_previous_largest_without_k;
					std::cout << "[" + get_name() + "]: " << l_beneficiary << " is pivotal - from " << l_best_option_without_k
							  << " - and its tax is " << m_taxes[static_cast<std::size_t>(k)] << std::endl;
				} else {
					std::cout << "[" + get_name() + "]: " << l_beneficiary << " is not pivotal and its tax is 0" << std::endl;
				}
			}

			for (int i = 0; i < settings::NO_BENEFICIARIES; i++) {
				const std::string l_name(1, static_cast<char>('a' + i));
				send_by_name(l_name, "result " + std::to_string(l_best_option) + " " + std::to_string(m_taxes[static_cast<std::size_t>(i)]));
			}
		}

	public:
		explicit DecisionMakerAgent(const std::string& p_name) : Agent(p_name) {}

		void setup() override {
			m_net_benefits.assign(settings::NO_BENEFICIARIES, std::vector<int>(settings::NO_OPTIONS, 0));
			m_taxes.assign(settings::NO_BENEFICIARIES, 0);
			m_answers_received = 0;
			broadcast("report");
		}

		void action(const cam::MessagePointer& p_message) override {
			try {
				std::cout << "\t" + format(p_message) << std::endl;
				const auto [l_action, l_parameters] = p_message->parse();
				if (l_action == "benefits") {
					handle_benefits(get_agent_name(p_message->get_sender()).value_or(""), l_parameters);
				}
			} catch (const std::exception& p_exception) {
				std::cout << p_exception.what() << std::endl;
			}
		}
};

// One run of 100 sequential turns in a fixed order, as the C# EnvironmentMas(noTurns: 100, randomOrder: false, parallel: false)
void run(const bool p_a_is_lying) {
	cam::Environment l_environment({.m_turns = 100, .m_mode = cam::EnvironmentMasMode::Sequential});
	settings::s_a_is_lying = p_a_is_lying;
	l_environment.add<BeneficiaryAgent>("a");
	l_environment.add<BeneficiaryAgent>("b");
	l_environment.add<BeneficiaryAgent>("c");
	l_environment.add<DecisionMakerAgent>("dm");
	l_environment.start();
}

int main() {
	run(false);
	std::cout << std::endl << "--------------------------------------------" << std::endl << std::endl;
	run(true);
	return 0;
}
