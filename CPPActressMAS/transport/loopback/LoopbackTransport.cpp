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

#include "LoopbackTransport.h"

#include "../../util/Identifier.h"
#include "../codec/ProtobufCodec.h"

namespace cam::transport {

	LoopbackTransport::LoopbackTransport(TransportConfig p_config, LoopbackOptions p_options, std::shared_ptr<const EnvelopeCodec> p_codec) :
			m_config(std::move(p_config)),
			m_options(std::move(p_options)),
			m_codec(p_codec ? std::move(p_codec) : std::make_shared<ProtobufCodec>()),
			m_node_id(cam::generate_uuid()) {
	}

	Result<void> LoopbackTransport::open() {
		if (!is_valid_namespace(m_config.m_namespace)) {
			return make_error(ErrorCategory::State, ErrorCode::InvalidConfiguration, "invalid namespace");
		}
		m_open = true;
		return {};
	}

	Result<void> LoopbackTransport::close(const std::chrono::milliseconds) {
		m_open = false;
		return {};
	}

	Result<void> LoopbackTransport::attach(Endpoint& p_endpoint) {
		if (!m_open) {
			return make_error(ErrorCategory::State, ErrorCode::NotConnected, "loopback transport not open");
		}
		if (!m_registry.add(p_endpoint)) {
			return make_error(ErrorCategory::State, ErrorCode::AlreadyConnected, "endpoint already attached");
		}
		// Retained presence: the newcomer learns the environments already here.
		std::vector<std::pair<Address, Envelope>> l_retained;
		{
			const std::lock_guard<std::mutex> l_lock(m_mutex);
			for (const auto& [l_environment, l_presence]: m_retained_presence) {
				l_retained.emplace_back(Address{AddressKind::EnvPresence, m_config.m_namespace, l_environment}, l_presence);
			}
		}
		for (auto& [l_address, l_presence]: l_retained) {
			if (l_presence.m_sender_environment != p_endpoint.environment_id()) {
				p_endpoint.deliver(l_address, std::move(l_presence));
			}
		}
		return {};
	}

	Result<void> LoopbackTransport::detach(Endpoint& p_endpoint) {
		if (!m_registry.remove(p_endpoint)) {
			return make_error(ErrorCategory::State, ErrorCode::NotAttached, "endpoint not attached");
		}
		const std::lock_guard<std::mutex> l_lock(m_mutex);
		m_retained_presence.erase(p_endpoint.environment_id());
		return {};
	}

	void LoopbackTransport::distribute(const Endpoint* p_sender, const Address& p_address, const Envelope& p_envelope) {
		if (!m_options.m_serialize) {
			m_registry.deliver_locally(p_sender, p_address, p_envelope);
			return;
		}
		const auto l_bytes = m_codec->encode(p_envelope);
		if (!l_bytes) {
			return;
		}
		switch (p_address.m_kind) {
			case AddressKind::EnvIn:
			case AddressKind::EnvAgent:
			case AddressKind::EnvQuery:
				m_registry.with(p_address.m_target, [&](Endpoint& p_target) {
					if (&p_target != p_sender) {
						std::vector<std::uint8_t> l_copy = l_bytes.value();
						p_target.deliver(p_address, std::move(l_copy));
					}
				});
				break;
			case AddressKind::AllIn:
			case AddressKind::EnvPresence:
				m_registry.for_each(p_sender, [&](Endpoint& p_endpoint) {
					std::vector<std::uint8_t> l_copy = l_bytes.value();
					p_endpoint.deliver(p_address, std::move(l_copy));
				});
				break;
			case AddressKind::NodePresence:
			default:
				break;
		}
	}

	Result<void> LoopbackTransport::publish(const Endpoint& p_sender, const Address& p_address, const Envelope& p_envelope) {
		if (!m_open) {
			return make_error(ErrorCategory::State, ErrorCode::NotConnected, "loopback transport not open");
		}
		if (p_address.m_namespace != m_config.m_namespace) {
			return make_error(ErrorCategory::Routing, ErrorCode::InvalidTarget, "address outside the transport namespace");
		}
		std::vector<std::pair<Address, Envelope>> l_to_deliver;
		{
			const std::lock_guard<std::mutex> l_lock(m_mutex);
			++m_published;
			if (p_envelope.m_kind == EnvelopeKind::Presence && p_address.m_kind == AddressKind::EnvPresence) {
				if (const auto* l_presence = std::get_if<Presence>(&p_envelope.m_payload); l_presence) {
					if (l_presence->m_online) {
						m_retained_presence[p_address.m_target] = p_envelope;
					} else {
						m_retained_presence.erase(p_address.m_target);
					}
				}
			}
			if (m_options.m_drop_if && m_options.m_drop_if(p_envelope)) {
				++m_dropped;
				return {};
			}
			if (m_options.m_drop_every_n != 0 && m_published % m_options.m_drop_every_n == 0) {
				++m_dropped;
				return {};
			}
			if (m_options.m_hold_if && m_options.m_hold_if(p_envelope)) {
				m_held.emplace_back(p_address, p_envelope);
				return {};
			}
			const std::size_t l_copies = (m_options.m_duplicate_every_n != 0 && m_published % m_options.m_duplicate_every_n == 0) ? 2 : 1;
			if (m_options.m_reorder_window != 0) {
				for (std::size_t i = 0; i < l_copies; i++) {
					m_reorder_buffer.emplace_back(p_address, p_envelope);
				}
				if (m_reorder_buffer.size() >= m_options.m_reorder_window) {
					for (auto l_it = m_reorder_buffer.rbegin(); l_it != m_reorder_buffer.rend(); ++l_it) {
						l_to_deliver.push_back(std::move(*l_it));
					}
					m_reorder_buffer.clear();
				}
			} else {
				for (std::size_t i = 0; i < l_copies; i++) {
					l_to_deliver.emplace_back(p_address, p_envelope);
				}
			}
		}
		for (const auto& [l_address, l_envelope]: l_to_deliver) {
			distribute(&p_sender, l_address, l_envelope);
		}
		return {};
	}

	std::size_t LoopbackTransport::release_held() {
		std::vector<std::pair<Address, Envelope>> l_held;
		{
			const std::lock_guard<std::mutex> l_lock(m_mutex);
			l_held.swap(m_held);
		}
		for (const auto& [l_address, l_envelope]: l_held) {
			distribute(nullptr, l_address, l_envelope);
		}
		return l_held.size();
	}

	Capabilities LoopbackTransport::capabilities() const {
		Capabilities l_capabilities;
		l_capabilities.m_pubsub = true;
		l_capabilities.m_request_reply = false;
		l_capabilities.m_liveliness = false;
		l_capabilities.m_retained_presence = true;
		l_capabilities.m_ordered_per_sender = m_options.m_reorder_window == 0;
		l_capabilities.m_at_least_once = m_options.m_drop_every_n == 0 && !m_options.m_drop_if;
		l_capabilities.m_thread_safe_publish = true;
		return l_capabilities;
	}

	std::size_t LoopbackTransport::published() const {
		const std::lock_guard<std::mutex> l_lock(m_mutex);
		return m_published;
	}

	std::size_t LoopbackTransport::dropped() const {
		const std::lock_guard<std::mutex> l_lock(m_mutex);
		return m_dropped;
	}

} // namespace cam::transport
