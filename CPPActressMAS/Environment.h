/**************************************************************************
 *                                                                        *
 *  Description: CPPActressMas multi-agent framework                      *
 *  Website:     https://github.com/jferdelyi/CPPActressMAS               *
 *  Copyright:   (c) 2023-Today, Jean-François Erdelyi                    *
 *                                                                        *
 *  CPP version of ActressMAS by Florin Leon                              *
 *  https://github.com/florinleon/ActressMas                              *
 *                                                                        *
 *  This program and the accompanying materials are made available under  *
 *  the terms of the Eclipse Public License 2.0, which is available at    *
 *  https://www.eclipse.org/legal/epl-2.0/ and in the LICENSE file.       *
 *                                                                        *
 *  SPDX-License-Identifier: EPL-2.0                                      *
 *                                                                        *
 **************************************************************************/

#pragma once

#include <algorithm>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

#include "Agent.h"
#include "AgentCollection.h"
#include "AggregateException.h"
#include "AgentTypes.h"
#include "Message.h"
#include "transport/Endpoint.h"
#include "util/RecyclingAllocator.hpp"

/**
 * CPPActressMAS
 **/
namespace cam {

	/**
	 * Environments execution modes.
	 **/
	enum class EnvironmentMasMode {
		Parallel,
		Sequential,
		SequentialRandom
	};

	/**
	 * Generator of the random order and of random_agent: Mt19937 draws the same numbers with every standard
	 * library; DotNet draws as the seeded System.Random of .NET (seeded with the int value of the seed), so that a port
	 * follows the order of the C# exactly.
	 **/
	enum class RandomEngine {
		Mt19937,
		DotNet
	};

	class Agent;

	/**
	 * Configuration of an environment, written with designated initializers:
	 * cam::Environment l_environment({.m_turns = 100, .m_mode = cam::EnvironmentMasMode::Sequential});
	 * m_turns: maximum number of turns, 0 for no limit; m_delay_after_turn: in milliseconds; m_seed: seed of the
	 * random order, drawn from std::random_device when absent; m_random: its generator; m_url: transport URL, the
	 * environment connects through it at its first start() (offline when empty); m_endpoint: name and limits of that
	 * connection; m_accepted_agent_types: wire names of the agent types that may arrive here by migration (every type
	 * of the catalogue of the process when absent).
	 **/
	struct EnvironmentConfig {
		/**
		 * Maximum number of turns, 0 for no limit.
		 **/
		int m_turns = 0;

		/**
		 * Execution mode of the turns.
		 **/
		EnvironmentMasMode m_mode = EnvironmentMasMode::Parallel;

		/**
		 * Delay after each turn, in milliseconds.
		 **/
		int m_delay_after_turn = 0;

		/**
		 * Seed of the random order, drawn from std::random_device when absent.
		 **/
		std::optional<unsigned int> m_seed{};

		/**
		 * Generator of the random order and of random_agent.
		 **/
		RandomEngine m_random = RandomEngine::Mt19937;

		/**
		 * Transport URL: the environment connects through it at its first start() (offline when empty).
		 **/
		std::string m_url{};

		/**
		 * Name and limits of that connection.
		 **/
		transport::EndpointConfig m_endpoint{};

		/**
		 * Wire names of the agent types that may arrive by migration (every type of the catalogue when absent).
		 **/
		std::optional<std::vector<std::string>> m_accepted_agent_types{};
	};

	/**
	 * An abstract base class for the multiagent environment, where all the agents are executed.
	 * Threads: the public functions that add, remove, send or search may be called from any thread, even while start()
	 * or continue_simulation() runs in another one (a service fed from outside); they read the agents under a shared lock,
	 * which the turn boundaries take exclusively, as the ConcurrentDictionary of the C# in Parallel mode. The agents call
	 * them during their turn without that lock.
	 **/
	class Environment {
		friend Agent;

	public:
		/**
		 * Builds a transport from a URL: the default one knows the schemes compiled in this build
		 * (loopback://, and mqtt://, mqtts://, zenoh:// behind their CMake options); a custom one can
		 * add schemes of its own and fall back on cam::transport::open for the others.
		 **/
		using TransportFactory = std::function<transport::Result<std::shared_ptr<transport::Transport>>(const std::string& p_url, transport::TransportConfig p_config)>;

	private:
		/**
		 * A move asked by an agent during a turn, handled at the end of the turn.
		 **/
		struct MoveRequest {
			/**
			 * Agent that moves.
			 **/
			AgentId m_agent_id;

