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

#include <algorithm>
#include <mutex>
#include <string>
#include <vector>

namespace cam::test {

	/**
	 * Thread-safe event log for tests.
	 *
	 * Agents never assert (an exception thrown by an agent is swallowed in Parallel mode):
	 * they record what happens here and the test checks the entries after start() returns.
	 **/
	class Recorder final {
	private:
		mutable std::mutex m_mutex;
		std::vector<std::string> m_entries;

	public:
		/**
		 * Append an entry.
		 * @param p_entry The entry
		 **/
		void record(const std::string& p_entry) {
			const std::lock_guard<std::mutex> l_lock(m_mutex);
			m_entries.push_back(p_entry);
		}

		/**
		 * Copy of all entries, in recording order.
		 * @return The entries
		 **/
		[[nodiscard]] std::vector<std::string> entries() const {
			const std::lock_guard<std::mutex> l_lock(m_mutex);
			return m_entries;
		}

		/**
		 * Number of entries.
		 * @return The count
		 **/
		[[nodiscard]] std::size_t count() const {
			const std::lock_guard<std::mutex> l_lock(m_mutex);
			return m_entries.size();
		}

		/**
		 * Number of entries equal to a value.
		 * @param p_entry The value
		 * @return The count
		 **/
		[[nodiscard]] std::size_t count_of(const std::string& p_entry) const {
			const std::lock_guard<std::mutex> l_lock(m_mutex);
			return static_cast<std::size_t>(std::count(m_entries.begin(), m_entries.end(), p_entry));
		}

		/**
		 * True if an entry equal to the value exists.
		 * @param p_entry The value
		 * @return True if present
		 **/
		[[nodiscard]] bool contains(const std::string& p_entry) const {
			return count_of(p_entry) > 0;
		}

		/**
		 * Forget all entries.
		 **/
		void clear() {
			const std::lock_guard<std::mutex> l_lock(m_mutex);
			m_entries.clear();
		}
	};

} // namespace cam::test
