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
// oracle by iso.PredatorPreyGui.
// Port of the C# example E11 Predator-Prey, GUI version (ActressMAS v3.1): 2 doodlebugs and 100 ants on a 20 x 20 grid,
// 200 turns, the world being the environment itself. As in the oracle, the form is replaced: the simulation runs at
// once, the text box prints the turn it shows after each turn, then the populations it loads from world-state.txt; the
// list of the agent names is not printed. Sequential in a random order; the draws come from the seeds of the C# oracle.
// Arguments: [pause after each turn in ms], 100 by default as the C#.

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <numeric>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <DotNet.hpp>
#include <Environment.h>
#include <util/DotnetRandom.h>

struct Settings {
	static constexpr int s_grid_size = 20;
	static constexpr int s_no_turns = 200;
	static constexpr int s_no_ants = 100;
	static constexpr int s_no_doodlebugs = 2;
	static constexpr bool s_show_world = false;
	static constexpr bool s_verbose = false;
	static inline const std::string s_world_state_file_name = "world-state.txt";

	// Settings.RandNoGen, the static Random of the C#
	static cam::DotnetRandom& rand_no_gen() {
		static cam::DotnetRandom s_rand(cam::dotnet::next_site_seed("Settings.cs:31"));
		return s_rand;
	}

	// As Enumerable.Range(0, n).OrderBy(x => RandNoGen.Next()): one key drawn per element in order, then a stable sort
	static std::vector<int> random_permutation(const int p_n) {
		std::vector<std::int32_t> l_keys(static_cast<std::size_t>(p_n));
		for (auto& l_key: l_keys) {
			l_key = rand_no_gen().next();
		}
		std::vector<int> l_order(static_cast<std::size_t>(p_n));
		std::iota(l_order.begin(), l_order.end(), 0);
		std::stable_sort(l_order.begin(), l_order.end(), [&l_keys](const int p_a, const int p_b) {
			return l_keys[static_cast<std::size_t>(p_a)] < l_keys[static_cast<std::size_t>(p_b)];
		});
		return l_order;
	}
};

enum class CellState { Empty, Ant, Doodlebug };

enum class Direction { Up, Down, Left, Right };

struct Cell {
	cam::AgentId m_agent_in_cell;
	CellState m_state = CellState::Empty;
};

class InsectAgent;

class World final : public cam::Environment {
	private:
		int m_current_id = 0;
		std::vector<std::array<Cell, Settings::s_grid_size>> m_map;
		std::ofstream m_sw;
		int m_pause;

		[[nodiscard]] Cell& cell(const int p_line, const int p_column) {
			return m_map[static_cast<std::size_t>(p_line)][static_cast<std::size_t>(p_column)];
		}

	public:
		World(const int p_no_turns, const int p_pause) :
				Environment({.m_turns = p_no_turns,
							 .m_mode = cam::EnvironmentMasMode::SequentialRandom,
							 .m_seed = static_cast<unsigned int>(cam::dotnet::next_site_seed("EnvironmentMas.cs:61")),
							 .m_random = cam::RandomEngine::DotNet}),
				m_map(Settings::s_grid_size),
				m_sw(Settings::s_world_state_file_name),
				m_pause(p_pause) {
			m_sw << "Doodlebugs\tAnts\n";
		}

		// AddAgentToMap: the insect, created at that position, in its cell
		void add_agent_to_map(const bool p_ant, const cam::AgentId& p_id, const int p_line, const int p_column) {
			cell(p_line, p_column).m_state = p_ant ? CellState::Ant : CellState::Doodlebug;
			cell(p_line, p_column).m_agent_in_cell = p_id;
		}

		void breed(const InsectAgent& p_agent, int p_new_line, int p_new_column);

		void count_insects(int& p_no_doodlebugs, int& p_no_ants) const {
			p_no_ants = 0;
			p_no_doodlebugs = 0;
			for (const auto& l_row: m_map) {
				for (const auto& l_cell: l_row) {
					if (l_cell.m_state == CellState::Doodlebug) {
						p_no_doodlebugs++;
					} else if (l_cell.m_state == CellState::Ant) {
						p_no_ants++;
					}
				}
			}
		}

