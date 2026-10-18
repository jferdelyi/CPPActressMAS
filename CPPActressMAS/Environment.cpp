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

#include "Agent.h"
#include "transport/codec/ContentCodec.h"
#include "transport/Facade.h"
#include "util/Identifier.h"
#include "util/RecyclingAllocator.hpp"

namespace {

	// Bytes given by pointer and length become a bytes content (empty when the pointer is null).
	cam::MessageContent make_content(const uint8_t* p_message, const size_t& p_length) {
		return p_message ? std::vector<std::uint8_t>(p_message, p_message + p_length) : std::vector<std::uint8_t>();
	}

} // namespace

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
		m_id(generate_uuid()),
		m_no_turns(p_no_turns),
		m_delay_after_turn(p_delay_after_turn),
		m_agent_collection(p_mode, p_seed),
		m_boundary(0) {
}

cam::Environment::~Environment() {
	if (m_endpoint) {
		disconnect();
	}
}

cam::AgentId cam::Environment::add(AgentPointer p_agent) {
	p_agent->set_environment(this);
	m_agent_collection.add(p_agent);
	return p_agent->get_id();
}

cam::AgentPointer cam::Environment::get(const AgentId& p_id) const {
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
	if (!p_agent) {
		return;  // unknown, or added but not yet inserted (C5)
	}
	// During a turn the collections do not change: the agent is marked, the boundary erases and announces it (C10)
	if (m_agent_collection.is_turn_running()) {
		retire(*p_agent);
		return;
	}
	const AgentId l_id = p_agent->get_id();
	const std::string l_name = p_agent->get_name();
	m_agent_collection.remove(l_id);
	if (m_endpoint) {
		announce_agent(transport::DirectoryEventKind::AgentRemoved, l_id, l_name);
	}
}

void cam::Environment::retire(Agent& p_agent) {
	// Counted once, and only while the agent is in the collection (an agent not yet inserted is counted at insertion)
	if (!p_agent.m_is_dead.exchange(true, std::memory_order_relaxed) && p_agent.m_is_inserted) {
		m_agent_collection.m_retired.fetch_add(1, std::memory_order_relaxed);
	}
}

void cam::Environment::remove(const AgentId& p_agent_id) {
	remove(m_agent_collection.get(p_agent_id));
}

cam::transport::Result<void>
cam::Environment::send_content(const AgentId& p_sender_id, const AgentId& p_receiver_id, MessageContent p_content,
							   const transport::SendOptions& p_options) const {
	// A local receiver takes the content without a copy; the transport needs it otherwise
	if (Agent* const l_agent = m_agent_collection.find(p_receiver_id)) {
		if (!l_agent->is_dead()) {
			l_agent->receive(make_message(p_sender_id, p_receiver_id, std::move(p_content), {}));
		}
		return {};
	}
	if (!m_endpoint) {
		return make_error(ErrorCategory::Routing, ErrorCode::UnknownDestination, "unknown agent " + p_receiver_id.str());
	}
	const auto l_host = m_endpoint->locate_agent(p_receiver_id.str());
	if (!l_host) {
		m_endpoint->count_unknown_destination();
		return make_error(ErrorCategory::Routing, ErrorCode::UnknownDestination, "no environment hosts agent " + p_receiver_id.str());
	}
	return enqueue_content(m_endpoint->address(transport::AddressKind::EnvIn, *l_host), transport::TargetKind::Id,
						   p_receiver_id.str(), {}, false, p_sender_id.str(), p_content, p_options);
}

cam::transport::Result<void>
cam::Environment::send(const AgentId& p_sender_id, const AgentId& p_receiver_id, const uint8_t* p_message,
					   const size_t& p_length, const transport::SendOptions& p_options) const {
	// A local receiver gets a message whose content is built once from the bytes (P8, P12)
	if (Agent* const l_agent = m_agent_collection.find(p_receiver_id)) {
		if (!l_agent->is_dead()) {
			l_agent->receive(make_message(p_sender_id, p_receiver_id, make_content(p_message, p_length), {}));
		}
		return {};
	}
	return send_content(p_sender_id, p_receiver_id, make_content(p_message, p_length), p_options);
}

cam::MessagePointer cam::Environment::make_message(const AgentId& p_sender_id, const AgentId& p_receiver_id,
												   MessageContent&& p_content, const std::string& p_sender_environment) {
	// The message carries its counter, and its block is recycled by the thread that drops the last pointer (P8, D76)
	return MessagePointer(new Message(Message::Key(), p_sender_id, p_receiver_id, std::move(p_content), p_sender_environment));
}

