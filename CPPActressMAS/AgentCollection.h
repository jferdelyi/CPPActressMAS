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
	class AgentCollection {

		protected:

			/**
			 * Agents: name, content
			 * Multiple names are allowed
			 **/
			std::map<std::string, AgentPointer> m_agents;

		public:
		
			/**
			 * Initializes a new instance of a collection of agents
			 **/
			AgentCollection() = default;

			/**
			 * Nothing to delete
			 **/
			virtual ~AgentCollection() = default;

			/**
			 * Return number of agents
			 * @return Number of agents
			 **/
			int count() const;

			/**
			 * Add new agent
			 * @param p_agent Agent to add
			 **/
			void add(AgentPointer& p_agent);

			/**
			 * Returns the name of a randomly selected agent from the environment
			 * @return Random agent
			 **/
			const cam::AgentPointer& random_agent() const;

			/**
			 * Return true if the agent exists
			 * @return True if exists
			 **/
			bool contains(const AgentPointer& p_agent) const;

			/**
			 * Return true if the agent exists
			 * @param p_agent_name The agent id
			 * @return True if exists
			 **/
			bool contains(const std::string& p_agent_name) const;

			/**
			 * Remove agent
			 * @param p_agent_name The agent id
			 **/
			void remove(const std::string& p_agent_name);

			/**
			 * Get agent by id
			 * @param p_agent_name The agent id
			 * @return The agent
			 **/
			AgentPointer get(const std::string& p_agent_name);

			/**
			 * Get all names
			 * @return All names
			 **/
			const std::vector<std::string> get_names() const;

			/**
			 * Get all ids
			 * @return All ids
			 **/
			const std::vector<std::string> get_ids() const;

			/**
			 * Get all agents
			 * @return All names
			 **/
			const std::map<std::string, AgentPointer>& get_agents() const;

			/**
			 * Returns a std::vector with the names of all the agents that contain a certain std::string
			 * @param p_name_fragment The name fragment that the agent names should contain
			 **/
			const std::vector<cam::AgentPointer> filtered_agents(const std::string& p_name_fragment);

			// Delete copy constructor
			AgentCollection(const AgentCollection&) = delete;
			AgentCollection& operator=(AgentCollection&) = delete;
	};
}
