/*
 * Copyright (C) 2015 deipi.com LLC and contributors. All rights reserved.
 * Copyright (C) 2014, lamerman. All rights reserved.
 *
 * No licenses, express or implied, are granted with respect to any parts of
 * this software. deipi.com LLC retains all intellectual property rights of
 * the source code contained in these files and no copies of any portions of
 * it should be made without express permission from the copyright holders.
 *
 */

#pragma once

#include <list>
#include <unordered_map>
#include <stdexcept>
#include <cstddef>


namespace lru {

	enum class DropAction {
		drop,
		leave,
		renew
	};

	enum class GetAction {
		leave,
		renew
	};

	template<typename Key, typename T>
	class LRU {
	public:

	protected:
		std::list<std::pair<const Key, T>> _items_list;
		std::unordered_map<Key, typename std::list<std::pair<const Key, T>>::iterator> _items_map;
		ssize_t _max_size;

	public:
		LRU(ssize_t max_size=-1) : _max_size(max_size) {}

		size_t erase(const Key& key) {
			auto it(_items_map.find(key));
			if (it != _items_map.end()) {
				_items_list.erase(it->second);
				_items_map.erase(it);
				return 1;
			}
			return 0;
		}

		template<typename P>
		T& insert(P&& p) {
			erase(p.first);

			_items_list.push_front(std::forward<P>(p));
			auto first(_items_list.begin());
			_items_map[first->first] = first;

			if (_max_size != -1) {
				auto last(_items_list.rbegin());
				for (size_t i = _items_map.size(); i != 0 && static_cast<ssize_t>(_items_map.size()) > _max_size && last != _items_list.rend(); i--) {
					auto it = (++last).base();
					_items_map.erase(it->first);
					_items_list.erase(it);
					last = _items_list.rbegin();
				}
			}

			return first->second;
		}

		template<typename... Args>
		T& emplace(Args&&... args) {
			return insert(std::make_pair(std::forward<Args>(args)...));
		}

		T& at(const Key& key) {
			auto it(_items_map.find(key));
			if (it == _items_map.end()) {
				throw std::range_error("There is no such key in cache");
			}

			_items_list.splice(_items_list.begin(), _items_list, it->second);
			return it->second->second;
		}

		T& get(const Key& key) {
			try {
				return at(key);
			} catch (std::range_error) {
				return insert(std::make_pair(key, T()));
			}
		}

		T& operator[] (const Key& key) {
			return get(key);
		}

		bool exists(const Key& key) const {
			return _items_map.find(key) != _items_map.end();
		}

		bool empty() const {
			return _items_map.empty();
		}

		size_t size() const {
			return _items_map.size();
		}

		size_t max_size() const {
			return (_max_size == -1) ? _items_map.max_size() : _max_size;
		}

		template<typename OnDrop, typename P>
		T& insert_and(const OnDrop &on_drop, P&& p) {
			erase(p.first);

			_items_list.push_front(std::forward<P>(p));
			auto first(_items_list.begin());
			_items_map[first->first] = first;

			if (_max_size != -1) {
				auto last(_items_list.rbegin());
				for (size_t i = _items_map.size(); i != 0 && static_cast<ssize_t>(_items_map.size()) > _max_size && last != _items_list.rend(); i--) {
					auto it = --last.base();
					switch (on_drop(it->second)) {
						case DropAction::renew:
							_items_list.splice(_items_list.begin(), _items_list, it);
							break;
						case DropAction::leave:
							break;
						case DropAction::drop:
							_items_map.erase(it->first);
							_items_list.erase(it);
							break;
					}
					last = _items_list.rbegin();
				}
			}

			return first->second;
		}

		template<typename OnDrop, typename... Args>
		T& emplace_and(OnDrop&& on_drop, Args&&... args) {
			return insert_and(std::forward<OnDrop>(on_drop), std::make_pair(std::forward<Args>(args)...));
		}

		template<typename OnGet>
		T& at_and(const OnGet& on_get, const Key& key) {
			auto it(_items_map.find(key));
			if (it == _items_map.end()) {
				throw std::range_error("There is no such key in cache");
			}

			T& ref = it->second->second;
			switch (on_get(ref)) {
				case GetAction::leave:
					break;
				case GetAction::renew:
					_items_list.splice(_items_list.begin(), _items_list, it->second);
					break;
			}
			return ref;
		}

		template<typename OnGet>
		T& get_and(const OnGet& on_get, const Key& key) {
			try {
				return at_and(on_get, key);
			} catch (std::range_error) {
				T& ref = insert(std::make_pair(key, T()));
				switch (on_get(ref)) {
					case GetAction::leave:
						break;
					case GetAction::renew:
						break;
				}
				return ref;
			}
		}
	};
};


#ifdef TESTING_LRU
// Use test as: c++ -DTESTING_LRU -std=c++14 -g -x c++ lru.h -o test_lru && ./test_lru

#include <cassert>
#include <iostream>
using namespace lru;

void test_lru()
{
	LRU<std::string, int> lru(3);
	lru.insert(std::make_pair("test1", 111));
	lru.insert(std::make_pair("test2", 222));
	lru.insert(std::make_pair("test3", 333));
	lru.insert(std::make_pair("test4", 444));  // this pushes 'test1' out of the lru

	try {
		assert(!lru.at("test1"));
	} catch (std::range_error) {
	}

	assert(lru.at("test4") == 444);
	assert(lru.at("test3") == 333);
	assert(lru.at("test2") == 222);

	lru.insert(std::make_pair("test5", 555));  // this pushes 'test4' out of the lru

	try {
		assert(!lru.at("test4"));
	} catch (std::range_error) {
	}

	assert(lru.at("test2") == 222);
	assert(lru.at("test3") == 333);
	assert(lru.at("test5") == 555);

}

void test_lru_emplace()
{
	LRU<std::string, int> lru(3);
	lru.emplace("test1", 111);
	lru.emplace_and([](int&){ return DropAction::leave; }, "test2", 222);
}

void test_lru_actions()
{
	LRU<std::string, int> lru(3);
	lru.insert(std::make_pair("test1", 111));
	lru.insert(std::make_pair("test2", 222));
	lru.insert(std::make_pair("test3", 333));
	lru.insert_and([](int&){ return DropAction::leave; }, std::make_pair("test4", 444));  // this DOES NOT push 'test1' out of the lru

	assert(lru.size() == 4);

	assert(lru.at_and([](int&){ return GetAction::leave; }, "test1") == 111);  // this gets, but doesn't renew 'test1'

	lru.insert(std::make_pair("test5", 555));  // this pushes 'test1' *and* 'test2' out of the lru

	try {
		assert(!lru.at("test1"));
	} catch (std::range_error) {
	}

	assert(lru.size() == 3);

	lru.insert_and([](int&){ return DropAction::renew; }, std::make_pair("test6", 666));  // this renews 'test3'

	assert(lru.size() == 4);

	assert(lru.at("test3") == 333);
	assert(lru.at("test4") == 444);
	assert(lru.at("test5") == 555);
	assert(lru.at("test6") == 666);
}

void test_lru_mutate()
{
	LRU<std::string, int> lru(3);
	lru.insert(std::make_pair("test1", 111));
	assert(lru.at_and([](int& o){ o = 123; return GetAction::leave; }, "test1") == 123);
	assert(lru.get_and([](int& o){ o = 456; return GetAction::leave; }, "test1") == 456);
	assert(lru.at("test1") == 456);
}

int main()
{
	test_lru();
	test_lru_emplace();
	test_lru_actions();
	test_lru_mutate();
	return 0;
}

#endif
