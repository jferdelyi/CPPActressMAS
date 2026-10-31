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

#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <shared_mutex>
#include <string>
#include <utility>
#include <vector>

#include "Agent.h"
#include "util/DotnetRandom.h"
#include "util/RecyclingAllocator.hpp"
#include "util/TurnPool.h"
#include "tsl/ordered_map.h"

/**
 * CPPActressMAS
 */
namespace cam {

	enum class EnvironmentMasMode;

	enum class RandomEngine;

	/**
	 * A collection of agents
	 **/
	class AgentCollection final {
		friend Environment;

	protected:
		/**
		 * Agents: id, content
		 **/
		tsl::ordered_map<AgentId, AgentPointer> m_agents;

		/**
		 * Agents: name, id
		 **/
		std::unordered_multimap<std::string, AgentId, std::hash<std::string>, std::equal_to<std::string>,
							   RecyclingAllocator<std::pair<const std::string, AgentId>>> m_agents_by_name;

		/**
		 * Guards m_agents and m_agents_by_name against the calls of other threads: the simulation thread writes under the
		 * exclusive lock (turn boundary, removal outside a simulation, arrival of a migrated agent), the public functions of
		 * the environment read under the shared lock; the agents read without it, a turn never meeting a boundary
		 **/
		mutable std::shared_mutex m_agents_mutex;

		/**
		 * A part of the agents added and not inserted yet, chosen by the thread that adds: its agents in the order of their
		 * addition (an addition costs a lock that no other thread takes but a search; a search scans every part, consulted
		 * only when the inserted agents do not answer), and their number, written under the lock
		 **/
		struct PendingShard {
			mutable std::mutex m_mutex;
			std::vector<AgentPointer> m_agents;
			std::atomic<std::size_t> m_size{0};
		};

		/**
		 * Agents added and not inserted yet, in parts chosen by the thread that adds (agents of many threads add at once):
		 * visible at once to every send and search, as the immediate Add of the C#, inserted at the next turn boundary,
		 * part after part, each in the order of its thread (the order between two threads that add at once is not fixed,
		 * as in the C#)
		 **/
		static constexpr std::size_t PENDING_SHARDS = 16;
		mutable std::array<PendingShard, PENDING_SHARDS> m_pending_shards;


		/**
		 * Threads of a Parallel environment (none otherwise)
		 **/
		std::unique_ptr<TurnPool> m_turn_pool;

		/**
		 * Agents of the current Parallel turn (capacity kept from turn to turn)
		 **/
		std::vector<Agent*> m_turn_agents;

		/**
		 * Ids of the alive agents of the current SequentialRandom turn, and their drawn order (capacity kept from turn to
		 * turn)
		 **/
		std::vector<AgentId> m_turn_ids;
		std::vector<int> m_turn_order;

		/**
		 * Dead agents still in m_agents (stopped or removed during a turn, erased at the next boundary): not counted
		 **/
		std::atomic<size_t> m_retired{0};

		/**
		 * True while run_turn() runs the agents: a removal is then deferred to the turn boundary
		 **/
		std::atomic<bool> m_turn_running{false};

		/**
		 * True while a start() or a continue_simulation() runs, turn boundaries included: a removal is then deferred to
		 * the next boundary, whatever the thread
		 **/
		std::atomic<bool> m_simulation_running{false};

		/**
		 * Environment mode.
		 **/
		EnvironmentMasMode m_environment_mas_mode;

		/**
		 * Seed of the random generator
		 **/
		unsigned int m_seed;

		/**
		 * Random generator of this environment (random order, random_agent): mt19937 or DotnetRandom, as chosen by
		 * m_random_engine, guarded by m_random_mutex since several threads of a Parallel turn may draw
		 **/
		RandomEngine m_random_engine;
		mutable std::mt19937 m_mt19937;
		mutable DotnetRandom m_dotnet_random;
		mutable std::mutex m_random_mutex;

