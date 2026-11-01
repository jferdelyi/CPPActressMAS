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
 */
namespace cam::transport {

	/**
	 * Compact codec (id 1, version 1): little-endian fixed-width scalars, 32-bit length-prefixed strings
	 * and byte arrays, no schema, no dependency. It is the floor of the envelope micro-benchmark
	 * and the fallback for constrained targets. Identifiers travel as strings.
	 **/
	class CompactCodec final : public EnvelopeCodec {
	public:
		static constexpr std::uint8_t ID = 1;
		static constexpr std::uint8_t VERSION_MAJOR = 1;

		[[nodiscard]] std::uint8_t id() const override { return ID; }
		[[nodiscard]] std::uint8_t version_major() const override { return VERSION_MAJOR; }
		[[nodiscard]] const char* name() const override { return "compact"; }
		[[nodiscard]] Result<std::vector<std::uint8_t>> encode(const Envelope& p_envelope) const override;
		[[nodiscard]] Result<Envelope> decode(std::span<const std::uint8_t> p_bytes, std::size_t p_max_bytes) const override;
	};

} // namespace cam::transport
