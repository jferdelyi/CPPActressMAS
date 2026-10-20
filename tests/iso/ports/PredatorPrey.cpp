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
// oracle by iso.PredatorPrey.
// Port of the C# example E11 Predator-Prey, console version (ActressMAS v3.1): 20 doodlebugs and 1 000 ants on a 50 x 50
// grid; the ants move and breed, the doodlebugs eat the ants, breed and starve; the environment prints the populations
// after each turn and writes them into world-state.txt. Sequential in a random order, 1 000 turns. The draws come from
// the seeds of the C# oracle. The C# keeps the world in the memory of the environment, a dictionary of any objects; the
// global data of the C++ environment is JSON, so the world is an object of the program; a cell keeps the identifier
// and the name of the insect in it instead of the object.

#include <array>
#include <cstdint>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <DotNet.hpp>
#include <Environment.h>
#include <util/DotnetRandom.h>

struct Settings {
	static constexpr int s_grid_size = 50;  // 20
	static constexpr int s_no_turns = 1000;
	static constexpr int s_no_ants = 1000;  // 100
	static constexpr int s_no_doodlebugs = 20;  // 2
	static constexpr bool s_show_world = false;
	static constexpr bool s_verbose = false;
};

enum class CellState { Empty, Ant, Doodlebug };

enum class Direction { Up, Down, Left, Right };

struct Cell {
	CellState m_state = CellState::Empty;
	std::optional<cam::AgentId> m_agent_in_cell;
	std::string m_name_in_cell;
};

class InsectAgent;

class World {
	private:
		std::vector<std::array<Cell, Settings::s_grid_size>> m_map;
		int m_current_id = 0;

	public:
		World() : m_map(Settings::s_grid_size) {}

		std::string create_name(const bool p_ant) {
			return (p_ant ? "a" : "d") + std::to_string(m_current_id++);
		}

		// AddAgentToMap: the insect, created at that position, in its cell
		void place(const bool p_ant, const cam::AgentId& p_id, const std::string& p_name, const int p_line, const int p_column) {
			Cell& l_cell = m_map[static_cast<std::size_t>(p_line)][static_cast<std::size_t>(p_column)];
			l_cell.m_state = p_ant ? CellState::Ant : CellState::Doodlebug;
			l_cell.m_agent_in_cell = p_id;
			l_cell.m_name_in_cell = p_name;
		}

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

		void move(InsectAgent& p_agent, int p_new_line, int p_new_column);

		// Breed: the offspring is created in the environment, placed on the map, and its id returned
		cam::AgentId breed(const InsectAgent& p_agent, cam::Environment& p_environment, int p_new_line, int p_new_column);

		// Eat: the doodlebug moves into the cell of the ant, whose id is returned
		cam::AgentId eat(InsectAgent& p_doodlebug, int p_new_line, int p_new_column);

		void die(const InsectAgent& p_doodlebug);

		bool valid_movement(const InsectAgent& p_agent, Direction p_direction, CellState p_desired_state, int& p_new_line, int& p_new_column) const;
};

World g_world;

class InsectAgent : public cam::Agent {
	protected:
		int m_turns_survived = 0;
		World* m_world = nullptr;

		// Shared by the insects, as the static Random of the C#
		static std::int32_t next(const std::int32_t p_max) {
			static cam::DotnetRandom s_rand(cam::dotnet::next_site_seed("InsectAgent.cs:27"));
			return s_rand.next(p_max);
		}

		bool try_to_move() {
			const auto l_direction = static_cast<Direction>(next(4));
			int l_new_line = 0;
			int l_new_column = 0;
			if (m_world->valid_movement(*this, l_direction, CellState::Empty, l_new_line, l_new_column)) {
				if (Settings::s_verbose) {
					cam::dotnet::write_line("Moving " + get_name());
				}
				m_world->move(*this, l_new_line, l_new_column);
				return true;
			}
			return false;
		}

		bool try_to_breed() {
			std::vector<Direction> l_allowed_directions;
			int l_new_line = 0;
			int l_new_column = 0;

			for (int i = 0; i < 4; i++) {
				if (m_world->valid_movement(*this, static_cast<Direction>(i), CellState::Empty, l_new_line, l_new_column)) {
					l_allowed_directions.push_back(static_cast<Direction>(i));
				}
			}

			if (l_allowed_directions.empty()) {
				return false;
			}

			const int l_r = next(static_cast<std::int32_t>(l_allowed_directions.size()));
			m_world->valid_movement(*this, l_allowed_directions[static_cast<std::size_t>(l_r)], CellState::Empty, l_new_line, l_new_column);
			m_world->breed(*this, *get_environment(), l_new_line, l_new_column);  // Environment.Add(newInsect)
			return true;
		}

