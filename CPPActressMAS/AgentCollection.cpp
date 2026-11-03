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

#include "AgentCollection.h"

#include <algorithm>
#include <iterator>
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

cam::AgentCollection::AgentCollection(const EnvironmentMasMode& p_environment_mas_mode, const unsigned int p_seed,
									   const RandomEngine p_random_engine) :
		m_turn_pool(p_environment_mas_mode == EnvironmentMasMode::Parallel ? std::make_unique<TurnPool>(worker_count()) : nullptr),
		m_environment_mas_mode(p_environment_mas_mode),
		m_seed(p_seed),
		m_random_engine(p_random_engine),
		m_mt19937(p_seed),
		m_dotnet_random(static_cast<std::int32_t>(p_seed)) {

}

size_t cam::AgentCollection::count() const {
	return m_agents.size() - m_retired.load(std::memory_order_relaxed) + pending_count();
}

cam::AgentCollection::PendingShard& cam::AgentCollection::pending_shard() const {
	// Each thread takes its part once, in turn: the threads of a Parallel turn rarely share one
	static std::atomic<std::size_t> s_next_shard{0};
	thread_local const std::size_t t_shard = s_next_shard.fetch_add(1, std::memory_order_relaxed) % PENDING_SHARDS;
	return m_pending_shards[t_shard];
}

std::size_t cam::AgentCollection::pending_count() const {
	std::size_t l_count = 0;
	for (const PendingShard& l_shard: m_pending_shards) {
		l_count += l_shard.m_size.load(std::memory_order_acquire);
	}
	return l_count;
}

template<typename F>
std::vector<cam::AgentPointer> cam::AgentCollection::pending_where(F p_keep) const {
	std::vector<AgentPointer> l_agents;
	if (pending_count() == 0) {
		return l_agents;
	}
	for (const PendingShard& l_shard: m_pending_shards) {
		const std::lock_guard<std::mutex> l_lock(l_shard.m_mutex);
		for (const AgentPointer& l_agent: l_shard.m_agents) {
			if (p_keep(*l_agent)) {
				l_agents.push_back(l_agent);
			}
		}
	}
	return l_agents;
}

void cam::AgentCollection::add(const AgentPointer& p_agent) {
	// Counted under the lock, after the push: a search that counts the agent finds it
	PendingShard& l_shard = pending_shard();
	const std::lock_guard<std::mutex> l_lock(l_shard.m_mutex);
	l_shard.m_agents.push_back(p_agent);
	l_shard.m_size.store(l_shard.m_agents.size(), std::memory_order_release);
}

cam::AgentPointer cam::AgentCollection::get_pending(const AgentId& p_id) const {
	if (pending_count() == 0) {
		return nullptr;
	}
	for (const PendingShard& l_shard: m_pending_shards) {
		const std::lock_guard<std::mutex> l_lock(l_shard.m_mutex);
		for (const AgentPointer& l_agent: l_shard.m_agents) {
			if (l_agent->get_id() == p_id) {
				return l_agent;
			}
		}
	}
	return nullptr;
}

std::vector<cam::AgentPointer> cam::AgentCollection::pending_agents() const {
	return pending_where([](const Agent&) { return true; });
}

std::vector<cam::AgentPointer> cam::AgentCollection::pending_named(const std::string& p_name) const {
	return pending_where([&p_name](const Agent& p_agent) { return p_agent.get_name() == p_name; });
}

cam::AgentPointer cam::AgentCollection::take_pending(const AgentId& p_id) {
	if (pending_count() == 0) {
		return nullptr;
	}
	AgentPointer l_removed;  // returned, destroyed outside the locks by its last holder
	for (PendingShard& l_shard: m_pending_shards) {
		const std::lock_guard<std::mutex> l_lock(l_shard.m_mutex);
		const auto l_it = std::find_if(l_shard.m_agents.begin(), l_shard.m_agents.end(),
									   [&p_id](const AgentPointer& p_agent) { return p_agent->get_id() == p_id; });
		if (l_it != l_shard.m_agents.end()) {
			l_removed = std::move(*l_it);
			l_shard.m_agents.erase(l_it);
			l_shard.m_size.store(l_shard.m_agents.size(), std::memory_order_release);
			break;
		}
	}
	if (!l_removed) {
		return nullptr;
	}
	// A send that found it a moment ago skips it from now on
	l_removed->m_state.fetch_or(Agent::DEAD, std::memory_order_relaxed);
	return l_removed;
}

