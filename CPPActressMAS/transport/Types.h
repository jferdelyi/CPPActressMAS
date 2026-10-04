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

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include "../Message.h"

/**
 * CPPActressMAS transport layer: basic types shared by the core, the codecs and the transports.
 * None of these types depends on a network library.
 */
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
		AddressKind m_kind = AddressKind::EnvIn;
		std::string m_namespace = "default";
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
	 * Per-message options. The defaults come from the environment (D19): local scope, encoding chosen by
	 * the send overload, priority reserved (always 0 today).
	 **/
	struct SendOptions {
		std::optional<MessageBinaryFormat> m_encoding;
		Scope m_scope = Scope::Local;
		std::uint32_t m_priority = 0;
	};

	/**
	 * What a transport can do; the core only requires pubsub.
	 **/
	struct Capabilities {
		bool m_pubsub = true;
		bool m_request_reply = false;
		bool m_liveliness = false;
		bool m_retained_presence = false;
		bool m_ordered_per_sender = false;
		bool m_at_least_once = false;
		bool m_thread_safe_publish = false;
	};

	/**
	 * Configuration of a transport instance (one node: one connection, one namespace, one codec).
	 **/
	struct TransportConfig {
		std::string m_namespace = "default";
		std::string m_endpoint;
		std::chrono::milliseconds m_connect_timeout{5000};
		std::chrono::seconds m_session_expiry{300};
		std::string m_codec = "compact";
		std::size_t m_max_envelope_bytes = 1u << 20;
		bool m_tls = false;
		std::string m_ca_file;
		std::string m_certificate_file;
		std::string m_key_file;
		bool m_verify_peer = true;
		std::string m_username;
		std::string m_password;
	};

	/**
	 * Configuration of an endpoint (one environment attached to a transport).
	 **/
	struct EndpointConfig {
		std::string m_name;
		std::size_t m_inbox_capacity = 65536;
		std::size_t m_outbox_capacity = 65536;
		std::size_t m_dedup_window = 4096;
		std::size_t m_reorder_capacity = 256;
		std::uint32_t m_reorder_turns = 2;
		std::chrono::milliseconds m_migration_timeout{5000};
		std::chrono::milliseconds m_accept_hold_timeout{15000};
		std::uint32_t m_forward_grace_turns = 10;
		std::uint32_t m_pending_agent_turns = 10;
		std::size_t m_flush_max_messages = 0;
		std::chrono::milliseconds m_flush_max_time{50};
	};

	/**
	 * Counters of an endpoint. Losses are counted, never silent.
	 **/
	struct Statistics {
		std::uint64_t m_sent = 0;
		std::uint64_t m_received = 0;
		std::uint64_t m_delivered = 0;
		std::uint64_t m_duplicates = 0;
		std::uint64_t m_out_of_order = 0;
		std::uint64_t m_lost = 0;
		std::uint64_t m_rejected = 0;
		std::uint64_t m_echo = 0;
		std::uint64_t m_inbox_overflow = 0;
		std::uint64_t m_outbox_overflow = 0;
		std::uint64_t m_forwarded = 0;
		std::uint64_t m_stale_control = 0;
		std::uint64_t m_unknown_destination = 0;
		std::uint64_t m_migrations_completed = 0;  ///< agents that left or arrived with a confirmed migration
		std::uint64_t m_migrations_failed = 0;     ///< refused, timed out or abandoned migrations
	};

	/**
	 * What an environment knows about another one (snapshot value, never a pointer).
	 **/
	struct EnvironmentInfo {
		std::string m_id;
		std::string m_name;
		std::string m_node;
		bool m_online = false;
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
		ErrorCategory m_category = ErrorCategory::State;
		ErrorCode m_code = ErrorCode::None;
		std::string m_message;
		int m_native = 0;
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
		std::variant<T, Error> m_value;

	public:
		Result(T p_value) : m_value(std::move(p_value)) {}  // NOLINT(google-explicit-constructor)
		Result(Error p_error) : m_value(std::move(p_error)) {}  // NOLINT(google-explicit-constructor)

		[[nodiscard]] bool has_value() const { return std::holds_alternative<T>(m_value); }
		explicit operator bool() const { return has_value(); }
		[[nodiscard]] T& value() { return std::get<T>(m_value); }
		[[nodiscard]] const T& value() const { return std::get<T>(m_value); }
		[[nodiscard]] const Error& error() const { return std::get<Error>(m_value); }
	};

	/**
	 * Result without value: success or an error.
	 **/
	template<>
	class Result<void> final {
	private:
		std::optional<Error> m_error;

	public:
		Result() = default;
		Result(Error p_error) : m_error(std::move(p_error)) {}  // NOLINT(google-explicit-constructor)

		[[nodiscard]] bool has_value() const { return !m_error.has_value(); }
		explicit operator bool() const { return has_value(); }
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
