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

#include "TurnPool.h"

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#endif

#include <algorithm>

#include "../Agent.h"
#include "../AggregateException.h"

namespace {

	// Iterations of the short busy wait before sleeping: turns follow each other within microseconds
	constexpr int SPIN_COUNT = 1 << 12;

	// Tell the processor this is a busy wait (cheaper for the sibling hyper-thread, less power)
	void relax() {
#if defined(__x86_64__) || defined(_M_X64)
		_mm_pause();
#elif defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
		asm volatile("yield");
#endif
	}

	// Wait until the atomic leaves the given value: spin first, then sleep
	template<typename T>
	T wait_change(const std::atomic<T>& p_atomic, const T p_old) {
		T l_value = p_atomic.load(std::memory_order_acquire);
		for (int i = 0; l_value == p_old && i < SPIN_COUNT; i++) {
			relax();
			l_value = p_atomic.load(std::memory_order_acquire);
		}
		while (l_value == p_old) {
			p_atomic.wait(p_old, std::memory_order_acquire);
			l_value = p_atomic.load(std::memory_order_acquire);
		}
		return l_value;
	}

} // namespace

cam::TurnPool::TurnPool(const std::size_t p_threads) {
	m_threads.reserve(p_threads);
	for (std::size_t i = 0; i < p_threads; i++) {
		m_threads.emplace_back([this] { work(); });
	}
}

cam::TurnPool::~TurnPool() {
	m_stop.store(true, std::memory_order_release);
	m_generation.fetch_add(1, std::memory_order_release);
	m_generation.notify_all();
	for (auto& l_thread: m_threads) {
		l_thread.join();
	}
}

void cam::TurnPool::run(const std::vector<Agent*>& p_agents, const bool p_run_setup_separately) {
	// Published to the workers by the release of the generation
	m_agents = &p_agents;
	m_run_setup_separately = p_run_setup_separately;
	m_next.store(0, std::memory_order_relaxed);
	m_busy.store(m_threads.size(), std::memory_order_relaxed);
	m_generation.fetch_add(1, std::memory_order_release);
	m_generation.notify_all();
	run_agents();
	for (std::size_t l_busy = m_busy.load(std::memory_order_acquire); l_busy != 0;) {
		l_busy = wait_change(m_busy, l_busy);
	}
	m_agents = nullptr;

	// Every agent of the turn has run: their exceptions leave together, in the order of the agents, as Parallel.Invoke
	std::vector<std::pair<std::size_t, std::exception_ptr>> l_thrown;
	{
		const std::lock_guard<std::mutex> l_lock(m_exceptions_mutex);
		l_thrown.swap(m_exceptions);
	}
	if (!l_thrown.empty()) {
		std::sort(l_thrown.begin(), l_thrown.end(), [](const auto& p_left, const auto& p_right) { return p_left.first < p_right.first; });
		std::vector<std::exception_ptr> l_exceptions;
		l_exceptions.reserve(l_thrown.size());
		for (auto& l_entry: l_thrown) {
			l_exceptions.push_back(std::move(l_entry.second));
		}
		throw AggregateException(std::move(l_exceptions));
	}
}

void cam::TurnPool::work() {
	std::uint64_t l_seen = 0;
	while (true) {
		l_seen = wait_change(m_generation, l_seen);
		if (m_stop.load(std::memory_order_acquire)) {
			return;
		}
		run_agents();
		if (m_busy.fetch_sub(1, std::memory_order_acq_rel) == 1) {
			m_busy.notify_one();
		}
	}
}

void cam::TurnPool::run_agents() {
	const std::vector<Agent*>& l_agents = *m_agents;
	for (std::size_t l_index = m_next.fetch_add(1, std::memory_order_relaxed); l_index < l_agents.size();
		 l_index = m_next.fetch_add(1, std::memory_order_relaxed)) {
		try {
			l_agents[l_index]->run_parallel_turn(m_run_setup_separately);  // even if removed since the turn began, as in the C#
		} catch (...) {
			// Kept: the exceptions of the turn leave run() together once every agent has run
			const std::lock_guard<std::mutex> l_lock(m_exceptions_mutex);
			m_exceptions.emplace_back(l_index, std::current_exception());
		}
	}
}
