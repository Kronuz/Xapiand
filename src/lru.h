/*
 * Copyright (c) 2015-2018 Dubalu LLC
 * Copyright (c) 2014 lamerman
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

#include <cstddef>
#include <cstdint>
#include <list>
#include <stdexcept>
#include <unordered_map>

#include "cassert.h"    // for ASSERT


namespace lru {


enum class DropAction : uint8_t {
	leave,
	renew,
	evict,
	stop,
};


enum class GetAction : uint8_t {
	leave,
	renew,
};


template <typename Key, typename T>
class LRU {
protected:
	using list_t = std::list<std::pair<Key, T>>;
	using map_t = std::unordered_map<Key, typename list_t::iterator>;

	list_t _items_list;
	map_t _items_map;
	size_t _max_size;

public:
	using iterator = typename list_t::iterator;
	using const_iterator = typename list_t::const_iterator;

	explicit LRU(size_t max_size=SIZE_MAX)
		: _max_size(max_size)
	{
		ASSERT(_max_size != 0);
	}

	iterator begin() noexcept {
		return _items_list.begin();
	}

	const_iterator begin() const noexcept {
		return _items_list.begin();
	}

	const_iterator cbegin() const noexcept {
		return _items_list.cbegin();
	}

	iterator end() noexcept {
		return _items_list.end();
	}

	const_iterator end() const noexcept {
		return _items_list.end();
	}

	const_iterator cend() const noexcept {
		return _items_list.cend();
	}

	template <typename K>
	iterator find(const K& key) {
		auto m_it = _items_map.find(key);
		if (m_it == _items_map.end()) {
			return _items_list.end();
		}
		auto it = m_it->second;
		_items_list.splice(_items_list.begin(), _items_list, it);
		return it;
	}

	template <typename K>
	const_iterator find(const K& key) const {
		auto m_it = _items_map.find(key);
		if (m_it == _items_map.cend()) {
			return _items_list.cend();
		}
		return m_it->second;
	}

	void erase(const_iterator it) {
		_items_map.erase(it->first);
		_items_list.erase(it->second);
	}

	template <typename K>
	size_t erase(const K& key) {
		auto m_it = _items_map.find(key);
		if (m_it != _items_map.end()) {
			_items_list.erase(m_it->second);
			_items_map.erase(m_it);
			return 1;
		}
		return 0;
	}

	void trim() {
		if (_max_size != SIZE_MAX) {
			auto size = _items_map.size() + 1;
			auto last = _items_list.rbegin();
			for (size_t i = _items_map.size(); i != 0 && last != _items_list.rend() && size > _max_size; --i) {
				auto it = (++last).base();
				--size;
				_items_map.erase(it->first);
				_items_list.erase(it);
			}
		}
	}

	template <typename P>
	std::pair<iterator, bool> insert(P&& p) {
		erase(p.first);
		trim();
		_items_list.push_front(std::forward<P>(p));
		auto it = _items_list.begin();
		bool created = _items_map.emplace(it->first, it).second;
		return std::make_pair(it, created);
	}

	template <typename P>
	std::pair<iterator, bool> insert_back(P&& p) {
		erase(p.first);
		trim();
		_items_list.push_back(std::forward<P>(p));
		auto last = _items_list.rbegin();
		auto it = (++last).base();
		bool created = _items_map.emplace(it->first, it).second;
		return std::make_pair(it, created);
	}

	template <typename... Args>
	std::pair<iterator, bool> emplace(Args&&... args) {
		return insert(std::make_pair(std::forward<Args>(args)...));
	}

	template <typename... Args>
	std::pair<iterator, bool> emplace_back(Args&&... args) {
		return insert_back(std::make_pair(std::forward<Args>(args)...));
	}

	T& at(iterator it) {
		_items_list.splice(_items_list.begin(), _items_list, it);
		return it->second;
	}

	const T& at(const_iterator it) const {
		return it->second;
	}

	template <typename K>
	T& at(const K& key) {
		auto m_it = _items_map.find(key);
		if (m_it == _items_map.end()) {
			throw std::out_of_range("There is no such key in cache");
		}
		return at(m_it->second);
	}

	template <typename K>
	const T& at(const K& key) const {
		auto m_it = _items_map.find(key);
		if (m_it == _items_map.end()) {
			throw std::out_of_range("There is no such key in cache");
		}
		return at(m_it->second);
	}

	template <typename K>
	T& get(const K& key, T& default_) {
		auto m_it = _items_map.find(key);
		if (m_it == _items_map.end()) {
			return emplace(key, default_).first->second;
		}
		return at(m_it->second);
	}

	template <typename K, typename... Args>
	T& get(const K& key, Args&&... args) {
		auto m_it = _items_map.find(key);
		if (m_it == _items_map.end()) {
			return emplace(key, T(std::forward<Args>(args)...)).first->second;
		}
		return at(m_it->second);
	}

	template <typename K>
	T& get_back(const K& key, T& default_) {
		auto m_it = _items_map.find(key);
		if (m_it == _items_map.end()) {
			return emplace_back(key, default_).first->second;
		}
		return at(m_it->second);
	}

	template <typename K, typename... Args>
	T& get_back(const K& key, Args&&... args) {
		auto m_it = _items_map.find(key);
		if (m_it == _items_map.end()) {
			return emplace_back(key, T(std::forward<Args>(args)...)).first->second;
		}
		return at(m_it->second);
	}

	template <typename K>
	T& operator[] (const K& key) {
		return get(key);
	}

	template <typename K>
	bool exists(const K& key) const {
		return _items_map.find(key) != _items_map.end();
	}

	void clear() noexcept {
		_items_map.clear();
		_items_list.clear();
	}

	bool empty() const noexcept {
		return _items_map.empty();
	}

	size_t size() const noexcept {
		return _items_map.size();
	}

	size_t max_size() const noexcept {
		return (_max_size == SIZE_MAX) ? _items_map.max_size() : _max_size;
	}

	template <typename OnDrop>
	void trim(const OnDrop& on_drop, size_t size) {
		if (_max_size != SIZE_MAX) {
			auto last = --_items_list.end();
			for (size_t i = _items_list.size(); i != 0; --i) {
				auto it = last--;
				switch (on_drop(it->second, size, _max_size)) {
					case DropAction::evict:
						--size;
						_items_map.erase(it->first);
						_items_list.erase(it);
						break;
					case DropAction::renew:
						_items_list.splice(_items_list.begin(), _items_list, it);
						break;
					case DropAction::leave:
						break;
					case DropAction::stop:
						return;
				}
			}
		}
	}

	template <typename OnDrop>
	void trim(const OnDrop& on_drop) {
		trim(on_drop, _items_map.size());
	}

	template <typename OnDrop, typename P>
	std::pair<iterator, bool> insert_and(const OnDrop& on_drop, P&& p) {
		erase(p.first);
		trim(on_drop, _items_map.size() + 1);
		_items_list.push_front(std::forward<P>(p));
		auto it = _items_list.begin();
		bool created = _items_map.emplace(it->first, it).second;
		return std::make_pair(it, created);
	}

	template <typename OnDrop, typename P>
	std::pair<iterator, bool> insert_back_and(const OnDrop& on_drop, P&& p) {
		erase(p.first);
		trim(on_drop, _items_map.size() + 1);
		_items_list.push_back(std::forward<P>(p));
		auto last = _items_list.rbegin();
		auto it = (++last).base();
		bool created = _items_map.emplace(it->first, it).second;
		return std::make_pair(it, created);
	}

	template <typename OnDrop, typename... Args>
	std::pair<iterator, bool> emplace_and(OnDrop&& on_drop, Args&&... args) {
		return insert_and(std::forward<OnDrop>(on_drop), std::make_pair(std::forward<Args>(args)...));
	}

	template <typename OnDrop, typename... Args>
	std::pair<iterator, bool> emplace_back_and(OnDrop&& on_drop, Args&&... args) {
		return insert_back_and(std::forward<OnDrop>(on_drop), std::make_pair(std::forward<Args>(args)...));
	}

	template <typename OnGet, typename K>
	iterator find_and(const OnGet& on_get, const K& key) {
		auto m_it = _items_map.find(key);
		if (m_it == _items_map.end()) {
			return _items_list.end();
		}
		auto it = m_it->second;
		T& ref = it->second;
		switch (on_get(ref)) {
			case GetAction::leave:
				break;
			case GetAction::renew:
				_items_list.splice(_items_list.begin(), _items_list, it);
				break;
		}
		return it;
	}

	template <typename K>
	iterator find_and_leave(const K& key) {
		auto m_it = _items_map.find(key);
		if (m_it == _items_map.end()) {
			return _items_list.end();
		}
		auto it = m_it->second;
		return it;
	}

	template <typename K>
	const_iterator find_and_leave(const K& key) const {
		auto m_it = _items_map.find(key);
		if (m_it == _items_map.end()) {
			return _items_list.end();
		}
		auto it = m_it->second;
		return it;
	}

	template <typename K>
	iterator find_and_renew(const K& key) {
		auto m_it = _items_map.find(key);
		if (m_it == _items_map.end()) {
			return _items_list.end();
		}
		auto it = m_it->second;
		_items_list.splice(_items_list.begin(), _items_list, it);
		return it;
	}

	template <typename OnGet>
	T& at_and(const OnGet& on_get, iterator it) {
		T& ref = it->second;
		switch (on_get(ref)) {
			case GetAction::leave:
				break;
			case GetAction::renew:
				_items_list.splice(_items_list.begin(), _items_list, it);
				break;
		}
		return ref;
	}

	T& at_and_leave(iterator it) {
		T& ref = it->second;
		return ref;
	}

	const T& at_and_leave(iterator it) const {
		T& ref = it->second;
		return ref;
	}

	T& at_and_renew(iterator it) {
		T& ref = it->second;
		_items_list.splice(_items_list.begin(), _items_list, it);
		return ref;
	}

	template <typename K, typename OnGet>
	T& at_and(const OnGet& on_get, const K& key) {
		auto m_it = _items_map.find(key);
		if (m_it == _items_map.end()) {
			throw std::out_of_range("There is no such key in cache");
		}
		return at_and(on_get, m_it->second);
	}

	template <typename K>
	T& at_and_leave(const K& key) {
		auto m_it = _items_map.find(key);
		if (m_it == _items_map.end()) {
			throw std::out_of_range("There is no such key in cache");
		}
		return at_and_leave(m_it->second);
	}

	template <typename K>
	const T& at_and_leave(const K& key) const {
		auto m_it = _items_map.find(key);
		if (m_it == _items_map.end()) {
			throw std::out_of_range("There is no such key in cache");
		}
		return at_and_leave(m_it->second);
	}

	template <typename K>
	T& at_and_renew(const K& key) {
		auto m_it = _items_map.find(key);
		if (m_it == _items_map.end()) {
			throw std::out_of_range("There is no such key in cache");
		}
		return at_and_renew(m_it->second);
	}

	template <typename OnGet, typename OnDrop, typename K>
	T& get_and(const OnGet& on_get, const OnDrop& on_drop, const K& key, T& default_) {
		auto m_it = _items_map.find(key);
		if (m_it == _items_map.end()) {
			T& ref = emplace_and(on_drop, key, default_).first->second;
			switch (on_get(ref)) {
				case GetAction::leave:
					break;
				case GetAction::renew:
					break;
			}
			return ref;
		}
		return at_and(on_get, m_it->second);
	}

	template <typename OnGet, typename OnDrop, typename K, typename... Args>
	T& get_and(const OnGet& on_get, const OnDrop& on_drop, const K& key, Args&&... args) {
		auto m_it = _items_map.find(key);
		if (m_it == _items_map.end()) {
			T& ref = emplace_and(on_drop, key, T(std::forward<Args>(args)...)).first->second;
			switch (on_get(ref)) {
				case GetAction::leave:
					break;
				case GetAction::renew:
					break;
			}
			return ref;
		}
		return at_and(on_get, m_it->second);
	}

	template <typename OnGet, typename OnDrop, typename K>
	T& get_back_and(const OnGet& on_get, const OnDrop& on_drop, const K& key, T& default_) {
		auto m_it = _items_map.find(key);
		if (m_it == _items_map.end()) {
			T& ref = emplace_back_and(on_drop, key, default_).first->second;
			switch (on_get(ref)) {
				case GetAction::leave:
					break;
				case GetAction::renew:
					break;
			}
			return ref;
		}
		return at_and(on_get, m_it->second);
	}

	template <typename OnGet, typename OnDrop, typename K, typename... Args>
	T& get_back_and(const OnGet& on_get, const OnDrop& on_drop, const K& key, Args&&... args) {
		auto m_it = _items_map.find(key);
		if (m_it == _items_map.end()) {
			T& ref = emplace_back_and(on_drop, key, T(std::forward<Args>(args)...)).first->second;
			switch (on_get(ref)) {
				case GetAction::leave:
					break;
				case GetAction::renew:
					break;
			}
			return ref;
		}
		return at_and(on_get, m_it->second);
	}
};

}
