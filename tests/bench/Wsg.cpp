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

// Wolf, Sheep and Grass benchmark of Antelmi et al., "Experimenting with Agent-Based Model Simulation Tools", Applied
// Sciences 13(1), 13, 2023 (https://github.com/isislab-unisa/ABM_Comparison, ActressMAS/PredatorPreyConsole,
// Apache-2.0): the console Predator-Prey example of ActressMAS adapted by its authors, ported from that C# with the
// driver of its C# counterpart (tools/csharp-ref/ref/Abm/Wsg). The v3.1 has no public RunTurn, so the 200 turns run
// through start() and the grass grows in turn_finished(), after the populations are written, as between two RunTurn
// of the original. Wolves and sheep move at random and lose one energy per turn; a wolf eats an adjacent sheep (+20),
// a sheep the grass of its cell once grown (+4); each breeds into an empty adjacent cell with probability 0.4 (half
// its energy to the offspring, half kept) and dies without energy. A sheep that dies still eats and breeds in the same
// action, as in the C#. Settings.Verbose and ShowWorld are false in the C#: their output is left out.
// --agents N (1 000 by default): 60 % sheep and 40 % wolves (Settings.cs), on a field of side floor(sqrt(10 N)), Table
// 8 of the article (density about 10 %); sequential in a random order, 200 turns. The stopwatch covers the agents and
// start(), as the C#; the elapsed time is printed in ms. --seed N seeds the generators.

#include <cmath>
#include <fstream>
#include <iomanip>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <util/DotnetRandom.h>

#include "common.hpp"

namespace {

	struct Settings {
		static constexpr int s_no_turns = 200;
		static inline int s_grid_size = 100;
		static inline int s_no_ants = 600;  // the sheep
		static inline int s_no_doodlebugs = 400;  // the wolves
	};

	enum class CellState { Empty, Sheep, Doodlebug };

	enum class Direction { Up, Down, Left, Right };

	struct Cell {
		CellState m_state = CellState::Empty;
		cam::AgentId m_agent_in_cell;
	};

	class InsectAgent;

	class World {
	private:
		std::vector<Cell> m_map;
		std::vector<int> m_grassfield;
		int m_current_id = 0;
		cam::DotnetRandom m_rand;

		[[nodiscard]] static std::size_t index(const int p_line, const int p_column) {
			return static_cast<std::size_t>(p_line) * static_cast<std::size_t>(Settings::s_grid_size) + static_cast<std::size_t>(p_column);
		}

	public:
		explicit World(const int p_seed) :
				m_map(index(Settings::s_grid_size, 0)),
				m_grassfield(index(Settings::s_grid_size, 0)),
				m_rand(p_seed) {
			for (std::size_t i = 0; i < m_grassfield.size(); i++) {
				const double l_r = m_rand.next_double();
				m_grassfield[i] = l_r < 0.5 ? 20 : m_rand.next(20);
			}
		}

		std::string create_name(const bool p_sheep) {
			return (p_sheep ? "a" : "d") + std::to_string(m_current_id++);
		}

		// AddAgentToMap: the insect, created at that position, in its cell
		void place(const bool p_sheep, const cam::AgentId& p_id, const int p_line, const int p_column) {
			Cell& l_cell = m_map[index(p_line, p_column)];
			l_cell.m_state = p_sheep ? CellState::Sheep : CellState::Doodlebug;
			l_cell.m_agent_in_cell = p_id;
		}

		bool check_food(const int p_line, const int p_column) {
			// true if there is food
			int& l_grass = m_grassfield[index(p_line, p_column)];
			if (l_grass >= 20) {
				l_grass = 0;
				return true;
			}
			return false;
		}

		void count_insects(int& p_no_doodlebugs, int& p_no_sheeps) const {
			p_no_sheeps = 0;
			p_no_doodlebugs = 0;
			for (const Cell& l_cell: m_map) {
				if (l_cell.m_state == CellState::Doodlebug) {
					p_no_doodlebugs++;
				} else if (l_cell.m_state == CellState::Sheep) {
					p_no_sheeps++;
				}
			}
		}

