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

#include <array>
#include <atomic>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <stdio.h>


/*
 * "Non-Blocking Doubly-Linked Lists with Good Amortized Complexity"
 * Based on: Non-Blocking Doubly-Linked Lists with Good Amortized Complexity by Niloufar Shafiei.
 * http://arxiv.org/pdf/1408.1935v1.pdf
 */


template<class T>
class DLList {
	class deleted_iterator : public std::logic_error {
	public:
		deleted_iterator()
			: std::logic_error("Invalid Iterator") { }
	};

	class Info;

	class Node {
		enum class State : uint8_t {
			COPIED,
			MARKED,
			ORDINARY
		};

		enum class Type : uint8_t {
			HEAD,
			TAIL,
			EOL,
			NORMAL
		};

		T value;
		std::shared_ptr<Node> nxt;   // Next Node.
		std::shared_ptr<Node> prv;   // Previous Node.
		std::shared_ptr<Node> copy;  // New copy of Node (if any).
		std::shared_ptr<Info> info;  // Descriptor of update.
		std::atomic<State> state;    // Shows if Node is replaced or deleted.
		Type type;

		friend DLList;

	public:
		Node(const std::shared_ptr<Info>& info_, State state_, Type type_)
			: info(info_),
			  state(state_),
			  type(type_) { }

		template <typename... Args>
		Node(const std::shared_ptr<Node>& nxt_, const std::shared_ptr<Node>& prv_, const std::shared_ptr<Node>& copy_,
			 const std::shared_ptr<Info>& info_, State state_, Type type_, Args&&... args)
			: value(std::forward<Args>(args)...),
			  nxt(nxt_),
			  prv(prv_),
			  copy(copy_),
			  info(info_),
			  state(state_),
			  type(type_) { }

		bool isHead() {
			return type == Type::HEAD;
		}

		bool isEOL() {
			return type == Type::EOL;
		}

		bool isNormal() {
			return type == Type::NORMAL;
		}
	};

	class Info {
		enum class Status : uint8_t {
			INPROGRESS,
			COMMITTED,
			ABORTED
		};

		std::array<std::shared_ptr<Node>, 3> nodes;    // Nodes to be flagged.
		std::array<std::shared_ptr<Info>, 3> oldInfo;  // Expected values of CASs that flag.
		std::shared_ptr<Node> newNxt;                  // Set nodes[0]->nxt to this.
		std::shared_ptr<Node> newPrv;                  // Set nodes[2]->prv to this.
		std::atomic_bool rmv;                          // Indicates whether node should be deleted from the list or replaced by a new copy.
		std::atomic<Status> status;

		friend DLList;

	public:
		Info()
			: rmv(false),
			  status(Status::ABORTED) { }

		Info(const std::array<std::shared_ptr<Node>, 3>& nodes_, const std::array<std::shared_ptr<Info>, 3>& oldInfo_,
			 const std::shared_ptr<Node>& newNxt_, const std::shared_ptr<Node>& newPrv_, bool rmv_, Status status_)
			: nodes(nodes_),
			  oldInfo(oldInfo_),
			  newNxt(newNxt_),
			  newPrv(newPrv_),
			  rmv(rmv_),
			  status(status_) { }
	};

	std::shared_ptr<Info> dum;
	std::shared_ptr<Node> head;
	std::shared_ptr<Node> tail;
	std::atomic_size_t _size;

	template <typename TT, bool R>
	class _iterator : public std::iterator<std::bidirectional_iterator_tag, TT> {
		friend DLList;

		using iterator_move = void (DLList<T>::_iterator<TT, R>::*)();

		std::shared_ptr<Node> node;
		bool is_valid;

		iterator_move moveL;
		iterator_move moveR;

		auto update() {
			bool invDel = false;
			while (node->state != Node::State::ORDINARY && std::atomic_load(&std::atomic_load(&node->prv)->nxt) != node) {
				if (node->state == Node::State::COPIED) {
					node = std::atomic_load(&node->copy);
				}
				if (node->state == Node::State::MARKED) {
					node = std::atomic_load(&node->nxt);
					invDel = true;
				}
			}
			return invDel;
		}

		auto moveRight() {
			if (update()) {
				return;
			}
			if (node->isEOL()) {
				throw invalid_iterator();
			}
			node = std::atomic_load(&node->nxt);
		}

		auto moveLeft() {
			if (update()) {
				return;
			}
			if (node->isHead()) {
				throw invalid_iterator();
			}
			auto prvNode = std::atomic_load(&node->prv);
			if (prvNode->state != Node::State::ORDINARY && std::atomic_load(&std::atomic_load(&prvNode->prv)->nxt) != prvNode && std::atomic_load(&prvNode->nxt) == node) {
				if (prvNode->state == Node::State::COPIED) {
					node = std::atomic_load(&prvNode->copy);
				} else {
					auto prvPrvNode = std::atomic_load(&prvNode->prv);
					if (prvPrvNode->isHead()) {
						return;
					}
					node = prvPrvNode;
				}
			} else {
				node = prvNode;
			}
		}

