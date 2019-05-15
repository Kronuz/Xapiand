/*
 * Copyright (c) 2015-2019 Dubalu LLC
 * Copyright (c) 2017 moya-lang.org
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <cstddef>        // for std::size_t
#include <cstdlib>        // for std::malloc, std::free
#include <limits>         // for std::numeric_limits
#include <new>            // for std::bad_alloc, std::nothrow_t, std::new_handler

#include "likely.h"


namespace allocator {
	// Allocating Aligned Memory Blocks

	// The address of a block returned by malloc or realloc is always a
	// multiple of eight (or sixteen on 64-bit systems).
	constexpr std::size_t alignment = alignof(std::max_align_t);

	// allocate using ::malloc
	static inline void* malloc(std::size_t __sz) {
		if (__sz == 0) {
			__sz = 1;
		}
		void* __p;
		while ((__p = std::malloc(__sz)) == nullptr) {
			// If malloc fails and there is a new_handler, call it to try free up memory.
			std::new_handler nh = std::get_new_handler();
			if (nh != nullptr) {
				nh();
			} else {
				throw std::bad_alloc();
			}
		}
		return __p;
	}

	// deallocate using ::free
	static inline void free(void* __p) noexcept {
		std::free(__p);
	}
}

//  _____               _            _
// |_   _| __ __ _  ___| | _____  __| |
//   | || '__/ _` |/ __| |/ / _ \/ _` |
//   | || | | (_| | (__|   <  __/ (_| |
//   |_||_|  \__,_|\___|_|\_\___|\__,_|
//
namespace allocator {
	class vanilla_allocator {
	public:
		static void* allocate(std::size_t __sz) noexcept;
		static void deallocate(void *__p) noexcept;
	};

	class tracked_allocator {
	public:
		static void* allocate(std::size_t __sz) noexcept;
		static void deallocate(void *__p) noexcept;
	};

	template <typename _Tp, typename _Allocator>
	class allocator {
	public:
		// type definitions
		typedef std::size_t size_type;
		typedef std::ptrdiff_t difference_type;
		typedef _Tp* pointer;
		typedef const _Tp* const_pointer;
		typedef _Tp& reference;
		typedef const _Tp& const_reference;
		typedef _Tp value_type;

		// constructors and destructor - deleted copy
		allocator() throw() = default;
		allocator(const allocator&) throw() = delete;
		allocator operator=(const allocator&) throw() = delete;

		// rebind allocator to type _Tp1
		template<typename _Tp1>
		struct rebind {
			typedef allocator<_Tp1, _Allocator> other;
		};

		// allocate but don't initialize n elements
		pointer allocate(size_type __n, const void* __hint = static_cast<const void*>(0)) {
			if (__n > max_size() || __hint) {
				throw std::bad_alloc();
			}
			return static_cast<pointer>(_Allocator::allocate(__n * sizeof(value_type)));
		}

		// deallocate storage __p of deleted elements
		void deallocate(pointer __p, size_type) {
			_Allocator::deallocate(__p);
		}

		// initialize elements of allocated storage __p with value value
		void construct(pointer __p, const_reference __val) {
			::new(__p) value_type(__val);
		}

		// destroy elements of initialized storage __p
		void destroy(pointer __p) {
			__p->~value_type();
		}

		// return address of values
		pointer address(reference __x) const {
			return &__x;
		}

		const_pointer address(const_reference __x) const {
			return &__x;
		}

		// return maximum number of elements that can be allocated
		size_type max_size() const throw() {
			return std::numeric_limits<std::size_t>::max() / sizeof(value_type);
		}
	};

	long long total_allocated() noexcept;
	long long local_allocated() noexcept;

	template<typename _Tp, typename _Allocator>
	inline bool operator==(const allocator<_Tp, _Allocator>&, const allocator<_Tp, _Allocator>&) { return true; }
	template<typename _Tp, typename _Allocator>
	inline bool operator!=(const allocator<_Tp, _Allocator>&, const allocator<_Tp, _Allocator>&) { return false; }
}


//  __  __                                   ____             _
// |  \/  | ___ _ __ ___   ___  _ __ _   _  |  _ \ ___   ___ | |
// | |\/| |/ _ \ '_ ` _ \ / _ \| '__| | | | | |_) / _ \ / _ \| |
// | |  | |  __/ | | | | | (_) | |  | |_| | |  __/ (_) | (_) | |
// |_|  |_|\___|_| |_| |_|\___/|_|   \__, | |_|   \___/ \___/|_|
//                                   |___/
namespace allocator {
	template <typename _Tp>
	class memory_pool_allocator {
	public:
		// type definitions
		typedef std::size_t size_type;
		typedef std::ptrdiff_t difference_type;
		typedef _Tp* pointer;
		typedef const _Tp* const_pointer;
		typedef _Tp& reference;
		typedef const _Tp& const_reference;
		typedef _Tp value_type;

	private:
		union alignas(alignment) memory_pool_block {
			char value[sizeof(value_type)];
			memory_pool_block* next;
		};

		struct memory_pool_buffer {
			memory_pool_buffer* const next;
			memory_pool_block data[1];

			memory_pool_buffer(memory_pool_buffer* next) :
				next(next) {
			}

			pointer get_block(std::size_t index) {
				return reinterpret_cast<pointer>(&data[index].value);
			}
		};

		memory_pool_block* next_free_block = nullptr;
		memory_pool_buffer* next_buffer = nullptr;
		std::size_t reserved_blocks = 0;
		std::size_t used_blocks = 0;

	public:
		// constructors and destructor - deleted copy
		memory_pool_allocator() throw() = default;
		memory_pool_allocator(const memory_pool_allocator&) throw() = delete;
		memory_pool_allocator operator=(const memory_pool_allocator&) throw() = delete;

		~memory_pool_allocator() {
			while (next_buffer) {
				memory_pool_buffer* buffer = next_buffer;
				next_buffer = buffer->next;
				::allocator::free(buffer);
			}
		}

		// rebind allocator to type _Tp1
		template <class _Tp1>
		struct rebind {
			typedef memory_pool_allocator<_Tp1> other;
		};

		// allocate but don't initialize n elements
		pointer allocate(size_type __n, const void* __hint = static_cast<const void*>(0)) {
			if (__n != 1 || __hint) {
				throw std::bad_alloc();
			}

			if (next_free_block) {
				memory_pool_block* block = next_free_block;
				next_free_block = block->next;
				return reinterpret_cast<pointer>(&block->value);
			}

			if (used_blocks >= reserved_blocks) {
				reserved_blocks = (reserved_blocks + 1) * 1.5;  // Growth factor of 1.5
				auto buffer = static_cast<memory_pool_buffer*>(::allocator::malloc(sizeof(memory_pool_buffer) + sizeof(memory_pool_block) * (reserved_blocks - 1)));
				::new(buffer) memory_pool_buffer(next_buffer);
				next_buffer = buffer;
				used_blocks = 0;
			}

			return next_buffer->get_block(used_blocks++);
		}

		// deallocate storage __p of deleted elements
		void deallocate(pointer __p, size_type) {
			auto block = reinterpret_cast<memory_pool_block*>(__p);
			block->next = next_free_block;
			next_free_block = block;
		}

		// initialize elements of allocated storage __p with value value
		void construct(pointer __p, const_reference __val) {
			::new(__p) value_type(__val);
		}

		// destroy elements of initialized storage __p
		void destroy(pointer __p) {
			__p->~value_type();
		}
	};
}
