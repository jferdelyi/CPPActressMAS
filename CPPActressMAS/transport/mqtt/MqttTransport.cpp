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

#include "MqttTransport.h"

#include <chrono>

#include <mqtt/async_client.h>
#include <uuid/UUID.hpp>

#include "../CompactCodec.h"

namespace cam::transport {

	namespace {

		constexpr int QOS = 1;

		Error transport_error(const std::string& p_message, const int p_native = 0) {
			return make_error(ErrorCategory::Transport, ErrorCode::TransportFailure, p_message, p_native);
		}

		std::uint64_t now_ms() {
			return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
					std::chrono::system_clock::now().time_since_epoch()).count());
		}

	} // namespace

	/**
	 * The Paho client and its options (private: no Paho type in the header).
	 **/
	struct MqttTransport::Client {
		mqtt::async_client m_client;
		mqtt::connect_options m_connect_options;

		Client(const std::string& p_endpoint, const std::string& p_client_id, const int p_version) :
				m_client(p_endpoint, p_client_id, mqtt::create_options(p_version)) {
		}
	};

	MqttTransport::MqttTransport(TransportConfig p_config, MqttOptions p_options, std::shared_ptr<const EnvelopeCodec> p_codec) :
			m_config(std::move(p_config)),
			m_options(p_options),
			m_codec(p_codec ? std::move(p_codec) : std::make_shared<CompactCodec>()),
			m_node_id(UUID::generate_uuid()),
			m_client_id("cam-" + m_config.m_namespace + "-" + m_node_id) {
	}

	MqttTransport::~MqttTransport() {
		if (m_open.load()) {
			(void) close(std::chrono::milliseconds(2000));
		}
	}

	// ---------------------------------------------------------------- keys and presence

	std::string MqttTransport::key_of(const AddressKind p_kind, const std::string& p_target) const {
		return Address{p_kind, m_config.m_namespace, p_target}.to_key();
	}

	std::vector<std::string> MqttTransport::endpoint_keys(const Endpoint& p_endpoint) const {
		return {key_of(AddressKind::EnvIn, p_endpoint.environment_id()),
				key_of(AddressKind::EnvAgent, p_endpoint.environment_id()),
				key_of(AddressKind::EnvQuery, p_endpoint.environment_id())};
	}

	Envelope MqttTransport::node_presence(const bool p_online) const {
		// A node presence is a presence whose environment is the node itself.
		Envelope l_envelope;
		l_envelope.m_kind = EnvelopeKind::Presence;
		l_envelope.m_sender_environment = m_node_id;
		l_envelope.m_stream_kind = StreamKind::Presence;
		l_envelope.m_stream_target = m_node_id;
		l_envelope.m_sequence = p_online ? 1 : 2;
		Presence l_presence;
		l_presence.m_node_id = m_node_id;
		l_presence.m_environment_id = m_node_id;
		l_presence.m_namespace = m_config.m_namespace;
		l_presence.m_online = p_online;
		l_presence.m_timestamp_ms = now_ms();
		l_envelope.m_payload = std::move(l_presence);
		return l_envelope;
	}

	// ---------------------------------------------------------------- Paho calls

	Result<void> MqttTransport::subscribe(const std::vector<std::string>& p_keys) {
		if (!m_client || !m_client->m_client.is_connected()) {
			return {};  // subscribed on (re)connection
		}
		try {
			// Wait for each acknowledgement: a publication made before the broker knows the subscription
			// would never come back.
			std::vector<mqtt::token_ptr> l_tokens;
			for (const auto& l_key: p_keys) {
				if (m_options.m_mqtt5) {
					l_tokens.push_back(m_client->m_client.subscribe(l_key, QOS, mqtt::subscribe_options(true)));
				} else {
					l_tokens.push_back(m_client->m_client.subscribe(l_key, QOS));
				}
			}
			for (const auto& l_token: l_tokens) {
				if (!l_token->wait_for(m_config.m_connect_timeout)) {
					return make_error(ErrorCategory::Transport, ErrorCode::Timeout, "subscription not acknowledged by the broker");
				}
			}
		} catch (const mqtt::exception& p_exception) {
			return transport_error(std::string("subscribe failed: ") + p_exception.what(), p_exception.get_reason_code());
		}
		return {};
	}

	Result<void> MqttTransport::unsubscribe(const std::vector<std::string>& p_keys) {
		if (!m_client || !m_client->m_client.is_connected()) {
			return {};
		}
		try {
			for (const auto& l_key: p_keys) {
				m_client->m_client.unsubscribe(l_key);
			}
		} catch (const mqtt::exception& p_exception) {
			return transport_error(std::string("unsubscribe failed: ") + p_exception.what(), p_exception.get_reason_code());
		}
		return {};
	}

	Result<void> MqttTransport::publish_bytes(const std::string& p_key, const std::vector<std::uint8_t>& p_bytes, const bool p_retained) {
		if (!m_client) {
			return make_error(ErrorCategory::State, ErrorCode::NotConnected, "mqtt transport not open");
		}
		try {
			m_client->m_client.publish(p_key, p_bytes.data(), p_bytes.size(), QOS, p_retained);
		} catch (const mqtt::exception& p_exception) {
			return transport_error(std::string("publish failed: ") + p_exception.what(), p_exception.get_reason_code());
		}
		return {};
	}

	// ---------------------------------------------------------------- callbacks (Paho threads)

	void MqttTransport::on_connected(const bool p_reconnected) {
		// Paho calls happen on the worker, never inside this callback.
		m_worker.post([this, p_reconnected] {
			std::vector<std::string> l_keys = {key_of(AddressKind::AllIn), "cam/v1/" + m_config.m_namespace + "/env/+/presence",
											   "cam/v1/" + m_config.m_namespace + "/node/+/presence"};
			m_registry.for_each(nullptr, [&](const Endpoint& p_endpoint) {
				for (auto& l_key: endpoint_keys(p_endpoint)) {
					l_keys.push_back(std::move(l_key));
				}
			});
			(void) subscribe(l_keys);
			if (const auto l_bytes = m_codec->encode(node_presence(true)); l_bytes) {
				(void) publish_bytes(key_of(AddressKind::NodePresence, m_node_id), l_bytes.value(), true);
			}
			// The first connection is reported by attach() to each endpoint; only the recoveries come from here.
			if (p_reconnected) {
				m_registry.for_each(nullptr, [](Endpoint& p_endpoint) { p_endpoint.push_event(TransportEvent::Reconnected); });
			}
		});
	}

	void MqttTransport::on_connection_lost() {
		m_registry.for_each(nullptr, [](Endpoint& p_endpoint) { p_endpoint.push_event(TransportEvent::Disconnected); });
	}

	void MqttTransport::on_message(const std::string& p_key, const std::uint8_t* p_payload, const std::size_t p_size) {
		if (p_size > m_config.m_max_envelope_bytes) {
			++m_oversized;
			return;
		}
		const auto l_address = Address::parse(p_key);
		if (!l_address || l_address->m_namespace != m_config.m_namespace) {
			return;
		}
		if (p_size == 0) {
			return;  // a cleared retained message
		}
		std::vector<std::uint8_t> l_bytes(p_payload, p_payload + p_size);
		switch (l_address->m_kind) {
			case AddressKind::EnvIn:
			case AddressKind::EnvAgent:
			case AddressKind::EnvQuery:
				m_registry.with(l_address->m_target, [&](Endpoint& p_target) { p_target.deliver(*l_address, std::move(l_bytes)); });
				break;
			case AddressKind::EnvPresence:
				// Keep the last online presence of each environment for the endpoints attached later.
				if (const auto l_decoded = m_codec->decode(l_bytes, m_config.m_max_envelope_bytes); l_decoded) {
					m_presence.remember(*l_address, l_decoded.value());
				}
				[[fallthrough]];
			case AddressKind::AllIn:
			case AddressKind::NodePresence:
			default:
				m_registry.for_each(nullptr, [&](Endpoint& p_endpoint) {
					std::vector<std::uint8_t> l_copy = l_bytes;
					p_endpoint.deliver(*l_address, std::move(l_copy));
				});
				break;
		}
	}

	// ---------------------------------------------------------------- Transport

	Result<void> MqttTransport::open() {
		if (m_open.load()) {
			return make_error(ErrorCategory::State, ErrorCode::AlreadyConnected, "mqtt transport already open");
		}
		if (!is_valid_namespace(m_config.m_namespace)) {
			return make_error(ErrorCategory::State, ErrorCode::InvalidConfiguration, "invalid namespace");
		}
		if (m_config.m_endpoint.empty()) {
			return make_error(ErrorCategory::State, ErrorCode::InvalidConfiguration, "no broker address");
		}
		m_worker.start();
		try {
			m_client = std::make_unique<Client>(m_config.m_endpoint, m_client_id, m_options.m_mqtt5 ? MQTTVERSION_5 : MQTTVERSION_3_1_1);
			auto l_builder = m_options.m_mqtt5 ? mqtt::connect_options_builder::v5() : mqtt::connect_options_builder();
			l_builder.keep_alive_interval(m_options.m_keep_alive)
					.connect_timeout(m_config.m_connect_timeout)
					.automatic_reconnect(m_options.m_reconnect_min, m_options.m_reconnect_max);
			if (m_options.m_mqtt5) {
				l_builder.clean_start(false)
						.properties({{mqtt::property::SESSION_EXPIRY_INTERVAL, static_cast<std::uint32_t>(m_config.m_session_expiry.count())}});
			} else {
				l_builder.clean_session(false);
			}
			if (const auto l_will = m_codec->encode(node_presence(false)); l_will) {
				l_builder.will(mqtt::will_options(key_of(AddressKind::NodePresence, m_node_id), l_will.value().data(),
												  l_will.value().size(), QOS, true));
			}
			if (!m_config.m_username.empty()) {
				l_builder.user_name(m_config.m_username).password(m_config.m_password);
			}
			if (m_config.m_tls) {
				mqtt::ssl_options l_ssl;
				if (!m_config.m_ca_file.empty()) l_ssl.set_trust_store(m_config.m_ca_file);
				if (!m_config.m_certificate_file.empty()) l_ssl.set_key_store(m_config.m_certificate_file);
				if (!m_config.m_key_file.empty()) l_ssl.set_private_key(m_config.m_key_file);
				l_ssl.set_enable_server_cert_auth(m_config.m_verify_peer);
				l_ssl.set_verify(m_config.m_verify_peer);
				l_builder.ssl(std::move(l_ssl));
			}
			m_client->m_connect_options = l_builder.finalize();

			m_client->m_client.set_connected_handler([this](const std::string&) {
				const bool l_reconnected = m_connected_once.exchange(true);
				on_connected(l_reconnected);
			});
			m_client->m_client.set_connection_lost_handler([this](const std::string&) { on_connection_lost(); });
			m_client->m_client.set_message_callback([this](mqtt::const_message_ptr p_message) {
				const auto& l_payload = p_message->get_payload();
				on_message(p_message->get_topic(), reinterpret_cast<const std::uint8_t*>(l_payload.data()), l_payload.size());
			});

			const auto l_token = m_client->m_client.connect(m_client->m_connect_options);
			if (!l_token->wait_for(m_config.m_connect_timeout)) {
				(void) m_client->m_client.disconnect()->wait_for(std::chrono::milliseconds(500));
				m_worker.stop();
				m_client.reset();
				return make_error(ErrorCategory::Transport, ErrorCode::Timeout, "no answer from the broker " + m_config.m_endpoint);
			}
		} catch (const mqtt::exception& p_exception) {
			m_worker.stop();
			m_client.reset();
			return transport_error(std::string("connection to ") + m_config.m_endpoint + " failed: " + p_exception.what(),
								   p_exception.get_reason_code());
		}
		// The shared keys are subscribed here, before the first publication of any environment; the
		// connected handler subscribes them again (idempotent) after a reconnection.
		if (const auto l_result = subscribe({key_of(AddressKind::AllIn), "cam/v1/" + m_config.m_namespace + "/env/+/presence",
											 "cam/v1/" + m_config.m_namespace + "/node/+/presence"}); !l_result) {
			(void) m_client->m_client.disconnect()->wait_for(std::chrono::milliseconds(500));
			m_worker.stop();
			m_client.reset();
			return l_result;
		}
		m_open.store(true);
		return {};
	}

	Result<void> MqttTransport::close(const std::chrono::milliseconds p_timeout) {
		if (!m_open.load()) {
			return make_error(ErrorCategory::State, ErrorCode::NotConnected, "mqtt transport not open");
		}
		m_open.store(false);
		m_worker.stop();
		Result<void> l_result;
		try {
			if (m_client->m_client.is_connected()) {
				if (const auto l_bytes = m_codec->encode(node_presence(false)); l_bytes) {
					const auto l_token = m_client->m_client.publish(key_of(AddressKind::NodePresence, m_node_id), l_bytes.value().data(),
																	l_bytes.value().size(), QOS, true);
					l_token->wait_for(p_timeout);
				}
				if (!m_client->m_client.disconnect(p_timeout)->wait_for(p_timeout)) {
					l_result = make_error(ErrorCategory::Transport, ErrorCode::Timeout, "disconnection timed out");
				}
			}
		} catch (const mqtt::exception& p_exception) {
			l_result = transport_error(std::string("disconnection failed: ") + p_exception.what(), p_exception.get_reason_code());
		}
		m_client.reset();
		return l_result;
	}

	Result<void> MqttTransport::attach(Endpoint& p_endpoint) {
		if (!m_open.load()) {
			return make_error(ErrorCategory::State, ErrorCode::NotConnected, "mqtt transport not open");
		}
		if (!m_registry.add(p_endpoint)) {
			return make_error(ErrorCategory::State, ErrorCode::AlreadyConnected, "endpoint already attached");
		}
		if (const auto l_result = subscribe(endpoint_keys(p_endpoint)); !l_result) {
			m_registry.remove(p_endpoint);
			return l_result;
		}
		m_presence.replay(p_endpoint);
		p_endpoint.push_event(TransportEvent::Connected);
		return {};
	}

	Result<void> MqttTransport::detach(Endpoint& p_endpoint) {
		if (!m_registry.remove(p_endpoint)) {
			return make_error(ErrorCategory::State, ErrorCode::NotAttached, "endpoint not attached");
		}
		m_presence.forget(p_endpoint.environment_id());
		return unsubscribe(endpoint_keys(p_endpoint));
	}

	Result<void> MqttTransport::publish(const Endpoint& p_sender, const Address& p_address, const Envelope& p_envelope) {
		if (!m_open.load()) {
			return make_error(ErrorCategory::State, ErrorCode::NotConnected, "mqtt transport not open");
		}
		if (p_address.m_namespace != m_config.m_namespace) {
			return make_error(ErrorCategory::Routing, ErrorCode::InvalidTarget, "address outside the transport namespace");
		}
		m_presence.remember(p_address, p_envelope);
		const auto l_delivery = m_registry.deliver_locally(&p_sender, p_address, p_envelope);
		if (!l_delivery.m_needs_network) {
			return {};
		}
		const auto l_bytes = m_codec->encode(p_envelope);
		if (!l_bytes) {
			return l_bytes.error();
		}
		if (l_bytes.value().size() > m_config.m_max_envelope_bytes) {
			return make_error(ErrorCategory::Validation, ErrorCode::TooLarge, "envelope larger than the configured maximum");
		}
		const std::string l_key = p_address.to_key();
		if (p_address.m_kind == AddressKind::EnvPresence && p_envelope.m_kind == EnvelopeKind::Presence) {
			const auto* l_presence = std::get_if<Presence>(&p_envelope.m_payload);
			if (l_presence && !l_presence->m_online) {
				// Tell everyone, then clear the retained presence of the broker.
				if (const auto l_result = publish_bytes(l_key, l_bytes.value(), false); !l_result) {
					return l_result;
				}
				return publish_bytes(l_key, {}, true);
			}
			return publish_bytes(l_key, l_bytes.value(), true);
		}
		return publish_bytes(l_key, l_bytes.value(), false);
	}

	Capabilities MqttTransport::capabilities() const {
		Capabilities l_capabilities;
		l_capabilities.m_pubsub = true;
		l_capabilities.m_request_reply = false;
		l_capabilities.m_liveliness = false;
		l_capabilities.m_retained_presence = true;
		l_capabilities.m_ordered_per_sender = true;
		l_capabilities.m_at_least_once = true;
		l_capabilities.m_thread_safe_publish = true;
		return l_capabilities;
	}

	bool MqttTransport::is_connected() const {
		return m_client && m_client->m_client.is_connected();
	}

} // namespace cam::transport
