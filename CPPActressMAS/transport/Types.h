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

#include <chrono>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>

#include "../Message.h"

/**
 * CPPActressMAS transport layer: basic types shared by the core, the codecs and the transports.
 * None of these types depends on a network library.
 **/
namespace cam::transport {

	/**
	 * Kind of destination of a publication (key scheme: cam/v1/<namespace>/...).
	 **/
	enum class AddressKind : std::uint8_t {
		EnvIn,        ///< cam/v1/<ns>/env/<uuid>/in : unicast, name or fragment aimed at one environment, replies
		EnvAgent,     ///< cam/v1/<ns>/env/<uuid>/agent : incoming migration
		EnvQuery,     ///< cam/v1/<ns>/env/<uuid>/query : requests (transports with request/reply)
		EnvPresence,  ///< cam/v1/<ns>/env/<uuid>/presence : presence of an environment and directory events
		AllIn,        ///< cam/v1/<ns>/all/in : explicit network scope (broadcast, name, fragment)
		NodePresence  ///< cam/v1/<ns>/node/<uuid>/presence : presence of a node (one transport instance)
	};

	/**
	 * Destination of a publication. The target is an environment id (EnvIn, EnvAgent, EnvQuery,
	 * EnvPresence), a node id (NodePresence) or empty (AllIn).
	 **/
	struct Address {
		/**
		 * Kind of destination.
		 **/
		AddressKind m_kind = AddressKind::EnvIn;

		/**
		 * Namespace of the key, the same for every environment of a simulation.
		 **/
		std::string m_namespace = "default";

		/**
		 * Environment id, node id, or empty for AllIn.
		 **/
		std::string m_target;

		/**
		 * Key of this address in the fixed-level scheme.
		 * @return The key, for example "cam/v1/default/env/<uuid>/in"
		 **/
		[[nodiscard]] std::string to_key() const;

		/**
		 * Parse a key of the fixed-level scheme.
		 * @param p_key The key
		 * @return The address, or nothing if the key does not follow the scheme
		 **/
		static std::optional<Address> parse(std::string_view p_key);

		/**
		 * Two addresses are equal when their kind, namespace and target are.
		 **/
		bool operator==(const Address&) const = default;
	};

	/**
	 * True if the text is an acceptable namespace: [a-z0-9-], 1 to 32 characters.
	 * @param p_namespace The text
	 * @return True if valid
	 **/
	[[nodiscard]] bool is_valid_namespace(std::string_view p_namespace);

	/**
	 * True if the text is an acceptable identifier for the wire: 1 to 128 printable ASCII characters,
	 * without slash, whitespace, '+', '#' or '*'.
	 * @param p_identifier The text
	 * @return True if valid
	 **/
	[[nodiscard]] bool is_valid_identifier(std::string_view p_identifier);

	/**
	 * True if the text is a canonical UUID (8-4-4-4-12 hexadecimal digits).
	 * @param p_text The text
	 * @return True if canonical
	 **/
	[[nodiscard]] bool is_canonical_uuid(std::string_view p_text);

	/**
	 * Scope of a send: local (the environment only) or network (local and the other environments).
	 **/
	enum class Scope : std::uint8_t {
		Local,
		Network
	};

	/**
	 * Per-message options. The defaults come from the environment: local scope, encoding chosen by
	 * the send overload, priority reserved (always 0 today).
	 **/
	struct SendOptions {
		/**
		 * Encoding of the content on the wire; empty: the one of the content (Text, Raw, MessagePack for JSON).
		 **/
		std::optional<ContentEncoding> m_encoding{};

		/**
		 * Local or network scope.
		 **/
		Scope m_scope = Scope::Local;

		/**
		 * Reserved, always 0 today.
		 **/
		std::uint32_t m_priority = 0;

		/**
		 * broadcast only: the sender receives its own message too, as includeSender of the C#; local, never on the wire.
		 **/
		bool m_include_sender = false;
	};

