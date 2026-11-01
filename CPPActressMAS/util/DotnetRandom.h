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

#include <array>
#include <cstdint>

/**
 * CPPActressMAS
 */
namespace cam {

	/**
	 * Random generator drawing exactly as the seeded System.Random of .NET (the subtractive generator of Knuth, which
	 * .NET 6 and later keep for a seeded instance): with the same seed and the same calls, a port draws the same numbers
	 * as the C#. Not thread-safe.
	 **/
	class DotnetRandom final {
		private:
			/**
			 * State of the generator (index 0 unused, as in .NET).
			 **/
			std::array<std::int32_t, 56> m_seed_array{};

			/**
			 * Indices of the next draw.
			 **/
			int m_next = 0;
			int m_next_p = 21;

			/**
			 * Next raw value, in [0, INT32_MAX).
			 * @return The value
			 **/
			std::int32_t internal_sample() noexcept;

			/**
			 * Next value in [0, 1).
			 * @return The value
			 **/
			double sample() noexcept;

			/**
			 * Value in [0, 1) for a range wider than INT32_MAX: two draws, as .NET.
			 * @return The value
			 **/
			double sample_for_large_range() noexcept;

		public:
			/**
			 * Generator of a seed, as new Random(seed).
			 * @param p_seed The seed
			 **/
			explicit DotnetRandom(std::int32_t p_seed) noexcept;

			/**
			 * As Next(): a value in [0, INT32_MAX).
			 * @return The value
			 **/
			std::int32_t next() noexcept;

			/**
			 * As Next(max): a value in [0, max), 0 when max is 0 (one draw all the same).
			 * @param p_max The exclusive upper bound
			 * @return The value
			 * @throw std::out_of_range If p_max is negative
			 **/
			std::int32_t next(std::int32_t p_max);

			/**
			 * As Next(min, max): a value in [min, max), min when they are equal (one draw all the same).
			 * @param p_min The inclusive lower bound
			 * @param p_max The exclusive upper bound
			 * @return The value
			 * @throw std::out_of_range If p_min is greater than p_max
			 **/
			std::int32_t next(std::int32_t p_min, std::int32_t p_max);

			/**
			 * As NextDouble(): a value in [0, 1).
			 * @return The value
			 **/
			double next_double() noexcept;
	};

} // namespace cam
