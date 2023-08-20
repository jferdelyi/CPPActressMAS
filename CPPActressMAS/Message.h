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

#include <nlohmann/json.hpp>
using json = nlohmann::json;

/**
 * CPPActressMAS
 */
namespace cam {

	/**
	 * Message binary format for JSON
	 */
	enum class MessageBinaryFormat {
		BJData,
		BSON,
		CBOR,
		MessagePack,
		UBJSON
	};

	/**
	 * A message that the agents use to communicate. In an agent-based system, the communication between the agents is exclusively performed by exchanging messages.
	 **/
	class Message {
		
		protected:

			/**
			 * Sender.
			 **/
			std::string m_sender;

			/**
			 * Receiver.
			 **/
			std::string m_receiver;

			/**
			 * Binary format.
			 **/
			MessageBinaryFormat m_binary_format;

			/**
			 * Raw message.
			 **/
			std::vector<std::uint8_t> m_binary_message;

		public:

			/**
			 * Message.
			 * @param p_sender Sender.
			 * @param p_receiver Receiver.
			 * @param p_message Message.
			 **/
			Message(const std::string& p_sender, const std::string& p_receiver, const json& p_message, const MessageBinaryFormat& p_binary_format = MessageBinaryFormat::MessagePack);

			/**
			 * Nothing to delete.
			 **/
			virtual ~Message() = default;

			/**
			 * Get sender.
			 * @return Sender.
			 **/
			const std::string& get_sender() const;

			/**
			 * Get receiver.
			 * @return Receiver.
			 **/
			const std::string& get_receiver() const;

			/**
			 * Get binary message.
			 * @return binary message JSON
			 **/
			const std::vector<std::uint8_t>& get_binary_message() const;

			/**
			 * Get message.
			 * @return message JSON
			 **/
			const json content() const;

			/**
			 * Format message to string.
			 * @return string message from JSON
			 **/
			const std::string to_string() const;

			/**
			 * Format message "SENDER -> RECEIVER: CONTENT".
			 * @return string formated message
			 **/
			const std::string format() const;

			// Delete copy constructor
			Message(const Message&) = delete;
			Message& operator=(Message&) = delete;
	};

	// Message pointer
	using MessagePointer = std::shared_ptr<cam::Message>;
}
