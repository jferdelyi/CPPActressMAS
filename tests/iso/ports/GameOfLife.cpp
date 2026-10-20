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
// oracle by iso.GameOfLife.
// Port of the C# example E13 Game of Life (ActressMAS v3.1): one agent per cell of a 25 x 25 grid, which sees the
// observable state of its 8 neighbours, and a grid agent that makes the cells evolve, then update, turn after turn.
// The initial pattern is read from patterns.txt in the working directory, as the C#.
// The C# draws the grid in a form; this port prints, each time the form would be refreshed, the grid it would show:
// one line per row, x for a living cell, . for a dead one. The grid copies the states of the cells in the turn where
// they update, before they do: what it shows lags one generation behind, as in the C#.
// Arguments: [turns, 0 for no end] [delay after each turn in ms], 0 and 10 by default as the C#.

#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include <DotNet.hpp>
#include <Environment.h>

enum class TurnType {
	Evolving,
	Updating
};

class CellAgent final : public cam::Agent {
	private:
		int m_x;
		int m_y;
		std::string m_state;
		std::string m_new_state;
		int m_no_living_neighbors = 0;

	public:
		CellAgent(const std::string& p_name, std::string p_state, const int p_x, const int p_y) :
				Agent(p_name, true),
				m_x(p_x),
				m_y(p_y),
				m_state(std::move(p_state)) {}

		void setup() override {
			// X and Y are numbers here, strings in the C#: the observables of the C++ hold JSON values
			(*m_observables)["Name"] = get_name();
			(*m_observables)["X"] = m_x;
			(*m_observables)["Y"] = m_y;
			(*m_observables)["State"] = m_state;  // "Living" or "Dead"
		}

		[[nodiscard]] bool perception_filter(const cam::ObservablesPointer& p_observed) const override {
			if (p_observed->at("Name") == "GridAgent") {  // the grid is also an agent, but has no cell properties
				return false;
			}

			// see only neighbors (8)
			const int l_observed_x = p_observed->at("X");
			const int l_observed_y = p_observed->at("Y");
			return std::abs(l_observed_x - m_x) <= 1 && std::abs(l_observed_y - m_y) <= 1;
		}

		void see(const std::vector<cam::ObservablesPointer>& p_observable_agents) override {
			try {
				m_no_living_neighbors = 0;
				for (const auto& l_observed: p_observable_agents) {
					if (l_observed->at("State") == "Living") {
						m_no_living_neighbors++;
					}
				}
			} catch (const std::exception& p_exception) {
				std::cout << p_exception.what() << std::endl;
			}
		}

		void action(const cam::MessagePointer& p_message) override {
			try {
				const std::string& l_content = p_message->as_text();
				if (l_content == "evolve") {
					if (m_state == "Living" && (m_no_living_neighbors == 2 || m_no_living_neighbors == 3)) {
						m_new_state = "Living";
					} else if (m_state == "Dead" && m_no_living_neighbors == 3) {
						m_new_state = "Living";
					} else {
						m_new_state = "Dead";
					}
				} else if (l_content == "update") {
					m_state = m_new_state;
					(*m_observables)["State"] = m_state;
				}
			} catch (const std::exception& p_exception) {
				std::cout << p_exception.what() << std::endl;
			}
		}
};

class GridAgent final : public cam::Agent {
	private:
		int m_size = 0;
		std::vector<std::vector<bool>> m_cell_states;
		TurnType m_turn_type = TurnType::Evolving;

		// What the form of the C# would draw
		void update_planet_gui() const {
			for (int y = 0; y < m_size; y++) {
				std::string l_row;
				for (int x = 0; x < m_size; x++) {
					l_row += m_cell_states[x][y] ? 'x' : '.';
				}
				std::cout << l_row << std::endl;
			}
		}

	public:
		explicit GridAgent(const std::string& p_name) : Agent(p_name, true) {}

		void setup() override {
			(*m_observables)["Name"] = get_name();

			m_size = get_environment()->get_global_data()["Size"];
			m_cell_states.assign(m_size, std::vector<bool>(m_size, false));
			m_turn_type = TurnType::Evolving;
		}

		[[nodiscard]] bool perception_filter(const cam::ObservablesPointer&) const override {
			return true;  // see all properties (position and state) of other cell agents
		}

		void see(const std::vector<cam::ObservablesPointer>& p_observable_agents) override {
			try {
				if (m_turn_type == TurnType::Evolving) {
					return;
				}

				for (const auto& l_observed: p_observable_agents) {
					const int l_x = l_observed->at("X");
					const int l_y = l_observed->at("Y");
					m_cell_states.at(l_x).at(l_y) = l_observed->at("State") == "Living";
				}
			} catch (const std::exception& p_exception) {
				std::cout << p_exception.what() << std::endl;
			}
		}

		void default_action() override {
			try {
				if (m_turn_type == TurnType::Evolving) {
					std::cout << "evolve" << std::endl;
					broadcast("evolve");
					m_turn_type = TurnType::Updating;
				} else {
					std::cout << "update" << std::endl;
					broadcast("update");
					m_turn_type = TurnType::Evolving;
				}

				update_planet_gui();
			} catch (const std::exception& p_exception) {
				std::cout << p_exception.what() << std::endl;
			}
		}
};

int main(int argc, char** argv) {
	const int l_turns = argc > 1 ? std::atoi(argv[1]) : 0;
	const int l_delay = argc > 2 ? std::atoi(argv[2]) : 10;

	cam::Environment l_environment({.m_turns = l_turns, .m_mode = cam::EnvironmentMasMode::Sequential, .m_delay_after_turn = l_delay});

	l_environment.add<GridAgent>("GridAgent");

	constexpr int l_size = 25;
	l_environment.set_global_data("Size", l_size);

	// The C# stops on an unhandled exception when the file is missing or too short
	std::ifstream l_patterns("patterns.txt");
	if (!l_patterns) {
		std::cerr << "patterns.txt not found in the working directory" << std::endl;
		return 1;
	}

	for (int y = 0; y < l_size; y++) {
		std::string l_line;
		const std::vector<std::string> l_line_tokens = std::getline(l_patterns, l_line) ? cam::dotnet::split(l_line) : std::vector<std::string>();
		if (l_line_tokens.size() < static_cast<std::size_t>(l_size)) {
			std::cerr << "patterns.txt: line " << y + 1 << " has less than " << l_size << " cells" << std::endl;
			return 1;
		}

		for (int x = 0; x < l_size; x++) {
			const std::string l_state = l_line_tokens[x] == "x" ? "Living" : "Dead";
			l_environment.add<CellAgent>("Agent-" + std::to_string(x) + "-" + std::to_string(y), l_state, x, y);
		}
	}

	l_environment.start();
	return 0;
}
