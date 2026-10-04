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

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>

/**
 * CPPActressMAS transport layer.
 */
namespace cam::transport {

	/**
	 * One thread running jobs in order. The Zenoh transport uses it for the calls into the client library
	 * asked by that library's own callbacks (a presence request from a liveliness callback): calling the
	 * client from inside one of its callbacks while another thread uses it may deadlock.
	 **/
	class Worker final {
	private:
		std::thread m_thread;
		std::mutex m_mutex;
		std::condition_variable m_condition;
		std::deque<std::function<void()>> m_jobs;
		bool m_stop = false;

		void run();

	public:
		Worker() = default;
		~Worker();

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
