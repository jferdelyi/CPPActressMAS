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

#include <queue>

/**
 * CPPActressMAS
 */
namespace cam {
	template <typename T> class ConcurrentQueue {
		protected:
			/**
			 * The queue
			 **/
			std::queue<T> m_queue;

			/**
			 * Mutex for the queue
			**/
			std::mutex m_mutex;
			std::condition_variable m_cond;

		public:
			/**
			 * Return the size of the queue
			 * @return the size of the queue
			 **/
			size_t size() { return m_queue.size(); }

			/**
			 * Enqueue new item
			 * @param p_item new item
			 **/
			void enqueue(const T& p_item) {
				std::unique_lock l_lock(m_mutex);
				m_queue.push(p_item);
				l_lock.unlock();
				m_cond.notify_one();
			}

			/**
			 * Enqueue new item (move)
			 * @param p_item new item
			 **/
			void enqueue(T&& p_item) {
				std::unique_lock l_lock(m_mutex);
				m_queue.push(std::move(p_item));
				l_lock.unlock();
				m_cond.notify_one();
			}

			/**
			 * Dequeue item
			 * @return first item of the stack
			 **/
			T dequeue() {
				std::unique_lock l_lock(m_mutex);
				while (m_queue.empty()) {
					m_cond.wait(l_lock);
				}
				auto item = m_queue.front();
				m_queue.pop();
				return item;
			}

			/**
			 * Dequeue item (output param)
			 * @param p_item first item of the stack
			 **/
			void dequeue(T& p_item) {
				std::unique_lock l_lock(m_mutex);
				while (m_queue.empty()) {
					m_cond.wait(l_lock);
				}
				p_item = m_queue.front();
				m_queue.pop();
			}
	};
} // namespace cam