			/**
			 * Environment it moves to.
			 **/
			std::string m_target;

			/**
			 * Priority of the move, copied to its offer (reserved, 0 today).
			 **/
			std::uint32_t m_priority = 0;
		};

		/**
		 * A migration started here, waiting for the destination.
		 **/
		struct Transit {
			/**
			 * Agent on the move.
			 **/
			AgentId m_agent_id;

			/**
			 * Environment it moves to.
			 **/
			std::string m_target;

			/**
			 * When the move fails without an answer (EndpointConfig::m_migration_timeout).
			 **/
			std::chrono::steady_clock::time_point m_deadline;
			std::vector<MessagePointer> m_carried;  ///< messages sent with the offer, given back if the move fails
		};

		/**
		 * An agent received here, waiting for the confirmation of its origin.
		 **/
		struct PendingArrival {
			/**
			 * The agent received, not inserted yet.
			 **/
			AgentPointer m_agent;

			/**
			 * Environment it comes from.
			 **/
			std::string m_origin;

			/**
			 * When its migration fails without the confirmation of its origin (EndpointConfig::m_accept_hold_timeout).
			 **/
			std::chrono::steady_clock::time_point m_deadline;
		};

		/**
		 * An agent that left: messages still arriving for it are forwarded for a few turns.
		 **/
		struct MovedAgent {
			/**
			 * Its new environment.
			 **/
			std::string m_host;

			/**
			 * Turn boundary when it left.
			 **/
			std::uint64_t m_boundary;
		};

		/**
		 * Unique id of the environment (UUID), the same online and offline.
		 **/
		std::string m_id;

		/**
		 * The maximum number of turns of the simulation.
		 **/
		int m_no_turns;

		/**
		 * A delay (in milliseconds) after each turn.
		 **/
		int m_delay_after_turn;

		/**
		 * The agents in the environment.
		 **/
		AgentCollection m_agent_collection;

		/**
		 * Information that agent can get about the environment.
		 **/
		json m_environment_data;

		/**
		 * Side of this environment on its transport; null while offline.
		 **/
		std::unique_ptr<transport::Endpoint> m_endpoint;

		/**
		 * Transport URL of this environment (empty: none).
		 **/
		std::string m_transport_url;

		/**
		 * Factory that opens the transport URL (empty: the default one).
		 **/
		TransportFactory m_transport_factory;

		/**
		 * Connection asked by the configuration (m_url), made at the first start(); dropped once connected or after an
		 * explicit disconnect().
		 **/
		bool m_connect_on_start = false;

		/**
		 * Endpoint configuration of that connection.
		 **/
		transport::EndpointConfig m_endpoint_config;

		/**
		 * Wire names of the agent types that may arrive here (every type of AgentTypes when absent).
		 **/
		std::optional<std::vector<std::string>> m_accepted_agent_types;

		/**
		 * Guards m_move_requests.
		 **/
		std::mutex m_move_mutex;

		/**
		 * Moves asked during the turn (any thread), handled at the end of the turn.
		 **/
		std::vector<MoveRequest> m_move_requests;

		/**
		 * Migrations started here and in progress, by transfer id.
		 **/
		std::unordered_map<std::string, Transit> m_transits;

		/**
		 * Migrations received here and waiting for the confirmation of their origin, by transfer id.
		 **/
		std::unordered_map<std::string, PendingArrival> m_arrivals;

		/**
		 * Agents that left, with their new host, for the forwarding grace period.
		 **/
		std::unordered_map<AgentId, MovedAgent> m_moved_agents;

		/**
		 * Number of turn boundaries seen (forwarding grace is counted in boundaries).
		 **/
		std::uint64_t m_boundary;

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
							 int p_delay_after_turn = 0, unsigned int p_seed = std::random_device()());

		/**
		 * Initializes a new instance of the Environment class from its configuration, for instance
		 * cam::Environment l_environment({.m_turns = 10, .m_mode = cam::EnvironmentMasMode::Sequential}).
		 * @param p_config The configuration
		 **/
		explicit Environment(const EnvironmentConfig& p_config);

		/**
		 * Disconnects from the transport, if connected.
		 **/
		virtual ~Environment();

