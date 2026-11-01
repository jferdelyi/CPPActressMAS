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

// Flockers benchmark of Antelmi et al., "Experimenting with Agent-Based Model Simulation Tools", Applied Sciences
// 13(1), 13, 2023 (https://github.com/isislab-unisa/ABM_Comparison, ActressMAS/flockers, Apache-2.0), ported from that
// C# with the driver and the corrections of its C# counterpart (tools/csharp-ref/ref/Abm/Flockers): one agent class
// (the C# names it AntAgent in World.cs and Program.cs, FlockAgent in FlockAgent.cs), and the neighbourhood bounded by
// the side of the field instead of 1131, written by hand in the C# for the largest field. Each bird moves at every turn
// from the avoidance, cohesion, consistency, randomness and momentum of the birds in the 8 cells around it, as published
// (consistency divided twice by the count, avoidance divided by an integer), then takes the next free cell.
// --agents N (1 000 by default) on a field of side floor(sqrt(10 N)), Table 8 of the article (density about 10 %);
// sequential in a random order, 200 turns. The stopwatch covers the agents and start(), as the C#; the elapsed time is
// printed in ms. --seed N seeds the generators.

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <memory>
#include <string>
#include <vector>

#include <util/DotnetRandom.h>

#include "common.hpp"

namespace {

	struct Settings {
		static constexpr int s_no_turns = 200;
		static inline int s_grid_size = 100;
		static inline int s_no_ants = 1000;  // the birds
	};

	enum class CellState { Empty, Ant };

	class FlockAgent;

	struct Cell {
		CellState m_state = CellState::Empty;
		FlockAgent* m_agent_in_cell = nullptr;
	};

	class World {
	private:
		std::vector<Cell> m_map;
		int m_current_id = 0;

		[[nodiscard]] static std::size_t index(const int p_line, const int p_column) {
			return static_cast<std::size_t>(p_line) * static_cast<std::size_t>(Settings::s_grid_size) + static_cast<std::size_t>(p_column);
		}

	public:
		World() : m_map(index(Settings::s_grid_size, 0)) {}

		[[nodiscard]] const Cell& cell(const int p_line, const int p_column) const {
			return m_map[index(p_line, p_column)];
		}

		bool add_agent_to_map(FlockAgent& p_agent, int p_line, int p_column);

		std::string create_name() {
			return "a" + std::to_string(m_current_id++);
		}

		void count_insects(int& p_no_ants) const {
			p_no_ants = 0;
			for (const Cell& l_cell: m_map) {
				if (l_cell.m_state == CellState::Ant) {
					p_no_ants++;
				}
			}
		}

		bool move(FlockAgent& p_agent, int p_new_line, int p_new_column);
	};

	// The world of the environment (its Memory["World"] in the C#) and the static Random of InsectAgent
	std::unique_ptr<World> s_world;
	std::unique_ptr<cam::DotnetRandom> s_insect_rand;

	class FlockAgent final : public cam::Agent {
	private:
		static constexpr double s_cohesion = 0.8;
		static constexpr double s_avoidance = 1.0;
		static constexpr double s_randomness = 1.1;
		static constexpr double s_consistency = 0.7;
		static constexpr double s_momentum = 1.0;
		static constexpr double s_jump = 0.7;

		World* m_world = nullptr;

		bool step();

	public:
		double m_x = 0.0;
		double m_y = 0.0;
		double m_last_x = 0.0;
		double m_last_y = 0.0;
		int m_line = 0;  // position on the grid
		int m_column = 0;  // position on the grid

		// Program.cs: the coordinates, no last move, and AddAgentToMap into the free cell checked by the caller
		FlockAgent(const std::string& p_name, const double p_x, const double p_y) :
				Agent(p_name),
				m_x(p_x),
				m_y(p_y) {
			s_world->add_agent_to_map(*this, static_cast<int>(std::floor(p_x)), static_cast<int>(std::floor(p_y)));
		}

		void setup() override {
			m_world = s_world.get();
		}

		void default_action() override {
			step();
		}
	};

