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

#include "Agent.h"

#include <algorithm>
#include <thread>

#include "Environment.h"
#include "util/Identifier.h"
#include "util/RecyclingAllocator.hpp"

namespace {

	// Holds a lock for the time of a pointer copy: a short spin, never a sleep
	struct SpinGuard final {
		std::atomic_flag& m_flag;
		explicit SpinGuard(std::atomic_flag& p_flag) : m_flag(p_flag) {
			while (m_flag.test_and_set(std::memory_order_acquire)) {
				std::this_thread::yield();
			}
		}
		~SpinGuard() { m_flag.clear(std::memory_order_release); }
		SpinGuard(const SpinGuard&) = delete;
		SpinGuard& operator=(const SpinGuard&) = delete;
	};

} // namespace

cam::Agent::Agent(std::string p_name, const bool p_using_observables) :
		m_id(AgentId::generate()),
		m_name(std::move(p_name)),
		m_environment(nullptr),
		m_is_setup(false),
		m_state(0),
		m_in_transit(false),
		m_is_using_observables(p_using_observables),
		m_observables(std::make_shared<Observables>()) {}

void cam::Agent::run_turn(const bool p_run_setup_separately) {
	if (is_dead() || is_in_transit()) {
		return;
	}
	run_phases(p_run_setup_separately);
}

void cam::Agent::run_parallel_turn(const bool p_run_setup_separately) {
	if (is_in_transit()) {
		return;
	}
	run_phases(p_run_setup_separately);
}

void cam::Agent::run_phases(const bool p_run_setup_separately) {
	// Once its turn has begun the agent runs every phase, even if it stops on the way, as the C#
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
		// The setup shares the first turn of the agent: once, then see and the action, at every turn
		if (!is_setup()) {
			internal_setup();
		}
		if (is_using_observables()) {
			internal_see();
		}
		internal_action();
	}
	// The agents that run later in a sequential turn see what this one wrote, as the C#
	publish_observables();
}

cam::ObservablesPointer cam::Agent::published_observables() const {
	const SpinGuard l_guard(m_published_lock);
	return m_published;
}

void cam::Agent::publish_observables() {
	// Only the thread that runs this agent, or the simulation thread while it does not run, replaces the copy: reading it
	// here needs no lock. The former copy is released once the lock is free (l_copy outlives l_guard).
	if (m_published ? *m_published == *m_observables : m_observables->empty()) {
		return;
	}
	ObservablesPointer l_copy = m_observables->empty() ? nullptr : std::make_shared<const Observables>(*m_observables);
	const SpinGuard l_guard(m_published_lock);
	m_published.swap(l_copy);
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
		m_state.fetch_or(DEAD, std::memory_order_relaxed);  // stopped before being added
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
		// The block of the value and its counter is recycled per thread
		return std::shared_ptr<const json>(std::allocate_shared<json>(cam::RecyclingAllocator<json>(), p_message));
	}

	cam::MessageContent text_content(const char* p_text) {
		return std::string(p_text ? p_text : "");
	}

} // namespace

cam::transport::Result<void>
cam::Agent::send(const AgentId& p_receiver_id, const json& p_message, const transport::SendOptions& p_options) const {
	return get_environment()->send_content(m_id, m_name, p_receiver_id, json_content(p_message), p_options);
}

cam::transport::Result<void>
cam::Agent::send(const AgentId& p_receiver_id, std::string p_text, const transport::SendOptions& p_options) const {
	return get_environment()->send_content(m_id, m_name, p_receiver_id, MessageContent(std::move(p_text)), p_options);
}

cam::transport::Result<void>
cam::Agent::send(const AgentId& p_receiver_id, const char* p_text, const transport::SendOptions& p_options) const {
	return get_environment()->send_content(m_id, m_name, p_receiver_id, text_content(p_text), p_options);
}