bool cam::Environment::deliver_local(const AgentId& p_sender_id, const AgentId& p_receiver_id,
									 MessageContent p_content, const std::string& p_sender_environment) const {
	Agent* const l_agent = m_agent_collection.find(p_receiver_id);
	if (!l_agent) {
		return false;
	}
	if (!l_agent->is_dead()) {
		l_agent->receive(make_message(p_sender_id, p_receiver_id, std::move(p_content), p_sender_environment));
	}
	return true;
}

void cam::Environment::deliver_broadcast(const AgentId& p_sender_id, MessageContent p_content,
										 const std::string& p_sender_environment) const {
	// Each receiver is served once the next one is known: the last one takes the content without a copy
	const AgentId* l_last_id = nullptr;
	const AgentPointer* l_last_agent = nullptr;
	for (auto& [l_id, l_agent]: m_agent_collection.m_agents) {
		if (l_id != p_sender_id && !l_agent->is_dead()) {
			if (l_last_agent) {
				(*l_last_agent)->receive(make_message(p_sender_id, *l_last_id, MessageContent(p_content), p_sender_environment));
			}
			l_last_id = &l_id;
			l_last_agent = &l_agent;
		}
	}
	if (l_last_agent) {
		(*l_last_agent)->receive(make_message(p_sender_id, *l_last_id, std::move(p_content), p_sender_environment));
	}
}

bool cam::Environment::deliver_by_name(const AgentId& p_sender_id, const std::string& p_receiver_name,
									   MessageContent p_content, const bool p_is_fragment, const bool p_first_only,
									   const std::string& p_sender_environment) const {
	// Each receiver is served once the next one is known: the last one takes the content without a copy
	const AgentId* l_last_id = nullptr;
	Agent* l_last_agent = nullptr;
	// Visit a matching agent; false once the search is over (first_only)
	const auto l_visit = [&](const AgentId& p_id) {
		Agent* const l_agent = m_agent_collection.find(p_id);
		if (!l_agent || l_agent->is_dead()) {
			return true;
		}
		if (l_last_agent) {
			l_last_agent->receive(make_message(p_sender_id, *l_last_id, MessageContent(p_content), p_sender_environment));
		}
		l_last_id = &p_id;
		l_last_agent = l_agent;
		return !p_first_only;
	};
	if (p_is_fragment) {
		for (const auto& [l_name, l_id]: m_agent_collection.m_agents_by_name) {
			if (l_name.find(p_receiver_name) != std::string::npos && !l_visit(l_id)) {
				break;
			}
		}
	} else {
		// A name is a key of the index: no scan of every agent (P13)
		const auto [l_begin, l_end] = m_agent_collection.m_agents_by_name.equal_range(p_receiver_name);
		for (auto l_it = l_begin; l_it != l_end; ++l_it) {
			if (!l_visit(l_it->second)) {
				break;
			}
		}
	}
	if (!l_last_agent) {
		return false;
	}
	l_last_agent->receive(make_message(p_sender_id, *l_last_id, std::move(p_content), p_sender_environment));
	return true;
}

cam::transport::Result<void>
cam::Environment::send_content_by_name(const AgentId& p_sender_id, const std::string& p_receiver_name,
									   MessageContent p_content, const bool p_is_fragment, const bool p_first_only,
									   const transport::SendOptions& p_options) const {
	if (p_options.m_scope != transport::Scope::Network) {
		deliver_by_name(p_sender_id, p_receiver_name, std::move(p_content), p_is_fragment, p_first_only);
		return {};
	}
	deliver_by_name(p_sender_id, p_receiver_name, p_content, p_is_fragment, p_first_only);
	if (!m_endpoint) {
		return make_error(ErrorCategory::State, ErrorCode::NotConnected, "network scope asked while offline");
	}
	return enqueue_content(m_endpoint->address(transport::AddressKind::AllIn),
						   p_is_fragment ? transport::TargetKind::Fragment : transport::TargetKind::Name,
						   {}, p_receiver_name, p_first_only, p_sender_id.str(), p_content, p_options);
}

cam::transport::Result<void>
cam::Environment::send_by_name(const AgentId& p_sender_id, const std::string& p_receiver_name,
							   const uint8_t* p_message, const size_t& p_length, const bool p_is_fragment,
							   const bool p_first_only, const transport::SendOptions& p_options) const {
	return send_content_by_name(p_sender_id, p_receiver_name, make_content(p_message, p_length), p_is_fragment, p_first_only, p_options);
}