		/**
		 * Adds an agent to the environment, as the immediate Add of the C#: it is at once reached by every send and search,
		 * counted by agents_count() and drawn by random_agent(), and it runs from the next turn (inserted at the next
		 * turn boundary).
		 * @param p_args Argument to create a new agent
		 * @return Id of the agent, a copy that stays valid after the agent is gone
		 **/
		template<ConceptAgent T>
		AgentId add(auto&& ... p_args) {
			static const bool s_catalogued = AgentTypes::catalogue<T>();  // once per type: its wire name for migration
			(void) s_catalogued;
			const AgentPointer l_agent = make_agent<T>(std::forward<decltype(p_args)>(p_args)...);
			l_agent->set_environment(this);
			m_agent_collection.add(l_agent);
			return l_agent->get_id();
		}

		/**
		 * Adds an agent built by the caller (for instance restored with load_state()).
		 * @param p_agent The agent
		 * @return Id of the agent
		 **/
		AgentId add(AgentPointer p_agent);

		/**
		 * Continues the simulation for an additional number of turns, after an
		 * initial simulation has finished. The simulation may stop earlier if there
		 * are no more agents in the environment. If the number of turns is 0, the
		 * simulation runs indefinitely, or until there are no more agents in the
		 * environment. An exception thrown by an agent leaves it as it leaves start().
		 *
		 * @param p_no_turns The maximum number of turns of the continued simulation
		 **/
		void continue_simulation(int p_no_turns = 0);

		/**
		 * Stops the execution of the agent identified by id and removes it from the
		 * environment. Use the Remove method instead of Agent.Stop when the decision
		 * to stop an agent does not belong to the agent itself, but to some other
		 * agent or to an external factor.
		 * During a turn the agent is no longer counted, found by name nor sent to, and it is destroyed at the turn
		 * boundary (in Parallel mode it still runs in this turn if it has not yet, as the C#); while a simulation runs,
		 * from any thread, it is marked the same way and destroyed at the next boundary; outside a simulation it is
		 * removed at once. An agent added but not yet inserted is removed at once, as the C#. An unknown id is ignored.
		 *
		 * @param p_agent_id The id of the agent to be removed
		 **/
		void remove(const AgentId& p_agent_id);

		/**
		 * Sends a message from the outside of the multiagent system. Whenever
		 * possible, the agents should use the Send method of their own class, not
		 * the Send method of the environment. This method can also be used to
		 * simulate a forwarding behavior. A local receiver is served directly; a
		 * receiver known from the directory of another environment is reached through
		 * the transport at the end of the turn, whatever the scope.
		 * @param p_sender_id The sender ID
		 * @param p_receiver_id The receiver ID
		 * @param p_content The content (text, JSON value or bytes), moved into the message of a local receiver
		 * @param p_options Send options (encoding of a JSON value on the wire; scope and priority are not used by id)
		 * @return Success, or UnknownDestination when no environment hosts the receiver
		 **/
		transport::Result<void> send_content(const AgentId& p_sender_id, const AgentId& p_receiver_id,
											 MessageContent p_content, const transport::SendOptions& p_options = {}) const;

		/**
		 * Sends bytes by id (see send_content).
		 * @param p_sender_id The sender ID
		 * @param p_receiver_id The receiver ID
		 * @param p_message The bytes
		 * @param p_length The number of bytes
		 * @param p_options Send options
		 * @return Success, or UnknownDestination when no environment hosts the receiver
		 **/
		transport::Result<void> send(const AgentId& p_sender_id, const AgentId& p_receiver_id, const uint8_t* p_message,
									 const size_t& p_length, const transport::SendOptions& p_options = {}) const;

		/**
		 * Sends a message by name: to the local agents, then, with the network scope,
		 * to every connected environment (first_only applies per environment).
		 * @param p_sender_id The sender ID
		 * @param p_receiver_name The receiver name
		 * @param p_content The content (text, JSON value or bytes), moved into the message of the last local receiver
		 * @param p_is_fragment If true search all agent that the name contain "p_receiver_name"
		 * @param p_first_only If true send to the first agent found
		 * @param p_options Send options (scope, encoding of a JSON value on the wire)
		 * @return Success, or NotConnected when the network scope is asked offline
		 **/
		transport::Result<void> send_content_by_name(const AgentId& p_sender_id, const std::string& p_receiver_name,
													 MessageContent p_content, bool p_is_fragment, bool p_first_only,
													 const transport::SendOptions& p_options = {}) const;

		/**
		 * Sends bytes by name (see send_content_by_name).
		 * @param p_sender_id The sender ID
		 * @param p_receiver_name The receiver name
		 * @param p_message The bytes
		 * @param p_length The number of bytes
		 * @param p_is_fragment If true search all agent that the name contain "p_receiver_name"
		 * @param p_first_only If true send to the first agent found
		 * @param p_options Send options (scope)
		 * @return Success, or NotConnected when the network scope is asked offline
		 **/
		transport::Result<void> send_by_name(const AgentId& p_sender_id, const std::string& p_receiver_name, const uint8_t* p_message,
											 const size_t& p_length, bool p_is_fragment, bool p_first_only,
											 const transport::SendOptions& p_options = {}) const;