		void update_grass() {
			for (int& l_grass: m_grassfield) {
				l_grass += 1;
			}
		}

		void move(InsectAgent& p_agent, int p_new_line, int p_new_column);

		// Breed: the offspring is created in the environment and placed on the map
		void breed(const InsectAgent& p_agent, cam::Environment& p_environment, int p_new_line, int p_new_column, int p_e);

		// Eat: the wolf moves into the cell of the sheep, whose id is returned
		cam::AgentId eat(InsectAgent& p_doodlebug, int p_new_line, int p_new_column);

		void die(const InsectAgent& p_agent);

		bool valid_movement(const InsectAgent& p_agent, Direction p_direction, CellState p_desired_state, int& p_new_line, int& p_new_column) const;
	};

	// The world of the environment (its Memory["World"] in the C#) and the static Random of InsectAgent
	std::unique_ptr<World> s_world;
	std::unique_ptr<cam::DotnetRandom> s_insect_rand;

	class InsectAgent : public cam::Agent {
	protected:
		int m_turns_survived = 0;
		World* m_world = nullptr;
		int m_energy = 0;

		bool try_to_move() {
			const auto l_direction = static_cast<Direction>(s_insect_rand->next(4));
			int l_new_line = 0;
			int l_new_column = 0;
			if (m_world->valid_movement(*this, l_direction, CellState::Empty, l_new_line, l_new_column)) {
				m_world->move(*this, l_new_line, l_new_column);
				return true;
			}
			return false;
		}

		bool try_to_eat() {
			return m_world->check_food(m_line, m_column);
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

			const int l_r = s_insect_rand->next(static_cast<std::int32_t>(l_allowed_directions.size()));
			m_world->valid_movement(*this, l_allowed_directions[static_cast<std::size_t>(l_r)], CellState::Empty, l_new_line, l_new_column);

			const double l_r2 = s_insect_rand->next_double();
			if (l_r2 < 0.4) {
				m_world->breed(*this, *get_environment(), l_new_line, l_new_column, m_energy / 2);  // Environment.Add(newInsect)
				return true;
			}

			return false;
		}

		void die() {
			// removing the insect
			m_world->die(*this);
			stop();
		}

	public:
		int m_line = 0;  // position on the grid
		int m_column = 0;  // position on the grid

		InsectAgent(const std::string& p_name, const int p_energy, const int p_line, const int p_column) :
				Agent(p_name),
				m_energy(p_energy),
				m_line(p_line),
				m_column(p_column) {}

		[[nodiscard]] virtual bool is_sheep() const = 0;
	};

	class Sheep final : public InsectAgent {
	private:
		void ant_action() {
			// move
			try_to_move();  // implemented in base class InsectAgent

			m_energy -= 1;
			if (m_energy <= 0) {
				die();
			}

			// eat
			if (try_to_eat()) {
				m_energy += 4;
			}

			// reproduce
			if (try_to_breed()) {
				m_energy /= 2;
			}
		}

	public:
		using InsectAgent::InsectAgent;

		[[nodiscard]] bool is_sheep() const override { return true; }

		void setup() override {
			m_turns_survived = 0;
			m_world = s_world.get();
		}

		void default_action() override {
			ant_action();
		}
	};

	class DoodlebugAgent final : public InsectAgent {
	private:
		int m_last_eaten = 0;

		void doodlebug_action() {
			m_turns_survived++;
			m_last_eaten++;

			// move
			try_to_move();

			m_energy -= 1;
			// eat
			if (try_to_eat()) {
				m_energy += 20;
			}

			// breed
			if (try_to_breed()) {  // implemented in base class InsectAgent
				m_energy /= 2;
			}

			// starve
			if (m_energy <= 0) {
				die();
			}
		}

