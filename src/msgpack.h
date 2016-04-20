/*
 * Copyright (C) 2015,2016 deipi.com LLC and contributors. All rights reserved.
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

#include "msgpack.hpp"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"
#include "xchange/rapidjson.hpp"

#include <memory>
#include <sstream>
#include <unordered_map>

#define MSGPACK_MAP_INIT_SIZE 64
#define MSGPACK_ARRAY_INIT_SIZE 64


class MsgPack {
	class final_on_range : public std::out_of_range {
		using std::out_of_range::out_of_range;
	};

	struct Body {
		std::unordered_map<std::string, std::pair<std::shared_ptr<MsgPack>, std::shared_ptr<MsgPack>>> map;
		std::vector<std::shared_ptr<MsgPack>> array;

		ssize_t _capacity;

		const std::shared_ptr<msgpack::zone> _zone;
		const std::shared_ptr<msgpack::object> _base;
		std::shared_ptr<MsgPack> _parent;
		bool _is_key;
		size_t _pos;
		std::shared_ptr<MsgPack> _key;
		msgpack::object* _obj;

		mutable std::shared_ptr<const MsgPack> _nil;

		Body(
			const std::shared_ptr<MsgPack>& parent,
			bool is_key,
			size_t pos,
			const std::shared_ptr<MsgPack>& key,
			msgpack::object* obj
		) : _capacity(-1),
			_zone(parent->_body->_zone),
			_base(parent->_body->_base),
			_parent(parent),
			_is_key(is_key),
			_pos(pos),
			_key(key),
			_obj(obj) { }

		template <typename T>
		Body(T&& v) :
			_capacity(-1),
			_zone(std::make_shared<msgpack::zone>()),
			_base(std::make_shared<msgpack::object>(std::forward<T>(v), *_zone)),
			_pos(0),
			_obj(_base.get()) { }
	};

	std::shared_ptr<Body> _body;

public:
	template <typename T>
	class Iterator : public std::iterator<std::input_iterator_tag, MsgPack> {
		friend MsgPack;

		T* _mobj;
		off_t _off;

	public:
		Iterator(T* o, off_t off)
			: _mobj(o),
			  _off(off) { }

		Iterator(const Iterator& it)
			: _mobj(it._mobj),
			  _off(it._off) { }

		Iterator& operator++() {
			++_off;
			return *this;
		}

		Iterator operator++(int) {
			Iterator tmp(*this);
			++_off;
			return tmp;
		}

		Iterator& operator+=(int _off) {
			_off += _off;
			return *this;
		}

		Iterator operator+(int _off) const {
			Iterator tmp(*this);
			tmp._off += _off;
			return tmp;
		}

		Iterator operator=(const Iterator& other) {
			_mobj = other._mobj;
			_off = other._off;
			return *this;
		}

		T& operator*() const {
			switch (_mobj->_body->_obj->type) {
				case msgpack::type::MAP:
					return *_mobj->at(_off)._body->_key;
				case msgpack::type::ARRAY:
					return _mobj->at(_off);
				default:
					throw msgpack::type_error();
			}
		}

		bool operator==(const Iterator& other) const {
			return *_mobj == *other._mobj && _off == other._off;
		}

		bool operator!=(const Iterator& other) const {
			return !operator==(other);
		}
	};

	using iterator = Iterator<MsgPack>;
	using const_iterator = Iterator<const MsgPack>;

	iterator begin() {
		return iterator(this, 0);
	}

	const_iterator begin() const {
		return const_iterator(this, 0);
	}

	const_iterator cbegin() const {
		return const_iterator(this, 0);
	}

	iterator end() {
		return iterator(this, size());
	}

	const_iterator end() const {
		return const_iterator(this, size());
	}

	const_iterator cend() const {
		return const_iterator(this, size());
	}

private:
	template <typename... Args>
	static decltype(auto) make_shared(Args&&... args) {
		/*
		 * std::make_shared only can call a public constructor, for this reason
		 * it is neccesary wrap the constructor in a struct.
		 */
		struct enable_make_shared : MsgPack {
			enable_make_shared(Args&&... args) : MsgPack(std::forward<Args>(args)...) { }
		};
		return static_cast<std::shared_ptr<MsgPack>>(std::make_shared<enable_make_shared>(std::forward<Args>(args)...));
	}

	MsgPack(const std::shared_ptr<MsgPack>& parent, bool is_key, size_t pos, const std::shared_ptr<MsgPack>& key, msgpack::object* obj)
		: _body(std::make_shared<Body>(parent, is_key, pos, key, obj))
	{
		_init();
	}

	MsgPack(const std::shared_ptr<Body>& b)
		: _body(b) { }

	MsgPack(std::shared_ptr<Body>&& b)
		: _body(std::move(b)) { }

	void _initializer_array(const std::initializer_list<MsgPack>& list) {
		_body->_obj->type = msgpack::type::ARRAY;
		_body->_obj->via.array.ptr = nullptr;
		_body->_obj->via.array.size = 0;
		_reserve_array(list.size());
		for (const auto& val : list) {
			push_back(val);
		}
	}

	void _initializer_map(const std::initializer_list<MsgPack>& list) {
		_body->_obj->type = msgpack::type::MAP;
		_body->_obj->via.map.ptr = nullptr;
		_body->_obj->via.map.size = 0;
		_reserve_map(list.size());
		for (const auto& val : list) {
			put(val.at(0), val.at(1));
		}
	}

	void _initializer(const std::initializer_list<MsgPack>& list) {
		auto isMap = true;
		for (const auto& val : list) {
			if (!val.is_array() || val.size() != 2 || !val.at(0).is_string()) {
				isMap = false;
				break;
			}
		}
		if (isMap) _initializer_map(list);
		else _initializer_array(list);
	}

