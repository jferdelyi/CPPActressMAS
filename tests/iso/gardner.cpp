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

// The game of life of the port of E13 (iso_GameOfLife) against the published history of its patterns (Gardner, "The
// fantastic combinations of John Conway's new solitaire game life", Scientific American, October 1970): each pattern
// is written in the patterns.txt of a folder of its own, the port runs there, and every generation it shows is
// compared with a direct computation of Conway's rules on the same 25 x 25 grid; the facts of the article are checked
// on that computation. Every pattern stays at least four cells away from the edges, where the grid of E13 differs from
// the infinite plane of the article.

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32)
#define CAM_POPEN _popen
#define CAM_PCLOSE _pclose
#else
#define CAM_POPEN popen
#define CAM_PCLOSE pclose
#endif

namespace {

	constexpr int s_size = 25;

	using Cell = std::pair<int, int>;
	using Grid = std::set<Cell>;

	// Cells of a pattern drawn with 'x', its top left corner at (p_x, p_y)
	Grid place(const std::vector<std::string>& p_rows, const int p_x, const int p_y) {
		Grid l_grid;
		for (int y = 0; y < static_cast<int>(p_rows.size()); y++) {
			for (int x = 0; x < static_cast<int>(p_rows[y].size()); x++) {
				if (p_rows[y][x] == 'x') {
					l_grid.insert({p_x + x, p_y + y});
				}
			}
		}
		return l_grid;
	}

	// One generation of Conway's rules on the grid of E13: no cell beyond the edges
	Grid step(const Grid& p_grid) {
		Grid l_next;
		for (int x = 0; x < s_size; x++) {
			for (int y = 0; y < s_size; y++) {
				int l_neighbors = 0;
				for (int dx = -1; dx <= 1; dx++) {
					for (int dy = -1; dy <= 1; dy++) {
						if ((dx != 0 || dy != 0) && p_grid.contains({x + dx, y + dy})) {
							l_neighbors++;
						}
					}
				}
				const bool l_alive = p_grid.contains({x, y});
				if (l_neighbors == 3 || (l_alive && l_neighbors == 2)) {
					l_next.insert({x, y});
				}
			}
		}
		return l_next;
	}

	// Generations 0 to p_generations by the rules
	std::vector<Grid> history(Grid p_grid, const int p_generations) {
		std::vector<Grid> l_history{p_grid};
		for (int i = 0; i < p_generations; i++) {
			p_grid = step(p_grid);
			l_history.push_back(p_grid);
		}
		return l_history;
	}

	int margin(const Grid& p_grid) {
		int l_margin = s_size;
		for (const auto& [l_x, l_y]: p_grid) {
			l_margin = std::min({l_margin, l_x, l_y, s_size - 1 - l_x, s_size - 1 - l_y});
		}
		return l_margin;
	}

	Grid shifted(const Grid& p_grid, const int p_dx, const int p_dy) {
		Grid l_grid;
		for (const auto& [l_x, l_y]: p_grid) {
			l_grid.insert({l_x + p_dx, l_y + p_dy});
		}
		return l_grid;
	}

	// The generations the port shows: the grid printed after each "update" is the generation before it (the grid agent
	// copies the states of the cells in the turn where they update, as the C#), so the k-th one is generation k - 1
	std::vector<Grid> run_port(const Grid& p_pattern, const int p_generations, const std::string& p_name) {
		const std::filesystem::path l_folder = std::filesystem::temp_directory_path() / ("cam-gardner-" + p_name);
		std::filesystem::create_directories(l_folder);
		{
			std::ofstream l_patterns(l_folder / "patterns.txt");
			for (int y = 0; y < s_size; y++) {
				for (int x = 0; x < s_size; x++) {
					l_patterns << (x == 0 ? "" : "\t") << (p_pattern.contains({x, y}) ? "x" : ".");
				}
				l_patterns << "\n";
			}
		}
		// The setup turn, then two turns per generation (evolve, update), and one more to show the last one
		const int l_turns = 2 * p_generations + 4;
#if defined(_WIN32)
		const std::string l_command = "cd /d \"" + l_folder.string() + "\" && \"" + CAM_GAME_OF_LIFE + "\" " + std::to_string(l_turns) + " 0";
#else
		const std::string l_command = "cd \"" + l_folder.string() + "\" && \"" + CAM_GAME_OF_LIFE + "\" " + std::to_string(l_turns) + " 0";
#endif
		std::string l_output;
		FILE* l_pipe = CAM_POPEN(l_command.c_str(), "r");
		REQUIRE(l_pipe != nullptr);
		std::array<char, 4096> l_buffer{};
		for (std::size_t l_read; (l_read = std::fread(l_buffer.data(), 1, l_buffer.size(), l_pipe)) > 0;) {
			l_output.append(l_buffer.data(), l_read);
		}
		REQUIRE(CAM_PCLOSE(l_pipe) == 0);
		std::filesystem::remove_all(l_folder);

		std::vector<Grid> l_generations;
		std::istringstream l_lines(l_output);
		for (std::string l_line; std::getline(l_lines, l_line);) {
			if (l_line != "update") {
				continue;
			}
			Grid l_grid;
			for (int y = 0; y < s_size; y++) {
				std::string l_row;
				REQUIRE(std::getline(l_lines, l_row));
				REQUIRE(l_row.size() == static_cast<std::size_t>(s_size));
				for (int x = 0; x < s_size; x++) {
					if (l_row[x] == 'x') {
						l_grid.insert({x, y});
					}
				}
			}
			l_generations.push_back(l_grid);
		}
		return l_generations;
	}

