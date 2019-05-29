/*
 * Copyright (c) 2019 German Mendez Bravo (Kronuz)
 * Copyright (c) 2013 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *
 * A btree::map<> implements the STL unique sorted associative container
 * interface and the pair associative container interface (a.k.a map<>) using a
 * btree. See btree.h for details of the btree implementation and caveats.
 */

#ifndef BTREE_MAP_H__
#define BTREE_MAP_H__

#include "btree.h"

#include <stdexcept>

namespace btree {

// A common base class for map and safe_map.
template <typename Tree>
class btree_map_container : public btree_unique_container<Tree> {
	typedef btree_map_container<Tree> self_type;
	typedef btree_unique_container<Tree> super_type;

public:
	typedef typename Tree::key_type key_type;
	typedef typename Tree::data_type data_type;
	typedef typename Tree::value_type value_type;
	typedef typename Tree::mapped_type mapped_type;
	typedef typename Tree::key_compare key_compare;
	typedef typename Tree::allocator_type allocator_type;
	typedef typename Tree::iterator iterator;
	typedef typename Tree::const_iterator const_iterator;

public:
	// Default constructor.
	btree_map_container(const key_compare& comp = key_compare(),
				  const allocator_type& alloc = allocator_type())
			: super_type(comp, alloc) {
	}

	// Copy constructor.
	btree_map_container(const self_type& x)
			: super_type(x) {
	}

	// Range constructor.
	template <class InputIterator>
	btree_map_container(InputIterator b, InputIterator e,
				  const key_compare& comp = key_compare(),
				  const allocator_type& alloc = allocator_type())
			: super_type(b, e, comp, alloc) {
	}

	template <typename... Args>
	std::pair<iterator, bool> try_emplace(const key_type& key, Args&&... args) {
		return this->__tree.emplace_unique_key_args(key,
			std::piecewise_construct,
			std::forward_as_tuple(key),
			std::forward_as_tuple(std::forward<Args>(args)...));
	}

	template <typename... Args>
	std::pair<iterator, bool> try_emplace(key_type&& key, Args&&... args) {
		return this->__tree.emplace_unique_key_args(key,
			std::piecewise_construct,
			std::forward_as_tuple(std::move(key)),
			std::forward_as_tuple(std::forward<Args>(args)...));
	}

	template <typename... Args>
	iterator try_emplace(const_iterator hint, const key_type& key, Args&&... args) {
		return this->__tree.emplace_hint_unique_key_args(hint, key,
			std::piecewise_construct,
			std::forward_as_tuple(key),
			std::forward_as_tuple(std::forward<Args>(args)...));
	}

	template <typename... Args>
	iterator try_emplace(const_iterator hint, key_type&& key, Args&&... args) {
		return this->__tree.emplace_hint_unique_key_args(hint, key,
			std::piecewise_construct,
			std::forward_as_tuple(std::move(key)),
			std::forward_as_tuple(std::forward<Args>(args)...));
	}

	// Access specified element with bounds checking.
	mapped_type& at(const key_type& key) {
		auto it = this->find(key);
		if (it == this->end()) {
			throw std::out_of_range("map::at:  key not found");
		}
		return it->second;
	}
	const mapped_type& at(const key_type& key) const {
		auto it = this->find(key);
		if (it == this->end()) {
			throw std::out_of_range("map::at:  key not found");
		}
		return it->second;
	}

	// Insertion routines.
	data_type& operator[](const key_type& key) {
		return this->try_emplace(key).first->second;
	}

	data_type& operator[](key_type&& key) {
		return this->try_emplace(std::move(key)).first->second;
	}
};

// The map class is needed mainly for its constructors.
template <typename Key, typename Value,
	typename Compare = std::less<Key>,
	typename Alloc = std::allocator<std::pair<const Key, Value>>,
	int TargetNodeSize = 256>
class map : public btree_map_container<
	btree<btree_map_params<Key, Value, Compare, Alloc, TargetNodeSize>>> {

	typedef map<Key, Value, Compare, Alloc, TargetNodeSize> self_type;
	typedef btree_map_params<Key, Value, Compare, Alloc, TargetNodeSize> params_type;
	typedef btree<params_type> btree_type;
	typedef btree_map_container<btree_type> super_type;

public:
	typedef typename btree_type::key_compare key_compare;
	typedef typename btree_type::allocator_type allocator_type;

public:
	// Default constructor.
	map(const key_compare& comp = key_compare(),
		const allocator_type& alloc = allocator_type())
		: super_type(comp, alloc) {
	}

	// Copy constructor.
	map(const self_type& x)
		: super_type(x) {
	}

	// Range constructor.
	template <class InputIterator>
	map(InputIterator b, InputIterator e,
		const key_compare& comp = key_compare(),
		const allocator_type& alloc = allocator_type())
		: super_type(b, e, comp, alloc) {
	}
};

} // namespace btree

