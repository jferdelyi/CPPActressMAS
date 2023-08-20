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

#include <moodycamel/concurrentqueue.h>
#include <uuid/UUID.hpp>

#include "Message.h"

/**
 * CPPActressMAS
 */
namespace cam {
	
	class EnvironmentMas;
	
	/**
	 * The struct that represents the observable properties of an agent. They depend on the set of Observables properties of an agent and
	 * on the PerceptionFilter function of an agent who wants to observe other agents.
	 **/
	struct ObservableAgent {
		ObservableAgent(const std::unordered_map<std::string, std::string>& p_observables) :
			m_observables(p_observables) {
		}
		std::unordered_map<std::string, std::string> m_observables;
	};
	using ObservableAgentPointer = std::shared_ptr<cam::ObservableAgent>;

	/**
	 * The base class for an agent that runs on a turn-based manner in its environment. You must create your own agent classes derived from this abstract class.
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
			 * True if setup.
			 **/
			bool m_is_setup;

			/**
			 * List of observables.
			 **/
			std::unordered_map<std::string, std::string> m_observables;

			/**
			 * The environment where the agent is.
			 **/
			cam::EnvironmentMas* m_environment;

			/**
			 * Messages arrived.
			 **/
			moodycamel::ConcurrentQueue<cam::MessagePointer> m_messages;

		public:
			
			/**
			 * Create a new agent.
			 * @param p_name name of the new agent
			 **/
			Agent(const std::string& p_name);
			
			/**
			 * Nothing to delete.
			 **/
			virtual ~Agent() = default;

			/**
			 * Return id.
			 * @return Id of the agent
			 **/
			const std::string& get_id() const;

			/**
			 * Return name.
			 * @return Name of the agent
			 **/
			const std::string& get_name() const;

			/**
			 * True if using observables.
			 * @return True if using observables
			 **/
			bool is_using_observables() const;

			/**
			 * Get observables.
			 * @return Observables
			 **/
			const std::unordered_map<std::string, std::string>& get_observables() const;

			/**
			 * True is must run setup.
			 * @return True is must run setup
			 **/
			bool is_setup() const;

			/**
			 * Set name.
			 * @param p_name New name 
			 **/
			void set_name(const std::string& p_name);

			/**
			 * Set environment.
			 * @param p_environment The environment
			 **/
			void set_environment(cam::EnvironmentMas* p_environment);

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
			void post(const cam::MessagePointer& p_message);

			/**
			 * Send a new message.
			 * @param p_receiver The name of the receiver
			 * @param p_message The message
			 **/
			void send(const std::string& p_receiver, const json& p_message);

			/**
			 * Send a new message to all agents.
			 * @param p_message The message
			 **/
			void broadcast(const json& p_message);

			/**
			 * Perception filter.
			 * @param p_observed Observed properties
			 * @return True if the agent is observable
			 **/
			virtual bool perception_filter(const std::unordered_map<std::string, std::string>& p_observed) const;
			
			/**
			 * Setup the agent.
			 **/
			virtual void setup();

			/**
			 * Compute see.
			 * @param p_observable_agents The list of observable agents
			 **/
			virtual void see(std::vector<ObservableAgentPointer> p_observable_agents);

			/**
			 * Compute action.
			 * @param p_message The message to compute
			 **/
			virtual void action(const cam::MessagePointer& p_message);
		
			/**
			 * Compute action if there is no message.
			 **/
			virtual void default_action();

			// Delete copy constructor
			Agent(const Agent&) = delete;
			Agent& operator=(Agent&) = delete;
	};
	
	// Agent pointer
	using AgentPointer = std::shared_ptr<cam::Agent>;
}
