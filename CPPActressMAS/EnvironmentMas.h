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

#include <vector>

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
		Parallel,
		Sequential,
		SequentialRandom
	};

	class Agent;

	template <typename T>
	concept ConceptAgent = std::is_base_of_v<Agent, T>;

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
			 * The agents in the environment
			 **/
			AgentCollection m_agent_collection;

			/**
			 * Random generator
			 **/
			std::default_random_engine m_random_generator;

		public:

			/**
			 * Initializes a new instance of the EnvironmentMas class.
			 *
			 * @param p_no_turns The maximum number of turns of the simulation. Setup is
			 * considered to be the first turn. The simulation may stop earlier if there
			 * are no more agents in the environment. If the number of turns is 0, the
			 * simulation runs indefinitely, or until there are no more agents in the
			 * environment.
			 * @param p_mode Whether agent behaviors are executed in parallel,
			 * sequentially or sequentially with random order. The code of a single agent
			 * in a turn is always executed sequentially.
			 * @param p_delay_after_turn A delay (in miliseconds) after each turn.
			 * @param p_seed A random number generator seed for non-deterministic but
			 * repeatable experiments.
			 **/
			explicit EnvironmentMas(int p_no_turns = 0, const EnvironmentMasMode& p_mode = EnvironmentMasMode::Parallel, int p_delay_after_turn = 0, unsigned int p_seed = std::time(nullptr));

			/**
			 * Nothing to delete.
			 **/
			virtual ~EnvironmentMas() = default;

			/**
			 * Adds an agent to the environment.
			 * @param p_args Argument to create a new agent
			 * @return Id of the agent
			 **/
			template <ConceptAgent T>
			const std::string& add(auto&&... p_args) {
				const auto& l_agent = AgentPointer(new T(std::forward<decltype(p_args)>(p_args)...));
				l_agent->set_environment(this);
				m_agent_collection.add(l_agent);
				return l_agent->get_id();
			}

			/**
			 * Continues the simulation for an additional number of turns, after an
			 * initial simulation has finished. The simulation may stop earlier if there
			 * are no more agents in the environment. If the number of turns is 0, the
			 * simulation runs indefinitely, or until there are no more agents in the
			 * environment.
			 *
			 * @param p_no_turns The maximum number of turns of the continued simulation
			 **/
			void continue_simulation(int p_no_turns = 0);

			/**
			 * Stops the execution of the agent identified by id and removes it from the
			 * environment. Use the Remove method instead of Agent.Stop when the decision
			 * to stop an agent does not belong to the agent itself, but to some other
			 * agent or to an external factor.
			 *
			 * @param p_agent_id The id of the agent to be removed
			 **/
			void remove(const std::string& p_agent_id);

			/**
			 * Sends a message from the outside of the multiagent system. Whenever
			 * possible, the agents should use the Send method of their own class, not
			 * the Send method of the environment. This method can also be used to
			 * simulate a forwarding behavior.
			 * @param p_sender_id The sender ID
			 * @param p_receiver_id The receiver name
			 * @param p_message The message to be sent
			 * @param p_length The message length
			 * @param p_binary_format The message binary format
			 **/
			void send(const std::string& p_sender_id, const std::string& p_receiver_id, const uint8_t* p_message, const size_t& p_length, const MessageBinaryFormat& p_binary_format = MessageBinaryFormat::RAW) const;

			/**
			 * Sends a message by name.
			 * @param p_sender_id The sender ID
			 * @param p_receiver_name The receiver name
			 * @param p_message The message to be sent
			 * @param p_length The message length
			 * @param p_is_fragment If true search all agent that the name contain "p_receiver_name"
			 * @param p_first_only If true send to the first agent found
			 * @param p_binary_format The message binary format
			 **/
			void send_by_name(const std::string& p_sender_id, const std::string& p_receiver_name, const uint8_t* p_message, const size_t& p_length, bool p_is_fragment, bool p_first_only, const MessageBinaryFormat& p_binary_format = MessageBinaryFormat::RAW) const;

			/**
			 * Send a new message to all agents.
			 * @param p_sender_id From
			 * @param p_message The message
			 * @param p_length The message length
			 * @param p_binary_format The message binary format
			 **/
			void broadcast(const std::string& p_sender_id, const uint8_t* p_message, const size_t& p_length, const MessageBinaryFormat& p_binary_format = MessageBinaryFormat::RAW) const;

			/**
			 * Starts the simulation.
			 **/
			 void start();

			/**
			 * The number of agents in the environment
			 **/
			[[nodiscard]] size_t agents_count() const;

			/**
			 * Get all agents by name
			 * @param p_name the name of agent
			 * @param p_first_only if true the first found
			 * @return All agents by name
			 **/
			[[nodiscard]] std::vector<std::string> get_agents_by_name(const std::string& p_name, bool p_first_only = false) const;

			/**
			 * Get first agent by name
			 * @param p_name the name of agent
			 * @return All agents by name
			 **/
			[[nodiscard]] std::optional<std::string> get_first_agent_by_name(const std::string& p_name) const;

			/**
			 * Get agent name by id
			 * @param p_id the name of agent
			 * @return Agents name
			 **/
			[[nodiscard]] std::optional<std::string> get_agent_name(const std::string& p_id) const;

			/**
			 * Get all agents by fragment name
			 * @param p_fragment_name the fragment name of agent
			 * @param p_first_only if true the first found
			 * @return All agents by fragment name
			 **/
			[[nodiscard]] std::vector<std::string> get_filtered_agents(const std::string& p_fragment_name, bool p_first_only = false) const;

			/**
			 * A method that may be optionally overriden to perform additional
			 * processing after the simulation has finished.
			 **/
			virtual void simulation_finished();

			/**
			 * A method that may be optionally overriden to perform additional
			 * processing after a turn of the simulation has finished.
			 * @param p_turn The turn that has just finished
			 **/
			virtual void turn_finished(int p_turn);

			/**
			 * Get the list of observable agents for an agent and its perception filter.
			 * @param p_perceiving_agent Peceiving agent
			 **/
			std::vector<const ObservablesPointer> get_list_of_observable_agents(const Agent *p_perceiving_agent) const;

			// Delete copy constructor
			EnvironmentMas(const EnvironmentMas& ) = delete;
			EnvironmentMas& operator=(EnvironmentMas& ) = delete;

		protected:

			/**
			 * Adds an agent to the environment.
			 * @param p_agent The concurrent agent that will be added
			 * @return Id of the agent
			 **/
			const std::string& add(AgentPointer&& p_agent);

			/**
			 * Stops the execution of the agent and removes it from the environment. Use
			 * the Remove method instead of Agent.Stop when the decision to stop an agent
			 * does not belong to the agent itself, but to some other agent or to an
			 * external factor.
			 *
			 * @param p_agent The agent to be removed
			 **/
			 void remove(const AgentPointer& p_agent);

			/**
			 * Returns the id of a randomly selected agent from the environment
			 * @retrun Randomly selected agent
			 **/
			[[nodiscard]] const AgentPointer& random_agent() const;

			/**
			 * Get an agent.
			 * @param p_id Agent ID
			 * @return Agent pointer
			 **/
			AgentPointer get(const std::string& p_id) const;

			/**
			 * Run one turn.
			 * @param p_turn The current turn
			 **/
			virtual void run_turn(int p_turn);

	};
} // namespace cam
