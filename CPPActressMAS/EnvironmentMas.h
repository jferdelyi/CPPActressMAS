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

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <future>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "AgentCollection.h"
#include "Message.h"

/**
 * CPPActressMAS
 */
namespace cam {

	/**
	 * Environments execution modes
	 */
	enum class EnvironmentMasMode { 
		Parallel, 			// If the execution is parallel, agents are always run in random order.
		Sequential, 
		SequentialRandom
	};

	class Agent;

	/**
	 * An abstract base class for the multiagent environment, where all the agents are executed.
	 **/
	class EnvironmentMas {

		protected:

			/**
			 * The maximum number of turns of the simulation..
			 **/
			int m_no_turns;

			/**
			 * A delay (in miliseconds) after each turn.
			 **/
			int m_delay_after_turn;

			/**
			 * If true the order is random.
			 **/
			bool m_random_order;

			/**
			 * True the execution is parallel.
			 **/
			bool m_parallel;

			/**
			 * Random seed.
			 **/
			unsigned int m_seed;

			/**
			 * The agents in the environment
			 **/
			AgentCollection m_agents;

		public:
		 
			/**
			 * Initializes a new instance of the EnvironmentMas class.
			 *
			 * @param p_no_turns The maximum number of turns of the simulation. Setup is considered to be the first turn. The simulation may stop earlier if there are no more agents in the environment. If the number of turns is 0, the simulation runs indefinitely, or until there are no more agents in the environment.
			 * @param p_mode Whether agent behaviors are executed in parallel, sequentially or sequentially with random order. The code of a single agent in a turn is always executed sequentially.
			 * @param p_delay_after_turn A delay (in miliseconds) after each turn.
			 * @param p_seed A random number generator seed for non-deterministic but repeatable experiments.
			 **/
			EnvironmentMas(int p_no_turns = 0, const EnvironmentMasMode& p_mode = EnvironmentMasMode::Parallel, int p_delay_after_turn = 0, const unsigned int p_seed = std::time(0));
			
			/**
			 * Nothing to delete.
			 **/
			virtual ~EnvironmentMas() = default;

			/**
			 * Adds an agent to the environment. The agent should already have a name and its name should be unique.
			 *
			 * @param p_agent The concurrent agent that will be added
			 * @return Id of the agent
			 **/
			const std::string add(cam::AgentPointer&& p_agent);

			/**
			 * Continues the simulation for an additional number of turns, after an initial simulation has finished.
			 * The simulation may stop earlier if there are no more agents in the environment.
			 * If the number of turns is 0, the simulation runs indefinitely, or until there are no more agents in the environment.
			 *
			 * @param p_no_turns The maximum number of turns of the continued simulation
			 **/
			void continue_simulation(int p_no_turns = 0);

			/**
			 * Returns a std::vector with the names of all the agents that contain a certain std::string.
			 *
			 * @param p_name_fragment The name fragment that the agent names should contain
			 **/
			const std::vector<cam::AgentPointer> filtered_agents(const std::string& p_name_fragment);

			/**
			 * Returns the name of a randomly selected agent from the environment
			 * @retrun Randomly selected agent
			 **/
			const cam::AgentPointer& random_agent();

			/**
			 * Stops the execution of the agent and removes it from the environment. Use the Remove method instead of Agent.Stop
			 * when the decision to stop an agent does not belong to the agent itself, but to some other agent or to an external factor.
			 *
			 * @param p_agent The agent to be removed
			 **/
			void remove(const AgentPointer& p_agent);

			/**
			 * Stops the execution of the agent identified by name and removes it from the environment. Use the Remove method instead of Agent.Stop
			 * when the decision to stop an agent does not belong to the agent itself, but to some other agent or to an external factor.
			 *
			 * @param p_agent_name The name of the agent to be removed
			 **/
			void remove(const std::string& p_agent_name);

			/**
			 * Sends a message from the outside of the multiagent system. Whenever possible, the agents should use the Send method of their own class,
			 * not the Send method of the environment. This method can also be used to simulate a forwarding behavior.
			 *
			 * @param p_message The message to be sent
			 **/
			void send(const MessagePointer& p_message);

			/**
			 * Send a new message to all agents.
			 * @param p_sender From
			 * @param p_message The message
			 **/
			void broadcast(const std::string& p_sender, const json& p_message);

			/**
			 * Starts the simulation.
			 **/
			void start();

			/**
			 * The number of agents in the environment
			 **/
			int agents_count() const;

			/**
			 * A method that may be optionally overriden to perform additional processing after the simulation has finished.
			 **/
			virtual void simulation_finished();

			/**
			 * A method that may be optionally overriden to perform additional processing after a turn of the simulation has finished.
			 *
			 * @param p_turn The turn that has just finished
			 **/
			virtual void turn_finished(int p_turn);

			/**
			 * Get the list of observable agenst for an agent and its perception filter.
			 *
			 * @param p_perceiving_agent Peceiving agent
			 **/
			std::vector<ObservableAgentPointer> get_list_of_observable_agents(const cam::Agent* p_perceiving_agent);

		protected:

			/**
			 * Execute See and Act of the given agent in parallel.
			 *
			 * @param p_agent Agent to Execute See and Act
			 **/
			std::future<void> execute_setup(AgentPointer& p_agent);

			/**
			 * Execute See and Act of the given agent in parallel.
			 *
			 * @param p_agent Agent to Execute See and Act
			 **/
			std::future<void> execute_see_action(AgentPointer& p_agent);

			/**
			 * Run one turn.
			 *
			 * @param p_turn The current turn
			 **/
			void run_turn(int p_turn);

			/**
 			 * Return a vector (p_number length) of random index.
			 * Fisher-Yates shuffle.
			 *
			 * @param p_number Number of value in the returned vector
			 **/
			const std::vector<int> random_permutation(int p_number);
			
			/**
			 * Return a vector (p_number length) of ordered index.
			 *
			 * @param p_number Number of value in the returned vector
			 **/
			const std::vector<int> sorted_permutation(int p_number);

			// Delete copy constructor
			EnvironmentMas(const EnvironmentMas&) = delete;
			EnvironmentMas& operator=(EnvironmentMas&) = delete;
	};
}