		std::string create_name(const bool p_ant) {
			return (p_ant ? "a" : "d") + std::to_string(m_current_id++);
		}

		void die(const InsectAgent& p_doodlebug);

		void eat(InsectAgent& p_doodlebug, int p_new_line, int p_new_column);

		void move(InsectAgent& p_agent, int p_new_line, int p_new_column);

		void simulation_finished() override {
			cam::dotnet::write_line("\r\nSimulation finished.");
			m_sw.close();
		}

		void turn_finished(const int p_turn) override {
			int l_no_doodlebugs = 0;
			int l_no_ants = 0;
			count_insects(l_no_doodlebugs, l_no_ants);
			m_sw << l_no_doodlebugs << "\t" << l_no_ants << "\n" << std::flush;

			// the controls of the form: the list of the agents is not printed, the text box prints its text
			if (agents_count() == 0) {
				return;
			}
			cam::dotnet::write_line("Turn " + std::to_string(p_turn) + " / " + std::to_string(Settings::s_no_turns));
			std::this_thread::sleep_for(std::chrono::milliseconds(m_pause));
		}

		bool valid_movement(const InsectAgent& p_agent, Direction p_direction, CellState p_desired_state, int& p_new_line, int& p_new_column);
};

class InsectAgent : public cam::Agent {
	protected:
		int m_turns_survived = 0;

		[[nodiscard]] World& world() const {
			return *static_cast<World*>(get_environment());
		}

		bool try_to_move() {
			const auto l_direction = static_cast<Direction>(Settings::rand_no_gen().next(4));
			int l_new_line = 0;  // ValidMovement computes newLine and newColumn
			int l_new_column = 0;
			if (world().valid_movement(*this, l_direction, CellState::Empty, l_new_line, l_new_column)) {
				if (Settings::s_verbose) {
					cam::dotnet::write_line("Moving " + get_name());
				}
				world().move(*this, l_new_line, l_new_column);
				return true;
			}
			return false;
		}

		bool try_to_breed() {
			std::vector<Direction> l_allowed_directions;
			int l_new_line = 0;
			int l_new_column = 0;

			for (int i = 0; i < 4; i++) {
				if (world().valid_movement(*this, static_cast<Direction>(i), CellState::Empty, l_new_line, l_new_column)) {
					l_allowed_directions.push_back(static_cast<Direction>(i));
				}
			}

			if (l_allowed_directions.empty()) {
				return false;
			}

			const int l_r = Settings::rand_no_gen().next(static_cast<std::int32_t>(l_allowed_directions.size()));
			world().valid_movement(*this, l_allowed_directions[static_cast<std::size_t>(l_r)], CellState::Empty, l_new_line, l_new_column);
			world().breed(*this, l_new_line, l_new_column);
			return true;
		}

	public:
		// position on the grid
		int m_line;
		int m_column;

		InsectAgent(const std::string& p_name, const int p_line, const int p_column) : Agent(p_name), m_line(p_line), m_column(p_column) {}

		[[nodiscard]] virtual bool is_ant() const = 0;
};

class AntAgent final : public InsectAgent {
	private:
		void ant_action() {
			// Move: every time step, the ant tries to move up, down, left or right at random; if the cell is occupied or
			// off the grid, it stays. Breed: an ant that survived three time steps breeds, after moving, into an empty
			// adjacent cell if there is one, then waits three more time steps.

			m_turns_survived++;

			// move
			try_to_move();  // implemented in base class InsectAgent

			// breed
			if (m_turns_survived >= 3) {
				if (try_to_breed()) {  // implemented in base class InsectAgent
					m_turns_survived = 0;
				}
			}
		}

	public:
		AntAgent(const std::string& p_name, const int p_line, const int p_column) : InsectAgent(p_name, p_line, p_column) {}

		[[nodiscard]] bool is_ant() const override { return true; }

		void setup() override {
			m_turns_survived = 0;

			if (Settings::s_verbose) {
				cam::dotnet::write_line("AntAgent " + get_name() + " started in (" + std::to_string(m_line) + "," + std::to_string(m_column) + ")");
			}
		}

