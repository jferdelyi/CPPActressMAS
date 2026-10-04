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
#include <chrono>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Transport.h"

/**
 * CPPActressMAS transport layer.
 */
namespace cam::transport {

	/**
	 * What a transport deposits in an endpoint: an address and either decoded envelope (in-memory
	 * loopback) or bytes to decode at the turn boundary.
	 **/
	struct Incoming {
		Address m_address;
		std::vector<std::uint8_t> m_bytes;
		std::optional<Envelope> m_envelope;
	};

	/**
	 * The side of an environment on a transport: bounded inbox filled by transport threads and drained
	 * at the turn boundary by the simulation thread; bounded outbox filled by agents and flushed at the
	 * end of the turn; sequences per stream; deduplication and bounded reordering per (sender, epoch,
	 * stream); directory of remote agents; presence table; counters. No network header here.
	 **/
	class Endpoint final {
	private:
		/**
		 * Reception state of one stream.
		 **/
		struct FlowState {
			bool m_started = false;
			std::uint64_t m_next_expected = 0;
			std::deque<std::uint64_t> m_recent;
			std::unordered_set<std::uint64_t> m_recent_set;
			std::map<std::uint64_t, Envelope> m_pending;
			std::uint64_t m_gap_since_turn = 0;
			bool m_has_gap = false;
		};

		struct Outgoing {
			Address m_address;
			Envelope m_envelope;
		};

		std::string m_environment_id;
		EndpointConfig m_config;
		std::uint32_t m_epoch;
		std::shared_ptr<Transport> m_transport;

		mutable std::mutex m_inbox_mutex;
		std::deque<Incoming> m_inbox;

		mutable std::mutex m_outbox_mutex;
		std::deque<Outgoing> m_outbox;
		std::unordered_map<std::string, std::uint64_t> m_sequences;  ///< per destination key

		std::unordered_map<std::string, FlowState> m_flows;           ///< per (sender, epoch, key)
		std::unordered_map<std::string, std::string> m_directory;      ///< agent id -> environment id
		std::unordered_map<std::string, std::string> m_moved_from;     ///< agent id -> previous environment (grace)
		std::map<std::string, EnvironmentInfo> m_environments;         ///< environment id -> info
		std::uint64_t m_turn = 0;

		Statistics m_statistics;                                  ///< simulation thread only
		std::atomic<std::uint64_t> m_inbox_overflow{0};           ///< transport threads
		std::atomic<std::uint64_t> m_received{0};                 ///< transport threads
		std::atomic<std::uint64_t> m_outbox_overflow{0};          ///< agent threads
		std::atomic<std::uint64_t> m_unknown_destination{0};      ///< agent threads

		void deliver_ready(FlowState& p_flow, std::vector<Envelope>& p_out);
		void release_gap(FlowState& p_flow, std::vector<Envelope>& p_out);
		void remember(FlowState& p_flow, std::uint64_t p_sequence);
		static std::string flow_key(const Envelope& p_envelope);

	public:
		/**
		 * Create the endpoint of an environment.
		 * @param p_environment_id The environment id
		 * @param p_config The configuration
		 **/
		Endpoint(std::string p_environment_id, EndpointConfig p_config);

		Endpoint(const Endpoint&) = delete;
		Endpoint& operator=(const Endpoint&) = delete;

		// ---------------------------------------------------------------- identity and configuration

		[[nodiscard]] const std::string& environment_id() const { return m_environment_id; }
		[[nodiscard]] const std::string& name() const { return m_config.m_name; }
		[[nodiscard]] const EndpointConfig& config() const { return m_config; }
		[[nodiscard]] std::uint32_t epoch() const { return m_epoch; }
		[[nodiscard]] bool is_attached() const { return m_transport != nullptr; }
		[[nodiscard]] const std::shared_ptr<Transport>& transport() const { return m_transport; }
		[[nodiscard]] std::uint64_t turn() const { return m_turn; }

		/**
		 * Namespace of the transport, or "default" when detached.
		 * @return The namespace
		 **/
		[[nodiscard]] const std::string& ns() const;

