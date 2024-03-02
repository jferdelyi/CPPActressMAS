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

#include <EnvironmentMas.h>

class Colors {
	public:
		static std::string GenerateColor() {
			static std::random_device l_rd;
			static std::vector<std::string> l_colors {"red", "green", "blue"};
			std::uniform_int_distribution l_dist(0,  2);
			return l_colors.at(l_dist(l_rd));
		}
};

class MyAgent : public cam::Agent {
	protected:
		std::vector<const cam::ObservablesPointer> m_observable_agents;

	public:
		explicit MyAgent(const std::string& p_name) :
			Agent(p_name) {
		}

		void see(const std::vector<const cam::ObservablesPointer>& p_observable_agents) override {
			std::cout << "I am " << m_name << ". I am looking around..." << std::endl;
			m_observable_agents.clear();
			for (const auto& l_observable_agent : p_observable_agents) {
				m_observable_agents.push_back(l_observable_agent);
			}
		}

		void default_action() override {
		    if (m_observable_agents.empty()) {
		        std::cout << "I did't see anything interesting." << std::endl;
		    } else {
		        std::cout << "I saw: ";
		        for (const auto& l_observable : m_observable_agents) {
                    std::cout << l_observable->at("Name") << " (" << l_observable->at("Color") << ")" << std::endl;
		        }
		    }
			m_observables->insert(std::make_pair("Color", Colors::GenerateColor()));
		    std::cout << "My color is now " << m_observables->at("Color") << std::endl;
		    std::cout << "----------------------------------------------" << std::endl;
		}
};

class MyAgent1 final : public MyAgent {
public:
	explicit MyAgent1(const std::string& p_name) : MyAgent(p_name) {}

protected:
	void setup() override {
		m_observables->emplace(std::make_pair("Name", m_name));
		m_observables->emplace(std::make_pair("Color", "red"));
	}

	bool perception_filter(const cam::ObservablesPointer& p_observed) const override {
		return p_observed->at("Color") == "green";
	}
};

class MyAgent2 final : public MyAgent {
public:
	explicit MyAgent2(const std::string& p_name) : MyAgent(p_name) {}

protected:
	void setup() override {
		m_observables->emplace(std::make_pair("Name", m_name));
		m_observables->emplace(std::make_pair("Color", "green"));
	}

	bool perception_filter(const cam::ObservablesPointer& p_observed) const override {
		return p_observed->at("Color") == "blue";
	}
};

class MyAgent3 final : public MyAgent {
public:

	explicit MyAgent3(const std::string& p_name) : MyAgent(p_name) {}

protected:
	void setup() override {
		m_observables->emplace(std::make_pair("Name", m_name));
		m_observables->emplace(std::make_pair("Color", "blue"));
	}

	bool perception_filter(const cam::ObservablesPointer& p_observed) const override {
		return p_observed->at("Color") == "red";
	}
};

int main() {
    cam::EnvironmentMas l_environment(10, cam::EnvironmentMasMode::Sequential);

    l_environment.add<MyAgent1>("Agent1");
    l_environment.add<MyAgent2>("Agent2");
    l_environment.add<MyAgent3>("Agent3");

    l_environment.start();
}