	public:
		int m_line = 0;  // position on the grid
		int m_column = 0;  // position on the grid

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
			m_world = &g_world;

			if (Settings::s_verbose) {
				cam::dotnet::write_line("AntAgent " + get_name() + " started in (" + std::to_string(m_line) + "," + std::to_string(m_column) + ")");
			}
		}

		void default_action() override {
			ant_action();
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
				if (m_world->valid_movement(*this, static_cast<Direction>(i), CellState::Ant, l_new_line, l_new_column)) {
					l_allowed_directions.push_back(static_cast<Direction>(i));
				}
			}

			if (l_allowed_directions.empty()) {
				return false;
			}

			const int l_r = next(static_cast<std::int32_t>(l_allowed_directions.size()));
			m_world->valid_movement(*this, l_allowed_directions[static_cast<std::size_t>(l_r)], CellState::Ant, l_new_line, l_new_column);
			const cam::AgentId l_ant = m_world->eat(*this, l_new_line, l_new_column);
			get_environment()->remove(l_ant);
			return true;
		}

		void die() {
			// removing the doodlebug
			if (Settings::s_verbose) {
				cam::dotnet::write_line("Removing " + get_name());
			}
			m_world->die(*this);
			stop();
		}

	public:
		DoodlebugAgent(const std::string& p_name, const int p_line, const int p_column) : InsectAgent(p_name, p_line, p_column) {}

		[[nodiscard]] bool is_ant() const override { return false; }

		void setup() override {
			m_turns_survived = 0;
			m_last_eaten = 0;
			m_world = &g_world;

			if (Settings::s_verbose) {
				cam::dotnet::write_line("DoodlebugAgent " + get_name() + " started in (" + std::to_string(m_line) + "," + std::to_string(m_column) + ")");
			}
		}

		void default_action() override {
			doodlebug_action();
		}
};

void World::move(InsectAgent& p_agent, const int p_new_line, const int p_new_column) {
	// moving the agent
	Cell& l_from = m_map[static_cast<std::size_t>(p_agent.m_line)][static_cast<std::size_t>(p_agent.m_column)];
	Cell& l_to = m_map[static_cast<std::size_t>(p_new_line)][static_cast<std::size_t>(p_new_column)];
	l_to = l_from;
	l_from = Cell();

	// updating agent position
	p_agent.m_line = p_new_line;
	p_agent.m_column = p_new_column;
}

cam::AgentId World::breed(const InsectAgent& p_agent, cam::Environment& p_environment, const int p_new_line, const int p_new_column) {
	const bool l_ant = p_agent.is_ant();
	const std::string l_name = create_name(l_ant);
	const cam::AgentId l_id = l_ant ? p_environment.add<AntAgent>(l_name, p_new_line, p_new_column)
									: p_environment.add<DoodlebugAgent>(l_name, p_new_line, p_new_column);

	place(l_ant, l_id, l_name, p_new_line, p_new_column);

	if (Settings::s_verbose) {
		cam::dotnet::write_line("Breeding " + l_name);
	}

	return l_id;
}

cam::AgentId World::eat(InsectAgent& p_doodlebug, const int p_new_line, const int p_new_column) {
	Cell& l_ant_cell = m_map[static_cast<std::size_t>(p_new_line)][static_cast<std::size_t>(p_new_column)];
	const cam::AgentId l_ant = l_ant_cell.m_agent_in_cell.value();

	if (Settings::s_verbose) {
		cam::dotnet::write_line("Removing " + l_ant_cell.m_name_in_cell);
	}

	// moving the doodlebug
	if (Settings::s_verbose) {
		cam::dotnet::write_line("Moving " + p_doodlebug.get_name());
	}

	Cell& l_from = m_map[static_cast<std::size_t>(p_doodlebug.m_line)][static_cast<std::size_t>(p_doodlebug.m_column)];
	l_ant_cell.m_state = CellState::Doodlebug;
	l_ant_cell.m_agent_in_cell = l_from.m_agent_in_cell;
	l_ant_cell.m_name_in_cell = l_from.m_name_in_cell;
	l_from = Cell();

	// updating doodlebug position
	p_doodlebug.m_line = p_new_line;
	p_doodlebug.m_column = p_new_column;

	return l_ant;
}

void World::die(const InsectAgent& p_doodlebug) {
	m_map[static_cast<std::size_t>(p_doodlebug.m_line)][static_cast<std::size_t>(p_doodlebug.m_column)] = Cell();
}

