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

#include "Base64.h"

namespace cam {

	namespace {

		constexpr std::string_view ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

		int value_of(const char p_char) {
			if (p_char >= 'A' && p_char <= 'Z') {
				return p_char - 'A';
			}
			if (p_char >= 'a' && p_char <= 'z') {
				return p_char - 'a' + 26;
			}
			if (p_char >= '0' && p_char <= '9') {
				return p_char - '0' + 52;
			}
			if (p_char == '+') {
				return 62;
			}
			if (p_char == '/') {
				return 63;
			}
			return -1;
		}

	} // namespace

	std::string to_base64(const std::span<const std::uint8_t> p_bytes) {
		std::string l_out;
		l_out.reserve((p_bytes.size() + 2) / 3 * 4);
		std::size_t i = 0;
		for (; i + 2 < p_bytes.size(); i += 3) {
			const std::uint32_t l_block = (static_cast<std::uint32_t>(p_bytes[i]) << 16) | (static_cast<std::uint32_t>(p_bytes[i + 1]) << 8) |
										  p_bytes[i + 2];
			l_out.push_back(ALPHABET[(l_block >> 18) & 63]);
			l_out.push_back(ALPHABET[(l_block >> 12) & 63]);
			l_out.push_back(ALPHABET[(l_block >> 6) & 63]);
			l_out.push_back(ALPHABET[l_block & 63]);
		}
		if (i + 1 == p_bytes.size()) {
			const std::uint32_t l_block = static_cast<std::uint32_t>(p_bytes[i]) << 16;
			l_out.push_back(ALPHABET[(l_block >> 18) & 63]);
			l_out.push_back(ALPHABET[(l_block >> 12) & 63]);
			l_out.append("==");
		} else if (i + 2 == p_bytes.size()) {
			const std::uint32_t l_block = (static_cast<std::uint32_t>(p_bytes[i]) << 16) | (static_cast<std::uint32_t>(p_bytes[i + 1]) << 8);
			l_out.push_back(ALPHABET[(l_block >> 18) & 63]);
			l_out.push_back(ALPHABET[(l_block >> 12) & 63]);
			l_out.push_back(ALPHABET[(l_block >> 6) & 63]);
			l_out.push_back('=');
		}
		return l_out;
	}

	std::optional<std::vector<std::uint8_t>> from_base64(const std::string_view p_text) {
		if (p_text.size() % 4 != 0) {
			return std::nullopt;
		}
		std::vector<std::uint8_t> l_out;
		l_out.reserve(p_text.size() / 4 * 3);
		for (std::size_t i = 0; i < p_text.size(); i += 4) {
			int l_values[4];
			int l_padding = 0;
			for (std::size_t k = 0; k < 4; k++) {
				const char l_char = p_text[i + k];
				if (l_char == '=') {
					// Padding only in the last two places of the last block
					if (i + 4 != p_text.size() || k < 2) {
						return std::nullopt;
					}
					l_values[k] = 0;
					l_padding++;
				} else {
					if (l_padding > 0) {
						return std::nullopt;
					}
					l_values[k] = value_of(l_char);
					if (l_values[k] < 0) {
						return std::nullopt;
					}
				}
			}
			const std::uint32_t l_block = (static_cast<std::uint32_t>(l_values[0]) << 18) | (static_cast<std::uint32_t>(l_values[1]) << 12) |
										  (static_cast<std::uint32_t>(l_values[2]) << 6) | static_cast<std::uint32_t>(l_values[3]);
			l_out.push_back(static_cast<std::uint8_t>(l_block >> 16));
			if (l_padding < 2) {
				l_out.push_back(static_cast<std::uint8_t>(l_block >> 8));
			}
			if (l_padding < 1) {
				l_out.push_back(static_cast<std::uint8_t>(l_block));
			}
		}
		return l_out;
	}

} // namespace cam