	/**
	 * What a transport can do; the core only requires pubsub.
	 **/
	struct Capabilities {
		/**
		 * Publish on keys and subscribe to them: the only capability the core requires.
		 **/
		bool m_pubsub = true;

		/**
		 * Queries answered by the other nodes (directory fallback).
		 **/
		bool m_request_reply = false;

		/**
		 * Presence of the nodes followed by the transport itself: the death of a process takes its environments offline.
		 **/
		bool m_liveliness = false;

		/**
		 * A newcomer receives the last presence of every attached environment.
		 **/
		bool m_retained_presence = false;

		/**
		 * The publications of one sender arrive in the order they were published.
		 **/
		bool m_ordered_per_sender = false;

		/**
		 * No publication is lost.
		 **/
		bool m_at_least_once = false;

		/**
		 * publish() may be called from several threads at once.
		 **/
		bool m_thread_safe_publish = false;
	};

	/**
	 * Configuration of a transport instance (one node: one connection, one namespace, one codec).
	 **/
	struct TransportConfig {
		/**
		 * Namespace of the keys ([a-z0-9-], 1 to 32 characters): the environments of one simulation share it.
		 **/
		std::string m_namespace = "default";

		/**
		 * Broker or peer to connect to ("tcp://host:port" or "ssl://host:port" for MQTT, "tcp/host:port" for Zenoh).
		 **/
		std::string m_endpoint{};

		/**
		 * Time open() waits for the connection.
		 **/
		std::chrono::milliseconds m_connect_timeout{5000};

		/**
		 * Time the MQTT broker keeps the session of a disconnected client.
		 **/
		std::chrono::seconds m_session_expiry{300};

		/**
		 * Envelope codec: "protobuf" (default), "compact" or "json".
		 **/
		std::string m_codec = "protobuf";

		/**
		 * Largest envelope sent or accepted; a larger one is refused.
		 **/
		std::size_t m_max_envelope_bytes = 1u << 20;

		/**
		 * TLS connection (mqtts).
		 **/
		bool m_tls = false;

		/**
		 * File of the certificate authority that signed the certificate of the broker (TLS).
		 **/
		std::string m_ca_file{};

		/**
		 * File of the certificate of the client (TLS).
		 **/
		std::string m_certificate_file{};

		/**
		 * File of the private key of the client (TLS).
		 **/
		std::string m_key_file{};

		/**
		 * Verify the certificate of the broker (TLS).
		 **/
		bool m_verify_peer = true;

		/**
		 * User name given to the MQTT broker.
		 **/
		std::string m_username{};

		/**
		 * Password given to the MQTT broker.
		 **/
		std::string m_password{};
	};

	/**
	 * Configuration of an endpoint (one environment attached to a transport).
	 **/
	struct EndpointConfig {
		/**
		 * Name of the environment, announced in its presence (EnvironmentInfo::m_name of the others).
		 **/
		std::string m_name{};

		/**
		 * Envelopes waiting for the next turn boundary; beyond, an arrival is dropped and counted (m_inbox_overflow).
		 **/
		std::size_t m_inbox_capacity = 65536;

		/**
		 * Envelopes waiting to be published; beyond, a send is refused (ErrorCode::OutboxFull).
		 **/
		std::size_t m_outbox_capacity = 65536;

		/**
		 * Sequences remembered by flow to recognize a duplicate.
		 **/
		std::size_t m_dedup_window = 4096;

		/**
		 * Envelopes held by flow behind a missing sequence; beyond, the missing ones are given up.
		 **/
		std::size_t m_reorder_capacity = 256;

		/**
		 * Turns a missing sequence is waited for before it is counted lost.
		 **/
		std::uint32_t m_reorder_turns = 2;

		/**
		 * Time a leaving agent waits for the answer of its destination; then it stays.
		 **/
		std::chrono::milliseconds m_migration_timeout{5000};

		/**
		 * Time an arrived agent waits for the confirmation of its origin; then its migration fails.
		 **/
		std::chrono::milliseconds m_accept_hold_timeout{15000};

