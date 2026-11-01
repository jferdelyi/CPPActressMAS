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
 **/
namespace cam::transport {

	/**
	 * What a transport deposits in an endpoint: an address and either decoded envelope (in-memory
	 * loopback) or bytes to decode at the turn boundary.
	 **/
	struct Incoming {
		/**
		 * Key it arrived on.
		 **/
		Address m_address;

		/**
		 * Encoded envelope, decoded at the turn boundary (empty when m_envelope is set).
		 **/
		std::vector<std::uint8_t> m_bytes;

		/**
		 * Envelope already decoded (in-memory delivery).
		 **/
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
			/**
			 * True once the first sequence of the flow is known.
			 **/
			bool m_started = false;

			/**
			 * Next sequence to deliver.
			 **/
			std::uint64_t m_next_expected = 0;

			/**
			 * Last sequences delivered, oldest first (m_dedup_window of them).
			 **/
			std::deque<std::uint64_t> m_recent;

			/**
			 * The same sequences, for the search of a duplicate.
			 **/
			std::unordered_set<std::uint64_t> m_recent_set;

			/**
			 * Envelopes held behind a missing sequence, by sequence.
			 **/
			std::map<std::uint64_t, Envelope> m_pending;

			/**
			 * Turn when the current gap appeared (0: none).
			 **/
			std::uint64_t m_gap_since_turn = 0;

			/**
			 * True while a sequence is missing.
			 **/
			bool m_has_gap = false;
		};

		/**
		 * Envelope waiting in the outbox for its publication.
		 **/
		struct Outgoing {
			/**
			 * Destination.
			 **/
			Address m_address;

			/**
			 * The envelope, stamped with its stream and sequence.
			 **/
			Envelope m_envelope;
		};

		/**
		 * Id of the environment of this endpoint.
		 **/
		std::string m_environment_id;

		/**
		 * Configuration.
		 **/
		EndpointConfig m_config;

		/**
		 * Random epoch of this endpoint: the sequences of a restarted environment do not mix with the old ones.
		 **/
		std::uint32_t m_epoch;

		/**
		 * Transport it is attached to, null when detached.
		 **/
		std::shared_ptr<Transport> m_transport;

		/**
		 * Guards m_inbox and m_events.
		 **/
		mutable std::mutex m_inbox_mutex;

		/**
		 * Arrivals since the last drain.
		 **/
		std::deque<Incoming> m_inbox;

		/**
		 * Connection events since the last take_events().
		 **/
		std::vector<TransportEvent> m_events;

		/**
		 * Guards m_outbox and m_sequences.
		 **/
		mutable std::mutex m_outbox_mutex;

		/**
		 * Envelopes waiting for the next flush.
		 **/
		std::deque<Outgoing> m_outbox;

		/**
		 * Last sequence sent, by destination key.
		 **/
		std::unordered_map<std::string, std::uint64_t> m_sequences;

		/**
		 * Reception state, by (sender, epoch, stream).
		 **/
		std::unordered_map<std::string, FlowState> m_flows;

		/**
		 * Environment of each known remote agent, by agent id.
		 **/
		std::unordered_map<std::string, std::string> m_directory;

		/**
		 * Previous environment of an agent that moved, by agent id (forwarding grace).
		 **/
		std::unordered_map<std::string, std::string> m_moved_from;

		/**
		 * What is known of the other environments, by environment id.
		 **/
		std::map<std::string, EnvironmentInfo> m_environments;

		/**
		 * Drains so far: the turn counter of the reordering delay.
		 **/
		std::uint64_t m_turn = 0;

		/**
		 * Counters of the simulation thread.
		 **/
		Statistics m_statistics;

		/**
		 * Arrivals dropped on a full inbox (transport threads).
		 **/
		std::atomic<std::uint64_t> m_inbox_overflow{0};

		/**
		 * Arrivals accepted (transport threads).
		 **/
		std::atomic<std::uint64_t> m_received{0};

		/**
		 * Sends refused on a full outbox (agent threads).
		 **/
		std::atomic<std::uint64_t> m_outbox_overflow{0};

		/**
		 * Messages for an agent found nowhere (agent threads).
		 **/
		std::atomic<std::uint64_t> m_unknown_destination{0};

		/**
		 * Deliver the held envelopes that follow the last one delivered, and note whether a gap remains.
		 * @param p_flow The flow
		 * @param p_out Where the ready envelopes go
		 **/
		void deliver_ready(FlowState& p_flow, std::vector<Envelope>& p_out);

		/**
		 * Give up the missing sequences of a flow: count them lost, deliver what waited behind them.
		 * @param p_flow The flow
		 * @param p_out Where the ready envelopes go
		 **/
		void release_gap(FlowState& p_flow, std::vector<Envelope>& p_out);

		/**
		 * Remember a delivered sequence, within the deduplication window.
		 * @param p_flow The flow
		 * @param p_sequence The sequence
		 **/
		void remember(FlowState& p_flow, std::uint64_t p_sequence);

		/**
		 * Key of the flow of an envelope: sender, epoch, stream kind and target.
		 * @param p_envelope The envelope
		 * @return The key
		 **/
		static std::string flow_key(const Envelope& p_envelope);

	public:
		/**
		 * Create the endpoint of an environment.
		 * @param p_environment_id The environment id
		 * @param p_config The configuration
		 **/
		Endpoint(std::string p_environment_id, EndpointConfig p_config);

		// Delete copy constructor
		Endpoint(const Endpoint&) = delete;

		Endpoint& operator=(const Endpoint&) = delete;

