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

#include <uuid/UUID.hpp>

#include "MPSCQueue.hpp"
#include "Message.h"

/**
 * CPPActressMAS
 */
namespace cam {
	class EnvironmentMas;

	/**
	 * Represents the observable properties of an agent. They depend on the set of
	 * Observables properties of an agent and on the PerceptionFilter function of an
	 * agent who wants to observe other agents.
	 **/
	using Observables = std::unordered_map<std::string, json>;
	using ObservablesPointer = std::shared_ptr<const Observables>;

	/**
	 * The base class for an agent that runs on a turn-based manner in its
	 * environment. You must create your own agent classes derived from this abstract class.
	 **/
	class Agent {

		protected:
			/**
			 * Unique ID.
			 **/
			std::string m_id;

			/**
			 * Name of the agent.
			 **/
			std::string m_name;

			/**
			 * True if using observables.
			 **/
			bool m_using_observables;

			/**
			 * True if setup.
			 **/
			bool m_is_setup;

			/**
			 * True if dead.
			 **/
			bool m_is_dead;

			/**
			 * List of observables.
			 **/
			std::shared_ptr<Observables> m_observables;

			/**
			 * The environment where the agent is.
			 **/
			EnvironmentMas* m_environment;

			/**
			 * Messages arrived.
			 **/
			MPSCQueue<MessagePointer> m_messages;

		public:
			/**
			 * Create a new agent.
			 * @param p_name Name of the new agent
			 * @param p_using_observables If true, this agent can see
			 **/
			explicit Agent(std::string p_name, const bool p_using_observables = false);

			/**
			 * Nothing to delete.
			 **/
			virtual ~Agent() = default;

			/**
			 * Return id.
			 * @return Id of the agent
			 **/
			[[nodiscard]] const std::string& get_id() const { return m_id; }

			/**
			 * Return name.
			 * @return Name of the agent
			 **/
			[[nodiscard]] const std::string& get_name() const { return m_name; }

			/**
			 * True if using observables.
			 * @return True if using observables
			 **/
			[[nodiscard]] bool is_using_observables() const { return m_using_observables; }

			/**
			 * True if using observables.
			 * @return True if using observables
			 **/
			void set_using_observables(const bool p_using_observables) { m_using_observables = p_using_observables; }

			/**
			 * Get observables.
			 * @return Observables
			 **/
			[[nodiscard]] ObservablesPointer get_observables() const { return m_observables; }

			/**
			 * True is must run setup.
			 * @return True is must run setup
			 **/
			[[nodiscard]] bool is_setup() const { return m_is_setup; }

			/**
			 * True if is dead.
			 * @return True if is dead
			 **/
			[[nodiscard]] bool is_dead() const { return m_is_dead; }

			/**
			 * Set environment.
			 * @param p_environment The environment
			 **/
			void set_environment(EnvironmentMas *p_environment) { m_environment = p_environment; }

			/**
			 * Internal setup called by the environment.
			 **/
			void internal_setup();

			/**
			 * Internal see called by the environment.
			 **/
			void internal_see();

			/**
			 * Internal action called by the environment.
			 **/
			void internal_action();

			/**
			 * Stops the execution of the agent and removes it from the environment.
			 * Use the Stop method instead of Environment.
			 * Remove when the decision to be stopped belongs to the agent itself.
			 **/
			void stop();

			/**
			 * Receive a new message.
			 * @param p_message The new message
			 **/
			void post(const MessagePointer& p_message);

			/**
			 * Send a new message by ID.
			 * @param p_receiver_id The id of the receiver
			 * @param p_message The message
			 * @param p_length The message size
			 **/
			void send(const std::string& p_receiver_id, const uint8_t* p_message = nullptr, const size_t& p_length = 0) const;
			void send(const std::string& p_receiver_id, const json& p_message) const;

			/**
			 * Send a new message by name.
			 * @param p_receiver_name The name of the receiver
			 * @param p_message The message
			 * @param p_length The message size
			 * @param p_first_only If true, only the first agent found
			 **/
			void send_by_name(const std::string& p_receiver_name, const uint8_t* p_message = nullptr, const size_t& p_length = 0, bool p_first_only = true) const;
			void send_by_name(const std::string& p_receiver_name, const json& p_message, bool p_first_only = true) const;

			/**
			 * Send a new message by fragment name.
			 * @param p_fragment_name The fragment of the receivers
			 * @param p_message The message
			 * @param p_length The message size
			 * @param p_first_only If true, only the first agent found
			 **/
			void send_by_fragment_name(const std::string& p_fragment_name, const uint8_t* p_message = nullptr, const size_t& p_length = 0, bool p_first_only = true) const;
			void send_by_fragment_name(const std::string& p_fragment_name, const json& p_message, bool p_first_only = true) const;

			/**
			 * Send a new message to all agents.
			 * @param p_message The message
			 * @param p_length The message size
			 **/
			void broadcast(const uint8_t* p_message = nullptr, const size_t& p_length = 0) const;
			void broadcast(const json& p_message) const;

			/**
			 * Perception filter.
			 * @param p_observed Observed properties
			 * @return True if the agent is observable
			 **/
			[[nodiscard]] virtual bool perception_filter(const ObservablesPointer& p_observed) const;

			/**
			 * Setup the agent.
			 **/
			virtual void setup();

			/**
			 * Compute see.
			 * @param p_observable_agents The list of observable agents
			 **/
			virtual void see(const std::vector<const ObservablesPointer>& p_observable_agents);

			/**
			 * Compute action.
			 * @param p_message The message to compute
			 **/
			virtual void action(const MessagePointer& p_message);

			/**
			 * Compute action if there is no message.
			 **/
			virtual void default_action();

			// Delete copy constructor
			Agent(const Agent& ) = delete;
			Agent& operator=(Agent& ) = delete;
	};

	// Agent pointer
	using AgentPointer = std::shared_ptr<Agent>;
} // namespace cam
