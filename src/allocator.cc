/*
 * Copyright (C) 2015-2018 dubalu.com LLC. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "xapiand.h"

#include "allocator.h"

#ifdef XAPIAND_TRACKED_MEM

#include <atomic>         // for std::atomic_llong
#include <cassert>        // for assert
#include <cstdlib>        // for std::malloc, std::free
#include <functional>     // for std::hash
#include <mutex>          // for std::mutex
#include <new>            // for std::nothrow_t, std::new_handler
#include <thread>         // for std::this_thread
#include <unordered_map>  // for std::unordered_map
#include <utility>        // for std::make_pair


#if defined(_MSC_VER) || (defined(__GNUC__) && __GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8) || (defined(__llvm__) && !defined(_LIBCPP_VERSION))
# define HAS_THREAD_LOCAL
#elif defined(__clang__)
# if __has_feature(cxx_thread_local)
#   define HAS_THREAD_LOCAL
# endif
#endif
#if defined(__FreeBSD__) && (__FreeBSD__ < 11)  //Bug in freeBSD lower that 11v. Use of thread_local produces linking error
# undef HAS_THREAD_LOCAL
#endif


namespace allocator {
	// Allocating Aligned Memory Blocks
	// The address of a block returned by malloc or realloc is always a
	// multiple of eight (or sixteen on 64-bit systems).
	constexpr std::size_t alloc_alignment = sizeof(std::size_t) * 2;

	// allocate/deallocate using ::malloc/::free

	static inline void* allocate(std::size_t size) noexcept {
    	if (size == 0) {
        	size = 1;
    	}
    	void* p;
    	while ((p = std::malloc(size)) == 0) {
        	// If malloc fails and there is a new_handler, call it to try free up memory.
        	std::new_handler nh = std::get_new_handler();
        	if (nh) {
        		nh();
        	} else {
        		break;
        	}
    	}
    	return p;
    }

    static inline void deallocate(void* p) noexcept {
    	if (p) {
    		std::free(p);
    	}
    }


    // VanillaAllocator

	inline void* VanillaAllocator::allocate(std::size_t size) noexcept {
		// fprintf(stderr, "{allocate %zu bytes}\n", size);
		void* p = ::allocator::allocate(size);
		if (!p) return nullptr;
		return p;
	}

	inline void VanillaAllocator::deallocate(void* p) noexcept {
		if (p) {
			// fprintf(stderr, "{deallocate}\n");
			::allocator::deallocate(p);
		}
	}


	// TrackedAllocator

	static inline std::atomic_llong& _total_allocated() noexcept {
		static std::atomic_llong t_allocated;
		return t_allocated;
	}

#ifdef HAS_THREAD_LOCAL
	static inline long long& _local_allocated() noexcept {
		thread_local static long long l_allocated;
		return l_allocated;
	}

	long long local_allocated() noexcept {
		return _local_allocated();
	}

#else
	#pragma message ("Threading without thread_local support is not well supported.")

	static std::mutex tracked_mutex;
	static std::unordered_map<
		std::thread::id,
		long long,
		std::hash<std::thread::id>,
		std::equal_to<std::thread::id>,
		allocator<std::pair<const std::thread::id, long long>, VanillaAllocator>
	> tracked_sizes;

	static inline long long& __local_allocated() noexcept {
		const auto id = std::this_thread::get_id();
		auto it = tracked_sizes.find(id);
		if (it != tracked_sizes.end()) return it->second;
		auto& l_allocated = tracked_sizes.emplace(id, 0).first->second;
		return l_allocated;
	}

	static inline long long& _local_allocated() noexcept {
		std::lock_guard<std::mutex> lock(tracked_mutex);
		return __local_allocated();
	}

	long long local_allocated() noexcept {
		std::lock_guard<std::mutex> lock(tracked_mutex);
		std::size_t l_allocated = __local_allocated();
		return l_allocated;
	}
#endif

	long long total_allocated() noexcept {
		return _total_allocated();
	}

	inline void* TrackedAllocator::allocate(std::size_t size) noexcept {
		// fprintf(stderr, "[allocate %zu bytes]\n", size);
		void* p = ::allocator::allocate(size + alloc_alignment);
		if (!p) return nullptr;
		auto& t_allocated = _total_allocated();
		auto& l_allocated = _local_allocated();
		t_allocated += size;
		l_allocated += size;
		*static_cast<std::size_t*>(p) = size;
		// fprintf(stderr, "[allocated %zu bytes at %p, %lld [%lld] are now allocated]\n", size, p, l_allocated, t_allocated.load());
		p = static_cast<char*>(p) + alloc_alignment;
		return p;
	}

	inline void TrackedAllocator::deallocate(void* p) noexcept {
		if (p) {
			p = static_cast<char*>(p) - alloc_alignment;
			// fprintf(stderr, "[deallocate %p]\n", p);
			std::size_t size = *static_cast<std::size_t*>(p);
			auto& t_allocated = _total_allocated();
			auto& l_allocated = _local_allocated();
			t_allocated -= size;
			l_allocated -= size;
			// fprintf(stderr, "[deallocating %zu bytes at %p, %lld [%lld] remain allocated]\n", size, p, l_allocated, t_allocated.load());
			::allocator::deallocate(p);
		}
	}
}

// Operators overload for tracking

void* operator new(std::size_t size) noexcept(false) {
	void* p = allocator::TrackedAllocator::allocate(size);
	if (!p) {
		static const std::bad_alloc nomem;
		throw nomem;
	}
	return p;
}


void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
	return allocator::TrackedAllocator::allocate(size);
}


void* operator new[](std::size_t size) noexcept(false) {
	void* p = allocator::TrackedAllocator::allocate(size);
	if (!p) {
		static const std::bad_alloc nomem;
		throw nomem;
	}
	return p;
}


void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
	return allocator::TrackedAllocator::allocate(size);
}


void operator delete(void* p) noexcept {
	return allocator::TrackedAllocator::deallocate(p);
}


void operator delete[](void* p) noexcept {
	return allocator::TrackedAllocator::deallocate(p);
}

#else  /* XAPIAND_TRACKED_MEM */

namespace allocator {
	long long total_allocated() noexcept {
		return 0;
	}

	long long local_allocated() noexcept {
		return 0;
	}
}

#endif
