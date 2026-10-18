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

#include <cassert>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "util/MPSCQueue.hpp"
#include "Message.h"
#include "transport/Types.h"

/**
 * CPPActressMAS
 */
namespace cam {
	class Environment;

	class Agent;

	template<typename T>
	concept ConceptAgent = std::is_base_of_v<Agent, T>;

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
		friend Environment;

	private:
		/**
		 * Unique ID: 15 base62 characters (see generate_agent_id), never allocated when copied.
		 **/
		AgentId m_id;

		/**
		 * Name of the agent.
		 **/
		std::string m_name;

		/**
		 * The environment where the agent is.
		 **/
		Environment* m_environment;

		/**
		 * True if setup.
		 **/
		bool m_is_setup;

		/**
		 * True if dead.
		 **/
		bool m_is_dead;

		/**
		 * True while a migration of this agent is in progress: the agent does not run, its messages wait.
		 **/
		bool m_in_transit;

		/**
		 * Messages arrived.
		 **/
		MPSCQueue<MessagePointer> m_messages;

	protected:

		/**
		 * True if using observables.
		 **/
		bool m_is_using_observables;

		/**
		 * List of observables.
		 **/
		std::shared_ptr<Observables> m_observables;

	public:
		/**
		 * Create a new agent.
		 * @param p_name Name of the new agent
		 * @param p_using_observables If true, this agent can see
		 **/
		explicit Agent(std::string p_name, bool p_using_observables = false);

		/**
		 * Nothing to delete.
		 **/
		virtual ~Agent() = default;

		/**
		 * Return id.
		 * @return Id of the agent
		 **/
		[[nodiscard]] const AgentId& get_id() const { return m_id; }

		/**
		 * Return name.
		 * @return Name of the agent
		 **/
		[[nodiscard]] const std::string& get_name() const { return m_name; }

		/**
		 * True if using observables.
		 * @return True if using observables
		 **/
		[[nodiscard]] bool is_using_observables() const { return m_is_using_observables; }

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
		 * True while a migration of this agent is in progress.
		 * @return True if in transit
		 **/
		[[nodiscard]] bool is_in_transit() const { return m_in_transit; }

		/**
		 * Run one turn (for one agent)
		 * @param p_run_setup_separately If true, run setup in the same tick as see/act
		 **/
		void run_turn(bool p_run_setup_separately = true);

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
		 * Send a new message by ID. A local receiver is served directly; a receiver hosted by
		 * another environment is reached through the transport at the end of the turn. The
		 * content stays typed locally (text, JSON value or bytes) and is encoded once on the wire.
		 * @param p_receiver_id The id of the receiver
		 * @param p_message The message (bytes)
		 * @param p_length The message size
		 * @param p_options Send options (encoding tag of the bytes, priority)
		 * @return Success, or UnknownDestination
		 **/
		transport::Result<void>
		send(const AgentId& p_receiver_id, const uint8_t* p_message = nullptr, const size_t& p_length = 0,
			 const transport::SendOptions& p_options = {}) const;

		transport::Result<void> send(const AgentId& p_receiver_id, const json& p_message,
									 const transport::SendOptions& p_options = {}) const;

		transport::Result<void> send(const AgentId& p_receiver_id, std::string p_text,
									 const transport::SendOptions& p_options = {}) const;

		transport::Result<void> send(const AgentId& p_receiver_id, const char* p_text,
									 const transport::SendOptions& p_options = {}) const;

		transport::Result<void> send(const AgentId& p_receiver_id, const Text& p_text,
									 const transport::SendOptions& p_options = {}) const;

		/**
		 * Send a new message by name: local agents, and with the network scope every connected environment.
		 * @param p_receiver_name The name of the receiver
		 * @param p_message The message (bytes)
		 * @param p_length The message size
		 * @param p_first_only If true, only the first agent found (per environment)
		 * @param p_options Send options (scope, encoding, priority)
		 * @return Success, or NotConnected when the network scope is asked offline
		 **/
		transport::Result<void>
		send_by_name(const std::string& p_receiver_name, const uint8_t* p_message = nullptr, const size_t& p_length = 0,
					 bool p_first_only = true, const transport::SendOptions& p_options = {}) const;

		transport::Result<void> send_by_name(const std::string& p_receiver_name, const json& p_message, bool p_first_only = true,
											 const transport::SendOptions& p_options = {}) const;

		transport::Result<void> send_by_name(const std::string& p_receiver_name, std::string p_text, bool p_first_only = true,
											 const transport::SendOptions& p_options = {}) const;

		transport::Result<void> send_by_name(const std::string& p_receiver_name, const Text& p_text, bool p_first_only = true,
											 const transport::SendOptions& p_options = {}) const;

		transport::Result<void> send_by_name(const std::string& p_receiver_name, const char* p_text, bool p_first_only = true,
											 const transport::SendOptions& p_options = {}) const;

		/**
		 * Send a new message by fragment name: local agents, and with the network scope every connected environment.
		 * @param p_fragment_name The fragment of the receivers
		 * @param p_message The message (bytes)
		 * @param p_length The message size
		 * @param p_first_only If true, only the first agent found (per environment)
		 * @param p_options Send options (scope, encoding, priority)
		 * @return Success, or NotConnected when the network scope is asked offline
		 **/
		transport::Result<void> send_by_fragment_name(const std::string& p_fragment_name, const uint8_t* p_message = nullptr,
													  const size_t& p_length = 0, bool p_first_only = true,
													  const transport::SendOptions& p_options = {}) const;