cam::transport::Result<void>
cam::Agent::send_text(const AgentId& p_receiver_id, const Text& p_text, const transport::SendOptions& p_options) const {
	return get_environment()->send_content(m_id, m_name, p_receiver_id, MessageContent(p_text), p_options);
}

cam::transport::Result<void>
cam::Agent::send(const AgentId& p_receiver_id, const uint8_t* p_message, const size_t& p_length,
				 const transport::SendOptions& p_options) const {
	return get_environment()->send_content(m_id, m_name, p_receiver_id, bytes_content(p_message, p_length), p_options);
}

cam::transport::Result<void>
cam::Agent::send_by_name(const std::string& p_receiver_name, const json& p_message, const bool p_first_only,
						 const transport::SendOptions& p_options) const {
	return get_environment()->send_content_by_name(m_id, m_name, p_receiver_name, json_content(p_message), false, p_first_only, p_options);
}

cam::transport::Result<void>
cam::Agent::send_by_name(const std::string& p_receiver_name, std::string p_text, const bool p_first_only,
						 const transport::SendOptions& p_options) const {
	return get_environment()->send_content_by_name(m_id, m_name, p_receiver_name, MessageContent(std::move(p_text)), false, p_first_only, p_options);
}

cam::transport::Result<void>
cam::Agent::send_by_name(const std::string& p_receiver_name, const char* p_text, const bool p_first_only,
						 const transport::SendOptions& p_options) const {
	return get_environment()->send_content_by_name(m_id, m_name, p_receiver_name, text_content(p_text), false, p_first_only, p_options);
}

cam::transport::Result<void>
cam::Agent::send_text_by_name(const std::string& p_name, const Text& p_text, const bool p_is_fragment, const bool p_first_only,
							  const transport::SendOptions& p_options) const {
	return get_environment()->send_content_by_name(m_id, m_name, p_name, MessageContent(p_text), p_is_fragment, p_first_only, p_options);
}

cam::transport::Result<void>
cam::Agent::send_by_name(const std::string& p_receiver_name, const uint8_t* p_message, const size_t& p_length,
						 const bool p_first_only, const transport::SendOptions& p_options) const {
	return get_environment()->send_content_by_name(m_id, m_name, p_receiver_name, bytes_content(p_message, p_length), false, p_first_only,
												   p_options);
}

cam::transport::Result<void>
cam::Agent::send_by_fragment_name(const std::string& p_fragment_name, const json& p_message, const bool p_first_only,
								  const transport::SendOptions& p_options) const {
	return get_environment()->send_content_by_name(m_id, m_name, p_fragment_name, json_content(p_message), true, p_first_only, p_options);
}

cam::transport::Result<void>
cam::Agent::send_by_fragment_name(const std::string& p_fragment_name, std::string p_text, const bool p_first_only,
								  const transport::SendOptions& p_options) const {
	return get_environment()->send_content_by_name(m_id, m_name, p_fragment_name, MessageContent(std::move(p_text)), true, p_first_only, p_options);
}

cam::transport::Result<void>
cam::Agent::send_by_fragment_name(const std::string& p_fragment_name, const char* p_text, const bool p_first_only,
								  const transport::SendOptions& p_options) const {
	return get_environment()->send_content_by_name(m_id, m_name, p_fragment_name, text_content(p_text), true, p_first_only, p_options);
}

cam::transport::Result<void>
cam::Agent::send_by_fragment_name(const std::string& p_fragment_name, const uint8_t* p_message, const size_t& p_length,
								  const bool p_first_only, const transport::SendOptions& p_options) const {
	return get_environment()->send_content_by_name(m_id, m_name, p_fragment_name, bytes_content(p_message, p_length), true, p_first_only,
												   p_options);
}

cam::transport::Result<void> cam::Agent::broadcast(const json& p_message, const transport::SendOptions& p_options) const {
	return get_environment()->broadcast_content(m_id, m_name, json_content(p_message), p_options);
}

