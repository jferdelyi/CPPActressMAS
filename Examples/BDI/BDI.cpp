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

// Port of the C# example E02 BDI Architecture (ActressMAS v3.1): a patrol walks along a terrain of 15 cells and puts
// out the fires that start on it, with water taken from the left end. The patrol is a BDI agent: beliefs revised from
// its percepts, desires, one intention, and a plan of actions sent to the terrain one by one.
// The C# draws the terrain in a form; this port prints, each time the form would be refreshed, the line
// "GUI pos=<patrol position> <cells>" with . normal, F fire, W water, w water being drawn.
// Arguments: [turns, 0 for no end] [delay after each turn in ms], 0 and 250 by default as the C#.

#include <array>
#include <cstdlib>
#include <deque>
#include <exception>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <DotNet.hpp>
#include <Environment.h>

enum class TerrainState {
	Normal,
	Fire,
	Water,
	GettingWater,
	None
};

class TerrainAgent final : public cam::Agent {
	private:
		int m_patrol_position = 0;
		std::vector<TerrainState> m_states;
		int m_turns = 0;
		int m_size = 0;

		// None beyond both ends (D84: every access to the terrain is bounded)
		[[nodiscard]] TerrainState state_at(const int p_position) const {
			return p_position < 0 || p_position >= m_size ? TerrainState::None : m_states[p_position];
		}

		void update_visual_field(const cam::AgentId& p_sender) const {
			std::string l_percepts = "percepts " + std::to_string(m_patrol_position);
			for (int i = m_patrol_position - 1; i <= m_patrol_position + 1; i++) {  // left, current, right
				l_percepts += " " + std::to_string(static_cast<int>(state_at(i)));
			}
			send(p_sender, l_percepts);
		}

		// What the form of the C# would draw
		void update_terrain_gui() const {
			std::string l_cells;
			for (const auto l_state: m_states) {
				switch (l_state) {
					case TerrainState::Fire: l_cells += 'F'; break;
					case TerrainState::Water: l_cells += 'W'; break;
					case TerrainState::GettingWater: l_cells += 'w'; break;
					default: l_cells += '.'; break;
				}
			}
			std::cout << "GUI pos=" << m_patrol_position << " " << l_cells << std::endl;
		}

	public:
		explicit TerrainAgent(const std::string& p_name) : Agent(p_name) {}

		void setup() override {
			std::cout << "Starting " << get_name() << std::endl;

			m_size = get_environment()->get_global_data()["Size"];

			m_states.assign(m_size, TerrainState::Normal);
			m_states[0] = TerrainState::Water;

			m_patrol_position = 0;
		}

		void action(const cam::MessagePointer& p_message) override {
			try {
				std::cout << "\t" + format(p_message) << std::endl;
				const auto [l_action, l_parameters] = p_message->parse_rest();

				for (auto& l_state: m_states) {
					if (l_state == TerrainState::GettingWater) {
						l_state = TerrainState::Water;
					}
				}

				if (l_action == "move") {
					if (m_turns == 4) {
						m_states[4] = TerrainState::Fire;  // at some point, start a fire
					}
					if (m_turns == 20) {
						m_states[7] = TerrainState::Fire;
					}
					if (m_turns == 50) {
						m_states[9] = TerrainState::Fire;
					}

					// The patrol stays on the terrain (D84): the C# would leave it, then fail on the next access
					if (l_parameters == "right" && m_patrol_position < m_size - 1) {
						m_patrol_position++;
					} else if (l_parameters == "left" && m_patrol_position > 0) {
						m_patrol_position--;
					}

					update_visual_field(p_message->get_sender());
					m_turns++;
				} else if (l_action == "get-water") {
					if (state_at(m_patrol_position) == TerrainState::Water) {
						m_states[m_patrol_position] = TerrainState::GettingWater;
						send(p_message->get_sender(), "got-water");
					}
					update_visual_field(p_message->get_sender());
				} else if (l_action == "drop-water") {
					if (state_at(m_patrol_position) == TerrainState::Fire) {
						m_states[m_patrol_position] = TerrainState::Normal;
						send(p_message->get_sender(), "fire-out");
					}
					// Percepts in both cases, as get-water (D84): without them the C# leaves the patrol waiting forever
					update_visual_field(p_message->get_sender());
				}

				update_terrain_gui();
			} catch (const std::exception& p_exception) {
				std::cout << p_exception.what() << std::endl;
			}
		}
};

class PatrolAgent final : public cam::Agent {
	private:
		std::map<std::string, int> m_beliefs;
		std::set<std::string> m_desires;
		std::string m_intention;  // only 1 intention active in this model
		std::deque<std::string> m_plan;
		bool m_need_to_replan = false;
		int m_size = 0;