		// Hides the grass of InsectAgent, as the private TryToEat of the C#: an adjacent sheep
		bool try_to_eat() {
			std::vector<Direction> l_allowed_directions;
			int l_new_line = 0;
			int l_new_column = 0;

			for (int i = 0; i < 4; i++) {
				if (m_world->valid_movement(*this, static_cast<Direction>(i), CellState::Sheep, l_new_line, l_new_column)) {
					l_allowed_directions.push_back(static_cast<Direction>(i));
				}
			}

			if (l_allowed_directions.empty()) {
				return false;
			}

			const int l_r = s_insect_rand->next(static_cast<std::int32_t>(l_allowed_directions.size()));
			m_world->valid_movement(*this, l_allowed_directions[static_cast<std::size_t>(l_r)], CellState::Sheep, l_new_line, l_new_column);

			const cam::AgentId l_sheep = m_world->eat(*this, l_new_line, l_new_column);
			get_environment()->remove(l_sheep);

			return true;
		}

	public:
		using InsectAgent::InsectAgent;

		[[nodiscard]] bool is_sheep() const override { return false; }

		void setup() override {
			m_turns_survived = 0;
			m_last_eaten = 0;
			m_world = s_world.get();
		}

		void default_action() override {
			doodlebug_action();
		}
	};

	void World::move(InsectAgent& p_agent, const int p_new_line, const int p_new_column) {
		// moving the agent
		Cell& l_from = m_map[index(p_agent.m_line, p_agent.m_column)];
		m_map[index(p_new_line, p_new_column)] = l_from;
		l_from = Cell();

		// updating agent position
		p_agent.m_line = p_new_line;
		p_agent.m_column = p_new_column;
	}

	void World::breed(const InsectAgent& p_agent, cam::Environment& p_environment, const int p_new_line, const int p_new_column, const int p_e) {
		const bool l_sheep = p_agent.is_sheep();
		const std::string l_name = create_name(l_sheep);
		const cam::AgentId l_id = l_sheep ? p_environment.add<Sheep>(l_name, p_e, p_new_line, p_new_column)
										  : p_environment.add<DoodlebugAgent>(l_name, p_e, p_new_line, p_new_column);
		place(l_sheep, l_id, p_new_line, p_new_column);
	}

	cam::AgentId World::eat(InsectAgent& p_doodlebug, const int p_new_line, const int p_new_column) {
		Cell& l_sheep_cell = m_map[index(p_new_line, p_new_column)];
		const cam::AgentId l_sheep = l_sheep_cell.m_agent_in_cell;

		// moving the wolf
		Cell& l_from = m_map[index(p_doodlebug.m_line, p_doodlebug.m_column)];
		l_sheep_cell.m_state = CellState::Doodlebug;
		l_sheep_cell.m_agent_in_cell = l_from.m_agent_in_cell;
		l_from = Cell();

		// updating wolf position
		p_doodlebug.m_line = p_new_line;
		p_doodlebug.m_column = p_new_column;

		return l_sheep;
	}

	void World::die(const InsectAgent& p_agent) {
		m_map[index(p_agent.m_line, p_agent.m_column)] = Cell();
	}