		/**
		 * Turns during which the messages for an agent that left are forwarded to its new environment.
		 **/
		std::uint32_t m_forward_grace_turns = 10;

		/**
		 * Reserved: not used today.
		 **/
		std::uint32_t m_pending_agent_turns = 10;

		/**
		 * Publications of one flush at the turn boundary (0: no limit).
		 **/
		std::size_t m_flush_max_messages = 0;

		/**
		 * Time of one flush at the turn boundary; the rest waits for the next one.
		 **/
		std::chrono::milliseconds m_flush_max_time{50};
	};

	/**
	 * Counters of an endpoint. Losses are counted, never silent.
	 **/
	struct Statistics {
		/**
		 * Envelopes published.
		 **/
		std::uint64_t m_sent = 0;

		/**
		 * Envelopes accepted in the inbox.
		 **/
		std::uint64_t m_received = 0;

		/**
		 * Envelopes handed to the environment, in order.
		 **/
		std::uint64_t m_delivered = 0;

		/**
		 * Envelopes received twice, dropped.
		 **/
		std::uint64_t m_duplicates = 0;

		/**
		 * Envelopes delivered out of order: late, or behind a sequence given up.
		 **/
		std::uint64_t m_out_of_order = 0;

		/**
		 * Sequences given up, and publications that failed.
		 **/
		std::uint64_t m_lost = 0;

		/**
		 * Envelopes refused: undecodable, invalid or inconsistent.
		 **/
		std::uint64_t m_rejected = 0;

		/**
		 * Envelopes of this environment received back, dropped.
		 **/
		std::uint64_t m_echo = 0;

		/**
		 * Arrivals dropped because the inbox was full.
		 **/
		std::uint64_t m_inbox_overflow = 0;

		/**
		 * Sends refused because the outbox was full.
		 **/
		std::uint64_t m_outbox_overflow = 0;

		/**
		 * Messages forwarded to the new environment of an agent that left.
		 **/
		std::uint64_t m_forwarded = 0;

		/**
		 * Migration controls about a transfer unknown or already finished.
		 **/
		std::uint64_t m_stale_control = 0;

		/**
		 * Messages for an agent found nowhere.
		 **/
		std::uint64_t m_unknown_destination = 0;

		/**
		 * Agents that left or arrived with a confirmed migration.
		 **/
		std::uint64_t m_migrations_completed = 0;

		/**
		 * Refused, timed out or abandoned migrations.
		 **/
		std::uint64_t m_migrations_failed = 0;
	};

	/**
	 * What an environment knows about another one (snapshot value, never a pointer).
	 **/
	struct EnvironmentInfo {
		/**
		 * Id of the environment.
		 **/
		std::string m_id;

		/**
		 * Name of the environment.
		 **/
		std::string m_name;

		/**
		 * Id of the node (transport instance) it is attached to.
		 **/
		std::string m_node;

		/**
		 * True while it is online.
		 **/
		bool m_online = false;

		/**
		 * When its last presence arrived.
		 **/
		std::chrono::steady_clock::time_point m_last_seen;
	};

	/**
	 * Events of the connection of a transport, delivered to the environments at the turn boundary.
	 **/
	enum class TransportEvent : std::uint8_t {
		Connected,
		Disconnected,
		Reconnected,
		Error
	};

	/**
	 * Name of a transport event.
	 * @param p_event The event
	 * @return Its name
	 **/
	[[nodiscard]] const char* to_string(TransportEvent p_event);

	/**
	 * Error categories.
	 **/
	enum class ErrorCategory : std::uint8_t {
		Transport,
		Validation,
		Routing,
		Capacity,
		State
	};

	/**
	 * Error codes.
	 **/
	enum class ErrorCode : std::uint8_t {
		None,
		NotConnected,
		AlreadyConnected,
		NotAttached,
		UnknownDestination,
		TooLarge,
		OutboxFull,
		InboxFull,
		InvalidTarget,
		InvalidEnvelope,
		UnsupportedCodec,
		UnsupportedVersion,
		Timeout,
		TransportFailure,
		UnknownAgentType,
		InvalidConfiguration,
		NotSupported,
		InTransit
	};

