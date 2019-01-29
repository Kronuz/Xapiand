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

#include <algorithm>    // for move
#include <atomic>       // for atomic_store, atomic_is_lock_free, at...
#include <memory>       // for shared_ptr


/*
 * The class template atomic_shared_ptr provides thread-safe
 * atomic pointer operations over a std::shared_ptr using
 * specializes atomic operations for std::shared_ptr.
 */


template <typename T>
class atomic_shared_ptr {
private:
	std::shared_ptr<T> ptr;

public:
	constexpr atomic_shared_ptr() noexcept = default;

	explicit constexpr atomic_shared_ptr(const std::shared_ptr<T>& ptr_) noexcept
		: ptr(ptr_) { }

	atomic_shared_ptr(atomic_shared_ptr&& o) noexcept
		: ptr(std::move(o.ptr)) { }

	atomic_shared_ptr(const atomic_shared_ptr&) = delete;

	~atomic_shared_ptr() = default;

	void operator=(std::shared_ptr<T> ptr_) noexcept {
		std::atomic_store(&ptr, ptr_);
	}

	void operator=(const atomic_shared_ptr&) = delete;

	auto is_lock_free() const noexcept {
		return std::atomic_is_lock_free(&ptr);
	}

	void store(std::shared_ptr<T> desr, std::memory_order order = std::memory_order_seq_cst) noexcept {
		std::atomic_store_explicit(&ptr, desr, order);
	}

	auto load(std::memory_order order = std::memory_order_seq_cst) const noexcept {
		return std::atomic_load_explicit(&ptr, order);
	}

	operator std::shared_ptr<T>() const noexcept {
		return load();
	}

	auto exchange(std::shared_ptr<T> r, std::memory_order order = std::memory_order_seq_cst) noexcept {
		return std::atomic_exchange_explicit(&ptr, r, order);
	}

	auto compare_exchange_weak(std::shared_ptr<T>& old_value, std::shared_ptr<T> new_value, std::memory_order order = std::memory_order_seq_cst) noexcept {
		return std::atomic_compare_exchange_weak_explicit(&ptr, &old_value, new_value, order, order);
	}

	auto compare_exchange_weak(std::shared_ptr<T>& old_value, std::shared_ptr<T> new_value, std::memory_order success_order, std::memory_order failure_order) noexcept {
		return std::atomic_compare_exchange_weak_explicit(&ptr, &old_value, new_value, success_order, failure_order);
	}

	auto compare_exchange_strong(std::shared_ptr<T>& old_value, std::shared_ptr<T> new_value, std::memory_order order = std::memory_order_seq_cst) noexcept {
		return std::atomic_compare_exchange_strong_explicit(&ptr, &old_value, new_value, order, order);
	}

	auto compare_exchange_strong(std::shared_ptr<T>& old_value, std::shared_ptr<T> new_value, std::memory_order success_order, std::memory_order failure_order) noexcept {
		return std::atomic_compare_exchange_strong_explicit(&ptr, &old_value, new_value, success_order, failure_order);
	}
};
