
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

#include <random>
#include <utility>

#include <Environment.h>

constexpr int sign(int x) { return (x > 0) ? 1 : ((x < 0) ? -1 : 0); }

enum class State {
	Free, Carrying
};

enum class MessageAction {
	None, Position, Change, PickUp, Carry, Unload, Move, Rock, Block
};
std::ostream& operator<<(std::ostream& p_stream, const MessageAction& p_message_action) {
	switch (p_message_action) {
		case MessageAction::Position:
			return p_stream << "Position";
		case MessageAction::Change:
			return p_stream << "Change";
		case MessageAction::PickUp:
			return p_stream << "PickUp";
		case MessageAction::Carry:
			return p_stream << "Carry";
		case MessageAction::Unload:
			return p_stream << "Unload";
		case MessageAction::Move:
			return p_stream << "Move";
		case MessageAction::Rock:
			return p_stream << "Rock";
		case MessageAction::Block:
			return p_stream << "Block";
		default:
			return p_stream << "Not set";
	}
}

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
		m_x(-1),
		m_y(-1) {}

	explicit Position(const int p_x, const int p_y) :
		m_x(p_x),
		m_y(p_y){}

	explicit Position(std::stringstream& p_stream) :
		Position() {
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
	return p_stream << "[" << p_position.m_x << "; " << p_position.m_y << "]";
}

class Message {
public:
	MessageAction m_action;
	Position m_position;
	int m_resource_id;

	explicit Message() :
		m_action(MessageAction::None),
		m_position(),
		m_resource_id(0) {}

	explicit Message(const MessageAction& p_action, const Position& p_position = Position(), const int p_resource_id = -1) :
		m_action(p_action),
		m_position(p_position),
		m_resource_id(p_resource_id) {}

	explicit Message(std::stringstream& p_stream) :
		Message() {
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
		p_archive(m_action, m_position, m_resource_id);
	}

	bool operator==(const Message& p_other) const {
		return m_action == p_other.m_action && m_position == p_other.m_position && m_resource_id == p_other.m_resource_id;
	}

	Message& operator=(const Message& m_other) = default;

};
std::ostream& operator<<(std::ostream& p_stream, const Message& p_message) {
	p_stream << "Action = " << p_message.m_action;
	if (p_message.m_position.m_x != -1 && p_message.m_position.m_y != -1) {
		p_stream << "; " << "Position = " << p_message.m_position;
	}
	if (p_message.m_resource_id != -1) {
		p_stream << "; " << "ResourceID = " << p_message.m_resource_id;
	}
	return p_stream;
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
		const std::string& l_content = p_message->content();
		std::stringstream l_stream(l_content);
		const auto& l_message = Message(l_stream);
		std::cout << "[" << get_name() << "] " << l_message << std::endl;

		switch (l_message.m_action) {
			case MessageAction::Position:
				handle_position(p_message->get_sender(), l_message.m_position);
				break;

			case MessageAction::Change:
				handle_change(p_message->get_sender(), l_message.m_position);
				break;

			case MessageAction::PickUp:
				handle_pick_up(p_message->get_sender(), l_message.m_resource_id);
				break;

			case MessageAction::Carry:
				handle_carry(p_message->get_sender(), l_message.m_resource_id, l_message.m_position);
				break;

			case MessageAction::Unload:
				handle_unload(p_message->get_sender(), l_message.m_resource_id);
				break;

			default:
				std::cerr << "Impossible (should be)" << std::endl;
		}
	}

private:
	void handle_position(const std::string& p_sender, const Position& p_position) {
		m_explorer_positions.insert(std::make_pair(p_sender, p_position));
		send(p_sender, Message(MessageAction::Move).serialize_to_stream().str());
	}

	void handle_change(const std::string& p_sender, const Position& p_position) {
		m_explorer_positions[p_sender] = p_position;

		for (const auto& l_key_value: m_explorer_positions) {
			if (l_key_value.first == p_sender) {
				continue;
			}
			if (l_key_value.second == p_position) {
				send(p_sender, Message(MessageAction::Block).serialize_to_stream().str());
				return;
			}
		}

		for (const auto& l_key_value: m_resource_positions) {
			if (l_key_value.second == p_position) {
				send(p_sender, Message(MessageAction::Rock, l_key_value.second, l_key_value.first).serialize_to_stream().str());
				return;
			}
		}

		send(p_sender, Message(MessageAction::Move).serialize_to_stream().str());
	}

	void handle_pick_up(const std::string& p_sender, const int p_resource_id) {
		m_resource_positions[p_resource_id] = m_explorer_positions[p_sender];
		send(p_sender, Message(MessageAction::Move).serialize_to_stream().str());
	}

	void handle_carry(const std::string& p_sender, const int p_resource_id, const Position& p_position) {
		m_explorer_positions[p_sender] = p_position;
		m_resource_positions[p_resource_id] = m_explorer_positions[p_sender];
		send(p_sender, Message(MessageAction::Move).serialize_to_stream().str());
	}

	void handle_unload(const std::string& p_sender, const int p_resource_id) {
		m_resource_positions.erase(p_resource_id);
		send(p_sender, Message(MessageAction::Move).serialize_to_stream().str());
	}
};

