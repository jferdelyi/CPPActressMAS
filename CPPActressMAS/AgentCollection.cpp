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

#include "Environment.h"

namespace {

	// Worker threads of a Parallel environment: one per core but the calling thread, which works too
	std::size_t worker_count() {
		const unsigned int l_cores = std::thread::hardware_concurrency();
		return l_cores == 0 ? 7 : l_cores - 1;
	}

	// Marks a turn as running for its lifetime, exceptions included (the Sequential modes let them through)
	struct TurnGuard final {
		std::atomic<bool>& m_running;
		explicit TurnGuard(std::atomic<bool>& p_running) : m_running(p_running) { m_running.store(true, std::memory_order_relaxed); }
		~TurnGuard() { m_running.store(false, std::memory_order_relaxed); }
		TurnGuard(const TurnGuard&) = delete;
		TurnGuard& operator=(const TurnGuard&) = delete;
	};

} // namespace

cam::AgentCollection::AgentCollection(const EnvironmentMasMode& p_environment_mas_mode, const unsigned int p_seed) :
		m_turn_pool(p_environment_mas_mode == EnvironmentMasMode::Parallel ? std::make_unique<TurnPool>(worker_count()) : nullptr),
		m_environment_mas_mode(p_environment_mas_mode),
		m_seed(p_seed) {

}

size_t cam::AgentCollection::count() const {
	return m_agents.size() - m_retired.load(std::memory_order_relaxed);
}

void cam::AgentCollection::add(const AgentPointer& p_agent) {
	m_new_agents.enqueue(p_agent);
}

void cam::AgentCollection::insert(const AgentPointer& p_agent) {
	m_agents.insert(std::make_pair(p_agent->get_id(), p_agent));
	m_agents_by_name.insert(std::make_pair(p_agent->get_name(), p_agent->get_id()));
	if (p_agent->is_dead()) {
		m_retired.fetch_add(1, std::memory_order_relaxed);
	}
}

const cam::AgentPointer& cam::AgentCollection::random_agent() const {
	auto l_it = m_agents.begin();
	do {

		std::advance(l_it, get_random_value(0, m_agents.size()));
	} while (l_it->second->is_dead());
	return l_it->second;
}

bool cam::AgentCollection::contains(const AgentPointer& p_agent) const {
	return m_agents.contains(p_agent->get_id());
}

bool cam::AgentCollection::contains(const AgentId& p_id) const {
	return m_agents.contains(p_id);
}

void cam::AgentCollection::remove(const AgentId& p_id) {
	const auto l_it = m_agents.find(p_id);
	if (l_it == m_agents.end()) {
		return;
	}
	// Drop the (name, id) entry of the name index as well, otherwise a later send_by_name()
	// dereferences a null pointer (C6).
	remove_name(l_it->second->get_name(), p_id);
	if (l_it->second->is_dead()) {
		m_retired.fetch_sub(1, std::memory_order_relaxed);
	}
	m_agents.erase(l_it);
}

void cam::AgentCollection::remove_name(const std::string& p_name, const AgentId& p_id) {
	const auto [l_start, l_end] = m_agents_by_name.equal_range(p_name);
	for (auto l_name_it = l_start; l_name_it != l_end; ++l_name_it) {
		if (l_name_it->second == p_id) {
			m_agents_by_name.erase(l_name_it);
			return;
		}
	}
}

void cam::AgentCollection::run_turn() {
	// The collections do not change during the turn: a removal marks the agent and the boundary erases it (C10, C12)
	const TurnGuard l_guard(m_turn_running);

	// Sequential: an agent removed earlier in the turn is skipped by run_turn(), as the C# (ContainsKey)
	if (m_environment_mas_mode == EnvironmentMasMode::Sequential) {
		for (auto& [l_agent_id, l_agent]: m_agents) {
			l_agent->run_turn();
		}

		// Parallel: every agent alive at the start of the turn at once, without copy nor shuffle, as the C# does
		// (Parallel.Invoke, P2); an agent of this snapshot runs even if it is removed meanwhile (ISO s)
	} else if (m_environment_mas_mode == EnvironmentMasMode::Parallel) {
		m_turn_agents.clear();
		for (auto& [l_agent_id, l_agent]: m_agents) {
			if (!l_agent->is_dead()) {
				m_turn_agents.push_back(l_agent.get());
			}
		}
		m_turn_pool->run(m_turn_agents);

		// Sequential, in a random order drawn each turn
	} else {
		// Ids and order kept in members: no large allocation per turn. An agent removed during the turn is skipped by
		// run_turn(), as the C# (ContainsKey).
		m_turn_ids.clear();
		for (auto& [l_id, l_agent]: m_agents) {
			if (!l_agent->is_dead()) {
				m_turn_ids.push_back(l_id);
			}
		}
		const auto l_count = m_turn_ids.size();
		random_permutation(l_count, m_turn_order);
		for (size_t l_index = 0; l_index < l_count; l_index++) {
			if (AgentPointer l_agent = get(m_turn_ids[static_cast<size_t>(m_turn_order[l_index])]); l_agent) {
				l_agent->run_turn();
			}
		}
	}
}

