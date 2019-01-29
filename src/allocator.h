/*
 * Copyright (c) 2015-2018 Dubalu LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

#include <cstddef>        // for std::size_t
#include <limits>         // for std::numeric_limits
#include <new>            // for std::bad_alloc


namespace allocator {
	long long total_allocated() noexcept;
	long long local_allocated() noexcept;

	class VanillaAllocator {
	public:
		static void* allocate(std::size_t __sz) noexcept;
		static void deallocate(void *__p) noexcept;
	};

	class TrackedAllocator {
	public:
		static void* allocate(std::size_t __sz) noexcept;
		static void deallocate(void *__p) noexcept;
	};

	template <typename _Tp, typename _Allocator>
	class allocator {
	public:
		// type definitions
		typedef size_t     size_type;
		typedef ptrdiff_t  difference_type;
		typedef _Tp*       pointer;
		typedef const _Tp* const_pointer;
		typedef _Tp&       reference;
		typedef const _Tp& const_reference;
		typedef _Tp        value_type;

		// rebind allocator to type _Tp1
		template<typename _Tp1>
		struct rebind { typedef allocator<_Tp1, _Allocator> other; };

		// constructors and destructor - nothing to do because the allocator has no state
		allocator() throw() { }
		allocator(const allocator&) throw() { }
		template<typename _Tp1>
		allocator(const allocator<_Tp1, _Allocator>&) throw() { }
		~allocator() throw() { }

		// return address of values
		pointer address(reference __x) const { return &__x; }
		const_pointer address(const_reference __x) const { return &__x; }

		// allocate but don't initialize num elements of type _Tp
		pointer allocate(size_type __n, const void* = static_cast<const void*>(0))
		{
			if (__n > max_size()) {
				static const std::bad_alloc nomem;
				throw nomem;
			}
			return static_cast<_Tp*>(_Allocator::allocate(__n * sizeof(_Tp)));
		}

		// deallocate storage __p of deleted elements
		void deallocate(pointer __p, size_type) {
			_Allocator::deallocate(__p);
		}

		// return maximum number of elements that can be allocated
		size_type max_size() const throw() {
			return std::numeric_limits<std::size_t>::max() / sizeof(_Tp);
		}

		// initialize elements of allocated storage __p with value value
		void construct(pointer __p, const _Tp& __val) {
			::new(__p) _Tp(__val);
		}

		// destroy elements of initialized storage __p
		void destroy(pointer __p) {
			__p->~_Tp();
		}
	};

	template<typename _Tp, typename _Allocator>
	inline bool operator==(const allocator<_Tp, _Allocator>&, const allocator<_Tp, _Allocator>&) { return true; }
	template<typename _Tp, typename _Allocator>
	inline bool operator!=(const allocator<_Tp, _Allocator>&, const allocator<_Tp, _Allocator>&) { return false; }
}