		/**
		 * Send a new message to all agents: the local ones except the sender, then,
		 * with the network scope, every agent of the connected environments.
		 * @param p_sender_id From
		 * @param p_content The content (text, JSON value or bytes), moved into the message of the last local receiver
		 * @param p_options Send options (scope, encoding of a JSON value on the wire)
		 * @return Success, or NotConnected when the network scope is asked offline
		 **/
		transport::Result<void> broadcast_content(const AgentId& p_sender_id, MessageContent p_content,
												  const transport::SendOptions& p_options = {}) const;

		/**
		 * Broadcast bytes (see broadcast_content).
		 * @param p_sender_id From
		 * @param p_message The bytes
		 * @param p_length The number of bytes
		 * @param p_options Send options (scope)
		 * @return Success, or NotConnected when the network scope is asked offline
		 **/
		transport::Result<void> broadcast(const AgentId& p_sender_id, const uint8_t* p_message, const size_t& p_length,
										  const transport::SendOptions& p_options = {}) const;

		/**
		 * Starts the simulation. An exception thrown by an agent leaves it, as in the C#: at once in the Sequential modes,
		 * and in Parallel mode once every agent of the turn has run, together with those of the other agents of the turn,
		 * in an AggregateException; turn_finished() and simulation_finished() are not called then.
		 **/
		void start();

		/**
		 * The number of agents in the environment, the stopped or removed ones excluded.
		 **/
		[[nodiscard]] size_t agents_count() const;

		/**
		 * Get the agents of a name, the stopped or removed ones excluded.
		 * @param p_name The name
		 * @param p_first_only If true, the first one found only
		 * @return Their ids
		 **/
		[[nodiscard]] std::vector<AgentId>
		get_agents_by_name(const std::string& p_name, bool p_first_only = false) const;

		/**
		 * Get the first agent of a name.
		 * @param p_name The name
		 * @return Its id, or nothing
		 **/
		[[nodiscard]] std::optional<AgentId> get_first_agent_by_name(const std::string& p_name) const;

		/**
		 * Get the name of an agent.
		 * @param p_id The agent id
		 * @return Its name, or nothing
		 **/
		[[nodiscard]] std::optional<std::string> get_agent_name(const AgentId& p_id) const;

		/**
		 * Get all agents by fragment name (the stopped or removed ones excluded).
		 * @param p_fragment_name the fragment name of agent
		 * @param p_first_only if true the first found
		 * @return All agents by fragment name
		 **/
		[[nodiscard]] std::vector<AgentId>
		get_filtered_agents(const std::string& p_fragment_name, bool p_first_only = false) const;

		/**
		 * Returns the id of a randomly selected agent from the environment: one draw of the generator of this
		 * environment over the agents in their order, as RandomAgent of the C# (the stopped or removed ones excluded).
		 * Any agent may come out, the one that calls included: a caller that wants another kind of agent draws again,
		 * as the manager of the C# example E04 does.
		 * @return The id, or nothing when no agent is left (the C# throws)
		 **/
		[[nodiscard]] std::optional<AgentId> random_agent() const;

		/**
		 * Seed of the random generator of this environment (given, or drawn when absent).
		 * @return The seed
		 **/
		[[nodiscard]] unsigned int seed() const { return m_agent_collection.m_seed; }

		/**
		 * Get the id of the environment (UUID), online or offline.
		 * @return The id
		 **/
		[[nodiscard]] const std::string& get_id() const { return m_id; }

		/**
		 * Get the data of the environment, shared with its agents.
		 * @return The data
		 **/
		[[nodiscard]] json get_global_data() const {
			return m_environment_data;
		}

		/**
		 * Set a data of the environment.
		 * @param p_key The key
		 * @param p_value The value
		 **/
		void set_global_data(const std::string& p_key, const json& p_value) {
			m_environment_data[p_key] = p_value;
		}

		/**
		 * Remove a data of the environment.
		 * @param p_key The key
		 **/
		void remove_global_data(const std::string& p_key) {
			m_environment_data.erase(p_key);
		}

		//###############################################################
		//	Transport
		//###############################################################