		void default_action() override {
			try {
				ant_action();
			} catch (const std::exception& p_exception) {
				cam::dotnet::write_line(p_exception.what());
			}
		}
};

class DoodlebugAgent final : public InsectAgent {
	private:
		int m_last_eaten = 0;

		void doodlebug_action() {
			// Move: the doodlebug moves into an adjacent cell with an ant and eats it; without ants around, it moves as
			// an ant. Breed: a doodlebug that survived eight time steps breeds as an ant. Starve: a doodlebug that did
			// not eat for three time steps dies and leaves the grid.

			m_turns_survived++;
			m_last_eaten++;

			// eat
			const bool l_success = try_to_eat();
			if (l_success) {
				m_last_eaten = 0;
			}

			// move
			if (!l_success) {
				try_to_move();  // implemented in base class InsectAgent
			}

			// breed
			if (m_turns_survived >= 8) {
				if (try_to_breed()) {  // implemented in base class InsectAgent
					m_turns_survived = 0;
				}
			}

			// starve
			if (m_last_eaten >= 3) {
				die();
			}
		}

		bool try_to_eat() {
			std::vector<Direction> l_allowed_directions;
			int l_new_line = 0;
			int l_new_column = 0;

			for (int i = 0; i < 4; i++) {
				if (world().valid_movement(*this, static_cast<Direction>(i), CellState::Ant, l_new_line, l_new_column)) {
					l_allowed_directions.push_back(static_cast<Direction>(i));
				}
			}

			if (l_allowed_directions.empty()) {
				return false;
			}

			const int l_r = Settings::rand_no_gen().next(static_cast<std::int32_t>(l_allowed_directions.size()));
			world().valid_movement(*this, l_allowed_directions[static_cast<std::size_t>(l_r)], CellState::Ant, l_new_line, l_new_column);
			world().eat(*this, l_new_line, l_new_column);
			return true;
		}

		void die() {
			// removing the doodlebug
			if (Settings::s_verbose) {
				cam::dotnet::write_line("Removing " + get_name());
			}
			world().die(*this);
		}

	public:
		DoodlebugAgent(const std::string& p_name, const int p_line, const int p_column) : InsectAgent(p_name, p_line, p_column) {}

		[[nodiscard]] bool is_ant() const override { return false; }

		void setup() override {
			m_turns_survived = 0;
			m_last_eaten = 0;

			if (Settings::s_verbose) {
				cam::dotnet::write_line("DoodlebugAgent " + get_name() + " started in (" + std::to_string(m_line) + "," + std::to_string(m_column) + ")");
			}
		}

		void default_action() override {
			try {
				doodlebug_action();
			} catch (const std::exception& p_exception) {
				cam::dotnet::write_line(p_exception.what());
			}
		}
};

void World::breed(const InsectAgent& p_agent, const int p_new_line, const int p_new_column) {
	const bool l_ant = p_agent.is_ant();
	const std::string l_name = create_name(l_ant);
	const cam::AgentId l_id = l_ant ? add<AntAgent>(l_name, p_new_line, p_new_column)  // Add is from ActressMas.Environment
									: add<DoodlebugAgent>(l_name, p_new_line, p_new_column);
	add_agent_to_map(l_ant, l_id, p_new_line, p_new_column);

	if (Settings::s_verbose) {
		cam::dotnet::write_line("Breeding " + l_name);
	}
}

void World::die(const InsectAgent& p_doodlebug) {
	remove(cell(p_doodlebug.m_line, p_doodlebug.m_column).m_agent_in_cell);  // from ActressMas.Environment
	cell(p_doodlebug.m_line, p_doodlebug.m_column) = Cell();
}

