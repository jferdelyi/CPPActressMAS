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

#include <iostream>
#include <random>

#include <Environment.h>

class Numbers {
	public:
		static double GenerateNumber(const double p_max = 30.0) {
			static std::default_random_engine s_seeder(std::time(nullptr));
			static std::mt19937 s_generator(s_seeder());

			std::uniform_real_distribution l_dist(0.0,  p_max);
			return std::ceil(l_dist(s_generator) * 100.0) / 100.0;
		}
};

class MyAgent : public cam::Agent {
	protected:
		std::vector<cam::ObservablesPointer> m_observable_agents;

	public:
		explicit MyAgent(const std::string& p_name) :
			Agent(p_name, true) {
		}

		void setup() override {
		    m_observables->emplace(std::make_pair("Name", get_name()));
			m_observables->emplace(std::make_pair("Number", Numbers::GenerateNumber()));
		}

		bool perception_filter(const cam::ObservablesPointer& p_observed) const override {
			const double l_my_number = m_observables->at("Number");
			const double l_observed_number = p_observed->at("Number");
			return abs(l_my_number - l_observed_number) < 10;
		}

		void see(const std::vector<cam::ObservablesPointer>& p_observable_agents) override {
			m_observable_agents.clear();
			for (const auto& l_observable_agent : p_observable_agents) {
				m_observable_agents.push_back(l_observable_agent);
			}
		}

		void default_action() override {
		    std::cout << "I am " << get_name() << ". ";
		    if (m_observable_agents.empty()) {
		        std::cout << "I did't see anything interesting." << std::endl;
		    } else {
		        std::cout << "My number is " << m_observables->at("Number") << " and I saw:" << std::endl;
		        for (const auto& l_observable : m_observable_agents) {
                    std::cout << l_observable->at("Name") << " with number = " << l_observable->at("Number") << std::endl;
		        }
		    }
			m_observables->erase("Number");
			m_observables->insert(std::make_pair("Number", Numbers::GenerateNumber()));
		    std::cout << "My number is now " << m_observables->at("Number") << std::endl;
		    std::cout << "----------------------------------------------" << std::endl;
		}
};

int main() {
    cam::Environment l_environment(10, cam::EnvironmentMasMode::Sequential);

    l_environment.add<MyAgent>("Agent1");
    l_environment.add<MyAgent>("Agent2");
    l_environment.add<MyAgent>("Agent3");

    l_environment.start();
}