cam::transport::Result<void>
cam::Environment::broadcast_content(const AgentId& p_sender_id, MessageContent p_content,
									const transport::SendOptions& p_options) const {
	if (p_options.m_scope != transport::Scope::Network) {
		deliver_broadcast(p_sender_id, std::move(p_content));
		return {};
	}
	deliver_broadcast(p_sender_id, p_content);
	if (!m_endpoint) {
		return make_error(ErrorCategory::State, ErrorCode::NotConnected, "network scope asked while offline");
	}
	return enqueue_content(m_endpoint->address(transport::AddressKind::AllIn), transport::TargetKind::Broadcast,
						   {}, {}, false, p_sender_id.str(), p_content, p_options);
}

cam::transport::Result<void>
cam::Environment::broadcast(const AgentId& p_sender_id, const uint8_t* p_message, const size_t& p_length,
							const transport::SendOptions& p_options) const {
	return broadcast_content(p_sender_id, make_content(p_message, p_length), p_options);
}

cam::transport::Result<void>
cam::Environment::enqueue_message(const transport::Address& p_address, const transport::TargetKind p_target_kind,
								  const std::string& p_target_id, const std::string& p_target_name, const bool p_first_only,
								  const std::string& p_sender_id, std::vector<std::uint8_t> p_content,
								  const MessageBinaryFormat p_content_encoding, const transport::SendOptions& p_options,
								  const std::string& p_origin_environment) const {
	transport::Envelope l_envelope = m_endpoint->make_envelope(transport::EnvelopeKind::Message);
	l_envelope.m_origin_environment = p_origin_environment == m_id ? std::string() : p_origin_environment;
	l_envelope.m_sender_agent = p_sender_id;
	l_envelope.m_target_kind = p_target_kind;
	l_envelope.m_target_id = p_target_id;
	l_envelope.m_target_name = p_target_name;
	l_envelope.m_first_only = p_first_only;
	l_envelope.m_scope = p_options.m_scope;
	l_envelope.m_priority = p_options.m_priority;
	l_envelope.m_content_encoding = p_content_encoding;
	l_envelope.m_content = std::move(p_content);
	return m_endpoint->enqueue(p_address, std::move(l_envelope));
}