void cam::AgentCollection::set_simulation_running(const bool p_running) {
	const std::unique_lock<std::shared_mutex> l_lock(m_agents_mutex);
	m_simulation_running.store(p_running, std::memory_order_release);
}

bool cam::AgentCollection::remove_unless_running(const AgentId& p_id) {
	const std::unique_lock<std::shared_mutex> l_lock(m_agents_mutex);
	if (m_simulation_running.load(std::memory_order_acquire)) {
		return false;
	}
	remove_locked(p_id);
	return true;
}

void cam::AgentCollection::insert(const AgentPointer& p_agent) {
	const std::unique_lock<std::shared_mutex> l_lock(m_agents_mutex);
	m_agents.insert(std::make_pair(p_agent->get_id(), p_agent));
	m_agents_by_name.insert(std::make_pair(p_agent->get_name(), p_agent->get_id()));
	mark_inserted(*p_agent);
}

std::optional<cam::AgentId> cam::AgentCollection::random_agent() const {
	// The living agents are copied first: a Parallel turn may stop one between the count and the pick; the agents added
	// and not inserted yet come last, as in the order of addition of the C#
	std::vector<AgentId> l_alive;
	l_alive.reserve(m_agents.size());
	for (const auto& [l_id, l_agent]: m_agents) {
		if (!l_agent->is_dead()) {
			l_alive.push_back(l_id);
		}
	}
	for (const auto& l_agent: pending_agents()) {
		if (!l_agent->is_dead()) {
			l_alive.push_back(l_agent->get_id());
		}
	}
	const std::lock_guard<std::mutex> l_lock(m_random_mutex);
	if (l_alive.empty()) {
		// The C# draws Next(0) then throws: the draw is kept, the exception is not
		if (m_random_engine == RandomEngine::DotNet) {
			m_dotnet_random.next(0);
		} else {
			m_mt19937();
		}
		return {};
	}
	return l_alive[next_index(l_alive.size())];
}

bool cam::AgentCollection::contains(const AgentPointer& p_agent) const {
	return m_agents.contains(p_agent->get_id());
}

bool cam::AgentCollection::contains(const AgentId& p_id) const {
	return m_agents.contains(p_id);
}

void cam::AgentCollection::remove(const AgentId& p_id) {
	const std::unique_lock<std::shared_mutex> l_lock(m_agents_mutex);
	remove_locked(p_id);
}

