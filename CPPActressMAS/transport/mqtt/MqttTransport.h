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

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "../Endpoint.h"
#include "../PresenceCache.h"

/**
 * CPPActressMAS transport layer: MQTT (libmosquitto, built by the project). This header has no libmosquitto
 * include: the client lives behind a private implementation.
 */
namespace cam::transport {

	/**
	 * Options of the MQTT transport.
	 **/
	struct MqttOptions {
		bool m_mqtt5 = true;                              ///< MQTT 5 (no local, session expiry); false: 3.1.1
		std::chrono::seconds m_keep_alive{30};
		std::chrono::seconds m_reconnect_min{1};
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
		struct Client;

		TransportConfig m_config;
		MqttOptions m_options;
		std::shared_ptr<const EnvelopeCodec> m_codec;
		std::string m_node_id;
		std::string m_client_id;
		std::unique_ptr<Client> m_client;
		EndpointRegistry m_registry;
		PresenceCache m_presence;
		std::atomic<bool> m_open{false};
		std::atomic<bool> m_connected_once{false};
		std::atomic<std::uint64_t> m_oversized{0};

		[[nodiscard]] std::string key_of(AddressKind p_kind, const std::string& p_target = {}) const;
		[[nodiscard]] std::vector<std::string> endpoint_keys(const Endpoint& p_endpoint) const;
		[[nodiscard]] Envelope node_presence(bool p_online) const;
		[[nodiscard]] std::vector<std::string> shared_keys() const;
		Result<void> subscribe(const std::vector<std::string>& p_keys);
		Result<void> wait_subscribed(const std::vector<int>& p_mids);
		Result<void> unsubscribe(const std::vector<std::string>& p_keys);
		Result<void> publish_bytes(const std::string& p_key, const std::vector<std::uint8_t>& p_bytes, bool p_retained);
		void shutdown();
		void on_connected(int p_reason);
		void on_disconnected(int p_reason);
		void on_subscribed(int p_mid, int p_count, const int* p_granted);
		void on_published(int p_mid);
		void on_message(const std::string& p_key, const std::uint8_t* p_payload, std::size_t p_size);

	public:
		/**
		 * Create an MQTT transport; nothing is opened yet.
		 * @param p_config The configuration (endpoint "tcp://host:port" or "ssl://host:port", namespace, TLS, credentials)
		 * @param p_options The MQTT options
		 * @param p_codec The envelope codec (compact by default)
		 **/
		explicit MqttTransport(TransportConfig p_config, MqttOptions p_options = {},
							   std::shared_ptr<const EnvelopeCodec> p_codec = nullptr);

		~MqttTransport() override;

		Result<void> open() override;
		Result<void> close(std::chrono::milliseconds p_timeout) override;
		Result<void> attach(Endpoint& p_endpoint) override;
		Result<void> detach(Endpoint& p_endpoint) override;
		Result<void> publish(const Endpoint& p_sender, const Address& p_address, const Envelope& p_envelope) override;
		[[nodiscard]] Capabilities capabilities() const override;
		[[nodiscard]] const TransportConfig& config() const override { return m_config; }
		[[nodiscard]] const std::string& node_id() const override { return m_node_id; }
		[[nodiscard]] std::shared_ptr<const EnvelopeCodec> codec() const override { return m_codec; }
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
