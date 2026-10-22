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
 */
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
		std::string m_transfer_id;
		std::string m_type_name;
		std::string m_agent_id;
		std::string m_agent_name;
		MessageBinaryFormat m_state_encoding = MessageBinaryFormat::MessagePack;
		std::vector<std::uint8_t> m_state;
		std::vector<Envelope> m_queue;
		bool m_uses_observables = false;
		std::vector<std::uint8_t> m_observables;
	};

	/**
	 * Payload of AgentAccept, AgentReject, AgentCommit and AgentAbort.
	 **/
	struct AgentControl {
		std::string m_transfer_id;
		std::string m_agent_id;
		bool m_accepted = false;
		std::string m_reason;
	};

	/**
	 * Payload of a Presence: an environment (or a node) announces itself, with its agents.
	 **/
	struct Presence {
		std::string m_node_id;
		std::string m_environment_id;
		std::string m_environment_name;
		std::string m_namespace;
		bool m_online = true;
		std::optional<std::uint64_t> m_turn;
		std::uint64_t m_timestamp_ms = 0;
		std::vector<std::pair<std::string, std::string>> m_agents;  ///< (id, name)
	};

	/**
	 * Payload of a Directory event.
	 **/
	struct DirectoryEvent {
		DirectoryEventKind m_event = DirectoryEventKind::AgentAdded;
		std::string m_agent_id;
		std::string m_agent_name;
		std::string m_environment_id;
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
		std::uint8_t m_version_major = 1;
		std::uint8_t m_version_minor = 0;
		EnvelopeKind m_kind = EnvelopeKind::Message;
		std::string m_sender_environment;
		std::string m_origin_environment;  ///< empty unless forwarded by another environment
		std::uint32_t m_epoch = 0;
		StreamKind m_stream_kind = StreamKind::In;
		std::string m_stream_target;       ///< destination environment of the stream; empty for All
		std::uint64_t m_sequence = 0;
		std::string m_conversation_id;
		std::string m_sender_agent;
		std::string m_sender_name;         ///< name of the sender agent (version 1.1); empty when unknown
		TargetKind m_target_kind = TargetKind::Id;
		std::string m_target_id;
		std::string m_target_name;
		bool m_first_only = false;
		Scope m_scope = Scope::Local;
		MessageBinaryFormat m_content_encoding = MessageBinaryFormat::RAW;
		std::vector<std::uint8_t> m_content;
		std::uint32_t m_priority = 0;      ///< reserved
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