public:
	MsgPack()
		: MsgPack(nullptr) { }

	template <typename T, typename = std::enable_if_t<!std::is_same<std::shared_ptr<Body>, std::decay_t<T>>::value>>
	MsgPack(T&& v)
		: _body(std::make_shared<Body>(std::forward<T>(v)))
	{
		_init();
	}

	MsgPack(const std::initializer_list<MsgPack>& list)
		: MsgPack(nullptr)
	{
		_initializer(list);
	}

	MsgPack& operator=(const std::initializer_list<MsgPack>& list) {
		clear();
		_initializer(list);
		return *this;
	}

	template <typename T>
	MsgPack& operator=(T&& v) {
		clear();
		auto obj = msgpack::object(std::forward<T>(v), *_body->_zone);
		if (_body->_is_key) {
			if (obj.type != msgpack::type::STR) {
				throw msgpack::type_error();
			}
			// Change key in the parent's map:
			auto it = _body->_parent->_body->map.find(std::string(_body->_obj->via.str.ptr, _body->_obj->via.str.size));
			if (it != _body->_parent->_body->map.end()) {
				_body->_parent->_body->map.insert(std::make_pair(std::string(obj.via.str.ptr, obj.via.str.size), it->second));
				_body->_parent->_body->map.erase(it);
				assert(_body->_parent->_body->_obj->via.map.size == _body->_parent->_body->map.size());
			}
		}
		*_body->_obj = obj;
		_init();
		return *this;
	}

