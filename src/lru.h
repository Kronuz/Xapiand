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

#pragma once

#include <cassert>                                // for assert
#include <chrono>                                 // for std::chrono
#include <functional>                             // for std::equal_to, std::hash
#include <limits>                                 // for std::numeric_limits
#include <memory>                                 // for std::allocator
#include <stdexcept>                              // for std::out_of_range
#include <type_traits>                            // for std::enable_if_t
#include <unordered_map>                          // for std::unordered_map


namespace lru {

namespace detail {


struct aging
{
	std::chrono::time_point<std::chrono::steady_clock> now;
};


struct iterate_by_age {};


template <typename BaseNode>
class lru {
protected:
	size_t _max_size;
	std::chrono::milliseconds _max_age;
	BaseNode _end;
};


template <typename LRU, typename T, typename Node, typename BaseNode, typename Mode = void>
class iterator : public aging, public std::iterator<std::bidirectional_iterator_tag, T>
{
	LRU* _lru;

	BaseNode* _node;

public:
	using mode = Mode;

	iterator(LRU* lru, BaseNode* node) : _lru(lru), _node(node) { }

	T& operator*() const noexcept {
		return static_cast<Node*>(_node)->data;
	}

	T* operator->() const noexcept {
		return &static_cast<Node*>(_node)->data;
	}

	bool operator==(const iterator& rhs) const noexcept {
		return _node == rhs._node;
	}

	bool operator!=(const iterator& rhs) const noexcept {
		return _node != rhs._node;
	}

	iterator& operator++() noexcept {
		_node = _node->template next<mode>(this);
		return *this;
	}

	iterator& operator--() noexcept {
		_node = _node->template prev<mode>(this);
		return *this;
	}

	iterator operator++(int) noexcept {
		iterator tmp(_lru, _node);
		_node = _node->template next<mode>(this);
		return tmp;
	}

	iterator operator--(int) noexcept {
		iterator tmp(_lru, _node);
		_node = _node->template prev<mode>(this);
		return tmp;
	}

	BaseNode* node() const noexcept{
		return _node;
	}

	auto relink(std::chrono::milliseconds max_age = std::chrono::milliseconds{0}) noexcept {
		_node->unlink();
		_lru->_end.link(_node, max_age == std::chrono::milliseconds{0} ? _lru->_max_age : max_age);
	}

	auto expiration() const noexcept {
		return _node->expiration();
	}
};

}  // namespace detail


struct base_node
{
	base_node* _next;
	base_node* _prev;

	base_node() :
		_next(this),
		_prev(this) { }

	bool expired(std::chrono::time_point<std::chrono::steady_clock>) const noexcept {
		return false;
	}

	bool expired() const noexcept {
		return false;
	}

	auto expiration() const noexcept {
		return std::chrono::steady_clock::time_point::max();
	}

	void unlink() noexcept {
		_prev->_next = _next;
		_next->_prev = _prev;
	}

	void link(base_node* node) noexcept {
		node->_prev = _prev;
		node->_next = this;
		_prev->_next = node;
		_prev = node;
	}

	void link(base_node* node, std::chrono::milliseconds) noexcept {
		link(node);
	}

	void renew(base_node* node) noexcept {
		node->unlink();
		link(node);
	}

	template <typename>
	base_node* next(void*) const noexcept {
		return _next;
	}

	template <typename>
	base_node* prev(void*) const noexcept {
		return _prev;
	}

	void clear() noexcept {
		_prev = _next = this;
	}
};


struct aging_base_node : public base_node
{
	std::chrono::time_point<std::chrono::steady_clock> _expiration;

	aging_base_node* _next_by_age;
	aging_base_node* _prev_by_age;

	aging_base_node() :
		_expiration{std::chrono::steady_clock::time_point::max()},
		_next_by_age{this},
		_prev_by_age{this} { }

	bool expired(std::chrono::time_point<std::chrono::steady_clock> now) const noexcept {
		return now > _expiration;
	}

	bool expired() const noexcept {
		return expired(std::chrono::steady_clock::now());
	}

	auto expiration() const noexcept {
		return _expiration;
	}

	void unlink() noexcept {
		base_node::unlink();

		_prev_by_age->_next_by_age = _next_by_age;
		_next_by_age->_prev_by_age = _prev_by_age;
	}

