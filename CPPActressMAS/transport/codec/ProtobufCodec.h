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

#include "EnvelopeCodec.h"

/**
 * CPPActressMAS transport layer.
 */
namespace cam::transport {

	/**
	 * Protobuf codec (id 2, version 1): the proto3 wire format of envelope.proto, written and read
	 * directly (varints, tags, length-delimited fields), without libprotobuf. Any proto3 implementation
	 * fed with envelope.proto speaks it; unknown fields are skipped, defaults are omitted. The default
	 * codec of the transports.
	 **/
	class ProtobufCodec final : public EnvelopeCodec {
	public:
		static constexpr std::uint8_t ID = 2;
		static constexpr std::uint8_t VERSION_MAJOR = 1;

		[[nodiscard]] std::uint8_t id() const override { return ID; }
		[[nodiscard]] std::uint8_t version_major() const override { return VERSION_MAJOR; }
		[[nodiscard]] const char* name() const override { return "protobuf"; }
		[[nodiscard]] Result<std::vector<std::uint8_t>> encode(const Envelope& p_envelope) const override;
		[[nodiscard]] Result<Envelope> decode(std::span<const std::uint8_t> p_bytes, std::size_t p_max_bytes) const override;
	};

} // namespace cam::transport
