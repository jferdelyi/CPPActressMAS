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

#include "Endpoint.h"

#include <algorithm>
#include <random>

namespace cam::transport {

	namespace {

		/**
		 * Namespace of a detached endpoint (ns() returns a reference to it).
		 **/
		const std::string DEFAULT_NAMESPACE = "default";

		std::uint32_t random_epoch() {
			std::random_device l_device;
			std::uint32_t l_epoch = l_device();
			return l_epoch == 0 ? 1 : l_epoch;
		}

		int kind_rank(const EnvelopeKind p_kind) {
			switch (p_kind) {
				case EnvelopeKind::AgentOffer:
				case EnvelopeKind::AgentAccept:
				case EnvelopeKind::AgentReject:
				case EnvelopeKind::AgentCommit:
				case EnvelopeKind::AgentAbort:
					return 0;
				case EnvelopeKind::Presence:
				case EnvelopeKind::Directory:
					return 1;
				default:
					return 2;
			}
		}

	} // namespace

	Endpoint::Endpoint(std::string p_environment_id, EndpointConfig p_config) :
			m_environment_id(std::move(p_environment_id)),
			m_config(std::move(p_config)),
			m_epoch(random_epoch()) {
	}

	const std::string& Endpoint::ns() const {
		return m_transport ? m_transport->config().m_namespace : DEFAULT_NAMESPACE;
	}

	Address Endpoint::address(const AddressKind p_kind, const std::string& p_target) const {
		return Address{p_kind, ns(), p_target};
	}

	Envelope Endpoint::make_envelope(const EnvelopeKind p_kind) const {
		Envelope l_envelope;
		l_envelope.m_version_minor = ENVELOPE_VERSION_MINOR;
		l_envelope.m_kind = p_kind;
		l_envelope.m_sender_environment = m_environment_id;
		l_envelope.m_epoch = m_epoch;
		return l_envelope;
	}

	// ---------------------------------------------------------------- attachment

	Result<void> Endpoint::attach(std::shared_ptr<Transport> p_transport) {
		if (m_transport) {
			return make_error(ErrorCategory::State, ErrorCode::AlreadyConnected, "endpoint already attached");
		}
		if (!p_transport) {
			return make_error(ErrorCategory::State, ErrorCode::InvalidConfiguration, "null transport");
		}
		m_transport = std::move(p_transport);
		if (const auto l_result = m_transport->attach(*this); !l_result) {
			m_transport.reset();
			return l_result;
		}
		return {};
	}

	Result<void> Endpoint::detach() {
		if (!m_transport) {
			return make_error(ErrorCategory::State, ErrorCode::NotAttached, "endpoint not attached");
		}
		const auto l_result = m_transport->detach(*this);
		m_transport.reset();
		return l_result;
	}

	// ---------------------------------------------------------------- sending

	Result<void> Endpoint::enqueue(const Address& p_address, Envelope&& p_envelope) {
		const std::lock_guard<std::mutex> l_lock(m_outbox_mutex);
		if (!m_transport) {
			return make_error(ErrorCategory::State, ErrorCode::NotAttached, "endpoint not attached");
		}
		if (m_outbox.size() >= m_config.m_outbox_capacity) {
			++m_outbox_overflow;
			return make_error(ErrorCategory::Capacity, ErrorCode::OutboxFull, "outbox full");
		}
		p_envelope.m_sender_environment = m_environment_id;
		p_envelope.m_epoch = m_epoch;
		p_envelope.m_stream_kind = to_stream_kind(p_address.m_kind);
		p_envelope.m_stream_target = p_address.m_kind == AddressKind::AllIn ? std::string() : p_address.m_target;
		p_envelope.m_sequence = ++m_sequences[p_address.to_key()];
		m_outbox.push_back(Outgoing{p_address, std::move(p_envelope)});
		return {};
	}

	std::vector<Error> Endpoint::flush_outbox() {
		std::vector<Error> l_errors;
		if (!m_transport) {
			return l_errors;
		}
		const auto l_deadline = std::chrono::steady_clock::now() + m_config.m_flush_max_time;
		std::size_t l_count = 0;
		while (true) {
			Outgoing l_outgoing;
			{
				const std::lock_guard<std::mutex> l_lock(m_outbox_mutex);
				if (m_outbox.empty()) {
					break;
				}
				l_outgoing = std::move(m_outbox.front());
				m_outbox.pop_front();
			}
			if (const auto l_result = m_transport->publish(*this, l_outgoing.m_address, l_outgoing.m_envelope); !l_result) {
				l_errors.push_back(l_result.error());
				++m_statistics.m_lost;
			} else {
				++m_statistics.m_sent;
			}
			++l_count;
			if (m_config.m_flush_max_messages != 0 && l_count >= m_config.m_flush_max_messages) {
				break;
			}
			if (std::chrono::steady_clock::now() >= l_deadline) {
				break;
			}
		}
		return l_errors;
	}

