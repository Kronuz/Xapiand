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
#include <iterator>
#include <memory>
#include <mutex>


/*
 * "A nonblocking Doubly Linked List"
 */


template<class T>
class DLList {
	class spinLock {
		std::atomic_flag _flag;

	public:
		spinLock() : _flag ATOMIC_FLAG_INIT { }

		void lock() {
			while (_flag.test_and_set(std::memory_order_acquire));
		}

		void unlock() {
			_flag.clear(std::memory_order_release);
		}
	};

	class Node {
		friend DLList;

		T val;
		std::shared_ptr<Node> next;
		std::shared_ptr<Node> prev;

	public:
		Node() { }

		template <typename... Args>
		Node(Args&&... args) : val(std::forward<Args>(args)...) { }
	};

	std::shared_ptr<Node> head;
	std::shared_ptr<Node> tail;
	std::atomic_size_t _size;
	spinLock lk;


	template <typename TT, typename I, bool R>
	class _iterator : std::iterator<std::bidirectional_iterator_tag, TT> {
		friend DLList;

		std::shared_ptr<I> p;
		bool r;

	public:
		_iterator() = default;

		explicit _iterator(std::shared_ptr<I> p_) : p(p_), r(R) { }

		_iterator& operator++() {
			p = std::atomic_load(&(r ? p->prev : p->next));
			return *this;
		}

		_iterator& operator--() {
			p = std::atomic_load(&(r ? p->next : p->prev));
			return *this;
		}

		_iterator operator++(int) {
			_iterator tmp(*this);
			++(*this);
			return tmp;
		}

		_iterator operator--(int) {
			_iterator tmp(*this);
			--(*this);
			return tmp;
		}

		bool operator==(const _iterator& other) const {
			return p == other.p;
		}

		bool operator!=(const _iterator& other) const {
			return !operator==(other);
		}

		TT& operator*() {
			return p->val;
		}

		TT* operator->(){
			return &p->val;
		}
	};

public:
	using iterator = _iterator<T, Node, false>;
	using const_iterator = _iterator<const T, const Node, false>;
	using reverse_iterator = _iterator<T, Node, true>;
	using const_reverse_iterator = _iterator<const T, const Node, true>;

	DLList() : head(std::make_shared<Node>()), tail(std::make_shared<Node>()), _size(0) {
		head->next = tail;
		tail->prev = head;
	}

	~DLList() {
		auto cur = std::atomic_load(&head->next);
		while (cur != tail) {
			cur = _erase(cur);
		}
		head->next.reset();
	}

private:
	void _insert(std::shared_ptr<Node> p, std::shared_ptr<Node>& node) {
		node->next = p;
		node->prev = p->prev;
		p->prev->next = node;
		p->prev = node;
		++_size;
	}

	auto _erase(std::shared_ptr<Node> p) {
		if (_size.load() == 0) {
			throw std::out_of_range("Empty");
		}
		auto next = p->next;
		auto prev = p->prev;
		prev->next = next;
		next->prev = prev;
		--_size;
		return next;
	}

public:
	template <typename V>
	void push_front(V&& val) {
		auto node = std::make_shared<Node>(std::forward<V>(val));
		lk.lock();
		_insert(head->next, node);
		lk.unlock();
	}

	template <typename... Args>
	void emplace_front(Args&&... args) {
		auto node = std::make_shared<Node>(std::forward<Args>(args)...);
		lk.lock();
		_insert(head->next, node);
		lk.unlock();
	}

	template <typename V>
	void push_back(V&& val) {
		auto node = std::make_shared<Node>(std::forward<V>(val));
		lk.lock();
		_insert(tail, node);
		lk.unlock();
	}

	template <typename... Args>
	void emplace_back(Args&&... args) {
		auto node = std::make_shared<Node>(std::forward<Args>(args)...);
		lk.lock();
		_insert(tail, node);
		lk.unlock();
	}

	T& front() {
		if (_size.load() == 0) {
			throw std::out_of_range("Empty");
		}
		return std::atomic_load(&head->next->val);
	}

	const T& front() const {
		if (_size.load() == 0) {
			throw std::out_of_range("Empty");
		}
		return std::atomic_load(&head->next->val);
	}

	T& back() {
		if (_size.load() == 0) {
			throw std::out_of_range("Empty");
		}
		return std::atomic_load(&tail->prev->val);
	}

	const T& back() const {
		if (_size.load() == 0) {
			throw std::out_of_range("Empty");
		}
		return std::atomic_load(&tail->prev->val);
	}

	auto pop_front() {
		std::lock_guard<spinLock> lock(lk);
		_erase(head->next);
	}

	auto pop_back() {
		std::lock_guard<spinLock> lock(lk);
		_erase(tail->prev);
	}

	auto erase(iterator it) {
		std::lock_guard<spinLock> lock(lk);
		return iterator(_erase(it.p));
	}

	auto size() const noexcept {
		return _size.load();
	}

	auto clear() noexcept {
		std::lock_guard<spinLock> lock(lk);
		auto cur = head->next;
		while (cur != tail) {
			cur = _erase(cur);
		}
	}

	auto begin() {
		return iterator(std::atomic_load(&head->next));
	}

	auto end() {
		return iterator(tail);
	}

	auto begin() const {
		return const_iterator(std::atomic_load(&head->next));
	}

	auto end() const {
		return const_iterator(tail);
	}

	auto cbegin() const {
		return const_iterator(std::atomic_load(&head->next));
	}

	auto cend() const {
		return const_iterator(tail);
	}

	auto rbegin() {
		return reverse_iterator(std::atomic_load(&tail->prev));
	}

	auto rend() {
		return reverse_iterator(head);
	}

	auto rbegin() const {
		return const_reverse_iterator(std::atomic_load(&tail->prev));
	}

	auto rend() const {
		return const_reverse_iterator(head);
	}

	auto crbegin() const {
		return const_reverse_iterator(std::atomic_load(&tail->prev));
	}

	auto crend() const {
		return const_reverse_iterator(head);
	}
};
