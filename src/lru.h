/*
 * Copyright (C) 2015 deipi.com LLC and contributors. All rights reserved.
 * Copyright (C) 2014, lamerman. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of lamerman nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVERCAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef XAPIAND_INCLUDED_LRU_H
#define	XAPIAND_INCLUDED_LRU_H

#include <cstddef>
#include <stdexcept>
#include <list>

#ifdef HAVE_CXX11
#  include <unordered_map>
#else
#  include <map>
#endif


template<class Key, class T>
class lru_map {
	typedef typename std::pair<const Key, std::unique_ptr<T>> key_value_pair_t;
	typedef typename std::list<key_value_pair_t>::iterator list_iterator_t;
	typedef typename std::list<key_value_pair_t>::reverse_iterator list_reverse_iterator_t;
#ifdef HAVE_CXX11
	typedef typename std::unordered_map<Key, list_iterator_t> lru_map_t;
#else
	typedef typename std::map<Key, list_iterator_t> lru_map_t;
#endif
	typedef typename std::list<key_value_pair_t> lru_list_t;
	typedef typename lru_map_t::iterator map_iterator_t;

protected:
	enum dropping_action {
		drop,
		leave,
		renew
	};

	lru_list_t _items_list;
	lru_map_t _items_map;
	size_t _max_size;

	dropping_action on_drop(T & val) {
		return drop;
	}

public:
	lru_map(size_t max_size=-1) :
		_max_size(max_size) {
	}

	size_t erase(const Key & key) {
		map_iterator_t it(_items_map.find(key));
		if (it != _items_map.end()) {
			_items_list.erase(it->second);
			_items_map.erase(it);
			return 1;
		}
		return 0;
	}

	T & insert(key_value_pair_t &p) {
		erase(p.first);

		_items_list.push_front(std::move(p));
		list_iterator_t first(_items_list.begin());
		_items_map[p.first] = first;

		if (_max_size != -1) {
			list_reverse_iterator_t last(_items_list.rbegin());
			for (size_t i = _items_map.size(); i != 0 && _items_map.size() > _max_size && last != _items_list.rend(); i--) {
				list_iterator_t it = --(last++).base();
				T *ptr = it->second.get();
				switch (on_drop(*ptr)) {
					case renew:
						_items_list.splice(_items_list.begin(), _items_list, it);
						break;
					case leave:
						break;
					case drop:
					default:
						_items_map.erase(it->first);
						_items_list.erase(it);
						break;
				}
			}
		}
		T *ptr = first->second.get();
		return *ptr;
	}

	T & at(const Key & key) {
		map_iterator_t it(_items_map.find(key));
		if (it == _items_map.end()) {
			throw std::range_error("There is no such key in cache");
		} else {
			_items_list.splice(_items_list.begin(), _items_list, it->second);
			T *ptr = it->second->second.get();
			return *ptr;
		}
	}

	T & operator[] (const Key & key) {
		try {
			return at(key);
		} catch (std::range_error) {
			key_value_pair_t pair(key, std::unique_ptr<T>(new T()));
			return insert(pair);
		}
	}

	bool exists(const Key & key) const {
		return _items_map.find(key) != _items_map.end();
	}

	size_t size() const {
		return _items_map.size();
	}

	size_t empty() const {
		return _items_map.empty();
	}

	size_t max_size() const {
		return (_max_size == -1) ? _items_map.max_size() : _max_size;
	}
};

#endif	/* XAPIAND_INCLUDED_LRU_H */
