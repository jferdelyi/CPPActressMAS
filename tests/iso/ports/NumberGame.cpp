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
// oracle by iso.NumberGame.
// Port of the C# example E12 Simple Observables/NumberGame (ActressMAS v3.1): three agents publish a number between 0
// and 30, written with two decimals; each sees the agents whose number is within 10 of its own, then draws another.
// Sequential, fixed order, 10 turns; the numbers are drawn as the C# draws them (the seed of its oracle).

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include <DotNet.hpp>
#include <Environment.h>
#include <util/DotnetRandom.h>

class Numbers {
	public:
		static double generate_number() {
			static cam::DotnetRandom s_rand(cam::dotnet::next_site_seed("Program.cs:90"));
			return s_rand.next_double() * 30;
		}
};

class MyAgent final : public cam::Agent {
	private:
		std::vector<cam::ObservablesPointer> m_observable_agents;

		// The number is kept as its text with two decimals, and read back for the comparison (Convert.ToDouble)
		[[nodiscard]] static double to_double(const cam::ObservablesPointer& p_observables) {
			return cam::dotnet::to_double(p_observables->at("Number").get<std::string>());
		}

	public:
		explicit MyAgent(const std::string& p_name) : Agent(p_name, true) {}

		void setup() override {
			(*m_observables)["Name"] = get_name();
			(*m_observables)["Number"] = cam::dotnet::format_fixed(Numbers::generate_number(), 2);
		}

		[[nodiscard]] bool perception_filter(const cam::ObservablesPointer& p_observed) const override {
			const double l_my_number = to_double(m_observables);
			const double l_observed_number = to_double(p_observed);
			return std::fabs(l_my_number - l_observed_number) < 10;
		}

		void see(const std::vector<cam::ObservablesPointer>& p_observable_agents) override {
			m_observable_agents = p_observable_agents;
		}

		void default_action() override {
			std::cout << "I am " << get_name() << ". ";

			if (m_observable_agents.empty()) {
				std::cout << "I did't see anything interesting." << std::endl;
			} else {
				std::cout << "My number is " << m_observables->at("Number").get<std::string>() << " and I saw:" << std::endl;
				for (const auto& l_observed: m_observable_agents) {
					std::cout << l_observed->at("Name").get<std::string>() << " with number = " << l_observed->at("Number").get<std::string>() << std::endl;
				}
			}

			(*m_observables)["Number"] = cam::dotnet::format_fixed(Numbers::generate_number(), 2);

			std::cout << "My number is now " << m_observables->at("Number").get<std::string>() << std::endl;
			std::cout << "----------------------------------------------" << std::endl;
		}
};

int main() {
	cam::Environment l_environment({.m_turns = 10,
									.m_mode = cam::EnvironmentMasMode::Sequential,
									.m_seed = static_cast<unsigned int>(cam::dotnet::next_site_seed("EnvironmentMas.cs:61")),
									.m_random = cam::dotnet::random_engine()});

	l_environment.add<MyAgent>("Agent1");
	l_environment.add<MyAgent>("Agent2");
	l_environment.add<MyAgent>("Agent3");

	l_environment.start();
	return 0;
}
