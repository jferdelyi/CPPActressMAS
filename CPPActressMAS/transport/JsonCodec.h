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
	 * JSON codec (id 3, version 1): the envelope as UTF-8 JSON text, field names of envelope.proto, enums
	 * as lower-case strings, byte fields in base64. Readable on the wire (a text content travels in
	 * clear), tolerant on input (unknown members ignored, absent members at their default), meant for
	 * debugging and for the simplest clients.
	 **/
	class JsonCodec final : public EnvelopeCodec {
	public:
		static constexpr std::uint8_t ID = 3;
		static constexpr std::uint8_t VERSION_MAJOR = 1;

		[[nodiscard]] std::uint8_t id() const override { return ID; }
		[[nodiscard]] std::uint8_t version_major() const override { return VERSION_MAJOR; }
		[[nodiscard]] const char* name() const override { return "json"; }
		[[nodiscard]] Result<std::vector<std::uint8_t>> encode(const Envelope& p_envelope) const override;
		[[nodiscard]] Result<Envelope> decode(std::span<const std::uint8_t> p_bytes, std::size_t p_max_bytes) const override;
	};

} // namespace cam::transport