private:
	const MsgPack& nil() const {
		auto old_nil = _body->_nil;
		if (!_body->_nil) {
			std::shared_ptr<const MsgPack> new_nil = make_shared(std::make_shared<Body>(make_shared(_body), false, 0, nullptr, nullptr));
			std::atomic_compare_exchange_strong(&_body->_nil, &old_nil, new_nil);
		}
		return *_body->_nil;
	}

	std::shared_ptr<MsgPack> _init_map(size_t pos) {
		std::shared_ptr<MsgPack> last_val;
		_body->map.reserve(_body->_capacity);
		const auto pend = &_body->_obj->via.map.ptr[_body->_obj->via.map.size];
		for (auto p = &_body->_obj->via.map.ptr[pos]; p != pend; ++p, ++pos) {
			if (p->key.type != msgpack::type::STR) {
				throw msgpack::type_error();
			}
			auto parent = make_shared(_body);
			auto last_key = make_shared(std::make_shared<Body>(parent, true, 0, nullptr, &p->key));
			last_val = make_shared(std::make_shared<Body>(parent, false, pos, last_key, &p->val));
			auto pair = std::make_pair(
				std::string(p->key.via.str.ptr, p->key.via.str.size),
				std::make_pair(last_key, last_val)
			);
			_body->map.insert(std::move(pair));
		}
		assert(_body->_obj->via.map.size == _body->map.size());
		return last_val;
	}

	void _update_map(size_t pos) {
		const auto pend = &_body->_obj->via.map.ptr[_body->_obj->via.map.size];
		for (auto p = &_body->_obj->via.map.ptr[pos]; p != pend; ++p, ++pos) {
			auto mobj = _body->map.at(std::string(p->key.via.str.ptr, p->key.via.str.size)).second;
			mobj->_body->_pos = pos;
			mobj->_body->_obj = &p->val;
		}
	}

	std::shared_ptr<MsgPack> _init_array(size_t pos) {
		std::shared_ptr<MsgPack> last_val;

		if (pos < _body->array.size()) {
			// Destroy the previous objects to update
			_body->array.resize(pos);
		}

		_body->array.reserve(_body->_capacity);
		const auto pend = &_body->_obj->via.array.ptr[_body->_obj->via.array.size];
		for (auto p = &_body->_obj->via.array.ptr[pos]; p != pend; ++p, ++pos) {
			last_val = make_shared(std::make_shared<Body>(make_shared(_body), false, pos, nullptr, p));
			_body->array.push_back(last_val);
		}
		assert(_body->_obj->via.array.size == _body->array.size());
		return last_val;
	}

	void _update_array(size_t pos) {
		const auto pend = &_body->_obj->via.array.ptr[_body->_obj->via.array.size];
		for (auto p = &_body->_obj->via.array.ptr[pos]; p != pend; ++p, ++pos) {
			auto mobj = _body->array.at(pos);
			mobj->_body->_pos = pos;
			mobj->_body->_obj = p;
		}
	}

	void _init() {
		switch (_body->_obj->type) {
			case msgpack::type::MAP:
				assert(_body->map.empty());
				_body->_capacity = _body->_obj->via.map.size;
				_init_map(0);
				break;
			case msgpack::type::ARRAY:
				assert(_body->array.empty());
				_body->_capacity = _body->_obj->via.array.size;
				_init_array(0);
				break;
			default:
				_body->_capacity = 0;
				break;
		}
	}

	void _reserve_map(size_t rsize) {
		if (_body->_capacity <= static_cast<ssize_t>(rsize)) {
			size_t nsize = _body->_capacity > 0 ? _body->_capacity * 2 : MSGPACK_MAP_INIT_SIZE;
			while (nsize < rsize || nsize < MSGPACK_MAP_INIT_SIZE) {
				nsize *= 2;
			}
			auto ptr = static_cast<msgpack::object_kv*>(_body->_zone->allocate_align(nsize * sizeof(msgpack::object_kv)));
			if (_body->_obj->via.map.ptr != nullptr && _body->_capacity > 0) {
				std::memcpy(ptr, _body->_obj->via.map.ptr, _body->_obj->via.map.size * sizeof(msgpack::object_kv));
				// std::memset(_body->_obj->via.map.ptr, 0, _body->_obj->via.map.size * sizeof(msgpack::object_kv));  // clear memory for debugging
			}
			_body->_obj->via.map.ptr = ptr;
			_body->_capacity = nsize;
			_init_map(_body->map.size());
			_update_map(0);
		}
	}

	void _reserve_array(size_t rsize) {
		if (_body->_capacity <= static_cast<ssize_t>(rsize)) {
			size_t nsize = _body->_capacity > 0 ? _body->_capacity * 2 : MSGPACK_ARRAY_INIT_SIZE;
			while (nsize < rsize || nsize < MSGPACK_ARRAY_INIT_SIZE) {
				nsize *= 2;
			}
			auto ptr = static_cast<msgpack::object*>(_body->_zone->allocate_align(nsize * sizeof(msgpack::object)));
			if (_body->_obj->via.array.ptr != nullptr && _body->_capacity > 0) {
				std::memcpy(ptr, _body->_obj->via.array.ptr, _body->_obj->via.array.size * sizeof(msgpack::object));
				// std::memset(_body->_obj->via.array.ptr, 0, _body->_obj->via.array.size * sizeof(msgpack::object));  // clear memory for debugging
			}
			_body->_obj->via.array.ptr = ptr;
			_body->_capacity = nsize;
			_init_array(_body->array.size());
			_update_array(0);
		}
	}

	MsgPack& _at(const std::string& key) const {
		switch (_body->_obj->type) {
			case msgpack::type::NIL:
				throw std::out_of_range("nil");
			case msgpack::type::MAP:
				return *_body->map.at(key).second;
			default:
				throw msgpack::type_error();
		}
	}

	MsgPack& _at(size_t pos) const {
		switch (_body->_obj->type) {
			case msgpack::type::NIL:
				throw std::out_of_range("nil");
			case msgpack::type::MAP:
				if (pos >= _body->_obj->via.map.size) {
					throw std::out_of_range("The map only contains " + std::to_string(_body->_obj->via.map.size) + " elements");
				}
				return _at(std::string(_body->_obj->via.map.ptr[pos].key.via.str.ptr, _body->_obj->via.map.ptr[pos].key.via.str.size));
			case msgpack::type::ARRAY:
				return *_body->array.at(pos);
			default:
				throw msgpack::type_error();
		}
	}

	MsgPack& _at_or_create(const std::string& key) {
		try {
			return _at(key);
		} catch (const std::out_of_range&) {
		}
		return put(key, nullptr);
	}

	const MsgPack& _at_or_create(const std::string& key) const {
		try {
			return _at(key);
		} catch (const std::out_of_range&) {
			return nil();
		}
		throw msgpack::type_error();
	}

	MsgPack& _at_or_create(size_t pos) {
		try {
			return _at(pos);
		} catch (const std::out_of_range&) {
		}
		return put(pos, nullptr);
	}

	const MsgPack& _at_or_create(size_t pos) const {
		try {
			return _at(pos);
		} catch (const std::out_of_range&) {
			return nil();
		}
		throw msgpack::type_error();
	}

	MsgPack& _find(const std::string& key) const {
		switch (_body->_obj->type) {
			case msgpack::type::NIL:
				throw std::out_of_range("nil");
			case msgpack::type::MAP:
				return *_body->map.at(key).second;
			default:
				throw msgpack::type_error();
		}
	}

	MsgPack& _find(size_t pos) const {
		switch (_body->_obj->type) {
			case msgpack::type::NIL:
				throw std::out_of_range("nil");
			case msgpack::type::ARRAY:
				return *_body->array.at(pos);
			default:
				throw msgpack::type_error();
		}
	}

	MsgPack& _erase(const std::string& key) {
		switch (_body->_obj->type) {
			case msgpack::type::NIL:
				throw std::out_of_range("nil");
			case msgpack::type::MAP: {
				auto it = _body->map.find(key);
				if (it == _body->map.end()) {
					throw std::out_of_range("Key not found");
				}
				auto& mobj = it->second.second;
				auto pos = mobj->_body->_pos;
				auto p = &_body->_obj->via.map.ptr[pos];
				std::memcpy(p, p + 1, (_body->_obj->via.map.size - pos - 1) * sizeof(msgpack::object_kv));
				--_body->_obj->via.map.size;
				// Unlink element (in case someone else hase it):
				mobj->_body->_pos = 0;
				mobj->_body->_key.reset();
				mobj->_body->_parent.reset();
				// Erase from map:
				_body->map.erase(it);
				_update_map(pos);
				try {
					return *_body->map.at(std::string(p->key.via.str.ptr, p->key.via.str.size)).second;
				} catch (const std::out_of_range&) {
					throw final_on_range("Final element erased");
				}
			}
			default:
				throw msgpack::type_error();
		}
	}

	MsgPack& _erase(size_t pos) {
		switch (_body->_obj->type) {
			case msgpack::type::NIL:
				throw std::out_of_range("nil");
			case msgpack::type::MAP:
				if (pos >= _body->_obj->via.map.size) {
					throw std::out_of_range("The map only contains " + std::to_string(_body->_obj->via.map.size) + " elements");
				}
				return _erase(std::string(_body->_obj->via.map.ptr[pos].key.via.str.ptr, _body->_obj->via.map.ptr[pos].key.via.str.size));
			case msgpack::type::ARRAY: {
				auto it = _body->array.begin() + pos;
				if (it >= _body->array.end()) {
					throw std::out_of_range("Position not found");
				}
				auto& mobj = *it;
				auto pos = mobj->_body->_pos;
				auto p = &_body->_obj->via.array.ptr[pos];
				std::memcpy(p, p + 1, (_body->_obj->via.array.size - pos - 1) * sizeof(msgpack::object));
				--_body->_obj->via.array.size;
				// Unlink element (in case someone else hase it):
				mobj->_body->_pos = 0;
				mobj->_body->_key.reset();
				mobj->_body->_parent.reset();
				// Erase from map:
				_body->array.erase(it);
				_update_array(pos);
				try {
					return *_body->array.at(pos);
				} catch (const std::out_of_range&) {
					throw final_on_range("Final element erased");
				}
			}
			default:
				throw msgpack::type_error();
		}
	}

	template <typename T>
	static auto& _path(T current, const std::vector<std::string>& path) {
		for (const auto& s : path) {
			switch (current->_body->_obj->type) {
				case msgpack::type::MAP:
					try {
						current = &current->at(s);
					} catch (const std::out_of_range&) {
						throw std::out_of_range("The map must contain an object at key:" + s);
					}
					break;

				case msgpack::type::ARRAY: {
					std::string::size_type sz;
					int pos = std::stoi(s, &sz);
					if (pos < 0 || sz != s.size()) {
						throw std::invalid_argument("The index for the array must be a positive integer, it is: " + s);
					}
					try {
						current = &current->at(pos);
					} catch (const std::out_of_range&) {
						throw std::out_of_range(("The array must contain an object at index: " + s).c_str());
					}
					break;
				}

				default:
					throw std::invalid_argument(("The container must be a map or an array to access: " + s).c_str());
			}
		}

		return *current;
	}

	template <typename T>
	MsgPack& _put(const std::string& key, T&& val) {
		switch (_body->_obj->type) {
			case msgpack::type::NIL:
				_body->_obj->type = msgpack::type::MAP;
				_body->_obj->via.map.ptr = nullptr;
				_body->_obj->via.map.size = 0;
			case msgpack::type::MAP: {
				_reserve_map(_body->_obj->via.map.size + 1);

				auto p = &_body->_obj->via.map.ptr[_body->_obj->via.map.size];
				p->key.type = msgpack::type::STR;
				p->key.via.str.size = static_cast<uint32_t>(key.size());
				auto ptr = static_cast<char*>(_body->_zone->allocate_align(p->key.via.str.size));
				std::memcpy(ptr, key.data(), p->key.via.str.size);
				p->key.via.str.ptr = ptr;
				p->val = msgpack::object(std::forward<T>(val), *_body->_zone);
				++_body->_obj->via.map.size;
				return *_init_map(static_cast<size_t>(_body->map.size()));
			}
			default:
				throw msgpack::type_error();
		}
	}

	template <typename T>
	MsgPack& _put(size_t pos, T&& val) {
		switch (_body->_obj->type) {
			case msgpack::type::NIL:
				_body->_obj->type = msgpack::type::ARRAY;
				_body->_obj->via.array.ptr = nullptr;
				_body->_obj->via.array.size = 0;
			case msgpack::type::ARRAY: {
				_reserve_array(pos + 1);

				// Initialize new elements.
				auto plast = &_body->_obj->via.array.ptr[pos];
				auto p = &_body->_obj->via.array.ptr[_body->_obj->via.array.size];
				for (; p != plast; ++p) {
					p->type = msgpack::type::NIL;
					++_body->_obj->via.array.size;
				}
				*p = msgpack::object(std::forward<T>(val), *_body->_zone);
				++_body->_obj->via.array.size;
				return *_init_array(static_cast<size_t>(_body->array.size()));
			}
			default:
				throw msgpack::type_error();
		}
	}