		// ---------------------------------------------------------------- identity and configuration

		/**
		 * Id of the environment.
		 * @return The id
		 **/
		[[nodiscard]] const std::string& environment_id() const { return m_environment_id; }

		/**
		 * Name of the environment, announced in its presence.
		 * @return The name
		 **/
		[[nodiscard]] const std::string& name() const { return m_config.m_name; }

		/**
		 * Configuration.
		 * @return The configuration
		 **/
		[[nodiscard]] const EndpointConfig& config() const { return m_config; }

		/**
		 * Epoch of this endpoint.
		 * @return The epoch
		 **/
		[[nodiscard]] std::uint32_t epoch() const { return m_epoch; }

		/**
		 * True if attached to a transport.
		 * @return True if attached
		 **/
		[[nodiscard]] bool is_attached() const { return m_transport != nullptr; }

		/**
		 * Transport it is attached to.
		 * @return The transport, null when detached
		 **/
		[[nodiscard]] const std::shared_ptr<Transport>& transport() const { return m_transport; }

		/**
		 * Drains so far.
		 * @return The count
		 **/
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
		 * Deposit encoded bytes (transport threads). Never blocks; overflow is counted. The bytes are decoded at the
		 * turn boundary, where an oversized or invalid envelope is rejected.
		 * @param p_address The key it arrived on
		 * @param p_bytes The bytes
		 * @return False on overflow
		 **/
		bool deliver(const Address& p_address, std::vector<std::uint8_t>&& p_bytes);

		/**
		 * Deposit a connection event (transport threads).
		 * @param p_event The event
		 **/
		void push_event(TransportEvent p_event);

		/**
		 * Take the connection events received since the last call (simulation thread).
		 * @return The events, oldest first
		 **/
		std::vector<TransportEvent> take_events();

		/**
		 * Take everything received since the last call, decode and validate it, drop echoes and
		 * duplicates, reorder per stream within the configured bounds, and return what is ready, agent
		 * control first, then presence and directory, then messages (simulation thread, turn boundary).
		 * Advances the turn counter used by the reordering delay.
		 * @return The envelopes to apply
		 **/
		std::vector<Envelope> drain();

		// ---------------------------------------------------------------- directory and presence (simulation thread)

		/**
		 * Environment of a remote agent.
		 * @param p_agent_id The agent id
		 * @return The environment id, or nothing if unknown
		 **/
		[[nodiscard]] std::optional<std::string> locate_agent(const std::string& p_agent_id) const;

		/**
		 * Record the environment of a remote agent.
		 * @param p_agent_id The agent id
		 * @param p_environment_id The environment id
		 **/
		void set_agent_location(const std::string& p_agent_id, const std::string& p_environment_id);

		/**
		 * Forget a remote agent.
		 * @param p_agent_id The agent id
		 **/
		void remove_agent(const std::string& p_agent_id);

		/**
		 * An environment went offline: forget its agents and mark it offline.
		 * @param p_environment_id The environment id
		 **/
		void purge_environment(const std::string& p_environment_id);

		/**
		 * A node died: every environment it hosted goes offline and loses its agents.
		 * @param p_node_id The node
		 * @return The environments that were online on that node
		 **/
		std::vector<EnvironmentInfo> purge_node(const std::string& p_node_id);

		/**
		 * Apply a presence: an online one records the environment and its agents, an offline one purges it.
		 * @param p_presence The presence
		 **/
		void update_environment(const Presence& p_presence);

		/**
		 * The other environments known, online or not.
		 * @return Their snapshots, by id
		 **/
		[[nodiscard]] std::vector<EnvironmentInfo> environments() const;

		/**
		 * True if the environment is known and online.
		 * @param p_environment_id The environment id
		 * @return True if online
		 **/
		[[nodiscard]] bool knows_environment(const std::string& p_environment_id) const;

		/**
		 * Environment an agent left, during its forwarding grace.
		 * @param p_agent_id The agent id
		 * @return The environment id, or nothing
		 **/
		[[nodiscard]] std::optional<std::string> previous_host(const std::string& p_agent_id) const;

		/**
		 * Record the environment an agent left.
		 * @param p_agent_id The agent id
		 * @param p_environment_id The environment it left
		 **/
		void set_previous_host(const std::string& p_agent_id, const std::string& p_environment_id);

		// ---------------------------------------------------------------- counters

		/**
		 * Snapshot of the counters.
		 * @return The counters
		 **/
		[[nodiscard]] Statistics statistics() const;

		/**
		 * Count an envelope refused by the environment.
		 **/
		void count_rejected() { ++m_statistics.m_rejected; }

		/**
		 * Count a message for an agent found nowhere (any thread).
		 **/
		void count_unknown_destination() { ++m_unknown_destination; }

		/**
		 * Count a message forwarded to the new environment of an agent.
		 **/
		void count_forwarded() { ++m_statistics.m_forwarded; }

		/**
		 * Count a migration control about an unknown or finished transfer.
		 **/
		void count_stale_control() { ++m_statistics.m_stale_control; }

		/**
		 * Count a confirmed migration.
		 **/
		void count_migration_completed() { ++m_statistics.m_migrations_completed; }

		/**
		 * Count a refused, timed out or abandoned migration.
		 **/
		void count_migration_failed() { ++m_statistics.m_migrations_failed; }

		/**
		 * Count lost envelopes.
		 * @param p_count Their number
		 **/
		void count_lost(const std::uint64_t p_count) { m_statistics.m_lost += p_count; }
	};

} // namespace cam::transport