	private:
		/**
		 * Remove the (name, id) entry of an agent from the name index.
		 * @param p_name The agent name
		 * @param p_id The agent id
		 **/
		void remove_name(const std::string& p_name, const AgentId& p_id);

		/**
		 * Mark an agent inserted in the collection; counted retired if it was stopped before.
		 * @param p_agent The agent
		 **/
		void mark_inserted(Agent& p_agent);

		/**
		 * Mark an agent out of the collection; no longer counted retired if it was.
		 * @param p_agent The agent
		 **/
		void mark_removed(Agent& p_agent);

		/**
		 * Fill a vector with a random permutation of 0 to p_number - 1 (Fisher-Yates shuffle), reusing its capacity.
		 * @param p_number Number of values
		 * @param p_numbers Receives the permutation
		 **/
		void random_permutation(size_t p_number, std::vector<int>& p_numbers) const;

		/**
		 * Uniform index in [0, p_bound) from the generator of this environment; the caller holds m_random_mutex.
		 * @param p_bound The exclusive bound, at least 1
		 * @return The index
		 **/
		size_t next_index(size_t p_bound) const;

		/**
		 * The part of the agents added and not inserted yet of the calling thread.
		 * @return The part
		 **/
		PendingShard& pending_shard() const;

		/**
		 * Number of agents added and not inserted yet, the sum of the parts (0 most of the time: no lock then).
		 * @return The number
		 **/
		[[nodiscard]] std::size_t pending_count() const;

		/**
		 * The agents added and not inserted yet that pass a test, in their order of addition.
		 * @param p_keep The test
		 * @return The agents
		 **/
		template<typename F>
		[[nodiscard]] std::vector<AgentPointer> pending_where(F p_keep) const;

	public:
		/**
		 * Initializes a new instance of a collection of agents
		 **/
		explicit AgentCollection(const EnvironmentMasMode& p_environment_mas_mode, unsigned int p_seed, RandomEngine p_random_engine);

		/**
		 * Nothing to delete
		 **/
		~AgentCollection() = default;

		/**
		 * Return number of agents, the stopped or removed ones excluded
		 * @return Number of agents
		 **/
		[[nodiscard]] size_t count() const;

		/**
		 * Add an agent: visible at once to every send and search, inserted at the next turn boundary, run from the turn
		 * after it (any thread)
		 * @param p_agent Agent to add
		 **/
		void add(const AgentPointer& p_agent);

		/**
		 * An agent added and not inserted yet, shared: it stays alive while the caller uses it, even if removed meanwhile
		 * @param p_id The agent id
		 * @return The agent, or nullptr
		 **/
		[[nodiscard]] AgentPointer get_pending(const AgentId& p_id) const;

		/**
		 * The agents added and not inserted yet, in their order of addition (none when there are none, without any lock)
		 * @return The agents
		 **/
		[[nodiscard]] std::vector<AgentPointer> pending_agents() const;

		/**
		 * The agents added and not inserted yet that bear a name, in their order of addition
		 * @param p_name The name
		 * @return The agents
		 **/
		[[nodiscard]] std::vector<AgentPointer> pending_named(const std::string& p_name) const;

		/**
		 * Take out at once an agent added and not inserted yet (any thread), as the immediate Remove of the C#
		 * @param p_id The agent id
		 * @return The agent if it was waiting for its insertion, else nullptr: its last holder destroys it, outside the locks
		 **/
		[[nodiscard]] AgentPointer take_pending(const AgentId& p_id);

		/**
		 * True while a start() or a continue_simulation() runs (removals are deferred to the next boundary)
		 * @return True during a simulation
		 **/
		[[nodiscard]] bool is_simulation_running() const { return m_simulation_running.load(std::memory_order_acquire); }

		/**
		 * Mark the start or the end of a simulation, under the exclusive lock: a removal from another thread then sees it
		 * before it erases anything (simulation thread only)
		 * @param p_running True at the start, false at the end
		 **/
		void set_simulation_running(bool p_running);

