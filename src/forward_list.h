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
#include <memory>


/*
 * Lock Free Single-Linked List Using Compare and Swap.
 * The algorithm is based in the article:
 * "Lock-Free Linked List Using Compare-and-Swap", John D. Valois, 1995.
 */


template<typename T, typename Compare = std::equal_to<T>>
class ForwardList {
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

		template<typename... Args>
		Node(Args&&... args)
			: contains_data(true),
			  isHead(false),
			  isTail(false),
			  data(std::forward<Args>(args)...) { }

		auto set_head() noexcept {
			isHead = true;
		}

		auto set_tail() noexcept {
			isTail = true;
		}

		inline auto is_normal() const noexcept {
			return contains_data && !isHead && !isTail;
		}

		inline auto is_auxiliary() const noexcept {
			return !contains_data && !isHead && !isTail;
		}

		inline auto is_head() const noexcept {
			return isHead;
		}

		inline auto is_tail() const noexcept {
			return isTail;
		}
	};

	std::shared_ptr<Node> head;
	std::shared_ptr<Node> tail;

	std::atomic_size_t number_elements;

	Compare value_compare;

	friend class iterator;

public:
	class iterator {
		std::shared_ptr<Node> target;   // Node where the iterator is visiting.
		std::shared_ptr<Node> pre_aux;  // Auxiliary node in data structure.
		std::shared_ptr<Node> pre_cell; // Normal node in data structure.

		friend class ForwardList<T, Compare>;

	public:
		iterator() = default;

		iterator(const iterator& rhs)
			: target(rhs.target),
			  pre_aux(rhs.pre_aux),
			  pre_cell(rhs.pre_cell) { }

		const iterator& operator=(const iterator& rhs) {
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
			if (std::atomic_load(&pre_aux->next) == std::atomic_load(&target)) return;

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
			return target == rhs.target;
		}

		bool operator!=(const iterator& rhs) const {
			return target != rhs.target;
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

	ForwardList()
		: head(std::make_shared<Node>()),
		  tail(std::make_shared<Node>()),
		  number_elements(0)
	{
		head->set_head();
		tail->set_tail();
		head->next = std::make_shared<Node>();
		head->next->next = tail;
	}

	ForwardList(const Compare& _comp)
		: head(std::make_shared<Node>()),
		  tail(std::make_shared<Node>()),
		  number_elements(0),
		  value_compare(_comp)
	{
		head->set_head();
		tail->set_tail();
		head->next = std::make_shared<Node>();
		head->next->next = tail;
	}

	~ForwardList() {
		clear();
	}

private:
	auto try_insert(iterator& it, std::shared_ptr<Node>& q, std::shared_ptr<Node>& a) {
		auto d = it.target;
		std::atomic_store(&q->next, a);
		std::atomic_store(&a->next, d);
		return std::atomic_compare_exchange_strong(&it.pre_aux->next, &d, q);
	}

	auto try_delete(iterator& it) {
		auto d = it.target;
		auto n = d->next;
		if (!std::atomic_compare_exchange_strong(&it.pre_aux->next, &d, n)) {
			return false;
		}

		auto p = it.pre_cell;
		std::atomic_store(&d->previous, p);
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

	template<typename... Args>
	auto _insert_after(iterator& position, Args&&... args) {
		position.next();
		auto q = std::make_shared<Node>(std::forward<Args>(args)...);
		auto a = std::make_shared<Node>();
		while (!try_insert(position, q, a)) {
			position.update();
		}
		position.update();
		++number_elements;
	}

	auto _erase(iterator& position) {
		while (position) {
			if (try_delete(position)) {
				position.update();
				--number_elements;
				return;
			} else {
				position.update();
			}
		}
	}

	auto find_from(iterator& it, const T& val) const {
		while (it) {
			if (value_compare(it.target->data, val)) {
				return true;
			}
			it.next();
		}

		return false;
	}

public:
	/*
	 * Returns an iterator pointing to the position before the first element in the Forward List.
	 * The iterator returned shall not be dereferenced.
	 */
	auto before_begin() const noexcept {
		iterator it;
		it.pre_cell = std::make_shared<Node>();
		it.pre_aux = it.pre_cell->next = std::make_shared<Node>();
		it.target = it.pre_aux->next = std::atomic_load(&head);
		return it;
	}

	/*
	 * Returns an iterator pointing to the first element in the Forward List.
	 */
	auto begin() const noexcept {
		iterator it;
		it.pre_cell = std::atomic_load(&head);
		it.pre_aux = std::atomic_load(&head->next);
		it.update();
		return it;
	}

	/*
	 * Returns an iterator referring to the tail of the Forward List.
	 */
	auto end() const noexcept{
		iterator it;
		it.target = std::atomic_load(&tail);
		return it;
	}

	/*
	 * Returns a reference to the first element in the Forward List.
	 */
	T& front() const {
		return *begin();
	}

	/*
	 * Inserts a new node at the beginning of the Forward List.
	 * The content of data is copied (or moved) to the inserted node.
	 */
	template<typename Data>
	auto push_front(Data&& data) {
		auto position = before_begin();
		_insert_after(position, std::forward<Data>(data));
	}

	/*
	 * Inserts a new node at the end of the Forward List.
	 * The content of data is copied (or moved) to the inserted node.
	 */
	template<typename Data>
	auto push_back(Data&& data) {
		auto position = before_begin();
		auto aux = ++position;
		while (aux != end()) {
			position = aux++;
		}
		_insert_after(position, std::forward<Data>(data));
	}

	/*
	 * Inserts new node after the position.
	 * Return: an iterator that points to the new inserted element.
	 */
	template<typename Data>
	auto insert_after(iterator position, Data&& data) {
		_insert_after(position, std::forward<Data>(data));
		return position;
	}

	/*
	 * Inserts n new nodes after the position.
	 * Return: an iterator that points to the last newly inserted nodes.
	 */
	auto insert_after(iterator position, size_t n, const T& data) {
		for (size_t i = 0; i < n; ++i) {
			_insert_after(position, data);

		}
		return position;
	}

	/*
	 * Copies of the elements in the range [first,last) are inserted after the position (in the same order).
	 * Return: an iterator that points to the last newly inserted nodes.
	 */
	template<typename InputIterator>
	auto insert_after(iterator position, InputIterator first, InputIterator last) {
		while (first != last) {
			_insert_after(position, *first++);
		}
		return position;
	}

	/*
	 * Copies of elements in il are inserted after the position (in the same order).
	 * Return: an iterator that points to the last newly inserted nodes.
	 */
	auto insert_after(iterator position, std::initializer_list<T> il) {
		return insert_after(std::move(position), il.begin(), il.end());
	}

	/*
	 * Inserts a new element at the beginning of the Forward List.
	 * This new element is constructed in place using args as the arguments for its construction.
	 */
	template<typename... Args>
	auto emplace_front(Args&&... args) {
		auto position = before_begin();
		_insert_after(position, std::forward<Args>(args)...);
	}

	/*
	 * A new element after the element at position is inserted.
	 * This new element is constructed in place using args as the arguments for its construction.
	 * Return: an iterator that points to the new inserted node.
	 */
	template<typename... Args>
	auto emplace_after(iterator position, Args&&... args) {
		_insert_after(position, std::forward<Args>(args)...);
		return position;
	}

	/*
	 * Removes the first element in the Forward List.
	 */
	auto pop_front() {
		auto it = begin();
		_erase(it);
	}

	/*
	 * Removes from the Forward List a single element (the one after position).
	 * Return: an iterator pointing to the element that follows the element erased.
	 */
	auto erase_after(iterator position) {
		position.next();
		_erase(position);
		return position;
	}

	/*
	 * Removes from the Forward List a range of elements (position, last).
	 * Return: an iterator pointing to the node that follows the last element erased.
	 */
	auto erase_after(iterator position, iterator last) {
		position.next();
		while (position != last) {
			_erase(position);
		}
		return position;
	}

	/*
	 * Removes from the list container the element's position
	 * Return: an iterator pointing to the element that follows the element erased.
	 */
	auto erase(iterator position) {
		_erase(position);
		return position;
	}

	/*
	 * Finds a node with data equivalent to data.
	 * Returns an iterator to element with data equivalent to data, if data is not found it returns an iterator to end.
	 */
	auto find(const T& val) const {
		auto it = begin();
		find_from(it, val);
		return it;
	}

	/*
	 * Removes from the container all the elements that compare equal to val.
	 */
	auto remove(const T& val) {
		auto it = begin();
		while (find_from(it, val)) {
			_erase(it);
		}
	}

	/*
	 * Return Forward List size.
	 */
	auto size() const noexcept {
		return number_elements.load();
	}

	/*
	 * Removes all elements in the Forward List.
	 */
	auto clear() noexcept {
		erase_after(before_begin(), end());
	}

	/*
	 * Test whether Forward List is empty.
	 */
	auto empty() const noexcept {
		return std::atomic_load(&head->next->next)->is_tail();
	}
};
