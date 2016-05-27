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

#include "atomic_shared_ptr.h"

#include <array>
#include <atomic>
#include <iterator>
#include <memory>
#include <stdexcept>


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

		const std::shared_ptr<T> value;
		atomic_shared_ptr<Node> nxt;     // Next Node.
		atomic_shared_ptr<Node> prv;     // Previous Node.
		atomic_shared_ptr<Node> copy;    // New copy of Node (if any).
		atomic_shared_ptr<Info> info;    // Descriptor of update.
		std::atomic<State> state;        // Shows if Node is replaced or deleted.
		const Type type;

		friend DLList;

	public:
		Node(const std::shared_ptr<Info>& info_, State state_, Type type_)
			: info(info_),
			  state(state_),
			  type(type_) { }

		Node(const std::shared_ptr<T>& value_, const std::shared_ptr<Node>& nxt_, const std::shared_ptr<Node>& prv_,
			 const std::shared_ptr<Node>& copy_, const std::shared_ptr<Info>& info_, State state_, Type type_)
			: value(value_),
			  nxt(nxt_),
			  prv(prv_),
			  copy(copy_),
			  info(info_),
			  state(state_),
			  type(type_) { }

		Node(Node&& other) = delete;
		Node(const Node& other) = delete;
		Node& operator=(Node&& other) = delete;
		Node& operator=(const Node& other) = delete;

		bool isHead() {
			return type == Type::HEAD;
		}

		bool isTail() {
			return type == Type::TAIL;
		}

		bool isEOL() {
			return type == Type::EOL;
		}

		bool isNormal() {
			return type == Type::NORMAL;
		}

		auto next() {
			return nxt.load();
		}

		auto prev() {
			return prv.load();
		}
	};

	class Info {
		enum class Status : uint8_t {
			INPROGRESS,
			COMMITTED,
			ABORTED
		};

		std::atomic_bool rmv;         // Indicates whether node should be deleted from the list or replaced by a new copy.
		std::atomic<Status> status;

		friend DLList;

	public:
		Info(bool rmv_=false, Status status_=Status::ABORTED)
			: rmv(rmv_),
			  status(status_) { }
	};

	const std::shared_ptr<Info> dum;
	const std::shared_ptr<Node> head;
	const std::shared_ptr<Node> tail;
	std::atomic_size_t _size;

	template <typename TT, bool R>
	class _iterator : public std::iterator<std::bidirectional_iterator_tag, TT> {
		friend DLList;

		using node_move = std::shared_ptr<DLList<T>::Node> (DLList<T>::Node::*)();
		using iterator_move = void (DLList<T>::_iterator<TT, R>::*)();

		std::shared_ptr<Node> node;
		bool is_valid;

		node_move moveN;
		iterator_move moveL;
		iterator_move moveR;

		struct Data {
			std::shared_ptr<Node> node;
			std::shared_ptr<Info> nodeInfo;
			std::shared_ptr<Node> nxtNode;
			std::shared_ptr<Node> prvNode;
			bool invDel;

			Data(const std::shared_ptr<Node>& node_, const std::shared_ptr<Info>& nodeInfo_,
				 const std::shared_ptr<Node>& nxtNode_, const std::shared_ptr<Node>& prvNode_, bool invDel_)
				: node(node_),
				  nodeInfo(nodeInfo_),
				  nxtNode(nxtNode_),
				  prvNode(prvNode_),
				  invDel(invDel_) { }
		};

		auto update() {
			bool invDel = false;
			while (node->state != Node::State::ORDINARY && node->prv.load()->nxt.load() != node) {
				if (node->state == Node::State::COPIED) {
					node = node->copy.load();
				} else {
					node = (*node.*moveN)();
					invDel = true;
				}
			}
			return invDel;
		}

		auto get_update_data() {
			bool invDel = false;
			while (node->state != Node::State::ORDINARY && node->prv.load()->nxt.load() != node) {
				if (node->state == Node::State::COPIED) {
					node = node->copy.load();
				} else {
					node = node->nxt.load();
					invDel = true;
				}
			}
			return Data(node, node->info.load(), node->nxt.load(), node->prv.load(), invDel);
		}

		auto moveRight() {
			if (update()) {
				return;
			}
			if (node->isEOL()) {
				throw invalid_iterator();
			}
			node = node->nxt.load();
		}

		auto moveLeft() {
			if (update()) {
				return;
			}
			if (node->isHead()) {
				throw invalid_iterator();
			}
			auto prvNode = node->prv.load();
			if (prvNode->state != Node::State::ORDINARY && prvNode->prv.load()->nxt.load() != prvNode && prvNode->nxt.load() == node) {
				if (prvNode->state == Node::State::COPIED) {
					node = prvNode->copy.load();
				} else {
					auto prvPrvNode = prvNode->prv.load();
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
		_iterator()
			: is_valid(false) { }

		explicit _iterator(const std::shared_ptr<Node>& node_)
			: node(node_),
			  is_valid(true)
		{
			if (R) {
				moveN = &DLList<T>::Node::prev;
				moveR = &DLList<T>::_iterator<TT, R>::moveLeft;
				moveL = &DLList<T>::_iterator<TT, R>::moveRight;
			} else {
				moveN = &DLList<T>::Node::next;
				moveR = &DLList<T>::_iterator<TT, R>::moveRight;
				moveL = &DLList<T>::_iterator<TT, R>::moveLeft;
			}
		}

		_iterator(const _iterator& it)
			: node(it.node),
			  is_valid(it.is_valid),
			  moveN(it.moveN),
			  moveL(it.moveL),
			  moveR(it.moveR)  { }

		_iterator(_iterator&& it)
			: node(std::move(it.node)),
			  is_valid(std::move(it.is_valid)),
			  moveN(std::move(it.moveN)),
			  moveL(std::move(it.moveL)),
			  moveR(std::move(it.moveR)) { }

		_iterator& operator=(const _iterator& it) {
			node = it.node;
			is_valid = it.is_valid;
			return *this;
		}

		_iterator& operator=(_iterator&& it) {
			node = std::move(it.node);
			is_valid = std::move(it.is_valid);
			return *this;
		}

		~_iterator() {
			if (node) {
				update();
			}
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
			if (node->type == other.node->type) {
				if (node->isNormal()) {
					return node->value.get() == other.node->value.get();
				}
				return true;
			}
			return false;
		}

		bool operator!=(_iterator other) {
			return !operator==(other);
		}

		TT& operator*() {
			if (!is_valid) {
				throw invalid_iterator();
			}
			if (!node->isNormal()) {
				throw invalid_iterator();
			}
			return *node->value;
		}

		TT* operator->() {
			if (!is_valid) {
				throw invalid_iterator();
			}
			if (!node->isNormal()) {
				throw invalid_iterator();
			}
			return node->value.get();
		}

		explicit operator bool() {
			if (!is_valid) {
				throw invalid_iterator();
			}
			return node->isNormal();
		}
	};

	template <typename TT, typename I>
	class _reference {
		std::shared_ptr<T> value;

	public:
		explicit _reference(const std::shared_ptr<T>& value_)
			: value(value_)
		{
			if (!value) {
				throw std::out_of_range("Empty");
			}
		}

		TT& operator*() {
			return *value;
		}

		TT* operator->() {
			return value.get();
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
	auto help(std::array<std::shared_ptr<Node>, 3>& nodes, std::array<std::shared_ptr<Info>, 3>& oldInfo,
			  const std::shared_ptr<Node>& newNxt, const std::shared_ptr<Node>& newPrv, const std::shared_ptr<Info>& I) {
		bool doPtrCAS = true;
		for (int i = 0; i < 3 && doPtrCAS; ++i) {
			doPtrCAS = nodes[i]->info.compare_exchange_strong(oldInfo[i], I);
		}
		if (doPtrCAS) {
			if (I->rmv) {
				nodes[1]->state = Node::State::MARKED;
			} else {
				nodes[1]->copy.store(newPrv);
				nodes[1]->state = Node::State::COPIED;
			}
			nodes[0]->nxt.compare_exchange_strong(nodes[1], newNxt);
			nodes[2]->prv.compare_exchange_strong(nodes[1], newPrv);
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
		for (int i = 1; i < 3; ++i) {
			if (nodes[i]->info.load() != oldInfo[i]) {
				return false;
			}
		}
		return true;
	}

	template <typename... Args>
	auto insertBefore(iterator it, Args&&... args) {
		auto newNode = std::make_shared<Node>(std::make_shared<T>(std::forward<Args>(args)...), nullptr, nullptr, nullptr, dum, Node::State::ORDINARY, Node::Type::NORMAL);
		do {
			auto data = it.get_update_data();
			std::array<std::shared_ptr<Node>, 3> nodes({{ data.prvNode, data.node, data.nxtNode }});
			std::array<std::shared_ptr<Info>, 3> oldInfo({{ data.prvNode->info.load(), data.nodeInfo, data.nxtNode->info.load() }});
			if (checkInfo(nodes, oldInfo)) {
				auto nodeCopy = std::make_shared<Node>(data.node->value, data.nxtNode, newNode, nullptr, dum, Node::State::ORDINARY, data.node->type);
				newNode->prv.store(data.prvNode);
				newNode->nxt.store(nodeCopy);
				if (help(nodes, oldInfo, newNode, nodeCopy, std::make_shared<Info>(false, Info::Status::INPROGRESS))) {
					it.node = nodeCopy;
					++_size;
					return iterator(newNode);
				} else {
					newNode->nxt.reset();
					newNode->prv.reset();
				}
			}
		} while (true);
	}

	auto Delete(iterator& it) {
		do {
			auto data = it.get_update_data();
			if (data.invDel || data.node->isEOL()) {
				return iterator(data.node);
			}
			std::array<std::shared_ptr<Node>, 3> nodes({{ data.prvNode, data.node, data.nxtNode }});
			std::array<std::shared_ptr<Info>, 3> oldInfo({{ data.prvNode->info.load(), data.nodeInfo, data.nxtNode->info.load() }});
			if (checkInfo(nodes, oldInfo)) {
				if (help(nodes, oldInfo, data.nxtNode, data.prvNode, std::make_shared<Info>(true, Info::Status::INPROGRESS))) {
					--_size;
					return iterator(data.nxtNode);
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
		auto eol = std::make_shared<Node>(nullptr, tail, head, nullptr, dum, Node::State::ORDINARY, Node::Type::EOL);
		head->prv.store(std::make_shared<Node>(nullptr, head, nullptr, nullptr, dum, Node::State::ORDINARY, Node::Type::EOL));
		head->nxt.store(eol);
		tail->prv.store(eol);
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
		return insertBefore(std::forward<Iterator>(it), std::forward<V>(val));
	}

	auto front() {
		return reference(head->nxt.load()->value);
	}

	auto front() const {
		return const_reference(head->nxt.load()->value);
	}

	auto back() {
		return reference(tail->prv.load()->prv.load()->value);
	}

	auto back() const {
		return const_reference(tail->prv.load()->prv.load()->value);
	}

	auto pop_front() {
		auto it = begin();
		reference ref(it.node->value);
		Delete(it);
		return ref;
	}

	auto pop_back() {
		iterator it(tail->prv.load()->prv.load());
		reference ref(it.node->value);
		Delete(it);
		return ref;
	}

	template <typename Iterator>
	auto erase(Iterator&& it) {
		it.is_valid = false;
		return Delete(it);
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
		return iterator(head->nxt.load());
	}

	auto end() {
		return iterator(tail->prv.load());
	}

	auto begin() const {
		return const_iterator(head->nxt.load());
	}

	auto end() const {
		return const_iterator(tail->prv.load());
	}

	auto cbegin() const {
		return const_iterator(head->nxt.load());
	}

	auto cend() const {
		return const_iterator(tail->prv.load());
	}

	auto rbegin() {
		return reverse_iterator(tail->prv.load()->prv.load());
	}

	auto rend() {
		return reverse_iterator(head);
	}

	auto rbegin() const {
		return const_reverse_iterator(tail->prv.load()->prv.load());
	}

	auto rend() const {
		return const_reverse_iterator(head);
	}

	auto crbegin() const {
		return const_reverse_iterator(tail->prv.load()->prv.load());
	}

	auto crend() const {
		return const_reverse_iterator(head);
	}
};