void cam::AgentCollection::remove_locked(const AgentId& p_id) {
	const auto l_it = m_agents.find(p_id);
	if (l_it == m_agents.end()) {
		return;
	}
	// Drop the (name, id) entry of the name index as well, otherwise a later send_by_name()
	// dereferences a null pointer.
	remove_name(l_it->second->get_name(), p_id);
	mark_removed(*l_it->second);
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

void cam::AgentCollection::mark_inserted(Agent& p_agent) {
	// Stopped before its insertion: counted now, the stop did not see it inserted
	if (const std::uint8_t l_old = p_agent.m_state.fetch_or(Agent::INSERTED, std::memory_order_relaxed);
		(l_old & Agent::DEAD) != 0 && (l_old & Agent::INSERTED) == 0) {
		m_retired.fetch_add(1, std::memory_order_relaxed);
	}
}

void cam::AgentCollection::mark_removed(Agent& p_agent) {
	// A dead agent in the collection was counted, by its stop or by its insertion
	constexpr auto l_keep = static_cast<std::uint8_t>(~Agent::INSERTED);
	if (const std::uint8_t l_old = p_agent.m_state.fetch_and(l_keep, std::memory_order_relaxed);
		(l_old & Agent::DEAD) != 0 && (l_old & Agent::INSERTED) != 0) {
		m_retired.fetch_sub(1, std::memory_order_relaxed);
	}
}

void cam::AgentCollection::run_turn() {
	// The collections do not change during the turn: a removal marks the agent and the boundary erases it
	const TurnGuard l_guard(m_turn_running);

	// Sequential: an agent removed earlier in the turn is skipped by run_turn(), as the C# (ContainsKey)
	if (m_environment_mas_mode == EnvironmentMasMode::Sequential) {
		for (auto& [l_agent_id, l_agent]: m_agents) {
			l_agent->run_turn();
		}

		// Parallel: every agent alive at the start of the turn at once, without copy nor shuffle, as the C# does
		// (Parallel.Invoke); an agent of this snapshot runs even if it is removed meanwhile, as in the C#
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
	const std::unique_lock<std::shared_mutex> l_lock(m_agents_mutex);

	// Erase the dead agents in one pass that keeps the insertion order (an erase per agent would shift the rest each
	// time), and only when one died: each dead agent of the collection is counted, so a turn without a death costs no
	// pass. The predicate runs once per agent, in order: it decides and cleans up at once, even if another thread stops
	// an agent meanwhile.
	if (m_retired.load(std::memory_order_relaxed) != 0) {
		erase_if(m_agents, [this, &l_changes, p_track_changes](const std::pair<AgentId, AgentPointer>& p_entry) {
			Agent& l_agent = *p_entry.second;
			if (!l_agent.is_dead()) {
				return false;
			}
			mark_removed(l_agent);
			remove_name(l_agent.get_name(), p_entry.first);
			if (p_track_changes) {
				l_changes.m_removed.emplace_back(p_entry.first, l_agent.get_name());
			}
			return true;
		});
	}

	insert_pending(l_changes, p_track_changes);
	return l_changes;
}

void cam::AgentCollection::insert_pending(BufferChanges& p_changes, const bool p_track_changes) {
	if (pending_count() == 0) {
		return;
	}
	// Taken from every part, part after part, each in the order of its thread; an agent added meanwhile waits for the next
	// boundary
	std::vector<AgentPointer> l_added;
	for (PendingShard& l_shard: m_pending_shards) {
		const std::lock_guard<std::mutex> l_lock(l_shard.m_mutex);
		std::move(l_shard.m_agents.begin(), l_shard.m_agents.end(), std::back_inserter(l_added));
		l_shard.m_agents.clear();
		l_shard.m_size.store(0, std::memory_order_release);
	}
	for (const AgentPointer& l_agent: l_added) {
		m_agents.insert(std::make_pair(l_agent->get_id(), l_agent));
		m_agents_by_name.insert(std::make_pair(l_agent->get_name(), l_agent->get_id()));
		mark_inserted(*l_agent);
		if (p_track_changes) {
			p_changes.m_added.emplace_back(l_agent->get_id(), l_agent->get_name());
		}
	}
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

	// Fisher-Yates of the C# (EnvironmentMas.RandomPermutation): a draw in [0, n) for n from the count down to 2, then a
	// swap with the last of the n; no draw with fewer than two elements
	const std::lock_guard<std::mutex> l_lock(m_random_mutex);
	for (size_t l_count = p_number; l_count > 1;) {
		const size_t l_k = next_index(l_count);
		--l_count;
		std::swap(p_numbers[l_count], p_numbers[l_k]);
	}
}

size_t cam::AgentCollection::next_index(const size_t p_bound) const {
	if (m_random_engine == RandomEngine::DotNet) {
		return static_cast<size_t>(m_dotnet_random.next(static_cast<std::int32_t>(p_bound)));
	}
	// Bounded draw of Lemire with rejection: the same numbers with every standard library, unlike
	// uniform_int_distribution
	const auto l_bound = static_cast<std::uint32_t>(p_bound);
	std::uint64_t l_product = static_cast<std::uint64_t>(static_cast<std::uint32_t>(m_mt19937())) * l_bound;
	auto l_low = static_cast<std::uint32_t>(l_product);
	if (l_low < l_bound) {
		const std::uint32_t l_threshold = static_cast<std::uint32_t>(0U - l_bound) % l_bound;
		while (l_low < l_threshold) {
			l_product = static_cast<std::uint64_t>(static_cast<std::uint32_t>(m_mt19937())) * l_bound;
			l_low = static_cast<std::uint32_t>(l_product);
		}
	}
	return static_cast<size_t>(l_product >> 32);
}

size_t cam::AgentCollection::get_random_value(const size_t& p_min, const size_t& p_max) const {
	const std::lock_guard<std::mutex> l_lock(m_random_mutex);
	return p_min + next_index(p_max - p_min + 1);
}
