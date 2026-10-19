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

#include "ZenohTransport.h"

#include <chrono>
#include <map>
#include <mutex>
#include <optional>
#include <thread>

#include <zenoh.hxx>

#include "../../util/Identifier.h"
#include "../codec/ProtobufCodec.h"

#if defined(__SANITIZE_THREAD__)
#define CAM_THREAD_SANITIZER 1
#elif defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define CAM_THREAD_SANITIZER 1
#endif
#endif

#if defined(CAM_THREAD_SANITIZER)
extern "C" void __tsan_acquire(void* p_address);
extern "C" void __tsan_release(void* p_address);
#endif

namespace cam::transport {

	namespace {

		/**
		 * zenoh-c hands our closures to its Rust threads with a synchronisation ThreadSanitizer cannot see (the
		 * runtime is not instrumented): these two calls tell it that what a thread wrote before handing a
		 * closure over happens before the callbacks that follow. They compile to nothing without the sanitizer.
		 **/
		void release_to_callbacks([[maybe_unused]] void* p_sync) {
#if defined(CAM_THREAD_SANITIZER)
			__tsan_release(p_sync);
#endif
		}

		void acquire_from_declarer([[maybe_unused]] void* p_sync) {
#if defined(CAM_THREAD_SANITIZER)
			__tsan_acquire(p_sync);
#endif
		}


		Error transport_error(const std::string& p_message, const int p_native = 0) {
			return make_error(ErrorCategory::Transport, ErrorCode::TransportFailure, p_message, p_native);
		}