void World::eat(InsectAgent& p_doodlebug, const int p_new_line, const int p_new_column) {
	// removing the ant
	const cam::AgentId l_ant = cell(p_new_line, p_new_column).m_agent_in_cell;
	remove(l_ant);  // from ActressMas.Environment

	if (Settings::s_verbose) {
		cam::dotnet::write_line("Removing " + get_agent_name(l_ant).value_or(""));
	}

	// moving the doodlebug
	if (Settings::s_verbose) {
		cam::dotnet::write_line("Moving " + p_doodlebug.get_name());
	}

	cell(p_new_line, p_new_column) = cell(p_doodlebug.m_line, p_doodlebug.m_column);
	cell(p_doodlebug.m_line, p_doodlebug.m_column) = Cell();

	// updating doodlebug position
	p_doodlebug.m_line = p_new_line;
	p_doodlebug.m_column = p_new_column;
}

void World::move(InsectAgent& p_agent, const int p_new_line, const int p_new_column) {
	// moving the agent
	cell(p_new_line, p_new_column) = cell(p_agent.m_line, p_agent.m_column);
	cell(p_agent.m_line, p_agent.m_column) = Cell();

	// updating agent position
	p_agent.m_line = p_new_line;
	p_agent.m_column = p_new_column;
}

bool World::valid_movement(const InsectAgent& p_agent, const Direction p_direction, const CellState p_desired_state, int& p_new_line,
						   int& p_new_column) {
	const int l_current_line = p_agent.m_line;
	const int l_current_column = p_agent.m_column;
	p_new_line = l_current_line;
	p_new_column = l_current_column;

	switch (p_direction) {
		case Direction::Up:
			if (l_current_line == 0 || cell(l_current_line - 1, l_current_column).m_state != p_desired_state) {
				return false;
			}
			p_new_line = l_current_line - 1;
			return true;

		case Direction::Down:
			if (l_current_line == Settings::s_grid_size - 1 || cell(l_current_line + 1, l_current_column).m_state != p_desired_state) {
				return false;
			}
			p_new_line = l_current_line + 1;
			return true;

		case Direction::Left:
			if (l_current_column == 0 || cell(l_current_line, l_current_column - 1).m_state != p_desired_state) {
				return false;
			}
			p_new_column = l_current_column - 1;
			return true;

		case Direction::Right:
			if (l_current_column == Settings::s_grid_size - 1 || cell(l_current_line, l_current_column + 1).m_state != p_desired_state) {
				return false;
			}
			p_new_column = l_current_column + 1;
			return true;
	}

	throw std::invalid_argument("Invalid direction");
}

int main(int argc, char** argv) {
	const int l_pause = argc > 1 ? std::atoi(argv[1]) : 100;

	// runMasButton_Click of the form, run at once
	World l_world_env(Settings::s_no_turns, l_pause);  // derived from ActressMas.TurnBasedEnvironment

	constexpr int l_no_cells = Settings::s_grid_size * Settings::s_grid_size;
	const auto l_rand_vect = Settings::random_permutation(l_no_cells);

	for (int i = 0; i < Settings::s_no_doodlebugs; i++) {
		const std::string l_name = l_world_env.create_name(false);  // unique name
		const int l_line = l_rand_vect[static_cast<std::size_t>(i)] / Settings::s_grid_size;
		const int l_column = l_rand_vect[static_cast<std::size_t>(i)] % Settings::s_grid_size;
		l_world_env.add_agent_to_map(false, l_world_env.add<DoodlebugAgent>(l_name, l_line, l_column), l_line, l_column);
	}

	for (int i = Settings::s_no_doodlebugs; i < Settings::s_no_doodlebugs + Settings::s_no_ants; i++) {
		const std::string l_name = l_world_env.create_name(true);
		const int l_line = l_rand_vect[static_cast<std::size_t>(i)] / Settings::s_grid_size;
		const int l_column = l_rand_vect[static_cast<std::size_t>(i)] % Settings::s_grid_size;
		l_world_env.add_agent_to_map(true, l_world_env.add<AntAgent>(l_name, l_line, l_column), l_line, l_column);
	}

	l_world_env.start();

	// richTextBox1.LoadFile: the text box prints the populations of world-state.txt
	std::ifstream l_file(Settings::s_world_state_file_name, std::ios::binary);
	std::stringstream l_content;
	l_content << l_file.rdbuf();
	cam::dotnet::write(l_content.str());
	return 0;
}