	/**
	 * Error value: category, code, message and the native code of the transport, if any.
	 **/
	struct Error {
		/**
		 * Category of the error.
		 **/
		ErrorCategory m_category = ErrorCategory::State;

		/**
		 * Code of the error.
		 **/
		ErrorCode m_code = ErrorCode::None;

		/**
		 * Readable description.
		 **/
		std::string m_message;

		/**
		 * Native code of the transport library, 0 if none.
		 **/
		int m_native = 0;
	};

	/**
	 * Exception carrying a transport error, thrown where no Result can be returned: Environment::start() when the URL
	 * of its configuration does not connect.
	 **/
	struct TransportError final : std::runtime_error {
		/**
		 * The error.
		 **/
		Error m_error;

		/**
		 * Create the exception of an error.
		 * @param p_error The error
		 **/
		explicit TransportError(Error p_error) : std::runtime_error(p_error.m_message), m_error(std::move(p_error)) {}
	};

	/**
	 * Name of an error code.
	 * @param p_code The code
	 * @return Its name
	 **/
	[[nodiscard]] const char* to_string(ErrorCode p_code);

	/**
	 * Result of an operation: a value or an error (no exception crosses the transport layer).
	 **/
	template<typename T>
	class Result final {
	private:
		/**
		 * The value, or the error.
		 **/
		std::variant<T, Error> m_value;

	public:
		/**
		 * Create a successful result.
		 * @param p_value The value
		 **/
		Result(T p_value) : m_value(std::move(p_value)) {}  // NOLINT(google-explicit-constructor)

		/**
		 * Create a failed result.
		 * @param p_error The error
		 **/
		Result(Error p_error) : m_value(std::move(p_error)) {}  // NOLINT(google-explicit-constructor)

		/**
		 * True if the operation succeeded.
		 * @return True if there is a value
		 **/
		[[nodiscard]] bool has_value() const { return std::holds_alternative<T>(m_value); }

		/**
		 * True if the operation succeeded.
		 * @return has_value()
		 **/
		explicit operator bool() const { return has_value(); }

		/**
		 * The value; the result must have one.
		 * @return The value
		 **/
		[[nodiscard]] T& value() { return std::get<T>(m_value); }

		/**
		 * The value; the result must have one.
		 * @return The value
		 **/
		[[nodiscard]] const T& value() const { return std::get<T>(m_value); }

		/**
		 * The error; the result must have one.
		 * @return The error
		 **/
		[[nodiscard]] const Error& error() const { return std::get<Error>(m_value); }
	};

	/**
	 * Result without value: success or an error.
	 **/
	template<>
	class Result<void> final {
	private:
		/**
		 * The error, empty on success.
		 **/
		std::optional<Error> m_error;

	public:
		/**
		 * Create a successful result.
		 **/
		Result() = default;

		/**
		 * Create a failed result.
		 * @param p_error The error
		 **/
		Result(Error p_error) : m_error(std::move(p_error)) {}  // NOLINT(google-explicit-constructor)

		/**
		 * True if the operation succeeded.
		 * @return True if there is no error
		 **/
		[[nodiscard]] bool has_value() const { return !m_error.has_value(); }

		/**
		 * True if the operation succeeded.
		 * @return has_value()
		 **/
		explicit operator bool() const { return has_value(); }

		/**
		 * The error; the result must have one.
		 * @return The error
		 **/
		[[nodiscard]] const Error& error() const { return *m_error; }
	};

	/**
	 * Build an error.
	 * @param p_category The category
	 * @param p_code The code
	 * @param p_message The message
	 * @param p_native The native code, if any
	 * @return The error
	 **/
	inline Error make_error(const ErrorCategory p_category, const ErrorCode p_code, std::string p_message, const int p_native = 0) {
		return Error{p_category, p_code, std::move(p_message), p_native};
	}

} // namespace cam::transport
