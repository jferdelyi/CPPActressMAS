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

#include <atomic>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "../Endpoint.h"
#include "../network/PresenceCache.h"
#include "ActressMasCodec.h"
#include "Socket.h"

/**
 * CPPActressMAS transport layer: the ContainerServer of ActressMAS v3.1 (C#), with its basic protocol.
 **/
namespace cam::transport {

	/**
	 * Options of the ActressMAS transport.
	 **/
	struct ActressMasOptions {
		/**
		 * Name of the container: the identity of the connection on the server, as in C# (no space, no '@').
		 **/
		std::string m_container;
	};

	/**
	 * A client of the ActressMAS v3.1 ContainerServer, with the qualities and the defects of its protocol: one TCP
	 * connection per container, one container per environment, messages between agents relayed by the server by the name
	 * of their container, the list of the containers as presence, no acknowledgement, no reconnection. An agent of another
	 * container is "name@container", as in C#: a send by name with "@C" goes to container C, a send by name without it
	 * goes to every known container (one without that agent drops it, as in C#), a reply by identifier goes back to
	 * "name@container". The containers and the agents of the C# have no identifier: they get identifiers derived from
	 * their names. Migration, directory, broadcast and fragments of names are not part of the protocol: they are refused
	 * (NotSupported) and counted; the presence of the environment is absorbed (the server keeps the list). The envelope
	 * codec is unused on the wire: codec() returns the configured one.
	 **/
	class ActressMasTransport final : public Transport {
	private:
		/**
		 * Configuration (endpoint "host:port").
		 **/
		TransportConfig m_config;

		/**
		 * Options.
		 **/
		ActressMasOptions m_options;

		/**
		 * Envelope codec, unused on the wire.
		 **/
		std::shared_ptr<const EnvelopeCodec> m_codec;

		/**
		 * Id of this node.
		 **/
		std::string m_node_id;

		/**
		 * Epoch of the flows this transport delivers, drawn at construction.
		 **/
		std::uint32_t m_epoch;

		/**
		 * The connection to the server.
		 **/
		TcpConnection m_connection;

		/**
		 * Reads the frames of the server, from open() to close().
		 **/
		std::thread m_reader;

		/**
		 * One frame at a time on the socket.
		 **/
		std::mutex m_write_mutex;

		/**
		 * The attached endpoint (one at most).
		 **/
		EndpointRegistry m_registry;

		/**
		 * Presence of the other containers, for an endpoint attached after it arrived.
		 **/
		PresenceCache m_presence;

		/**
		 * True between open() and close().
		 **/
		std::atomic<bool> m_open{false};

		/**
		 * Guards the state below.
		 **/
		mutable std::mutex m_mutex;

		/**
		 * Wakes open() and close() when the server answers or the reader stops.
		 **/
		std::condition_variable m_condition;

		/**
		 * The server listed this container: the registration is accepted.
		 **/
		bool m_registered = false;

		/**
		 * The server refused the name of this container.
		 **/
		bool m_refused = false;

		/**
		 * The reader stopped: the connection is closed or lost.
		 **/
		bool m_reader_done = false;

		/**
		 * Id of the attached environment, empty when none.
		 **/
		std::string m_environment_id;

		/**
		 * The other containers of the last list of the server.
		 **/
		std::vector<std::string> m_containers;

		/**
		 * "name@container" of each agent of another container met so far, by its derived identifier.
		 **/
		std::map<std::string, std::string> m_agent_addresses;

		/**
		 * Next sequence of each flow this transport delivers.
		 **/
		std::map<std::string, std::uint64_t> m_sequences;

		/**
		 * Frames that could not be read.
		 **/
		std::atomic<std::uint64_t> m_malformed{0};

		/**
		 * Agents of another container refused on arrival (an agent of the C# cannot run here).
		 **/
		std::atomic<std::uint64_t> m_refused_moves{0};

		/**
		 * Write one frame.
		 * @param p_message The container message
		 * @return Success, or NotConnected
		 **/
		Result<void> send_frame(const ContainerMessage& p_message);

		/**
		 * Body of the reader: read the frames until the connection closes, then tell the endpoint if it was unexpected.
		 **/
		void read_frames();

		/**
		 * Handle one frame of the server (reader thread).
		 * @param p_message The container message
		 **/
		void handle(const ContainerMessage& p_message);

		/**
		 * Take a list of containers into account: registration, arrivals and departures.
		 * @param p_list The names, each followed by a space
		 **/
		void on_container_list(std::string_view p_list);

		/**
		 * Deliver a message of another container to the attached environment.
		 * @param p_container The container that sent it
		 * @param p_message The message
		 **/
		void on_remote_message(const std::string& p_container, const RemoteMessage& p_message);

		/**
		 * Deliver the presence of a container (reader thread, under m_mutex).
		 * @param p_container The container
		 * @param p_online False when it leaves
		 **/
		void deliver_presence(const std::string& p_container, bool p_online);

		/**
		 * Deliver an envelope to the attached environment.
		 * @param p_address The address it arrives on
		 * @param p_envelope The envelope
		 **/
		void deliver(const Address& p_address, Envelope&& p_envelope);

		/**
		 * Next sequence of a flow (under m_mutex).
		 * @param p_flow The flow
		 * @return The sequence, from 1
		 **/
		std::uint64_t next_sequence(const std::string& p_flow);

		/**
		 * Stop the reader and release the connection, waiting at most p_timeout for the reader.
		 * @param p_timeout Maximum wait
		 **/
		void shutdown(std::chrono::milliseconds p_timeout);

