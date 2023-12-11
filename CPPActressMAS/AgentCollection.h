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

/**
 * CPPActressMAS
 */
namespace cam {

	/**
	 * A collection of agents
	 **/
	class AgentCollection final {

		protected:
			/**
			 * Agents: id, content
			 **/
			std::map<std::string, AgentPointer> m_agents;

			/**
			 * Mutex for the map
			 **/
			std::mutex m_mutex;
			std::condition_variable m_cond;

		public:
			/**
			 * Initializes a new instance of a collection of agents
			 **/
			AgentCollection() = default;

			/**
			 * Nothing to delete
			 **/
			/*virtual*/ ~AgentCollection() = default;

			/**
			 * Return number of agents
			 * @return Number of agents
			 **/
			[[nodiscard]] size_t count() const;

			/**
			 * Add new agent
			 * @param p_agent Agent to add
			 **/
			void add(AgentPointer& p_agent);

			/**
			 * Returns a randomly selected agent from the environment
			 * @return Random agent
			 **/
			[[nodiscard]] const AgentPointer& random_agent();

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
			AgentPointer get(const std::string& p_id);

			/**
			 * Get all ids
			 * @return All ids
			 **/
			[[nodiscard]] std::vector<std::string> get_ids();

			/**
			 * Get all agents
			 * @return All agents
			 **/
			[[nodiscard]] const std::map<std::string, AgentPointer>& get_agents() const;

			// Delete copy constructor
			AgentCollection(const AgentCollection& ) = delete;
			AgentCollection& operator=(AgentCollection& ) = delete;
	};
} // namespace cam