		/**
		 * Remember the transport URL of this environment without connecting; connect() uses it.
		 * @param p_url The URL ("loopback://sim", "zenoh://127.0.0.1:7447/sim", "mqtt://host:1883/sim"...)
		 **/
		void set_transport_url(std::string p_url) { m_transport_url = std::move(p_url); }

		/**
		 * The transport URL of this environment, empty when none was given.
		 * @return The URL
		 **/
		[[nodiscard]] const std::string& transport_url() const { return m_transport_url; }

		/**
		 * Replace the factory that turns the transport URL into a transport (extension point: a
		 * scheme of your own). An empty factory restores the default one.
		 * @param p_factory The factory
		 **/
		void set_transport_factory(TransportFactory p_factory) { m_transport_factory = std::move(p_factory); }

		/**
		 * Connect the environment through its transport URL (set_transport_url). An environment stays
		 * offline until this is called; without URL it refuses (InvalidConfiguration).
		 * @return Success, or an error (no URL, unknown scheme, transport not compiled in, broker unreachable...)
		 **/
		transport::Result<void> connect();

		/**
		 * Remember the transport URL and connect through it. Called by the simulation thread, outside a
		 * turn (before start() or in turn_finished()).
		 * @param p_url The URL
		 * @param p_config The endpoint configuration (name, capacities, delays)
		 * @return Success, or an error
		 **/
		transport::Result<void> connect(const std::string& p_url, transport::EndpointConfig p_config = {});

		/**
		 * Connect the environment to a transport built elsewhere: attach an endpoint, announce the
		 * presence of the environment and learn the environments already there.
		 * Called by the simulation thread, outside a turn (before start() or in
		 * turn_finished()). An environment stays offline until this is called.
		 * @param p_transport The transport (shared by the environments of the process)
		 * @param p_config The endpoint configuration (name, capacities, delays)
		 * @return Success, or an error (already connected, transport closed...)
		 **/
		transport::Result<void> connect(std::shared_ptr<transport::Transport> p_transport, transport::EndpointConfig p_config = {});

		/**
		 * Disconnect from the transport: flush the pending envelopes, announce the
		 * departure and detach. Called by the simulation thread, outside a turn.
		 * @return Success, or NotConnected
		 **/
		transport::Result<void> disconnect();

		/**
		 * True if connected to a transport.
		 * @return True if connected
		 **/
		[[nodiscard]] bool is_connected() const { return m_endpoint != nullptr; }

		/**
		 * The other environments known through the transport (snapshot of values).
		 * @return The environments, empty offline
		 **/
		[[nodiscard]] std::vector<transport::EnvironmentInfo> environments() const;

		/**
		 * Counters of the transport side of this environment.
		 * @return The statistics, zero offline
		 **/
		[[nodiscard]] transport::Statistics statistics() const;

		/**
		 * Give the agent type T a stable wire name in the catalogue of the process (AgentTypes) and accept it here
		 * when this environment has a list of accepted types. T is built as T(const std::string& name), then
		 * load_state() restores it. Without it, add<T>() already names T automatically and CAM_AGENT_TYPE(T) gives a
		 * stable name to a program that only receives T.
		 * @param p_type_name The wire name of the type (the same on every environment)
		 **/
		template<ConceptAgent T>
		void register_agent_type(const std::string& p_type_name) {
			AgentTypes::add(std::type_index(typeid(T)), p_type_name,
							[](const std::string& p_name) -> AgentPointer { return make_agent<T>(p_name); }, true);
			if (m_accepted_agent_types && std::find(m_accepted_agent_types->begin(), m_accepted_agent_types->end(), p_type_name) == m_accepted_agent_types->end()) {
				m_accepted_agent_types->push_back(p_type_name);
			}
		}

		/**
		 * True if agents of this wire name may arrive here: known by the catalogue and accepted here.
		 * @param p_type_name The wire name
		 * @return True if registered
		 **/
		[[nodiscard]] bool is_agent_type_registered(const std::string& p_type_name) const;

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
		const std::vector<cam::ObservablesPointer> get_list_of_observable_agents(const Agent* p_perceiving_agent) const;

		// Delete copy constructor
		Environment(const Environment&) = delete;

		Environment& operator=(const Environment&) = delete;

	protected:

		/**
		 * Stops the execution of the agent and removes it from the environment. Use
		 * the Remove method instead of Agent.Stop when the decision to stop an agent
		 * does not belong to the agent itself, but to some other agent or to an
		 * external factor. Same rules as remove(const AgentId&); a null agent is ignored.
		 *
		 * @param p_agent The agent to be removed
		 **/
		void remove(const AgentPointer& p_agent);

