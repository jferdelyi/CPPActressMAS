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
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "../Types.h"

/**
 * CPPActressMAS transport layer.
 **/
namespace cam::transport {

	/**
	 * A ContainerMessage of the ActressMAS v3.1 ContainerServer (C#, ContainerServer/ContainerMessage.cs): what the
	 * containers and the server exchange. A null string of the C# side reads as an empty one.
	 **/
	struct ContainerMessage {
		/**
		 * Container that sends, or "Server".
		 **/
		std::string m_sender;

		/**
		 * Container that receives, or "Server".
		 **/
		std::string m_receiver;

		/**
		 * The command (see ActressMasCodec).
		 **/
		std::string m_info;

		/**
		 * Argument of the command: container list, serialized Message or AgentState, or empty.
		 **/
		std::string m_content;
	};

	/**
	 * A Message of ActressMAS v3.1 (Agents/Message.cs) as it travels in the content of a "Send Remote Message":
	 * sender and receiver are "agent@container". A boxed ContentObj of the C# side (a string, a number or a
	 * boolean) reads as text when Content is null.
	 **/
	struct RemoteMessage {
		/**
		 * The sending agent, "agent@container".
		 **/
		std::string m_sender;

		/**
		 * The receiving agent, "agent@container".
		 **/
		std::string m_receiver;

		/**
		 * The text of the message.
		 **/
		std::string m_content;

		/**
		 * Conversation identifier, empty by default as with Agent.Send in C#.
		 **/
		std::string m_conversation_id;
	};

	/**
	 * Wire format of the ActressMAS v3.1 ContainerServer, captured from the C# code:
	 * a frame is a 38-byte header (01 FF, total length as a big-endian int32, two GUIDs never read, written as
	 * zeros here) followed by UTF-16LE(Base64(MS-NRBF(ContainerMessage))). Only the records that BinaryFormatter
	 * writes for a ContainerMessage and a Message are read and written, and the library must be the ActressMAS
	 * assembly of version 3.1.0.0, as BinaryFormatter itself requires. No dependency. This is not an EnvelopeCodec:
	 * a frame names containers, which an envelope does not know; the ActressMAS transport maps one to the other.
	 **/
	class ActressMasCodec final {
	public:
		/**
		 * Size of the frame header.
		 **/
		static constexpr std::size_t HEADER_SIZE = 38;

		/**
		 * Assembly written in every message; any other one is refused.
		 **/
		static constexpr std::string_view ASSEMBLY = "ActressMas, Version=3.1.0.0, Culture=neutral, PublicKeyToken=null";

		/**
		 * Command of a container that registers under its name (Info of a ContainerMessage).
		 **/
		static constexpr std::string_view REQUEST_REGISTER = "Request Register";

		/**
		 * Command of the server that gives the list of the containers.
		 **/
		static constexpr std::string_view INFORM_CONTAINERS = "Inform Containers";

		/**
		 * Command of the server that refuses the name of a container.
		 **/
		static constexpr std::string_view INFORM_INVALID_NAME = "Inform Invalid Name";

		/**
		 * Command that carries a message to an agent of another container.
		 **/
		static constexpr std::string_view SEND_REMOTE_MESSAGE = "Send Remote Message";

		/**
		 * Command that carries an agent to another container.
		 **/
		static constexpr std::string_view REQUEST_MOVE_AGENT = "Request Move Agent";

		/**
		 * Name of the server in Sender and Receiver.
		 **/
		static constexpr std::string_view SERVER = "Server";

		/**
		 * Encode a frame, header included.
		 * @param p_message The container message
		 * @return The bytes to write on the socket
		 **/
		[[nodiscard]] static std::vector<std::uint8_t> encode_frame(const ContainerMessage& p_message);

		/**
		 * Read the total size of a frame from its header.
		 * @param p_header The first HEADER_SIZE bytes of the frame (or more)
		 * @param p_max_bytes Largest frame accepted
		 * @return The total size, header included, or a validation error (marker, size)
		 **/
		[[nodiscard]] static Result<std::size_t> read_frame_size(std::span<const std::uint8_t> p_header, std::size_t p_max_bytes);

		/**
		 * Decode a whole frame.
		 * @param p_frame The frame, header included
		 * @param p_max_bytes Largest frame accepted
		 * @return The container message, or a validation error
		 **/
		[[nodiscard]] static Result<ContainerMessage> decode_frame(std::span<const std::uint8_t> p_frame, std::size_t p_max_bytes);

		/**
		 * Encode a Message for the content of a "Send Remote Message": Base64(MS-NRBF(Message)).
		 * @param p_message The message
		 * @return The Base64 text
		 **/
		[[nodiscard]] static std::string encode_remote_message(const RemoteMessage& p_message);

		/**
		 * Decode the content of a "Send Remote Message".
		 * @param p_base64 The Base64 text
		 * @return The message, or a validation error
		 **/
		[[nodiscard]] static Result<RemoteMessage> decode_remote_message(std::string_view p_base64);

		/**
		 * Serialize a container message as BinaryFormatter does (MS-NRBF), without Base64 nor frame.
		 * @param p_message The container message
		 * @return The bytes
		 **/
		[[nodiscard]] static std::vector<std::uint8_t> write_nrbf(const ContainerMessage& p_message);

		/**
		 * Serialize a message as BinaryFormatter does (MS-NRBF), without Base64.
		 * @param p_message The message
		 * @return The bytes
		 **/
		[[nodiscard]] static std::vector<std::uint8_t> write_nrbf(const RemoteMessage& p_message);

		/**
		 * Read a container message serialized by BinaryFormatter.
		 * @param p_bytes The bytes
		 * @return The container message, or a validation error
		 **/
		[[nodiscard]] static Result<ContainerMessage> read_container_nrbf(std::span<const std::uint8_t> p_bytes);

		/**
		 * Read a message serialized by BinaryFormatter.
		 * @param p_bytes The bytes
		 * @return The message, or a validation error
		 **/
		[[nodiscard]] static Result<RemoteMessage> read_remote_nrbf(std::span<const std::uint8_t> p_bytes);
	};

} // namespace cam::transport
