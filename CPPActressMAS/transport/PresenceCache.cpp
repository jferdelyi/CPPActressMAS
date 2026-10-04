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

#include "PresenceCache.h"

#include <algorithm>

namespace cam::transport {

	namespace {

		void remove_agent(Presence& p_presence, const std::string& p_agent_id) {
			p_presence.m_agents.erase(std::remove_if(p_presence.m_agents.begin(), p_presence.m_agents.end(),
													  [&](const auto& p_agent) { return p_agent.first == p_agent_id; }),
									  p_presence.m_agents.end());
		}

	} // namespace

	void PresenceCache::remember(const Address& p_address, const Envelope& p_envelope) {
		if (p_address.m_kind != AddressKind::EnvPresence) {
			return;
		}
		const std::lock_guard<std::mutex> l_lock(m_mutex);
		if (const auto* l_presence = std::get_if<Presence>(&p_envelope.m_payload); l_presence && p_envelope.m_kind == EnvelopeKind::Presence) {
			if (l_presence->m_online) {
				m_entries[p_address.m_target] = p_envelope;
			} else {
				m_entries.erase(p_address.m_target);
			}
			return;
		}
		const auto* l_event = std::get_if<DirectoryEvent>(&p_envelope.m_payload);
		if (!l_event || p_envelope.m_kind != EnvelopeKind::Directory) {
			return;
		}
		// Keep the agent lists current: the presence of an environment is what a newcomer will get.
		if (l_event->m_event == DirectoryEventKind::AgentMoved && !l_event->m_previous_environment_id.empty()) {
			if (const auto l_previous = m_entries.find(l_event->m_previous_environment_id); l_previous != m_entries.end()) {
				if (auto* l_presence = std::get_if<Presence>(&l_previous->second.m_payload)) {
					remove_agent(*l_presence, l_event->m_agent_id);
				}
			}
		}
		const auto l_entry = m_entries.find(l_event->m_environment_id);
		if (l_entry == m_entries.end()) {
			return;
		}
		auto* l_presence = std::get_if<Presence>(&l_entry->second.m_payload);
		if (!l_presence) {
			return;
		}
		remove_agent(*l_presence, l_event->m_agent_id);
		if (l_event->m_event != DirectoryEventKind::AgentRemoved) {
			l_presence->m_agents.emplace_back(l_event->m_agent_id, l_event->m_agent_name);
		}
	}

	void PresenceCache::forget(const std::string& p_environment_id) {
		const std::lock_guard<std::mutex> l_lock(m_mutex);
		m_entries.erase(p_environment_id);
	}

	std::optional<Envelope> PresenceCache::find(const std::string& p_environment_id) const {
		const std::lock_guard<std::mutex> l_lock(m_mutex);
		const auto l_it = m_entries.find(p_environment_id);
		if (l_it == m_entries.end()) {
			return std::nullopt;
		}
		return l_it->second;
	}

	void PresenceCache::replay(Endpoint& p_endpoint) const {
		std::vector<std::pair<std::string, Envelope>> l_entries;
		{
			const std::lock_guard<std::mutex> l_lock(m_mutex);
			for (const auto& [l_environment, l_envelope]: m_entries) {
				if (l_environment != p_endpoint.environment_id()) {
					l_entries.emplace_back(l_environment, l_envelope);
				}
			}
		}
		for (auto& [l_environment, l_envelope]: l_entries) {
			p_endpoint.deliver(Address{AddressKind::EnvPresence, p_endpoint.ns(), l_environment}, std::move(l_envelope));
		}
	}

} // namespace cam::transport
