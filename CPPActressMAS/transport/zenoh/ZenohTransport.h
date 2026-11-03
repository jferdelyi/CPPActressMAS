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
#include <memory>
#include <string>
#include <vector>

#include "../Endpoint.h"
#include "../network/PresenceCache.h"
#include "../network/Worker.h"

/**
 * CPPActressMAS transport layer: Zenoh (zenoh-c and zenoh-cpp 1.7). This header has no Zenoh include:
 * the session lives behind a private implementation.
 **/
namespace cam::transport {

	/**
	 * Options of the Zenoh transport. Without multicast scouting, the session only reaches the
	 * endpoints given here (an environment stays silent by default).
	 **/
	struct ZenohOptions {
		/**
		 * Mode of the session: "peer" or "client".
		 **/
		std::string m_mode = "peer";

		/**
		 * Endpoints to connect to, such as "tcp/127.0.0.1:7447".
		 **/
		std::vector<std::string> m_connect;

		/**
		 * Endpoints to listen on, such as "tcp/127.0.0.1:7447".
		 **/
		std::vector<std::string> m_listen;

		/**
		 * Discover the peers by UDP multicast (asks for the local network).
		 **/
		bool m_multicast_scouting = false;
	};

	/**
	 * One Zenoh session per instance (one node). Publications go out reliably (congestion control
	 * "block"), the local endpoints are served directly and never through the network
	 * (allowed origin "remote"). Presence relies on liveliness tokens: one per node, one per
	 * environment; a newcomer learns the tokens with their history, then asks each environment for its
	 * current presence through a queryable. The death of a node drops its tokens: its environments go
	 * offline for everyone.
	 **/
	class ZenohTransport final : public Transport {
	private:
		/**
		 * The Zenoh session and its declarations (ZenohTransport.cpp).
		 **/
		struct Node;

		/**
		 * Configuration.
		 **/
		TransportConfig m_config;

		/**
		 * Zenoh options.
		 **/
		ZenohOptions m_options;

		/**
		 * Envelope codec.
		 **/
		std::shared_ptr<const EnvelopeCodec> m_codec;

		/**
		 * Id of this node.
		 **/
		std::string m_node_id;

		/**
		 * The session, from open() to close().
		 **/
		std::unique_ptr<Node> m_node;

		/**
		 * Attached endpoints.
		 **/
		EndpointRegistry m_registry;

		/**
		 * Last presence of each environment, for the endpoints attached later.
		 **/
		PresenceCache m_presence;

		/**
		 * Runs the presence queries asked from the Zenoh callbacks.
		 **/
		Worker m_worker;

		/**
		 * True between open() and close().
		 **/
		std::atomic<bool> m_open{false};

		/**
		 * Zenoh callbacks in flight: close() waits for them.
		 **/
		std::atomic<int> m_callbacks{0};

		/**
		 * Payloads dropped because larger than the configured maximum.
		 **/
		std::atomic<std::uint64_t> m_oversized{0};

		/**
		 * Counts a callback in flight: undeclaring or closing does not wait for a callback already running.
		 * Its constructor also marks, for ThreadSanitizer, the hand-over of the closure by the declaring thread.
		 **/
		struct CallbackGuard final {
			/**
			 * The counter of the callbacks in flight.
			 **/
			std::atomic<int>& m_counter;

			/**
			 * Count a callback in flight.
			 * @param p_counter The counter
			 **/
			explicit CallbackGuard(std::atomic<int>& p_counter);

			/**
			 * The callback is over.
			 **/
			~CallbackGuard() { --m_counter; }
			// Delete copy constructor
			CallbackGuard(const CallbackGuard&) = delete;

			CallbackGuard& operator=(const CallbackGuard&) = delete;
		};

		/**
		 * Key of an address in the namespace of this transport.
		 * @param p_kind The kind
		 * @param p_target The target
		 * @return The key
		 **/
		[[nodiscard]] std::string key_of(AddressKind p_kind, const std::string& p_target = {}) const;

		/**
		 * Prefix of the liveliness tokens of the namespace, "cam/v1/<ns>/live/node/".
		 * @return The prefix
		 **/
		[[nodiscard]] std::string live_prefix() const;

		/**
		 * A publication arrived (Zenoh thread): hand its bytes to the endpoints of its key.
		 * @param p_key The key
		 * @param p_bytes The bytes
		 **/
		void on_sample(const std::string& p_key, std::vector<std::uint8_t>&& p_bytes);

