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

#include "EnvironmentMas.h"

#include <thread>

#include "Agent.h"

cam::EnvironmentMas::EnvironmentMas(const int p_no_turns, const EnvironmentMasMode& p_mode, const int p_delay_after_turn, const unsigned int p_seed) :
	m_no_turns(p_no_turns),
	m_delay_after_turn(p_delay_after_turn),
	m_agent_collection(p_mode),
	m_random_generator(p_seed) {
}

const std::string& cam::EnvironmentMas::add(AgentPointer&& p_agent) {
	p_agent->set_environment(this);
	m_agent_collection.add(p_agent);
	return p_agent->get_id();
}

cam::AgentPointer cam::EnvironmentMas::get(const std::string& p_id) const {
	return m_agent_collection.get(p_id);
}

void cam::EnvironmentMas::continue_simulation(const int p_no_turns) {
	int l_turn = 0;
	while (true) {
		run_turn(l_turn++);

		if (p_no_turns != 0 && l_turn >= p_no_turns) {
			break;
		}
		if (m_agent_collection.count() == 0) {
			break;
		}
	}
	simulation_finished();
}

const cam::AgentPointer& cam::EnvironmentMas::random_agent() const {
	return m_agent_collection.random_agent();
}

void cam::EnvironmentMas::remove(const AgentPointer& p_agent) {
	m_agent_collection.remove(p_agent->get_id());
}

void cam::EnvironmentMas::remove(const std::string& p_agent_id) {
	remove(m_agent_collection.get(p_agent_id));
}

void cam::EnvironmentMas::send(const std::string& p_sender_id, const std::string& p_receiver_id, const uint8_t* p_message, const size_t& p_length, const MessageBinaryFormat& p_binary_format) const {
	if (const AgentPointer& l_agent = m_agent_collection.get(p_receiver_id); l_agent) {
		l_agent->post(std::make_shared<Message>(p_sender_id, p_receiver_id, p_message, p_length, p_binary_format));
	}
}

void cam::EnvironmentMas::send_by_name(const std::string& p_sender_id, const std::string& p_receiver_name, const uint8_t* p_message, const size_t& p_length, const bool p_is_fragment, const bool p_first_only, const MessageBinaryFormat& p_binary_format) const {
	for (auto l_filtered_elements = m_agent_collection.m_agents_by_name | std::views::filter([p_is_fragment, p_receiver_name](auto& p_value) {
		if (p_is_fragment) {
			return p_value.first.find(p_receiver_name) != std::string::npos;
		}
		return p_value.first == p_receiver_name;
	}); const auto& [l_name, l_id] : l_filtered_elements) {
		const auto& l_agent = m_agent_collection.get(l_id);
		if (l_agent->is_dead()) {
			continue;
		}

		l_agent->post(std::make_shared<Message>(p_sender_id, l_id, p_message, p_length, p_binary_format));
		if (p_first_only) {
			break;
		}
	}
}

void cam::EnvironmentMas::broadcast(const std::string& p_sender_id, const uint8_t* p_message, const size_t& p_length, const MessageBinaryFormat& p_binary_format) const {
	for (auto& [l_id, l_agent] : m_agent_collection.m_agents) {
		if (l_id != p_sender_id && !l_agent->is_dead()) {
			l_agent->post(std::make_shared<Message>(p_sender_id, l_id, p_message, p_length, p_binary_format));
		}
	}
}

void cam::EnvironmentMas::start() {
	int l_turn = 0;

	m_agent_collection.process_buffers();
	while (true) {
		run_turn(l_turn++);
		if (m_no_turns != 0 && l_turn >= m_no_turns) {
			break;
		}
		m_agent_collection.process_buffers();
		if (m_agent_collection.count() == 0) {
			break;
		}
	}

	simulation_finished();
}

size_t cam::EnvironmentMas::agents_count() const {
	return m_agent_collection.count();
}

std::vector<std::string> cam::EnvironmentMas::get_agents_by_name(const std::string& p_name, const bool p_first_only) const {
	std::vector<std::string> l_returned_agents;
	for (const auto& [l_id, l_agent] : m_agent_collection.m_agents) {
		if (l_agent->get_name() == p_name) {
			l_returned_agents.push_back(l_id);
			if (p_first_only) {
				break;
			}
		}
	}
	return l_returned_agents;
}

std::optional<std::string> cam::EnvironmentMas::get_first_agent_by_name(const std::string& p_name) const {
	const auto& l_agents = get_agents_by_name(p_name, true);
	if (l_agents.empty()) {
		return {};
	}
	return l_agents[0];
}

std::optional<std::string> cam::EnvironmentMas::get_agent_name(const std::string& p_id) const {
	const auto& l_agent = m_agent_collection.m_agents.find(p_id);
	if (l_agent ==  m_agent_collection.m_agents.end()) {
		return {};
	}
	return l_agent->second->get_name();
}

std::vector<std::string> cam::EnvironmentMas::get_filtered_agents(const std::string& p_fragment_name, const bool p_first_only) const {
	std::vector<std::string> l_returned_agents;
	for (const auto& [l_id, l_agent] : m_agent_collection.m_agents) {
		if (l_agent->get_name().find(p_fragment_name) != std::string::npos) {
			l_returned_agents.push_back(l_agent->get_id());
			if (p_first_only) {
				break;
			}
		}
	}
	return l_returned_agents;
}

void cam::EnvironmentMas::simulation_finished() {}

void cam::EnvironmentMas::turn_finished(int) {}

std::vector<const cam::ObservablesPointer> cam::EnvironmentMas::get_list_of_observable_agents(const Agent *p_perceiving_agent) const {
	std::vector<const ObservablesPointer> l_observable_agent_list;

	// Map id:agent
	for (auto& [l_id, l_agent] : m_agent_collection.m_agents) {
		if (l_id == p_perceiving_agent->get_id() || !l_agent->is_using_observables()) {
			continue;
		}

		if (p_perceiving_agent->perception_filter(l_agent->get_observables())) {
			l_observable_agent_list.push_back(l_agent->get_observables());
		}
	}
	return l_observable_agent_list;
}

void cam::EnvironmentMas::run_turn(const int p_turn) {
	m_agent_collection.run_turn();
	if (m_delay_after_turn) {
		std::this_thread::sleep_for(std::chrono::milliseconds(m_delay_after_turn));
	}
	turn_finished(p_turn);
}