cam::transport::Result<void>
cam::Environment::enqueue_content(const transport::Address& p_address, const transport::TargetKind p_target_kind,
								  const std::string& p_target_id, const std::string& p_target_name, const bool p_first_only,
								  const std::string& p_sender_id, const MessageContent& p_content,
								  const transport::SendOptions& p_options, const std::string& p_origin_environment) const {
	// The content is encoded once here, when it leaves the node.
	const auto l_encoding = transport::wire_encoding(p_content, p_options);
	if (!l_encoding) {
		return l_encoding.error();
	}
	auto l_bytes = transport::encode_content(p_content, l_encoding.value());
	if (!l_bytes) {
		return l_bytes.error();
	}
	return enqueue_message(p_address, p_target_kind, p_target_id, p_target_name, p_first_only, p_sender_id,
						   std::move(l_bytes.value()), l_encoding.value(), p_options, p_origin_environment);
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

std::vector<cam::AgentId>
cam::Environment::get_agents_by_name(const std::string& p_name, const bool p_first_only) const {
	std::vector<AgentId> l_returned_agents;
	for (const auto& [l_id, l_agent]: m_agent_collection.m_agents) {
		if (l_agent->get_name() == p_name && !l_agent->is_dead()) {
			l_returned_agents.push_back(l_id);
			if (p_first_only) {
				break;
			}
		}
	}
	return l_returned_agents;
}

std::optional<cam::AgentId> cam::Environment::get_first_agent_by_name(const std::string& p_name) const {
	const auto& l_agents = get_agents_by_name(p_name, true);
	if (l_agents.empty()) {
		return {};
	}
	return l_agents[0];
}

std::optional<std::string> cam::Environment::get_agent_name(const AgentId& p_id) const {
	const auto& l_agent = m_agent_collection.m_agents.find(p_id);
	if (l_agent == m_agent_collection.m_agents.end()) {
		return {};
	}
	return l_agent->second->get_name();
}

std::vector<cam::AgentId>
cam::Environment::get_filtered_agents(const std::string& p_fragment_name, const bool p_first_only) const {
	std::vector<AgentId> l_returned_agents;
	for (const auto& [l_id, l_agent]: m_agent_collection.m_agents) {
		if (l_agent->get_name().find(p_fragment_name) != std::string::npos && !l_agent->is_dead()) {
			l_returned_agents.push_back(l_agent->get_id());
			if (p_first_only) {
				break;
			}
		}
	}
	return l_returned_agents;
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
	process_move_requests();
	if (m_endpoint) {
		flush_outbox();
	}
	if (m_delay_after_turn) {
		std::this_thread::sleep_for(std::chrono::milliseconds(m_delay_after_turn));
	}
	turn_finished(p_turn);
}

void cam::Environment::process_buffers() {
	++m_boundary;
	if (!m_endpoint) {
		m_agent_collection.process_buffers();
		return;
	}
	for (const auto l_event: m_endpoint->take_events()) {
		if (l_event == transport::TransportEvent::Reconnected) {
			// The broker may have lost the retained presences: announce this environment again (N15).
			announce_presence(true);
		}
		on_transport_event(l_event);
	}
	for (auto& l_envelope: m_endpoint->drain()) {
		apply_incoming(std::move(l_envelope));
	}
	expire_migrations();
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

cam::transport::Result<void> cam::Environment::connect() {
	if (m_transport_url.empty()) {
		return make_error(ErrorCategory::State, ErrorCode::InvalidConfiguration, "no transport URL: call set_transport_url or connect(url)");
	}
	return connect(m_transport_url, transport::EndpointConfig{});
}

cam::transport::Result<void> cam::Environment::connect(const std::string& p_url, transport::EndpointConfig p_config) {
	if (m_endpoint) {
		return make_error(ErrorCategory::State, ErrorCode::AlreadyConnected, "environment already connected");
	}
	m_transport_url = p_url;
	auto l_transport = m_transport_factory ? m_transport_factory(p_url, transport::TransportConfig{}) : transport::open(p_url);
	if (!l_transport) {
		return l_transport.error();
	}
	return connect(std::move(l_transport.value()), std::move(p_config));
}

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
	// Migrations cannot survive the transport: agents leaving resume here, agents arriving are dropped.
	{
		const std::lock_guard<std::mutex> l_lock(m_move_mutex);
		for (const auto& l_request: m_move_requests) {
			if (const AgentPointer l_agent = m_agent_collection.get(l_request.m_agent_id); l_agent) {
				l_agent->m_in_transit = false;
			}
		}
		m_move_requests.clear();
	}
	for (const auto& [l_transfer_id, l_transit]: m_transits) {
		restore_agent(l_transit, make_error(ErrorCategory::State, ErrorCode::NotConnected, "disconnected during the move"));
	}
	m_transits.clear();
	for (std::size_t i = 0; i < m_arrivals.size(); i++) {
		m_endpoint->count_migration_failed();
	}
	m_arrivals.clear();
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
				l_presence.m_agents.emplace_back(l_id.str(), l_agent->get_name());
			}
		}
	}
	transport::Envelope l_envelope = m_endpoint->make_envelope(transport::EnvelopeKind::Presence);
	l_envelope.m_payload = std::move(l_presence);
	const auto l_presence_address = m_endpoint->address(transport::AddressKind::EnvPresence, m_id);
	if (const auto l_result = m_endpoint->enqueue(l_presence_address, std::move(l_envelope)); !l_result) {
		on_delivery_failure(l_result.error());
	}
}

void cam::Environment::announce_agent(const transport::DirectoryEventKind p_event, const AgentId& p_agent_id,
									  const std::string& p_agent_name) {
	transport::DirectoryEvent l_event;
	l_event.m_event = p_event;
	l_event.m_agent_id = p_agent_id.str();
	l_event.m_agent_name = p_agent_name;
	l_event.m_environment_id = m_id;
	transport::Envelope l_envelope = m_endpoint->make_envelope(transport::EnvelopeKind::Directory);
	l_envelope.m_payload = std::move(l_event);
	const auto l_presence_address = m_endpoint->address(transport::AddressKind::EnvPresence, m_id);
	if (const auto l_result = m_endpoint->enqueue(l_presence_address, std::move(l_envelope)); !l_result) {
		on_delivery_failure(l_result.error());
	}
}

