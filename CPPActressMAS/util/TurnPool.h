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

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <thread>
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
	 * atomic wait. An exception thrown by an agent is swallowed, as with the former thread pool.
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
		 * Run one turn of every agent and return when all are done.
		 * @param p_agents The agents of the turn
		 **/
		void run(const std::vector<Agent*>& p_agents);
	};

} // namespace cam