	void link(aging_base_node* node, std::chrono::milliseconds timeout) noexcept {
		if (timeout == std::chrono::milliseconds{0}) {
			node->_expiration = std::chrono::steady_clock::time_point::max();
		} else {
			node->_expiration = std::chrono::steady_clock::now() + timeout;
		}

		base_node::link(node, timeout);

		node->_prev_by_age = _prev_by_age;
		node->_next_by_age = this;
		_prev_by_age->_next_by_age = node;
		_prev_by_age = node;
	}

	template <typename>
	aging_base_node* next(void* ptr) const noexcept {
		auto age = static_cast<detail::aging*>(ptr);
		if (age->now == std::chrono::time_point<std::chrono::steady_clock>{}) {
			age->now = std::chrono::steady_clock::now();
		}
		auto ret = static_cast<aging_base_node*>(_next);
		while (ret->expired(age->now)) ret = static_cast<aging_base_node*>(ret->_next);
		return ret;
	}

	template <typename>
	aging_base_node* prev(void* ptr) const noexcept {
		auto age = static_cast<detail::aging*>(ptr);
		if (age->now == std::chrono::time_point<std::chrono::steady_clock>{}) {
			age->now = std::chrono::steady_clock::now();
		}
		auto ret = static_cast<aging_base_node*>(_prev);
		while (ret->expired(age->now)) ret = static_cast<aging_base_node*>(ret->_prev);
		return ret;
	}

	void clear() noexcept {
		base_node::clear();
		_prev_by_age = _next_by_age = this;
	}
};


template <>
inline aging_base_node* aging_base_node::next<detail::iterate_by_age>(void*) const noexcept {
	return _next_by_age;
}


template <>
inline aging_base_node* aging_base_node::prev<detail::iterate_by_age>(void*) const noexcept {
	return _prev_by_age;
}


template <typename Type, typename BaseNodeType>
struct node : BaseNodeType
{
	using type = Type;
	using base_node_type = BaseNodeType;

	Type data;

	template <typename... Args>
	node(Args&&... args) :
		data(std::forward<Args>(args)...) { }
};


enum class DropAction {
	leave,    // leave alone
	renew,    // renew item
	relink,   // renew item + reset timeout
	evict,    // remove item
	stop,     // stop any loops as soon as possible
};


enum class GetAction {
	leave,    // leave alone
	renew,    // renew item
	relink,   // renew item + reset timeout
	evict,    // remove item
};


template <typename Key, typename T,
	typename Hash = std::hash<Key>,
	typename KeyEqual = std::equal_to<Key>,
	typename Node = node<std::pair<const Key, T>, base_node>,
	typename Allocator = std::allocator<std::pair<const Key, Node>>>
class lru : public detail::lru<typename Node::base_node_type> {
	std::unordered_map<Key, Node, Hash, KeyEqual, Allocator> _map;

public:
	static constexpr const size_t max = std::numeric_limits<size_t>::max();

	typedef Key key_type;
	typedef T mapped_type;
	typedef Node node_type;
	typedef typename Node::base_node_type base_node_type;
	typedef std::pair<const key_type, mapped_type> value_type;

	typedef detail::iterator<lru<Key, T, Hash, KeyEqual, Node, Allocator>, std::pair<const Key, T>, Node, typename Node::base_node_type> iterator;
	typedef detail::iterator<const lru<Key, T, Hash, KeyEqual, Node, Allocator>, const std::pair<const Key, T>, const Node, const typename Node::base_node_type> const_iterator;
	typedef detail::iterator<lru<Key, T, Hash, KeyEqual, Node, Allocator>, std::pair<const Key, T>, Node, typename Node::base_node_type, detail::iterate_by_age> iterator_by_age;
	typedef detail::iterator<const lru<Key, T, Hash, KeyEqual, Node, Allocator>, const std::pair<const Key, T>, const Node, const typename Node::base_node_type, detail::iterate_by_age> const_iterator_by_age;

	friend iterator;
	friend iterator_by_age;
	friend const_iterator;
	friend const_iterator_by_age;

	using GetAction = GetAction;
	using DropAction = DropAction;

