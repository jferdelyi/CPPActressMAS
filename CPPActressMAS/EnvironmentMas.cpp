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
#include "PahoWrapper.hpp"

cam::EnvironmentMas::EnvironmentMas(const int p_no_turns, const EnvironmentMasMode& p_mode,
									const int p_delay_after_turn, const unsigned int p_seed) :
		m_no_turns(p_no_turns),
		m_delay_after_turn(p_delay_after_turn),
		m_agent_collection(p_mode, p_seed),
		m_remote_client(nullptr) {
}

cam::EnvironmentMas::~EnvironmentMas() {
	if (m_remote_client) {
		m_remote_client->disconnect_to_broker();
		while (m_remote_client->is_connected());
		delete m_remote_client;
		m_remote_client = nullptr;
	}
}

const std::string& cam::EnvironmentMas::add(AgentPointer&& p_agent) {
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

void
cam::EnvironmentMas::send(const std::string& p_sender_id, const std::string& p_receiver_id, const uint8_t* p_message,
						  const size_t& p_length, const MessageBinaryFormat& p_binary_format,
						  const bool p_from_remote) const {
	if (const AgentPointer& l_agent = m_agent_collection.get(p_receiver_id); l_agent) {
		if (l_agent->is_dead()) {
			return;
		}
		l_agent->post(std::make_shared<Message>(p_sender_id, p_receiver_id, p_message, p_length, p_binary_format));
	} else if (m_remote_client && !p_from_remote) {
		const json& l_data = {
				{"sender_id",     p_sender_id},
				{"receiver_id",   p_receiver_id},
				{"message",       std::vector(p_message, p_message + p_length)},
				{"length",        p_length},
				{"binary_format", p_binary_format}
		};
		const auto& l_message = m_remote_client->format_message(l_data, "object", "message");
		m_remote_client->send_message_by_id(cam::Message::to_binary(l_message));
	}
}

void cam::EnvironmentMas::send_by_name(const std::string& p_sender_id, const std::string& p_receiver_name,
									   const uint8_t* p_message, const size_t& p_length, const bool p_is_fragment,
									   const bool p_first_only, const MessageBinaryFormat& p_binary_format,
									   const bool p_from_remote) const {
	for (auto l_filtered_elements =
			m_agent_collection.m_agents_by_name | std::views::filter([p_is_fragment, p_receiver_name](auto& p_value) {
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
	if (m_remote_client && !p_from_remote) {
		const json& l_data = {
				{"sender_id",     p_sender_id},
				{"receiver_name", p_receiver_name},
				{"message",       std::vector(p_message, p_message + p_length)},
				{"is_fragment",   p_is_fragment},
				{"first_only",    p_first_only},
				{"binary_format", p_binary_format}
		};
		const auto& l_message = m_remote_client->format_message(l_data, "object", "message");
		m_remote_client->broadcast_message_by_name(cam::Message::to_binary(l_message));
	}
}

void cam::EnvironmentMas::broadcast(const std::string& p_sender_id, const uint8_t* p_message, const size_t& p_length,
									const MessageBinaryFormat& p_binary_format, const bool p_from_remote) const {
	for (auto& [l_id, l_agent]: m_agent_collection.m_agents) {
		if (l_id != p_sender_id && !l_agent->is_dead()) {
			l_agent->post(std::make_shared<Message>(p_sender_id, l_id, p_message, p_length, p_binary_format));
		}
	}
	if (m_remote_client && !p_from_remote) {
		const json& l_data = {
				{"sender_id",     p_sender_id},
				{"message",       std::vector(p_message, p_message + p_length)},
				{"binary_format", p_binary_format}
		};
		const auto& l_message = m_remote_client->format_message(l_data, "object", "message");
		m_remote_client->broadcast_message(cam::Message::to_binary(l_message));
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

std::vector<std::string>
cam::EnvironmentMas::get_agents_by_name(const std::string& p_name, const bool p_first_only) const {
	std::vector<std::string> l_returned_agents;
	for (const auto& [l_id, l_agent]: m_agent_collection.m_agents) {
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
	if (l_agent == m_agent_collection.m_agents.end()) {
		return {};
	}
	return l_agent->second->get_name();
}

std::vector<std::string>
cam::EnvironmentMas::get_filtered_agents(const std::string& p_fragment_name, const bool p_first_only) const {
	std::vector<std::string> l_returned_agents;
	for (const auto& [l_id, l_agent]: m_agent_collection.m_agents) {
		if (l_agent->get_name().find(p_fragment_name) != std::string::npos) {
			l_returned_agents.push_back(l_agent->get_id());
			if (p_first_only) {
				break;
			}
		}
	}
	return l_returned_agents;
}

[[nodiscard]] std::string cam::EnvironmentMas::get_id() const {
	return m_remote_client ? m_remote_client->get_id() : "";
}

void cam::EnvironmentMas::move(const std::stringstream& p_agent_stream, const json& p_message,
							   const std::string& p_environment_id) {
	if (!m_remote_client) {
		return;
	}

	const json& l_data = {
			{"agent",   p_agent_stream.str()},
			{"message", p_message}
	};
	const auto& l_message_to_send = m_remote_client->format_message(l_data, "byte", "agent");
	m_remote_client->post_agent(p_environment_id, cam::Message::to_binary(l_message_to_send));
}

std::vector<const cam::ObservablesPointer>
cam::EnvironmentMas::get_list_of_observable_agents(const Agent* p_perceiving_agent) const {
	std::vector<const ObservablesPointer> l_observable_agent_list;

	// Map id:agent
	for (auto& [l_id, l_agent]: m_agent_collection.m_agents) {
		if (l_agent->is_dead() || l_id == p_perceiving_agent->get_id()) {
			continue;
		}
		const auto& l_observable = l_agent->get_observables();
		if (l_observable->empty()) {
			continue;
		}

		if (p_perceiving_agent->perception_filter(l_observable)) {
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

//###############################################################
//	Remote handlers
//###############################################################

void cam::EnvironmentMas::send_discovery_callback() {
	assert(m_remote_client);
	for (const auto& l_container: m_new_containers) {
		const json& l_data_to_send = get_id();
		const auto& l_message_to_send = m_remote_client->format_message(l_data_to_send, "string", "environment_id");
		m_remote_client->send_callback_discovery(l_container, cam::Message::to_binary(l_message_to_send));
	}
	m_new_containers.clear();
}

int cam::EnvironmentMas::initialise_remote_connection_by_address(const std::string& p_id,
																 const std::string& p_full_address) {
	m_remote_client = new PahoWrapper(*this, p_id, p_full_address);
	return m_remote_client->initialise_remote_connection_by_address();
}

int cam::EnvironmentMas::initialise_remote_connection(const std::string& p_id, const std::string& p_host, int p_port) {
	return initialise_remote_connection_by_address(p_id, "tcp://" + p_host + ":" + std::to_string(p_port));
}

int cam::EnvironmentMas::username_pw_set(const std::string& p_username, const std::string& p_password) {
	if (!m_remote_client) {
		return PAHO_ERR_NOT_INIT;
	}
	m_remote_client->username_pw_set(p_username, p_password);
	return mqtt::ReasonCode::SUCCESS;
}

void cam::EnvironmentMas::on_new_agent(const json& p_json_message) {
	const std::string l_message = p_json_message["agent"];
	std::stringstream l_stream = std::stringstream(l_message);
	add<Agent>(l_stream);
}

void cam::EnvironmentMas::on_new_environment(const std::string& p_environment_id, const bool p_send_back) {
	m_containers.insert(p_environment_id);
	if (p_send_back) {
		m_new_containers.insert(p_environment_id);
	}
}