template <typename K, typename V, typename C, typename A, int N>
bool operator==(const btree::map<K, V, C, A, N>& lhs, const btree::map<K, V, C, A, N>& rhs) {
	return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

template <typename K, typename V, typename C, typename A, int N>
bool operator<(const btree::map<K, V, C, A, N>& lhs, const btree::map<K, V, C, A, N>& rhs) {
	return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

template <typename K, typename V, typename C, typename A, int N>
bool operator!=(const btree::map<K, V, C, A, N>& lhs, const btree::map<K, V, C, A, N>& rhs) {
	return !(lhs == rhs);
}

template <typename K, typename V, typename C, typename A, int N>
bool operator>(const btree::map<K, V, C, A, N>& lhs, const btree::map<K, V, C, A, N>& rhs) {
	return rhs < lhs;
}

template <typename K, typename V, typename C, typename A, int N>
bool operator>=(const btree::map<K, V, C, A, N>& lhs, const btree::map<K, V, C, A, N>& rhs) {
	return !(lhs < rhs);
}

template <typename K, typename V, typename C, typename A, int N>
bool operator<=(const btree::map<K, V, C, A, N>& lhs, const btree::map<K, V, C, A, N>& rhs) {
	return !(rhs < lhs);
}

template <typename K, typename V, typename C, typename A, int N>
inline void swap(btree::map<K, V, C, A, N>& x, btree::map<K, V, C, A, N>& y) {
	x.swap(y);
}

namespace btree {

// The multimap class is needed mainly for its constructors.
template <typename Key, typename Value,
	typename Compare = std::less<Key>,
	typename Alloc = std::allocator<std::pair<const Key, Value> >,
	int TargetNodeSize = 256>
class multimap : public btree_multi_container<
	btree<btree_map_params<Key, Value, Compare, Alloc, TargetNodeSize> > > {

	typedef multimap<Key, Value, Compare, Alloc, TargetNodeSize> self_type;
	typedef btree_map_params< Key, Value, Compare, Alloc, TargetNodeSize> params_type;
	typedef btree<params_type> btree_type;
	typedef btree_multi_container<btree_type> super_type;

 public:
	typedef typename btree_type::key_compare key_compare;
	typedef typename btree_type::allocator_type allocator_type;
	typedef typename btree_type::data_type data_type;
	typedef typename btree_type::mapped_type mapped_type;

 public:
	// Default constructor.
	multimap(const key_compare& comp = key_compare(),
			 const allocator_type& alloc = allocator_type())
		: super_type(comp, alloc) {
	}

	// Copy constructor.
	multimap(const self_type& x)
		: super_type(x) {
	}

	// Range constructor.
	template <class InputIterator>
	multimap(InputIterator b, InputIterator e,
			 const key_compare& comp = key_compare(),
			 const allocator_type& alloc = allocator_type())
		: super_type(b, e, comp, alloc) {
	}
};

} // namespace btree

template <typename K, typename V, typename C, typename A, int N>
bool operator==(const btree::multimap<K, V, C, A, N>& lhs, const btree::multimap<K, V, C, A, N>& rhs) {
	return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

template <typename K, typename V, typename C, typename A, int N>
bool operator<(const btree::multimap<K, V, C, A, N>& lhs, const btree::multimap<K, V, C, A, N>& rhs) {
	return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

template <typename K, typename V, typename C, typename A, int N>
bool operator!=(const btree::multimap<K, V, C, A, N>& lhs, const btree::multimap<K, V, C, A, N>& rhs) {
	return !(lhs == rhs);
}

template <typename K, typename V, typename C, typename A, int N>
bool operator>(const btree::multimap<K, V, C, A, N>& lhs, const btree::multimap<K, V, C, A, N>& rhs) {
	return rhs < lhs;
}

template <typename K, typename V, typename C, typename A, int N>
bool operator>=(const btree::multimap<K, V, C, A, N>& lhs, const btree::multimap<K, V, C, A, N>& rhs) {
	return !(lhs < rhs);
}

template <typename K, typename V, typename C, typename A, int N>
bool operator<=(const btree::multimap<K, V, C, A, N>& lhs, const btree::multimap<K, V, C, A, N>& rhs) {
	return !(rhs < lhs);
}

template <typename K, typename V, typename C, typename A, int N>
inline void swap(btree::multimap<K, V, C, A, N>& x, btree::multimap<K, V, C, A, N>& y) {
	x.swap(y);
}

#endif  // BTREE_MAP_H__
