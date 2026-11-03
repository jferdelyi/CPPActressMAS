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

// Harness program of the oracle tests (tests/iso): the C# original as it is, its bugs included, compared with its
// oracle by iso.LrtaStar.
// Port of the C# example E03 LRTA Search (ActressMAS v3.1): a search agent finds its way from A to E on the map
// Pendulum by LRTA*, asking the map agent for the neighbors of each state and for the heuristics of the new ones.
// Parallel, no turn limit, 100 ms after each move. The map (Pendulum.grx) and its heuristics (Pendulum.grh) are read
// from the working directory, as the C#; the search agent draws among equal moves from the seed of the C# oracle.

#include <chrono>
#include <cstdint>
#include <exception>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <DotNet.hpp>
#include <Environment.h>
#include <util/DotnetRandom.h>

// As String.Split(char[] separators, StringSplitOptions.RemoveEmptyEntries)
std::vector<std::string> split_remove_empty(const std::string& p_text, const std::string& p_separators) {
	std::vector<std::string> l_words;
	std::string l_word;
	for (const char l_char: p_text) {
		if (p_separators.find(l_char) != std::string::npos) {
			if (!l_word.empty()) {
				l_words.push_back(l_word);
			}
			l_word.clear();
		} else {
			l_word += l_char;
		}
	}
	if (!l_word.empty()) {
		l_words.push_back(l_word);
	}
	return l_words;
}

// As String.Split(char separator): every piece, empty ones kept
std::vector<std::string> split_on(const std::string& p_text, const char p_separator) {
	std::vector<std::string> l_words(1);
	for (const char l_char: p_text) {
		if (l_char == p_separator) {
			l_words.emplace_back();
		} else {
			l_words.back() += l_char;
		}
	}
	return l_words;
}

// As StreamReader.ReadToEnd
std::string read_to_end(const std::string& p_path) {
	std::ifstream l_file(p_path, std::ios::binary);
	if (!l_file) {
		throw std::runtime_error("Could not find file '" + p_path + "'.");
	}
	std::stringstream l_buffer;
	l_buffer << l_file.rdbuf();
	return l_buffer.str();
}

class MapAgent final : public cam::Agent {
	private:
		std::map<std::string, std::string> m_neighbors;
		std::vector<std::string> m_heuristics;

		[[nodiscard]] std::string expand_state(const std::string& p_state) const {
			if (const auto l_it = m_neighbors.find(p_state); l_it != m_neighbors.end()) {
				return l_it->second;
			}
			return "";
		}

		[[nodiscard]] double heuristic_estimate(const std::string& p_current_state, const std::string& p_goal_state) const {
			for (const auto& l_heuristic: m_heuristics) {
				const auto l_toks = split_on(l_heuristic, '|');
				if (p_current_state == cam::dotnet::trim(l_toks[0]) && p_goal_state == cam::dotnet::trim(l_toks[1])) {
					return cam::dotnet::to_int32(l_toks[2]);
				}
			}
			return 0;
		}

		void handle_expand(const std::string& p_sender, const std::string& p_state) const {
			send_by_name(p_sender, "neighbors " + expand_state(p_state));
		}

		void handle_heuristics_query(const std::string& p_sender, const std::string& p_states) const {
			const auto l_toks = cam::dotnet::split(cam::dotnet::trim(p_states));

			// the map agent is not interested in the goal state of a search agent, that is why it receives it within the query
			const std::string& l_goal = l_toks[0];

			std::string l_reply;
			for (std::size_t i = 1; i < l_toks.size(); i++) {
				l_reply += cam::dotnet::to_string(heuristic_estimate(l_toks[i], l_goal)) + " ";
			}
			send_by_name(p_sender, "heuristicsReply " + l_reply);
		}

	public:
		explicit MapAgent(const std::string& p_name) : Agent(p_name) {}

		void setup() override {
			m_neighbors.clear();

			const std::string l_map_name = get_environment()->get_global_data().at("MapName").get<std::string>();

			const std::string l_graph = read_to_end(l_map_name + ".grx");

			const auto l_lines = split_remove_empty(l_graph, "\r\n");
			for (const auto& l_line: l_lines) {
				const auto l_toks = split_on(l_line, '=');
				// Dictionary.Add: an existing key is an error
				if (!m_neighbors.emplace(cam::dotnet::trim(l_toks[0]), cam::dotnet::trim(l_toks[1])).second) {
					throw std::invalid_argument("An item with the same key has already been added. Key: " + cam::dotnet::trim(l_toks[0]));
				}
			}

			const std::string l_all = read_to_end(l_map_name + ".grh");

			m_heuristics = split_remove_empty(l_all, "\r\n");
		}

		void action(const cam::MessagePointer& p_message) override {
			try {
				cam::dotnet::write_line("\t" + format(p_message));
				const auto [l_action, l_parameters] = p_message->parse_rest();

				if (l_action == "expand") {
					handle_expand(get_agent_name(p_message->get_sender()).value_or(""), l_parameters);
				} else if (l_action == "heuristicsQuery") {
					handle_heuristics_query(get_agent_name(p_message->get_sender()).value_or(""), l_parameters);
				} else if (l_action == "finished") {
					stop();
				}
			} catch (const std::exception& p_exception) {
				cam::dotnet::write_line(p_exception.what());
			}
		}
};