		/**
		 * Get an agent.
		 * @param p_id Agent ID
		 * @return Agent pointer
		 **/
		AgentPointer get(const AgentId& p_id) const;

		/**
		 * Run one turn.
		 * @param p_turn The current turn
		 **/
		void run_turn(int p_turn);

		/**
		 * Run the turns of start() or continue_simulation(), numbered from 0, with a turn boundary before each one.
		 * @param p_no_turns The maximum number of turns, 0 for no limit
		 **/
		void run_turns(int p_no_turns);

		//###############################################################
		//	Transport handlers (simulation thread, turn boundary)
		//###############################################################

		/**
		 * Handler called when the connection of the transport changes (connected, lost, recovered).
		 * @param p_event The event
		 **/
		virtual void on_transport_event(transport::TransportEvent) {}

		/**
		 * Handler called when another environment appears on the transport.
		 * @param p_environment The environment
		 **/
		virtual void on_environment_joined(const transport::EnvironmentInfo&) {}

		/**
		 * Handler called when another environment leaves the transport, or when its node dies.
		 * @param p_environment The environment
		 **/
		virtual void on_environment_left(const transport::EnvironmentInfo&) {}

		/**
		 * Handler called when an envelope could not be published at the end of a turn, or when a
		 * migration started here failed (refused, timed out: the agent resumed here).
		 * @param p_error The error
		 **/
		virtual void on_delivery_failure(const transport::Error&) {}

	private:

		/**
		 * Mark an agent dead (stop or removal), once: it is no longer counted, found by name nor sent to, and the turn
		 * boundary erases it. Called from any thread of a turn.
		 * @param p_agent The agent
		 **/
		void retire(Agent& p_agent);

		/**
		 * Make an agent: one block for the agent and its counter, recycled per thread, unless its type needs more than
		 * the default alignment of new.
		 * @param p_args Arguments of its constructor
		 * @return The agent
		 **/
		template<ConceptAgent T>
		static std::shared_ptr<T> make_agent(auto&& ... p_args) {
			if constexpr (alignof(T) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__) {
				return std::allocate_shared<T>(RecyclingAllocator<T>(), std::forward<decltype(p_args)>(p_args)...);
			} else {
				return std::make_shared<T>(std::forward<decltype(p_args)>(p_args)...);
			}
		}

		/**
		 * Turn boundary: apply what the transport delivered, then add the new agents
		 * and remove the dead ones (publishing the directory events).
		 **/
		void process_buffers();

		/**
		 * Apply one envelope received from the transport.
		 * @param p_envelope The envelope
		 **/
		void apply_incoming(transport::Envelope&& p_envelope);

		/**
		 * Send a message of an agent by id (see the public send_content): the message keeps the name of its sender.
		 * @param p_sender_id Id of the sending agent
		 * @param p_sender_name Name of the sender; empty when unknown
		 * @param p_receiver_id Id of the receiver
		 * @param p_content The content
		 * @param p_options Send options (scope, encoding, priority)
		 * @return Success, or NotConnected when the network scope is asked offline
		 **/
		transport::Result<void> send_content(const AgentId& p_sender_id, const std::string& p_sender_name,
											 const AgentId& p_receiver_id, MessageContent p_content,
											 const transport::SendOptions& p_options) const;

		/**
		 * Send a message of an agent by name (see the public send_content_by_name): the message keeps the name of its
		 * sender.
		 * @param p_sender_id Id of the sending agent
		 * @param p_sender_name Name of the sender; empty when unknown
		 * @param p_receiver_name Name, or fragment of name, of the receivers
		 * @param p_content The content
		 * @param p_is_fragment If true, every agent whose name contains p_receiver_name
		 * @param p_first_only If true, the first matching agent only
		 * @param p_options Send options (scope, encoding, priority)
		 * @return Success, or NotConnected when the network scope is asked offline
		 **/
		transport::Result<void> send_content_by_name(const AgentId& p_sender_id, const std::string& p_sender_name,
													 const std::string& p_receiver_name, MessageContent p_content, bool p_is_fragment,
													 bool p_first_only, const transport::SendOptions& p_options) const;

		/**
		 * Broadcast a message of an agent (see the public broadcast_content): the messages keep the name of their sender.
		 * @param p_sender_id Id of the sending agent
		 * @param p_sender_name Name of the sender; empty when unknown
		 * @param p_content The content
		 * @param p_options Send options (scope, encoding, priority)
		 * @return Success, or NotConnected when the network scope is asked offline
		 **/
		transport::Result<void> broadcast_content(const AgentId& p_sender_id, const std::string& p_sender_name,
												  MessageContent p_content, const transport::SendOptions& p_options) const;

