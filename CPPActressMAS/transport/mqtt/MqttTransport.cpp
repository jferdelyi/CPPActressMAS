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

#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <map>
#include <mutex>
#include <set>
#include <thread>

#include <mosquitto.h>
#include <mqtt_protocol.h>

#include "../../util/Identifier.h"
#include "../ProtobufCodec.h"

namespace cam::transport {

	namespace {

		constexpr int QOS = 1;

		Error transport_error(const std::string& p_message, const int p_native = 0) {
			return make_error(ErrorCategory::Transport, ErrorCode::TransportFailure, p_message, p_native);
		}

		// A libmosquitto return code, with the system error when the library only says "errno".
		Error library_error(const std::string& p_what, const int p_rc) {
			std::string l_message = p_what + ": " + mosquitto_strerror(p_rc);
			if (p_rc == MOSQ_ERR_ERRNO) {
				l_message += std::string(" (") + std::strerror(errno) + ")";
			}
			return transport_error(l_message, p_rc);
		}

		std::uint64_t now_ms() {
			return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
					std::chrono::system_clock::now().time_since_epoch()).count());
		}

		// libmosquitto is initialised once per process (mosquitto_lib_init is not thread-safe) and released at exit.
		void ensure_library() {
			static const struct Library {
				Library() { mosquitto_lib_init(); }
				~Library() { mosquitto_lib_cleanup(); }
			} s_library;
			(void) s_library;
		}

		// "tcp://host:port" or "ssl://host:port", as the facade builds it; a bracketed IPv6 host is accepted.
		bool split_endpoint(const std::string& p_endpoint, std::string& p_host, int& p_port) {
			std::string l_rest = p_endpoint;
			if (const auto l_scheme = l_rest.find("://"); l_scheme != std::string::npos) {
				l_rest = l_rest.substr(l_scheme + 3);
			}
			const auto l_colon = l_rest.rfind(':');
			const auto l_bracket = l_rest.rfind(']');
			p_port = 1883;
			if (l_colon == std::string::npos || (l_bracket != std::string::npos && l_bracket > l_colon)) {
				p_host = l_rest;
			} else {
				p_host = l_rest.substr(0, l_colon);
				try {
					p_port = std::stoi(l_rest.substr(l_colon + 1));
				} catch (const std::exception&) {
					return false;
				}
			}
			if (p_host.size() > 2 && p_host.front() == '[' && p_host.back() == ']') {
				p_host = p_host.substr(1, p_host.size() - 2);
			}
			return !p_host.empty() && p_port > 0 && p_port < 65536;
		}

	} // namespace

	/**
	 * The libmosquitto client, its network thread and the acknowledgements awaited (private: no
	 * libmosquitto type in the header). The callbacks run on the network thread, which the transport owns:
	 * none can run once the thread is joined.
	 **/
	struct MqttTransport::Client {
		struct mosquitto* m_handle = nullptr;
		std::thread m_loop;
		std::mutex m_mutex;
		std::condition_variable m_condition;
		bool m_connack = false;              ///< a CONNACK arrived (reason below)
		int m_connack_reason = 0;
		std::set<int> m_subscribed;          ///< message ids acknowledged by a SUBACK
		std::map<int, int> m_refused;        ///< message id -> reason code of a refused subscription
		std::set<int> m_published;           ///< message ids acknowledged by a PUBACK
		std::atomic<bool> m_connected{false};

		~Client() {
			if (m_loop.joinable()) {
				m_loop.join();
			}
			if (m_handle) {
				mosquitto_destroy(m_handle);
			}
		}

		static MqttTransport& self(void* p_object) { return *static_cast<MqttTransport*>(p_object); }

		static void on_connect(struct mosquitto*, void* p_object, const int p_reason, int, const mosquitto_property*) {
			self(p_object).on_connected(p_reason);
		}

		static void on_disconnect(struct mosquitto*, void* p_object, const int p_reason, const mosquitto_property*) {
			self(p_object).on_disconnected(p_reason);
		}

		static void on_message(struct mosquitto*, void* p_object, const struct mosquitto_message* p_message, const mosquitto_property*) {
			if (p_message && p_message->topic) {
				self(p_object).on_message(p_message->topic, static_cast<const std::uint8_t*>(p_message->payload),
										  p_message->payloadlen > 0 ? static_cast<std::size_t>(p_message->payloadlen) : 0);
			}
		}

		static void on_subscribe(struct mosquitto*, void* p_object, const int p_mid, const int p_count, const int* p_granted, const mosquitto_property*) {
			self(p_object).on_subscribed(p_mid, p_count, p_granted);
		}

		static void on_publish(struct mosquitto*, void* p_object, const int p_mid, int, const mosquitto_property*) {
			self(p_object).on_published(p_mid);
		}
	};

	MqttTransport::MqttTransport(TransportConfig p_config, MqttOptions p_options, std::shared_ptr<const EnvelopeCodec> p_codec) :
			m_config(std::move(p_config)),
			m_options(p_options),
			m_codec(p_codec ? std::move(p_codec) : std::make_shared<ProtobufCodec>()),
			m_node_id(cam::generate_uuid()),
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
				key_of(AddressKind::EnvAgent, p_endpoint.environment_id())};
	}

	std::vector<std::string> MqttTransport::shared_keys() const {
		return {key_of(AddressKind::AllIn), "cam/v1/" + m_config.m_namespace + "/env/+/presence",
				"cam/v1/" + m_config.m_namespace + "/node/+/presence"};
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

	// ---------------------------------------------------------------- libmosquitto calls

	Result<void> MqttTransport::subscribe(const std::vector<std::string>& p_keys) {
		if (!m_client || !m_client->m_connected.load()) {
			return {};  // subscribed on (re)connection
		}
		std::vector<int> l_mids;
		for (const auto& l_key: p_keys) {
			int l_mid = 0;
			const int l_rc = mosquitto_subscribe_v5(m_client->m_handle, &l_mid, l_key.c_str(), QOS,
													m_options.m_mqtt5 ? MQTT_SUB_OPT_NO_LOCAL : 0, nullptr);
			if (l_rc != MOSQ_ERR_SUCCESS) {
				return library_error("subscribe failed", l_rc);
			}
			l_mids.push_back(l_mid);
		}
		return wait_subscribed(l_mids);
	}

	Result<void> MqttTransport::wait_subscribed(const std::vector<int>& p_mids) {
		// Wait for each acknowledgement: a publication made before the broker knows the subscription
		// would never come back.
		std::unique_lock<std::mutex> l_lock(m_client->m_mutex);
		const auto l_deadline = std::chrono::steady_clock::now() + m_config.m_connect_timeout;
		for (const int l_mid: p_mids) {
			if (!m_client->m_condition.wait_until(l_lock, l_deadline, [&] { return m_client->m_subscribed.count(l_mid) > 0; })) {
				return make_error(ErrorCategory::Transport, ErrorCode::Timeout, "subscription not acknowledged by the broker");
			}
			m_client->m_subscribed.erase(l_mid);
			if (const auto l_refused = m_client->m_refused.find(l_mid); l_refused != m_client->m_refused.end()) {
				const int l_reason = l_refused->second;
				m_client->m_refused.erase(l_refused);
				return transport_error(std::string("subscription refused by the broker: ") + mosquitto_reason_string(l_reason), l_reason);
			}
		}
		return {};
	}

	Result<void> MqttTransport::unsubscribe(const std::vector<std::string>& p_keys) {
		if (!m_client || !m_client->m_connected.load()) {
			return {};
		}
		for (const auto& l_key: p_keys) {
			if (const int l_rc = mosquitto_unsubscribe_v5(m_client->m_handle, nullptr, l_key.c_str(), nullptr); l_rc != MOSQ_ERR_SUCCESS) {
				return library_error("unsubscribe failed", l_rc);
			}
		}
		return {};
	}

	Result<void> MqttTransport::publish_bytes(const std::string& p_key, const std::vector<std::uint8_t>& p_bytes, const bool p_retained) {
		if (!m_client || !m_client->m_handle) {
			return make_error(ErrorCategory::State, ErrorCode::NotConnected, "mqtt transport not open");
		}
		const int l_rc = mosquitto_publish_v5(m_client->m_handle, nullptr, p_key.c_str(), static_cast<int>(p_bytes.size()),
											  p_bytes.empty() ? nullptr : p_bytes.data(), QOS, p_retained, nullptr);
		if (l_rc != MOSQ_ERR_SUCCESS) {
			return library_error("publish failed", l_rc);
		}
		return {};
	}

	void MqttTransport::shutdown() {
		if (!m_client) {
			return;
		}
		if (m_client->m_handle) {
			(void) mosquitto_disconnect_v5(m_client->m_handle, 0, nullptr);  // the network loop returns
		}
		if (m_client->m_loop.joinable()) {
			m_client->m_loop.join();  // the callbacks still use m_client: none runs after this point
		}
		m_client.reset();
	}

	// ---------------------------------------------------------------- callbacks (network thread)

	void MqttTransport::on_connected(const int p_reason) {
		if (p_reason == 0) {
			m_client->m_connected.store(true);
		}
		{
			const std::lock_guard<std::mutex> l_lock(m_client->m_mutex);
			m_client->m_connack = true;
			m_client->m_connack_reason = p_reason;
		}
		m_client->m_condition.notify_all();
		if (p_reason != 0) {
			return;
		}
		// The first connection is completed by open(); only the recoveries are handled here: subscribe
		// again (the SUBSCRIBE packets leave before any later publication of the environments), announce
		// the node, then tell the endpoints.
		if (m_connected_once.exchange(true)) {
			std::vector<std::string> l_keys = shared_keys();
			m_registry.for_each(nullptr, [&](const Endpoint& p_endpoint) {
				for (auto& l_key: endpoint_keys(p_endpoint)) {
					l_keys.push_back(std::move(l_key));
				}
			});
			for (const auto& l_key: l_keys) {
				(void) mosquitto_subscribe_v5(m_client->m_handle, nullptr, l_key.c_str(), QOS, m_options.m_mqtt5 ? MQTT_SUB_OPT_NO_LOCAL : 0, nullptr);
			}
			if (const auto l_bytes = m_codec->encode(node_presence(true)); l_bytes) {
				(void) publish_bytes(key_of(AddressKind::NodePresence, m_node_id), l_bytes.value(), true);
			}
			m_registry.for_each(nullptr, [](Endpoint& p_endpoint) { p_endpoint.push_event(TransportEvent::Reconnected); });
		}
	}

	void MqttTransport::on_disconnected(const int p_reason) {
		m_client->m_connected.store(false);
		if (p_reason != 0) {
			// Unexpected: the loop reconnects by itself, the endpoints hear about the gap.
			m_registry.for_each(nullptr, [](Endpoint& p_endpoint) { p_endpoint.push_event(TransportEvent::Disconnected); });
		}
	}

	void MqttTransport::on_subscribed(const int p_mid, const int p_count, const int* p_granted) {
		{
			const std::lock_guard<std::mutex> l_lock(m_client->m_mutex);
			for (int i = 0; i < p_count; i++) {
				if (p_granted[i] >= 0x80) {
					m_client->m_refused[p_mid] = p_granted[i];
				}
			}
			m_client->m_subscribed.insert(p_mid);
		}
		m_client->m_condition.notify_all();
	}

	void MqttTransport::on_published(const int p_mid) {
		{
			const std::lock_guard<std::mutex> l_lock(m_client->m_mutex);
			m_client->m_published.insert(p_mid);
		}
		m_client->m_condition.notify_all();
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
		std::string l_host;
		int l_port = 0;
		if (m_config.m_endpoint.empty() || !split_endpoint(m_config.m_endpoint, l_host, l_port)) {
			return make_error(ErrorCategory::State, ErrorCode::InvalidConfiguration, "no broker address");
		}
		ensure_library();
		m_client = std::make_unique<Client>();
		m_client->m_handle = mosquitto_new(m_client_id.c_str(), false, this);  // clean start false: the session survives a gap
		if (!m_client->m_handle) {
			m_client.reset();
			return transport_error("mqtt client creation failed", errno);
		}
		struct mosquitto* l_handle = m_client->m_handle;
		mosquitto_int_option(l_handle, MOSQ_OPT_PROTOCOL_VERSION, m_options.m_mqtt5 ? MQTT_PROTOCOL_V5 : MQTT_PROTOCOL_V311);
		mosquitto_int_option(l_handle, MOSQ_OPT_TCP_NODELAY, 1);  // small packets leave at once (N18)
		mosquitto_reconnect_delay_set(l_handle, static_cast<unsigned int>(m_options.m_reconnect_min.count()),
									  static_cast<unsigned int>(m_options.m_reconnect_max.count()), true);
		mosquitto_connect_v5_callback_set(l_handle, &Client::on_connect);
		mosquitto_disconnect_v5_callback_set(l_handle, &Client::on_disconnect);
		mosquitto_message_v5_callback_set(l_handle, &Client::on_message);
		mosquitto_subscribe_v5_callback_set(l_handle, &Client::on_subscribe);
		mosquitto_publish_v5_callback_set(l_handle, &Client::on_publish);
		if (const auto l_will = m_codec->encode(node_presence(false)); l_will) {
			const int l_rc = mosquitto_will_set_v5(l_handle, key_of(AddressKind::NodePresence, m_node_id).c_str(),
												   static_cast<int>(l_will.value().size()), l_will.value().data(), QOS, true, nullptr);
			if (l_rc != MOSQ_ERR_SUCCESS) {
				m_client.reset();
				return library_error("will registration failed", l_rc);
			}
		}
		if (!m_config.m_username.empty()) {
			mosquitto_username_pw_set(l_handle, m_config.m_username.c_str(), m_config.m_password.empty() ? nullptr : m_config.m_password.c_str());
		}
		if (m_config.m_tls) {
			const int l_rc = mosquitto_tls_set(l_handle, m_config.m_ca_file.empty() ? nullptr : m_config.m_ca_file.c_str(), nullptr,
											   m_config.m_certificate_file.empty() ? nullptr : m_config.m_certificate_file.c_str(),
											   m_config.m_key_file.empty() ? nullptr : m_config.m_key_file.c_str(), nullptr);
			if (l_rc != MOSQ_ERR_SUCCESS) {
				m_client.reset();
				return library_error("TLS configuration failed", l_rc);
			}
			mosquitto_tls_opts_set(l_handle, m_config.m_verify_peer ? 1 : 0, nullptr, nullptr);
			mosquitto_tls_insecure_set(l_handle, !m_config.m_verify_peer);
		}
		mosquitto_property* l_properties = nullptr;
		if (m_options.m_mqtt5) {
			mosquitto_property_add_int32(&l_properties, MQTT_PROP_SESSION_EXPIRY_INTERVAL, static_cast<std::uint32_t>(m_config.m_session_expiry.count()));
		}
		mosquitto_threaded_set(l_handle, true);
		const int l_rc = mosquitto_connect_bind_v5(l_handle, l_host.c_str(), l_port, static_cast<int>(m_options.m_keep_alive.count()), nullptr, l_properties);
		mosquitto_property_free_all(&l_properties);
		if (l_rc != MOSQ_ERR_SUCCESS) {
			m_client.reset();
			return library_error("connection to " + m_config.m_endpoint + " failed", l_rc);
		}
		// The network thread: reads, dispatches the callbacks and reconnects by itself until shutdown().
		m_client->m_loop = std::thread([l_handle] { (void) mosquitto_loop_forever(l_handle, -1, 1); });
		{
			std::unique_lock<std::mutex> l_lock(m_client->m_mutex);
			if (!m_client->m_condition.wait_for(l_lock, m_config.m_connect_timeout, [&] { return m_client->m_connack; })) {
				l_lock.unlock();
				shutdown();
				return make_error(ErrorCategory::Transport, ErrorCode::Timeout, "no answer from the broker " + m_config.m_endpoint);
			}
			if (m_client->m_connack_reason != 0) {
				const int l_reason = m_client->m_connack_reason;
				l_lock.unlock();
				shutdown();
				return transport_error(std::string("connection refused by the broker: ") +
									   (m_options.m_mqtt5 ? mosquitto_reason_string(l_reason) : mosquitto_connack_string(l_reason)), l_reason);
			}
		}
		// The shared keys are subscribed and acknowledged here, before the first publication of any environment.
		if (const auto l_result = subscribe(shared_keys()); !l_result) {
			shutdown();
			return l_result;
		}
		if (const auto l_bytes = m_codec->encode(node_presence(true)); l_bytes) {
			if (const auto l_result = publish_bytes(key_of(AddressKind::NodePresence, m_node_id), l_bytes.value(), true); !l_result) {
				shutdown();
				return l_result;
			}
		}
		m_open.store(true);
		return {};
	}

	Result<void> MqttTransport::close(const std::chrono::milliseconds p_timeout) {
		if (!m_open.load()) {
			return make_error(ErrorCategory::State, ErrorCode::NotConnected, "mqtt transport not open");
		}
		m_open.store(false);
		if (m_client->m_connected.load()) {
			// Tell the others that the node leaves, and wait for the broker to hold it before disconnecting.
			if (const auto l_bytes = m_codec->encode(node_presence(false)); l_bytes) {
				int l_mid = 0;
				const int l_rc = mosquitto_publish_v5(m_client->m_handle, &l_mid, key_of(AddressKind::NodePresence, m_node_id).c_str(),
													  static_cast<int>(l_bytes.value().size()), l_bytes.value().data(), QOS, true, nullptr);
				if (l_rc == MOSQ_ERR_SUCCESS) {
					std::unique_lock<std::mutex> l_lock(m_client->m_mutex);
					(void) m_client->m_condition.wait_for(l_lock, p_timeout, [&] { return m_client->m_published.count(l_mid) > 0; });
				}
			}
		}
		shutdown();
		return {};
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
		return m_client && m_client->m_connected.load();
	}

} // namespace cam::transport
