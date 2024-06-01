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

#include "Agent.h"

#include <uuid/UUID.hpp>

#include "Environment.h"

cam::Agent::Agent(std::string p_name, const bool p_using_observables) :
		m_id(UUID::generate_uuid()),
		m_name(std::move(p_name)),
		m_environment(nullptr),
		m_is_setup(false),
		m_is_dead(false),
		m_is_using_observables(p_using_observables),
		m_observables(new Observables()) {}

cam::Agent::Agent() :
		m_environment(nullptr),
		m_is_setup(false),
		m_is_dead(false),
		m_is_using_observables(false),
		m_observables(new Observables()) {
}

void cam::Agent::run_turn(const bool p_run_setup_separately) {
	if (is_dead()) {
		return;
	}

	if (p_run_setup_separately) {
		if (!is_setup()) {
			internal_setup();
		} else {
			if (is_using_observables()) {
				internal_see();
			}
			if (is_dead()) {
				return;
			}
			internal_action();
		}

	} else {
		internal_setup();
		if (is_dead()) {
			return;
		}
		if (is_using_observables()) {
			internal_see();
		}
		if (is_dead()) {
			return;
		}
		internal_action();
	}
}

void cam::Agent::internal_setup() {
	setup();
	m_is_setup = true;
}

void cam::Agent::internal_see() {
	see(m_environment->get_list_of_observable_agents(this));
}

void cam::Agent::internal_action() {
	if (MessagePointer l_message; m_messages.dequeue(l_message)) {
		do {
			action(l_message);
		} while (m_messages.dequeue(l_message));
	} else {
		default_action();
	}
}

void cam::Agent::stop() {
	m_is_dead = true;
}

void cam::Agent::post(const MessagePointer& p_message) {
	m_messages.enqueue(p_message);
}

void cam::Agent::send(const std::string& p_receiver_id, const json& p_message) const {
	const auto& l_binary_data = json::to_msgpack(p_message);
	get_environment()->send(m_id, p_receiver_id, &l_binary_data[0], l_binary_data.size(),
							MessageBinaryFormat::MessagePack);
}

void cam::Agent::send(const std::string& p_receiver_id, const uint8_t* p_message, const size_t& p_length) const {
	get_environment()->send(m_id, p_receiver_id, p_message, p_length);
}

void
cam::Agent::send_by_name(const std::string& p_receiver_name, const json& p_message, const bool p_first_only) const {
	const auto& l_binary_data = json::to_msgpack(p_message);
	get_environment()->send_by_name(m_id, p_receiver_name, &l_binary_data[0], l_binary_data.size(), false, p_first_only,
									MessageBinaryFormat::MessagePack);
}

void cam::Agent::send_by_name(const std::string& p_receiver_name, const uint8_t* p_message, const size_t& p_length,
							  const bool p_first_only) const {
	get_environment()->send_by_name(m_id, p_receiver_name, p_message, p_length, false, p_first_only);
}

void cam::Agent::send_by_fragment_name(const std::string& p_fragment_name, const json& p_message,
									   const bool p_first_only) const {
	const auto& l_binary_data = json::to_msgpack(p_message);
	get_environment()->send_by_name(m_id, p_fragment_name, &l_binary_data[0], l_binary_data.size(), true, p_first_only,
									MessageBinaryFormat::MessagePack);
}

void
cam::Agent::send_by_fragment_name(const std::string& p_fragment_name, const uint8_t* p_message, const size_t& p_length,
								  const bool p_first_only) const {
	get_environment()->send_by_name(m_id, p_fragment_name, p_message, p_length, true, p_first_only);
}

void cam::Agent::broadcast(const json& p_message) const {
	const auto& l_binary_data = json::to_msgpack(p_message);
	get_environment()->broadcast(m_id, &l_binary_data[0], l_binary_data.size(), MessageBinaryFormat::MessagePack);
}

void cam::Agent::broadcast(const uint8_t* p_message, const size_t& p_length) const {
	get_environment()->broadcast(m_id, p_message, p_length);
}

void cam::Agent::reset() {
	m_is_setup = false;
	m_is_dead = false;
}

void cam::Agent::internal_move(const std::stringstream& p_agent_stream, const json& p_message,
							   const std::string& p_environment_id) {
	get_environment()->move(p_agent_stream, p_message, p_environment_id);
	stop();
}

size_t cam::Agent::agents_count() const {
	return get_environment()->agents_count();
}

std::vector<std::string> cam::Agent::get_agents_by_name(const std::string& p_name, bool p_first_only) const {
	return get_environment()->get_agents_by_name(p_name, p_first_only);
}

std::optional<std::string> cam::Agent::get_first_agent_by_name(const std::string& p_name) const {
	return get_environment()->get_first_agent_by_name(p_name);
}

std::optional<std::string> cam::Agent::get_agent_name(const std::string& p_id) const {
	return get_environment()->get_agent_name(p_id);
}

std::vector<std::string> cam::Agent::get_filtered_agents(const std::string& p_fragment_name, bool p_first_only) const {
	return get_environment()->get_filtered_agents(p_fragment_name, p_first_only);
}

bool cam::Agent::perception_filter(const ObservablesPointer&) const {
	return false;
}

void cam::Agent::setup() {}

void cam::Agent::see(const std::vector<const ObservablesPointer>&) {}

void cam::Agent::action(const MessagePointer&) {}

void cam::Agent::default_action() {}
