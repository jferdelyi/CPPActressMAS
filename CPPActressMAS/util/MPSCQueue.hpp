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
#include <new>

#include "RecyclingAllocator.hpp"

/**
 * CPPActressMAS
 **/
namespace cam {

	/**
	 * MPSC (Multiple Producer Single Consumer) lock free queue from CPP Benchmark, in the public domain (Unlicense)
	 * https://github.com/chronoxor/CppBenchmark/blob/master/examples/lockfree/mpsc-queue.hpp
	 * It implements the non-intrusive MPSC node-based queue of Dmitry Vyukov
	 * http://www.1024cores.net/home/lock-free-algorithms/queues/non-intrusive-mpsc-node-based-queue
	 * The nodes come from the free blocks of each thread (FreeBlocks), so a warm queue allocates nothing.
	 **/
	template<typename T>
	class MPSCQueue {
		/**
		 * Node struct.
		 **/
		struct MPSCQueueNode {
			/**
			 * The item.
			 **/
			T m_data;

			/**
			 * Next node, null for the last one.
			 **/
			std::atomic<MPSCQueueNode*> m_next;
		};

		/**
		 * Padding of a cache line between the head and the tail.
		 **/
		typedef char MPSCQueuePad[64];

		/**
		 * A node from the free blocks of the current thread.
		 * @return The node
		 **/
		static MPSCQueueNode* acquire_node() {
			return new(FreeBlocks<sizeof(MPSCQueueNode)>::acquire()) MPSCQueueNode;
		}

		/**
		 * Give a node back to the free blocks of the current thread, whose item was moved out.
		 * @param p_node The node
		 **/
		static void release_node(MPSCQueueNode* p_node) {
			p_node->~MPSCQueueNode();
			FreeBlocks<sizeof(MPSCQueueNode)>::release(p_node);
		}

		/**
		 * Head of the queue.
		 **/
		[[maybe_unused]] MPSCQueuePad m_head_pad;
		std::atomic<MPSCQueueNode*> m_head;

		/**
		 * Tail of the queue.
		 **/
		[[maybe_unused]] MPSCQueuePad m_tail_pad;
		std::atomic<MPSCQueueNode*> m_tail;

	public:

		/**
		 * MPSC (Multiple Producer Single Consumer) lock free queue constructor.
		 **/
		MPSCQueue() :
				m_head_pad{},
				m_head(acquire_node()),
				m_tail_pad{},
				m_tail(m_head.load(std::memory_order_relaxed)) {
			MPSCQueueNode* l_front = m_head.load(std::memory_order_relaxed);
			l_front->m_next.store(nullptr, std::memory_order_relaxed);
		}

		/**
		 * Free all nodes.
		 **/
		~MPSCQueue() {
			T l_output;
			while (this->dequeue(l_output)) {}
			release_node(m_head.load(std::memory_order_relaxed));
		}

		/**
		 * Enqueue new item.
		 * @param p_input_item new item
		 **/
		void enqueue(const T& p_input_item) {
			enqueue(T(p_input_item));
		}

		/**
		 * Enqueue new item, moved in.
		 * @param p_input_item new item
		 **/
		void enqueue(T&& p_input_item) {
			auto* l_node = acquire_node();
			l_node->m_data = std::move(p_input_item);
			l_node->m_next.store(nullptr, std::memory_order_relaxed);
			MPSCQueueNode* l_prev_head = m_head.exchange(l_node, std::memory_order_acq_rel);
			l_prev_head->m_next.store(l_node, std::memory_order_release);
		}

		/**
		 * Dequeue last item.
		 * @param p_output_item last item
		 * @return false if the queue is empty
		 **/
		bool dequeue(T& p_output_item) {
			MPSCQueueNode* l_tail = m_tail.load(std::memory_order_relaxed);
			MPSCQueueNode* l_next = l_tail->m_next.load(std::memory_order_acquire);

			if (l_next == nullptr) {
				return false;
			}

			// Moved out: the node that becomes the sentinel no longer keeps the item alive
			p_output_item = std::move(l_next->m_data);
			m_tail.store(l_next, std::memory_order_release);
			release_node(l_tail);
			return true;
		}

		// Delete copy constructor
		MPSCQueue(const MPSCQueue&) = delete;

		MPSCQueue& operator=(const MPSCQueue&) = delete;
	};

} // namespace cam
