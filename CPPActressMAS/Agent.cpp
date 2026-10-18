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

#include "Environment.h"
#include "util/Identifier.h"
#include "util/RecyclingAllocator.hpp"

cam::Agent::Agent(std::string p_name, const bool p_using_observables) :
		m_id(AgentId::generate()),
		m_name(std::move(p_name)),
		m_environment(nullptr),
		m_is_setup(false),
		m_is_dead(false),
		m_in_transit(false),
		m_is_inserted(false),
		m_is_using_observables(p_using_observables),
		m_observables(std::make_shared<Observables>()) {}

void cam::Agent::run_turn(const bool p_run_setup_separately) {
	if (is_dead() || is_in_transit()) {
		return;
	}
	run_phases(p_run_setup_separately);
}

void cam::Agent::run_parallel_turn() {
	if (is_in_transit()) {
		return;
	}
	run_phases(true);
}

void cam::Agent::run_phases(const bool p_run_setup_separately) {
	// Once its turn has begun the agent runs every phase, even if it stops on the way, as the C# (C26, ISO m)
	if (p_run_setup_separately) {
		if (!is_setup()) {
			internal_setup();
		} else {
			if (is_using_observables()) {
				internal_see();
			}
			internal_action();
		}

	} else {
		internal_setup();
		if (is_using_observables()) {
			internal_see();
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
	if (m_environment) {
		m_environment->retire(*this);
	} else {
		m_is_dead.store(true, std::memory_order_relaxed);  // stopped before being added (C11)
	}
}

void cam::Agent::post(const MessagePointer& p_message) {
	m_messages.enqueue(p_message);
}

void cam::Agent::receive(MessagePointer&& p_message) {
	m_messages.enqueue(std::move(p_message));
}

namespace {

	cam::MessageContent bytes_content(const uint8_t* p_message, const size_t& p_length) {
		return p_message ? std::vector<std::uint8_t>(p_message, p_message + p_length) : std::vector<std::uint8_t>();
	}

	cam::MessageContent json_content(const json& p_message) {
		// The block of the value and its counter is recycled per thread (P8)
		return std::shared_ptr<const json>(std::allocate_shared<json>(cam::RecyclingAllocator<json>(), p_message));
	}

	cam::MessageContent text_content(const char* p_text) {
		return std::string(p_text ? p_text : "");
	}

} // namespace

cam::transport::Result<void>
cam::Agent::send(const AgentId& p_receiver_id, const json& p_message, const transport::SendOptions& p_options) const {
	return get_environment()->send_content(m_id, p_receiver_id, json_content(p_message), p_options);
}

cam::transport::Result<void>
cam::Agent::send(const AgentId& p_receiver_id, std::string p_text, const transport::SendOptions& p_options) const {
	return get_environment()->send_content(m_id, p_receiver_id, MessageContent(std::move(p_text)), p_options);
}

cam::transport::Result<void>
cam::Agent::send(const AgentId& p_receiver_id, const char* p_text, const transport::SendOptions& p_options) const {
	return get_environment()->send_content(m_id, p_receiver_id, text_content(p_text), p_options);
}

cam::transport::Result<void>
cam::Agent::send_text(const AgentId& p_receiver_id, const Text& p_text, const transport::SendOptions& p_options) const {
	return get_environment()->send_content(m_id, p_receiver_id, MessageContent(p_text), p_options);
}

cam::transport::Result<void>
cam::Agent::send(const AgentId& p_receiver_id, const uint8_t* p_message, const size_t& p_length,
				 const transport::SendOptions& p_options) const {
	return get_environment()->send(m_id, p_receiver_id, p_message, p_length, p_options);
}

cam::transport::Result<void>
cam::Agent::send_by_name(const std::string& p_receiver_name, const json& p_message, const bool p_first_only,
						 const transport::SendOptions& p_options) const {
	return get_environment()->send_content_by_name(m_id, p_receiver_name, json_content(p_message), false, p_first_only, p_options);
}

cam::transport::Result<void>
cam::Agent::send_by_name(const std::string& p_receiver_name, std::string p_text, const bool p_first_only,
						 const transport::SendOptions& p_options) const {
	return get_environment()->send_content_by_name(m_id, p_receiver_name, MessageContent(std::move(p_text)), false, p_first_only, p_options);
}

cam::transport::Result<void>
cam::Agent::send_by_name(const std::string& p_receiver_name, const char* p_text, const bool p_first_only,
						 const transport::SendOptions& p_options) const {
	return get_environment()->send_content_by_name(m_id, p_receiver_name, text_content(p_text), false, p_first_only, p_options);
}

cam::transport::Result<void>
cam::Agent::send_text_by_name(const std::string& p_name, const Text& p_text, const bool p_is_fragment, const bool p_first_only,
							  const transport::SendOptions& p_options) const {
	return get_environment()->send_content_by_name(m_id, p_name, MessageContent(p_text), p_is_fragment, p_first_only, p_options);
}

cam::transport::Result<void>
cam::Agent::send_by_name(const std::string& p_receiver_name, const uint8_t* p_message, const size_t& p_length,
						 const bool p_first_only, const transport::SendOptions& p_options) const {
	return get_environment()->send_content_by_name(m_id, p_receiver_name, bytes_content(p_message, p_length), false, p_first_only,
												   p_options);
}

cam::transport::Result<void>
cam::Agent::send_by_fragment_name(const std::string& p_fragment_name, const json& p_message, const bool p_first_only,
								  const transport::SendOptions& p_options) const {
	return get_environment()->send_content_by_name(m_id, p_fragment_name, json_content(p_message), true, p_first_only, p_options);
}

cam::transport::Result<void>
cam::Agent::send_by_fragment_name(const std::string& p_fragment_name, std::string p_text, const bool p_first_only,
								  const transport::SendOptions& p_options) const {
	return get_environment()->send_content_by_name(m_id, p_fragment_name, MessageContent(std::move(p_text)), true, p_first_only, p_options);
}

cam::transport::Result<void>
cam::Agent::send_by_fragment_name(const std::string& p_fragment_name, const char* p_text, const bool p_first_only,
								  const transport::SendOptions& p_options) const {
	return get_environment()->send_content_by_name(m_id, p_fragment_name, text_content(p_text), true, p_first_only, p_options);
}

cam::transport::Result<void>
cam::Agent::send_by_fragment_name(const std::string& p_fragment_name, const uint8_t* p_message, const size_t& p_length,
								  const bool p_first_only, const transport::SendOptions& p_options) const {
	return get_environment()->send_content_by_name(m_id, p_fragment_name, bytes_content(p_message, p_length), true, p_first_only,
												   p_options);
}

cam::transport::Result<void> cam::Agent::broadcast(const json& p_message, const transport::SendOptions& p_options) const {
	return get_environment()->broadcast_content(m_id, json_content(p_message), p_options);
}

cam::transport::Result<void> cam::Agent::broadcast(std::string p_text, const transport::SendOptions& p_options) const {
	return get_environment()->broadcast_content(m_id, MessageContent(std::move(p_text)), p_options);
}

cam::transport::Result<void> cam::Agent::broadcast(const char* p_text, const transport::SendOptions& p_options) const {
	return get_environment()->broadcast_content(m_id, text_content(p_text), p_options);
}

cam::transport::Result<void> cam::Agent::broadcast_text(const Text& p_text, const transport::SendOptions& p_options) const {
	return get_environment()->broadcast_content(m_id, MessageContent(p_text), p_options);
}

cam::transport::Result<void>
cam::Agent::broadcast(const uint8_t* p_message, const size_t& p_length, const transport::SendOptions& p_options) const {
	return get_environment()->broadcast_content(m_id, bytes_content(p_message, p_length), p_options);
}

void cam::Agent::reset() {
	m_is_setup = false;
	m_is_dead.store(false, std::memory_order_relaxed);
}

cam::transport::Result<void> cam::Agent::move(const std::string& p_environment_id, const transport::SendOptions& p_options) {
	const auto l_result = get_environment()->request_move(*this, p_environment_id, p_options);
	if (l_result) {
		m_in_transit = true;
	}
	return l_result;
}

std::string cam::Agent::format(const MessagePointer& p_message) const {
	const auto l_name = [this](const AgentId& p_id) { return get_agent_name(p_id).value_or(p_id.str()); };
	return "[" + l_name(p_message->get_sender()) + " -> " + l_name(p_message->get_receiver()) + "]: " + p_message->to_string();
}

size_t cam::Agent::agents_count() const {
	return get_environment()->agents_count();
}

std::vector<cam::AgentId> cam::Agent::get_agents_by_name(const std::string& p_name, bool p_first_only) const {
	return get_environment()->get_agents_by_name(p_name, p_first_only);
}

std::optional<cam::AgentId> cam::Agent::get_first_agent_by_name(const std::string& p_name) const {
	return get_environment()->get_first_agent_by_name(p_name);
}

std::optional<std::string> cam::Agent::get_agent_name(const AgentId& p_id) const {
	return get_environment()->get_agent_name(p_id);
}

std::vector<cam::AgentId> cam::Agent::get_filtered_agents(const std::string& p_fragment_name, bool p_first_only) const {
	return get_environment()->get_filtered_agents(p_fragment_name, p_first_only);
}

bool cam::Agent::perception_filter(const ObservablesPointer&) const {
	return false;
}

void cam::Agent::setup() {}

void cam::Agent::see(const std::vector<ObservablesPointer>&) {}

void cam::Agent::action(const MessagePointer&) {}

void cam::Agent::default_action() {}