public:
	MsgPack& path(const std::vector<std::string>& path) {
		return _path(this, path);
	}

	const MsgPack& path(const std::vector<std::string>& path) const {
		return _path(this, path);
	}

	template <typename T>
	MsgPack& put(const MsgPack& o, T&& val) {
		switch (o._body->_obj->type) {
			case msgpack::type::STR:
				return _put(std::string(o._body->_obj->via.str.ptr, o._body->_obj->via.str.size), std::forward<T>(val));
			case msgpack::type::NEGATIVE_INTEGER:
				if (o._body->_obj->via.i64 < 0) throw msgpack::type_error();
				return _put(static_cast<size_t>(o._body->_obj->via.i64), std::forward<T>(val));
			case msgpack::type::POSITIVE_INTEGER:
				return _put(static_cast<size_t>(o._body->_obj->via.u64), std::forward<T>(val));
			default:
				throw msgpack::type_error();
		}
	}


	template <typename T>
	MsgPack& push_back(T&& v) {
		return _put(size(), std::forward<T>(v));
	}

	const msgpack::object& internal_msgpack() const {
		return *_body->_obj;
	}

    template <typename T, typename = std::enable_if_t<std::is_constructible<MsgPack, T>::value>>
	std::pair<iterator, bool> insert(T&& v) {
		MsgPack o(v);
		switch (o._body->_obj->type) {
			case msgpack::type::ARRAY:
				if (o._body->_obj->via.array.size % 2) {
					break;
				}
				for (auto it = o.begin(); it != o.end(); ++it) {
					auto key = *it++;
					auto val = *it;
					if (key._body->_obj->type != msgpack::type::STR) {
						break;
					}
					put(key, val);
				}

				break;
			case msgpack::type::MAP:
				for (auto& key : o) {
					auto val = o.at(key);
					put(key, val);
				}
				break;
			default:
				break;
		}
		return std::make_pair(end(), false);
	}

	MsgPack& operator[](const MsgPack& o) {
		switch (o._body->_obj->type) {
			case msgpack::type::STR:
				return _at_or_create(std::string(o._body->_obj->via.str.ptr, o._body->_obj->via.str.size));
			case msgpack::type::NEGATIVE_INTEGER:
				if (o._body->_obj->via.i64 < 0) throw msgpack::type_error();
				return _at_or_create(static_cast<size_t>(o._body->_obj->via.i64));
			case msgpack::type::POSITIVE_INTEGER:
				return _at_or_create(static_cast<size_t>(o._body->_obj->via.u64));
			default:
				throw msgpack::type_error();
		}
	}

	const MsgPack& operator[](const MsgPack& o) const {
		switch (o._body->_obj->type) {
			case msgpack::type::STR:
				return _at_or_create(std::string(o._body->_obj->via.str.ptr, o._body->_obj->via.str.size));
			case msgpack::type::NEGATIVE_INTEGER:
				if (o._body->_obj->via.i64 < 0) throw msgpack::type_error();
				return _at_or_create(static_cast<size_t>(o._body->_obj->via.i64));
			case msgpack::type::POSITIVE_INTEGER:
				return _at_or_create(static_cast<size_t>(o._body->_obj->via.u64));
			default:
				throw msgpack::type_error();
		}
	}

	MsgPack& at(const MsgPack& o) {
		switch (o._body->_obj->type) {
			case msgpack::type::STR:
				return _at(std::string(o._body->_obj->via.str.ptr, o._body->_obj->via.str.size));
			case msgpack::type::NEGATIVE_INTEGER:
				if (o._body->_obj->via.i64 < 0) throw msgpack::type_error();
				return _at(static_cast<size_t>(o._body->_obj->via.i64));
			case msgpack::type::POSITIVE_INTEGER:
				return _at(static_cast<size_t>(o._body->_obj->via.u64));
			default:
				throw msgpack::type_error();
		}
	}

	const MsgPack& at(const MsgPack& o) const {
		switch (o._body->_obj->type) {
			case msgpack::type::STR:
				return _at(std::string(o._body->_obj->via.str.ptr, o._body->_obj->via.str.size));
			case msgpack::type::NEGATIVE_INTEGER:
				if (o._body->_obj->via.i64 < 0) throw msgpack::type_error();
				return _at(static_cast<size_t>(o._body->_obj->via.i64));
			case msgpack::type::POSITIVE_INTEGER:
				return _at(static_cast<size_t>(o._body->_obj->via.u64));
			default:
				throw msgpack::type_error();
		}
	}

	iterator find(const MsgPack& o) {
		try {
			switch (o._body->_obj->type) {
				case msgpack::type::STR:
					return iterator(this, _find(std::string(o._body->_obj->via.str.ptr, o._body->_obj->via.str.size))._body->_pos);
				case msgpack::type::NEGATIVE_INTEGER:
					if (o._body->_obj->via.i64 < 0) throw msgpack::type_error();
					return iterator(this, _find(static_cast<size_t>(o._body->_obj->via.i64))._body->_pos);
				case msgpack::type::POSITIVE_INTEGER:
					return iterator(this, _find(static_cast<size_t>(o._body->_obj->via.u64))._body->_pos);
				default:
					throw msgpack::type_error();
			}
		} catch (const std::out_of_range&) {
			return end();
		}
	}

	const_iterator find(const MsgPack& o) const {
		try {
			switch (o._body->_obj->type) {
				case msgpack::type::STR:
					return const_iterator(this, _find(std::string(o._body->_obj->via.str.ptr, o._body->_obj->via.str.size))._body->_pos);
				case msgpack::type::NEGATIVE_INTEGER:
					if (o._body->_obj->via.i64 < 0) throw msgpack::type_error();
					return const_iterator(this, _find(static_cast<size_t>(o._body->_obj->via.i64))._body->_pos);
				case msgpack::type::POSITIVE_INTEGER:
					return const_iterator(this, _find(static_cast<size_t>(o._body->_obj->via.u64))._body->_pos);
				default:
					throw msgpack::type_error();
			}
		} catch (const std::out_of_range&) {
			return cend();
		}
	}

	inline MsgPack& parent() {
		return *_body->_parent;
	}

	inline const MsgPack& parent() const {
		return *_body->_parent;
	}

	size_t count(const MsgPack& o) const {
		return (find(o) == end()) ? 0 : 1;
	}

	size_t erase(const MsgPack& o) {
		try {
			switch (o._body->_obj->type) {
				case msgpack::type::STR:
					iterator(this, _erase(std::string(o._body->_obj->via.str.ptr, o._body->_obj->via.str.size))._body->_pos);
					return 1;
				case msgpack::type::NEGATIVE_INTEGER:
					if (o._body->_obj->via.i64 < 0) throw msgpack::type_error();
					iterator(this, _erase(static_cast<size_t>(o._body->_obj->via.i64))._body->_pos);
					return 1;
				case msgpack::type::POSITIVE_INTEGER:
					iterator(this, _erase(static_cast<size_t>(o._body->_obj->via.u64))._body->_pos);
					return 1;
				default:
					throw msgpack::type_error();
			}
		} catch (const final_on_range&) {
			return 1;
		} catch (const std::out_of_range&) {
			return 0;
		}
	}

	iterator erase(const iterator& it) {
		try {
			return iterator(this, _erase(it._off)._body->_pos);
		} catch (const std::out_of_range&) {
			return end();
		}
	}

	void clear() noexcept {
		switch (_body->_obj->type) {
			case msgpack::type::MAP:
				_body->_obj->via.map.size = 0;
				_body->map.clear();
				break;
			case msgpack::type::ARRAY:
				_body->_obj->via.array.size = 0;
				_body->array.clear();
				break;
			case msgpack::type::STR:
				_body->_obj->via.str.size = 0;
				break;
			default:
				break;
		}
	}

	explicit operator bool() const {
		switch (_body->_obj->type) {
			case msgpack::type::NIL:
				return false;
			case msgpack::type::MAP:
			case msgpack::type::ARRAY:
			case msgpack::type::STR:
				return size() > 0;
			case msgpack::type::NEGATIVE_INTEGER:
				return _body->_obj->via.i64 != 0;
			case msgpack::type::POSITIVE_INTEGER:
				return _body->_obj->via.u64 != 0;
			case msgpack::type::FLOAT:
				return _body->_obj->via.f64 != 0;
			case msgpack::type::BOOLEAN:
				return _body->_obj->via.boolean;
			default:
				return false;
		}
	}

	void reserve(size_t n) {
		switch (_body->_obj->type) {
			case msgpack::type::MAP:
				return _reserve_map(n);
			case msgpack::type::ARRAY:
				return _reserve_array(n);
			default:
				return;
		}
	}

	size_t capacity() const noexcept {
		return _body->_capacity;
	}

	size_t size() const noexcept {
		switch (_body->_obj->type) {
			case msgpack::type::MAP:
				return _body->_obj->via.map.size;
			case msgpack::type::ARRAY:
				return _body->_obj->via.array.size;
			case msgpack::type::STR:
				return _body->_obj->via.str.size;
			default:
				return 0;
		}
	}

	bool empty() const noexcept {
		switch (_body->_obj->type) {
			case msgpack::type::MAP:
				return _body->_obj->via.map.size == 0;
			case msgpack::type::ARRAY:
				return _body->_obj->via.array.size = 0;
			case msgpack::type::STR:
				return _body->_obj->via.str.size = 0;
			default:
				return false;
		}
	}

	uint64_t as_u64() const {
		switch (_body->_obj->type) {
			case msgpack::type::NEGATIVE_INTEGER:
				return _body->_obj->via.i64;
			case msgpack::type::POSITIVE_INTEGER:
				return _body->_obj->via.u64;
			default:
				throw msgpack::type_error();
		}
	}

	int64_t as_i64() const {
		switch (_body->_obj->type) {
			case msgpack::type::NEGATIVE_INTEGER:
				return _body->_obj->via.i64;
			case msgpack::type::POSITIVE_INTEGER:
				return _body->_obj->via.u64;
			default:
				throw msgpack::type_error();
		}
	}

	double as_f64() const {
		switch (_body->_obj->type) {
			case msgpack::type::NEGATIVE_INTEGER:
				return _body->_obj->via.i64;
			case msgpack::type::POSITIVE_INTEGER:
				return _body->_obj->via.u64;
			case msgpack::type::FLOAT:
				return _body->_obj->via.f64;
			default:
				throw msgpack::type_error();
		}
	}

	std::string as_string() const {
		switch (_body->_obj->type) {
			case msgpack::type::STR:
				return std::string(_body->_obj->via.str.ptr, _body->_obj->via.str.size);
			default:
				throw msgpack::type_error();
		}
	}

	bool as_bool() const {
		switch (_body->_obj->type) {
			case msgpack::type::BOOLEAN:
				return _body->_obj->via.boolean;
			default:
				throw msgpack::type_error();
		}
	}

	rapidjson::Document as_document() const {
		rapidjson::Document doc;
		_body->_obj->convert(&doc);
		return doc;
	}

	bool is_null() const {
		return _body->_obj->type == msgpack::type::NIL;
	}

	bool is_boolean() const {
		return _body->_obj->type == msgpack::type::BOOLEAN;
	}

	bool is_number() const {
		switch (_body->_obj->type) {
			case msgpack::type::NEGATIVE_INTEGER:
			case msgpack::type::POSITIVE_INTEGER:
			case msgpack::type::FLOAT:
				return true;
			default:
				return false;
		}
	}

	bool is_map() const {
		return _body->_obj->type == msgpack::type::MAP;
	}

	bool is_array() const {
		return _body->_obj->type == msgpack::type::ARRAY;
	}

	bool is_string() const {
		return _body->_obj->type == msgpack::type::STR;
	}

	msgpack::type::object_type type() const {
		return _body->_obj->type;
	}

	bool operator==(const MsgPack& other) const {
		return *_body->_obj == *other._body->_obj;
	}

	bool operator!=(const MsgPack& other) const {
		return *_body->_obj != *other._body->_obj;
	}

	MsgPack operator +(long val) {
		MsgPack o(_body);
		switch (_body->_obj->type) {
			case msgpack::type::NEGATIVE_INTEGER:
				o._body->_obj->via.i64 += val;
				return o;
			case msgpack::type::POSITIVE_INTEGER:
				o._body->_obj->via.u64 += val;
				return o;
			case msgpack::type::FLOAT:
				o._body->_obj->via.f64 += val;
				return o;
			default:
				throw msgpack::type_error();
		}
	}

	MsgPack& operator +=(long val) {
		switch (_body->_obj->type) {
			case msgpack::type::NEGATIVE_INTEGER:
				_body->_obj->via.i64 += val;
				return *this;
			case msgpack::type::POSITIVE_INTEGER:
				_body->_obj->via.u64 += val;
				return *this;
			case msgpack::type::FLOAT:
				_body->_obj->via.f64 += val;
				return *this;
			default:
				throw msgpack::type_error();
		}
	}

	std::string to_string(bool prettify=false) const {
		if (prettify) {
			rapidjson::Document doc = as_document();
			rapidjson::StringBuffer buffer;
			rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
			doc.Accept(writer);
			return std::string(buffer.GetString(), buffer.GetSize());
		} else {
			std::ostringstream oss;
			oss << *_body->_obj;
			return oss.str();
		}
	}

	std::ostream& operator<<(std::ostream& s) const {
		s << *_body->_obj;
		return s;
	}

	inline std::string serialise() const {
		msgpack::sbuffer sbuf;
		msgpack::pack(&sbuf, *_body->_obj);
		return std::string(sbuf.data(), sbuf.size());
	}

	inline static MsgPack unserialise(const std::string& s) {
		auto obj = msgpack::unpack(s.data(), s.size()).get();
		return MsgPack(msgpack::unpack(s.data(), s.size()).get());
	}

	friend msgpack::adaptor::convert<MsgPack>;
	friend msgpack::adaptor::pack<MsgPack>;
	friend msgpack::adaptor::object<MsgPack>;
	friend msgpack::adaptor::object_with_zone<MsgPack>;
	friend msgpack::adaptor::object<std::nullptr_t>;
	friend msgpack::adaptor::object_with_zone<std::nullptr_t>;
};


