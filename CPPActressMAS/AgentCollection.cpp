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

int cam::AgentCollection::count() const {
	return m_agents.size();
}

void cam::AgentCollection::add(cam::AgentPointer& p_agent) {
	m_agents.emplace(p_agent->get_name(), p_agent);
}

const cam::AgentPointer& cam::AgentCollection::random_agent() const {
	auto l_it = m_agents.begin();
	std::advance(l_it, rand() % m_agents.size());
	return l_it->second;
}

bool cam::AgentCollection::contains(const AgentPointer& p_agent) const {
	return m_agents.find(p_agent->get_name()) != m_agents.end();
}

bool cam::AgentCollection::contains(const std::string& p_agent_name) const {
	return m_agents.find(p_agent_name) != m_agents.end();
}

void cam::AgentCollection::remove(const std::string& p_agent_name) {
	if (contains(p_agent_name)) {
		m_agents.erase(p_agent_name);
	}
}

cam::AgentPointer cam::AgentCollection::get(const std::string& p_name) {
	const auto& l_it = m_agents.find(p_name);
	if (l_it == m_agents.end()) {
		return cam::AgentPointer(nullptr);
	}
	return l_it->second;
}

const std::vector<std::string> cam::AgentCollection::get_names() const {
	std::vector<std::string> l_result;
	for (const auto& l_it : m_agents) {
		l_result.push_back(l_it.first);
	}
	return l_result;
}

const std::map<std::string, cam::AgentPointer>& cam::AgentCollection::get_agents() const {
	return m_agents;
}

const std::vector<cam::AgentPointer> cam::AgentCollection::filtered_agents(const std::string& p_name_fragment) {
	std::vector<cam::AgentPointer> return_value;
	for (const auto& p_agent : m_agents) {
		if (p_agent.first.find(p_name_fragment) != std::string::npos) {
			return_value.push_back(p_agent.second);
		}
	}
	return return_value;
}