class ExplorerAgent : public cam::Agent {
private:
	Position m_position;
	State m_state;
	int m_resource_carried;
	int m_size;

public:
	explicit ExplorerAgent(const std::string& p_name) :
			Agent(p_name),
			m_position(),
			m_state(State::Free),
			m_resource_carried(-1),
			m_size() {}

	void setup() override {
		std::cout << "[" << get_name() << "] starting" << std::endl;

		m_size = get_environment()->get_global_data()["Size"];

		m_position = Position(m_size / 2, m_size / 2);
		m_state = State::Free;

		while (is_at_base()) {
			m_position = Position(Random::next(m_size), Random::next(m_size));
		}

		send_by_name("planet", Message(MessageAction::Position, m_position).serialize_to_stream().str());
	}

	bool is_at_base() const {
		return (m_position.m_x == m_size / 2 && m_position.m_y == m_size / 2); // the position of the base
	}

	void action(const cam::MessagePointer& p_message) override {
		const std::string& l_content = p_message->content();
		std::stringstream l_stream(l_content);
		const auto& l_message = Message(l_stream);
		std::cout << "[" << get_name() << "] " << l_message << std::endl;

		if (l_message.m_action == MessageAction::Move && m_state == State::Carrying && is_at_base()) {
			// R2. If carrying samples and at the base, then unload samples
			m_state = State::Free;
			send_by_name("planet", Message(MessageAction::Unload, m_position, m_resource_carried).serialize_to_stream().str());

		} else if (l_message.m_action == MessageAction::Move && m_state == State::Carrying && !is_at_base()) {
			// R3. If carrying samples and not at the base, then travel up gradient
			move_to_base();
			send_by_name("planet", Message(MessageAction::Carry, m_position, m_resource_carried).serialize_to_stream().str());

		} else if (l_message.m_action == MessageAction::Rock) {
			// R4. If you detect a sample, then pick sample up
			m_state = State::Carrying;
			m_resource_carried = l_message.m_resource_id;
			send_by_name("planet", Message(MessageAction::PickUp, m_position, m_resource_carried).serialize_to_stream().str());

		} else if (l_message.m_action == MessageAction::Block || l_message.m_action == MessageAction::Move) {
			// R1. If you detect an obstacle, then change direction
			// R5. If (true), then move randomly
			move_randomly();
			send_by_name("planet", Message(MessageAction::Change, m_position).serialize_to_stream().str());

		}
	}

	void move_randomly() {
		switch (Random::next(4)) {
			case 0:
				if (m_position.m_x > 0) {
					m_position.m_x--;
				}
				break;
			case 1:
				if (m_position.m_x < m_size - 1) {
					m_position.m_x++;
				}
				break;
			case 2:
				if (m_position.m_y > 0) {
					m_position.m_y--;
				}
				break;
			case 3:
				if (m_position.m_y < m_size - 1) {
					m_position.m_y++;
				}
				break;
			default:
				std::cerr << "Impossible (should be)" << std::endl;
		}
	}

	void move_to_base() {
		int l_dx = m_position.m_x - m_size / 2;
		int l_dy = m_position.m_y - m_size / 2;

		if (abs(l_dx) > abs(l_dy)) {
			m_position.m_x -= sign(l_dx);
		} else {
			m_position.m_y -= sign(l_dy);
		}
	}
};

int main() {
	cam::Environment l_environment(100);
	l_environment.add<PlanetAgent>("planet");

	// 5 explorers
	for (int i = 1; i <= 5; i++) {
		l_environment.add<ExplorerAgent>("explorer" + std::to_string(i));
	}

	l_environment.set_global_data("Size", 11);
	l_environment.set_global_data("NoResources", 10);

	l_environment.start();
}
