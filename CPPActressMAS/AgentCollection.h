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

#include <memory>
#include <vector>

#include "Agent.h"
#include "TurnPool.h"
#include "tsl/ordered_map.h"

/**
 * CPPActressMAS
 */
namespace cam {

	enum class EnvironmentMasMode;

	/**
	 * A collection of agents
	 **/
	class AgentCollection final {
		friend Environment;

	protected:
		/**
		 * Agents: id, content
		 **/
		tsl::ordered_map<std::string, AgentPointer> m_agents;

		/**
		 * Agents: name, id
		 **/
		std::unordered_multimap<std::string, std::string> m_agents_by_name;

		/**
		 * New agent buffer
		 */
		MPSCQueue<AgentPointer> m_new_agents;

		/**
		 * Threads of a Parallel environment (none otherwise)
		 **/
		std::unique_ptr<TurnPool> m_turn_pool;

		/**
		 * Agents of the current Parallel turn (capacity kept from turn to turn)
		 **/
		std::vector<Agent*> m_turn_agents;

		/**
		 * Environment mode.
		 **/
		EnvironmentMasMode m_environment_mas_mode;

		/**
		 * Random generator
		 **/
		unsigned int m_seed;

	private:
		/**
		 * Remove the (name, id) entry of an agent from the name index.
		 * @param p_name The agent name
		 * @param p_id The agent id
		 **/
		void remove_name(const std::string& p_name, const std::string& p_id);

	public:
		/**
		 * Initializes a new instance of a collection of agents
		 **/
		explicit AgentCollection(const EnvironmentMasMode& p_environment_mas_mode, unsigned int p_seed);

		/**
		 * Nothing to delete
		 **/
		~AgentCollection() = default;

		/**
		 * Return number of agents
		 * @return Number of agents
		 **/
		[[nodiscard]] size_t count() const;

		/**
		 * Add new agent
		 * @param p_agent Agent to add
		 **/
		void add(const AgentPointer& p_agent);

		/**
		 * Insert an agent at once, outside any turn (turn boundary only: arrival of a migrated agent)
		 * @param p_agent Agent to insert
		 **/
		void insert(const AgentPointer& p_agent);

		/**
		 * Returns a randomly selected agent from the environment
		 * @return Random agent
		 **/
		[[nodiscard]] const AgentPointer& random_agent() const;

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
		[[nodiscard]] bool contains(const std::string& p_id) const;

		/**
		 * Remove agent from the collection and from the name index; unknown ids are ignored
		 * @param p_id The agent id
		 **/
		void remove(const std::string& p_id);

		/**
		 * Get agent by id
		 * @param p_id The agent id
		 * @return The agent
		 **/
		AgentPointer get(const std::string& p_id) const;

		/**
		 * Run one turn
		 **/
		void run_turn();

		/**
		 * What a turn boundary changed: (id, name) of the agents inserted and removed.
		 **/
		struct BufferChanges {
			std::vector<std::pair<std::string, std::string>> m_added;
			std::vector<std::pair<std::string, std::string>> m_removed;
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
		[[nodiscard]] std::vector<std::string> get_ids(bool p_alive_only = true);

		/**
		 * Return a vector (p_number length) of random index.
		 * Fisher-Yates shuffle.
		 * @param p_number Number of value in the returned vector
		 **/
		std::vector<int> random_permutation(size_t p_number) const;

		/**
		 * Return random number
		 * @param p_min min value
		 * @param p_max max value
		 * @return random value between min and max
		 **/
		size_t get_random_value(const size_t& p_min, const size_t& p_max) const;

		// Delete copy constructor
		AgentCollection(const AgentCollection&) = delete;

		AgentCollection& operator=(AgentCollection&) = delete;
	};
} // namespace cam
