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

#include "AgentCollection.h"

#include <random>

size_t cam::AgentCollection::count() const { return m_agents.size(); }

void cam::AgentCollection::add(AgentPointer& p_agent) {
	std::unique_lock l_lock(m_mutex);
	m_agents.emplace(p_agent->get_id(), p_agent);
	l_lock.unlock();
	m_cond.notify_one();
}

const cam::AgentPointer& cam::AgentCollection::random_agent() {
	auto l_it = m_agents.begin();

	std::random_device l_rd;
	std::uniform_int_distribution<size_t> l_dist(0, m_agents.size());

	std::advance(l_it, l_dist(l_rd));
	return l_it->second;
}

bool cam::AgentCollection::contains(const AgentPointer& p_agent) const {
	return m_agents.contains(p_agent->get_id());
}

bool cam::AgentCollection::contains(const std::string& p_id) const {
	return m_agents.contains(p_id);
}

void cam::AgentCollection::remove(const std::string& p_id) {
	std::unique_lock l_lock(m_mutex);
	if (contains(p_id)) {
		m_agents.erase(p_id);
	}
	l_lock.unlock();
	m_cond.notify_one();
}

cam::AgentPointer cam::AgentCollection::get(const std::string& p_id) {
	std::unique_lock l_lock(m_mutex);
	const auto& l_it = m_agents.find(p_id);
	l_lock.unlock();
	m_cond.notify_one();

	if (l_it == m_agents.end()) {
		return {nullptr};
	}
	return l_it->second;
}

std::vector<std::string> cam::AgentCollection::get_ids() {
	std::vector<std::string> l_result;
	l_result.reserve(m_agents.size());

	for (auto& [l_id, _] : m_agents) {
		l_result.push_back(l_id);
	}

	return l_result;
}

const std::map<std::string, cam::AgentPointer>& cam::AgentCollection::get_agents() const {
	return m_agents;
}
