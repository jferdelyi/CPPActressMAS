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

#include <mqtt/message.h>

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

	class PahoWrapper;

	class Callback;

	class SubscribeActionListener;

	/**
	 * An abstract base class for the multiagent environment, where all the agents are executed.
	 **/
	class Environment {
		friend PahoWrapper;
		friend Callback;
		friend SubscribeActionListener;

	private:
		/**
		 * The maximum number of turns of the simulation..
		 **/
		int m_no_turns;

		/**
		 * A delay (in milliseconds) after each turn.
		 **/
		int m_delay_after_turn;

		/**
		 * The agents in the environment
		 **/
		AgentCollection m_agent_collection;

		/**
		 * Random generator
		 **/
		PahoWrapper* m_remote_client;

		/**
		 * Containers
		 **/
		std::set<std::string> m_containers;

		/**
		 * Information that agent can get about the environment
		 **/
		json m_environement_data;

	public:

		/**
		 * Initializes a new instance of the Environment class.
		 *
		 * @param p_no_turns The maximum number of turns of the simulation. Setup is
		 * considered to be the first turn. The simulation may stop earlier if there
		 * are no more agents in the environment. If the number of turns is 0, the
		 * simulation runs indefinitely, or until there are no more agents in the
		 * environment.
		 * @param p_mode Whether agent behaviors are executed in parallel,
		 * sequentially or sequentially with random order. The code of a single agent
		 * in a turn is always executed sequentially.
		 * @param p_delay_after_turn A delay (in milliseconds) after each turn.
		 * @param p_seed A random number generator seed for non-deterministic but
		 * repeatable experiments.
		 **/
		explicit Environment(int p_no_turns = 0, const EnvironmentMasMode& p_mode = EnvironmentMasMode::Parallel,
							 int p_delay_after_turn = 0, unsigned int p_seed = std::time(nullptr));

		/**
		 * Delete PahoWrapper
		 **/
		virtual ~Environment();

		/**
		 * Adds an agent to the environment.
		 * @param p_args Argument to create a new agent
		 * @return Id of the agent
		 **/
		template<ConceptAgent T>
		const std::string& add(auto&& ... p_args) {
			const auto& l_agent = AgentPointer(new T(std::forward<decltype(p_args)>(p_args)...));
			l_agent->set_environment(this);
			m_agent_collection.add(l_agent);
			return l_agent->get_id();
		}

		/**
		 * Adds an agent to the environment from stream.
		 * @param p_stream serialized stream agent
		 * @return Id of the agent
		 **/
		template<ConceptAgent T>
		const std::string& add(std::stringstream& p_stream) {
			const auto& l_agent = AgentPointer(new T());
			l_agent->set_environment(this);
			l_agent->deserialize_from_stream<T>(p_stream);
			l_agent->reset();
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
		 * @param p_from_remote If from remote, do not broadcast remotely
		 **/
		void send(const std::string& p_sender_id, const std::string& p_receiver_id, const uint8_t* p_message,
				  const size_t& p_length, const MessageBinaryFormat& p_binary_format = MessageBinaryFormat::RAW,
				  bool p_from_remote = false) const;

		/**
		 * Sends a message by name.
		 * @param p_sender_id The sender ID
		 * @param p_receiver_name The receiver name
		 * @param p_message The message to be sent
		 * @param p_length The message length
		 * @param p_is_fragment If true search all agent that the name contain "p_receiver_name"
		 * @param p_first_only If true send to the first agent found
		 * @param p_binary_format The message binary format
		 * @param p_from_remote If from remote, do not broadcast remotely
		 **/
		void send_by_name(const std::string& p_sender_id, const std::string& p_receiver_name, const uint8_t* p_message,
						  const size_t& p_length, bool p_is_fragment, bool p_first_only,
						  const MessageBinaryFormat& p_binary_format = MessageBinaryFormat::RAW,
						  bool p_from_remote = false) const;

		/**
		 * Send a new message to all agents.
		 * @param p_sender_id From
		 * @param p_message The message
		 * @param p_length The message length
		 * @param p_binary_format The message binary format
		 * @param p_from_remote If from remote, do not broadcast remotely
		 **/
		void broadcast(const std::string& p_sender_id, const uint8_t* p_message, const size_t& p_length,
					   const MessageBinaryFormat& p_binary_format = MessageBinaryFormat::RAW,
					   bool p_from_remote = false) const;

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
		 * Get id
		 * @return ID if remote if initialized, empty string otherwise
		 **/
		[[nodiscard]] std::string get_id() const;

		/**
		 * Get data about the environement
		 * @return data about the environement
		 **/
		[[nodiscard]] json get_global_data() const {
			return m_environement_data;
		}

		/**
		 * Set data about the environement
		 * @param p_key key of the data
		 * @param p_value value of the data
		 **/
		void set_global_data(const std::string& p_key, const json& p_value) {
			m_environement_data[p_key] = p_value;
		}

		/**
		 * Remove data about the environement
		 * @param p_key key of the data
		 **/
		void remove_global_data(const std::string& p_key) {
			m_environement_data.erase(p_key);
		}

		/**
		 * Move agent.
		 * @param p_agent_stream The agent stream
		 * @param p_message Move message
		 * @param p_environment_id New environment
		 **/
		void move(const std::stringstream& p_agent_stream, const json& p_message, const std::string& p_environment_id);

		/**
		 * Get id
		 * @return ID if remote if initialized, empty string otherwise
		 **/
		[[nodiscard]] const std::set<std::string>& get_containers() const { return m_containers; }

		/**
		 * A method that may be optionally overridden to perform additional
		 * processing after the simulation has finished.
		 **/
		virtual void simulation_finished() {}

		/**
		 * A method that may be optionally overridden to perform additional
		 * processing after a turn of the simulation has finished.
		 * @param p_turn The turn that has just finished
		 **/
		virtual void turn_finished(int) {}

		/**
		 * Get the list of observable agents for an agent and its perception filter.
		 * @param p_perceiving_agent Perceiving agent
		 **/
		std::vector<const ObservablesPointer> get_list_of_observable_agents(const Agent* p_perceiving_agent) const;

		//###############################################################
		//	MQTT handlers
		//###############################################################

		/**
		 * Create a new paho client instance
		 * @param p_id string to use as the base of the client id (p_id + _ + UUID).
		 * @param p_full_address the full connection address
		 * @param p_connection_options connection options (username, password, ...)
		 * @return the reason code, if something wrong happen. 0 = OK (see https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901031)
		 */
		int initialise_remote_connection_by_address(const std::string& p_id,
													const std::string& p_full_address = "tcp://127.0.0.1:1883",
													const json& p_connection_options = "");

		/**
		 * Create a new paho client instance
		 * @param p_id string to use as the base of the client id (p_id + _ + UUID).
		 * @param p_host the hostname or ip address of the broker to connect to
		 * @param p_port the network port to connect to. Usually 1883
		 * @param p_connection_options connection options (username, password, ...)
		 * @return the reason code, if something wrong happen. 0 = OK (see https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901031)
		  */
		int initialise_remote_connection(const std::string& p_id = "",
										 const std::string& p_host = "127.0.0.1",
										 int p_port = 1883,
										 const json& p_connection_options = "");

		// Delete copy constructor
		Environment(const Environment&) = delete;

		Environment& operator=(Environment&) = delete;

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
		void run_turn(int p_turn);

		//###############################################################
		//	Remote handlers
		//###############################################################

		/**
		 * Handler on_connect
		 * @param p_rc the reason code, if something wrong happen. 0 = OK (see https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901031)
		 */
		virtual void on_connect(int) {}

		/**
		 * Handler on_disconnect
		 */
		virtual void on_disconnect() {}

		/**
		 * Handler on_publish
		 * @param p_mid the message id of the subscribe message
		 */
		virtual void on_publish(int) {}

		/**
		 * Handler on_message
		 * @param p_message the received message
		 * @param p_topic the topic
		 */
		virtual void on_message(const json&, const std::string&) {}

		/**
		 * Handler on_new_agent
		 * @param p_json_message JSON message
		 */
		virtual void on_new_agent(const json& p_json_message);

		/**
		 * Handler on_new_environment
		 * @param p_environment_id environment ID
		 * @param p_send_back if true, then send back
		 */
		virtual void on_new_environment(const std::string& p_environment_id, bool p_send_back);

		/**
		 * Handler on_subscribe
		 * @param p_mid the message id of the subscribe message
		 * @param p_topic the topic
		 */
		virtual void on_subscribe(int, const std::string&) {}

		/**
		 * Handler on_unsubscribe
		 * @param p_mid the message id of the subscribe message
		 * @param p_topic the topic
		 */
		virtual void on_unsubscribe(int, const std::string&) {}

		/**
		 * Handler on_log
		 * @param p_message the message string
		 */
		virtual void on_log(const std::string&) {}

		/**
		 * Handler on_subscribe_error
		 * @param p_message the message string
		 * @param p_rc the reason code, if something wrong happen. 0 = OK (see https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901031)
		 */
		virtual void on_subscribe_error(const std::string&, int) {}

		/**
		 * Handler on_connection_error
		 * @param p_message the message string
		 * @param p_rc the reason code, if something wrong happen. 0 = OK (see https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901031)
		 */
		virtual void on_connection_error(const std::string&, int) {}

		/**
		 * Handler on_connection_success
		 * @param p_message the message string
		 * @param p_rc the reason code, if something wrong happen. 0 = OK (see https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901031)
		 */
		virtual void on_connection_success(const std::string&, int) {}
	};

	// Environment pointer
	//using EnvironmentPointer = std::shared_ptr<Environment>;
} // namespace cam