	public:
		/**
		 * Create an ActressMAS transport; nothing is opened yet.
		 * @param p_config The configuration (endpoint "host:port" of the server)
		 * @param p_options The options (name of the container)
		 * @param p_codec The envelope codec, unused on the wire (protobuf by default)
		 **/
		explicit ActressMasTransport(TransportConfig p_config, ActressMasOptions p_options,
									 std::shared_ptr<const EnvelopeCodec> p_codec = nullptr);

		/**
		 * Close the transport if it is still open.
		 **/
		~ActressMasTransport() override;

		// Delete copy constructor
		ActressMasTransport(const ActressMasTransport&) = delete;

		ActressMasTransport& operator=(const ActressMasTransport&) = delete;

		/**
		 * Connect to the server, register the container, and wait for the server to list it (at most the connection
		 * timeout; the server lists its containers once per period, 3 s for the one of the C# examples).
		 * @return Success, InvalidConfiguration (no name, or a name the server refuses), Timeout, or the error of the
		 * connection
		 **/
		Result<void> open() override;

		/**
		 * Close the connection; the server drops the container from its list.
		 * @param p_timeout Maximum wait for the reader
		 * @return Success, or NotConnected
		 **/
		Result<void> close(std::chrono::milliseconds p_timeout) override;

		/**
		 * Attach the environment of this container and replay the known containers to it.
		 * @param p_endpoint The endpoint
		 * @return Success, NotSupported for a second environment, or an error
		 **/
		Result<void> attach(Endpoint& p_endpoint) override;

		/**
		 * Detach the environment.
		 * @param p_endpoint The endpoint
		 * @return Success, or NotAttached
		 **/
		Result<void> detach(Endpoint& p_endpoint) override;

		/**
		 * Send a message to the agents of other containers (see the class); absorb the presence of the environment;
		 * refuse what the protocol cannot carry.
		 * @param p_sender The publishing endpoint
		 * @param p_address The destination
		 * @param p_envelope The envelope
		 * @return Success, NotSupported, UnknownDestination, or NotConnected
		 **/
		Result<void> publish(const Endpoint& p_sender, const Address& p_address, const Envelope& p_envelope) override;

		/**
		 * Capabilities: none of the guarantees (the server relays on a thread pool, without acknowledgement).
		 * @return The capabilities
		 **/
		[[nodiscard]] Capabilities capabilities() const override;

		/**
		 * Configuration.
		 * @return The configuration
		 **/
		[[nodiscard]] const TransportConfig& config() const override { return m_config; }

		/**
		 * Id of this node.
		 * @return The node id
		 **/
		[[nodiscard]] const std::string& node_id() const override { return m_node_id; }

		/**
		 * Envelope codec, unused on the wire.
		 * @return The codec
		 **/
		[[nodiscard]] std::shared_ptr<const EnvelopeCodec> codec() const override { return m_codec; }

		/**
		 * Name of the transport.
		 * @return "actressmas"
		 **/
		[[nodiscard]] const char* name() const override { return "actressmas"; }

		/**
		 * Name of the container of this connection.
		 * @return The name
		 **/
		[[nodiscard]] const std::string& container() const { return m_options.m_container; }

		/**
		 * The other containers of the last list of the server.
		 * @return Their names
		 **/
		[[nodiscard]] std::vector<std::string> containers() const;

		/**
		 * True while the connection to the server is up.
		 * @return True if connected
		 **/
		[[nodiscard]] bool is_connected() const;

		/**
		 * Frames that could not be read.
		 * @return The count
		 **/
		[[nodiscard]] std::uint64_t malformed() const { return m_malformed.load(); }

		/**
		 * Agents of another container refused on arrival.
		 * @return The count
		 **/
		[[nodiscard]] std::uint64_t refused_moves() const { return m_refused_moves.load(); }

		/**
		 * Identifier of the environment of a container, derived from its name: a canonical UUID, the same in every
		 * process.
		 * @param p_container The name of the container
		 * @return The identifier
		 **/
		[[nodiscard]] static std::string environment_id_of(std::string_view p_container);

		/**
		 * Identifier of the node of a container, derived from its name (distinct from the one of its environment).
		 * @param p_container The name of the container
		 * @return The identifier
		 **/
		[[nodiscard]] static std::string node_id_of(std::string_view p_container);

		/**
		 * Identifier of an agent of another container, derived from "name@container": AgentId::CAPACITY base62
		 * characters, the same in every process.
		 * @param p_address The agent, "name@container"
		 * @return The identifier
		 **/
		[[nodiscard]] static std::string agent_id_of(std::string_view p_address);

		/**
		 * Split "name@container" at its last '@'.
		 * @param p_address The text
		 * @return The name and the container, or nothing without '@' or with an empty part
		 **/
		[[nodiscard]] static std::optional<std::pair<std::string, std::string>> split_address(std::string_view p_address);

		/**
		 * Text of the content of a message, as the C# reads it: text as it is, a JSON value as its text, bytes as
		 * "base64:" followed by their base64.
		 * @param p_envelope The message
		 * @return The text, or the error of the decoding
		 **/
		[[nodiscard]] static Result<std::string> text_of(const Envelope& p_envelope);

		/**
		 * Set the content of a message from the text of the C#: "base64:" followed by valid base64 gives bytes back, any
		 * other text stays text.
		 * @param p_envelope The message
		 * @param p_text The text
		 **/
		static void set_text(Envelope& p_envelope, const std::string& p_text);
	};

} // namespace cam::transport
