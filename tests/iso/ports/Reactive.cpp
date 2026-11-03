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
// oracle by iso.Reactive.
// Port of the C# example E01 Reactive Architecture (ActressMAS v3.1): five explorers walk at random on an 11 x 11
// planet, pick up the samples they find and carry them to the base, by five reactive rules; the planet agent keeps the
// positions and answers each move. Parallel, 100 ms after each turn, no turn limit in the C#. The form of the planet is
// replaced, as in the oracle, by nothing. The explorers draw from one generator and the planet from another, as from
// the static Random of the C#, with the seeds of its oracle, but under a lock: the C# draws without one, which C++
// cannot do without undefined behaviour.
// Arguments: [turns, 0 for no end] [delay after each turn in ms], 0 and 100 by default as the C#; then "sequential"
// for its sequential variant (fixed order).

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <DotNet.hpp>
#include <Environment.h>
#include <util/DotnetRandom.h>

// Dictionary<string, string> of the C#: insertion order, Add refuses an existing key
class Dictionary {
	private:
		std::vector<std::pair<std::string, std::string>> m_entries;

	public:
		[[nodiscard]] const std::vector<std::pair<std::string, std::string>>& entries() const { return m_entries; }

		void add(const std::string& p_key, const std::string& p_value) {
			for (const auto& [l_key, l_value]: m_entries) {
				if (l_key == p_key) {
					throw std::invalid_argument("An item with the same key has already been added. Key: " + p_key);
				}
			}
			m_entries.emplace_back(p_key, p_value);
		}

		std::string& operator[](const std::string& p_key) {
			for (auto& [l_key, l_value]: m_entries) {
				if (l_key == p_key) {
					return l_value;
				}
			}
			return m_entries.emplace_back(p_key, "").second;
		}

		[[nodiscard]] const std::string& at(const std::string& p_key) const {
			for (const auto& [l_key, l_value]: m_entries) {
				if (l_key == p_key) {
					return l_value;
				}
			}
			throw std::out_of_range("The given key '" + p_key + "' was not present in the dictionary.");
		}

		void remove(const std::string& p_key) {
			std::erase_if(m_entries, [&p_key](const auto& p_entry) { return p_entry.first == p_key; });
		}
};

class PlanetAgent final : public cam::Agent {
	private:
		Dictionary m_explorer_positions;
		Dictionary m_resource_positions;
		Dictionary m_loads;

		// Shared by the planet agents, as the static Random of the C#
		static std::int32_t next(const std::int32_t p_max) {
			static std::mutex s_mutex;
			static cam::DotnetRandom s_rand(cam::dotnet::next_site_seed("PlanetAgent.cs:29"));
			const std::scoped_lock l_lock(s_mutex);
			return s_rand.next(p_max);
		}

		void handle_position(const std::string& p_sender, const std::string& p_position) {
			m_explorer_positions.add(p_sender, p_position);
			send_by_name(p_sender, "move");
		}

		void handle_change(const std::string& p_sender, const std::string& p_position) {
			m_explorer_positions[p_sender] = p_position;

			for (const auto& [l_key, l_value]: m_explorer_positions.entries()) {
				if (l_key == p_sender) {
					continue;
				}
				if (l_value == p_position) {
					send_by_name(p_sender, "block");
					return;
				}
			}

			for (const auto& [l_key, l_value]: m_resource_positions.entries()) {
				if (l_value == p_position) {
					send_by_name(p_sender, "rock " + l_key);
					return;
				}
			}

			send_by_name(p_sender, "move");
		}

		void handle_pick_up(const std::string& p_sender, const std::string& p_position) {
			m_loads[p_sender] = p_position;
			send_by_name(p_sender, "move");
		}

		void handle_carry(const std::string& p_sender, const std::string& p_position) {
			m_explorer_positions[p_sender] = p_position;
			const std::string l_res = m_loads.at(p_sender);
			m_resource_positions[l_res] = p_position;
			send_by_name(p_sender, "move");
		}

		void handle_unload(const std::string& p_sender) {
			m_loads.remove(p_sender);
			send_by_name(p_sender, "move");
		}

	public:
		explicit PlanetAgent(const std::string& p_name) : Agent(p_name) {}

		void setup() override {
			cam::dotnet::write_line("Starting " + get_name());

			const json l_memory = get_environment()->get_global_data();
			const int l_size = l_memory.at("Size").get<int>();
			const int l_no_resources = l_memory.at("NoResources").get<int>();

			std::vector<std::string> l_res_pos;
			std::string l_comp_pos = std::to_string(l_size / 2) + " " + std::to_string(l_size / 2);
			l_res_pos.push_back(l_comp_pos);  // the position of the base

			for (int i = 1; i <= l_no_resources; i++) {
				while (std::find(l_res_pos.begin(), l_res_pos.end(), l_comp_pos) != l_res_pos.end()) {  // resources do not overlap
					const int l_x = next(l_size);
					const int l_y = next(l_size);
					l_comp_pos = std::to_string(l_x) + " " + std::to_string(l_y);
				}

				m_resource_positions.add("res" + std::to_string(i), l_comp_pos);
				l_res_pos.push_back(l_comp_pos);
			}
		}

