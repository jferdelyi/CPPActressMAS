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

#include "Agent.h"
#include "ThreadPool/ThreadPool.h"
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
		friend EnvironmentMas;

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
			 * Thread pool for agents
			 **/
			ThreadPool m_pool;

			/**
			 * Environment mode.
			 **/
			EnvironmentMasMode m_environment_mas_mode;

		public:
			/**
			 * Initializes a new instance of a collection of agents
			 **/
			explicit AgentCollection(const EnvironmentMasMode& p_environment_mas_mode);

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
			 * Remove agent
			 * @param p_id The agent id
			 **/
			void remove(const std::string& p_id);

			/**
			 * Get agent by id
			 * @param p_id The agent id
			 * @return The agentx
			 **/
			AgentPointer get(const std::string& p_id) const;

			/**
			 * Run one turn
			 **/
			void run_turn();

			/**
			 * Add new agents, remove dead ones
			 **/
			void process_buffers();

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
			static std::vector<int> random_permutation(size_t p_number);

			// Delete copy constructor
			AgentCollection(const AgentCollection& ) = delete;
			AgentCollection& operator=(AgentCollection& ) = delete;
	};
} // namespace cam