		/**
		 * A token of a node or environment appeared or vanished (Zenoh thread): ask a new environment for its presence, or
		 * tell the endpoints that it went offline.
		 * @param p_key The key of the token
		 * @param p_alive True if it appeared
		 **/
		void on_liveliness(const std::string& p_key, bool p_alive);

		/**
		 * An environment answered the presence query (Zenoh thread): remember and deliver its presence.
		 * @param p_environment_id The environment
		 * @param p_bytes The encoded presence
		 **/
		void on_presence_reply(const std::string& p_environment_id, std::vector<std::uint8_t>&& p_bytes);

		/**
		 * Deliver a presence to every attached endpoint.
		 * @param p_environment_id The environment
		 * @param p_envelope The presence
		 **/
		void deliver_presence(const std::string& p_environment_id, Envelope&& p_envelope);

		/**
		 * Ask an environment for its presence, from the worker (never in a Zenoh callback).
		 * @param p_environment_id The environment
		 **/
		void request_presence(const std::string& p_environment_id);

		/**
		 * Wait, at most 2 s, for the callbacks still running.
		 **/
		void wait_callbacks();

		/**
		 * Publish encoded bytes on a key.
		 * @param p_key The key
		 * @param p_bytes The bytes
		 * @return Success or an error
		 **/
		Result<void> publish_bytes(const std::string& p_key, std::vector<std::uint8_t>&& p_bytes);

	public:
		/**
		 * Create a Zenoh transport; nothing is opened yet.
		 * @param p_config The configuration (namespace, maximum envelope size)
		 * @param p_options Mode and endpoints
		 * @param p_codec The envelope codec (protobuf by default)
		 **/
		explicit ZenohTransport(TransportConfig p_config, ZenohOptions p_options = {},
								std::shared_ptr<const EnvelopeCodec> p_codec = nullptr);

		/**
		 * Close the transport if it is still open.
		 **/
		~ZenohTransport() override;

		/**
		 * Open the session, subscribe to the shared keys and the liveliness of the namespace, declare the token of the node,
		 * and wait (bounded) for the first link to an explicit peer.
		 * @return Success or an error
		 **/
		Result<void> open() override;

		/**
		 * Undeclare the endpoints and the node (the peers see them leave), close the session and wait for the callbacks still
		 * running.
		 * @param p_timeout Unused: the waits are bounded by 2 s
		 * @return Success, NotConnected, or the failure of the session
		 **/
		Result<void> close(std::chrono::milliseconds p_timeout) override;

		/**
		 * Attach an endpoint: subscribe its keys, declare its token and queryable, replay the known presences to it.
		 * @param p_endpoint The endpoint
		 * @return Success or an error
		 **/
		Result<void> attach(Endpoint& p_endpoint) override;

		/**
		 * Detach an endpoint and undeclare what it declared.
		 * @param p_endpoint The endpoint
		 * @return Success or an error
		 **/
		Result<void> detach(Endpoint& p_endpoint) override;

		/**
		 * Deliver a publication to the local endpoints, then on the network if the key needs it.
		 * @param p_sender The publishing endpoint
		 * @param p_address The destination
		 * @param p_envelope The envelope
		 * @return Success or an error
		 **/
		Result<void> publish(const Endpoint& p_sender, const Address& p_address, const Envelope& p_envelope) override;

		/**
		 * Capabilities: liveliness and request and reply, order per sender and at least once (reliable).
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
		 * Envelope codec.
		 * @return The codec
		 **/
		[[nodiscard]] std::shared_ptr<const EnvelopeCodec> codec() const override { return m_codec; }

		/**
		 * Name of the transport.
		 * @return "zenoh"
		 **/
		[[nodiscard]] const char* name() const override { return "zenoh"; }

		/**
		 * Options of this instance.
		 * @return The options
		 **/
		[[nodiscard]] const ZenohOptions& options() const { return m_options; }

		/**
		 * Zenoh identifier of the session (hexadecimal), empty while closed.
		 * @return The identifier
		 **/
		[[nodiscard]] std::string session_id() const;

		/**
		 * Payloads dropped because larger than the configured maximum.
		 * @return The count
		 **/
		[[nodiscard]] std::uint64_t oversized() const { return m_oversized.load(); }
	};

} // namespace cam::transport