		std::uint64_t now_ms() {
			return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
					std::chrono::system_clock::now().time_since_epoch()).count());
		}

		// JSON5 array of strings for the endpoints of the configuration.
		std::string json_array(const std::vector<std::string>& p_values) {
			std::string l_result = "[";
			for (std::size_t i = 0; i < p_values.size(); i++) {
				if (i > 0) l_result += ",";
				l_result += "\"" + p_values[i] + "\"";
			}
			return l_result + "]";
		}

		std::vector<std::uint8_t> to_vector(const zenoh::Bytes& p_bytes) {
			return p_bytes.as_vector();
		}

		zenoh::Session::PutOptions put_options() {
			zenoh::Session::PutOptions l_options;
			l_options.congestion_control = Z_CONGESTION_CONTROL_BLOCK;
			l_options.priority = Z_PRIORITY_DATA;
			l_options.allowed_destination = Z_LOCALITY_REMOTE;
			return l_options;
		}

		zenoh::Session::SubscriberOptions remote_only() {
			zenoh::Session::SubscriberOptions l_options;
			l_options.allowed_origin = Z_LOCALITY_REMOTE;
			return l_options;
		}

	} // namespace

	/**
	 * The Zenoh session and the declarations alive with it (private: no Zenoh type in the header).
	 * Subscribers, queryables and tokens are RAII objects: their destruction undeclares them.
	 **/
	struct ZenohTransport::Node {
		struct Handles {
			std::vector<zenoh::Subscriber<void>> m_subscribers;
			std::optional<zenoh::Queryable<void>> m_queryable;
			std::optional<zenoh::LivelinessToken> m_token;
		};

		zenoh::Session m_session;
		std::vector<zenoh::Subscriber<void>> m_shared;
		std::optional<zenoh::Subscriber<void>> m_liveliness;
		std::optional<zenoh::LivelinessToken> m_node_token;
		std::mutex m_mutex;
		std::map<std::string, Handles> m_endpoints;  ///< environment id -> declarations

		explicit Node(zenoh::Config&& p_config) : m_session(zenoh::Session::open(std::move(p_config))) {}
	};

	ZenohTransport::CallbackGuard::CallbackGuard(std::atomic<int>& p_counter) : m_counter(p_counter) {
		acquire_from_declarer(&m_counter);
		++m_counter;
	}

	ZenohTransport::ZenohTransport(TransportConfig p_config, ZenohOptions p_options, std::shared_ptr<const EnvelopeCodec> p_codec) :
			m_config(std::move(p_config)),
			m_options(std::move(p_options)),
			m_codec(p_codec ? std::move(p_codec) : std::make_shared<ProtobufCodec>()),
			m_node_id(cam::generate_uuid()) {
	}

	ZenohTransport::~ZenohTransport() {
		if (m_open.load()) {
			(void) close(std::chrono::milliseconds(2000));
		}
	}

	// ---------------------------------------------------------------- keys

	std::string ZenohTransport::key_of(const AddressKind p_kind, const std::string& p_target) const {
		return Address{p_kind, m_config.m_namespace, p_target}.to_key();
	}

	std::string ZenohTransport::live_prefix() const {
		return "cam/v1/" + m_config.m_namespace + "/live/node/";
	}

	// ---------------------------------------------------------------- callbacks (Zenoh threads): deposit only, never throw

	void ZenohTransport::on_sample(const std::string& p_key, std::vector<std::uint8_t>&& p_bytes) {
		if (p_bytes.size() > m_config.m_max_envelope_bytes) {
			++m_oversized;
			return;
		}
		const auto l_address = Address::parse(p_key);
		if (!l_address || l_address->m_namespace != m_config.m_namespace || p_bytes.empty()) {
			return;
		}
		switch (l_address->m_kind) {
			case AddressKind::EnvIn:
			case AddressKind::EnvAgent:
			case AddressKind::EnvQuery:
				m_registry.with(l_address->m_target, [&](Endpoint& p_target) { p_target.deliver(*l_address, std::move(p_bytes)); });
				break;
			case AddressKind::EnvPresence:
				if (const auto l_decoded = m_codec->decode(p_bytes, m_config.m_max_envelope_bytes); l_decoded) {
					m_presence.remember(*l_address, l_decoded.value());
				}
				[[fallthrough]];
			default:
				m_registry.for_each(nullptr, [&](Endpoint& p_endpoint) {
					std::vector<std::uint8_t> l_copy = p_bytes;
					p_endpoint.deliver(*l_address, std::move(l_copy));
				});
				break;
		}
	}

	void ZenohTransport::deliver_presence(const std::string& p_environment_id, Envelope&& p_envelope) {
		const Address l_address{AddressKind::EnvPresence, m_config.m_namespace, p_environment_id};
		m_registry.for_each(nullptr, [&](Endpoint& p_endpoint) {
			Envelope l_copy = p_envelope;
			p_endpoint.deliver(l_address, std::move(l_copy));
		});
	}

	void ZenohTransport::on_liveliness(const std::string& p_key, const bool p_alive) {
		// cam/v1/<ns>/live/node/<node>            : a node
		// cam/v1/<ns>/live/node/<node>/env/<env>  : an environment of that node
		const std::string l_prefix = live_prefix();
		if (p_key.rfind(l_prefix, 0) != 0) {
			return;
		}
		const std::string l_rest = p_key.substr(l_prefix.size());
		const std::size_t l_slash = l_rest.find('/');
		const std::string l_node = l_rest.substr(0, l_slash);
		std::string l_environment;
		if (l_slash != std::string::npos) {
			const std::string l_tail = l_rest.substr(l_slash + 1);
			if (l_tail.rfind("env/", 0) != 0) {
				return;
			}
			l_environment = l_tail.substr(4);
		}
		if (l_node == m_node_id || !is_valid_identifier(l_node) || (!l_environment.empty() && !is_valid_identifier(l_environment))) {
			return;
		}
		if (p_alive) {
			if (!l_environment.empty() && !m_registry.is_local(l_environment)) {
				request_presence(l_environment);
			}
			return;
		}
		// Gone: an offline presence for the environment, or for the node (environment id = node id).
		Envelope l_envelope;
		l_envelope.m_kind = EnvelopeKind::Presence;
		l_envelope.m_sender_environment = l_environment.empty() ? l_node : l_environment;
		l_envelope.m_epoch = 0;  // synthesized here: never a real sender's epoch
		l_envelope.m_stream_kind = StreamKind::Presence;
		l_envelope.m_stream_target = l_envelope.m_sender_environment;
		l_envelope.m_sequence = now_ms();
		Presence l_presence;
		l_presence.m_node_id = l_node;
		l_presence.m_environment_id = l_environment.empty() ? l_node : l_environment;
		l_presence.m_namespace = m_config.m_namespace;
		l_presence.m_online = false;
		l_presence.m_timestamp_ms = now_ms();
		l_envelope.m_payload = std::move(l_presence);
		m_presence.forget(l_presence.m_environment_id);
		deliver_presence(l_envelope.m_sender_environment, std::move(l_envelope));
	}

	void ZenohTransport::on_presence_reply(const std::string& p_environment_id, std::vector<std::uint8_t>&& p_bytes) {
		if (p_bytes.empty() || p_bytes.size() > m_config.m_max_envelope_bytes) {
			return;
		}
		const Address l_address{AddressKind::EnvPresence, m_config.m_namespace, p_environment_id};
		const auto l_decoded = m_codec->decode(p_bytes, m_config.m_max_envelope_bytes);
		if (!l_decoded) {
			return;
		}
		m_presence.remember(l_address, l_decoded.value());
		m_registry.for_each(nullptr, [&](Endpoint& p_endpoint) {
			std::vector<std::uint8_t> l_copy = p_bytes;
			p_endpoint.deliver(l_address, std::move(l_copy));
		});
	}

	void ZenohTransport::request_presence(const std::string& p_environment_id) {
		// Asked from a liveliness callback: the query itself runs on the worker.
		m_worker.post([this, p_environment_id] {
			if (!m_open.load() || !m_node) {
				return;
			}
			try {
				zenoh::Session::GetOptions l_options;
				l_options.timeout_ms = 2000;
				l_options.target = Z_QUERY_TARGET_BEST_MATCHING;
				m_node->m_session.get(
						zenoh::KeyExpr(key_of(AddressKind::EnvQuery, p_environment_id)), "what=presence",
						[this, p_environment_id](const zenoh::Reply& p_reply) {
							CallbackGuard l_guard(m_callbacks);
							try {
								if (p_reply.is_ok()) {
									on_presence_reply(p_environment_id, to_vector(p_reply.get_ok().get_payload()));
								}
							} catch (...) {
							}
						},
						zenoh::closures::none, std::move(l_options));
				release_to_callbacks(&m_callbacks);
			} catch (const zenoh::ZException&) {
				// The peer vanished between its token and our question: its token DELETE will follow.
			}
		});
	}

	// ---------------------------------------------------------------- Transport

	Result<void> ZenohTransport::open() {
		if (m_open.load()) {
			return make_error(ErrorCategory::State, ErrorCode::AlreadyConnected, "zenoh transport already open");
		}
		if (!is_valid_namespace(m_config.m_namespace)) {
			return make_error(ErrorCategory::State, ErrorCode::InvalidConfiguration, "invalid namespace");
		}
		if (m_options.m_mode != "peer" && m_options.m_mode != "client") {
			return make_error(ErrorCategory::State, ErrorCode::InvalidConfiguration, "zenoh mode must be peer or client");
		}
		if (m_options.m_connect.empty() && m_options.m_listen.empty() && !m_options.m_multicast_scouting) {
			return make_error(ErrorCategory::State, ErrorCode::InvalidConfiguration,
							  "zenoh needs an endpoint: zenoh://host:port, ?listen=tcp/host:port or ?scouting=multicast");
		}
		try {
			zenoh::Config l_config = zenoh::Config::create_default();
			l_config.insert_json5("mode", "\"" + m_options.m_mode + "\"");
			l_config.insert_json5("scouting/multicast/enabled", m_options.m_multicast_scouting ? "true" : "false");
			if (!m_options.m_connect.empty()) {
				l_config.insert_json5("connect/endpoints", json_array(m_options.m_connect));
			}
			if (!m_options.m_listen.empty()) {
				l_config.insert_json5("listen/endpoints", json_array(m_options.m_listen));
			}
			release_to_callbacks(&m_callbacks);
			m_node = std::make_unique<Node>(std::move(l_config));

			// Shared keys of the namespace: the explicit network scope and the presence of every environment.
			for (const std::string& l_key: {key_of(AddressKind::AllIn), "cam/v1/" + m_config.m_namespace + "/env/*/presence"}) {
				m_node->m_shared.push_back(m_node->m_session.declare_subscriber(
						zenoh::KeyExpr(l_key),
						[this](const zenoh::Sample& p_sample) {
							CallbackGuard l_guard(m_callbacks);
							try {
								on_sample(std::string(p_sample.get_keyexpr().as_string_view()), to_vector(p_sample.get_payload()));
							} catch (...) {
							}
						},
						zenoh::closures::none, remote_only()));
			}
			// Liveliness of the other nodes and environments, history included for the newcomer we are.
			zenoh::Session::LivelinessSubscriberOptions l_live_options;
			l_live_options.history = true;
			m_node->m_liveliness.emplace(m_node->m_session.liveliness_declare_subscriber(
					zenoh::KeyExpr(live_prefix() + "**"),
					[this](const zenoh::Sample& p_sample) {
						CallbackGuard l_guard(m_callbacks);
						try {
							on_liveliness(std::string(p_sample.get_keyexpr().as_string_view()), p_sample.get_kind() == Z_SAMPLE_KIND_PUT);
						} catch (...) {
						}
					},
					zenoh::closures::none, std::move(l_live_options)));
			m_node->m_node_token.emplace(m_node->m_session.liveliness_declare_token(zenoh::KeyExpr(live_prefix() + m_node_id)));
			release_to_callbacks(&m_callbacks);

			// With explicit peers to reach, wait for the first link (bounded): a publication made before
			// the peers know our subscriptions would be lost, and an unreachable peer is an error, as a
			// broker would be.
			if (!m_options.m_connect.empty()) {
				const auto l_deadline = std::chrono::steady_clock::now() + m_config.m_connect_timeout;
				while (m_node->m_session.get_peers_z_id().empty() && m_node->m_session.get_routers_z_id().empty()) {
					if (std::chrono::steady_clock::now() >= l_deadline) {
						m_node->m_session.close();
						wait_callbacks();
						m_node.reset();
						return make_error(ErrorCategory::Transport, ErrorCode::Timeout,
										  "no zenoh peer reached at " + json_array(m_options.m_connect));
					}
					std::this_thread::sleep_for(std::chrono::milliseconds(20));
				}
				// Let the declarations (subscribers, tokens) cross the new link before the first publication.
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
			}
		} catch (const zenoh::ZException& p_exception) {
			m_node.reset();
			return transport_error(std::string("zenoh session failed: ") + p_exception.what(), p_exception.e);
		}
		m_worker.start();
		m_open.store(true);
		return {};
	}

	Result<void> ZenohTransport::close(const std::chrono::milliseconds) {
		if (!m_open.load()) {
			return make_error(ErrorCategory::State, ErrorCode::NotConnected, "zenoh transport not open");
		}
		m_open.store(false);
		m_worker.stop();
		Result<void> l_result;
		try {
			{
				const std::lock_guard<std::mutex> l_lock(m_node->m_mutex);
				m_node->m_endpoints.clear();  // undeclares: tokens go, peers see the environments leave
			}
			m_node->m_node_token.reset();
			m_node->m_liveliness.reset();
			m_node->m_shared.clear();
			m_node->m_session.close();
		} catch (const zenoh::ZException& p_exception) {
			l_result = transport_error(std::string("zenoh close failed: ") + p_exception.what(), p_exception.e);
		}
		wait_callbacks();
		m_node.reset();
		return l_result;
	}

	void ZenohTransport::wait_callbacks() {
		// Undeclaring and closing do not wait for a callback already running on a Zenoh thread: give it
		// (bounded) time to leave before anything it may touch is destroyed.
		const auto l_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
		while (m_callbacks.load() > 0 && std::chrono::steady_clock::now() < l_deadline) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}

	Result<void> ZenohTransport::attach(Endpoint& p_endpoint) {
		if (!m_open.load()) {
			return make_error(ErrorCategory::State, ErrorCode::NotConnected, "zenoh transport not open");
		}
		if (!m_registry.add(p_endpoint)) {
			return make_error(ErrorCategory::State, ErrorCode::AlreadyConnected, "endpoint already attached");
		}
		const std::string l_environment = p_endpoint.environment_id();
		try {
			Node::Handles l_handles;
			for (const AddressKind l_kind: {AddressKind::EnvIn, AddressKind::EnvAgent}) {
				l_handles.m_subscribers.push_back(m_node->m_session.declare_subscriber(
						zenoh::KeyExpr(key_of(l_kind, l_environment)),
						[this](const zenoh::Sample& p_sample) {
							CallbackGuard l_guard(m_callbacks);
							try {
								on_sample(std::string(p_sample.get_keyexpr().as_string_view()), to_vector(p_sample.get_payload()));
							} catch (...) {
							}
						},
						zenoh::closures::none, remote_only()));
			}
			// The queryable serves the current presence of this environment to the newcomers.
			zenoh::Session::QueryableOptions l_queryable_options;
			l_queryable_options.complete = true;
			l_queryable_options.allowed_origin = Z_LOCALITY_REMOTE;
			l_handles.m_queryable.emplace(m_node->m_session.declare_queryable(
					zenoh::KeyExpr(key_of(AddressKind::EnvQuery, l_environment)),
					[this, l_environment](const zenoh::Query& p_query) {
						CallbackGuard l_guard(m_callbacks);
						try {
							if (p_query.get_parameters().find("what=presence") == std::string_view::npos) {
								return;
							}
							const auto l_entry = m_presence.find(l_environment);
							if (!l_entry) {
								return;
							}
							if (auto l_encoded = m_codec->encode(*l_entry); l_encoded) {
								p_query.reply(zenoh::KeyExpr(key_of(AddressKind::EnvPresence, l_environment)),
											  zenoh::Bytes(std::move(l_encoded.value())));
							}
						} catch (...) {
						}
					},
					zenoh::closures::none, std::move(l_queryable_options)));
			l_handles.m_token.emplace(m_node->m_session.liveliness_declare_token(
					zenoh::KeyExpr(live_prefix() + m_node_id + "/env/" + l_environment)));
			release_to_callbacks(&m_callbacks);
			const std::lock_guard<std::mutex> l_lock(m_node->m_mutex);
			m_node->m_endpoints.emplace(l_environment, std::move(l_handles));
		} catch (const zenoh::ZException& p_exception) {
			m_registry.remove(p_endpoint);
			return transport_error(std::string("zenoh declarations failed: ") + p_exception.what(), p_exception.e);
		}
		m_presence.replay(p_endpoint);
		p_endpoint.push_event(TransportEvent::Connected);
		return {};
	}

	Result<void> ZenohTransport::detach(Endpoint& p_endpoint) {
		if (!m_registry.remove(p_endpoint)) {
			return make_error(ErrorCategory::State, ErrorCode::NotAttached, "endpoint not attached");
		}
		m_presence.forget(p_endpoint.environment_id());
		if (m_node) {
			const std::lock_guard<std::mutex> l_lock(m_node->m_mutex);
			m_node->m_endpoints.erase(p_endpoint.environment_id());  // the token goes: peers see the environment leave
		}
		return {};
	}

	Result<void> ZenohTransport::publish_bytes(const std::string& p_key, std::vector<std::uint8_t>&& p_bytes) {
		try {
			m_node->m_session.put(zenoh::KeyExpr(p_key), zenoh::Bytes(std::move(p_bytes)), put_options());
		} catch (const zenoh::ZException& p_exception) {
			return transport_error(std::string("zenoh put failed: ") + p_exception.what(), p_exception.e);
		}
		return {};
	}

	Result<void> ZenohTransport::publish(const Endpoint& p_sender, const Address& p_address, const Envelope& p_envelope) {
		if (!m_open.load()) {
			return make_error(ErrorCategory::State, ErrorCode::NotConnected, "zenoh transport not open");
		}
		if (p_address.m_namespace != m_config.m_namespace) {
			return make_error(ErrorCategory::Routing, ErrorCode::InvalidTarget, "address outside the transport namespace");
		}
		m_presence.remember(p_address, p_envelope);
		const auto l_delivery = m_registry.deliver_locally(&p_sender, p_address, p_envelope);
		if (!l_delivery.m_needs_network || p_address.m_kind == AddressKind::NodePresence) {
			return {};  // the node presence is a liveliness token here
		}
		auto l_bytes = m_codec->encode(p_envelope);
		if (!l_bytes) {
			return l_bytes.error();
		}
		if (l_bytes.value().size() > m_config.m_max_envelope_bytes) {
			return make_error(ErrorCategory::Validation, ErrorCode::TooLarge, "envelope larger than the configured maximum");
		}
		return publish_bytes(p_address.to_key(), std::move(l_bytes.value()));
	}

	Capabilities ZenohTransport::capabilities() const {
		Capabilities l_capabilities;
		l_capabilities.m_pubsub = true;
		l_capabilities.m_request_reply = true;
		l_capabilities.m_liveliness = true;
		l_capabilities.m_retained_presence = false;
		l_capabilities.m_ordered_per_sender = true;
		l_capabilities.m_at_least_once = true;
		l_capabilities.m_thread_safe_publish = true;
		return l_capabilities;
	}

	std::string ZenohTransport::session_id() const {
		if (!m_open.load() || !m_node) {
			return {};
		}
		return m_node->m_session.get_zid().to_string();
	}

} // namespace cam::transport
