
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

#include <EnvironmentMas.h>

constexpr int sign(int x) { return (x > 0) ? 1 : ((x < 0) ? -1 : 0); }

enum class State {
	Free, Carrying
};

class Random {
public:
	static int next(const int p_max, const time_t p_seed = std::time(nullptr)) {
		static std::default_random_engine s_seeder(p_seed);
		static std::mt19937 s_generator(s_seeder());
		std::uniform_int_distribution l_dist(0, p_max - 1);
		return l_dist(s_generator);
	}
};

class Position {
public:
	int m_x;
	int m_y;

	explicit Position() :
			m_x(0),
			m_y(0) {}

	explicit Position(const int p_x, const int p_y) :
			m_x(p_x),
			m_y(p_y) {}

	explicit Position(std::stringstream& p_stream) :
			m_x(0),
			m_y(0) {
		cereal::PortableBinaryInputArchive l_input_archive(p_stream);
		l_input_archive(*this);
	}

	[[nodiscard]] std::stringstream serialize_to_stream() const {
		std::stringstream l_stream;
		cereal::PortableBinaryOutputArchive l_output_archive(l_stream);
		l_output_archive(*this);
		return l_stream;
	}

	template<class Archive>
	void serialize(Archive& p_archive) {
		p_archive(m_x, m_y);
	}

	bool operator==(const Position& m_other) const {
		return m_x == m_other.m_x && m_y == m_other.m_y;
	}

	Position& operator=(const Position& m_other) = default;

};

std::ostream& operator<<(std::ostream& p_stream, const Position& p_position) {
	return p_stream << "x = " << p_position.m_x << "; " << "y = " << p_position.m_y;
}

class PlanetAgent : public cam::Agent {
private:
	std::map<std::string, Position> m_explorer_positions;
	std::map<int, Position> m_resource_positions;

public:
	explicit PlanetAgent(const std::string& p_name) :
			Agent(p_name),
			m_explorer_positions(),
			m_resource_positions() {}

	void setup() override {
		std::cout << "[" << get_name() << "] starting" << std::endl;

		int l_size = get_environment()->get_global_data()["Size"];
		int l_no_resources = get_environment()->get_global_data()["NoResources"];

		std::vector<Position> l_res_pos;
		Position l_pos(l_size / 2, l_size / 2);
		l_res_pos.push_back(l_pos); // the position of the base

		for (int i = 1; i <= l_no_resources; i++) {
			// resources do not overlap
			while (std::find(l_res_pos.begin(), l_res_pos.end(), l_pos) != l_res_pos.end()) {
				int l_x = Random::next(l_size);
				int l_y = Random::next(l_size);
				l_pos = Position(l_x, l_y);
			}

			m_resource_positions.insert(std::make_pair(i, l_pos));
			l_res_pos.push_back(l_pos);
		}
	}

	void action(const cam::MessagePointer& p_message) override {
		const auto& l_content = p_message->content();
		const std::string& l_action = l_content["action"];
		const auto& l_parameters = l_content["parameters"];

		Position l_position;
		int l_resource_id;

		std::cout << "[" << get_name() << "] " << l_action << " ";
		if (l_parameters.contains("position")) {
			std::string l_position_str = l_parameters["position"].get<std::string>();
			std::stringstream l_stream(l_position_str);
			l_position = Position(l_stream);
			std::cout << l_position;
		}
		if (l_parameters.contains("ressource_id")) {
			l_resource_id = l_parameters["ressource_id"];
			std::cout << " Resource ID " << l_resource_id;
		}

		if (l_action == "position" || l_action == "change") {

			if (l_action == "position") {
				handle_position(p_message->get_sender(), l_position);
			} else if (l_action == "change") {
				handle_change(p_message->get_sender(), l_position);
			}
		} else {

			if (l_action == "pick_up") {
				handle_pick_up(p_message->get_sender(), l_resource_id);
			} else if (l_action == "carry") {
				handle_carry(p_message->get_sender(), l_resource_id, l_position);
			} else if (l_action == "unload") {
				handle_unload(p_message->get_sender(), l_resource_id);
			}
		}
		std::cout << std::endl;
	}

private:
	void handle_position(const std::string& p_sender, const Position& p_position) {
		m_explorer_positions.insert(std::make_pair(p_sender, p_position));
		send(p_sender, {{"action", "move"}});
	}

	void handle_change(const std::string& p_sender, const Position& p_position) {
		m_explorer_positions[p_sender] = p_position;

		for (const auto& l_key_value: m_explorer_positions) {
			if (l_key_value.first == p_sender) {
				continue;
			}
			if (l_key_value.second == p_position) {
				send(p_sender, {{"action", "block"}});
				return;
			}
		}

		for (const auto& l_key_value: m_resource_positions) {
			if (l_key_value.second == p_position) {
				send(p_sender, {{"action",     "rock"},
								{"parameters", l_key_value.first}});
				return;
			}
		}

		send(p_sender, {{"action", "move"}});
	}

