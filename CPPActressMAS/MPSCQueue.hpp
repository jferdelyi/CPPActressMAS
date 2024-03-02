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

/**
 * CPPActressMAS
 */
namespace cam {

    /**
     * MPSC (Multiple Producer Single Consumer) lock free queue from CPP Benchmark
     * https://github.com/chronoxor/CppBenchmark/blob/master/examples/lockfree/mpsc-queue.hpp
     */
    template<typename T> class MPSCQueue {
    	/**
		 * Node struct
		 */
    	struct MPSCQueueNode {
    		T data;
    		std::atomic<MPSCQueueNode*> next;
    	};
    	typedef char MPSCQueuePad[64];

    	/**
		 * Head of the queue
		 */
    	MPSCQueuePad m_head_pad;
    	std::atomic<MPSCQueueNode*> m_head;

    	/**
		 * Tail of the queue
		 */
    	MPSCQueuePad m_tail_pad;
    	std::atomic<MPSCQueueNode*> m_tail;

    public:

        /**
         * MPSC (Multiple Producer Single Consumer) lock free queue constructor
         */
        MPSCQueue() :
            m_head_pad{},
            m_head(new MPSCQueueNode),
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
            const MPSCQueueNode* l_front = m_head.load(std::memory_order_relaxed);
            delete l_front;
        }

        /**
         * Enqueue new item
         * @param p_input_item new item
         */
        void enqueue(const T& p_input_item) {
            auto* l_node = new MPSCQueueNode;
            l_node->data = p_input_item;
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

            p_output_item = l_next->data;
            m_tail.store(l_next, std::memory_order_release);
            delete l_tail;
            return true;
        }

    	// Delete copy constructor
    	MPSCQueue(const MPSCQueue& ) = delete;
    	MPSCQueue& operator=(MPSCQueue& ) = delete;
    };

} // namespace cam