inline static std::ostream& operator<<(std::ostream& s, const MsgPack& o){
	return o.operator<<(s);
}


namespace msgpack {
	MSGPACK_API_VERSION_NAMESPACE(MSGPACK_DEFAULT_API_NS) {
		namespace adaptor {
			////////////////////////////////////////////////////////////////////
			// convert

			template <>
			struct convert<MsgPack> {
				const msgpack::object& operator()(const msgpack::object& o, MsgPack& v) const {
						v = MsgPack(o);
						return o;
				}
			};

			////////////////////////////////////////////////////////////////////
			// pack

			template <>
			struct pack<MsgPack> {
				template <typename Stream>
				msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& o, const MsgPack& v) const {
					o << v;
					return o;
				}
			};

			////////////////////////////////////////////////////////////////////
			// object and object_with_zone

			template <>
			struct object<MsgPack> {
				void operator()(msgpack::object& o, const MsgPack& v) const {
					o = msgpack::object(*v._body->_obj);
				}
			};

			template <>
			struct object_with_zone<MsgPack> {
				void operator()(msgpack::object::with_zone& o, const MsgPack& v) const {
					msgpack::object obj(*v._body->_obj, o.zone);
					o.type = obj.type;
					o.via = obj.via;
				}
			};

			template <>
			struct object<std::nullptr_t> {
				void operator()(msgpack::object& o, std::nullptr_t) const {
					msgpack::object nil;
					nil.type = msgpack::type::NIL;
					o = msgpack::object(nil);
				}
			};

			template <>
			struct object_with_zone<std::nullptr_t> {
				void operator()(msgpack::object::with_zone& o, std::nullptr_t) const {
					msgpack::object nil;
					nil.type = msgpack::type::NIL;
					msgpack::object obj(nil, o.zone);
					o.type = obj.type;
					o.via = obj.via;
				}
			};
		} // namespace adaptor
	} // MSGPACK_API_VERSION_NAMESPACE(MSGPACK_DEFAULT_API_NS)
} // namespace msgpack