		/**
		 * Remove an inserted agent at once unless a simulation runs, under the exclusive lock (any thread); the caller
		 * marks it otherwise, and the next boundary erases it
		 * @param p_id The agent id
		 * @return True if the agent was erased (or unknown), false if a simulation runs
		 **/
		bool remove_unless_running(const AgentId& p_id);

		/**
		 * Insert an agent at once, outside any turn (turn boundary only: arrival of a migrated agent)
		 * @param p_agent Agent to insert
		 **/
		void insert(const AgentPointer& p_agent);

		/**
		 * Returns a randomly selected agent: one draw in [0, number of agents) over the agents in their order, as
		 * RandomAgent of the C#, one draw all the same when no agent is left
		 * @return Random agent id, or nothing when no agent is left
		 **/
		[[nodiscard]] std::optional<AgentId> random_agent() const;

		/**
		 * Return true if the agent exists
		 * @return True if exists
		 **/
		[[nodiscard]] bool contains(const AgentPointer& p_agent) const;

		/**
		 * Return true if the agent exists
		 * @param p_id The agent id
		 * @return True if exists
		 **/
		[[nodiscard]] bool contains(const AgentId& p_id) const;

		/**
		 * Remove agent from the collection and from the name index at once, outside any turn; unknown ids are ignored
		 * @param p_id The agent id
		 **/
		void remove(const AgentId& p_id);

		/**
		 * True while the agents of a turn run (removals are deferred to the turn boundary)
		 * @return True during a turn
		 **/
		[[nodiscard]] bool is_turn_running() const { return m_turn_running.load(std::memory_order_relaxed); }

		/**
		 * Get agent by id
		 * @param p_id The agent id
		 * @return The agent
		 **/
		AgentPointer get(const AgentId& p_id) const;

		/**
		 * Agent by id, without sharing its ownership: valid until the next turn boundary, where agents are removed
		 * @param p_id The agent id
		 * @return The agent, or nullptr
		 **/
		[[nodiscard]] Agent* find(const AgentId& p_id) const;

		/**
		 * Run one turn
		 **/
		void run_turn();

		/**
		 * What a turn boundary changed: (id, name) of the agents inserted and removed.
		 **/
		struct BufferChanges {
			std::vector<std::pair<AgentId, std::string>> m_added;
			std::vector<std::pair<AgentId, std::string>> m_removed;
		};

		/**
		 * Add new agents, remove dead ones
		 * @param p_track_changes If true, report the inserted and removed agents (directory events)
		 * @return The changes (empty unless tracked)
		 **/
		BufferChanges process_buffers(bool p_track_changes = false);

		/**
		 * Get all ids
		 * @param p_alive_only if true then return only alive agents
		 * @return All ids
		 **/
		[[nodiscard]] std::vector<AgentId> get_ids(bool p_alive_only = true);

		/**
		 * Return a vector (p_number length) of random index.
		 * Fisher-Yates shuffle.
		 * @param p_number Number of value in the returned vector
		 **/
		std::vector<int> random_permutation(size_t p_number) const;

		/**
		 * Return random number from the generator of this environment
		 * @param p_min min value
		 * @param p_max max value
		 * @return random value between min and max, both included
		 **/
		size_t get_random_value(const size_t& p_min, const size_t& p_max) const;

		// Delete copy constructor
		AgentCollection(const AgentCollection&) = delete;

		AgentCollection& operator=(AgentCollection&) = delete;

	private:
		/**
		 * Insert the agents added since the previous boundary, in their order of addition; the caller holds the exclusive
		 * lock.
		 * @param p_changes Receives the inserted agents when tracked
		 * @param p_track_changes If true, report them
		 **/
		void insert_pending(BufferChanges& p_changes, bool p_track_changes);

		/**
		 * Remove an inserted agent from the collection and from the name index; the caller holds the exclusive lock;
		 * unknown ids are ignored.
		 * @param p_id The agent id
		 **/
		void remove_locked(const AgentId& p_id);
	};
} // namespace cam
