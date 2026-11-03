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

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>

/**
 * CPPActressMAS transport layer.
 **/
namespace cam::transport {

	/**
	 * One thread running jobs in order. The Zenoh transport uses it for the calls into the client library
	 * asked by that library's own callbacks (a presence request from a liveliness callback): calling the
	 * client from inside one of its callbacks while another thread uses it may deadlock.
	 **/
	class Worker final {
	private:
		/**
		 * The thread, from start() to stop().
		 **/
		std::thread m_thread;

		/**
		 * Guards m_jobs and m_stop.
		 **/
		std::mutex m_mutex;

		/**
		 * Wakes the thread for a job or for the stop.
		 **/
		std::condition_variable m_condition;

		/**
		 * Jobs waiting, oldest first.
		 **/
		std::deque<std::function<void()>> m_jobs;

		/**
		 * True once stop() was asked.
		 **/
		bool m_stop = false;

		/**
		 * Body of the thread: run the jobs in order until the stop.
		 **/
		void run();

	public:
		/**
		 * Create a worker; its thread starts with start().
		 **/
		Worker() = default;

		/**
		 * Stop the thread.
		 **/
		~Worker();

		// Delete copy constructor
		Worker(const Worker&) = delete;

		Worker& operator=(const Worker&) = delete;

		/**
		 * Start the thread (idempotent).
		 **/
		void start();

		/**
		 * Stop the thread after the current job, drop the pending ones, join.
		 **/
		void stop();

		/**
		 * Queue a job.
		 * @param p_job The job
		 **/
		void post(std::function<void()> p_job);
	};

} // namespace cam::transport