void cam::Environment::apply_incoming(transport::Envelope&& p_envelope) {
	using transport::EnvelopeKind;
	using transport::TargetKind;

	switch (p_envelope.m_kind) {
		case EnvelopeKind::Message: {
			const std::string& l_origin = p_envelope.effective_sender_environment();
			// No agent identifier here is longer than AgentId::CAPACITY: such a message is refused and counted (D76)
			const auto l_sender = AgentId::parse(p_envelope.m_sender_agent);
			const auto l_target = AgentId::parse(p_envelope.m_target_id);
			if (!l_sender || !l_target) {
				m_endpoint->count_rejected();
				break;
			}
			if (p_envelope.m_target_kind == TargetKind::Id && !m_agent_collection.contains(*l_target)) {
				if (const auto l_moved = m_moved_agents.find(*l_target); l_moved != m_moved_agents.end()) {
					// The agent left recently and the sender's directory lags behind: forward the bytes as they
					// came to its new host.
					transport::SendOptions l_options;
					l_options.m_priority = p_envelope.m_priority;
					if (enqueue_message(m_endpoint->address(transport::AddressKind::EnvIn, l_moved->second.m_host), TargetKind::Id,
										p_envelope.m_target_id, {}, false, p_envelope.m_sender_agent, std::move(p_envelope.m_content),
										p_envelope.m_content_encoding, l_options, l_origin)) {
						m_endpoint->count_forwarded();
					}
				} else {
					m_endpoint->count_unknown_destination();
				}
				break;
			}
			// The content is decoded once here, at the turn boundary, for every local receiver.
			auto l_content = transport::decode_content(p_envelope.m_content, p_envelope.m_content_encoding);
			if (!l_content) {
				m_endpoint->count_rejected();
				on_delivery_failure(l_content.error());
				break;
			}
			switch (p_envelope.m_target_kind) {
				case TargetKind::Id:
					deliver_local(*l_sender, *l_target, std::move(l_content.value()), l_origin);
					break;
				case TargetKind::Name:
				case TargetKind::Fragment:
					deliver_by_name(*l_sender, p_envelope.m_target_name, std::move(l_content.value()),
									p_envelope.m_target_kind == TargetKind::Fragment, p_envelope.m_first_only, l_origin);
					break;
				case TargetKind::Broadcast:
					deliver_broadcast(*l_sender, std::move(l_content.value()), l_origin);
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
			if (l_presence->m_environment_id == l_presence->m_node_id) {
				// Presence of a node (a transport instance): its death takes every environment it hosted.
				if (!l_presence->m_online) {
					for (const auto& l_lost: m_endpoint->purge_node(l_presence->m_node_id)) {
						on_environment_left(l_lost);
					}
				}
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
		case EnvelopeKind::AgentOffer:
			apply_agent_offer(p_envelope);
			break;
		case EnvelopeKind::AgentAccept:
		case EnvelopeKind::AgentReject:
		case EnvelopeKind::AgentCommit:
		case EnvelopeKind::AgentAbort:
			apply_agent_control(p_envelope);
			break;
		default:
			// Queries and replies belong to transports with request/reply (mission 2).
			m_endpoint->count_rejected();
			break;
	}
}

//###############################################################
//	Migration
//###############################################################

cam::transport::Result<void> cam::Environment::request_move(const Agent& p_agent, const std::string& p_environment_id,
															const transport::SendOptions& p_options) {
	if (!m_endpoint) {
		return make_error(ErrorCategory::State, ErrorCode::NotConnected, "move needs a transport");
	}
	if (p_agent.is_in_transit()) {
		return make_error(ErrorCategory::State, ErrorCode::InTransit, "agent already moving");
	}
	if (p_environment_id == m_id) {
		return make_error(ErrorCategory::Routing, ErrorCode::InvalidTarget, "the agent is already in this environment");
	}
	if (!m_endpoint->knows_environment(p_environment_id)) {
		return make_error(ErrorCategory::Routing, ErrorCode::UnknownDestination, "unknown environment " + p_environment_id);
	}
	if (!m_agent_type_names.contains(std::type_index(typeid(p_agent)))) {
		return make_error(ErrorCategory::Validation, ErrorCode::UnknownAgentType,
						  "agent type not registered here (Environment::register_agent_type)");
	}
	const std::lock_guard<std::mutex> l_lock(m_move_mutex);
	m_move_requests.push_back(MoveRequest{p_agent.get_id(), p_environment_id, p_options.m_priority});
	return {};
}

void cam::Environment::process_move_requests() {
	std::vector<MoveRequest> l_requests;
	{
		const std::lock_guard<std::mutex> l_lock(m_move_mutex);
		l_requests.swap(m_move_requests);
	}
	for (const auto& l_request: l_requests) {
		const AgentPointer l_agent = m_agent_collection.get(l_request.m_agent_id);
		if (!l_agent) {
			continue;
		}
		if (!m_endpoint) {
			l_agent->m_in_transit = false;
			continue;
		}
		Transit l_transit;
		l_transit.m_agent_id = l_request.m_agent_id;
		l_transit.m_target = l_request.m_target;
		l_transit.m_deadline = std::chrono::steady_clock::now() + m_endpoint->config().m_migration_timeout;
		l_transit.m_carried = l_agent->take_messages();

		const Agent& l_agent_ref = *l_agent;
		transport::AgentTransfer l_transfer;
		l_transfer.m_transfer_id = generate_uuid();
		l_transfer.m_type_name = m_agent_type_names.at(std::type_index(typeid(l_agent_ref)));
		l_transfer.m_agent_id = l_agent->get_id().str();
		l_transfer.m_agent_name = l_agent->get_name();
		l_transfer.m_state_encoding = MessageBinaryFormat::MessagePack;
		l_transfer.m_state = json::to_msgpack(l_agent->save_state());
		l_transfer.m_uses_observables = l_agent->is_using_observables();
		json l_observables = json::object();
		for (const auto& [l_key, l_value]: *l_agent->m_observables) {
			l_observables[l_key] = l_value;
		}
		l_transfer.m_observables = json::to_msgpack(l_observables);
		for (const auto& l_message: l_transit.m_carried) {
			const auto l_encoding = transport::wire_encoding(l_message->get_content(), {});
			auto l_bytes = l_encoding ? transport::encode_content(l_message->get_content(), l_encoding.value())
									  : transport::Result<std::vector<std::uint8_t>>(l_encoding.error());
			if (!l_bytes) {
				on_delivery_failure(l_bytes.error());
				continue;
			}
			transport::Envelope l_queued;
			l_queued.m_kind = transport::EnvelopeKind::Message;
			l_queued.m_sender_environment = m_id;
			l_queued.m_origin_environment = l_message->get_sender_environment();
			l_queued.m_epoch = m_endpoint->epoch();
			l_queued.m_stream_kind = transport::StreamKind::In;
			l_queued.m_stream_target = l_request.m_target;
			l_queued.m_sender_agent = l_message->get_sender().str();
			l_queued.m_target_kind = transport::TargetKind::Id;
			l_queued.m_target_id = l_agent->get_id().str();
			l_queued.m_content_encoding = l_encoding.value();
			l_queued.m_content = std::move(l_bytes.value());
			l_transfer.m_queue.push_back(std::move(l_queued));
		}
		const std::string l_transfer_id = l_transfer.m_transfer_id;
		transport::Envelope l_offer = m_endpoint->make_envelope(transport::EnvelopeKind::AgentOffer);
		l_offer.m_priority = l_request.m_priority;
		l_offer.m_payload = std::move(l_transfer);
		const auto l_offer_address = m_endpoint->address(transport::AddressKind::EnvAgent, l_request.m_target);
		if (const auto l_result = m_endpoint->enqueue(l_offer_address, std::move(l_offer)); !l_result) {
			restore_agent(l_transit, l_result.error());
			continue;
		}
		m_transits.emplace(l_transfer_id, std::move(l_transit));
	}
}

void cam::Environment::send_control(const transport::EnvelopeKind p_kind, const std::string& p_environment_id,
									const std::string& p_transfer_id, const AgentId& p_agent_id, const bool p_accepted,
									const std::string& p_reason) {
	transport::AgentControl l_control;
	l_control.m_transfer_id = p_transfer_id;
	l_control.m_agent_id = p_agent_id.str();
	l_control.m_accepted = p_accepted;
	l_control.m_reason = p_reason;
	transport::Envelope l_envelope = m_endpoint->make_envelope(p_kind);
	l_envelope.m_payload = std::move(l_control);
	// Decisions of the destination go to the origin's inbox; orders of the origin go to the destination's agent key.
	const bool l_to_origin = p_kind == transport::EnvelopeKind::AgentAccept || p_kind == transport::EnvelopeKind::AgentReject;
	const auto l_kind = l_to_origin ? transport::AddressKind::EnvIn : transport::AddressKind::EnvAgent;
	const auto l_address = m_endpoint->address(l_kind, p_environment_id);
	if (const auto l_result = m_endpoint->enqueue(l_address, std::move(l_envelope)); !l_result) {
		on_delivery_failure(l_result.error());
	}
}

void cam::Environment::restore_agent(const Transit& p_transit, const transport::Error& p_error) {
	if (const AgentPointer l_agent = m_agent_collection.get(p_transit.m_agent_id); l_agent) {
		// Messages carried by the offer come back first, then those received while leaving.
		const auto l_held = l_agent->take_messages();
		for (const auto& l_message: p_transit.m_carried) {
			l_agent->post(l_message);
		}
		for (const auto& l_message: l_held) {
			l_agent->post(l_message);
		}
		l_agent->m_in_transit = false;
	}
	m_endpoint->count_migration_failed();
	on_delivery_failure(p_error);
}

void cam::Environment::forward_messages(const AgentId& p_agent_id, const std::string& p_host,
										const std::vector<MessagePointer>& p_messages) {
	for (const auto& l_message: p_messages) {
		if (enqueue_content(m_endpoint->address(transport::AddressKind::EnvIn, p_host), transport::TargetKind::Id, p_agent_id.str(), {},
							false, l_message->get_sender().str(), l_message->get_content(), {}, l_message->get_sender_environment())) {
			m_endpoint->count_forwarded();
		}
	}
}

void cam::Environment::expire_migrations() {
	const auto l_now = std::chrono::steady_clock::now();
	for (auto l_it = m_transits.begin(); l_it != m_transits.end();) {
		if (l_now >= l_it->second.m_deadline) {
			send_control(transport::EnvelopeKind::AgentAbort, l_it->second.m_target, l_it->first, l_it->second.m_agent_id, false, "timeout");
			restore_agent(l_it->second, make_error(ErrorCategory::Transport, ErrorCode::Timeout,
												   "no answer from " + l_it->second.m_target + " for agent " + l_it->second.m_agent_id.str()));
			l_it = m_transits.erase(l_it);
		} else {
			++l_it;
		}
	}
	for (auto l_it = m_arrivals.begin(); l_it != m_arrivals.end();) {
		if (l_now >= l_it->second.m_deadline) {
			m_endpoint->count_migration_failed();
			l_it = m_arrivals.erase(l_it);
		} else {
			++l_it;
		}
	}
	const std::uint64_t l_grace = m_endpoint->config().m_forward_grace_turns;
	for (auto l_it = m_moved_agents.begin(); l_it != m_moved_agents.end();) {
		if (m_boundary - l_it->second.m_boundary > l_grace) {
			l_it = m_moved_agents.erase(l_it);
		} else {
			++l_it;
		}
	}
}

void cam::Environment::apply_agent_offer(const transport::Envelope& p_envelope) {
	const auto* l_transfer = std::get_if<transport::AgentTransfer>(&p_envelope.m_payload);
	if (!l_transfer) {
		m_endpoint->count_rejected();
		return;
	}
	const std::string& l_origin = p_envelope.m_sender_environment;
	// No agent identifier here is longer than AgentId::CAPACITY: such a transfer is refused (D76)
	const auto l_agent_id = AgentId::parse(l_transfer->m_agent_id);
	if (!l_agent_id) {
		send_control(transport::EnvelopeKind::AgentReject, l_origin, l_transfer->m_transfer_id, AgentId(), false,
					 "agent identifier longer than 15 characters");
		m_endpoint->count_migration_failed();
		return;
	}
	const auto l_factory = m_agent_factories.find(l_transfer->m_type_name);
	if (l_factory == m_agent_factories.end()) {
		send_control(transport::EnvelopeKind::AgentReject, l_origin, l_transfer->m_transfer_id, *l_agent_id, false,
					 "unknown agent type " + l_transfer->m_type_name);
		m_endpoint->count_migration_failed();
		return;
	}
	if (m_arrivals.contains(l_transfer->m_transfer_id)) {
		return;  // the same offer twice: already answered
	}
	if (m_agent_collection.contains(*l_agent_id)) {
		send_control(transport::EnvelopeKind::AgentReject, l_origin, l_transfer->m_transfer_id, *l_agent_id, false,
					 "agent already present");
		m_endpoint->count_migration_failed();
		return;
	}
	AgentPointer l_agent;
	try {
		l_agent = l_factory->second(l_transfer->m_agent_name);
		l_agent->set_environment(this);
		l_agent->set_identity(*l_agent_id, l_transfer->m_agent_name);
		l_agent->m_is_using_observables = l_transfer->m_uses_observables;
		if (!l_transfer->m_observables.empty()) {
			const json l_observables = json::from_msgpack(l_transfer->m_observables);
			for (const auto& [l_key, l_value]: l_observables.items()) {
				l_agent->m_observables->insert_or_assign(l_key, l_value);
			}
		}
		if (!l_transfer->m_state.empty()) {
			const auto l_state = transport::decode_content(l_transfer->m_state, l_transfer->m_state_encoding);
			const auto* l_json = l_state ? std::get_if<std::shared_ptr<const json>>(&l_state.value()) : nullptr;
			if (!l_json || !*l_json) {
				throw std::runtime_error(l_state ? "agent state is not a JSON value" : l_state.error().m_message);
			}
			l_agent->load_state(**l_json);
		}
	} catch (const std::exception& p_exception) {
		send_control(transport::EnvelopeKind::AgentReject, l_origin, l_transfer->m_transfer_id, *l_agent_id, false,
					 std::string("invalid agent state: ") + p_exception.what());
		m_endpoint->count_migration_failed();
		return;
	}
	for (const auto& l_queued: l_transfer->m_queue) {
		auto l_content = transport::decode_content(l_queued.m_content, l_queued.m_content_encoding);
		const auto l_sender = AgentId::parse(l_queued.m_sender_agent);
		if (!l_content || !l_sender) {
			m_endpoint->count_rejected();
			continue;
		}
		l_agent->receive(make_message(*l_sender, *l_agent_id, std::move(l_content.value()),
								   l_queued.effective_sender_environment()));
	}
	PendingArrival l_arrival;
	l_arrival.m_agent = l_agent;
	l_arrival.m_origin = l_origin;
	l_arrival.m_deadline = std::chrono::steady_clock::now() + m_endpoint->config().m_accept_hold_timeout;
	m_arrivals.emplace(l_transfer->m_transfer_id, std::move(l_arrival));
	send_control(transport::EnvelopeKind::AgentAccept, l_origin, l_transfer->m_transfer_id, *l_agent_id, true, "");
}

void cam::Environment::apply_agent_control(const transport::Envelope& p_envelope) {
	using transport::EnvelopeKind;
	const auto* l_control = std::get_if<transport::AgentControl>(&p_envelope.m_payload);
	if (!l_control) {
		m_endpoint->count_rejected();
		return;
	}
	switch (p_envelope.m_kind) {
		case EnvelopeKind::AgentAccept:
		case EnvelopeKind::AgentReject: {
			const auto l_it = m_transits.find(l_control->m_transfer_id);
			if (l_it == m_transits.end() || l_it->second.m_target != p_envelope.m_sender_environment) {
				m_endpoint->count_stale_control();
				return;
			}
			const Transit l_transit = std::move(l_it->second);
			m_transits.erase(l_it);
			if (p_envelope.m_kind == EnvelopeKind::AgentReject) {
				const bool l_unknown_type = l_control->m_reason.rfind("unknown agent type", 0) == 0;
				restore_agent(l_transit, make_error(ErrorCategory::Routing, l_unknown_type ? ErrorCode::UnknownAgentType : ErrorCode::InvalidTarget,
													"move refused by " + l_transit.m_target + ": " + l_control->m_reason));
				return;
			}
			send_control(EnvelopeKind::AgentCommit, l_transit.m_target, l_control->m_transfer_id, l_transit.m_agent_id, true, "");
			if (const AgentPointer l_agent = m_agent_collection.get(l_transit.m_agent_id); l_agent) {
				const auto l_held = l_agent->take_messages();
				m_agent_collection.remove(l_transit.m_agent_id);
				forward_messages(l_transit.m_agent_id, l_transit.m_target, l_held);
			}
			m_endpoint->set_agent_location(l_transit.m_agent_id.str(), l_transit.m_target);
			m_moved_agents[l_transit.m_agent_id] = MovedAgent{l_transit.m_target, m_boundary};
			m_endpoint->count_migration_completed();
			return;
		}
		case EnvelopeKind::AgentCommit:
		case EnvelopeKind::AgentAbort: {
			const auto l_it = m_arrivals.find(l_control->m_transfer_id);
			if (l_it == m_arrivals.end() || l_it->second.m_origin != p_envelope.m_sender_environment) {
				m_endpoint->count_stale_control();
				return;
			}
			const PendingArrival l_arrival = std::move(l_it->second);
			m_arrivals.erase(l_it);
			if (p_envelope.m_kind == EnvelopeKind::AgentAbort) {
				m_endpoint->count_migration_failed();
				return;
			}
			l_arrival.m_agent->reset();  // setup runs again on arrival (C#: MustRunSetup)
			m_agent_collection.insert(l_arrival.m_agent);
			m_moved_agents.erase(l_arrival.m_agent->get_id());
			m_endpoint->set_agent_location(l_arrival.m_agent->get_id().str(), m_id);
			transport::DirectoryEvent l_event;
			l_event.m_event = transport::DirectoryEventKind::AgentMoved;
			l_event.m_agent_id = l_arrival.m_agent->get_id().str();
			l_event.m_agent_name = l_arrival.m_agent->get_name();
			l_event.m_environment_id = m_id;
			l_event.m_previous_environment_id = l_arrival.m_origin;
			transport::Envelope l_envelope = m_endpoint->make_envelope(EnvelopeKind::Directory);
			l_envelope.m_payload = std::move(l_event);
			const auto l_presence_address = m_endpoint->address(transport::AddressKind::EnvPresence, m_id);
			if (const auto l_result = m_endpoint->enqueue(l_presence_address, std::move(l_envelope)); !l_result) {
				on_delivery_failure(l_result.error());
			}
			m_endpoint->count_migration_completed();
			return;
		}
		default:
			m_endpoint->count_rejected();
			return;
	}
}

