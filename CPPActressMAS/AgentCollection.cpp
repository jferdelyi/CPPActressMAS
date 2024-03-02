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

#include <future>
#include <random>

#include "EnvironmentMas.h"

cam::AgentCollection::AgentCollection(const EnvironmentMasMode& p_environment_mas_mode) :
	m_pool(std::thread::hardware_concurrency() == 0 ? 8 : std::thread::hardware_concurrency()),
	m_environment_mas_mode(p_environment_mas_mode) {
}

size_t cam::AgentCollection::count() const {
	return m_agents.size();
}

void cam::AgentCollection::add(const AgentPointer& p_agent) {
	m_new_agents.enqueue(p_agent);
}

const cam::AgentPointer& cam::AgentCollection::random_agent() const {
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
	if (contains(p_id)) {
		m_agents.erase(p_id);
	}
}

void cam::AgentCollection::run_turn() {
	// Sequential
	if (m_environment_mas_mode == EnvironmentMasMode::Sequential) {
		for (auto& [l_agent_id, l_agent] : m_agents) {
			if (!l_agent->is_setup()) {
				l_agent->internal_setup();
			} else {
				if (l_agent->is_using_observables()) {
					l_agent->internal_see();
				}
				l_agent->internal_action();
			}
		}

	// Random
	} else {
		const auto m_parallel = m_environment_mas_mode == EnvironmentMasMode::Parallel;
		std::vector<std::future<void>> l_asyncs;
		const std::vector<std::string> l_agents = get_ids();
		const auto l_count = l_agents.size();
		const std::vector<int> l_agent_order = random_permutation(l_count);
		size_t l_index = 0;

		while (l_index < l_count) {
			const std::string& l_agent_id = l_agents.at(l_agent_order.at(l_index++));
			if(AgentPointer l_agent = get(l_agent_id); l_agent) {
				if (!l_agent->is_setup()) {
					if (m_parallel) {
						//l_asyncs.push_back(std::async(std::launch::async, [l_agent] {
						//	l_agent->internal_setup();
						//}));
						l_asyncs.push_back(m_pool.addWorkFunc([l_agent] {
							l_agent->internal_setup();
						}));
					} else {
						l_agent->internal_setup();
					}
				} else {
					if (m_parallel) {
						//l_asyncs.push_back(std::async(std::launch::async, [l_agent] {
						//	if (l_agent->is_using_observables()) {
						//		l_agent->internal_see();
						//	}
						//	l_agent->internal_action();
						//}));
						l_asyncs.push_back(m_pool.addWorkFunc([l_agent] {
							if (l_agent->is_using_observables()) {
								l_agent->internal_see();
							}
							l_agent->internal_action();
						}));
					} else {
						if (l_agent->is_using_observables()) {
							l_agent->internal_see();
						}
						l_agent->internal_action();
					}
				}
			}
		}
		for (std::future<void>& l_async : l_asyncs) {
			l_async.wait();
		}
	}
}

void cam::AgentCollection::process_buffers() {
	// Add new agents
	if (AgentPointer l_agent; m_new_agents.dequeue(l_agent)) {
		do {
			m_agents.insert(std::make_pair(l_agent->get_id(), l_agent));
			m_agents_by_name.insert(std::make_pair(l_agent->get_name(), l_agent->get_id()));
		} while (m_new_agents.dequeue(l_agent));
	}

	// Remove dead agents
	std::vector<std::string> l_ids;
	l_ids.reserve(m_agents.size());
	for (auto& [l_id, l_agent] : m_agents) {
		if (l_agent->is_dead()) {
			l_ids.push_back(l_id);
			const auto [l_start, l_end] = m_agents_by_name.equal_range(l_agent->get_name());
			for (auto l_agent_it = l_start; l_agent_it != l_end; ++l_agent_it) {
				if (l_agent_it->second == l_id) {
					m_agents_by_name.erase(l_agent_it);
					break;
				}
			}
		}
	}
	for (const auto& l_id : l_ids) {
		remove(l_id);
	}
}

cam::AgentPointer cam::AgentCollection::get(const std::string& p_id) const{
	const auto& l_it = m_agents.find(p_id);
	return l_it == m_agents.end() ? nullptr : l_it->second;
}

std::vector<std::string> cam::AgentCollection::get_ids(const bool p_alive_only) {
	std::vector<std::string> l_result;
	l_result.reserve(m_agents.size());

	for (auto& [l_id, l_agent] : m_agents) {
		if (!p_alive_only || !l_agent->is_dead()) {
			l_result.push_back(l_id);
		}
	}

	return l_result;
}

std::vector<int> cam::AgentCollection::random_permutation(const size_t p_number) {
	std::vector<int> l_numbers(p_number);
	for (size_t l_index = 0; l_index < p_number; l_index++) {
		l_numbers[l_index] = static_cast<int>(l_index);
	}

	std::random_device l_rd;
	int l_index = static_cast<int>(p_number) - 1;
	while (l_index > 1) {
		std::uniform_int_distribution l_dist(0,  l_index);
		const int l_k = l_dist(l_rd);
		if (l_k == l_index) {
			continue;
		}
		const int l_temp = l_numbers[l_index];
		l_numbers[l_index] = l_numbers[l_k];
		l_numbers[l_k] = l_temp;
		l_index--;
	}

	return l_numbers;
}
