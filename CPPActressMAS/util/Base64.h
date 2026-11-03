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
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/**
 * CPPActressMAS.
 **/
namespace cam {

	/**
	 * Encode bytes in base64 (RFC 4648, standard alphabet, with padding), as the json codec and the
	 * ActressMAS ContainerServer (Convert.ToBase64String) write it.
	 * @param p_bytes The bytes
	 * @return The text
	 **/
	[[nodiscard]] std::string to_base64(std::span<const std::uint8_t> p_bytes);

	/**
	 * Decode base64 (RFC 4648, standard alphabet, padding required, no whitespace).
	 * @param p_text The text
	 * @return The bytes, or nothing if the text is not valid base64
	 **/
	[[nodiscard]] std::optional<std::vector<std::uint8_t>> from_base64(std::string_view p_text);

} // namespace cam