	lru(size_t max_size = 0, std::chrono::milliseconds max_age = std::chrono::milliseconds{0}) {
		this->_max_size = max_size ? max_size : max;
		this->_max_age = max_age;
		if (typeid(typename Node::base_node_type) != typeid(aging_base_node)) {
			assert(this->_max_age == std::chrono::milliseconds{0});
		}
	}

	template <typename OnDrop, typename P>
	std::pair<iterator, bool> emplace_and(const OnDrop& on_drop, P&& pair) {
		trim_and(on_drop);
		Node* node;
		bool created = true;
		auto map_it = _map.find(pair.first);
		if (map_it == _map.end()) {
			map_it = _map.emplace(pair.first, std::forward<P>(pair)).first;
			node = &map_it->second;
			this->_end.link(node, this->_max_age);
		} else {
			node = &map_it->second;
			if (node->expired()) {
				node->unlink();
				_map.erase(map_it++);
				map_it = _map.emplace_hint(map_it, pair.first, std::forward<P>(pair));
				node = &map_it->second;
				this->_end.link(node, this->_max_age);
			} else {
				this->_end.renew(node);
				created = false;
			}
		}
		return std::make_pair(iterator(this, node), created);
	}

	template <typename OnDrop, typename... Args>
	std::pair<iterator, bool> emplace_and(const OnDrop& on_drop, Args&&... args) {
		return emplace_and(on_drop, std::make_pair(std::forward<Args>(args)...));
	}

	template <typename... Args>
	std::pair<iterator, bool> emplace(Args&&... args) {
		return emplace_and([](const value_type&, bool overflowed, bool expired) {
			if (overflowed || expired) {
				return DropAction::evict;
			}
			return DropAction::stop;
		}, std::forward<Args>(args)...);
	}

	template <typename OnDrop>
	std::pair<iterator, bool> insert_and(const OnDrop& on_drop, const value_type& value) {
		return emplace_and(on_drop, value);
	}

	template <typename OnDrop>
	std::pair<iterator, bool> insert_and(const OnDrop& on_drop, value_type&& value) {
		return emplace_and(on_drop, std::move(value));
	}

	template <typename OnDrop, typename P, typename = typename std::enable_if_t<std::is_constructible<value_type, P>::value>>
	std::pair<iterator, bool> insert_and(const OnDrop& on_drop, P&& value) {
		return emplace_and(on_drop, std::forward<P>(value));
	}

	std::pair<iterator, bool> insert(const value_type& value) {
		return emplace(value);
	}

	std::pair<iterator, bool> insert(value_type&& value) {
		return emplace(std::move(value));
	}

	template <typename P, typename = typename std::enable_if_t<std::is_constructible<value_type, P>::value>>
	std::pair<iterator, bool> insert(P&& value) {
		return emplace(std::forward<P>(value));
	}

	template <typename OnGet, typename K>
	iterator find_and(const OnGet& on_get, const K& key) {
		auto map_it = _map.find(key);
		if (map_it == _map.end()) {
			return end();
		}
		auto node = &map_it->second;
		switch (on_get(node->data, _map.size() > this->_max_size, node->expired())) {
			case GetAction::leave:
				break;
			case GetAction::renew:
				this->_end.renew(node);
				break;
			case GetAction::relink:
				node->unlink();
				this->_end.link(node, this->_max_age);
				break;
			case GetAction::evict:
				node->unlink();
				_map.erase(map_it);
				return end();
		}
		return iterator(this, node);
	}

	template <typename K>
	iterator find_and_leave(const K& key) {
		return find_and([](const value_type&, bool, bool) {
			return GetAction::leave;
		}, key);
	}

	template <typename K>
	iterator find_and_renew(const K& key) {
		return find_and([](const value_type&, bool, bool) {
			return GetAction::renew;
		}, key);
	}

	template <typename K>
	iterator find_and_relink(const K& key) {
		return find_and([](const value_type&, bool, bool) {
			return GetAction::relink;
		}, key);
	}

	template <typename K>
	iterator find(const K& key) {
		return find_and([](const value_type&, bool, bool expired) {
			if (expired) {
				return GetAction::evict;
			}
			return GetAction::renew;
		}, key);
	}