		/**
		 * The living agents whose name matches, in their order, the agents added and not inserted yet last; without the
		 * lock (an agent during a turn, or the simulation thread).
		 * @param p_name The name, or a fragment of it
		 * @param p_is_fragment If true, the names that contain p_name
		 * @param p_first_only If true, the first one only
		 * @return Their ids
		 **/
		[[nodiscard]] std::vector<AgentId> find_agents(const std::string& p_name, bool p_is_fragment, bool p_first_only) const;

		/**
		 * The name of an agent, added and not inserted yet included; without the lock (an agent during a turn, or the
		 * simulation thread).
		 * @param p_id The agent id
		 * @return The name, or nothing
		 **/
		[[nodiscard]] std::optional<std::string> find_agent_name(const AgentId& p_id) const;

		/**
		 * The number of agents, added and not inserted yet included; without the lock (an agent during a turn, or the
		 * simulation thread).
		 * @return The number of agents
		 **/
		[[nodiscard]] size_t count_agents() const { return m_agent_collection.count(); }

		/**
		 * Deliver a message to a local agent by id; nothing leaves the environment.
		 * @param p_sender_id Id of the sending agent
		 * @param p_sender_name Name of the sender; empty when unknown
		 * @param p_receiver_id Id of the receiver
		 * @param p_content The content, moved into the message
		 * @param p_sender_environment Origin environment of a message received through the transport
		 * @return True if the agent exists here
		 **/
		bool deliver_local(const AgentId& p_sender_id, const std::string& p_sender_name, const AgentId& p_receiver_id,
						   MessageContent p_content, const std::string& p_sender_environment = {}) const;

		/**
		 * Deliver a message to the local agents matching a name or a fragment.
		 * @param p_sender_id Id of the sending agent
		 * @param p_sender_name Name of the sender; empty when unknown
		 * @param p_receiver_name Name, or fragment of name, of the receivers
		 * @param p_content The content, copied for every receiver but the last, which takes it
		 * @param p_is_fragment If true, every agent whose name contains p_receiver_name
		 * @param p_first_only If true, the first matching agent only
		 * @param p_sender_environment Origin environment of a message received through the transport
		 * @return True if at least one agent received it
		 **/
		bool deliver_by_name(const AgentId& p_sender_id, const std::string& p_sender_name,
							 const std::string& p_receiver_name, MessageContent p_content, bool p_is_fragment, bool p_first_only,
							 const std::string& p_sender_environment = {}) const;

		/**
		 * Deliver a message to every local agent but the sender.
		 * @param p_sender_id Id of the sending agent
		 * @param p_sender_name Name of the sender; empty when unknown
		 * @param p_content The content, copied for every receiver but the last, which takes it
		 * @param p_sender_environment Origin environment of a message received through the transport
		 **/
		void deliver_broadcast(const AgentId& p_sender_id, const std::string& p_sender_name, MessageContent p_content,
							   const std::string& p_sender_environment = {}) const;

		/**
		 * Build a message for a local queue.
		 * @param p_sender_id Id of the sending agent
		 * @param p_sender_name Name of the sender; empty when unknown
		 * @param p_receiver_id Id of the receiver
		 * @param p_content The content, moved into the message (a reference: one move less per message)
		 * @param p_sender_environment Origin environment of a message received through the transport
		 * @return The message
		 **/
		static MessagePointer make_message(const AgentId& p_sender_id, const std::string& p_sender_name,
										   const AgentId& p_receiver_id, MessageContent&& p_content, const std::string& p_sender_environment);

		//###############################################################
		//	Migration (three steps: offer, accept or reject, commit or abort)
		//###############################################################

		/**
		 * Called by Agent::move from the agent's thread: checks, then queues the move for the end of the turn.
		 * @param p_agent The agent that moves
		 * @param p_environment_id The environment it moves to
		 * @param p_options Send options of the offer
		 * @return Success, or the reason the move is refused
		 **/
		transport::Result<void> request_move(const Agent& p_agent, const std::string& p_environment_id,
											 const transport::SendOptions& p_options);

		/**
		 * End of turn: send an AgentOffer for every move asked during the turn.
		 **/
		void process_move_requests();

		/**
		 * Turn boundary: give up the migrations whose deadline passed (origin: restore; destination: discard),
		 * forget the moved agents past the forwarding grace.
		 **/
		void expire_migrations();

