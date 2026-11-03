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

#include "EnvelopeCodec.h"

/**
 * CPPActressMAS transport layer.
 **/
namespace cam::transport {

	/**
	 * Protobuf codec (id 2, version 1): the proto3 wire format of envelope.proto, written and read
	 * directly (varints, tags, length-delimited fields), without libprotobuf. Any proto3 implementation
	 * fed with envelope.proto speaks it; unknown fields are skipped, defaults are omitted. The default
	 * codec of the transports.
	 **/
	class ProtobufCodec final : public EnvelopeCodec {
	public:
		/**
		 * Codec id, second byte of the header.
		 **/
		static constexpr std::uint8_t ID = 2;

		/**
		 * Major version of the format, third byte of the header.
		 **/
		static constexpr std::uint8_t VERSION_MAJOR = 1;

		/**
		 * Codec id.
		 * @return ID
		 **/
		[[nodiscard]] std::uint8_t id() const override { return ID; }

		/**
		 * Major version of the format.
		 * @return VERSION_MAJOR
		 **/
		[[nodiscard]] std::uint8_t version_major() const override { return VERSION_MAJOR; }

		/**
		 * Name of the codec.
		 * @return "protobuf"
		 **/
		[[nodiscard]] const char* name() const override { return "protobuf"; }

		/**
		 * Encode an envelope, header included.
		 * @param p_envelope The envelope
		 * @return The bytes, or a validation error
		 **/
		[[nodiscard]] Result<std::vector<std::uint8_t>> encode(const Envelope& p_envelope) const override;

		/**
		 * Decode an envelope, header included, and validate it.
		 * @param p_bytes The bytes
		 * @param p_max_bytes Largest size accepted
		 * @return The envelope, or a validation error
		 **/
		[[nodiscard]] Result<Envelope> decode(std::span<const std::uint8_t> p_bytes, std::size_t p_max_bytes) const override;
	};

} // namespace cam::transport
