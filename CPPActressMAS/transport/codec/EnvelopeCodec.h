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
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "../Envelope.h"

/**
 * CPPActressMAS transport layer: interchangeable envelope codec. Every encoded envelope starts
 * with a fixed 4-byte header: magic 0xCA, codec id, codec major version, reserved. One codec per
 * transport instance, never per message.
 */
namespace cam::transport {

	constexpr std::uint8_t ENVELOPE_MAGIC = 0xCA;
	constexpr std::size_t ENVELOPE_HEADER_SIZE = 4;
	constexpr std::size_t ENVELOPE_MAX_STRING_BYTES = 65535;  ///< longest string a codec accepts
	constexpr std::size_t ENVELOPE_MAX_LIST_ITEMS = 65535;    ///< longest list a codec accepts

	/**
	 * Header found in front of every encoded envelope.
	 **/
	struct EnvelopeHeader {
		std::uint8_t m_codec_id = 0;
		std::uint8_t m_version_major = 0;
	};

	/**
	 * Interface of an envelope codec.
	 **/
	class EnvelopeCodec {
	public:
		virtual ~EnvelopeCodec() = default;

		/**
		 * Identifier written in the header (1 = compact, 2 = protobuf, 3 = json).
		 * @return The identifier
		 **/
		[[nodiscard]] virtual std::uint8_t id() const = 0;

		/**
		 * Major version written in the header; a receiver refuses an unknown major version.
		 * @return The major version
		 **/
		[[nodiscard]] virtual std::uint8_t version_major() const = 0;

		/**
		 * Short name of the codec (configuration key).
		 * @return The name
		 **/
		[[nodiscard]] virtual const char* name() const = 0;

		/**
		 * Encode an envelope, header included.
		 * @param p_envelope The envelope
		 * @return The bytes, or an error
		 **/
		[[nodiscard]] virtual Result<std::vector<std::uint8_t>> encode(const Envelope& p_envelope) const = 0;

		/**
		 * Decode and validate an envelope, header included.
		 * @param p_bytes The bytes
		 * @param p_max_bytes Maximum accepted size
		 * @return The envelope, or an error
		 **/
		[[nodiscard]] virtual Result<Envelope> decode(std::span<const std::uint8_t> p_bytes, std::size_t p_max_bytes) const = 0;

		/**
		 * Read the header of an encoded envelope.
		 * @param p_bytes The bytes
		 * @return The header, or an error if the magic is wrong or the buffer too short
		 **/
		static Result<EnvelopeHeader> read_header(std::span<const std::uint8_t> p_bytes);

		/**
		 * Append the header to a buffer.
		 * @param p_out The buffer
		 * @param p_codec_id The codec identifier
		 * @param p_version_major The codec major version
		 **/
		static void write_header(std::vector<std::uint8_t>& p_out, std::uint8_t p_codec_id, std::uint8_t p_version_major);
	};

	/**
	 * Strict validation of a decoded (or about to be encoded) envelope: identifiers, consistency of the
	 * target with its fields, known encodings, payload matching the kind, sizes.
	 * @param p_envelope The envelope
	 * @param p_max_content_bytes Maximum accepted content size
	 * @return Success, or a validation error
	 **/
	[[nodiscard]] Result<void> validate_envelope(const Envelope& p_envelope, std::size_t p_max_content_bytes);

	/**
	 * The codec of a configuration name: "protobuf" (default), "compact" or "json".
	 * @param p_name The name
	 * @return The codec, or null for an unknown name
	 **/
	[[nodiscard]] std::shared_ptr<const EnvelopeCodec> make_codec(std::string_view p_name);

	/**
	 * Names accepted by make_codec, default first.
	 * @return The names
	 **/
	[[nodiscard]] std::vector<std::string> codec_names();

} // namespace cam::transport