		/**
		 * Queue a control envelope (accept, reject, commit, abort) about a transfer.
		 * @param p_kind The kind of control
		 * @param p_environment_id The environment it goes to
		 * @param p_transfer_id The transfer
		 * @param p_agent_id The agent on the move
		 * @param p_accepted True if the agent is accepted
		 * @param p_reason Why it is refused or abandoned, if it is
		 **/
		void send_control(transport::EnvelopeKind p_kind, const std::string& p_environment_id, const std::string& p_transfer_id,
						  const AgentId& p_agent_id, bool p_accepted, const std::string& p_reason);

		/**
		 * End of a transit without success: the agent runs here again with all its messages.
		 * @param p_transit The transit
		 * @param p_error Why it failed, reported to on_delivery_failure()
		 **/
		void restore_agent(const Transit& p_transit, const transport::Error& p_error);

		/**
		 * Forward to the new host of an agent the messages it received here while leaving.
		 * @param p_agent_id The agent
		 * @param p_host Its new environment
		 * @param p_messages The messages
		 **/
		void forward_messages(const AgentId& p_agent_id, const std::string& p_host, const std::vector<MessagePointer>& p_messages);

		/**
		 * Apply an agent offer: rebuild the agent and accept it, held until its origin commits the move, or refuse it.
		 * @param p_envelope The offer
		 **/
		void apply_agent_offer(const transport::Envelope& p_envelope);

		/**
		 * Apply an answer or a confirmation of a migration (accept, reject, commit, abort).
		 * @param p_envelope The control
		 **/
		void apply_agent_control(const transport::Envelope& p_envelope);

		/**
		 * Queue the presence of this environment (online or offline) for the end of the turn.
		 * @param p_online True if present
		 **/
		void announce_presence(bool p_online);

		/**
		 * Queue a directory event about a local agent for the end of the turn.
		 * @param p_event What happened to the agent
		 * @param p_agent_id The agent
		 * @param p_agent_name Its name
		 **/
		void announce_agent(transport::DirectoryEventKind p_event, const AgentId& p_agent_id, const std::string& p_agent_name);

		/**
		 * Publish the queued envelopes and report the failures to on_delivery_failure().
		 **/
		void flush_outbox();

		/**
		 * Queue a message envelope carrying already encoded bytes.
		 * @param p_address The destination
		 * @param p_target_kind Kind of target
		 * @param p_target_id Id of the target agent (TargetKind::Id)
		 * @param p_target_name Name or fragment of the targets (TargetKind::Name, Fragment)
		 * @param p_first_only If true, the first matching agent only
		 * @param p_sender_id Id of the sending agent
		 * @param p_sender_name Name of the sender; empty when unknown
		 * @param p_content The encoded content
		 * @param p_content_encoding Its encoding
		 * @param p_options Send options (scope, encoding, priority)
		 * @param p_origin_environment Environment whose agent emitted the message, when this one forwards it
		 * @return Success, OutboxFull or NotAttached
		 **/
		transport::Result<void> enqueue_message(const transport::Address& p_address, transport::TargetKind p_target_kind,
												const std::string& p_target_id, const std::string& p_target_name, bool p_first_only,
												const std::string& p_sender_id, const std::string& p_sender_name, std::vector<std::uint8_t> p_content,
												ContentEncoding p_content_encoding, const transport::SendOptions& p_options,
												const std::string& p_origin_environment = {}) const;

		/**
		 * Encode a content for the wire and queue its envelope.
		 * @param p_address The destination
		 * @param p_target_kind Kind of target
		 * @param p_target_id Id of the target agent (TargetKind::Id)
		 * @param p_target_name Name or fragment of the targets (TargetKind::Name, Fragment)
		 * @param p_first_only If true, the first matching agent only
		 * @param p_sender_id Id of the sending agent
		 * @param p_sender_name Name of the sender; empty when unknown
		 * @param p_content The content
		 * @param p_options Send options (scope, encoding, priority)
		 * @param p_origin_environment Environment whose agent emitted the message, when this one forwards it
		 * @return Success, a validation error of the content, OutboxFull or NotAttached
		 **/
		transport::Result<void> enqueue_content(const transport::Address& p_address, transport::TargetKind p_target_kind,
												const std::string& p_target_id, const std::string& p_target_name, bool p_first_only,
												const std::string& p_sender_id, const std::string& p_sender_name, const MessageContent& p_content,
												const transport::SendOptions& p_options, const std::string& p_origin_environment = {}) const;
	};

	// Environment pointer
	//using EnvironmentPointer = std::shared_ptr<Environment>;
} // namespace cam