	// ---------------------------------------------------------------- receiving

	bool Endpoint::deliver(const Address& p_address, Envelope&& p_envelope) {
		const std::lock_guard<std::mutex> l_lock(m_inbox_mutex);
		if (m_inbox.size() >= m_config.m_inbox_capacity) {
			++m_inbox_overflow;
			return false;
		}
		++m_received;
		m_inbox.push_back(Incoming{p_address, {}, std::move(p_envelope)});
		return true;
	}

	bool Endpoint::deliver(const Address& p_address, std::vector<std::uint8_t>&& p_bytes) {
		const std::lock_guard<std::mutex> l_lock(m_inbox_mutex);
		if (m_inbox.size() >= m_config.m_inbox_capacity) {
			++m_inbox_overflow;
			return false;
		}
		++m_received;
		m_inbox.push_back(Incoming{p_address, std::move(p_bytes), std::nullopt});
		return true;
	}

	void Endpoint::push_event(const TransportEvent p_event) {
		const std::lock_guard<std::mutex> l_lock(m_inbox_mutex);
		m_events.push_back(p_event);
	}

	std::vector<TransportEvent> Endpoint::take_events() {
		const std::lock_guard<std::mutex> l_lock(m_inbox_mutex);
		std::vector<TransportEvent> l_events;
		l_events.swap(m_events);
		return l_events;
	}

	std::string Endpoint::flow_key(const Envelope& p_envelope) {
		std::string l_key = p_envelope.m_sender_environment;
		l_key += '|';
		l_key += std::to_string(p_envelope.m_epoch);
		l_key += '|';
		l_key += std::to_string(static_cast<int>(p_envelope.m_stream_kind));
		l_key += '|';
		l_key += p_envelope.m_stream_target;
		return l_key;
	}

	void Endpoint::remember(FlowState& p_flow, const std::uint64_t p_sequence) {
		p_flow.m_recent.push_back(p_sequence);
		p_flow.m_recent_set.insert(p_sequence);
		while (p_flow.m_recent.size() > m_config.m_dedup_window) {
			p_flow.m_recent_set.erase(p_flow.m_recent.front());
			p_flow.m_recent.pop_front();
		}
	}

	void Endpoint::release_gap(FlowState& p_flow, std::vector<Envelope>& p_out) {
		// Give up on the missing sequences: count them lost, deliver what waited behind them.
		const std::uint64_t l_first = p_flow.m_pending.begin()->first;
		m_statistics.m_lost += l_first - p_flow.m_next_expected;
		m_statistics.m_out_of_order += p_flow.m_pending.size();
		p_flow.m_next_expected = l_first;
		deliver_ready(p_flow, p_out);
	}

	void Endpoint::deliver_ready(FlowState& p_flow, std::vector<Envelope>& p_out) {
		while (!p_flow.m_pending.empty() && p_flow.m_pending.begin()->first == p_flow.m_next_expected) {
			auto l_node = p_flow.m_pending.extract(p_flow.m_pending.begin());
			remember(p_flow, l_node.key());
			p_out.push_back(std::move(l_node.mapped()));
			++p_flow.m_next_expected;
		}
		p_flow.m_has_gap = !p_flow.m_pending.empty();
		if (p_flow.m_has_gap && p_flow.m_gap_since_turn == 0) {
			p_flow.m_gap_since_turn = m_turn;
		}
		if (!p_flow.m_has_gap) {
			p_flow.m_gap_since_turn = 0;
		}
	}