		void belief_revision(const std::vector<std::string>& p_parameters) {
			m_beliefs["position"] = cam::dotnet::to_int32(p_parameters.at(0));

			constexpr int l_visual_field_size = 3;
			std::array<TerrainState, l_visual_field_size> l_visual_field{};
			for (int i = 0; i < l_visual_field_size; i++) {
				l_visual_field[i] = static_cast<TerrainState>(cam::dotnet::to_int32(p_parameters.at(i + 1)));
			}

			// "see" and update beliefs

			if (m_intention == "extinguish-fire") {
				if (!m_plan.empty()) {  // plan in progress
					return;
				}
				// plan finished
				bool l_fire_out = true;
				for (const auto l_state: l_visual_field) {
					if (l_state == TerrainState::Fire) {
						l_fire_out = false;
					}
				}
				if (l_fire_out) {
					m_beliefs["fire"] = -1;
				}
			}

			for (int i = 0; i < l_visual_field_size; i++) {
				if (l_visual_field[i] == TerrainState::Water) {
					m_beliefs["water"] = m_beliefs["position"] + i - 1;
				}
				if (l_visual_field[i] == TerrainState::Fire) {
					m_beliefs["fire"] = m_beliefs["position"] + i - 1;
				}
			}
		}

		void generate_options() {
			if (m_desires.empty()) {
				m_desires.insert("patrol-right");
			}

			if (m_intention == "extinguish-fire" && !m_plan.empty()) {  // plan in progress
				return;
			}

			if (m_beliefs["position"] == m_size - 1) {  // at right end, turn left
				m_desires.erase("patrol-right");
				m_desires.insert("patrol-left");
			} else if (m_beliefs["position"] == 0) {  // at left end, turn right
				m_desires.erase("patrol-left");
				m_desires.insert("patrol-right");
			}

			if (m_beliefs["fire"] != -1) {  // fire position is known
				m_desires.insert("extinguish-fire");
			} else {  // the fire has been extinguished
				m_desires.erase("extinguish-fire");
			}
		}

		void filter_desires() {
			std::string l_new_intention;

			if (m_desires.contains("extinguish-fire")) {
				l_new_intention = "extinguish-fire";
			} else if (m_desires.contains("patrol-left")) {
				l_new_intention = "patrol-left";
			} else if (m_desires.contains("patrol-right")) {
				l_new_intention = "patrol-right";
			}

			if (l_new_intention != m_intention) {
				m_intention = l_new_intention;
				m_need_to_replan = true;

				std::cout << "Adopting new intention: " << m_intention << std::endl;
			}
		}

		void make_plan() {
			m_plan.clear();
			m_need_to_replan = false;

			if (m_intention == "patrol-right") {
				for (int i = m_beliefs["position"]; i < m_size; i++) {
					m_plan.emplace_back("move right");
				}
			} else if (m_intention == "patrol-left") {
				for (int i = m_beliefs["position"]; i >= 0; i--) {
					m_plan.emplace_back("move left");
				}
			} else if (m_intention == "extinguish-fire") {
				// 4 sub-goals: go to water, get water, go to fire, drop water

				// go to water
				for (int i = m_beliefs["position"]; i > m_beliefs["water"]; i--) {
					m_plan.emplace_back("move left");
				}

				// get water
				m_plan.emplace_back("get-water");

				// go to fire, this assumes that the position of water < position of fire
				for (int i = m_beliefs["water"]; i < m_beliefs["fire"]; i++) {
					m_plan.emplace_back("move right");
				}

				// drop water
				m_plan.emplace_back("drop-water");
			}
		}

		void execute_action() {
			if (m_plan.empty()) {  // plan finished
				// The C# drops the intention, then reads the empty plan and stops for good on the exception (D84):
				// the intention is dropped and the patrol deliberates again
				m_intention.clear();
				filter_desires();
				make_plan();
				if (m_plan.empty()) {
					return;
				}
			}

			const std::string l_action = m_plan.front();
			m_plan.pop_front();

			send_by_name("terrain", l_action);
		}

	public:
		explicit PatrolAgent(const std::string& p_name) : Agent(p_name) {}

		void setup() override {
			std::cout << "Starting " << get_name() << std::endl;

			m_size = get_environment()->get_global_data()["Size"];

			m_beliefs["position"] = 0;
			m_beliefs["have-water"] = 0;
			m_beliefs["fire"] = -1;  // unknown position
			m_beliefs["water"] = -1;  // unknown position

			send_by_name("terrain", "move right");
		}

		void action(const cam::MessagePointer& p_message) override {
			try {
				std::cout << "\t" + format(p_message) << std::endl;
				const auto [l_action, l_parameters] = p_message->parse();

				if (l_action == "percepts") {
					belief_revision(l_parameters);
					generate_options();
					filter_desires();
					if (m_need_to_replan) {  // if the environment is very dynamic, one can replan after each perception act
						make_plan();
					}
					execute_action();
				} else if (l_action == "got-water") {
					m_beliefs["have-water"] = 1;
				} else if (l_action == "fire-out") {
					m_beliefs["have-water"] = 0;
				}
			} catch (const std::exception& p_exception) {
				std::cout << p_exception.what() << std::endl;
			}
		}
};

int main(int argc, char** argv) {
	const int l_turns = argc > 1 ? std::atoi(argv[1]) : 0;
	const int l_delay = argc > 2 ? std::atoi(argv[2]) : 250;

	cam::Environment l_environment({.m_turns = l_turns, .m_mode = cam::EnvironmentMasMode::Sequential, .m_delay_after_turn = l_delay});
	l_environment.add<TerrainAgent>("terrain");
	l_environment.add<PatrolAgent>("patrol");
	l_environment.set_global_data("Size", 15);
	l_environment.start();
	return 0;
}