cam::transport::Result<void> cam::Agent::broadcast(std::string p_text, const transport::SendOptions& p_options) const {
	return get_environment()->broadcast_content(m_id, m_name, MessageContent(std::move(p_text)), p_options);
}

cam::transport::Result<void> cam::Agent::broadcast(const char* p_text, const transport::SendOptions& p_options) const {
	return get_environment()->broadcast_content(m_id, m_name, text_content(p_text), p_options);
}

cam::transport::Result<void> cam::Agent::broadcast_text(const Text& p_text, const transport::SendOptions& p_options) const {
	return get_environment()->broadcast_content(m_id, m_name, MessageContent(p_text), p_options);
}

cam::transport::Result<void>
cam::Agent::broadcast(const uint8_t* p_message, const size_t& p_length, const transport::SendOptions& p_options) const {
	return get_environment()->broadcast_content(m_id, m_name, bytes_content(p_message, p_length), p_options);
}

void cam::Agent::reset() {
	m_is_setup = false;
	m_state.fetch_and(static_cast<std::uint8_t>(~DEAD), std::memory_order_relaxed);
}

cam::transport::Result<void> cam::Agent::move(const std::string& p_environment_id, const transport::SendOptions& p_options) {
	const auto l_result = get_environment()->request_move(*this, p_environment_id, p_options);
	if (l_result) {
		m_in_transit = true;
	}
	return l_result;
}

bool cam::Agent::can_move(const std::string& p_environment_id) const {
	const Environment* const l_environment = get_environment();
	if (!l_environment->is_connected() || p_environment_id == l_environment->get_id()) {
		return false;
	}
	const auto l_environments = l_environment->environments();
	return std::any_of(l_environments.begin(), l_environments.end(), [&p_environment_id](const transport::EnvironmentInfo& p_info) {
		return p_info.m_online && p_info.m_id == p_environment_id;
	});
}

std::string cam::Agent::format(const MessagePointer& p_message) const {
	const auto l_name = [this](const AgentId& p_id) { return get_agent_name(p_id).value_or(p_id.str()); };
	// The sender as the message names it (the C# Message.Sender), even once it is removed or when it is remote
	const std::string& l_sender_name = p_message->get_sender_name();
	const std::string l_sender = l_sender_name.empty() ? l_name(p_message->get_sender()) : l_sender_name;
	return "[" + l_sender + " -> " + l_name(p_message->get_receiver()) + "]: " + p_message->to_string();
}

// The queries of an agent run during its turn, when no boundary changes the collection: without the lock of the
// environment, which the calls from other threads take
size_t cam::Agent::agents_count() const {
	return get_environment()->count_agents();
}

std::vector<cam::AgentId> cam::Agent::get_agents_by_name(const std::string& p_name, bool p_first_only) const {
	return get_environment()->find_agents(p_name, false, p_first_only);
}

std::optional<cam::AgentId> cam::Agent::get_first_agent_by_name(const std::string& p_name) const {
	const auto l_agents = get_environment()->find_agents(p_name, false, true);
	return l_agents.empty() ? std::nullopt : std::optional<AgentId>(l_agents[0]);
}

std::optional<std::string> cam::Agent::get_agent_name(const AgentId& p_id) const {
	return get_environment()->find_agent_name(p_id);
}

std::vector<cam::AgentId> cam::Agent::get_filtered_agents(const std::string& p_fragment_name, bool p_first_only) const {
	return get_environment()->find_agents(p_fragment_name, true, p_first_only);
}

std::vector<cam::AgentId> cam::Agent::all_agents() const {
	return get_environment()->list_agents();
}

bool cam::Agent::perception_filter(const ObservablesPointer&) const {
	return false;
}

void cam::Agent::setup() {}

void cam::Agent::see(const std::vector<ObservablesPointer>&) {}

void cam::Agent::action(const MessagePointer&) {}

void cam::Agent::default_action() {}