class SearchAgent final : public cam::Agent {
	public:
		struct StatePairCost {
			std::string m_from_state;
			std::string m_to_state;
			int m_cost;
		};

	private:
		std::string m_initial_state;
		std::string m_goal_state;
		std::string m_current_state;
		std::map<std::string, int> m_heuristics;
		std::vector<StatePairCost> m_costs;
		std::vector<std::string> m_heuristic_queries;
		cam::DotnetRandom m_rand;

		void handle_neighbors(const std::string& p_neighbors_and_costs) {
			const auto l_toks = split_remove_empty(p_neighbors_and_costs, " |");
			m_heuristic_queries.clear();

			for (std::size_t i = 0; i < l_toks.size() / 2; i++) {
				const std::string& l_neighbor_state = l_toks[i * 2];
				const std::string& l_cost_to_neighbor = l_toks[i * 2 + 1];

				m_costs.push_back({m_current_state, l_neighbor_state, cam::dotnet::to_int32(l_cost_to_neighbor)});

				if (!m_heuristics.contains(l_neighbor_state)) {
					m_heuristic_queries.push_back(l_neighbor_state);
				}
			}

			if (!m_heuristic_queries.empty()) {
				std::string l_hq = m_goal_state + " ";
				for (const auto& l_state: m_heuristic_queries) {
					l_hq += l_state + " ";
				}
				send_by_name("map", "heuristicsQuery " + l_hq);
			} else {
				move_to_neighbor_state();
			}
		}

		void handle_heuristics_reply(const std::string& p_heuristic_values) {
			const auto l_toks = split_remove_empty(p_heuristic_values, " ");
			for (std::size_t i = 0; i < l_toks.size(); i++) {
				// Dictionary.Add: an existing key is an error
				if (!m_heuristics.emplace(m_heuristic_queries.at(i), cam::dotnet::to_int32(l_toks[i])).second) {
					throw std::invalid_argument("An item with the same key has already been added. Key: " + m_heuristic_queries[i]);
				}
			}
			move_to_neighbor_state();
		}

		void move_to_neighbor_state() {
			int l_fn_min = std::numeric_limits<int>::max();
			std::vector<StatePairCost> l_states_f;

			for (const auto& l_spc: m_costs) {
				if (l_spc.m_from_state != m_current_state) {
					continue;
				}

				// only the neighbors are processed here

				const int l_fn = l_spc.m_cost + m_heuristics.at(l_spc.m_to_state);

				l_states_f.push_back({m_current_state, l_spc.m_to_state, l_fn});

				if (l_fn < l_fn_min) {
					l_fn_min = l_fn;
				}
			}

			std::vector<std::string> l_min_states;  // when there are more neighboring states with the same f

			for (const auto& l_spcf: l_states_f) {
				if (l_spcf.m_cost == l_fn_min) {
					l_min_states.push_back(l_spcf.m_to_state);
				}
			}

			const std::string l_next_state = l_min_states[static_cast<std::size_t>(m_rand.next(static_cast<std::int32_t>(l_min_states.size())))];

			cam::dotnet::write_line(get_name() + " moves to state " + l_next_state);

			if (l_next_state == m_goal_state) {
				cam::dotnet::write_line(get_name() + " reached goal state " + l_next_state);
				send_by_name("map", "finished");
				stop();
				return;
			}

			m_heuristics[m_current_state] = l_fn_min;

			cam::dotnet::write_line(get_name() + " updates h(" + m_current_state + ") = " + std::to_string(m_heuristics[m_current_state]));

			m_current_state = l_next_state;

			std::this_thread::sleep_for(std::chrono::milliseconds(get_environment()->get_global_data().at("Delay").get<int>()));

			process_state();
		}

		void process_state() {
			for (const auto& l_spc: m_costs) {
				if (l_spc.m_from_state == m_current_state) {
					// it has been in this state before and already knows the neighbors
					move_to_neighbor_state();
					return;
				}
			}

			send_by_name("map", "expand " + m_current_state);
		}

	public:
		explicit SearchAgent(const std::string& p_name) : Agent(p_name), m_rand(cam::dotnet::next_site_seed("SearchAgent.cs:52")) {}

		void setup() override {
			m_initial_state = "A";
			m_goal_state = "E";
			m_current_state = m_initial_state;
			send_by_name("map", "expand " + m_current_state);
		}

		void action(const cam::MessagePointer& p_message) override {
			try {
				cam::dotnet::write_line("\t" + format(p_message));
				const auto [l_action, l_parameters] = p_message->parse_rest();

				if (l_action == "neighbors") {
					handle_neighbors(l_parameters);
				} else if (l_action == "heuristicsReply") {
					handle_heuristics_reply(l_parameters);
				}
			} catch (const std::exception& p_exception) {
				cam::dotnet::write_line(p_exception.what());
			}
		}
};

int main() {
	cam::Environment l_environment({.m_turns = 0,
									.m_mode = cam::EnvironmentMasMode::Parallel,
									.m_seed = static_cast<unsigned int>(cam::dotnet::next_site_seed("EnvironmentMas.cs:61")),
									.m_random = cam::dotnet::random_engine()});
	l_environment.add<MapAgent>("map");
	l_environment.add<SearchAgent>("agent1");

	l_environment.set_global_data("MapName", "Pendulum");
	l_environment.set_global_data("Delay", 100);

	l_environment.start();
	return 0;
}
