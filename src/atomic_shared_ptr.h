/*
 * Copyright (C) 2016 deipi.com LLC and contributors. All rights reserved.
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

#pragma once

#include <atomic>
#include <memory>


template <typename T>
class atomic_shared_ptr {
private:
	std::shared_ptr<T> ptr;

public:
	atomic_shared_ptr() = default;

	atomic_shared_ptr(const atomic_shared_ptr& o)
		: ptr(o.load()) { }

	atomic_shared_ptr(const std::shared_ptr<T>& ptr_)
		: ptr(ptr_) { }

	auto is_lock_free() const noexcept {
		return std::atomic_is_lock_free(&ptr);
	}

	void store(const atomic_shared_ptr& desr, std::memory_order order = std::memory_order_seq_cst) noexcept {
		std::atomic_store_explicit(&ptr, desr.load(), order);
	}

	auto load(std::memory_order order = std::memory_order_seq_cst) const noexcept {
		return std::atomic_load_explicit(&ptr, order);
	}

	auto exchange(const atomic_shared_ptr& r, std::memory_order order = std::memory_order_seq_cst) noexcept {
		return std::atomic_exchange_explicit(&ptr, r.load(), order);
	}

	auto compare_exchange_weak(atomic_shared_ptr expected, atomic_shared_ptr desired, std::memory_order success=std::memory_order_seq_cst, std::memory_order failure=std::memory_order_seq_cst) noexcept {
		return std::atomic_compare_exchange_weak_explicit(&ptr, &expected.ptr, desired.load(), success, failure);
	}

	auto compare_exchange_strong(atomic_shared_ptr expected, atomic_shared_ptr desired, std::memory_order success=std::memory_order_seq_cst, std::memory_order failure=std::memory_order_seq_cst) noexcept {
		return std::atomic_compare_exchange_strong_explicit(&ptr, &expected.ptr, desired.load(), success, failure);
	}

	template <typename... Args>
	void reset(Args&&... args) {
		std::atomic_store(&ptr, std::shared_ptr<T>(std::forward<Args>(args)...));
	}

	void swap(const atomic_shared_ptr& r) noexcept {
		r.store(std::atomic_exchange(&ptr, r.load()));
	}

	auto get() const {
		return load().get();
	}

	auto& operator=(const atomic_shared_ptr& desired) noexcept {
		std::atomic_store(&ptr, desired.load());
		return *this;
	}

	auto operator==(const atomic_shared_ptr& o) const {
		return load() == o.load();
	}

	auto operator!=(const atomic_shared_ptr& o) const {
		return load() != o.load();
	}

	explicit operator bool() const {
		return load() != nullptr;
	}
};