	std::vector<Envelope> Endpoint::drain() {
		++m_turn;
		std::deque<Incoming> l_incoming;
		{
			const std::lock_guard<std::mutex> l_lock(m_inbox_mutex);
			l_incoming.swap(m_inbox);
		}
		std::vector<Envelope> l_ready;
		const std::size_t l_max_bytes = m_transport ? m_transport->config().m_max_envelope_bytes : (1u << 20);
		const auto l_codec = m_transport ? m_transport->codec() : nullptr;

		for (auto& l_item: l_incoming) {
			Envelope l_envelope;
			if (l_item.m_envelope) {
				l_envelope = std::move(*l_item.m_envelope);
				if (const auto l_valid = validate_envelope(l_envelope, l_max_bytes); !l_valid) {
					++m_statistics.m_rejected;
					continue;
				}
			} else {
				if (!l_codec) {
					++m_statistics.m_rejected;
					continue;
				}
				auto l_decoded = l_codec->decode(l_item.m_bytes, l_max_bytes);
				if (!l_decoded) {
					++m_statistics.m_rejected;
					continue;
				}
				l_envelope = std::move(l_decoded.value());
			}
			if (l_envelope.m_sender_environment == m_environment_id) {
				++m_statistics.m_echo;
				continue;
			}
			FlowState& l_flow = m_flows[flow_key(l_envelope)];
			const std::uint64_t l_sequence = l_envelope.m_sequence;
			if (!l_flow.m_started) {
				// A flow starts at the lowest sequence seen in its first drain: a batch received in
				// reverse order at the start is still delivered in order.
				if (!l_flow.m_pending.emplace(l_sequence, std::move(l_envelope)).second) {
					++m_statistics.m_duplicates;
				}
				continue;
			}
			if (l_sequence < l_flow.m_next_expected) {
				if (l_flow.m_recent_set.contains(l_sequence)) {
					++m_statistics.m_duplicates;
				} else {
					++m_statistics.m_out_of_order;
					remember(l_flow, l_sequence);
					l_ready.push_back(std::move(l_envelope));
				}
				continue;
			}
			if (l_flow.m_pending.contains(l_sequence)) {
				++m_statistics.m_duplicates;
				continue;
			}
			l_flow.m_pending.emplace(l_sequence, std::move(l_envelope));
			deliver_ready(l_flow, l_ready);
			if (l_flow.m_pending.size() > m_config.m_reorder_capacity) {
				release_gap(l_flow, l_ready);
			}
		}

		for (auto& [l_key, l_flow]: m_flows) {
			if (!l_flow.m_started) {
				if (l_flow.m_pending.empty()) {
					continue;
				}
				l_flow.m_started = true;
				l_flow.m_next_expected = l_flow.m_pending.begin()->first;
				deliver_ready(l_flow, l_ready);
				if (l_flow.m_pending.size() > m_config.m_reorder_capacity) {
					release_gap(l_flow, l_ready);
				}
			}
			if (l_flow.m_has_gap && m_turn - l_flow.m_gap_since_turn >= m_config.m_reorder_turns) {
				release_gap(l_flow, l_ready);
			}
		}

		std::stable_sort(l_ready.begin(), l_ready.end(), [](const Envelope& p_left, const Envelope& p_right) {
			return kind_rank(p_left.m_kind) < kind_rank(p_right.m_kind);
		});
		m_statistics.m_delivered += l_ready.size();
		return l_ready;
	}

	// ---------------------------------------------------------------- directory and presence

	std::optional<std::string> Endpoint::locate_agent(const std::string& p_agent_id) const {
		const auto l_it = m_directory.find(p_agent_id);
		if (l_it == m_directory.end()) {
			return std::nullopt;
		}
		return l_it->second;
	}

	void Endpoint::set_agent_location(const std::string& p_agent_id, const std::string& p_environment_id) {
		m_directory[p_agent_id] = p_environment_id;
	}

	void Endpoint::remove_agent(const std::string& p_agent_id) {
		m_directory.erase(p_agent_id);
	}

	void Endpoint::purge_environment(const std::string& p_environment_id) {
		for (auto l_it = m_directory.begin(); l_it != m_directory.end();) {
			if (l_it->second == p_environment_id) {
				l_it = m_directory.erase(l_it);
			} else {
				++l_it;
			}
		}
		if (const auto l_env = m_environments.find(p_environment_id); l_env != m_environments.end()) {
			l_env->second.m_online = false;
		}
	}

	std::vector<EnvironmentInfo> Endpoint::purge_node(const std::string& p_node_id) {
		std::vector<EnvironmentInfo> l_lost;
		for (auto& [l_id, l_info]: m_environments) {
			if (l_info.m_node == p_node_id && l_info.m_online) {
				l_lost.push_back(l_info);
			}
		}
		for (const auto& l_info: l_lost) {
			purge_environment(l_info.m_id);
		}
		return l_lost;
	}

