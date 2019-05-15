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
#include <deque>          // for std::deque
#include <limits>         // for std::numeric_limits
#include <new>            // for std::bad_alloc, std::nothrow_t, std::new_handler


//  _____               _            _
// |_   _| __ __ _  ___| | _____  __| |
//   | || '__/ _` |/ __| |/ / _ \/ _` |
//   | || | | (_| | (__|   <  __/ (_| |
//   |_||_|  \__,_|\___|_|\_\___|\__,_|
//
namespace allocators {
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
		allocator() = default;
		template<typename _Tp1>
		allocator(const allocator<_Tp1, _Allocator>&) throw() { }

		// rebind allocator to type _Tp1
		template <typename _Tp1>
		struct rebind {
			using other = allocator<_Tp1, _Allocator>;
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

	template <typename _Tp, typename _Allocator>
	inline bool operator==(const allocator<_Tp, _Allocator>&, const allocator<_Tp, _Allocator>&) { return true; }
	template <typename _Tp, typename _Allocator>
	inline bool operator!=(const allocator<_Tp, _Allocator>&, const allocator<_Tp, _Allocator>&) { return false; }
}


//  __  __                                   ____             _
// |  \/  | ___ _ __ ___   ___  _ __ _   _  |  _ \ ___   ___ | |
// | |\/| |/ _ \ '_ ` _ \ / _ \| '__| | | | | |_) / _ \ / _ \| |
// | |  | |  __/ | | | | | (_) | |  | |_| | |  __/ (_) | (_) | |
// |_|  |_|\___|_| |_| |_|\___/|_|   \__, | |_|   \___/ \___/|_|
//                                   |___/
namespace allocators {
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
		union alignas(alignof(std::max_align_t)) memory_pool_block {
			char value[sizeof(value_type)];
			memory_pool_block* next;
		};

		std::deque<memory_pool_block> buffer;

		memory_pool_block* next_free_block = nullptr;

	public:
		// constructors and destructor - deleted copy
		memory_pool_allocator() = default;
		template<typename _Tp1>
		memory_pool_allocator(const memory_pool_allocator<_Tp1>&) throw() { }

		// rebind allocator to type _Tp1
		template <typename _Tp1>
		struct rebind {
			using other = memory_pool_allocator<_Tp1>;
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

			buffer.emplace_back();
			return reinterpret_cast<pointer>(&buffer.back().value);
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
