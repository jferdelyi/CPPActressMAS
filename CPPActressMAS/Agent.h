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
#include <cereal/archives/portable_binary.hpp>
#include <cereal/types/vector.hpp>            // Serialize vector
#include <cereal/types/string.hpp>            // Serialize string
#include <cereal/types/unordered_map.hpp>    // Serialize unordered map

#include "MPSCQueue.hpp"
#include "Message.h"

/**
 * CPPActressMAS
 */
namespace cam {
	class EnvironmentMas;

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
		friend EnvironmentMas;

	private:
		/**
		 * Unique ID.
		 **/
		std::string m_id;

		/**
		 * Name of the agent.
		 **/
		std::string m_name;

		/**
		 * The environment where the agent is.
		 **/
		EnvironmentMas* m_environment;

		/**
		 * True if setup.
		 **/
		bool m_is_setup;

		/**
		 * True if dead.
		 **/
		bool m_is_dead;

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
		 * Send a new message by ID.
		 * @param p_receiver_id The id of the receiver
		 * @param p_message The message
		 * @param p_length The message size
		 **/
		void
		send(const std::string& p_receiver_id, const uint8_t* p_message = nullptr, const size_t& p_length = 0) const;

		void send(const std::string& p_receiver_id, const json& p_message) const;

		/**
		 * Send a new message by name.
		 * @param p_receiver_name The name of the receiver
		 * @param p_message The message
		 * @param p_length The message size
		 * @param p_first_only If true, only the first agent found
		 **/
		void
		send_by_name(const std::string& p_receiver_name, const uint8_t* p_message = nullptr, const size_t& p_length = 0,
					 bool p_first_only = true) const;

		void send_by_name(const std::string& p_receiver_name, const json& p_message, bool p_first_only = true) const;

		/**
		 * Send a new message by fragment name.
		 * @param p_fragment_name The fragment of the receivers
		 * @param p_message The message
		 * @param p_length The message size
		 * @param p_first_only If true, only the first agent found
		 **/
		void send_by_fragment_name(const std::string& p_fragment_name, const uint8_t* p_message = nullptr,
								   const size_t& p_length = 0, bool p_first_only = true) const;

		void send_by_fragment_name(const std::string& p_fragment_name, const json& p_message,
								   bool p_first_only = true) const;

		/**
		 * Send a new message to all agents.
		 * @param p_message The message
		 * @param p_length The message size
		 **/
		void broadcast(const uint8_t* p_message = nullptr, const size_t& p_length = 0) const;

		void broadcast(const json& p_message) const;

		/**
		 * Move to another environment.
		 * @param p_environment_id New environment
		 * @param p_message Move message
		 **/
		template<ConceptAgent T>
		void move(const std::string& p_environment_id, const json& p_message) {
			internal_move(serialize_to_stream<T>(), p_message, p_environment_id);
		}

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
		[[nodiscard]] std::vector<std::string>
		get_agents_by_name(const std::string& p_name, bool p_first_only = false) const;

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
		[[nodiscard]] std::vector<std::string>
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

		/**
		 * Serialize agent
		 * @return stream
		 */
		template<ConceptAgent T>
		std::stringstream serialize_to_stream() {
			std::stringstream l_stream;
			cereal::PortableBinaryOutputArchive l_output_archive(l_stream);
			l_output_archive(*dynamic_cast<T*>(this));
			return l_stream;
		}

		/**
		 * Serialize agent
		 * @param p_stream input stream
		 */
		template<ConceptAgent T>
		void deserialize_from_stream(std::stringstream& p_stream) {
			cereal::PortableBinaryInputArchive l_input_archive(p_stream);
			l_input_archive(*dynamic_cast<T*>(this));
		}

		/**
		 * Serialize agent
		 * @param p_archive archive to store agent
		 */
		template<class Archive>
		void save(Archive& p_archive) const {
			std::unordered_map<std::string, std::vector<std::uint8_t>> l_serialized_observables;
			for (const auto& l_observable: *m_observables) {
				l_serialized_observables.emplace(l_observable.first, json::to_msgpack(l_observable.second));
			}
			p_archive(m_id, m_name, m_is_dead, m_is_setup, m_is_using_observables, m_messages,
					  l_serialized_observables);
		}

		/**
		 * Deserialize agent
		 * @param p_archive archive to restore agent
		 */
		template<class Archive>
		void load(Archive& p_archive) {
			std::unordered_map<std::string, std::vector<std::uint8_t>> l_serialized_observables;
			p_archive(m_id, m_name, m_is_dead, m_is_setup, m_is_using_observables, m_messages,
					  l_serialized_observables);
			for (const auto& l_observable: l_serialized_observables) {
				m_observables->emplace(l_observable.first, json::from_msgpack(l_observable.second));
			}
		}

		// Delete copy constructor
		Agent(const Agent&) = delete;

		Agent& operator=(Agent&) = delete;

	protected:

		/**
		 * Constructor by stream
		 */
		explicit Agent();

		/**
		 * Get environment
		 * @param p_archive archive to restore agent
		 */
		EnvironmentMas* get_environment() const {
			assert(m_environment);
			return m_environment;
		}

		/**
		 * Internal move
		  * @param p_agent_stream The agent stream
		 * @param p_message Move message
		 * @param p_environment_id New environment
		 */
		void internal_move(const std::stringstream& p_agent_stream, const json& p_message,
						   const std::string& p_environment_id);

	private:

		/**
		 * Set environment.
		 * @param p_environment The environment
		 **/
		void set_environment(EnvironmentMas* p_environment) { m_environment = p_environment; }
	};

	// Agent pointer
	using AgentPointer = std::shared_ptr<Agent>;
} // namespace cam
