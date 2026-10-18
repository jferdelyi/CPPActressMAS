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

#include <memory>
#include <string>
#include <variant>
#include <vector>

#include <nlohmann/json.hpp>

#include "AgentId.h"

using json = nlohmann::json;

/**
 * CPPActressMAS
 */
namespace cam {

	/**
	 * Encoding of a message content on the wire: RAW for opaque bytes, Text for UTF-8 text, JSON for
	 * JSON text, the others for the binary encodings of a JSON value. A local message is never encoded.
	 */
	enum class MessageBinaryFormat {
		RAW, BJData, BSON, CBOR, MessagePack, UBJSON, Text, JSON
	};

	/**
	 * What a message carries.
	 */
	enum class ContentKind {
		None, Text, Json, Bytes
	};

	/**
	 * Content of a message: nothing, UTF-8 text, an immutable JSON value shared between the receivers of
	 * a broadcast, or opaque bytes.
	 **/
	using MessageContent = std::variant<std::monostate, std::string, std::shared_ptr<const json>, std::vector<std::uint8_t>>;

	/**
	 * A message that the agents use to communicate. In an agent-based system, the
	 * communication between the agents is exclusively performed by exchanging
	 * messages. The content stays typed as long as the message is local; the
	 * transport encodes it once when it crosses the network.
	 **/
	class Environment;

	class Message final {
		friend Environment;

	protected:
		/**
		 * Sender.
		 **/
		AgentId m_sender;

		/**
		 * Environment the sender belongs to when the message came through the transport; empty for a
		 * local message.
		 **/
		std::string m_sender_environment;

		/**
		 * Receiver.
		 **/
		AgentId m_receiver;

		/**
		 * Content.
		 **/
		MessageContent m_content;

	public:
		/**
		 * Key of the constructor kept for the environment: only the environment can make one.
		 **/
		class Key final {
			friend Environment;

			Key() = default;
		};

		/**
		 * Message built by the environment: identifiers copied once, content moved in (P8).
		 * @param p_key Key of the environment.
		 * @param p_sender Sender.
		 * @param p_receiver Receiver.
		 * @param p_content Content.
		 * @param p_sender_environment Environment of the sender when the message came through the transport, else empty.
		 **/
		Message(Key p_key, const AgentId& p_sender, const AgentId& p_receiver, MessageContent&& p_content,
				const std::string& p_sender_environment);

		/**
		 * Message carrying a JSON value.
		 * @param p_sender Sender.
		 * @param p_receiver Receiver.
		 * @param p_message JSON value.
		 **/
		Message(AgentId p_sender, AgentId p_receiver, const json& p_message);

		/**
		 * Message carrying a JSON value already shared.
		 * @param p_sender Sender.
		 * @param p_receiver Receiver.
		 * @param p_message Shared JSON value.
		 **/
		Message(AgentId p_sender, AgentId p_receiver, std::shared_ptr<const json> p_message);

		/**
		 * Message carrying text.
		 * @param p_sender Sender.
		 * @param p_receiver Receiver.
		 * @param p_text UTF-8 text.
		 **/
		Message(AgentId p_sender, AgentId p_receiver, std::string p_text);

		/**
		 * Message carrying opaque bytes.
		 * @param p_sender Sender.
		 * @param p_receiver Receiver.
		 * @param p_message Bytes (may be null when the length is 0).
		 * @param p_length Number of bytes.
		 **/
		Message(AgentId p_sender, AgentId p_receiver, const uint8_t* p_message, const size_t& p_length);

		/**
		 * Message carrying any content.
		 * @param p_sender Sender.
		 * @param p_receiver Receiver.
		 * @param p_content Content.
		 **/
		Message(AgentId p_sender, AgentId p_receiver, MessageContent p_content);

		Message();

		/**
		 * Nothing to delete.
		 **/
		/*virtual*/ ~Message() = default;

		/**
		 * Get sender.
		 * @return Sender.
		 **/
		[[nodiscard]] const AgentId& get_sender() const { return m_sender; }

		/**
		 * Get receiver.
		 * @return Receiver.
		 **/
		[[nodiscard]] const AgentId& get_receiver() const { return m_receiver; }

		/**
		 * Get the environment of the sender: empty for a local message, the id of the origin environment
		 * for a message received through the transport.
		 * @return Environment id or empty
		 **/
		[[nodiscard]] const std::string& get_sender_environment() const { return m_sender_environment; }

		/**
		 * Kind of content carried.
		 * @return The kind
		 **/
		[[nodiscard]] ContentKind kind() const;

		/**
		 * The content as it was given.
		 * @return The content
		 **/
		[[nodiscard]] const MessageContent& get_content() const { return m_content; }

		/**
		 * Text content; empty when the message carries something else (see kind()).
		 * @return The text
		 **/
		[[nodiscard]] const std::string& as_text() const;

		/**
		 * JSON content; null when the message carries something else (see kind()).
		 * @return The JSON value
		 **/
		[[nodiscard]] const json& as_json() const;

		/**
		 * Bytes content; empty when the message carries something else (see kind()).
		 * @return The bytes
		 **/
		[[nodiscard]] const std::vector<std::uint8_t>& as_bytes() const;

		/**
		 * Get the content as a JSON value: the value itself, a JSON string for text and for bytes, null
		 * for an empty message.
		 * @return message JSON
		 **/
		[[nodiscard]] json content() const;

		/**
		 * Format message to string: the text itself, the JSON dump, the bytes as characters.
		 * @return string message
		 **/
		[[nodiscard]] std::string to_string() const;

		/**
		 * Format message.
		 * @return string formated message
		 **/
		[[nodiscard]] /*virtual*/ std::string format() const;

		// Delete copy constructor
		Message(const Message&) = delete;

		Message& operator=(Message&) = delete;
	};

	// Message pointer
	using MessagePointer = std::shared_ptr<const Message>;
} // namespace cam
