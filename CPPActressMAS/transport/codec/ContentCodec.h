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

#include <span>
#include <vector>

#include "../../Message.h"
#include "../Types.h"

/**
 * CPPActressMAS transport layer: encoding of a message content at the network boundary. A local message
 * keeps its typed content; the bytes of the envelope are produced here once when the message leaves the
 * node, and read back here once when it arrives.
 */
namespace cam::transport {

	/**
	 * Encoding used on the wire for a content: Text for text, RAW for bytes (or the tag asked by the
	 * options), the binary encoding of the options for a JSON value (MessagePack by default).
	 * @param p_content The content
	 * @param p_options The send options
	 * @return The encoding, or a validation error (a JSON value cannot travel RAW)
	 **/
	[[nodiscard]] Result<MessageBinaryFormat> wire_encoding(const MessageContent& p_content, const SendOptions& p_options);

	/**
	 * Encode a content.
	 * @param p_content The content
	 * @param p_encoding The encoding (see wire_encoding)
	 * @return The bytes, or a validation error
	 **/
	[[nodiscard]] Result<std::vector<std::uint8_t>> encode_content(const MessageContent& p_content, MessageBinaryFormat p_encoding);

	/**
	 * Decode a content received from the network.
	 * @param p_bytes The bytes of the envelope
	 * @param p_encoding The encoding announced by the envelope
	 * @return The content, or a validation error when the bytes do not match the encoding
	 **/
	[[nodiscard]] Result<MessageContent> decode_content(std::span<const std::uint8_t> p_bytes, MessageBinaryFormat p_encoding);

} // namespace cam::transport
