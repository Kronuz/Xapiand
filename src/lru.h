/*
 * Copyright (c) 2015-2019 Dubalu LLC
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

#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <list>
#include <stdexcept>
#include <unordered_map>


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


constexpr const auto AGE_MAX = std::chrono::hours(8000000);


template <typename Key, typename T>
class LRU {
protected:
	using lru_list_t = std::list<std::pair<Key, T>>;
	struct item_t {
		Key key;
		typename lru_list_t::iterator lru_it;
		std::chrono::time_point<std::chrono::steady_clock> expiration;
	};
	using aged_list_t = std::list<item_t>;
	struct map_item_t {
		typename lru_list_t::iterator lru_it;
		typename aged_list_t::iterator aged_it;
		std::chrono::time_point<std::chrono::steady_clock> expiration;
	};
	using map_t = std::unordered_map<Key, map_item_t>;


	lru_list_t _lru_list;
	aged_list_t _aged_list;
	map_t _map;
	size_t _max_size;
	std::chrono::milliseconds _max_age;

public:
	using iterator = typename lru_list_t::iterator;
	using const_iterator = typename lru_list_t::const_iterator;

	explicit LRU(size_t max_size = SIZE_MAX, std::chrono::milliseconds max_age = AGE_MAX) :
		_max_size(max_size),
		_max_age(max_age)
	{
		assert(_max_size != 0);
		assert(_max_age != std::chrono::milliseconds(0));
	}

	iterator begin() noexcept {
		return _lru_list.begin();
	}

	const_iterator begin() const noexcept {
		return _lru_list.begin();
	}

	const_iterator cbegin() const noexcept {
		return _lru_list.cbegin();
	}

	iterator end() noexcept {
		return _lru_list.end();
	}

	const_iterator end() const noexcept {
		return _lru_list.end();
	}

	const_iterator cend() const noexcept {
		return _lru_list.cend();
	}

	template <typename K>
	iterator find(const K& key) {
		auto map_it = _map.find(key);
		if (map_it != _map.end() && _max_age != AGE_MAX && map_it->second.expiration > std::chrono::steady_clock::now()) {
			erase(map_it);
			map_it = _map.end();
		}
		if (map_it == _map.end()) {
			return _lru_list.end();
		}
		auto it = map_it->second.lru_it;
		_lru_list.splice(_lru_list.begin(), _lru_list, it);
		return it;
	}

	template <typename K>
	const_iterator find(const K& key) const {
		auto map_it = _map.find(key);
		if (map_it != _map.end() && _max_age != AGE_MAX && map_it->second.expiration > std::chrono::steady_clock::now()) {
			map_it = _map.end();
		}
		if (map_it == _map.cend()) {
			return _lru_list.cend();
		}
		return map_it->second.lru_it;
	}

	void erase(const_iterator it) {
		erase(_map.find(it->first));
	}

	size_t erase(typename aged_list_t::const_iterator aged_it) {
		if (aged_it != _aged_list.end()) {
			bool ret = aged_it->expiration > std::chrono::steady_clock::now() ? 0 : 1;
			_map.erase(aged_it->lru_it->first);
			_lru_list.erase(aged_it->lru_it);
			_aged_list.erase(aged_it);
			return ret;
		}
		return 0;
	}

	size_t erase(typename map_t::const_iterator map_it) {
		if (map_it != _map.end()) {
			bool ret = map_it->second.expiration > std::chrono::steady_clock::now() ? 0 : 1;
			_lru_list.erase(map_it->second.lru_it);
			_aged_list.erase(map_it->second.aged_it);
			_map.erase(map_it);
			return ret;
		}
		return 0;
	}

	size_t erase(const Key& key) {
		return erase(_map.find(key));
	}

	void _trim_aged(size_t& size) {
		auto now = std::chrono::steady_clock::now();
		auto last = --_aged_list.end();
		for (size_t i = _aged_list.size(); i != 0; --i) {
			auto aged_it = last--;
			if (aged_it->expiration > now) {
				--size;
				erase(aged_it);
			} else {
				break;
			}
		}
	}

	void _trim_lru(size_t& size) {
		auto last = --_lru_list.end();
		for (size_t i = _lru_list.size(); i != 0; --i) {
			auto it = last--;
			if (size > _max_size) {
				--size;
				erase(it);
			} else {
				return;
			}
		}
	}

	void trim(size_t size) {
		if (_max_age != AGE_MAX) {
			_trim_aged(size);
		}
		if (_max_size != SIZE_MAX) {
			_trim_lru(size);
		}
	}

	void trim() {
		trim(_map.size());
	}

	template <typename P>
	std::pair<iterator, bool> insert(P&& p) {
		erase(p.first);
		trim();
		_lru_list.push_front(std::forward<P>(p));
		auto it = _lru_list.begin();
		auto expiration = _max_age != AGE_MAX ? std::chrono::steady_clock::now() + _max_age : std::chrono::time_point<std::chrono::steady_clock>{};
		_aged_list.push_front({
			it->first,
			it,
			expiration,
		});
		bool created = _map.emplace(it->first, map_item_t{
			it,
			_aged_list.begin(),
			expiration,
		}).second;
		return std::make_pair(it, created);
	}

	template <typename P>
	std::pair<iterator, bool> insert_back(P&& p) {
		erase(p.first);
		trim();
		_lru_list.push_back(std::forward<P>(p));
		auto it = --_lru_list.end();
		auto expiration = _max_age != AGE_MAX ? std::chrono::steady_clock::now() + _max_age : std::chrono::time_point<std::chrono::steady_clock>{};
		_aged_list.push_front({
			it->first,
			it,
			expiration,
		});
		bool created = _map.emplace(it->first, map_item_t{
			it,
			_aged_list.begin(),
			expiration,
		}).second;
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
		_lru_list.splice(_lru_list.begin(), _lru_list, it);
		return it->second;
	}

	const T& at(const_iterator it) const {
		return it->second;
	}

	template <typename K>
	T& at(const K& key) {
		auto map_it = _map.find(key);
		if (map_it != _map.end() && _max_age != AGE_MAX && map_it->second.expiration > std::chrono::steady_clock::now()) {
			erase(map_it);
			map_it = _map.end();
		}
		if (map_it == _map.end()) {
			throw std::out_of_range("There is no such key in cache");
		}
		return at(map_it->second.lru_it);
	}

	template <typename K>
	const T& at(const K& key) const {
		auto map_it = _map.find(key);
		if (map_it != _map.end() && _max_age != AGE_MAX && map_it->second.expiration > std::chrono::steady_clock::now()) {
			map_it = _map.end();
		}
		if (map_it == _map.end()) {
			throw std::out_of_range("There is no such key in cache");
		}
		return at(map_it->second.lru_it);
	}

	template <typename K>
	T& get(const K& key, T& default_) {
		auto map_it = _map.find(key);
		if (map_it != _map.end() && _max_age != AGE_MAX && map_it->second.expiration > std::chrono::steady_clock::now()) {
			erase(map_it);
			map_it = _map.end();
		}
		if (map_it == _map.end()) {
			return emplace(key, default_).first->second;
		}
		return at(map_it->second.lru_it);
	}

	template <typename K, typename... Args>
	T& get(const K& key, Args&&... args) {
		auto map_it = _map.find(key);
		if (map_it != _map.end() && _max_age != AGE_MAX && map_it->second.expiration > std::chrono::steady_clock::now()) {
			erase(map_it);
			map_it = _map.end();
		}
		if (map_it == _map.end()) {
			return emplace(key, T(std::forward<Args>(args)...)).first->second;
		}
		return at(map_it->second.lru_it);
	}

	template <typename K>
	T& get_back(const K& key, T& default_) {
		auto map_it = _map.find(key);
		if (map_it != _map.end() && _max_age != AGE_MAX && map_it->second.expiration > std::chrono::steady_clock::now()) {
			erase(map_it);
			map_it = _map.end();
		}
		if (map_it == _map.end()) {
			return emplace_back(key, default_).first->second.second;
		}
		return at(map_it->second.lru_it);
	}

	template <typename K, typename... Args>
	T& get_back(const K& key, Args&&... args) {
		auto map_it = _map.find(key);
		if (map_it != _map.end() && _max_age != AGE_MAX && map_it->second.expiration > std::chrono::steady_clock::now()) {
			erase(map_it);
			map_it = _map.end();
		}
		if (map_it == _map.end()) {
			return emplace_back(key, T(std::forward<Args>(args)...)).first->second.second;
		}
		return at(map_it->second.lru_it);
	}

	template <typename K>
	T& operator[] (const K& key) {
		return get(key);
	}

	template <typename K>
	bool exists(const K& key) const {
		auto map_it = _map.find(key);
		if (map_it != _map.end() && _max_age != AGE_MAX && map_it->second.expiration > std::chrono::steady_clock::now()) {
			map_it = _map.end();
		}
		return map_it != _map.end();
	}

	void clear() noexcept {
		_map.clear();
		_lru_list.clear();
	}

	bool empty() const noexcept {
		return _map.empty();
	}

	size_t size() const noexcept {
		return _map.size();
	}

	size_t max_size() const noexcept {
		return (_max_size == SIZE_MAX) ? _map.max_size() : _max_size;
	}

	template <typename OnDrop>
	void _trim_aged(const OnDrop& on_drop, size_t& size) {
		auto now = std::chrono::steady_clock::now();
		auto last = --_aged_list.end();
		for (size_t i = _aged_list.size(); i != 0; --i) {
			auto aged_it = last--;
			if (aged_it->expiration > now) {
				switch (on_drop(aged_it->lru_it->second, size, _max_size)) {
					case DropAction::evict:
						--size;
						erase(aged_it);
						break;
					case DropAction::renew:
						_lru_list.splice(_lru_list.begin(), _lru_list, aged_it->lru_it);
						break;
					case DropAction::leave:
						break;
					case DropAction::stop:
						return;
				}
			} else {
				return;
			}
		}
	}

	template <typename OnDrop>
	void _trim_lru(const OnDrop& on_drop, size_t& size) {
		auto last = --_lru_list.end();
		for (size_t i = _lru_list.size(); i != 0; --i) {
			auto it = last--;
			switch (on_drop(it->second, size, _max_size)) {
				case DropAction::evict:
					--size;
					erase(it);
					break;
				case DropAction::renew:
					_lru_list.splice(_lru_list.begin(), _lru_list, it);
					break;
				case DropAction::leave:
					break;
				case DropAction::stop:
					return;
			}
		}
	}

	template <typename OnDrop>
	void trim(const OnDrop& on_drop, size_t size) {
		if (_max_age != AGE_MAX) {
			_trim_aged(on_drop, size);
		}
		if (_max_size != SIZE_MAX) {
			_trim_lru(on_drop, size);
		}
	}

	template <typename OnDrop>
	void trim(const OnDrop& on_drop) {
		trim(on_drop, _map.size());
	}

	template <typename OnDrop, typename P>
	std::pair<iterator, bool> insert_and(const OnDrop& on_drop, P&& p) {
		erase(p.first);
		trim(on_drop, _map.size() + 1);
		_lru_list.push_front(std::forward<P>(p));
		auto it = _lru_list.begin();
		auto expiration = _max_age != AGE_MAX ? std::chrono::steady_clock::now() + _max_age : std::chrono::time_point<std::chrono::steady_clock>{};
		_aged_list.push_front({
			it->first,
			it,
			expiration,
		});
		bool created = _map.emplace(it->first, map_item_t{
			it,
			_aged_list.begin(),
			expiration,
		}).second;
		return std::make_pair(it, created);
	}

	template <typename OnDrop, typename P>
	std::pair<iterator, bool> insert_back_and(const OnDrop& on_drop, P&& p) {
		erase(p.first);
		trim(on_drop, _map.size() + 1);
		_lru_list.push_back(std::forward<P>(p));
		auto it = --_lru_list.end();
		auto expiration = _max_age != AGE_MAX ? std::chrono::steady_clock::now() + _max_age : std::chrono::time_point<std::chrono::steady_clock>{};
		_aged_list.push_front({
			it->first,
			it,
			expiration,
		});
		bool created = _map.emplace(it->first, map_item_t{
			it,
			_aged_list.begin(),
			expiration,
		}).second;
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
		auto map_it = _map.find(key);
		if (map_it != _map.end() && _max_age != AGE_MAX && map_it->second.expiration > std::chrono::steady_clock::now()) {
			erase(map_it);
			map_it = _map.end();
		}
		if (map_it == _map.end()) {
			return _lru_list.end();
		}
		auto it = map_it->second.lru_it;
		T& ref = it->second;
		switch (on_get(ref)) {
			case GetAction::leave:
				break;
			case GetAction::renew:
				_lru_list.splice(_lru_list.begin(), _lru_list, it);
				break;
		}
		return it;
	}

	template <typename K>
	iterator find_and_leave(const K& key) {
		auto map_it = _map.find(key);
		if (map_it != _map.end() && _max_age != AGE_MAX && map_it->second.expiration > std::chrono::steady_clock::now()) {
			erase(map_it);
			map_it = _map.end();
		}
		if (map_it == _map.end()) {
			return _lru_list.end();
		}
		return map_it->second.lru_it;
	}

	template <typename K>
	const_iterator find_and_leave(const K& key) const {
		auto map_it = _map.find(key);
		if (map_it != _map.end() && _max_age != AGE_MAX && map_it->second.expiration > std::chrono::steady_clock::now()) {
			map_it = _map.end();
		}
		if (map_it == _map.end()) {
			return _lru_list.end();
		}
		return map_it->second.lru_it;
	}

	template <typename K>
	iterator find_and_renew(const K& key) {
		auto map_it = _map.find(key);
		if (map_it != _map.end() && _max_age != AGE_MAX && map_it->second.expiration > std::chrono::steady_clock::now()) {
			erase(map_it);
			map_it = _map.end();
		}
		if (map_it == _map.end()) {
			return _lru_list.end();
		}
		auto it = map_it->second.lru_it;
		_lru_list.splice(_lru_list.begin(), _lru_list, it);
		return it;
	}

	template <typename OnGet>
	T& at_and(const OnGet& on_get, iterator it) {
		T& ref = it->second;
		switch (on_get(ref)) {
			case GetAction::leave:
				break;
			case GetAction::renew:
				_lru_list.splice(_lru_list.begin(), _lru_list, it);
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
		_lru_list.splice(_lru_list.begin(), _lru_list, it);
		return ref;
	}

	template <typename K, typename OnGet>
	T& at_and(const OnGet& on_get, const K& key) {
		auto map_it = _map.find(key);
		if (map_it != _map.end() && _max_age != AGE_MAX && map_it->second.expiration > std::chrono::steady_clock::now()) {
			erase(map_it);
			map_it = _map.end();
		}
		if (map_it == _map.end()) {
			throw std::out_of_range("There is no such key in cache");
		}
		return at_and(on_get, map_it->second.lru_it);
	}

	template <typename K>
	T& at_and_leave(const K& key) {
		auto map_it = _map.find(key);
		if (map_it != _map.end() && _max_age != AGE_MAX && map_it->second.expiration > std::chrono::steady_clock::now()) {
			erase(map_it);
			map_it = _map.end();
		}
		if (map_it == _map.end()) {
			throw std::out_of_range("There is no such key in cache");
		}
		return at_and_leave(map_it->second.lru_it);
	}

	template <typename K>
	const T& at_and_leave(const K& key) const {
		auto map_it = _map.find(key);
		if (map_it != _map.end() && _max_age != AGE_MAX && map_it->second.expiration > std::chrono::steady_clock::now()) {
			map_it = _map.end();
		}
		if (map_it == _map.end()) {
			throw std::out_of_range("There is no such key in cache");
		}
		return at_and_leave(map_it->second.lru_it);
	}

	template <typename K>
	T& at_and_renew(const K& key) {
		auto map_it = _map.find(key);
		if (map_it != _map.end() && _max_age != AGE_MAX && map_it->second.expiration > std::chrono::steady_clock::now()) {
			erase(map_it);
			map_it = _map.end();
		}
		if (map_it == _map.end()) {
			throw std::out_of_range("There is no such key in cache");
		}
		return at_and_renew(map_it->second.lru_it);
	}

	template <typename OnGet, typename OnDrop, typename K>
	T& get_and(const OnGet& on_get, const OnDrop& on_drop, const K& key, T& default_) {
		auto map_it = _map.find(key);
		if (map_it != _map.end() && _max_age != AGE_MAX && map_it->second.expiration > std::chrono::steady_clock::now()) {
			erase(map_it);
			map_it = _map.end();
		}
		if (map_it == _map.end()) {
			T& ref = emplace_and(on_drop, key, default_).first->second;
			switch (on_get(ref)) {
				case GetAction::leave:
					break;
				case GetAction::renew:
					break;
			}
			return ref;
		}
		return at_and(on_get, map_it->second.lru_it);
	}

	template <typename OnGet, typename OnDrop, typename K, typename... Args>
	T& get_and(const OnGet& on_get, const OnDrop& on_drop, const K& key, Args&&... args) {
		auto map_it = _map.find(key);
		if (map_it != _map.end() && _max_age != AGE_MAX && map_it->second.expiration > std::chrono::steady_clock::now()) {
			erase(map_it);
			map_it = _map.end();
		}
		if (map_it == _map.end()) {
			T& ref = emplace_and(on_drop, key, T(std::forward<Args>(args)...)).first->second;
			switch (on_get(ref)) {
				case GetAction::leave:
					break;
				case GetAction::renew:
					break;
			}
			return ref;
		}
		return at_and(on_get, map_it->second.lru_it);
	}

	template <typename OnGet, typename OnDrop, typename K>
	T& get_back_and(const OnGet& on_get, const OnDrop& on_drop, const K& key, T& default_) {
		auto map_it = _map.find(key);
		if (map_it != _map.end() && _max_age != AGE_MAX && map_it->second.expiration > std::chrono::steady_clock::now()) {
			erase(map_it);
			map_it = _map.end();
		}
		if (map_it == _map.end()) {
			T& ref = emplace_back_and(on_drop, key, default_).first->second;
			switch (on_get(ref)) {
				case GetAction::leave:
					break;
				case GetAction::renew:
					break;
			}
			return ref;
		}
		return at_and(on_get, map_it->second.lru_it);
	}

	template <typename OnGet, typename OnDrop, typename K, typename... Args>
	T& get_back_and(const OnGet& on_get, const OnDrop& on_drop, const K& key, Args&&... args) {
		auto map_it = _map.find(key);
		if (map_it != _map.end() && _max_age != AGE_MAX && map_it->second.expiration > std::chrono::steady_clock::now()) {
			erase(map_it);
			map_it = _map.end();
		}
		if (map_it == _map.end()) {
			T& ref = emplace_back_and(on_drop, key, T(std::forward<Args>(args)...)).first->second;
			switch (on_get(ref)) {
				case GetAction::leave:
					break;
				case GetAction::renew:
					break;
			}
			return ref;
		}
		return at_and(on_get, map_it->second.lru_it);
	}
};

}