	public:
		_iterator() = default;

		explicit _iterator(const std::shared_ptr<Node>& node_)
			: node(node_),
			  is_valid(true)
		{
			if (R) {
				moveR = &DLList<T>::_iterator<TT, R>::moveLeft;
				moveL = &DLList<T>::_iterator<TT, R>::moveRight;
			} else {
				moveR = &DLList<T>::_iterator<TT, R>::moveRight;
				moveL = &DLList<T>::_iterator<TT, R>::moveLeft;
			}
		}

		~_iterator() {
			update();
		}

		_iterator& operator++() {
			if (!is_valid) {
				throw invalid_iterator();
			}
			(this->*moveR)();
			return *this;
		}

		_iterator& operator--() {
			if (!is_valid) {
				throw invalid_iterator();
			}
			(this->*moveL)();
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

		bool operator==(_iterator other) {
			if (!is_valid || !other.is_valid) {
				throw invalid_iterator();
			}
			update();
			other.update();
			return node == other.node;
		}

		bool operator!=(_iterator other) {
			return !operator==(other);
		}

		TT& operator*() {
			if (!is_valid || !node->isNormal()) {
				throw invalid_iterator();
			}
			return node->value;
		}

		TT* operator->() {
			return &operator*();
		}

		explicit operator bool() {
			if (!is_valid) {
				throw invalid_iterator();
			}
			update();
			return node->isNormal();
		}
	};

	template <typename TT, typename I>
	class _reference {
		std::shared_ptr<I> p;

		friend DLList;

	public:
		explicit _reference(std::shared_ptr<I> p_)
			: p(p_)
		{
			if (!p->isNormal()) {
				throw std::out_of_range("Empty");
			}
		}

		TT& operator*() {
			return p->value;
		}

		TT* operator->() {
			return &p->value;
		}
	};

public:
	class invalid_iterator : public std::logic_error {
	public:
		invalid_iterator()
			: std::logic_error("Invalid Iterator") { }
	};

	using iterator = _iterator<T, false>;
	using const_iterator = _iterator<const T, false>;
	using reverse_iterator = _iterator<T, true>;
	using const_reverse_iterator = _iterator<const T, true>;

	using reference = _reference<T, Node>;
	using const_reference = _reference<const T, Node>;

private:

	auto help(const std::shared_ptr<Info>& I) {
		bool doPtrCAS = true;
		for (int i = 0; i < 3 && doPtrCAS; ++i) {
			doPtrCAS = std::atomic_compare_exchange_strong(&std::atomic_load(&I->nodes[i])->info, &I->oldInfo[i], I);
		}
		if (doPtrCAS) {
			if (I->rmv) {
				std::atomic_load(&I->nodes[1])->state = Node::State::MARKED;
			} else {
				std::atomic_store(&std::atomic_load(&I->nodes[1])->copy, I->newPrv);
				std::atomic_load(&I->nodes[1])->state = Node::State::COPIED;
			}
			while (!std::atomic_compare_exchange_strong(&std::atomic_load(&I->nodes[0])->nxt, &I->nodes[1], I->newNxt));
			while (!std::atomic_compare_exchange_strong(&std::atomic_load(&I->nodes[2])->prv, &I->nodes[1], I->newPrv));
			I->status = Info::Status::COMMITTED;
		} else if (I->status == Info::Status::INPROGRESS) {
			I->status = Info::Status::ABORTED;
		}
		return I->status == Info::Status::COMMITTED;
	}

	auto checkInfo(const std::array<std::shared_ptr<Node>, 3>& nodes, const std::array<std::shared_ptr<Info>, 3>& oldInfo) {
		for (const auto& info : oldInfo) {
			if (info->status == Info::Status::INPROGRESS) {
				return false;
			}
		}
		for (const auto& node : nodes) {
			if (node->state != Node::State::ORDINARY) {
				return false;
			}
		}
		for (int i = 0; i < 3; ++i) {
			if (std::atomic_load(&nodes[i]->info) != oldInfo[i]) {
				return false;
			}
		}
		return true;
	}

	template <typename... Args>
	auto insertBefore(iterator it, Args&&... args) {
		auto newNode = std::make_shared<Node>(nullptr, nullptr, nullptr, dum, Node::State::ORDINARY, Node::Type::NORMAL, std::forward<Args>(args)...);
		do {
			it.update();
			std::array<std::shared_ptr<Node>, 3> nodes({{ std::atomic_load(&it.node->prv), it.node, std::atomic_load(&it.node->nxt) }});
			std::array<std::shared_ptr<Info>, 3> oldInfo({{ std::atomic_load(&nodes[0]->info), std::atomic_load(&it.node->info), std::atomic_load(&nodes[2]->info) }});
			if (checkInfo(nodes, oldInfo)) {
				newNode->prv = nodes[0];
				auto nodeCopy = std::make_shared<Node>(nodes[2], newNode, nullptr, dum, Node::State::ORDINARY, it.node->type, it.node->value);
				newNode->nxt = nodeCopy;
				if (help(std::make_shared<Info>(nodes, oldInfo, newNode, nodeCopy, false, Info::Status::INPROGRESS))) {
					it.node = nodeCopy;
					++_size;
					return iterator(newNode);
				} else {
					newNode->nxt.reset();
				}
			}
		} while (true);
	}