cam::AgentCollection::BufferChanges cam::AgentCollection::process_buffers(const bool p_track_changes) {
	BufferChanges l_changes;

	// Remove dead agents in one pass that keeps the insertion order (P9: an erase per agent shifted the rest each time)
	size_t l_dead = 0;
	for (auto& [l_id, l_agent]: m_agents) {
		if (l_agent->is_dead()) {
			++l_dead;
			remove_name(l_agent->get_name(), l_id);
			if (p_track_changes) {
				l_changes.m_removed.emplace_back(l_id, l_agent->get_name());
			}
		}
	}
	if (l_dead != 0) {
		erase_if(m_agents, [](const std::pair<AgentId, AgentPointer>& p_entry) { return p_entry.second->is_dead(); });
		m_retired.fetch_sub(l_dead, std::memory_order_relaxed);
	}

	// Add new agents
	if (AgentPointer l_agent; m_new_agents.dequeue(l_agent)) {
		do {
			m_agents.insert(std::make_pair(l_agent->get_id(), l_agent));
			m_agents_by_name.insert(std::make_pair(l_agent->get_name(), l_agent->get_id()));
			if (l_agent->is_dead()) {
				m_retired.fetch_add(1, std::memory_order_relaxed);  // stopped before its insertion (C11): never counted
			}
			if (p_track_changes) {
				l_changes.m_added.emplace_back(l_agent->get_id(), l_agent->get_name());
			}
		} while (m_new_agents.dequeue(l_agent));
	}
	return l_changes;
}

cam::AgentPointer cam::AgentCollection::get(const AgentId& p_id) const {
	const auto& l_it = m_agents.find(p_id);
	return l_it == m_agents.end() ? nullptr : l_it->second;
}

cam::Agent* cam::AgentCollection::find(const AgentId& p_id) const {
	const auto& l_it = m_agents.find(p_id);
	return l_it == m_agents.end() ? nullptr : l_it->second.get();
}

std::vector<cam::AgentId> cam::AgentCollection::get_ids(const bool p_alive_only) {
	std::vector<AgentId> l_result;
	l_result.reserve(m_agents.size());

	for (auto& [l_id, l_agent]: m_agents) {
		if (!p_alive_only || !l_agent->is_dead()) {
			l_result.push_back(l_id);
		}
	}

	return l_result;
}

std::vector<int> cam::AgentCollection::random_permutation(const size_t p_number) const {
	std::vector<int> l_numbers;
	random_permutation(p_number, l_numbers);
	return l_numbers;
}

void cam::AgentCollection::random_permutation(const size_t p_number, std::vector<int>& p_numbers) const {
	p_numbers.resize(p_number);
	for (size_t l_index = 0; l_index < p_number; l_index++) {
		p_numbers[l_index] = static_cast<int>(l_index);
	}

	// Nothing to shuffle with fewer than two elements; without this guard p_number - 1 underflows
	// for an empty collection (an empty turn in Parallel or SequentialRandom mode, C1).
	if (p_number < 2) {
		return;
	}

	size_t l_index = p_number - 1;
	while (l_index > 1) {
		const size_t l_k = get_random_value(0, l_index);
		if (l_k == l_index) {
			continue;
		}
		const int l_temp = p_numbers[l_index];
		p_numbers[l_index] = p_numbers[l_k];
		p_numbers[l_k] = l_temp;
		l_index--;
	}
}

size_t cam::AgentCollection::get_random_value(const size_t& p_min, const size_t& p_max) const {
	static std::default_random_engine s_seeder(m_seed);
	static std::mt19937 s_generator(s_seeder());
	std::uniform_int_distribution<size_t> l_dist(p_min, p_max);
	return l_dist(s_generator);
}
