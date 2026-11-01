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
#include <cstddef>
#include <memory>
#include <new>

// AddressSanitizer must see every free to report a use after free or a double free: no recycling under it
#if defined(__SANITIZE_ADDRESS__)
#define CAM_RECYCLE_BLOCKS 0
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define CAM_RECYCLE_BLOCKS 0
#endif
#endif
#ifndef CAM_RECYCLE_BLOCKS
#define CAM_RECYCLE_BLOCKS 1
#endif

/**
 * CPPActressMAS.
 **/
namespace cam {

	/**
	 * True when the blocks are recycled; false under AddressSanitizer, where every block goes back to the allocator.
	 **/
	inline constexpr bool RECYCLE_BLOCKS = CAM_RECYCLE_BLOCKS != 0;

	/**
	 * Free memory blocks of one size, kept by each thread. A block freed by a thread goes to that thread, which
	 * hands it out again at its next request of that size: once warm, a thread that frees as much as it takes no longer
	 * calls the allocator. Each thread keeps MAX_FREE_BLOCKS blocks at most (about 1 MiB of them per size) and frees them
	 * when it ends.
	 **/
	template<std::size_t SIZE>
	class FreeBlocks final {
	private:
		/**
		 * A free block, linked to the next one.
		 **/
		struct Block {
			Block* m_next;
		};

		/**
		 * Free blocks of the current thread (trivially destructible: usable until the thread ends).
		 **/
		static inline thread_local Block* s_free = nullptr;

		/**
		 * Number of free blocks of the current thread.
		 **/
		static inline thread_local std::size_t s_count = 0;

		/**
		 * True once the free blocks of the current thread are freed: the blocks released later are freed at once.
		 **/
		static inline thread_local bool s_closed = false;

		/**
		 * Frees the blocks of a thread when it ends.
		 **/
		struct Owner {
			~Owner() {
				while (Block* l_block = s_free) {
					s_free = l_block->m_next;
					::operator delete(l_block);
				}
				s_count = 0;
				s_closed = true;
			}
		};

	public:
		/**
		 * Size of a block, enough for the link.
		 **/
		static constexpr std::size_t BLOCK_SIZE = SIZE < sizeof(Block) ? sizeof(Block) : SIZE;

		/**
		 * Most free blocks kept by one thread, about 1 MiB of them, between 64 and 4096; beyond them a block goes back
		 * to the allocator.
		 **/
		static constexpr std::size_t MAX_FREE_BLOCKS = std::min<std::size_t>(4096, std::max<std::size_t>(64, (std::size_t{1} << 20) / BLOCK_SIZE));

		/**
		 * Take a free block of the current thread, or a new one.
		 * @return The block, BLOCK_SIZE bytes aligned for any standard type
		 **/
		static void* acquire() {
			if constexpr (!RECYCLE_BLOCKS) {
				return ::operator new(BLOCK_SIZE);
			} else {
				Block* l_block = s_free;
				if (l_block == nullptr) {
					return ::operator new(BLOCK_SIZE);
				}
				s_free = l_block->m_next;
				s_count--;
				return l_block;
			}
		}

		/**
		 * Give a block back to the current thread, whatever thread took it.
		 * @param p_block A block from acquire
		 **/
		static void release(void* p_block) {
			if constexpr (!RECYCLE_BLOCKS) {
				::operator delete(p_block);
			} else {
				if (s_closed || s_count >= MAX_FREE_BLOCKS) {
					::operator delete(p_block);
					return;
				}
				if (s_free == nullptr) {
					// The first block kept by this thread makes sure the blocks are freed when the thread ends
					static thread_local Owner s_owner;
					(void) s_owner;
				}
				auto* l_block = static_cast<Block*>(p_block);
				l_block->m_next = s_free;
				s_free = l_block;
				s_count++;
			}
		}
	};

	/**
	 * Allocator whose single objects come from FreeBlocks: for objects made and dropped at a high rate, such as the
	 * agents and the JSON contents through std::allocate_shared. Arrays go to std::allocator. Not final: the standard containers of
	 * libstdc++ derive from their allocator.
	 **/
	template<typename T>
	class RecyclingAllocator {
	public:
		using value_type = T;

		static_assert(alignof(T) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__, "the blocks only have the default alignment");

		/**
		 * Stateless allocator.
		 **/
		RecyclingAllocator() = default;

		/**
		 * Rebound copy, used by the standard containers and std::allocate_shared.
		 **/
		template<typename U>
		RecyclingAllocator(const RecyclingAllocator<U>&) noexcept {}

		/**
		 * Memory for p_count objects.
		 * @param p_count Number of objects
		 * @return The memory
		 **/
		[[nodiscard]] T* allocate(const std::size_t p_count) {
			if (p_count != 1) {
				return std::allocator<T>().allocate(p_count);
			}
			return static_cast<T*>(FreeBlocks<sizeof(T)>::acquire());
		}

		/**
		 * Give memory back, from any thread.
		 * @param p_pointer Memory from allocate
		 * @param p_count Number of objects it was allocated for
		 **/
		void deallocate(T* p_pointer, const std::size_t p_count) noexcept {
			if (p_count != 1) {
				std::allocator<T>().deallocate(p_pointer, p_count);
				return;
			}
			FreeBlocks<sizeof(T)>::release(p_pointer);
		}

		/**
		 * Every instance can free what another allocated.
		 * @return True
		 **/
		friend bool operator==(const RecyclingAllocator&, const RecyclingAllocator&) noexcept {
			return true;
		}
	};

} // namespace cam