	// The port shows every generation of the rules, and the pattern never comes near the edges
	std::vector<Grid> check_port(const Grid& p_pattern, const int p_generations, const std::string& p_name) {
		const std::vector<Grid> l_expected = history(p_pattern, p_generations);
		for (const auto& l_grid: l_expected) {
			REQUIRE(margin(l_grid) >= 4);
		}
		const std::vector<Grid> l_shown = run_port(p_pattern, p_generations, p_name);
		REQUIRE(l_shown.size() >= static_cast<std::size_t>(p_generations + 1));
		for (int i = 0; i <= p_generations; i++) {
			INFO("generation " << i);
			CHECK(l_shown[i] == l_expected[i]);
		}
		return l_expected;
	}

} // namespace

TEST_SUITE("the game of life of E13 follows the patterns of Gardner (1970)") {
	TEST_CASE("the block is a still life") {
		const auto l_history = check_port(place({"xx", "xx"}, 11, 11), 3, "block");
		CHECK(l_history[3] == l_history[0]);
	}

	TEST_CASE("a straight tetromino becomes a beehive, still, on the second move") {
		const auto l_history = check_port(place({"xxxx"}, 11, 12), 4, "beehive");
		CHECK(l_history[2] == place({".xx.", "x..x", ".xx."}, 11, 11));
		CHECK(l_history[3] == l_history[2]);
		CHECK(l_history[4] == l_history[2]);
	}

	TEST_CASE("the blinker oscillates with period 2") {
		const auto l_history = check_port(place({"xxx"}, 11, 12), 4, "blinker");
		CHECK(l_history[1] != l_history[0]);
		CHECK(l_history[2] == l_history[0]);
		CHECK(l_history[4] == l_history[0]);
	}

	TEST_CASE("the glider moves one cell down and to the right every four moves") {
		const Grid l_glider = place({".x.", "..x", "xxx"}, 5, 5);
		const auto l_history = check_port(l_glider, 8, "glider");
		CHECK(l_history[4] == shifted(l_glider, 1, 1));
		CHECK(l_history[8] == shifted(l_glider, 2, 2));
	}

	TEST_CASE("the T tetromino becomes the traffic lights, four blinkers, after nine moves") {
		const auto l_history = check_port(place({"xxx", ".x."}, 11, 11), 11, "traffic-lights");
		CHECK(l_history[9].size() == 12);
		CHECK(l_history[11] == l_history[9]);
		CHECK(l_history[10] != l_history[9]);
	}

	TEST_CASE("a row of five counters ends in the traffic lights") {
		const auto l_history = check_port(place({"xxxxx"}, 10, 12), 8, "row-5");
		CHECK(l_history[6].size() == 12);
		CHECK(l_history[8] == l_history[6]);
	}

	TEST_CASE("a row of seven counters becomes the honey farm, four beehives, after fourteen moves") {
		const auto l_history = check_port(place({"xxxxxxx"}, 9, 12), 15, "honey-farm");
		CHECK(l_history[14].size() == 24);
		CHECK(l_history[15] == l_history[14]);
		CHECK(l_history[13] != l_history[14]);
	}

	TEST_CASE("a row of ten counters leads to the pentadecathlon, with a period of fifteen") {
		const auto l_history = check_port(place({"xxxxxxxxxx"}, 7, 12), 18, "pentadecathlon");
		CHECK(l_history[17] == l_history[2]);
		CHECK(l_history[18] == l_history[3]);
		for (int i = 3; i < 17; i++) {
			CHECK(l_history[i] != l_history[2]);
		}
	}

	TEST_CASE("the heptomino generates the pulsar CP 48-56-72, with a period of three, in thirty-two moves") {
		const auto l_history = check_port(place({"xxxxx", "x...x"}, 10, 11), 35, "pulsar");
		CHECK(l_history[32].size() == 48);
		CHECK(l_history[33].size() == 56);
		CHECK(l_history[34].size() == 72);
		CHECK(l_history[35] == l_history[32]);
	}
}
