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

/**
 * CPPActressMAS transport layer: MQTT (libmosquitto, built by the project). This header has no libmosquitto
 * include: the client lives behind a private implementation.
 **/
namespace cam::transport {

	/**
	 * Options of the MQTT transport.
	 **/
	struct MqttOptions {
		/**
		 * MQTT 5 (no local, session expiry); false: MQTT 3.1.1.
		 **/
		bool m_mqtt5 = true;

		/**
		 * Keep-alive interval of the connection.
		 **/
		std::chrono::seconds m_keep_alive{30};

		/**
		 * First delay before a reconnection.
		 **/
		std::chrono::seconds m_reconnect_min{1};

		/**
		 * Longest delay between two reconnections.
		 **/
		std::chrono::seconds m_reconnect_max{30};
	};

	/**
	 * One MQTT connection per instance (one node): client id "cam-<namespace>-<node>", QoS 1, retained
	 * presence of the environments, retained will of the node, no local delivery through the broker,
	 * automatic reconnection with re-subscription, TCP_NODELAY on the socket. The transport owns the
	 * network thread of the client. Publications of an attached endpoint reach the other attached
	 * endpoints directly and the network once.
	 **/
	class MqttTransport final : public Transport {
	private:
		/**
		 * The libmosquitto client and its synchronization (MqttTransport.cpp).
		 **/
		struct Client;

		/**
		 * Configuration.
		 **/
		TransportConfig m_config;

		/**
		 * MQTT options.
		 **/
		MqttOptions m_options;

		/**
		 * Envelope codec.
		 **/
		std::shared_ptr<const EnvelopeCodec> m_codec;

		/**
		 * Id of this node.
		 **/
		std::string m_node_id;

		/**
		 * MQTT client id, "cam-<namespace>-<node>".
		 **/
		std::string m_client_id;

		/**
		 * The client, from open() to close().
		 **/
		std::unique_ptr<Client> m_client;

		/**
		 * Attached endpoints.
		 **/
		EndpointRegistry m_registry;

		/**
		 * Last presence of each environment, for the endpoints attached later.
		 **/
		PresenceCache m_presence;

		/**
		 * True between open() and close().
		 **/
		std::atomic<bool> m_open{false};

		/**
		 * True after the first connection: the next ones are recoveries.
		 **/
		std::atomic<bool> m_connected_once{false};

		/**
		 * Payloads dropped because larger than the configured maximum.
		 **/
		std::atomic<std::uint64_t> m_oversized{0};

		/**
		 * Key of an address in the namespace of this transport.
		 * @param p_kind The kind
		 * @param p_target The target
		 * @return The key
		 **/
		[[nodiscard]] std::string key_of(AddressKind p_kind, const std::string& p_target = {}) const;

		/**
		 * Keys an attached endpoint subscribes to: its EnvIn and EnvAgent keys.
		 * @param p_endpoint The endpoint
		 * @return The keys
		 **/
		[[nodiscard]] std::vector<std::string> endpoint_keys(const Endpoint& p_endpoint) const;

		/**
		 * Presence of this node (a presence whose environment is the node).
		 * @param p_online False when the node leaves
		 * @return The envelope
		 **/
		[[nodiscard]] Envelope node_presence(bool p_online) const;

		/**
		 * Keys every node subscribes to: AllIn and the presences of the environments and nodes.
		 * @return The keys
		 **/
		[[nodiscard]] std::vector<std::string> shared_keys() const;

		/**
		 * Subscribe to keys and wait for their acknowledgement; while disconnected, the reconnection does it.
		 * @param p_keys The keys
		 * @return Success or an error
		 **/
		Result<void> subscribe(const std::vector<std::string>& p_keys);

		/**
		 * Wait for the acknowledgement of subscriptions, within the connection timeout.
		 * @param p_mids The message ids of the subscriptions
		 * @return Success, Timeout, or the refusal of the broker
		 **/
		Result<void> wait_subscribed(const std::vector<int>& p_mids);

		/**
		 * Unsubscribe from keys.
		 * @param p_keys The keys
		 * @return Success or an error
		 **/
		Result<void> unsubscribe(const std::vector<std::string>& p_keys);