	bool World::valid_movement(const InsectAgent& p_agent, const Direction p_direction, const CellState p_desired_state, int& p_new_line,
							   int& p_new_column) const {
		const int l_current_line = p_agent.m_line;
		const int l_current_column = p_agent.m_column;
		p_new_line = l_current_line;
		p_new_column = l_current_column;

		switch (p_direction) {
			case Direction::Up:
				if (l_current_line == 0) {
					return false;
				}
				if (m_map[index(l_current_line - 1, l_current_column)].m_state != p_desired_state) {
					return false;
				}
				p_new_line = l_current_line - 1;
				return true;

			case Direction::Down:
				if (l_current_line == Settings::s_grid_size - 1) {
					return false;
				}
				if (m_map[index(l_current_line + 1, l_current_column)].m_state != p_desired_state) {
					return false;
				}
				p_new_line = l_current_line + 1;
				return true;

			case Direction::Left:
				if (l_current_column == 0) {
					return false;
				}
				if (m_map[index(l_current_line, l_current_column - 1)].m_state != p_desired_state) {
					return false;
				}
				p_new_column = l_current_column - 1;
				return true;

			case Direction::Right:
				if (l_current_column == Settings::s_grid_size - 1) {
					return false;
				}
				if (m_map[index(l_current_line, l_current_column + 1)].m_state != p_desired_state) {
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
		WorldEnvironment(const int p_no_turns, const unsigned int p_seed) :
				Environment({.m_turns = p_no_turns, .m_mode = cam::EnvironmentMasMode::SequentialRandom, .m_seed = p_seed}),
				m_sw("world-state.txt") {
			m_sw << "Doodlebugs\tAnts\n";
		}

		void turn_finished(const int p_turn) override {
			int l_no_doodlebugs = 0;
			int l_no_sheeps = 0;
			s_world->count_insects(l_no_doodlebugs, l_no_sheeps);
			std::cout << "Turn " << p_turn + 1 << ": " << l_no_doodlebugs << " wolves, " << l_no_sheeps << " sheeps\n";
			m_sw << l_no_doodlebugs << "\t" << l_no_sheeps << "\n" << std::flush;
			// The grass grows after the turn, as between two RunTurn of the original program
			s_world->update_grass();
		}

		void simulation_finished() override {
			std::cout << "\r\nSimulation finished.\n";
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

} // namespace

int main(int argc, char** argv) {
	const bench::Options l_options = bench::parse(argc, argv, 1000);
	Settings::s_grid_size = static_cast<int>(std::floor(std::sqrt(10.0 * l_options.m_agents)));
	Settings::s_no_doodlebugs = l_options.m_agents * 2 / 5;
	Settings::s_no_ants = l_options.m_agents - Settings::s_no_doodlebugs;
	s_insect_rand = std::make_unique<cam::DotnetRandom>(l_options.m_seed + 1);
	cam::DotnetRandom l_rand(l_options.m_seed + 2);

	// The C# creates the world in the constructor of WorldEnvironment
	s_world = std::make_unique<World>(l_options.m_seed);
	WorldEnvironment l_world_env(Settings::s_no_turns, static_cast<unsigned int>(l_options.m_seed + 3));

	const int l_no_cells = Settings::s_grid_size * Settings::s_grid_size;
	const auto l_rand_vect = random_permutation(l_no_cells, l_rand);
	const bench::Stopwatch l_stopwatch;

	for (int i = 0; i < Settings::s_no_doodlebugs; i++) {
		const int l_e = l_rand.next(20);
		const std::string l_name = s_world->create_name(false);  // unique name
		const int l_line = l_rand_vect[static_cast<std::size_t>(i)] / Settings::s_grid_size;
		const int l_column = l_rand_vect[static_cast<std::size_t>(i)] % Settings::s_grid_size;
		s_world->place(false, l_world_env.add<DoodlebugAgent>(l_name, l_e, l_line, l_column), l_line, l_column);
	}

	for (int i = Settings::s_no_doodlebugs; i < Settings::s_no_doodlebugs + Settings::s_no_ants; i++) {
		const int l_e = l_rand.next(8);
		const std::string l_name = s_world->create_name(true);
		const int l_line = l_rand_vect[static_cast<std::size_t>(i)] / Settings::s_grid_size;
		const int l_column = l_rand_vect[static_cast<std::size_t>(i)] % Settings::s_grid_size;
		s_world->place(true, l_world_env.add<Sheep>(l_name, l_e, l_line, l_column), l_line, l_column);
	}

	l_world_env.start();
	const long long l_elapsed_us = l_stopwatch.elapsed_us();
	std::cout << std::fixed << std::setprecision(3) << static_cast<double>(l_elapsed_us) / 1000.0 << " ms\n";
	return 0;
}
