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

#include "Environment.h"

#include <thread>

#include <uuid/UUID.hpp>

#include "Agent.h"
#include "PahoWrapper.hpp"

namespace {

	using cam::transport::ErrorCategory;
	using cam::transport::ErrorCode;
	using cam::transport::make_error;

	std::uint64_t now_ms() {
		return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::system_clock::now().time_since_epoch()).count());
	}

} // namespace

cam::Environment::Environment(const int p_no_turns, const EnvironmentMasMode& p_mode,
							  const int p_delay_after_turn, const unsigned int p_seed) :
		m_id(UUID::generate_uuid()),
		m_no_turns(p_no_turns),
		m_delay_after_turn(p_delay_after_turn),
		m_agent_collection(p_mode, p_seed),
		m_remote_client(nullptr) {
}

cam::Environment::~Environment() {
	if (m_endpoint) {
		disconnect();
	}
	if (m_remote_client) {
		m_remote_client->disconnect_to_broker();
		while (m_remote_client->is_connected());
		delete m_remote_client;
		m_remote_client = nullptr;
	}
}

const std::string& cam::Environment::add(AgentPointer&& p_agent) {
	m_agent_collection.add(p_agent);
	return p_agent->get_id();
}

cam::AgentPointer cam::Environment::get(const std::string& p_id) const {
	return m_agent_collection.get(p_id);
}