	template <typename OnGet, typename K>
	const_iterator find_and(const OnGet& on_get, const K& key) const {
		auto map_it = _map.find(key);
		if (map_it == _map.cend()) {
			return cend();
		}
		auto node = &map_it->second;
		switch (on_get(node->data, _map.size() > this->_max_size, node->expired())) {
			case GetAction::leave:
			case GetAction::renew:
			case GetAction::relink:
				break;
			case GetAction::evict:
				return cend();
		}
		return const_iterator(this, node);
	}

	template <typename K>
	const_iterator find_and_leave(const K& key) const {
		return find_and([](const value_type&, bool, bool) {
			return GetAction::leave;
		}, key);
	}

	template <typename K>
	const_iterator find(const K& key) const {
		return find_and([](const value_type&, bool, bool) {
			return GetAction::leave;
		}, key);
	}

	template <typename OnGet, typename K>
	T& at_and(const OnGet& on_get, const K& key) {
		auto it = find_and(on_get, key);
		if (it == end()) {
			throw std::out_of_range("lru::at: key not found");
		}
		return it->second;
	}

	template <typename K>
	T& at(const K& key) {
		auto it = find(key);
		if (it == end()) {
			throw std::out_of_range("lru::at: key not found");
		}
		return it->second;
	}

	template <typename K>
	iterator at_and_leave(const K& key) {
		return at_and([](const value_type&, bool, bool) {
			return GetAction::leave;
		}, key);
	}

	template <typename K>
	iterator at_and_renew(const K& key) {
		return at_and([](const value_type&, bool, bool) {
			return GetAction::renew;
		}, key);
	}

	template <typename K>
	iterator at_and_relink(const K& key) {
		return at_and([](const value_type&, bool, bool) {
			return GetAction::relink;
		}, key);
	}

	template <typename OnGet, typename K>
	T& at_and(const OnGet& on_get, const K& key) const {
		auto it = find_and(on_get, key);
		if (it == end()) {
			throw std::out_of_range("lru::at: key not found");
		}
		return it->second;
	}

	template <typename K>
	const_iterator at_and_leave(const K& key) const {
		return at_and([](const value_type&, bool, bool) {
			return GetAction::leave;
		}, key);
	}

	template <typename K>
	T& at(const K& key) const {
		auto it = find(key);
		if (it == end()) {
			throw std::out_of_range("lru::at: key not found");
		}
		return it->second;
	}

	template <typename K>
	iterator get_and_leave(const K& key, T&& default_) {
		return get_and([](const value_type&, bool, bool) {
			return GetAction::leave;
		}, key, default_);
	}

	template <typename K>
	iterator get_and_renew(const K& key, T&& default_) {
		return get_and([](const value_type&, bool, bool) {
			return GetAction::renew;
		}, key, default_);
	}

	template <typename K>
	iterator get_and_relink(const K& key, T&& default_) {
		return get_and([](const value_type&, bool, bool) {
			return GetAction::relink;
		}, key, default_);
	}

	template <typename OnGet, typename OnDrop, typename K>
	T& get_and(const OnGet& on_get, const OnDrop& on_drop, const K& key, T&& default_) {
		auto it = find_and(on_get, key);
		if (it == end()) {
			it = emplace_and(on_drop, key, std::forward<T>(default_)).first;
		}
		return it->second;
	}

	template <typename K, typename... Args>
	iterator get_and_leave(const K& key, Args&&... args) {
		return get_and([](const value_type&, bool, bool) {
			return GetAction::leave;
		}, key, std::forward<Args>(args)...);
	}

	template <typename K, typename... Args>
	iterator get_and_renew(const K& key, Args&&... args) {
		return get_and([](const value_type&, bool, bool) {
			return GetAction::renew;
		}, key, std::forward<Args>(args)...);
	}

	template <typename K, typename... Args>
	iterator get_and_relink(const K& key, Args&&... args) {
		return get_and([](const value_type&, bool, bool) {
			return GetAction::relink;
		}, key, std::forward<Args>(args)...);
	}

	template <typename OnGet, typename OnDrop, typename K, typename... Args>
	T& get_and(const OnGet& on_get, const OnDrop& on_drop, const K& key, Args&&... args) {
		auto it = find_and(on_get, key);
		if (it == end()) {
			it = emplace_and(on_drop, key, T(std::forward<Args>(args)...));
		}
		return it->second;
	}

