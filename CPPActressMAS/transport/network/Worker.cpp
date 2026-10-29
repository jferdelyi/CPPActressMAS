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

#include "Worker.h"

namespace cam::transport {

	Worker::~Worker() {
		stop();
	}

	void Worker::start() {
		if (m_thread.joinable()) {
			return;
		}
		// The jobs posted before the start are kept: they run first
		{
			const std::lock_guard<std::mutex> l_lock(m_mutex);
			m_stop = false;
		}
		m_thread = std::thread(&Worker::run, this);
	}

	void Worker::stop() {
		{
			const std::lock_guard<std::mutex> l_lock(m_mutex);
			m_stop = true;
		}
		m_condition.notify_all();
		if (m_thread.joinable()) {
			m_thread.join();
		}
		const std::lock_guard<std::mutex> l_lock(m_mutex);
		m_jobs.clear();
	}

	void Worker::post(std::function<void()> p_job) {
		{
			const std::lock_guard<std::mutex> l_lock(m_mutex);
			m_jobs.push_back(std::move(p_job));
		}
		m_condition.notify_one();
	}

	void Worker::run() {
		while (true) {
			std::function<void()> l_job;
			{
				std::unique_lock<std::mutex> l_lock(m_mutex);
				m_condition.wait(l_lock, [this] { return m_stop || !m_jobs.empty(); });
				if (m_stop) {
					return;
				}
				l_job = std::move(m_jobs.front());
				m_jobs.pop_front();
			}
			l_job();
		}
	}

} // namespace cam::transport
