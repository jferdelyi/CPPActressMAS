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

/**
 * CPPActressMAS
 */
namespace cam {

	/**
	 * MPSC (Multiple Producer Single Consumer) lock free queue from CPP Benchmark
	 * https://github.com/chronoxor/CppBenchmark/blob/master/examples/lockfree/mpsc-queue.hpp
	 * The nodes are recycled through a cache per thread, so a warm queue allocates nothing (P8).
	 */
	template<typename T>
	class MPSCQueue {
		/**
		 * Node struct
		 */
		struct MPSCQueueNode {
			T data;
			std::atomic<MPSCQueueNode*> next;
		};
		typedef char MPSCQueuePad[64];

		/**
		 * Most free nodes kept by one thread, beyond them a node is deleted
		 */
		static constexpr std::size_t MAX_FREE_NODES = 4096;

		/**
		 * Free nodes of the current thread, linked by next (trivially destructible: usable until the thread ends)
		 */
		static inline thread_local MPSCQueueNode* s_free_nodes = nullptr;

		/**
		 * Number of free nodes of the current thread
		 */
		static inline thread_local std::size_t s_free_count = 0;

		/**
		 * True once the free nodes of the current thread are deleted: the nodes released later are deleted at once
		 */
		static inline thread_local bool s_free_closed = false;

		/**
		 * Deletes the free nodes of a thread when it ends
		 */
		struct FreeNodesOwner {
			~FreeNodesOwner() {
				while (MPSCQueueNode* l_node = s_free_nodes) {
					s_free_nodes = l_node->next.load(std::memory_order_relaxed);
					delete l_node;
				}
				s_free_count = 0;
				s_free_closed = true;
			}
		};

		/**
		 * Take a free node of the current thread, or a new one
		 * @return The node
		 */
		static MPSCQueueNode* acquire_node() {
			MPSCQueueNode* l_node = s_free_nodes;
			if (l_node == nullptr) {
				return new MPSCQueueNode;
			}
			s_free_nodes = l_node->next.load(std::memory_order_relaxed);
			s_free_count--;
			return l_node;
		}

		/**
		 * Give a node back to the current thread, whose item was moved out
		 * @param p_node The node
		 */
		static void release_node(MPSCQueueNode* p_node) {
			if (s_free_closed || s_free_count >= MAX_FREE_NODES) {
				delete p_node;
				return;
			}
			if (s_free_nodes == nullptr) {
				// The first node kept by this thread makes sure the cache is deleted when the thread ends
				static thread_local FreeNodesOwner s_owner;
				(void) s_owner;
			}
			p_node->next.store(s_free_nodes, std::memory_order_relaxed);
			s_free_nodes = p_node;
			s_free_count++;
		}

		/**
		 * Head of the queue
		 */
		[[maybe_unused]] MPSCQueuePad m_head_pad;
		std::atomic<MPSCQueueNode*> m_head;

		/**
		 * Tail of the queue
		 */
		[[maybe_unused]] MPSCQueuePad m_tail_pad;
		std::atomic<MPSCQueueNode*> m_tail;

	public:

		/**
		 * MPSC (Multiple Producer Single Consumer) lock free queue constructor
		 */
		MPSCQueue() :
				m_head_pad{},
				m_head(acquire_node()),
				m_tail_pad{},
				m_tail(m_head.load(std::memory_order_relaxed)) {
			MPSCQueueNode* l_front = m_head.load(std::memory_order_relaxed);
			l_front->next.store(nullptr, std::memory_order_relaxed);
		}

		/**
		 * Free all nodes
		 */
		~MPSCQueue() {
			T l_output;
			while (this->dequeue(l_output)) {}
			release_node(m_head.load(std::memory_order_relaxed));
		}

		/**
		 * Enqueue new item
		 * @param p_input_item new item
		 */
		void enqueue(const T& p_input_item) {
			enqueue(T(p_input_item));
		}

		/**
		 * Enqueue new item, moved in
		 * @param p_input_item new item
		 */
		void enqueue(T&& p_input_item) {
			auto* l_node = acquire_node();
			l_node->data = std::move(p_input_item);
			l_node->next.store(nullptr, std::memory_order_relaxed);
			MPSCQueueNode* l_prev_head = m_head.exchange(l_node, std::memory_order_acq_rel);
			l_prev_head->next.store(l_node, std::memory_order_release);
		}

		/**
		 * Dequeue last item
		 * @param p_output_item last item
		 * @return false if the queue is empty
		 */
		bool dequeue(T& p_output_item) {
			MPSCQueueNode* l_tail = m_tail.load(std::memory_order_relaxed);
			MPSCQueueNode* l_next = l_tail->next.load(std::memory_order_acquire);

			if (l_next == nullptr) {
				return false;
			}

			// Moved out: the node that becomes the sentinel no longer keeps the item alive (C17)
			p_output_item = std::move(l_next->data);
			m_tail.store(l_next, std::memory_order_release);
			release_node(l_tail);
			return true;
		}

		// Delete copy constructor
		MPSCQueue(const MPSCQueue&) = delete;

		MPSCQueue& operator=(MPSCQueue&) = delete;
	};

} // namespace cam
