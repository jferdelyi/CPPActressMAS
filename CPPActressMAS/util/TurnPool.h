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

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

/**
 * CPPActressMAS.
 **/
namespace cam {

	class Agent;

	/**
	 * Persistent threads that run the agents of a Parallel turn. Each turn wakes the threads once; they and
	 * the calling thread take the agents one by one through an atomic index, so no task, future or allocation is
	 * made per agent. Between turns a thread spins briefly, turns being close to each other, then sleeps on an
	 * atomic wait. The exceptions thrown by the agents are kept, and leave run() together once every agent of the turn
	 * has run (AggregateException), as Parallel.Invoke in the C#.
	 **/
	class TurnPool final {
	private:
		/**
		 * The worker threads (the calling thread of run() works too).
		 **/
		std::vector<std::thread> m_threads;

		/**
		 * Agents of the current turn, valid while run() is running.
		 **/
		const std::vector<Agent*>* m_agents = nullptr;

		/**
		 * Index of the next agent to run.
		 **/
		std::atomic<std::size_t> m_next{0};

		/**
		 * Worker threads still busy with the current turn.
		 **/
		std::atomic<std::size_t> m_busy{0};

		/**
		 * Incremented to start a turn (and once more to stop the threads).
		 **/
		std::atomic<std::uint64_t> m_generation{0};

		/**
		 * True once the threads must end.
		 **/
		std::atomic<bool> m_stop{false};

		/**
		 * Guards m_exceptions.
		 **/
		std::mutex m_exceptions_mutex;

		/**
		 * Exceptions thrown by the agents of the current turn, with the index of their agent.
		 **/
		std::vector<std::pair<std::size_t, std::exception_ptr>> m_exceptions;

		/**
		 * Loop of a worker thread.
		 **/
		void work();

		/**
		 * Run agents of the current turn until none is left.
		 **/
		void run_agents();

	public:
		/**
		 * Start the worker threads.
		 * @param p_threads Number of worker threads, besides the calling thread
		 **/
		explicit TurnPool(std::size_t p_threads);

		/**
		 * Stop and join the worker threads.
		 **/
		~TurnPool();

		/**
		 * The threads refer to this pool: no copy.
		 **/
		TurnPool(const TurnPool&) = delete;

		/**
		 * The threads refer to this pool: no copy.
		 **/
		TurnPool& operator=(const TurnPool&) = delete;

		/**
		 * Run one turn of every agent and return when all are done; then throw the exceptions of the agents, if any, in
		 * an AggregateException, in the order of the agents.
		 * @param p_agents The agents of the turn
		 **/
		void run(const std::vector<Agent*>& p_agents);
	};

} // namespace cam
