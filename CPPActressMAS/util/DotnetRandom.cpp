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

#include "DotnetRandom.h"

#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace {

	constexpr std::int32_t INT32_MAXIMUM = std::numeric_limits<std::int32_t>::max();
	constexpr std::int32_t INT32_MINIMUM = std::numeric_limits<std::int32_t>::min();
	constexpr std::int32_t SEED = 161803398;

	// Unchecked int subtraction of C#: modulo 2^32, where a signed overflow would be undefined behaviour in C++
	std::int32_t subtract(const std::int32_t p_left, const std::int32_t p_right) {
		return static_cast<std::int32_t>(static_cast<std::uint32_t>(p_left) - static_cast<std::uint32_t>(p_right));
	}

} // namespace

cam::DotnetRandom::DotnetRandom(const std::int32_t p_seed) noexcept {
	const std::int32_t l_subtraction = p_seed == INT32_MINIMUM ? INT32_MAXIMUM : std::abs(p_seed);
	std::int32_t l_mj = SEED - l_subtraction;
	m_seed_array[55] = l_mj;
	std::int32_t l_mk = 1;
	for (int i = 1; i < 55; i++) {
		const int l_ii = (21 * i) % 55;
		m_seed_array[l_ii] = l_mk;
		l_mk = subtract(l_mj, l_mk);
		if (l_mk < 0) {
			l_mk += INT32_MAXIMUM;
		}
		l_mj = m_seed_array[l_ii];
	}
	for (int k = 1; k < 5; k++) {
		for (int i = 1; i < 56; i++) {
			m_seed_array[i] = subtract(m_seed_array[i], m_seed_array[1 + (i + 30) % 55]);
			if (m_seed_array[i] < 0) {
				m_seed_array[i] += INT32_MAXIMUM;
			}
		}
	}
}

std::int32_t cam::DotnetRandom::internal_sample() noexcept {
	int l_next = m_next;
	int l_next_p = m_next_p;
	if (++l_next >= 56) {
		l_next = 1;
	}
	if (++l_next_p >= 56) {
		l_next_p = 1;
	}
	std::int32_t l_result = subtract(m_seed_array[l_next], m_seed_array[l_next_p]);
	if (l_result == INT32_MAXIMUM) {
		l_result--;
	}
	if (l_result < 0) {
		l_result += INT32_MAXIMUM;
	}
	m_seed_array[l_next] = l_result;
	m_next = l_next;
	m_next_p = l_next_p;
	return l_result;
}

double cam::DotnetRandom::sample() noexcept {
	return internal_sample() * (1.0 / INT32_MAXIMUM);
}

double cam::DotnetRandom::sample_for_large_range() noexcept {
	std::int32_t l_result = internal_sample();
	if (internal_sample() % 2 == 0) {
		l_result = -l_result;
	}
	double l_value = l_result;
	l_value += INT32_MAXIMUM - 1;
	l_value /= 2.0 * static_cast<std::uint32_t>(INT32_MAXIMUM) - 1;
	return l_value;
}

std::int32_t cam::DotnetRandom::next() noexcept {
	return internal_sample();
}

std::int32_t cam::DotnetRandom::next(const std::int32_t p_max) {
	if (p_max < 0) {
		throw std::out_of_range("DotnetRandom::next: the maximum is negative");
	}
	return static_cast<std::int32_t>(sample() * p_max);
}

std::int32_t cam::DotnetRandom::next(const std::int32_t p_min, const std::int32_t p_max) {
	if (p_min > p_max) {
		throw std::out_of_range("DotnetRandom::next: the minimum is greater than the maximum");
	}
	const std::int64_t l_range = static_cast<std::int64_t>(p_max) - p_min;
	if (l_range <= INT32_MAXIMUM) {
		return static_cast<std::int32_t>(sample() * static_cast<double>(l_range)) + p_min;
	}
	return static_cast<std::int32_t>(static_cast<std::int64_t>(sample_for_large_range() * static_cast<double>(l_range)) + p_min);
}

double cam::DotnetRandom::next_double() noexcept {
	return sample();
}
