/*
 * Copyright (C) 2015 deipi.com LLC and contributors. All rights reserved.
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
#include <iostream>


template<typename T>
class slist {
	struct Node {
		T data;
		std::shared_ptr<Node> next;

		template<typename Data>
		Node(Data&& _data) : data(std::forward<Data>(_data)) { }

		Node() = default;
	};

	std::shared_ptr<Node> head;

	slist(const slist&) = delete;
	void operator=(const slist&) = delete;

public:
	slist() : head(std::make_shared<Node>()) {}

	class iterator {
		std::shared_ptr<Node> p;

		friend slist;

	public:
		iterator(std::shared_ptr<Node> _p)
			: p(std::move(_p)) { }

		iterator(const iterator& it)
			: p(it.p) { }

		explicit operator bool() const {
			return p != nullptr;
		}

		T& operator*() const {
			return p->data;
		}

		T* operator->() const {
			return &p->data;
		}

		auto operator==(const iterator& it) const {
			return p == it.p;
		}

		auto operator!=(const iterator& it) const {
			return p != it.p;
		}

		iterator& operator++() { // Prefix.
			p = std::atomic_load(&p->next);
			return *this;
		}

		iterator operator++(int) { // Postfix.
			iterator result(*this);
			++(*this);
			return result;
		}
	};

	template<typename Data>
	auto push_front(Data&& data) {
		auto n = std::make_shared<Node>(std::forward<Data>(data));
		do {
			n->next = std::atomic_load(&head->next);
		} while (!std::atomic_compare_exchange_weak(&head->next, &n->next, n));
	}

	auto pop_front() {
		auto curr = std::atomic_load(&head->next);
		while (curr && !std::atomic_compare_exchange_weak(&head->next, &curr, curr->next));
		return curr ? true : false;
	}

	auto erase(const iterator& it) {
		while (true) {
			auto prev = iterator(std::atomic_load(&head));
			auto curr = iterator(std::atomic_load(&head->next));
			while (curr && curr != it) {
				++prev;
				++curr;
			}

			if (curr) {
				if (std::atomic_compare_exchange_weak(&prev.p->next, &curr.p, curr.p->next)) {
					return iterator(prev.p->next);
				}
			} else {
				return end();
			}
		}
	}

	auto size() const {
		auto result = 0u;
		for (auto node = std::atomic_load(&head->next); node; node = std::atomic_load(&node->next))
			++result;
		return result;
	}

	auto begin() const {
		return iterator(std::atomic_load(&head->next));
	}

	auto end() const {
		return iterator(std::shared_ptr<Node>());
	}

	auto clear() {
		std::atomic_store(&head->next, std::shared_ptr<Node>());
	}
};