		void action(const cam::MessagePointer& p_message) override {
			try {
				cam::dotnet::write_line("\t" + format(p_message));
				const auto [l_action, l_parameters] = p_message->parse_rest();
				const std::string l_sender = get_agent_name(p_message->get_sender()).value_or("");

				if (l_action == "position") {
					handle_position(l_sender, l_parameters);
				} else if (l_action == "change") {
					handle_change(l_sender, l_parameters);
				} else if (l_action == "pick-up") {
					handle_pick_up(l_sender, l_parameters);
				} else if (l_action == "carry") {
					handle_carry(l_sender, l_parameters);
				} else if (l_action == "unload") {
					handle_unload(l_sender);
				}

				// the form would be redrawn here
			} catch (const std::exception& p_exception) {
				cam::dotnet::write_line(p_exception.what());
			}
		}
};

class ExplorerAgent final : public cam::Agent {
	private:
		enum class State { Free, Carrying };

		int m_x = 0;
		int m_y = 0;
		State m_state = State::Free;
		std::string m_resource_carried;
		int m_size = 0;

		// Shared by the explorers, as the static Random of the C#
		static std::int32_t next(const std::int32_t p_max) {
			static std::mutex s_mutex;
			static cam::DotnetRandom s_rand(cam::dotnet::next_site_seed("ExplorerAgent.cs:28"));
			const std::scoped_lock l_lock(s_mutex);
			return s_rand.next(p_max);
		}

		[[nodiscard]] bool is_at_base() const {
			return (m_x == m_size / 2 && m_y == m_size / 2);  // the position of the base
		}

		void move_randomly() {
			const int l_d = next(4);
			switch (l_d) {
				case 0:
					if (m_x > 0) {
						m_x--;
					}
					break;
				case 1:
					if (m_x < m_size - 1) {
						m_x++;
					}
					break;
				case 2:
					if (m_y > 0) {
						m_y--;
					}
					break;
				case 3:
					if (m_y < m_size - 1) {
						m_y++;
					}
					break;
				default:
					break;
			}
		}

		static int sign(const int p_value) {
			return (p_value > 0) - (p_value < 0);
		}

		void move_to_base() {
			const int l_dx = m_x - m_size / 2;
			const int l_dy = m_y - m_size / 2;

			if (std::abs(l_dx) > std::abs(l_dy)) {
				m_x -= sign(l_dx);
			} else {
				m_y -= sign(l_dy);
			}
		}

		[[nodiscard]] std::string position() const {
			return std::to_string(m_x) + " " + std::to_string(m_y);
		}

	public:
		explicit ExplorerAgent(const std::string& p_name) : Agent(p_name) {}

		void setup() override {
			cam::dotnet::write_line("Starting " + get_name());

			m_size = get_environment()->get_global_data().at("Size").get<int>();

			m_x = m_size / 2;
			m_y = m_size / 2;
			m_state = State::Free;

			while (is_at_base()) {
				m_x = next(m_size);
				m_y = next(m_size);
			}

			send_by_name("planet", "position " + position());
		}

		void action(const cam::MessagePointer& p_message) override {
			try {
				cam::dotnet::write_line("\t" + format(p_message));
				const auto [l_action, l_parameters] = p_message->parse();

				if (l_action == "block") {
					// R1. If you detect an obstacle, then change direction
					move_randomly();
					send_by_name("planet", "change " + position());
				} else if (l_action == "move" && m_state == State::Carrying && is_at_base()) {
					// R2. If carrying samples and at the base, then unload samples
					m_state = State::Free;
					send_by_name("planet", "unload " + m_resource_carried);
				} else if (l_action == "move" && m_state == State::Carrying && !is_at_base()) {
					// R3. If carrying samples and not at the base, then travel up gradient
					move_to_base();
					send_by_name("planet", "carry " + position());
				} else if (l_action == "rock") {
					// R4. If you detect a sample, then pick sample up
					m_state = State::Carrying;
					m_resource_carried = l_parameters.at(0);
					send_by_name("planet", "pick-up " + m_resource_carried);
				} else if (l_action == "move") {
					// R5. If (true), then move randomly
					move_randomly();
					send_by_name("planet", "change " + position());
				}
			} catch (const std::exception& p_exception) {
				cam::dotnet::write_line(p_exception.what());
			}
		}
};

int main(int argc, char** argv) {
	const int l_turns = argc > 1 ? std::atoi(argv[1]) : 0;
	const int l_delay = argc > 2 ? std::atoi(argv[2]) : 100;

	// no turn limit, run in parallel
	cam::Environment l_environment({.m_turns = l_turns,
									.m_mode = cam::dotnet::scenario_mode(argc, argv, cam::EnvironmentMasMode::Parallel),
									.m_delay_after_turn = l_delay,
									.m_seed = static_cast<unsigned int>(cam::dotnet::next_site_seed("EnvironmentMas.cs:61")),
									.m_random = cam::dotnet::random_engine()});

	l_environment.add<PlanetAgent>("planet");

	for (int i = 1; i <= 5; i++) {  // 5 explorers
		l_environment.add<ExplorerAgent>("explorer" + std::to_string(i));
	}

	l_environment.set_global_data("Size", 11);
	l_environment.set_global_data("NoResources", 10);

	l_environment.start();
	return 0;
}
