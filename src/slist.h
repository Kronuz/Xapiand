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


/*
 * Lock Free Shared Single-Linked List Using Compare and Swap.
 * This structure is based in the article:
 * "Lock-Free Linked List Using Compare-and-Swap", John D. Valois, 1995.
 */

template <typename T>
class slist {
	struct Node {
	private:
		bool contains_data;
		bool isHead;
		bool isTail;

	public:
		T data;
		std::shared_ptr<Node> next;
		std::shared_ptr<Node> previous; // Auxiliar node used only for try_delete algorithm.

		Node()
			: contains_data(false),
			  isHead(false),
			  isTail(false) { }

		template<typename Data>
		Node(Data&& _data)
			: contains_data(true),
			  isHead(false),
			  isTail(false),
			  data(std::forward<Data>(_data)) { }

		auto set_head() {
			isHead = true;
		}

		auto set_tail() {
			isTail = true;
		}

		inline auto is_normal() const {
			return contains_data && !isHead && !isTail;
		}

		inline auto is_auxiliary() {
			return !contains_data && !isHead && !isTail;
		}

		inline auto is_head() const {
			return isHead;
		}

		inline auto is_tail() const {
			return isTail;
		}
	};

	std::shared_ptr<Node> head;
	std::shared_ptr<Node> tail;
	std::atomic_size_t number_nodes;

	friend class iterator;

public:
	slist()
		: head(std::make_shared<Node>()),
		  tail(std::make_shared<Node>()),
		  number_nodes(0)
	{
		head->set_head();
		tail->set_tail();
		head->next = std::make_shared<Node>();
		head->next->next = tail;
	}

	~slist() {
		clear();
	}

	class iterator {
		std::shared_ptr<Node> target;   // Node where the iterator is visiting.
		std::shared_ptr<Node> pre_aux;  // Auxiliary node in data structure.
		std::shared_ptr<Node> pre_cell; // Normal node in data structure.

		friend class slist<T>;

	public:
		iterator() = default;

		iterator(const iterator &rhs)
			: target(rhs.target),
			  pre_aux(rhs.pre_aux),
			  pre_cell(rhs.pre_cell) { }

		const iterator& operator=(const iterator &rhs) {
			target = rhs.target;
			pre_aux = rhs.pre_aux;
			pre_cell = rhs.pre_cell;
			return *this;
		}

		auto next() {
			if (target->is_tail()) return false;

			pre_cell = std::atomic_load(&target);
			pre_aux = std::atomic_load(&target->next);

			update();

			return true;
		}

		void update() {
			if (pre_aux->next == target) return;

			auto p = pre_aux;
			auto n = std::atomic_load(&p->next);
			while (n->is_auxiliary()) {
				std::atomic_compare_exchange_strong(&pre_cell->next, &p, n);
				p = n;
				n = std::atomic_load(&p->next);
			}
			pre_aux = p;
			target = n;
		}

		iterator operator++(int) { // Postfix.
			iterator temp(*this);
			next();
			return temp;
		}

		iterator& operator++() { // Prefix
			next();
			return *this;
		}

		bool operator==(const iterator& rhs) const {
			return target->next == rhs.pre_aux;
		}

		bool operator!=(const iterator& rhs) const {
			return target->next != rhs.pre_aux;
		}

		T& operator*() const {
			return target->data;
		}

		T* operator->() const {
			return &target->data;
		}

		explicit operator bool() const {
			return !target->is_tail();
		}
	};

private:
	auto try_insert(iterator& it, std::shared_ptr<Node>& q, std::shared_ptr<Node>& a) {
		std::atomic_store(&a->next, it.target);
		std::atomic_store(&q->next, a);
		auto d = it.target;
		return std::atomic_compare_exchange_strong(&it.pre_aux->next, &d, q);
	}

	auto try_delete(iterator& it) {
		auto d = it.target;
		auto n = it.target->next;
		if (!std::atomic_compare_exchange_strong(&it.pre_aux->next, &d, n)) {
			return false;
		}

		std::atomic_store(&d->previous, it.pre_cell);
		auto p = it.pre_cell;
		while (p->previous) {
			p = std::atomic_load(&p->previous);
		}

		auto s = std::atomic_load(&p->next);
		while (n->next->is_auxiliary()) {
			n = std::atomic_load(&n->next);
		}

		bool change;
		do {
			change = std::atomic_compare_exchange_strong(&p->next, &s, n);
		} while (!change && !p->previous && !n->next->is_auxiliary());

		return true;
	}

	auto find_from(iterator& it, const T& _data) {
		while (it) {
			if (it.target->data == _data) {
				return true;
			}
			it.next();
		}

		return false;
	}

public:
	auto begin() const {
		iterator it;
		it.pre_cell = std::atomic_load(&head);
		it.pre_aux = std::atomic_load(&head->next);
		it.update();
		return it;
	}

	auto end() const {
		return iterator();
	}

	/*
	 * New node with _data is inserted at the front of the list.
	 */
	template<typename Data>
	auto push_front(Data&& _data) {
		auto q = std::make_shared<Node>(std::forward<Data>(_data));
		auto a = std::make_shared<Node>();
		auto it = begin();
		while (!try_insert(it, q, a)) {
			it.update();
		}
		++number_nodes;
	}

	/*
	 * New node with _data is inserted after the iterator.
	 * The iterator is pointing to the new node.
	 */
	template<typename Data>
	auto insert(iterator& it, Data&& _data) {
		auto q = std::make_shared<Node>(std::forward<Data>(_data));
		auto a = std::make_shared<Node>();
		while (!try_insert(it, q, a)) {
			it.update();
		}
		++number_nodes;
	}

	/*
	 * If the list has nodes, the first node is deleted.
	 */
	auto pop_front() {
		auto it = begin();
		while (it) {
			if (try_delete(it)) {
				--number_nodes;
				return;
			}
			it.update();
		}
	}

	/*
	 * Erase the iterator.
	 * The iterator is pointing to the previous node.
	 */
	auto erase(iterator& it) {
		while (it) {
			if (try_delete(it)) {
				--number_nodes;
				return;
			}
			it.update();
		}
	}

	/*
	 * Deletes the first node that contains _data.
	 * Returns if a node was deleted.
	 */
	auto remove(const T& _data) {
		auto it = begin();
		while (find_from(it, _data)) {
			if (try_delete(it)) {
				--number_nodes;
				return true;
			}
			it.update();
		}

		return false;
	}

	/*
	 * Returns if _data is on the list.
	 */
	auto find(const T& _data) {
		auto it = begin();
		return find_from(it, _data);
	}

	inline auto size() const {
		return number_nodes.load();
	}

	inline auto clear() {
		auto it = begin();
		while (it) {
			if (try_delete(it)) {
				--number_nodes;
			} else {
				it.update();
			}
		}
	}
};