		/**
		 * Publish an encoded envelope (QoS 1).
		 * @param p_key The key
		 * @param p_bytes The bytes (empty clears a retained message)
		 * @param p_retained True if the broker retains it
		 * @return Success or an error
		 **/
		Result<void> publish_bytes(const std::string& p_key, const std::vector<std::uint8_t>& p_bytes, bool p_retained);

		/**
		 * Disconnect and join the network thread: no callback runs after it.
		 **/
		void shutdown();

		/**
		 * A CONNACK arrived (network thread): wake open(), and after a recovery subscribe again, announce the node and tell
		 * the endpoints.
		 * @param p_reason The reason code, 0 on success
		 **/
		void on_connected(int p_reason);

		/**
		 * The connection dropped (network thread): tell the endpoints when it was unexpected.
		 * @param p_reason The reason code, 0 for a disconnection asked
		 **/
		void on_disconnected(int p_reason);

		/**
		 * A SUBACK arrived (network thread): note it, and the refused subscriptions.
		 * @param p_mid The message id
		 * @param p_count The number of granted codes
		 * @param p_granted The granted codes (0x80 and more: refused)
		 **/
		void on_subscribed(int p_mid, int p_count, const int* p_granted);

		/**
		 * A PUBACK arrived (network thread).
		 * @param p_mid The message id
		 **/
		void on_published(int p_mid);

		/**
		 * A publication arrived (network thread): hand its bytes to the endpoints of its key.
		 * @param p_key The key
		 * @param p_payload The bytes
		 * @param p_size Their size
		 **/
		void on_message(const std::string& p_key, const std::uint8_t* p_payload, std::size_t p_size);

	public:
		/**
		 * Create an MQTT transport; nothing is opened yet.
		 * @param p_config The configuration (endpoint "tcp://host:port" or "ssl://host:port", namespace, TLS, credentials)
		 * @param p_options The MQTT options
		 * @param p_codec The envelope codec (protobuf by default)
		 **/
		explicit MqttTransport(TransportConfig p_config, MqttOptions p_options = {},
							   std::shared_ptr<const EnvelopeCodec> p_codec = nullptr);

		/**
		 * Close the transport if it is still open.
		 **/
		~MqttTransport() override;

		/**
		 * Connect to the broker within the connection timeout, start the network thread, subscribe to the shared keys and
		 * announce the node (retained, with its will).
		 * @return Success or an error
		 **/
		Result<void> open() override;

		/**
		 * Announce that the node leaves, wait at most p_timeout for the broker to hold it, then disconnect.
		 * @param p_timeout Maximum wait
		 * @return Success, or NotConnected
		 **/
		Result<void> close(std::chrono::milliseconds p_timeout) override;

		/**
		 * Attach an endpoint: subscribe its keys and replay the known presences to it.
		 * @param p_endpoint The endpoint
		 * @return Success or an error
		 **/
		Result<void> attach(Endpoint& p_endpoint) override;

		/**
		 * Detach an endpoint and unsubscribe its keys.
		 * @param p_endpoint The endpoint
		 * @return Success or an error
		 **/
		Result<void> detach(Endpoint& p_endpoint) override;

		/**
		 * Deliver a publication to the local endpoints, then to the broker if the key needs the network (presences retained).
		 * @param p_sender The publishing endpoint
		 * @param p_address The destination
		 * @param p_envelope The envelope
		 * @return Success or an error
		 **/
		Result<void> publish(const Endpoint& p_sender, const Address& p_address, const Envelope& p_envelope) override;

		/**
		 * Capabilities: retained presence, order per sender and at least once (QoS 1).
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
		 * @return "mqtt"
		 **/
		[[nodiscard]] const char* name() const override { return "mqtt"; }

		/**
		 * MQTT client id of this node.
		 * @return The client id
		 **/
		[[nodiscard]] const std::string& client_id() const { return m_client_id; }

		/**
		 * True while the connection to the broker is up.
		 * @return True if connected
		 **/
		[[nodiscard]] bool is_connected() const;

		/**
		 * Payloads dropped because larger than the configured maximum.
		 * @return The count
		 **/
		[[nodiscard]] std::uint64_t oversized() const { return m_oversized.load(); }
	};

} // namespace cam::transport
