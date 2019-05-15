/*
 * Copyright (c) 2015-2019 Dubalu LLC
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

#include "config.h"

#include "allocator.h"

#ifdef XAPIAND_TRACKED_MEM

#include <atomic>         // for std::atomic_llong
#include <cstdlib>        // for std::malloc, std::free
#include <functional>     // for std::hash
#include <mutex>          // for std::mutex
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

	// vanilla_allocator

	inline void* vanilla_allocator::allocate(std::size_t __sz) noexcept {
		// fprintf(stderr, "{allocate %zu bytes}\n", __sz);
		return ::allocator::malloc(__sz);
	}

	inline void vanilla_allocator::deallocate(void* __p) noexcept {
		if (__p != nullptr) {
			// fprintf(stderr, "{deallocate}\n");
			::allocator::free(__p);
		}
	}


	// tracked_allocator

	static inline std::atomic_llong& _total_allocated() noexcept {
		static std::atomic_llong t_allocated;
		return t_allocated;
	}

#ifdef HAS_THREAD_LOCAL
	static inline long long& _local_allocated() noexcept {
		static thread_local long long l_allocated;
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
		allocator<std::pair<const std::thread::id, long long>, vanilla_allocator>
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

	inline void* tracked_allocator::allocate(std::size_t __sz) noexcept {
		// fprintf(stderr, "[allocate %zu bytes]\n", __sz);
		void* __p = ::allocator::malloc(__sz + alignment);
		auto& t_allocated = _total_allocated();
		auto& l_allocated = _local_allocated();
		t_allocated += __sz;
		l_allocated += __sz;
		*static_cast<std::size_t*>(__p) = __sz;
		// fprintf(stderr, "[allocated %zu bytes at %__p, %lld [%lld] are now allocated]\n", __sz, __p, l_allocated, t_allocated.load());
		__p = static_cast<char*>(__p) + alignment;
		return __p;
	}

	inline void tracked_allocator::deallocate(void* __p) noexcept {
		if (__p != nullptr) {
			__p = static_cast<char*>(__p) - alignment;
			// fprintf(stderr, "[deallocate %__p]\n", __p);
			std::size_t __sz = *static_cast<std::size_t*>(__p);
			auto& t_allocated = _total_allocated();
			auto& l_allocated = _local_allocated();
			t_allocated -= __sz;
			l_allocated -= __sz;
			// fprintf(stderr, "[deallocating %zu bytes at %__p, %lld [%lld] remain allocated]\n", __sz, __p, l_allocated, t_allocated.load());
			::allocator::free(__p);
		}
	}
}

// Operators overload for tracking

void* operator new(std::size_t __sz) noexcept(false) {
	return allocator::tracked_allocator::allocate(__sz);
}


void* operator new(std::size_t __sz, const std::nothrow_t& /*unused*/) noexcept {
	return allocator::tracked_allocator::allocate(__sz);
}


void* operator new[](std::size_t __sz) noexcept(false) {
	return allocator::tracked_allocator::allocate(__sz);
}


void* operator new[](std::size_t __sz, const std::nothrow_t& /*unused*/) noexcept {
	return allocator::tracked_allocator::allocate(__sz);
}


void operator delete(void* __p) noexcept {
	allocator::tracked_allocator::deallocate(__p);
}


void operator delete[](void* __p) noexcept {
	allocator::tracked_allocator::deallocate(__p);
}

void operator delete(void* __p, std::size_t /*unused*/) noexcept {
	allocator::tracked_allocator::deallocate(__p);
}


void operator delete[](void* __p, std::size_t /*unused*/) noexcept {
	allocator::tracked_allocator::deallocate(__p);
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

#endif  /* XAPIAND_TRACKED_MEM */
