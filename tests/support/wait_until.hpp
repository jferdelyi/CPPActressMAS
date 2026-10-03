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

#include <chrono>
#include <thread>

namespace cam::test {

	/**
	 * Poll a condition until it holds or the deadline passes. Tests wait for a condition with a
	 * maximum delay, never for a fixed amount of time.
	 * @param p_condition Callable returning true when the awaited state is reached
	 * @param p_timeout Maximum time to wait
	 * @param p_poll Delay between two evaluations
	 * @return True if the condition held before the deadline
	 **/
	template<typename Condition>
	bool wait_until(Condition&& p_condition, const std::chrono::milliseconds p_timeout,
					const std::chrono::milliseconds p_poll = std::chrono::milliseconds(10)) {
		const auto l_deadline = std::chrono::steady_clock::now() + p_timeout;
		while (!p_condition()) {
			if (std::chrono::steady_clock::now() >= l_deadline) {
				return false;
			}
			std::this_thread::sleep_for(p_poll);
		}
		return true;
	}

} // namespace cam::test
