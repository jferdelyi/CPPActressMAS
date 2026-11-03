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

// Port of the C# example E12 Simple Observables/ColorGame (ActressMAS v3.1): three agents publish a color; each sees
// the agents whose color is the one it looks for (its perception filter), says what it saw, then draws a new color.
// Sequential, fixed order, 10 turns; the colors are drawn as the C# draws them (the seed of its oracle, D96).

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include <DotNet.hpp>
#include <Environment.h>
#include <util/DotnetRandom.h>

class Colors {
	public:
		static std::string GenerateColor() {
			static const std::vector<std::string> s_colors{"red", "green", "blue"};
			static cam::DotnetRandom s_rand(cam::dotnet::next_site_seed("Program.cs:119"));
			return s_colors[s_rand.next(static_cast<std::int32_t>(s_colors.size()))];
		}
};

class MyAgent : public cam::Agent {
	private:
		std::vector<cam::ObservablesPointer> m_observable_agents;

	protected:
		explicit MyAgent(const std::string& p_name) : Agent(p_name, true) {}

	public:
		void see(const std::vector<cam::ObservablesPointer>& p_observable_agents) override {
			std::cout << "I am " << get_name() << ". I am looking around..." << std::endl;
			m_observable_agents = p_observable_agents;
		}

		void default_action() override {
			if (m_observable_agents.empty()) {
				std::cout << "I did't see anything interesting";
			} else {
				std::cout << "I saw: ";
				for (const auto& l_observed: m_observable_agents) {
					std::cout << l_observed->at("Name").get<std::string>() << " (" << l_observed->at("Color").get<std::string>() << ") ";
				}
			}

			(*m_observables)["Color"] = Colors::GenerateColor();

			std::cout << "\r\nMy color is now " << m_observables->at("Color").get<std::string>() << std::endl;
			std::cout << "-------------------------------------------" << std::endl;
		}
};

class Agent1 final : public MyAgent {
	public:
		explicit Agent1(const std::string& p_name) : MyAgent(p_name) {}

		void setup() override {
			(*m_observables)["Name"] = get_name();
			(*m_observables)["Color"] = "red";
		}

		[[nodiscard]] bool perception_filter(const cam::ObservablesPointer& p_observed) const override {
			return p_observed->at("Color") == "green";
		}
};

class Agent2 final : public MyAgent {
	public:
		explicit Agent2(const std::string& p_name) : MyAgent(p_name) {}

		void setup() override {
			(*m_observables)["Name"] = get_name();
			(*m_observables)["Color"] = "green";
		}

		[[nodiscard]] bool perception_filter(const cam::ObservablesPointer& p_observed) const override {
			return p_observed->at("Color") == "blue";
		}
};

class Agent3 final : public MyAgent {
	public:
		explicit Agent3(const std::string& p_name) : MyAgent(p_name) {}

		void setup() override {
			(*m_observables)["Name"] = get_name();
			(*m_observables)["Color"] = "blue";
		}

		[[nodiscard]] bool perception_filter(const cam::ObservablesPointer& p_observed) const override {
			return p_observed->at("Color") == "red";
		}
};

int main() {
	cam::Environment l_environment({.m_turns = 10,
									.m_mode = cam::EnvironmentMasMode::Sequential,
									.m_seed = static_cast<unsigned int>(cam::dotnet::next_site_seed("EnvironmentMas.cs:61")),
									.m_random = cam::RandomEngine::DotNet});

	l_environment.add<Agent1>("Agent1");
	l_environment.add<Agent2>("Agent2");
	l_environment.add<Agent3>("Agent3");

	l_environment.start();
	return 0;
}