	bool World::add_agent_to_map(FlockAgent& p_agent, const int p_line, const int p_column) {
		Cell& l_cell = m_map[index(p_line, p_column)];
		if (l_cell.m_state == CellState::Ant) {
			return false;
		}

		l_cell.m_state = CellState::Ant;
		p_agent.m_line = p_line;
		p_agent.m_column = p_column;
		l_cell.m_agent_in_cell = &p_agent;
		return true;
	}

	bool World::move(FlockAgent& p_agent, const int p_new_line, const int p_new_column) {
		// moving the agent (the C# prints a trace for a cell off the field, which the bounds of step() rule out)
		Cell& l_to = m_map[index(p_new_line, p_new_column)];
		if (l_to.m_state == CellState::Ant) {
			return false;
		}

		Cell& l_from = m_map[index(p_agent.m_line, p_agent.m_column)];
		l_to = l_from;
		l_from = Cell();

		// updating agent position
		p_agent.m_line = p_new_line;
		p_agent.m_column = p_new_column;
		return true;
	}

	bool FlockAgent::step() {
		const int l_neigh_distance = 1;
		const int l_matrix_lenght = Settings::s_grid_size;  // 1131 in the published C#, for the largest field

		double l_avoid_x = 0.0;
		double l_cohe_x = 0.0;
		double l_cons_x = 0.0;
		double l_avoid_y = 0.0;
		double l_cohe_y = 0.0;
		double l_cons_y = 0.0;
		int l_count = 0;

		const int l_my_x = static_cast<int>(std::floor(m_x));
		const int l_my_y = static_cast<int>(std::floor(m_y));

		for (int i = l_my_x - l_neigh_distance; i <= l_my_x + l_neigh_distance; i++) {
			for (int j = l_my_y - l_neigh_distance; j <= l_my_y + l_neigh_distance; j++) {
				if (i >= 0 && i < l_matrix_lenght && j >= 0 && j < l_matrix_lenght && (i != l_my_x || j != l_my_y)) {
					const Cell& l_cell = m_world->cell(i, j);
					if (l_cell.m_state == CellState::Ant) {
						l_count++;
						const double l_dx = m_x - l_cell.m_agent_in_cell->m_line;
						const double l_dy = m_y - l_cell.m_agent_in_cell->m_column;
						const double l_square = l_dx * l_dx + l_dy * l_dy;

						// Divided by an integer, as the (int) of the C#
						l_avoid_x += l_dx / static_cast<int>(l_square * l_square + 1.0);
						l_avoid_y += l_dy / static_cast<int>(l_square * l_square + 1.0);

						l_cohe_x += l_dx;
						l_cohe_y += l_dy;

						l_cons_x += l_cell.m_agent_in_cell->m_last_x;
						l_cons_y += l_cell.m_agent_in_cell->m_last_y;
					}
				}
			}
		}

		if (l_count > 0) {
			l_avoid_x /= l_count;
			l_avoid_y /= l_count;
			l_cohe_x /= l_count;
			l_cohe_y /= l_count;
			l_cons_x /= l_count;
			l_cons_y /= l_count;

			// Twice, as the C#
			l_cons_x /= l_count;
			l_cons_y /= l_count;
		}

		l_avoid_x *= 400.0;
		l_avoid_y *= 400.0;

		l_cohe_x = -l_cohe_x / 10.0;
		l_cohe_y = -l_cohe_y / 10.0;

		double l_rand_x = s_insect_rand->next_double() * 2.0 - 1.0;
		double l_rand_y = s_insect_rand->next_double() * 2.0 - 1.0;
		const double l_rand_square = std::sqrt(l_rand_x * l_rand_x + l_rand_y * l_rand_y);
		l_rand_x = 0.05 * l_rand_x / l_rand_square;
		l_rand_y = 0.05 * l_rand_y / l_rand_square;

		const double l_mom_x = m_last_x;
		const double l_mom_y = m_last_y;

		double l_ddx = s_cohesion * l_cohe_x + s_avoidance * l_avoid_x + s_consistency * l_cons_x + s_randomness * l_rand_x + s_momentum * l_mom_x;
		double l_ddy = s_cohesion * l_cohe_y + s_avoidance * l_avoid_y + s_consistency * l_cons_y + s_randomness * l_rand_y + s_momentum * l_mom_y;

		const double l_dis = std::sqrt(l_ddx * l_ddx + l_ddy * l_ddy);
		if (l_dis > 0.0) {
			l_ddx = l_ddx / l_dis * s_jump;
			l_ddy = l_ddy / l_dis * s_jump;
		}

		m_last_x = l_ddx;
		m_last_y = l_ddy;

		double l_loc_x = 0.0;
		double l_loc_y = 0.0;

		if (m_x + l_ddx > Settings::s_grid_size) {
			l_loc_x = m_x - l_ddx;
		} else {
			l_loc_x = m_x + l_ddx;
		}

		if (m_y + l_ddy > Settings::s_grid_size) {
			l_loc_y = m_y - l_ddy;
		} else {
			l_loc_y = m_y + l_ddy;
		}

		if (l_loc_x < 0) {
			l_loc_x = 0;
		}
		if (l_loc_y < 0) {
			l_loc_y = 0;
		}

		m_x = l_loc_x;
		m_y = l_loc_y;

		// Math.Round rounds half to even, as nearbyint in the default rounding mode
		int l_round_locx = static_cast<int>(std::nearbyint(l_loc_x));
		int l_round_locy = static_cast<int>(std::nearbyint(l_loc_y));

		if (l_round_locx >= Settings::s_grid_size) {
			l_round_locx = Settings::s_grid_size - 1;
		}
		if (l_round_locy >= Settings::s_grid_size) {
			l_round_locy = Settings::s_grid_size - 1;
		}

		// anti-collision system
		while (!m_world->move(*this, l_round_locx, l_round_locy)) {
			const int l_min = std::min(l_round_locx, l_round_locy);
			if (l_min == l_round_locx) {
				l_round_locx += 1;
			} else {
				l_round_locy += 1;
			}

			if (l_round_locx >= Settings::s_grid_size) {
				l_round_locx = 0;
			}
			if (l_round_locy >= Settings::s_grid_size) {
				l_round_locy = 0;
			}
		}

		return true;
	}