		/**
		 * Build an address in the namespace of the transport.
		 * @param p_kind The kind
		 * @param p_target The target (environment or node id, empty for AllIn)
		 * @return The address
		 **/
		[[nodiscard]] Address address(AddressKind p_kind, const std::string& p_target = {}) const;

		/**
		 * New envelope stamped with this endpoint as sender.
		 * @param p_kind The kind
		 * @return The envelope
		 **/
		[[nodiscard]] Envelope make_envelope(EnvelopeKind p_kind) const;

		// ---------------------------------------------------------------- attachment

		/**
		 * Attach to a transport (the transport is told).
		 * @param p_transport The transport
		 * @return Success or an error
		 **/
		Result<void> attach(std::shared_ptr<Transport> p_transport);

		/**
		 * Detach from the transport (the transport is told).
		 * @return Success or an error
		 **/
		Result<void> detach();

		// ---------------------------------------------------------------- sending (any thread)

		/**
		 * Queue an envelope for the end of the turn: the stream fields and the sequence are assigned
		 * here, in order. Thread-safe.
		 * @param p_address The destination
		 * @param p_envelope The envelope (moved)
		 * @return Success, OutboxFull or NotAttached
		 **/
		Result<void> enqueue(const Address& p_address, Envelope&& p_envelope);

		/**
		 * Publish the queued envelopes (simulation thread, end of turn), bounded by the configured
		 * maximum count and time. Failures are counted and reported through the returned list.
		 * @return The errors met, if any
		 **/
		std::vector<Error> flush_outbox();

		// ---------------------------------------------------------------- receiving

		/**
		 * Deposit a decoded envelope (transport threads). Never blocks; overflow is counted.
		 * @param p_address The key it arrived on
		 * @param p_envelope The envelope
		 * @return False on overflow
		 **/
		bool deliver(const Address& p_address, Envelope&& p_envelope);

		/**
		 * Deposit encoded bytes (transport threads). Never blocks; overflow and oversize are counted.
		 * @param p_address The key it arrived on
		 * @param p_bytes The bytes
		 * @return False on overflow or oversize
		 **/
		bool deliver(const Address& p_address, std::vector<std::uint8_t>&& p_bytes);

		/**
		 * Take everything received since the last call, decode and validate it, drop echoes and
		 * duplicates, reorder per stream within the configured bounds, and return what is ready, agent
		 * control first, then presence and directory, then messages (simulation thread, turn boundary).
		 * Advances the turn counter used by the reordering delay.
		 * @return The envelopes to apply
		 **/
		std::vector<Envelope> drain();

		// ---------------------------------------------------------------- directory and presence (simulation thread)

		[[nodiscard]] std::optional<std::string> locate_agent(const std::string& p_agent_id) const;
		void set_agent_location(const std::string& p_agent_id, const std::string& p_environment_id);
		void remove_agent(const std::string& p_agent_id);
		void purge_environment(const std::string& p_environment_id);
		void update_environment(const Presence& p_presence);
		[[nodiscard]] std::vector<EnvironmentInfo> environments() const;
		[[nodiscard]] bool knows_environment(const std::string& p_environment_id) const;
		[[nodiscard]] std::optional<std::string> previous_host(const std::string& p_agent_id) const;
		void set_previous_host(const std::string& p_agent_id, const std::string& p_environment_id);

		// ---------------------------------------------------------------- counters

		[[nodiscard]] Statistics statistics() const;
		void count_rejected() { ++m_statistics.m_rejected; }
		void count_unknown_destination() { ++m_unknown_destination; }
		void count_forwarded() { ++m_statistics.m_forwarded; }
		void count_stale_control() { ++m_statistics.m_stale_control; }
		void count_migration_completed() { ++m_statistics.m_migrations_completed; }
		void count_migration_failed() { ++m_statistics.m_migrations_failed; }
		void count_lost(const std::uint64_t p_count) { m_statistics.m_lost += p_count; }
	};

} // namespace cam::transport
