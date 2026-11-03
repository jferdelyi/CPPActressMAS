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

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "Types.h"

/**
 * CPPActressMAS transport layer: the envelope carried between environments. The structure is the
 * contract; an EnvelopeCodec gives it a form on the wire.
 **/
namespace cam::transport {

	/**
	 * Kind of envelope.
	 **/
	enum class EnvelopeKind : std::uint8_t {
		Message = 0,
		AgentOffer = 1,
		AgentAccept = 2,
		AgentReject = 3,
		AgentCommit = 4,
		AgentAbort = 5,
		Presence = 6,
		Directory = 7,
		Query = 8,
		Reply = 9
	};

	/**
	 * Stream of an envelope: the destination key it was published on. Ordering is guaranteed per
	 * (sender environment, epoch, stream).
	 **/
	enum class StreamKind : std::uint8_t {
		In = 0,
		Agent = 1,
		Query = 2,
		Presence = 3,
		All = 4
	};

	/**
	 * Kind of target of a message.
	 **/
	enum class TargetKind : std::uint8_t {
		Id = 0,
		Name = 1,
		Fragment = 2,
		Broadcast = 3,
		Environment = 4
	};

	/**
	 * Directory events published on the presence key of the hosting environment.
	 **/
	enum class DirectoryEventKind : std::uint8_t {
		AgentAdded = 0,
		AgentRemoved = 1,
		AgentMoved = 2
	};

	struct Envelope;

	/**
	 * Payload of an AgentOffer: the agent on the move. The queue holds the envelopes not yet consumed by
	 * the agent, never the message being handled.
	 **/
	struct AgentTransfer {
		/**
		 * Id of this transfer, repeated by its controls.
		 **/
		std::string m_transfer_id;

		/**
		 * Wire name of the type of the agent (AgentTypes), how the destination rebuilds it.
		 **/
		std::string m_type_name;

		/**
		 * Id of the agent.
		 **/
		std::string m_agent_id;

		/**
		 * Name of the agent.
		 **/
		std::string m_agent_name;

		/**
		 * Encoding of m_state.
		 **/
		ContentEncoding m_state_encoding = ContentEncoding::MessagePack;

		/**
		 * State of the agent (Agent::save_state()), encoded.
		 **/
		std::vector<std::uint8_t> m_state;

		/**
		 * Messages the agent had not consumed, in order.
		 **/
		std::vector<Envelope> m_queue;

		/**
		 * True if the agent uses observables.
		 **/
		bool m_uses_observables = false;

		/**
		 * Observables of the agent, in MessagePack.
		 **/
		std::vector<std::uint8_t> m_observables;
	};

	/**
	 * Payload of AgentAccept, AgentReject, AgentCommit and AgentAbort.
	 **/
	struct AgentControl {
		/**
		 * Id of the transfer this control is about.
		 **/
		std::string m_transfer_id;

		/**
		 * Id of the agent on the move.
		 **/
		std::string m_agent_id;

		/**
		 * True if the destination accepts the agent.
		 **/
		bool m_accepted = false;

		/**
		 * Why the agent is refused or the transfer abandoned, if it is.
		 **/
		std::string m_reason;
	};

	/**
	 * Payload of a Presence: an environment (or a node) announces itself, with its agents.
	 **/
	struct Presence {
		/**
		 * Id of the node (transport instance) of the environment.
		 **/
		std::string m_node_id;

		/**
		 * Id of the environment; for the presence of a node, its node id.
		 **/
		std::string m_environment_id;

		/**
		 * Name of the environment.
		 **/
		std::string m_environment_name;

		/**
		 * Namespace of the environment.
		 **/
		std::string m_namespace;

		/**
		 * False when the environment leaves.
		 **/
		bool m_online = true;

		/**
		 * Turn of the environment, if it gives it.
		 **/
		std::optional<std::uint64_t> m_turn;

		/**
		 * When the presence was made, in milliseconds since the epoch.
		 **/
		std::uint64_t m_timestamp_ms = 0;

		/**
		 * Agents of the environment, as (id, name).
		 **/
		std::vector<std::pair<std::string, std::string>> m_agents;
	};

	/**
	 * Payload of a Directory event.
	 **/
	struct DirectoryEvent {
		/**
		 * What happened to the agent.
		 **/
		DirectoryEventKind m_event = DirectoryEventKind::AgentAdded;

		/**
		 * Id of the agent.
		 **/
		std::string m_agent_id;

		/**
		 * Name of the agent.
		 **/
		std::string m_agent_name;