bool World::valid_movement(const InsectAgent& p_agent, const Direction p_direction, const CellState p_desired_state, int& p_new_line,
						   int& p_new_column) const {
	const int l_current_line = p_agent.m_line;
	const int l_current_column = p_agent.m_column;
	p_new_line = l_current_line;
	p_new_column = l_current_column;

	const auto l_state = [this](const int p_line, const int p_column) {
		return m_map[static_cast<std::size_t>(p_line)][static_cast<std::size_t>(p_column)].m_state;
	};

	switch (p_direction) {
		case Direction::Up:
			if (l_current_line == 0) {
				return false;
			}
			if (l_state(l_current_line - 1, l_current_column) != p_desired_state) {
				return false;
			}
			p_new_line = l_current_line - 1;
			return true;

		case Direction::Down:
			if (l_current_line == Settings::s_grid_size - 1) {
				return false;
			}
			if (l_state(l_current_line + 1, l_current_column) != p_desired_state) {
				return false;
			}
			p_new_line = l_current_line + 1;
			return true;

		case Direction::Left:
			if (l_current_column == 0) {
				return false;
			}
			if (l_state(l_current_line, l_current_column - 1) != p_desired_state) {
				return false;
			}
			p_new_column = l_current_column - 1;
			return true;

		case Direction::Right:
			if (l_current_column == Settings::s_grid_size - 1) {
				return false;
			}
			if (l_state(l_current_line, l_current_column + 1) != p_desired_state) {
				return false;
			}
			p_new_column = l_current_column + 1;
			return true;
	}

	throw std::invalid_argument("Invalid direction");
}

class WorldEnvironment final : public cam::Environment {
	private:
		std::ofstream m_sw;

	public:
		explicit WorldEnvironment(const int p_no_turns) :
				Environment({.m_turns = p_no_turns,
							 .m_mode = cam::EnvironmentMasMode::SequentialRandom,
							 .m_seed = static_cast<unsigned int>(cam::dotnet::next_site_seed("EnvironmentMas.cs:61")),
							 .m_random = cam::RandomEngine::DotNet}),
				m_sw("world-state.txt") {
			m_sw << "Doodlebugs\tAnts\n";
		}

		void turn_finished(const int p_turn) override {
			int l_no_doodlebugs = 0;
			int l_no_ants = 0;
			g_world.count_insects(l_no_doodlebugs, l_no_ants);
			cam::dotnet::write_line("Turn " + std::to_string(p_turn + 1) + ": " + std::to_string(l_no_doodlebugs) + " doodlebugs, " + std::to_string(l_no_ants) +
									" ants");
			m_sw << l_no_doodlebugs << "\t" << l_no_ants << "\n" << std::flush;
			// Settings.ShowWorld is false: the map is never printed, nor ENTER awaited
		}

		void simulation_finished() override {
			cam::dotnet::write_line("\r\nSimulation finished.");
			m_sw.close();
		}
};

// Fisher-Yates shuffle, from the static Random of the C# program
std::vector<int> random_permutation(int p_n, cam::DotnetRandom& p_rand) {
	std::vector<int> l_numbers(static_cast<std::size_t>(p_n));
	for (int i = 0; i < p_n; i++) {
		l_numbers[static_cast<std::size_t>(i)] = i;
	}

	while (p_n > 1) {
		const int l_k = p_rand.next(p_n--);
		std::swap(l_numbers[static_cast<std::size_t>(p_n)], l_numbers[static_cast<std::size_t>(l_k)]);
	}

	return l_numbers;
}

int main() {
	cam::DotnetRandom l_rand(cam::dotnet::next_site_seed("Program.cs:23"));

	WorldEnvironment l_world_env(Settings::s_no_turns);  // derived from ActressMas.EnvironmentMas

	constexpr int l_no_cells = Settings::s_grid_size * Settings::s_grid_size;
	const auto l_rand_vect = random_permutation(l_no_cells, l_rand);

	for (int i = 0; i < Settings::s_no_doodlebugs; i++) {
		const std::string l_name = g_world.create_name(false);  // unique name
		const int l_line = l_rand_vect[static_cast<std::size_t>(i)] / Settings::s_grid_size;
		const int l_column = l_rand_vect[static_cast<std::size_t>(i)] % Settings::s_grid_size;
		g_world.place(false, l_world_env.add<DoodlebugAgent>(l_name, l_line, l_column), l_name, l_line, l_column);
	}

	for (int i = Settings::s_no_doodlebugs; i < Settings::s_no_doodlebugs + Settings::s_no_ants; i++) {
		const std::string l_name = g_world.create_name(true);
		const int l_line = l_rand_vect[static_cast<std::size_t>(i)] / Settings::s_grid_size;
		const int l_column = l_rand_vect[static_cast<std::size_t>(i)] % Settings::s_grid_size;
		g_world.place(true, l_world_env.add<AntAgent>(l_name, l_line, l_column), l_name, l_line, l_column);
	}

	l_world_env.start();
	return 0;
}