	auto Delete(iterator it) {
		do {
			if (it.update()) {
				throw deleted_iterator();
			}
			std::array<std::shared_ptr<Node>, 3> nodes({{ std::atomic_load(&it.node->prv), it.node, std::atomic_load(&it.node->nxt) }});
			std::array<std::shared_ptr<Info>, 3> oldInfo({{ std::atomic_load(&nodes[0]->info), std::atomic_load(&it.node->info), std::atomic_load(&nodes[2]->info) }});
			if (checkInfo(nodes, oldInfo)) {
				if (!it.node->isNormal()) {
					return iterator(it.node);
				}
				if (help(std::make_shared<Info>(nodes, oldInfo, nodes[2], nodes[0], true, Info::Status::INPROGRESS))) {
					--_size;
					return iterator(nodes[2]);
				}
			}
		} while (true);
	}

public:
	DLList()
		: dum(std::make_shared<Info>()),
		  head(std::make_shared<Node>(dum, Node::State::ORDINARY, Node::Type::HEAD)),
		  tail(std::make_shared<Node>(dum, Node::State::ORDINARY, Node::Type::TAIL)),
		  _size(0)
	{
		auto eol = std::make_shared<Node>(tail, head, nullptr, dum, Node::State::ORDINARY, Node::Type::EOL);
		head->prv = std::make_shared<Node>(head, nullptr, nullptr, dum, Node::State::ORDINARY, Node::Type::EOL);
		head->nxt = eol;
		tail->prv = eol;
	}

	~DLList() {
		clear();
		head->nxt.reset();
		head->prv.reset();
		tail->prv.reset();
	}

	template <typename V>
	auto push_front(V&& val) {
		insertBefore(begin(), std::forward<V>(val));
	}

	template <typename... Args>
	auto emplace_front(Args&&... args) {
		insertBefore(begin(), std::forward<Args>(args)...);
	}

	template <typename V>
	auto push_back(V&& val) {
		insertBefore(end(), std::forward<V>(val));
	}

	template <typename... Args>
	auto emplace_back(Args&&... args) {
		insertBefore(end(), std::forward<Args>(args)...);
	}

	template <typename Iterator, typename V>
	auto insert(Iterator&& it, V&& val) {
		return insertBefore(it, std::forward<V>(val));
	}

	auto front() {
		return reference(std::atomic_load(&head->nxt));
	}

	auto front() const {
		return const_reference(std::atomic_load(&head->nxt));
	}

	auto back() {
		return reference(std::atomic_load(&std::atomic_load(&tail->prv)->prv));
	}

	auto back() const {
		return const_reference(std::atomic_load(&std::atomic_load(&tail->prv)->prv));
	}

	auto pop_front() {
		do {
			try {
				auto it = begin();
				reference ref(it.node);
				Delete(it);
				return ref;
			} catch (const deleted_iterator&) { }
		} while (true);
	}

	auto pop_back() {
		do {
			try {
				iterator it(std::atomic_load(&std::atomic_load(&tail->prv)->prv));
				reference ref(it.node);
				Delete(it);
				return ref;
			} catch (const deleted_iterator&) { }
		} while (true);
	}

	template <typename Iterator>
	auto erase(Iterator&& it) {
		it.is_valid = false;
		try {
			return Delete(std::forward<Iterator>(it));
		} catch (const deleted_iterator&) {
			return iterator(it.node);
		}
	}

	auto size() const noexcept {
		return _size.load();
	}

	auto clear() noexcept {
		try {
			do {
				pop_front();
			} while (true);
		} catch (const std::out_of_range&) { }
	}

	auto begin() {
		return iterator(std::atomic_load(&head->nxt));
	}

	auto end() {
		return iterator(std::atomic_load(&tail->prv));
	}

	auto begin() const {
		return const_iterator(std::atomic_load(&head->nxt));
	}

	auto end() const {
		return const_iterator(std::atomic_load(&tail->prv));
	}

	auto cbegin() const {
		return const_iterator(std::atomic_load(&head->nxt));
	}

	auto cend() const {
		return const_iterator(std::atomic_load(&tail->prv));
	}

	auto rbegin() {
		return reverse_iterator(std::atomic_load(&std::atomic_load(&tail->prv)->prv));
	}

	auto rend() {
		return reverse_iterator(head);
	}

	auto rbegin() const {
		return const_reverse_iterator(std::atomic_load(&std::atomic_load(&tail->prv)->prv));
	}

	auto rend() const {
		return const_reverse_iterator(head);
	}

	auto crbegin() const {
		return const_reverse_iterator(std::atomic_load(&std::atomic_load(&tail->prv)->prv));
	}

	auto crend() const {
		return const_reverse_iterator(head);
	}
};