	void handle_pick_up(const std::string& p_sender, const int p_ressource_id) {
		m_resource_positions[p_ressource_id] = m_explorer_positions[p_sender];
		send(p_sender, {{"action", "move"}});
	}

	void handle_carry(const std::string& p_sender, const int p_ressource_id, const Position& p_position) {
		m_explorer_positions[p_sender] = p_position;
		m_resource_positions[p_ressource_id] = m_explorer_positions[p_sender];
		send(p_sender, {{"action", "move"}});
	}

	void handle_unload(const std::string& p_sender, const int p_ressource_id) {
		m_resource_positions.erase(p_ressource_id);
		send(p_sender, {{"action", "move"}});
	}
};

class ExplorerAgent : public cam::Agent {
private:
	int m_x;
	int m_y;
	State m_state;
	int m_resource_carried;
	int m_size;

public:
	explicit ExplorerAgent(const std::string& p_name) :
			Agent(p_name),
			m_x(0),
			m_y(0),
			m_state(State::Free),
			m_resource_carried(-1),
			m_size() {}

	void setup() override {
		std::cout << "[" << get_name() << "] starting" << std::endl;

		m_size = get_environment()->get_global_data()["Size"];

		m_x = m_size / 2;
		m_y = m_size / 2;
		m_state = State::Free;

		while (is_at_base()) {
			m_x = Random::next(m_size);
			m_y = Random::next(m_size);
		}

		Position l_position(m_x, m_y);
		send_by_name("planet", {
				{"action",     "position"},
				{"parameters", {
									   {"position", l_position.serialize_to_stream().str()}
							   }}
		});
	}

	bool is_at_base() const {
		return (m_x == m_size / 2 && m_y == m_size / 2); // the position of the base
	}

	void action(const cam::MessagePointer& p_message) override {
		const auto& l_content = p_message->content();
		const std::string& l_action = l_content["action"];

		std::cout << "[" << get_name() << "] " << l_action;
		Position l_position(m_x, m_y);

		json l_message = json::object();
		if (l_action == "move" && m_state == State::Carrying && is_at_base()) {
			// R2. If carrying samples and at the base, then unload samples
			m_state = State::Free;
			l_message = {
					{"action",     "unload"},
					{"parameters", {
										   {"position", l_position.serialize_to_stream().str()},
										   {"ressource_id", m_resource_carried}
								   }
					}};

		} else if (l_action == "move" && m_state == State::Carrying && !is_at_base()) {
			// R3. If carrying samples and not at the base, then travel up gradient
			move_to_base();
			l_message = {
					{"action",     "carry"},
					{"parameters", {
										   {"position", l_position.serialize_to_stream().str()},
										   {"ressource_id", m_resource_carried}
								   }
					}};

		} else if (l_action == "rock") {
			// R4. If you detect a sample, then pick sample up
			m_state = State::Carrying;
			m_resource_carried = l_content["parameters"];
			std::cout << " Ressource ID " << m_resource_carried << l_action;
			l_message = {
					{"action",     "pick_up"},
					{"parameters", {
										   {"position", l_position.serialize_to_stream().str()},
										   {"ressource_id", m_resource_carried}
								   }
					}};

		} else if (l_action == "block" || l_action == "move") {
			// R1. If you detect an obstacle, then change direction
			// R5. If (true), then move randomly
			move_randomly();
			l_message = {
					{"action",     "change"},
					{"parameters", {
										   {"position", l_position.serialize_to_stream().str()}}
					}
			};

		}
		std::cout << std::endl;

		send_by_name("planet", l_message);
	}

	void move_randomly() {
		switch (Random::next(4)) {
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
				std::cerr << "Impossible (should be)" << std::endl;
		}
	}

	void move_to_base() {
		int l_dx = m_x - m_size / 2;
		int l_dy = m_y - m_size / 2;

		if (abs(l_dx) > abs(l_dy)) {
			m_x -= sign(l_dx);
		} else {
			m_y -= sign(l_dy);
		}
	}
};

int main() {
	cam::EnvironmentMas l_environment(100);
	l_environment.add<PlanetAgent>("planet");

	// 5 explorers
	for (int i = 1; i <= 5; i++) {
		l_environment.add<ExplorerAgent>("explorer" + std::to_string(i));
	}

	l_environment.set_global_data("Size", 11);
	l_environment.set_global_data("NoResources", 10);

	l_environment.start();
}