	class WorldEnvironment final : public cam::Environment {
	private:
		std::ofstream m_sw;

	public:
		WorldEnvironment(const int p_no_turns, const unsigned int p_seed) :
				Environment({.m_turns = p_no_turns, .m_mode = cam::EnvironmentMasMode::SequentialRandom, .m_seed = p_seed}),
				m_sw("world-state.txt") {
			m_sw << "Ants\n";
		}

		void turn_finished(const int p_turn) override {
			int l_no_ants = 0;
			s_world->count_insects(l_no_ants);
			std::cout << "Turn " << p_turn + 1 << ": " << l_no_ants << " ants\n";
			m_sw << "\t" << l_no_ants << "\n" << std::flush;
		}

		void simulation_finished() override {
			std::cout << "\r\nSimulation finished.\n";
			m_sw.close();
		}
	};

	// Fisher-Yates shuffle, from the static Random of the C# program (computed and unused, as in the C#)
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
	Settings::s_no_ants = l_options.m_agents;
	s_insect_rand = std::make_unique<cam::DotnetRandom>(l_options.m_seed + 1);
	cam::DotnetRandom l_rand(l_options.m_seed + 2);

	// The C# creates the world in the constructor of WorldEnvironment
	s_world = std::make_unique<World>();
	WorldEnvironment l_world_env(Settings::s_no_turns, static_cast<unsigned int>(l_options.m_seed + 3));

	const int l_no_cells = Settings::s_grid_size * Settings::s_grid_size;
	(void) random_permutation(l_no_cells, l_rand);
	const bench::Stopwatch l_stopwatch;

	int i = 0;
	while (i < Settings::s_no_ants) {
		const double l_x = l_rand.next_double() * Settings::s_grid_size;
		const double l_y = l_rand.next_double() * Settings::s_grid_size;
		const int l_xx = static_cast<int>(std::floor(l_x));
		const int l_yy = static_cast<int>(std::floor(l_y));

		if (s_world->cell(l_xx, l_yy).m_state != CellState::Ant) {
			l_world_env.add<FlockAgent>(s_world->create_name(), l_x, l_y);
			i++;
		}
	}

	l_world_env.start();
	const long long l_elapsed_us = l_stopwatch.elapsed_us();
	std::cout << std::fixed << std::setprecision(3) << static_cast<double>(l_elapsed_us) / 1000.0 << " ms\n";
	return 0;
}
