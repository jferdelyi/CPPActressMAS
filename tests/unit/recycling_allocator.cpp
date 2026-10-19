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

// Blocks recycled per thread, the memory of the messages: a thread hands out again the blocks it freed, blocks
// may be freed by another thread than the one that took them, and the blocks kept by a thread are freed when it ends.

#include <doctest/doctest.h>

#include <atomic>
#include <memory>
#include <numeric>
#include <thread>
#include <vector>

#include <util/RecyclingAllocator.hpp>

namespace {

	// Objects of a size of their own, so that no other part of the process shares their blocks
	template<std::size_t SIZE>
	struct Probe {
		static inline std::atomic<int> s_alive{0};
		char m_bytes[SIZE] = {};
		Probe() { s_alive.fetch_add(1, std::memory_order_relaxed); }
		~Probe() { s_alive.fetch_sub(1, std::memory_order_relaxed); }
		Probe(const Probe&) = delete;
		Probe& operator=(const Probe&) = delete;
	};

} // namespace

TEST_SUITE("unit") {

	TEST_CASE("a block freed by a thread is handed out again by that thread, arrays go to the standard allocator") {
		cam::RecyclingAllocator<Probe<200>> l_allocator;
		Probe<200>* l_first = l_allocator.allocate(1);
		l_allocator.deallocate(l_first, 1);
		Probe<200>* l_second = l_allocator.allocate(1);
		// Under AddressSanitizer the blocks go back to the allocator, which sees every free (RECYCLE_BLOCKS)
		if (cam::RECYCLE_BLOCKS) {
			CHECK(l_second == l_first);
		}
		l_allocator.deallocate(l_second, 1);

		std::vector<int, cam::RecyclingAllocator<int>> l_values;
		for (int i = 1; i <= 1000; i++) {
			l_values.push_back(i);
		}
		CHECK(std::accumulate(l_values.begin(), l_values.end(), 0) == 500500);
	}

	TEST_CASE("a thread keeps about 1 MiB of free blocks per size, between 64 and 4096 blocks") {
		static_assert(cam::FreeBlocks<200>::MAX_FREE_BLOCKS == 4096);
		static_assert(cam::FreeBlocks<1024>::MAX_FREE_BLOCKS == 1024);
		static_assert(cam::FreeBlocks<64 * 1024>::MAX_FREE_BLOCKS == 64);
		static_assert(cam::FreeBlocks<1>::BLOCK_SIZE == sizeof(void*));
		CHECK(cam::FreeBlocks<1024>::MAX_FREE_BLOCKS * cam::FreeBlocks<1024>::BLOCK_SIZE == std::size_t{1} << 20);
	}

	TEST_CASE("objects made through std::allocate_shared are built once and destroyed once") {
		using Object = Probe<208>;
		std::vector<std::shared_ptr<Object>> l_objects;
		for (int l_round = 0; l_round < 2; l_round++) {
			for (int i = 0; i < 1000; i++) {
				l_objects.push_back(std::allocate_shared<Object>(cam::RecyclingAllocator<Object>()));
			}
			CHECK(Object::s_alive.load() == 1000);
			l_objects.clear();
			CHECK(Object::s_alive.load() == 0);
		}
	}

	TEST_CASE("blocks taken by one thread and dropped by others, beyond what a thread keeps, are all freed once") {
		using Object = Probe<216>;
		constexpr int THREADS = 4;
		constexpr int OBJECTS = 6000;  // more than a thread keeps: the surplus goes back to the allocator
		std::vector<std::vector<std::shared_ptr<Object>>> l_slices(THREADS);
		for (auto& l_slice: l_slices) {
			for (int i = 0; i < OBJECTS; i++) {
				l_slice.push_back(std::allocate_shared<Object>(cam::RecyclingAllocator<Object>()));
			}
		}
		CHECK(Object::s_alive.load() == THREADS * OBJECTS);

		// Each thread drops its slice, takes and drops objects of its own, then ends with free blocks (freed at its end)
		std::vector<std::thread> l_threads;
		for (auto& l_slice: l_slices) {
			l_threads.emplace_back([&l_slice] {
				l_slice.clear();
				for (int i = 0; i < OBJECTS; i++) {
					auto l_object = std::allocate_shared<Object>(cam::RecyclingAllocator<Object>());
				}
			});
		}
		for (auto& l_thread: l_threads) {
			l_thread.join();
		}
		CHECK(Object::s_alive.load() == 0);

		// This thread goes on with its own blocks
		auto l_object = std::allocate_shared<Object>(cam::RecyclingAllocator<Object>());
		CHECK(Object::s_alive.load() == 1);
	}
}