		/**
		 * Environment that hosts the agent now.
		 **/
		std::string m_environment_id;

		/**
		 * Environment that hosted it before, for a move.
		 **/
		std::string m_previous_environment_id;
	};

	/**
	 * Minor version of the envelopes this library creates: 1 adds the name of the sender of a message.
	 **/
	inline constexpr std::uint8_t ENVELOPE_VERSION_MINOR = 1;

	/**
	 * The envelope. Identifiers are the strings used by the core (canonical UUIDs for environments
	 * and agents created by the framework).
	 **/
	struct Envelope {
		/**
		 * Major version of the envelope.
		 **/
		std::uint8_t m_version_major = 1;

		/**
		 * Minor version: 1 since the name of the sender travels.
		 **/
		std::uint8_t m_version_minor = 0;

		/**
		 * Kind of envelope.
		 **/
		EnvelopeKind m_kind = EnvelopeKind::Message;

		/**
		 * Environment that published the envelope.
		 **/
		std::string m_sender_environment;

		/**
		 * Environment whose agent emitted it, when another one forwarded it; empty otherwise.
		 **/
		std::string m_origin_environment;

		/**
		 * Epoch of the endpoint that published it.
		 **/
		std::uint32_t m_epoch = 0;

		/**
		 * Stream it was published on.
		 **/
		StreamKind m_stream_kind = StreamKind::In;

		/**
		 * Destination environment of the stream; empty for All.
		 **/
		std::string m_stream_target;

		/**
		 * Sequence in its stream, from 1.
		 **/
		std::uint64_t m_sequence = 0;

		/**
		 * Conversation id of the message (the ActressMAS transport only).
		 **/
		std::string m_conversation_id;

		/**
		 * Id of the agent that sent the message.
		 **/
		std::string m_sender_agent;

		/**
		 * Name of the sender agent (version 1.1); empty when unknown.
		 **/
		std::string m_sender_name;

		/**
		 * Kind of target of the message.
		 **/
		TargetKind m_target_kind = TargetKind::Id;

		/**
		 * Id of the target agent (TargetKind::Id), or of the target environment.
		 **/
		std::string m_target_id;

		/**
		 * Name or fragment of name of the target agents.
		 **/
		std::string m_target_name;

		/**
		 * True if a message by name goes to the first matching agent only.
		 **/
		bool m_first_only = false;

		/**
		 * Scope of the send.
		 **/
		Scope m_scope = Scope::Local;

		/**
		 * Encoding of m_content.
		 **/
		ContentEncoding m_content_encoding = ContentEncoding::Raw;

		/**
		 * Content of the message, encoded.
		 **/
		std::vector<std::uint8_t> m_content;

		/**
		 * Reserved.
		 **/
		std::uint32_t m_priority = 0;

		/**
		 * Payload of the kinds that have one (migration, presence, directory).
		 **/
		std::variant<std::monostate, AgentTransfer, AgentControl, Presence, DirectoryEvent> m_payload;

		/**
		 * The environment whose agent emitted the message: the origin when forwarded, the sender otherwise.
		 * @return The environment id
		 **/
		[[nodiscard]] const std::string& effective_sender_environment() const {
			return m_origin_environment.empty() ? m_sender_environment : m_origin_environment;
		}
	};

	/**
	 * Address kind matching a stream kind.
	 * @param p_kind The stream kind
	 * @return The address kind
	 **/
	inline AddressKind to_address_kind(const StreamKind p_kind) {
		switch (p_kind) {
			case StreamKind::In: return AddressKind::EnvIn;
			case StreamKind::Agent: return AddressKind::EnvAgent;
			case StreamKind::Query: return AddressKind::EnvQuery;
			case StreamKind::Presence: return AddressKind::EnvPresence;
			case StreamKind::All:
			default: return AddressKind::AllIn;
		}
	}

	/**
	 * Stream kind matching an address kind (NodePresence maps to Presence).
	 * @param p_kind The address kind
	 * @return The stream kind
	 **/
	inline StreamKind to_stream_kind(const AddressKind p_kind) {
		switch (p_kind) {
			case AddressKind::EnvIn: return StreamKind::In;
			case AddressKind::EnvAgent: return StreamKind::Agent;
			case AddressKind::EnvQuery: return StreamKind::Query;
			case AddressKind::EnvPresence:
			case AddressKind::NodePresence: return StreamKind::Presence;
			case AddressKind::AllIn:
			default: return StreamKind::All;
		}
	}

} // namespace cam::transport
