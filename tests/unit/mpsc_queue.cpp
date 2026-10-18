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

// Queue of the agent messages (P8): items come out in arrival order, every item exactly once even when several threads
// enqueue while the consumer dequeues, and the nodes recycled by each thread are freed when the thread ends.

#include <doctest/doctest.h>

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

#include <util/MPSCQueue.hpp>

TEST_SUITE("unit") {

	TEST_CASE("a queue gives its items back in arrival order and does not keep them alive") {
		cam::MPSCQueue<std::shared_ptr<int>> l_queue;
		std::shared_ptr<int> l_item;
		CHECK_FALSE(l_queue.dequeue(l_item));

		// Twice, so the second round runs on recycled nodes
		for (int l_round = 0; l_round < 2; l_round++) {
			for (int i = 0; i < 100; i++) {
				l_queue.enqueue(std::make_shared<int>(i));
			}
			for (int i = 0; i < 100; i++) {
				REQUIRE(l_queue.dequeue(l_item));
				CHECK(*l_item == i);
				CHECK(l_item.use_count() == 1);  // moved out, no node holds it any more (C17)
			}
			CHECK_FALSE(l_queue.dequeue(l_item));
		}
	}

	TEST_CASE("items enqueued by several threads while the consumer dequeues are all received once, in order per producer") {
		constexpr int PRODUCERS = 4;
		constexpr int ITEMS = 20000;
		cam::MPSCQueue<int> l_queue;
		std::vector<std::thread> l_producers;
		for (int p = 0; p < PRODUCERS; p++) {
			l_producers.emplace_back([&l_queue, p] {
				for (int i = 0; i < ITEMS; i++) {
					l_queue.enqueue(p * ITEMS + i);
				}
			});
		}
		std::vector<int> l_last(PRODUCERS, -1);
		int l_received = 0;
		bool l_ordered = true;
		int l_item = 0;
		while (l_received < PRODUCERS * ITEMS) {
			if (l_queue.dequeue(l_item)) {
				const int l_producer = l_item / ITEMS;
				l_ordered = l_ordered && l_item > l_last[l_producer];
				l_last[l_producer] = l_item;
				l_received++;
			} else {
				std::this_thread::yield();
			}
		}
		for (auto& l_producer: l_producers) {
			l_producer.join();
		}
		CHECK(l_ordered);
		CHECK(l_received == PRODUCERS * ITEMS);
	}

	TEST_CASE("threads that end with recycled nodes, and queues that outlive or move between threads, keep working") {
		cam::MPSCQueue<std::shared_ptr<int>> l_shared;
		std::vector<std::thread> l_workers;
		for (int w = 0; w < 4; w++) {
			l_workers.emplace_back([&l_shared, w] {
				// Each thread fills and drains its own queue, so it ends holding free nodes (freed at its end, no leak)
				cam::MPSCQueue<std::shared_ptr<int>> l_local;
				std::shared_ptr<int> l_item;
				for (int i = 0; i < 10000; i++) {
					l_local.enqueue(std::make_shared<int>(i));
					l_local.dequeue(l_item);
				}
				l_shared.enqueue(std::make_shared<int>(w));
			});
		}
		for (auto& l_worker: l_workers) {
			l_worker.join();
		}

		// The nodes enqueued by the ended threads are dequeued and recycled by this one
		int l_count = 0;
		std::shared_ptr<int> l_item;
		while (l_shared.dequeue(l_item)) {
			l_count++;
		}
		CHECK(l_count == 4);

		// A queue created here and destroyed by another thread
		auto l_moved = std::make_unique<cam::MPSCQueue<std::shared_ptr<int>>>();
		l_moved->enqueue(std::make_shared<int>(1));
		std::thread([l_queue = std::move(l_moved)]() mutable { l_queue.reset(); }).join();
		CHECK(l_moved == nullptr);
	}
}