void cam::Environment::continue_simulation(const int p_no_turns) {
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

const cam::AgentPointer& cam::Environment::random_agent() const {
	return m_agent_collection.random_agent();
}

void cam::Environment::remove(const AgentPointer& p_agent) {
	const std::string l_id = p_agent->get_id();
	const std::string l_name = p_agent->get_name();
	m_agent_collection.remove(l_id);
	if (m_endpoint) {
		announce_agent(transport::DirectoryEventKind::AgentRemoved, l_id, l_name);
	}
}

void cam::Environment::remove(const std::string& p_agent_id) {
	remove(m_agent_collection.get(p_agent_id));
}

cam::transport::Result<void>
cam::Environment::send(const std::string& p_sender_id, const std::string& p_receiver_id, const uint8_t* p_message,
					   const size_t& p_length, const MessageBinaryFormat& p_binary_format,
					   const transport::SendOptions& p_options) const {
	if (deliver_local(p_sender_id, p_receiver_id, p_message, p_length, p_binary_format)) {
		return {};
	}
	if (!m_endpoint) {
		if (m_remote_client) {
			// Legacy MQTT path (PahoWrapper), replaced by the transport in a later step of mission 1.
			const json& l_data = {
					{"sender_id",     p_sender_id},
					{"receiver_id",   p_receiver_id},
					{"message",       std::vector(p_message, p_message + p_length)},
					{"length",        p_length},
					{"binary_format", p_binary_format}
			};
			const auto& l_message = m_remote_client->format_message(l_data, "object", "message");
			m_remote_client->send_message_by_id(cam::Message::to_binary(l_message));
			return {};
		}
		return make_error(ErrorCategory::Routing, ErrorCode::UnknownDestination, "unknown agent " + p_receiver_id);
	}
	const auto l_host = m_endpoint->locate_agent(p_receiver_id);
	if (!l_host) {
		m_endpoint->count_unknown_destination();
		return make_error(ErrorCategory::Routing, ErrorCode::UnknownDestination, "no environment hosts agent " + p_receiver_id);
	}
	return enqueue_message(m_endpoint->address(transport::AddressKind::EnvIn, *l_host), transport::TargetKind::Id,
						   p_receiver_id, {}, false, p_sender_id, p_message, p_length, p_binary_format, p_options);
}

bool cam::Environment::deliver_local(const std::string& p_sender_id, const std::string& p_receiver_id,
									 const uint8_t* p_message, const size_t& p_length,
									 const MessageBinaryFormat& p_binary_format) const {
	const AgentPointer l_agent = m_agent_collection.get(p_receiver_id);
	if (!l_agent) {
		return false;
	}
	if (!l_agent->is_dead()) {
		l_agent->post(std::make_shared<Message>(p_sender_id, p_receiver_id, p_message, p_length, p_binary_format));
	}
	return true;
}

void cam::Environment::deliver_broadcast(const std::string& p_sender_id, const uint8_t* p_message, const size_t& p_length,
										 const MessageBinaryFormat& p_binary_format) const {
	for (auto& [l_id, l_agent]: m_agent_collection.m_agents) {
		if (l_id != p_sender_id && !l_agent->is_dead()) {
			l_agent->post(std::make_shared<Message>(p_sender_id, l_id, p_message, p_length, p_binary_format));
		}
	}
}

bool cam::Environment::deliver_by_name(const std::string& p_sender_id, const std::string& p_receiver_name,
									   const uint8_t* p_message, const size_t& p_length, const bool p_is_fragment,
									   const bool p_first_only, const MessageBinaryFormat& p_binary_format) const {
	bool l_delivered = false;
	auto l_filtered_elements =
		m_agent_collection.m_agents_by_name | std::views::filter([p_is_fragment, p_receiver_name](auto& p_value) {
		if (p_is_fragment) {
			return p_value.first.find(p_receiver_name) != std::string::npos;
		}
		return p_value.first == p_receiver_name;
			});

	for (auto it = std::ranges::begin(l_filtered_elements); it != std::ranges::end(l_filtered_elements); ++it) {
		const auto& [l_name, l_id] = *it;
		const auto& l_agent = m_agent_collection.get(l_id);
		if (l_agent->is_dead()) {
			continue;
		}

		l_agent->post(std::make_shared<Message>(p_sender_id, l_id, p_message, p_length, p_binary_format));
		l_delivered = true;
		if (p_first_only) {
			break;
		}
	}
	return l_delivered;
}

cam::transport::Result<void>
cam::Environment::send_by_name(const std::string& p_sender_id, const std::string& p_receiver_name,
							   const uint8_t* p_message, const size_t& p_length, const bool p_is_fragment,
							   const bool p_first_only, const MessageBinaryFormat& p_binary_format,
							   const transport::SendOptions& p_options) const {
	deliver_by_name(p_sender_id, p_receiver_name, p_message, p_length, p_is_fragment, p_first_only, p_binary_format);
	if (m_remote_client && !m_endpoint) {
		// Legacy MQTT path (PahoWrapper): always published, whatever the scope.
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
		return {};
	}
	if (p_options.m_scope != transport::Scope::Network) {
		return {};
	}
	if (!m_endpoint) {
		return make_error(ErrorCategory::State, ErrorCode::NotConnected, "network scope asked while offline");
	}
	return enqueue_message(m_endpoint->address(transport::AddressKind::AllIn),
						   p_is_fragment ? transport::TargetKind::Fragment : transport::TargetKind::Name,
						   {}, p_receiver_name, p_first_only, p_sender_id, p_message, p_length, p_binary_format, p_options);
}

cam::transport::Result<void>
cam::Environment::broadcast(const std::string& p_sender_id, const uint8_t* p_message, const size_t& p_length,
							const MessageBinaryFormat& p_binary_format, const transport::SendOptions& p_options) const {
	deliver_broadcast(p_sender_id, p_message, p_length, p_binary_format);
	if (m_remote_client && !m_endpoint) {
		// Legacy MQTT path (PahoWrapper): always published, whatever the scope.
		const json& l_data = {
				{"sender_id",     p_sender_id},
				{"message",       std::vector(p_message, p_message + p_length)},
				{"binary_format", p_binary_format}
		};
		const auto& l_message = m_remote_client->format_message(l_data, "object", "message");
		m_remote_client->broadcast_message(cam::Message::to_binary(l_message));
		return {};
	}
	if (p_options.m_scope != transport::Scope::Network) {
		return {};
	}
	if (!m_endpoint) {
		return make_error(ErrorCategory::State, ErrorCode::NotConnected, "network scope asked while offline");
	}
	return enqueue_message(m_endpoint->address(transport::AddressKind::AllIn), transport::TargetKind::Broadcast,
						   {}, {}, false, p_sender_id, p_message, p_length, p_binary_format, p_options);
}

cam::transport::Result<void>
cam::Environment::enqueue_message(const transport::Address& p_address, const transport::TargetKind p_target_kind,
								  const std::string& p_target_id, const std::string& p_target_name, const bool p_first_only,
								  const std::string& p_sender_id, const uint8_t* p_message, const size_t& p_length,
								  const MessageBinaryFormat& p_binary_format, const transport::SendOptions& p_options) const {
	transport::Envelope l_envelope = m_endpoint->make_envelope(transport::EnvelopeKind::Message);
	l_envelope.m_sender_agent = p_sender_id;
	l_envelope.m_target_kind = p_target_kind;
	l_envelope.m_target_id = p_target_id;
	l_envelope.m_target_name = p_target_name;
	l_envelope.m_first_only = p_first_only;
	l_envelope.m_scope = p_options.m_scope;
	l_envelope.m_priority = p_options.m_priority;
	l_envelope.m_content_encoding = p_binary_format;
	if (p_message && p_length > 0) {
		l_envelope.m_content.assign(p_message, p_message + p_length);
	}
	return m_endpoint->enqueue(p_address, std::move(l_envelope));
}

void cam::Environment::start() {
	int l_turn = 0;

	process_buffers();
	while (true) {
		run_turn(l_turn++);
		if (m_no_turns != 0 && l_turn >= m_no_turns) {
			break;
		}
		process_buffers();
		if (m_agent_collection.count() == 0) {
			break;
		}
	}

	simulation_finished();
}

size_t cam::Environment::agents_count() const {
	return m_agent_collection.count();
}

std::vector<std::string>
cam::Environment::get_agents_by_name(const std::string& p_name, const bool p_first_only) const {
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

std::optional<std::string> cam::Environment::get_first_agent_by_name(const std::string& p_name) const {
	const auto& l_agents = get_agents_by_name(p_name, true);
	if (l_agents.empty()) {
		return {};
	}
	return l_agents[0];
}

std::optional<std::string> cam::Environment::get_agent_name(const std::string& p_id) const {
	const auto& l_agent = m_agent_collection.m_agents.find(p_id);
	if (l_agent == m_agent_collection.m_agents.end()) {
		return {};
	}
	return l_agent->second->get_name();
}

std::vector<std::string>
cam::Environment::get_filtered_agents(const std::string& p_fragment_name, const bool p_first_only) const {
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

void cam::Environment::move(const std::stringstream& p_agent_stream, const json& p_message,
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

const std::vector<cam::ObservablesPointer>
cam::Environment::get_list_of_observable_agents(const Agent* p_perceiving_agent) const {
	std::vector<cam::ObservablesPointer> l_observable_agent_list;

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


void cam::Environment::run_turn(const int p_turn) {
	m_agent_collection.run_turn();
	if (m_endpoint) {
		flush_outbox();
	}
	if (m_delay_after_turn) {
		std::this_thread::sleep_for(std::chrono::milliseconds(m_delay_after_turn));
	}
	turn_finished(p_turn);
}

void cam::Environment::process_buffers() {
	if (!m_endpoint) {
		m_agent_collection.process_buffers();
		return;
	}
	for (auto& l_envelope: m_endpoint->drain()) {
		apply_incoming(std::move(l_envelope));
	}
	const auto l_changes = m_agent_collection.process_buffers(true);
	for (const auto& [l_id, l_name]: l_changes.m_removed) {
		announce_agent(transport::DirectoryEventKind::AgentRemoved, l_id, l_name);
	}
	for (const auto& [l_id, l_name]: l_changes.m_added) {
		announce_agent(transport::DirectoryEventKind::AgentAdded, l_id, l_name);
	}
}

//###############################################################
//	Transport
//###############################################################

cam::transport::Result<void>
cam::Environment::connect(std::shared_ptr<transport::Transport> p_transport, transport::EndpointConfig p_config) {
	if (m_endpoint) {
		return make_error(ErrorCategory::State, ErrorCode::AlreadyConnected, "environment already connected");
	}
	if (!p_transport) {
		return make_error(ErrorCategory::State, ErrorCode::InvalidConfiguration, "null transport");
	}
	auto l_endpoint = std::make_unique<transport::Endpoint>(m_id, std::move(p_config));
	if (const auto l_result = l_endpoint->attach(std::move(p_transport)); !l_result) {
		return l_result;
	}
	m_endpoint = std::move(l_endpoint);
	announce_presence(true);
	flush_outbox();
	// Retained presences delivered by the transport at attach: known right away.
	for (auto& l_envelope: m_endpoint->drain()) {
		apply_incoming(std::move(l_envelope));
	}
	return {};
}

cam::transport::Result<void> cam::Environment::disconnect() {
	if (!m_endpoint) {
		return make_error(ErrorCategory::State, ErrorCode::NotConnected, "environment not connected");
	}
	announce_presence(false);
	flush_outbox();
	const auto l_result = m_endpoint->detach();
	m_endpoint.reset();
	return l_result;
}

std::vector<cam::transport::EnvironmentInfo> cam::Environment::environments() const {
	if (!m_endpoint) {
		return {};
	}
	return m_endpoint->environments();
}

cam::transport::Statistics cam::Environment::statistics() const {
	if (!m_endpoint) {
		return {};
	}
	return m_endpoint->statistics();
}

void cam::Environment::flush_outbox() {
	for (const auto& l_error: m_endpoint->flush_outbox()) {
		on_delivery_failure(l_error);
	}
}

void cam::Environment::announce_presence(const bool p_online) {
	transport::Presence l_presence;
	l_presence.m_node_id = m_endpoint->transport()->node_id();
	l_presence.m_environment_id = m_id;
	l_presence.m_environment_name = m_endpoint->name();
	l_presence.m_namespace = m_endpoint->ns();
	l_presence.m_online = p_online;
	l_presence.m_timestamp_ms = now_ms();
	if (p_online) {
		l_presence.m_agents.reserve(m_agent_collection.m_agents.size());
		for (const auto& [l_id, l_agent]: m_agent_collection.m_agents) {
			if (!l_agent->is_dead()) {
				l_presence.m_agents.emplace_back(l_id, l_agent->get_name());
			}
		}
	}
	transport::Envelope l_envelope = m_endpoint->make_envelope(transport::EnvelopeKind::Presence);
	l_envelope.m_payload = std::move(l_presence);
	if (const auto l_result = m_endpoint->enqueue(m_endpoint->address(transport::AddressKind::EnvPresence, m_id), std::move(l_envelope)); !l_result) {
		on_delivery_failure(l_result.error());
	}
}

void cam::Environment::announce_agent(const transport::DirectoryEventKind p_event, const std::string& p_agent_id,
									  const std::string& p_agent_name) {
	transport::DirectoryEvent l_event;
	l_event.m_event = p_event;
	l_event.m_agent_id = p_agent_id;
	l_event.m_agent_name = p_agent_name;
	l_event.m_environment_id = m_id;
	transport::Envelope l_envelope = m_endpoint->make_envelope(transport::EnvelopeKind::Directory);
	l_envelope.m_payload = std::move(l_event);
	if (const auto l_result = m_endpoint->enqueue(m_endpoint->address(transport::AddressKind::EnvPresence, m_id), std::move(l_envelope)); !l_result) {
		on_delivery_failure(l_result.error());
	}
}

void cam::Environment::apply_incoming(transport::Envelope&& p_envelope) {
	using transport::EnvelopeKind;
	using transport::TargetKind;

	switch (p_envelope.m_kind) {
		case EnvelopeKind::Message: {
			const uint8_t* l_bytes = p_envelope.m_content.empty() ? nullptr : p_envelope.m_content.data();
			const size_t l_length = p_envelope.m_content.size();
			switch (p_envelope.m_target_kind) {
				case TargetKind::Id: {
					const AgentPointer l_agent = m_agent_collection.get(p_envelope.m_target_id);
					if (!l_agent) {
						m_endpoint->count_unknown_destination();
					} else if (!l_agent->is_dead()) {
						l_agent->post(std::make_shared<Message>(p_envelope.m_sender_agent, p_envelope.m_target_id, l_bytes, l_length,
																p_envelope.m_content_encoding));
					}
					break;
				}
				case TargetKind::Name:
				case TargetKind::Fragment:
					deliver_by_name(p_envelope.m_sender_agent, p_envelope.m_target_name, l_bytes, l_length,
									p_envelope.m_target_kind == TargetKind::Fragment, p_envelope.m_first_only,
									p_envelope.m_content_encoding);
					break;
				case TargetKind::Broadcast:
					for (auto& [l_id, l_agent]: m_agent_collection.m_agents) {
						if (l_id != p_envelope.m_sender_agent && !l_agent->is_dead()) {
							l_agent->post(std::make_shared<Message>(p_envelope.m_sender_agent, l_id, l_bytes, l_length,
																	p_envelope.m_content_encoding));
						}
					}
					break;
				case TargetKind::Environment:
				default:
					m_endpoint->count_rejected();
					break;
			}
			break;
		}
		case EnvelopeKind::Presence: {
			const auto* l_presence = std::get_if<transport::Presence>(&p_envelope.m_payload);
			if (!l_presence) {
				m_endpoint->count_rejected();
				break;
			}
			const bool l_known = m_endpoint->knows_environment(l_presence->m_environment_id);
			m_endpoint->update_environment(*l_presence);
			transport::EnvironmentInfo l_info;
			l_info.m_id = l_presence->m_environment_id;
			l_info.m_name = l_presence->m_environment_name;
			l_info.m_node = l_presence->m_node_id;
			l_info.m_online = l_presence->m_online;
			l_info.m_last_seen = std::chrono::steady_clock::now();
			if (l_presence->m_online && !l_known) {
				// A newcomer may hold a stale picture of this environment: answer with a fresh presence.
				announce_presence(true);
				on_environment_joined(l_info);
			} else if (!l_presence->m_online && l_known) {
				on_environment_left(l_info);
			}
			break;
		}
		case EnvelopeKind::Directory: {
			const auto* l_event = std::get_if<transport::DirectoryEvent>(&p_envelope.m_payload);
			if (!l_event) {
				m_endpoint->count_rejected();
				break;
			}
			switch (l_event->m_event) {
				case transport::DirectoryEventKind::AgentAdded:
				case transport::DirectoryEventKind::AgentMoved:
					m_endpoint->set_agent_location(l_event->m_agent_id, l_event->m_environment_id);
					break;
				case transport::DirectoryEventKind::AgentRemoved:
				default:
					if (const auto l_host = m_endpoint->locate_agent(l_event->m_agent_id); l_host && *l_host == l_event->m_environment_id) {
						m_endpoint->remove_agent(l_event->m_agent_id);
					}
					break;
			}
			break;
		}
		default:
			// Agent migration and queries arrive in a later step of mission 1.
			m_endpoint->count_rejected();
			break;
	}
}

//###############################################################
//	Remote handlers
//###############################################################

int cam::Environment::initialise_remote_connection_by_address(const std::string& p_id,
															  const std::string& p_full_address,
															  const json& p_connection_options) {
	m_remote_client = new PahoWrapper(*this, p_id, p_full_address);
	return m_remote_client->initialise_remote_connection_by_address(p_connection_options);
}

int cam::Environment::initialise_remote_connection(const std::string& p_id,
												   const std::string& p_host,
												   int p_port,
												   const json& p_connection_options) {
	return initialise_remote_connection_by_address(p_id, "tcp://" + p_host + ":" + std::to_string(p_port),
												   p_connection_options);
}

void cam::Environment::on_new_agent(const json& p_json_message) {
	const std::string l_message = p_json_message["agent"];
	std::stringstream l_stream = std::stringstream(l_message);
	add<Agent>(l_stream);
}

void cam::Environment::on_new_environment(const std::string& p_environment_id, const bool p_send_back) {
	m_containers.insert(p_environment_id);
	if (p_send_back) {
		m_remote_client->callback_discovery(p_environment_id);
	}
}