	void Endpoint::update_environment(const Presence& p_presence) {
		EnvironmentInfo& l_info = m_environments[p_presence.m_environment_id];
		l_info.m_id = p_presence.m_environment_id;
		l_info.m_name = p_presence.m_environment_name;
		l_info.m_node = p_presence.m_node_id;
		l_info.m_online = p_presence.m_online;
		l_info.m_last_seen = std::chrono::steady_clock::now();
		if (p_presence.m_online) {
			for (const auto& [l_agent_id, l_agent_name]: p_presence.m_agents) {
				m_directory[l_agent_id] = p_presence.m_environment_id;
			}
		} else {
			purge_environment(p_presence.m_environment_id);
		}
	}

	std::vector<EnvironmentInfo> Endpoint::environments() const {
		std::vector<EnvironmentInfo> l_result;
		l_result.reserve(m_environments.size());
		for (const auto& [l_id, l_info]: m_environments) {
			l_result.push_back(l_info);
		}
		return l_result;
	}

	bool Endpoint::knows_environment(const std::string& p_environment_id) const {
		const auto l_it = m_environments.find(p_environment_id);
		return l_it != m_environments.end() && l_it->second.m_online;
	}

	std::optional<std::string> Endpoint::previous_host(const std::string& p_agent_id) const {
		const auto l_it = m_moved_from.find(p_agent_id);
		if (l_it == m_moved_from.end()) {
			return std::nullopt;
		}
		return l_it->second;
	}

	void Endpoint::set_previous_host(const std::string& p_agent_id, const std::string& p_environment_id) {
		m_moved_from[p_agent_id] = p_environment_id;
	}

	Statistics Endpoint::statistics() const {
		Statistics l_statistics = m_statistics;
		l_statistics.m_inbox_overflow = m_inbox_overflow.load();
		l_statistics.m_received = m_received.load();
		l_statistics.m_outbox_overflow = m_outbox_overflow.load();
		l_statistics.m_unknown_destination = m_unknown_destination.load();
		return l_statistics;
	}

	// ---------------------------------------------------------------- registry (Transport.h)

	bool EndpointRegistry::add(Endpoint& p_endpoint) {
		const std::unique_lock<std::shared_mutex> l_lock(m_mutex);
		if (std::find(m_endpoints.begin(), m_endpoints.end(), &p_endpoint) != m_endpoints.end()) {
			return false;
		}
		m_endpoints.push_back(&p_endpoint);
		return true;
	}

	bool EndpointRegistry::remove(Endpoint& p_endpoint) {
		const std::unique_lock<std::shared_mutex> l_lock(m_mutex);  // waits for the deliveries in flight
		const auto l_it = std::find(m_endpoints.begin(), m_endpoints.end(), &p_endpoint);
		if (l_it == m_endpoints.end()) {
			return false;
		}
		m_endpoints.erase(l_it);
		return true;
	}

	Endpoint* EndpointRegistry::find_unlocked(const std::string& p_environment_id) const {
		for (Endpoint* l_endpoint: m_endpoints) {
			if (l_endpoint->environment_id() == p_environment_id) {
				return l_endpoint;
			}
		}
		return nullptr;
	}

	bool EndpointRegistry::is_local(const std::string& p_environment_id) const {
		const std::shared_lock<std::shared_mutex> l_lock(m_mutex);
		return find_unlocked(p_environment_id) != nullptr;
	}

	EndpointRegistry::LocalDelivery EndpointRegistry::deliver_locally(const Endpoint* p_sender, const Address& p_address,
																	  const Envelope& p_envelope) const {
		LocalDelivery l_delivery;
		const std::shared_lock<std::shared_mutex> l_lock(m_mutex);
		switch (p_address.m_kind) {
			case AddressKind::EnvIn:
			case AddressKind::EnvAgent:
			case AddressKind::EnvQuery: {
				if (Endpoint* l_target = find_unlocked(p_address.m_target); l_target) {
					l_delivery.m_target_is_local = true;
					if (l_target != p_sender) {
						Envelope l_copy = p_envelope;
						l_delivery.m_delivered_locally = l_target->deliver(p_address, std::move(l_copy));
					}
				} else {
					l_delivery.m_needs_network = true;
				}
				break;
			}
			case AddressKind::AllIn:
			case AddressKind::EnvPresence: {
				for (Endpoint* l_endpoint: m_endpoints) {
					if (l_endpoint == p_sender) {
						continue;
					}
					Envelope l_copy = p_envelope;
					if (l_endpoint->deliver(p_address, std::move(l_copy))) {
						l_delivery.m_delivered_locally = true;
					}
				}
				l_delivery.m_needs_network = true;
				break;
			}
			case AddressKind::NodePresence:
			default:
				l_delivery.m_needs_network = true;
				break;
		}
		return l_delivery;
	}

} // namespace cam::transport