	template <typename K>
	T& get(const K& key, T&& default_) {
		return get_and([](const value_type&, bool, bool expired) {
			if (expired) {
				return GetAction::evict;
			}
			return GetAction::renew;
		}, [](const value_type&, bool overflowed, bool expired) {
			if (overflowed || expired) {
				return DropAction::evict;
			}
			return DropAction::stop;
		}, key, std::forward<T>(default_));
	}

	template <typename K, typename... Args>
	T& get(const K& key, Args&&... args) {
		return get_and([](const value_type&, bool, bool expired) {
			if (expired) {
				return GetAction::evict;
			}
			return GetAction::renew;
		}, [](const value_type&, bool overflowed, bool expired) {
			if (overflowed || expired) {
				return DropAction::evict;
			}
			return DropAction::stop;
		}, key, T(std::forward<Args>(args)...));
	}

	template <typename K>
	T& operator[] (const K& key) {
		return get(key);
	}

	size_t erase(iterator it) {
		_map.erase(it->first);
	}

	size_t erase(const_iterator it) {
		_map.erase(it->first);
	}

	size_t erase(const Key& key) {
		return erase(find(key));
	}

	template <typename OnDrop>
	void trim_and(const OnDrop& on_drop) {
		auto now = std::chrono::steady_clock::now();
		auto size = _map.size();

		if (this->_max_age != std::chrono::milliseconds{0}) {
			auto it = iterator_by_age(this, &this->_end);
			auto it_end = it++;
			while (it != it_end) {
				auto current = it++;
				auto node = current.node();
				switch (on_drop(*current, size > this->_max_size, node->expired(now))) {
					case DropAction::leave:
						break;
					case DropAction::renew:
						this->_end.renew(node);
						break;
					case DropAction::relink:
						node->unlink();
						this->_end.link(node, this->_max_age);
						break;
					case DropAction::evict:
						node->unlink();
						_map.erase(current->first);
						--size;
						break;
					case DropAction::stop:
						it = it_end;
						break;
				}
			}
		}

		if (this->_max_size != max) {
			auto it = iterator(this, &this->_end);
			auto it_end = it++;
			while (it != it_end) {
				auto current = it++;
				auto node = current.node();
				switch (on_drop(*current, size > this->_max_size, node->expired(now))) {
					case DropAction::leave:
						break;
					case DropAction::renew:
						this->_end.renew(node);
						break;
					case DropAction::relink:
						node->unlink();
						this->_end.link(node, this->_max_age);
						break;
					case DropAction::evict:
						node->unlink();
						_map.erase(current->first);
						--size;
						break;
					case DropAction::stop:
						it = it_end;
						break;
				}
			}
		}
	}

	void trim() {
		trim_and([](const value_type&, bool overflowed, bool expired) {
			if (overflowed || expired) {
				return DropAction::evict;
			}
			return DropAction::stop;
		});
	}

	const_iterator cbegin() const noexcept {
		return ++const_iterator(this, &this->_end);
	}

	const_iterator cend() const noexcept {
		return const_iterator(this, &this->_end);
	}

	iterator begin() noexcept {
		return ++iterator(this, &this->_end);
	}

	iterator end() noexcept {
		return iterator(this, &this->_end);
	}

	const_iterator begin() const noexcept {
		return cbegin();
	}

	const_iterator end() const noexcept {
		return cend();
	}

	void clear() noexcept {
		this->_end.clear();
		_map.clear();
	}

	template <typename K>
	bool exists(const K& key) const {
		return find(key) != end();
	}

	size_t size() const noexcept {
		return _map.size();
	}

	bool empty() const noexcept {
		return _map.empty();
	}

	size_t max_size() const noexcept {
		return (this->_max_size != max) ? this->_max_size : _map.max_size();
	}
};


template <typename Key, typename T,
	typename Hash = std::hash<Key>,
	typename KeyEqual = std::equal_to<Key>,
	typename Node = node<std::pair<const Key, T>, aging_base_node>,
	typename Allocator = std::allocator<std::pair<const Key, Node>>>
using aging_lru = lru<Key, T, Hash, KeyEqual, Node, Allocator>;

}  // namespace lru
