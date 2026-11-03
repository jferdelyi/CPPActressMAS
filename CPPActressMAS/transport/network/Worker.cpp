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