		transport::Result<void> send_by_fragment_name(const std::string& p_fragment_name, const json& p_message,
													  bool p_first_only = true, const transport::SendOptions& p_options = {}) const;

		transport::Result<void> send_by_fragment_name(const std::string& p_fragment_name, std::string p_text,
													  bool p_first_only = true, const transport::SendOptions& p_options = {}) const;

		transport::Result<void> send_by_fragment_name(const std::string& p_fragment_name, const char* p_text,
													  bool p_first_only = true, const transport::SendOptions& p_options = {}) const;

		transport::Result<void> send_by_fragment_name(const std::string& p_fragment_name, const Text& p_text,
													  bool p_first_only = true, const transport::SendOptions& p_options = {}) const;

		/**
		 * Send a new message to all agents: local agents except this one, and with the network scope every
		 * agent of the connected environments.
		 * @param p_message The message (bytes)
		 * @param p_length The message size
		 * @param p_options Send options (scope, encoding, priority)
		 * @return Success, or NotConnected when the network scope is asked offline
		 **/
		transport::Result<void> broadcast(const uint8_t* p_message = nullptr, const size_t& p_length = 0,
										  const transport::SendOptions& p_options = {}) const;

		transport::Result<void> broadcast(const json& p_message, const transport::SendOptions& p_options = {}) const;

		transport::Result<void> broadcast(std::string p_text, const transport::SendOptions& p_options = {}) const;

		transport::Result<void> broadcast(const char* p_text, const transport::SendOptions& p_options = {}) const;

		transport::Result<void> broadcast(const Text& p_text, const transport::SendOptions& p_options = {}) const;

		/**
		 * Move to another environment through the transport (three-step migration). The agent type must
		 * be registered on both sides (Environment::register_agent_type). The agent keeps its id, carries
		 * save_state(), its observables and its unconsumed messages, and runs setup() again on arrival.
		 * Until the destination confirms, the agent stays here, inactive; it resumes if the move fails.
		 * @param p_environment_id Destination environment id
		 * @param p_options Send options (priority)
		 * @return Success (move started), or NotConnected, UnknownDestination, UnknownAgentType, InTransit
		 **/
		transport::Result<void> move(const std::string& p_environment_id, const transport::SendOptions& p_options = {});

		/**
		 * State of the agent carried by a migration, as JSON; the default is an empty object (never null).
		 * Derived agents return their own fields here.
		 * @return The state
		 **/
		[[nodiscard]] virtual json save_state() const { return json::object(); }

		/**
		 * Restore the state saved by save_state() on arrival in another environment.
		 * @param p_state The state
		 **/
		virtual void load_state(const json& p_state) { (void) p_state; }

		/**
		 * Call to reset agent i.e. set setup and dead to false
		 **/
		void reset();

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
		[[nodiscard]] std::vector<AgentId>
		get_agents_by_name(const std::string& p_name, bool p_first_only = false) const;

		/**
		 * Get first agent by name
		 * @param p_name the name of agent
		 * @return All agents by name
		 **/
		[[nodiscard]] std::optional<AgentId> get_first_agent_by_name(const std::string& p_name) const;

		/**
		 * Get agent name by id
		 * @param p_id the name of agent
		 * @return Agents name
		 **/
		[[nodiscard]] std::optional<std::string> get_agent_name(const AgentId& p_id) const;

		/**
		 * Get all agents by fragment name
		 * @param p_fragment_name the fragment name of agent
		 * @param p_first_only if true the first found
		 * @return All agents by fragment name
		 **/
		[[nodiscard]] std::vector<AgentId>
		get_filtered_agents(const std::string& p_fragment_name, bool p_first_only = false) const;

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
		virtual void see(const std::vector<ObservablesPointer>& p_observable_agents);

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
		Agent(const Agent&) = delete;

		Agent& operator=(Agent&) = delete;

	protected:

		/**
		 * Get environment
		 * @return The environment
		 */
		Environment* get_environment() const {
			assert(m_environment);
			return m_environment;
		}

	private:

		/**
		 * Set environment.
		 * @param p_environment The environment
		 **/
		void set_environment(Environment* p_environment) { m_environment = p_environment; }

		/**
		 * Give the agent the identity it had before a migration (environment only).
		 * @param p_id The id
		 * @param p_name The name
		 **/
		void set_identity(const AgentId& p_id, std::string p_name) {
			m_id = p_id;
			m_name = std::move(p_name);
		}

		/**
		 * Receive a message built by the environment, moved in: no reference count change (P8).
		 * @param p_message The new message
		 **/
		void receive(MessagePointer&& p_message);

		/**
		 * Take every message waiting in the queue (environment only, turn boundary).
		 * @return The messages, oldest first
		 **/
		std::vector<MessagePointer> take_messages() {
			std::vector<MessagePointer> l_messages;
			MessagePointer l_message;
			while (m_messages.dequeue(l_message)) {
				l_messages.push_back(std::move(l_message));
			}
			return l_messages;
		}
	};

	// Agent pointer
	using AgentPointer = std::shared_ptr<Agent>;
} // namespace cam
