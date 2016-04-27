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
	struct Body;

	class last_in_range : public std::out_of_range {
		using std::out_of_range::out_of_range;
	};

	std::shared_ptr<Body> _body;

public:
	class duplicate_key : public std::out_of_range {
		using std::out_of_range::out_of_range;
	};

	template <typename T>
	class Iterator;

	using iterator = Iterator<MsgPack>;
	using const_iterator = Iterator<const MsgPack>;

	iterator begin();
	const_iterator begin() const;
	const_iterator cbegin() const;
	iterator end();
	const_iterator end() const;
	const_iterator cend() const;

private:
	MsgPack(const std::shared_ptr<Body>& b);
	MsgPack(std::shared_ptr<Body>&& b);

	void _initializer_array(const std::initializer_list<MsgPack>& list);
	void _initializer_map(const std::initializer_list<MsgPack>& list);
	void _initializer(const std::initializer_list<MsgPack>& list);

public:
	MsgPack();
	MsgPack(const MsgPack& other);
	MsgPack(MsgPack&& other);
	MsgPack(const std::initializer_list<MsgPack>& list);

	template <typename T, typename = std::enable_if_t<not std::is_same<std::shared_ptr<Body>, std::decay_t<T>>::value>>
	MsgPack(T&& v);

	MsgPack& operator=(const MsgPack& other);
	MsgPack& operator=(MsgPack&& other);
	MsgPack& operator=(const std::initializer_list<MsgPack>& list);

	template <typename T>
	MsgPack& operator=(T&& v);

private:
	MsgPack* _init_map(size_t pos) const;
	void _update_map(size_t pos) const;

	MsgPack* _init_array(size_t pos) const;
	void _update_array(size_t pos) const;

	void _init() const;

	void _reserve_map(size_t rsize);
	void _reserve_array(size_t rsize);

	MsgPack& _at(const std::string& key) const;
	MsgPack& _at(size_t pos) const;

	MsgPack& _at_or_create(const std::string& key);
	const MsgPack& _at_or_create(const std::string& key) const;
	MsgPack& _at_or_create(size_t pos);
	const MsgPack& _at_or_create(size_t pos) const;

	MsgPack& _find(const std::string& key) const;
	MsgPack& _find(size_t pos) const;

	MsgPack& _erase(const std::string& key);
	MsgPack& _erase(size_t pos);

	template <typename T>
	static auto& _path(T current, const std::vector<std::string>& path);

	template <typename T>
	MsgPack& _put(const std::string& key, T&& val);

	template <typename T>
	MsgPack& _put(size_t pos, T&& val);

	template <typename T>
	MsgPack::iterator _insert(size_t pos, T&& val);

public:
	MsgPack& path(const std::vector<std::string>& path);
	const MsgPack& path(const std::vector<std::string>& path) const;

	template <typename M, typename T, typename = std::enable_if_t<std::is_same<MsgPack, std::decay_t<M>>::value>>
	MsgPack& put(const M& o, T&& val);
	template <typename T>
	MsgPack& put(const std::string& s, T&& val);
	template <typename T>
	MsgPack& put(size_t pos, T&& val);

	template <typename M, typename T, typename = std::enable_if_t<std::is_same<MsgPack, std::decay_t<M>>::value>>
	MsgPack::iterator insert(const M& o, T&& val);
	template <typename T>
	MsgPack::iterator insert(const std::string& s, T&& val);
	template <typename T>
	MsgPack::iterator insert(size_t pos, T&& val);

	template <typename T>
	MsgPack& push_back(T&& v);

	inline const msgpack::object& internal_msgpack() const;

	template <typename T, typename = std::enable_if_t<std::is_constructible<MsgPack, T>::value>>
	std::pair<iterator, bool> insert(T&& v);

	template <typename M, typename = std::enable_if_t<std::is_same<MsgPack, std::decay_t<M>>::value>>
	MsgPack& operator[](const M& o);
	template <typename M, typename = std::enable_if_t<std::is_same<MsgPack, std::decay_t<M>>::value>>
	const MsgPack& operator[](const M& o) const;
	MsgPack& operator[](const std::string& s);
	const MsgPack& operator[](const std::string& s) const;
	MsgPack& operator[](size_t pos);
	const MsgPack& operator[](size_t pos) const;

	template <typename M, typename = std::enable_if_t<std::is_same<MsgPack, std::decay_t<M>>::value>>
	MsgPack& at(const M& o);
	template <typename M, typename = std::enable_if_t<std::is_same<MsgPack, std::decay_t<M>>::value>>
	const MsgPack& at(const M& o) const;
	MsgPack& at(const std::string& s);
	const MsgPack& at(const std::string& s) const;
	MsgPack& at(size_t pos);
	const MsgPack& at(size_t pos) const;

	template <typename M, typename = std::enable_if_t<std::is_same<MsgPack, std::decay_t<M>>::value>>
	iterator find(const M& o);
	template <typename M, typename = std::enable_if_t<std::is_same<MsgPack, std::decay_t<M>>::value>>
	const_iterator find(const M& o) const;
	iterator find(const std::string& s);
	const_iterator find(const std::string& s) const;
	iterator find(size_t pos);
	const_iterator find(size_t pos) const;

	inline MsgPack& parent();
	inline const MsgPack& parent() const;

	size_t count(const MsgPack& o) const;

	template <typename M, typename = std::enable_if_t<std::is_same<MsgPack, std::decay_t<M>>::value>>
	size_t erase(const M& o);
	size_t erase(const std::string& s);
	size_t erase(size_t pos);
	iterator erase(const iterator& it);

	void clear() noexcept;

	explicit operator bool() const;

	void reserve(size_t n);

	size_t capacity() const noexcept;

	size_t size() const noexcept;

	bool empty() const noexcept;

	uint64_t as_u64() const;
	int64_t as_i64() const;
	double as_f64() const;
	std::string as_string() const;
	bool as_bool() const;
	rapidjson::Document as_document() const;

	bool is_null() const;
	bool is_boolean() const;
	bool is_number() const;
	bool is_map() const;
	bool is_array() const;
	bool is_string() const;

	msgpack::type::object_type type() const;

	bool operator ==(const MsgPack& other) const;
	bool operator !=(const MsgPack& other) const;
	MsgPack operator +(long val);
	MsgPack& operator +=(long val);
	std::ostream& operator <<(std::ostream& s) const;

	std::string to_string(bool prettify=false) const;

	std::string serialise() const;
	static MsgPack unserialise(const std::string& s);

	friend msgpack::adaptor::convert<MsgPack>;
	friend msgpack::adaptor::pack<MsgPack>;
	friend msgpack::adaptor::object<MsgPack>;
	friend msgpack::adaptor::object_with_zone<MsgPack>;
	friend msgpack::adaptor::object<std::nullptr_t>;
	friend msgpack::adaptor::object_with_zone<std::nullptr_t>;
};

template <typename T>
class MsgPack::Iterator : public std::iterator<std::input_iterator_tag, MsgPack> {
	friend MsgPack;

	T* _mobj;
	off_t _off;

public:
	Iterator(T* o, off_t off)
	: _mobj(o),
	_off(off) {
		assert(_off >= 0);
	}

	Iterator(const Iterator& it)
	: _mobj(it._mobj),
	_off(it._off) {
		assert(_off >= 0);
	}

	Iterator& operator ++() {
		++_off;
		return *this;
	}

	Iterator operator ++(int) {
		Iterator tmp(*this);
		++_off;
		return tmp;
	}

	Iterator& operator +=(int _off) {
		_off += _off;
		return *this;
	}

	Iterator operator +(int _off) const {
		Iterator tmp(*this);
		tmp._off += _off;
		return tmp;
	}

	Iterator operator =(const Iterator& other) {
		_mobj = other._mobj;
		_off = other._off;
		return *this;
	}

	T& operator *() const {
		switch (_mobj->_body->_obj->type) {
			case msgpack::type::MAP:
				return _mobj->at(_off)._body->_key;
			case msgpack::type::ARRAY:
				return _mobj->at(_off);
			default:
				throw msgpack::type_error();
		}
	}

	bool operator ==(const Iterator& other) const {
		return *_mobj == *other._mobj && _off == other._off;
	}

	bool operator !=(const Iterator& other) const {
		return !operator==(other);
	}
};

struct MsgPack::Body {
	std::unordered_map<std::string, std::pair<MsgPack, MsgPack>> map;
	std::vector<MsgPack> array;

	ssize_t _capacity;

	const std::shared_ptr<msgpack::zone> _zone;
	const std::shared_ptr<msgpack::object> _base;
	MsgPack _parent;
	bool _is_key;
	size_t _pos;
	MsgPack _key;
	msgpack::object* _obj;

	const MsgPack _nil;

	Body(
		 const std::shared_ptr<msgpack::zone>& zone,
		 const std::shared_ptr<msgpack::object>& base,
		 const std::shared_ptr<Body>& parent,
		 bool is_key,
		 size_t pos,
		 const std::shared_ptr<Body>& key,
		 msgpack::object* obj
		 ) : _capacity(-1),
	_zone(zone),
	_base(base),
	_parent(parent),
	_is_key(is_key),
	_pos(pos),
	_key(key),
	_obj(obj),
	_nil(std::make_shared<Body>(_zone, _base, _parent._body, nullptr))  // FIXME: _parent._body here should be shared_from_this()
	{ }

	Body(
		 const std::shared_ptr<msgpack::zone>& zone,
		 const std::shared_ptr<msgpack::object>& base,
		 const std::shared_ptr<Body>& parent,
		 msgpack::object* obj
		 ) : _capacity(-1),
	_zone(zone),
	_base(base),
	_parent(parent),
	_is_key(false),
	_pos(-1),
	_key(std::shared_ptr<Body>()),
	_obj(obj),
	_nil(std::shared_ptr<Body>())  // FIXME: _parent._body here should be shared_from_this()
	{ }

	template <typename T>
	Body(T&& v) :
	_capacity(-1),
	_zone(std::make_shared<msgpack::zone>()),
	_base(std::make_shared<msgpack::object>(std::forward<T>(v), *_zone)),
	_parent(std::shared_ptr<Body>()),
	_is_key(false),
	_pos(-1),
	_key(std::shared_ptr<Body>()),
	_obj(_base.get()),
	_nil(std::make_shared<Body>(_zone, _base, _parent._body, nullptr))  // FIXME: _parent._body here should be shared_from_this()
	{ }

	MsgPack& at(size_t pos) {
		auto& o = array.at(pos);
		o._init();
		return o;
	}

	const MsgPack& at(size_t pos) const {
		auto& o = array.at(pos);
		o._init();
		return o;
	}

	MsgPack& at(const std::string& key) {
		auto& o = map.at(key).second;
		o._init();
		return o;
	}

	const MsgPack& at(const std::string& key) const {
		auto& o = map.at(key).second;
		o._init();
		return o;
	}

	std::unordered_map<std::string, std::pair<MsgPack, MsgPack>>::iterator find(const std::string& key) {
		auto it = map.find(key);
		it->second.second._init();
		return it;
	}

	std::unordered_map<std::string, std::pair<MsgPack, MsgPack>>::const_iterator find(const std::string& key) const {
		auto it = map.find(key);
		it->second.second._init();
		return it;
	}
};


inline MsgPack::iterator MsgPack::begin() {
	return iterator(this, 0);
}

inline MsgPack::const_iterator MsgPack::begin() const {
	return const_iterator(this, 0);
}

inline MsgPack::const_iterator MsgPack::cbegin() const {
	return const_iterator(this, 0);
}

inline MsgPack::iterator MsgPack::end() {
	return iterator(this, size());
}

inline MsgPack::const_iterator MsgPack::end() const {
	return const_iterator(this, size());
}

inline MsgPack::const_iterator MsgPack::cend() const {
	return const_iterator(this, size());
}

inline MsgPack::MsgPack(const std::shared_ptr<MsgPack::Body>& b)
: _body(b) { }

inline MsgPack::MsgPack(std::shared_ptr<MsgPack::Body>&& b)
: _body(std::move(b)) { }

inline void MsgPack::_initializer_array(const std::initializer_list<MsgPack>& list) {
	if (_body->_obj->type != msgpack::type::ARRAY) {
		_body->_capacity = 0;
	}
	_body->array.clear();
	_body->_obj->type = msgpack::type::ARRAY;
	_body->_obj->via.array.ptr = nullptr;
	_body->_obj->via.array.size = 0;
	_reserve_array(list.size());
	for (const auto& val : list) {
		push_back(val);
	}
}

inline void MsgPack::_initializer_map(const std::initializer_list<MsgPack>& list) {
	if (_body->_obj->type != msgpack::type::MAP) {
		_body->_capacity = 0;
	}
	_body->map.clear();
	_body->_obj->type = msgpack::type::MAP;
	_body->_obj->via.map.ptr = nullptr;
	_body->_obj->via.map.size = 0;
	_reserve_map(list.size());
	for (const auto& val : list) {
		put(val.at(0), val.at(1));
	}
}

inline void MsgPack::_initializer(const std::initializer_list<MsgPack>& list) {
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


inline MsgPack::MsgPack() : MsgPack(nullptr) { }


inline MsgPack::MsgPack(const MsgPack& other)
	: _body(std::make_shared<MsgPack::Body>(other))
{
	_init();
}


inline MsgPack::MsgPack(MsgPack&& other) : _body(std::move(other._body)) { }


template <typename T, typename>
inline MsgPack::MsgPack(T&& v)
: _body(std::make_shared<MsgPack::Body>(std::forward<T>(v))) {
	_init();
}

inline MsgPack::MsgPack(const std::initializer_list<MsgPack>& list)
: MsgPack() {
	_initializer(list);
}


inline MsgPack& MsgPack::operator=(const MsgPack& other) {
	clear();
	*_body->_obj = msgpack::object(other, *_body->_zone);
	_init();
	return *this;
}


inline MsgPack& MsgPack::operator=(MsgPack&& other) {
	clear();
	*_body->_obj = msgpack::object(std::move(other), *_body->_zone);
	_init();
	return *this;
}


inline MsgPack& MsgPack::operator=(const std::initializer_list<MsgPack>& list) {
	_initializer(list);
	return *this;
}

template <typename T>
inline MsgPack& MsgPack::operator=(T&& v) {
	auto obj = msgpack::object(std::forward<T>(v), *_body->_zone);
	if (_body->_is_key) {
		if (obj.type != msgpack::type::STR) {
			throw msgpack::type_error();
		}
		// Change key in the parent's map:
		auto val = std::string(_body->_obj->via.str.ptr, _body->_obj->via.str.size);
		auto str_key = std::string(obj.via.str.ptr, obj.via.str.size);
		if (str_key == val) {
			return *this;
		}
		auto it = _body->_parent._body->find(val);
		if (it != _body->_parent._body->map.end()) {
			if (_body->_parent._body->map.insert(std::make_pair(str_key, std::move(it->second))).second) {
				_body->_parent._body->map.erase(it);
			} else {
				throw duplicate_key("Duplicate key: " + str_key);
			}
			assert(_body->_parent._body->_obj->via.map.size == _body->_parent._body->map.size());
		}
	}
	*_body->_obj = obj;
	_init();
	return *this;
}

inline MsgPack* MsgPack::_init_map(size_t pos) const {
	MsgPack* ret = nullptr;
	_body->map.reserve(_body->_capacity);
	const auto pend = &_body->_obj->via.map.ptr[_body->_obj->via.map.size];
	for (auto p = &_body->_obj->via.map.ptr[pos]; p != pend; ++p, ++pos) {
		if (p->key.type != msgpack::type::STR) {
			throw msgpack::type_error();
		}
		auto last_key = MsgPack(std::make_shared<MsgPack::Body>(_body->_zone, _body->_base, _body, true, 0, nullptr, &p->key));
		auto last_val = MsgPack(std::make_shared<MsgPack::Body>(_body->_zone, _body->_base, _body, false, pos, last_key._body, &p->val));
		auto str_key = std::string(p->key.via.str.ptr, p->key.via.str.size);
		auto inserted = _body->map.insert(std::make_pair(str_key, std::make_pair(std::move(last_key), std::move(last_val))));
		if (!inserted.second) {
			throw duplicate_key("Duplicate key: " + str_key);
		}
		ret = &inserted.first->second.second;
	}
	assert(_body->_obj->via.map.size == _body->map.size());
	return ret;
}

inline void MsgPack::_update_map(size_t pos) const {
	const auto pend = &_body->_obj->via.map.ptr[_body->_obj->via.map.size];
	for (auto p = &_body->_obj->via.map.ptr[pos]; p != pend; ++p, ++pos) {
		std::string str_key(p->key.via.str.ptr, p->key.via.str.size);
		auto& elem = _body->find(str_key)->second;
		elem.first._body->_obj = &p->key;
		elem.second._body->_pos = pos;
		elem.second._body->_obj = &p->val;
	}
}

inline MsgPack* MsgPack::_init_array(size_t pos) const {
	if (pos < _body->array.size()) {
		// Destroy the previous objects to update
		_body->array.resize(pos);
	}

	MsgPack* ret = nullptr;
	_body->array.reserve(_body->_capacity);
	const auto pend = &_body->_obj->via.array.ptr[_body->_obj->via.array.size];
	for (auto p = &_body->_obj->via.array.ptr[pos]; p != pend; ++p, ++pos) {
		auto last_val = MsgPack(std::make_shared<MsgPack::Body>(_body->_zone, _body->_base, _body, false, pos, nullptr, p));
		ret = &*_body->array.insert(_body->array.end(), std::move(last_val));
	}
	assert(_body->_obj->via.array.size == _body->array.size());
	return ret;
}

inline void MsgPack::_update_array(size_t pos) const {
	const auto pend = &_body->_obj->via.array.ptr[_body->_obj->via.array.size];
	for (auto p = &_body->_obj->via.array.ptr[pos]; p != pend; ++p, ++pos) {
		auto& mobj = _body->at(pos);
		mobj._body->_pos = pos;
		mobj._body->_obj = p;
	}
}

inline void MsgPack::_init() const {
	switch (_body->_obj->type) {
		case msgpack::type::MAP:
			if (_body->map.size() != _body->_obj->via.map.size) {
				_body->_capacity = _body->_obj->via.map.size;
				_init_map(0);
			}
			break;
		case msgpack::type::ARRAY:
			if (_body->array.size() != _body->_obj->via.array.size) {
				_body->_capacity = _body->_obj->via.array.size;
				_init_array(0);
			}
			break;
		default:
			_body->_capacity = 0;
			break;
	}
}

inline void MsgPack::_reserve_map(size_t rsize) {
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

inline void MsgPack::_reserve_array(size_t rsize) {
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

inline MsgPack& MsgPack::_at(const std::string& key) const {
	switch (_body->_obj->type) {
		case msgpack::type::NIL:
			throw std::out_of_range("nil");
		case msgpack::type::MAP:
			return _body->at(key);
		default:
			throw msgpack::type_error();
	}
}

inline MsgPack& MsgPack::_at(size_t pos) const {
	switch (_body->_obj->type) {
		case msgpack::type::NIL:
			throw std::out_of_range("nil");
		case msgpack::type::MAP:
			if (pos >= _body->_obj->via.map.size) {
				throw std::out_of_range("The map only contains " + std::to_string(_body->_obj->via.map.size) + " elements");
			}
			return _at(std::string(_body->_obj->via.map.ptr[pos].key.via.str.ptr, _body->_obj->via.map.ptr[pos].key.via.str.size));
		case msgpack::type::ARRAY:
			return _body->at(pos);
		default:
			throw msgpack::type_error();
	}
}

inline MsgPack& MsgPack::_at_or_create(const std::string& key) {
	try {
		return _at(key);
	} catch (const std::out_of_range&) { }

	return put(key, nullptr);
}

inline const MsgPack& MsgPack::_at_or_create(const std::string& key) const {
	try {
		return _at(key);
	} catch (const std::out_of_range&) {
		return _body->_nil;
	}
}

inline MsgPack& MsgPack::_at_or_create(size_t pos) {
	try {
		return _at(pos);
	} catch (const std::out_of_range&) { }

	return put(pos, nullptr);
}

inline const MsgPack& MsgPack::_at_or_create(size_t pos) const {
	try {
		return _at(pos);
	} catch (const std::out_of_range&) {
		return _body->_nil;
	}
}

inline MsgPack& MsgPack::_find(const std::string& key) const {
	switch (_body->_obj->type) {
		case msgpack::type::NIL:
			throw std::out_of_range("nil");
		case msgpack::type::MAP:
			return _body->at(key);
		default:
			throw msgpack::type_error();
	}
}

inline MsgPack& MsgPack::_find(size_t pos) const {
	switch (_body->_obj->type) {
		case msgpack::type::NIL:
			throw std::out_of_range("nil");
		case msgpack::type::ARRAY:
			return _body->at(pos);
		default:
			throw msgpack::type_error();
	}
}

inline MsgPack& MsgPack::_erase(const std::string& key) {
	switch (_body->_obj->type) {
		case msgpack::type::NIL:
			throw std::out_of_range("nil");
		case msgpack::type::MAP: {
			auto it = _body->find(key);
			if (it == _body->map.end()) {
				throw std::out_of_range("Key not found");
			}
			auto& mobj = it->second.second;
			auto pos_ = mobj._body->_pos;
			assert(pos_ < _body->_obj->via.map.size);
			auto p = &_body->_obj->via.map.ptr[pos_];
			std::memmove(p, p + 1, (_body->_obj->via.map.size - pos_ - 1) * sizeof(msgpack::object_kv));
			--_body->_obj->via.map.size;
			// Unlink element (in case someone else hase it):
			mobj._body->_pos = -1;
			mobj._body->_key._body.reset();
			mobj._body->_parent._body.reset();
			// Erase from map:
			_body->map.erase(it);
			_update_map(pos_);
			try {
				return _body->at(std::string(p->key.via.str.ptr, p->key.via.str.size));
			} catch (const std::out_of_range&) {
				throw last_in_range("Final element erased");
			}
		}
		default:
			throw msgpack::type_error();
	}
}

inline MsgPack& MsgPack::_erase(size_t pos) {
	switch (_body->_obj->type) {
		case msgpack::type::NIL:
			throw std::out_of_range("nil");
		case msgpack::type::MAP:
			if (pos >= _body->_obj->via.map.size) {
				throw std::out_of_range("The map only contains " + std::to_string(_body->_obj->via.map.size) + " elements");
			}
			return _erase(std::string(_body->_obj->via.map.ptr[pos].key.via.str.ptr, _body->_obj->via.map.ptr[pos].key.via.str.size));
		case msgpack::type::ARRAY: {
			if (pos >= _body->_obj->via.array.size) {
				throw std::out_of_range("The array only contains " + std::to_string(_body->_obj->via.array.size) + " elements");
			}
			auto it = _body->array.begin() + pos;
			assert(it < _body->array.end());
			auto& mobj = *it;
			auto pos_ = mobj._body->_pos;
			assert(pos_ < _body->_obj->via.array.size);
			auto p = &_body->_obj->via.array.ptr[pos_];
			std::memmove(p, p + 1, (_body->_obj->via.array.size - pos_ - 1) * sizeof(msgpack::object));
			--_body->_obj->via.array.size;
			// Unlink element (in case someone else hase it):
			mobj._body->_pos = -1;
			mobj._body->_key._body.reset();
			mobj._body->_parent._body.reset();
			// Erase from map:
			_body->array.erase(it);
			_update_array(pos_);
			try {
				return _body->at(pos_);
			} catch (const std::out_of_range&) {
				throw last_in_range("Final element erased");
			}
		}
		default:
			throw msgpack::type_error();
	}
}

template <typename T>
inline auto& MsgPack::_path(T current, const std::vector<std::string>& path) {
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
inline MsgPack& MsgPack::_put(const std::string& key, T&& val) {
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
			return *_init_map(_body->map.size());
		}
		default:
			throw msgpack::type_error();
	}
}

template <typename T>
inline MsgPack& MsgPack::_put(size_t pos, T&& val) {
	switch (_body->_obj->type) {
		case msgpack::type::NIL:
			_body->_obj->type = msgpack::type::ARRAY;
			_body->_obj->via.array.ptr = nullptr;
			_body->_obj->via.array.size = 0;
		case msgpack::type::ARRAY: {
			if (pos >= _body->_obj->via.array.size) {
				_reserve_array(pos + 1);

				// Initialize new elements.
				const auto plast = &_body->_obj->via.array.ptr[pos];
				auto p = &_body->_obj->via.array.ptr[_body->_obj->via.array.size];
				for (; p != plast; ++p) {
					p->type = msgpack::type::NIL;
					++_body->_obj->via.array.size;
				}

				*p = msgpack::object(std::forward<T>(val), *_body->_zone);
				++_body->_obj->via.array.size;
				return *_init_array(_body->array.size());
			} else {
				auto p = &_body->_obj->via.array.ptr[pos];
				*p = msgpack::object(std::forward<T>(val), *_body->_zone);
				return _body->at(pos);
			}
		}
		default:
			throw msgpack::type_error();
	}
}

template <typename T>
inline MsgPack::iterator MsgPack::_insert(size_t pos, T&& val) {
	switch (_body->_obj->type) {
		case msgpack::type::NIL:
			_body->_obj->type = msgpack::type::ARRAY;
			_body->_obj->via.array.ptr = nullptr;
			_body->_obj->via.array.size = 0;
		case msgpack::type::ARRAY:
			if (pos >= _body->_obj->via.array.size) {
				_reserve_array(pos + 1);

				// Initialize new elements.
				const auto plast = &_body->_obj->via.array.ptr[pos];
				auto p = &_body->_obj->via.array.ptr[_body->_obj->via.array.size];
				for (; p != plast; ++p) {
					p->type = msgpack::type::NIL;
					++_body->_obj->via.array.size;
				}

				*p = msgpack::object(std::forward<T>(val), *_body->_zone);
				++_body->_obj->via.array.size;
				_init_array(_body->array.size());
				return MsgPack::Iterator<MsgPack>(this, size()-1);
			} else {
				_reserve_array(_body->_obj->via.array.size + 1);

				auto p = _body->_obj->via.array.ptr + pos;
				if (p->type == msgpack::type::NIL) {
					*p = msgpack::object(std::forward<T>(val), *_body->_zone);
					return MsgPack::Iterator<MsgPack>(this, pos);
				} else {
					std::memmove(p + 1, p, (_body->_obj->via.array.size - pos) * sizeof(msgpack::object));
					*p = msgpack::object(std::forward<T>(val), *_body->_zone);
					++_body->_obj->via.array.size;
					_init_array(_body->array.size());
					return MsgPack::Iterator<MsgPack>(this, pos);
				}
			}
			break;
		default:
			throw msgpack::type_error();
	}
}

inline MsgPack& MsgPack::path(const std::vector<std::string>& path) {
	return _path(this, path);
}

inline const MsgPack& MsgPack::path(const std::vector<std::string>& path) const {
	return _path(this, path);
}

template <typename M, typename T, typename>
inline MsgPack& MsgPack::put(const M& o, T&& val) {
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

template <typename M, typename T, typename>
inline MsgPack::iterator MsgPack::insert(const M& o, T&& val) {
	switch (o._body->_obj->type) {
		case msgpack::type::STR:
			_put(std::string(o._body->_obj->via.str.ptr, o._body->_obj->via.str.size), std::forward<T>(val));
			return  MsgPack::Iterator<MsgPack>(this, size()-1);
		case msgpack::type::NEGATIVE_INTEGER:
			if (o._body->_obj->via.i64 < 0) throw msgpack::type_error();
			return _insert(static_cast<size_t>(o._body->_obj->via.i64), std::forward<T>(val));
		case msgpack::type::POSITIVE_INTEGER:
			return _insert(static_cast<size_t>(o._body->_obj->via.u64), std::forward<T>(val));
		default:
			throw msgpack::type_error();
	}
}

template <typename T>
inline MsgPack& MsgPack::put(const std::string& s, T&& val) {
	return _put(s, std::forward<T>(val));
}

template <typename T>
inline MsgPack::iterator MsgPack::insert(const std::string& s, T&& val) {
	_put(s, std::forward<T>(val));
	return MsgPack::Iterator<MsgPack>(this, size()-1);
}

template <typename T>
inline MsgPack& MsgPack::put(size_t pos, T&& val) {
	return _put(pos, std::forward<T>(val));
}

template <typename T>
inline MsgPack::iterator MsgPack::insert(size_t pos, T&& val) {
	return _insert(pos, std::forward<T>(val));
}

template <typename T>
inline MsgPack& MsgPack::push_back(T&& v) {
	return _put(size(), std::forward<T>(v));
}

inline const msgpack::object& MsgPack::internal_msgpack() const {
	return *_body->_obj;
}

template <typename T, typename>
inline std::pair<MsgPack::iterator, bool> MsgPack::insert(T&& v) {
	MsgPack o(v);
	bool done = false;
	MsgPack::Iterator<MsgPack> it(end());
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
			done = true;
			break;
		case msgpack::type::MAP:
			for (auto& key : o) {
				auto& val = o.at(key);
				put(key, val);
			}
			done = true;
			break;
		default:
			break;
	}

	return std::make_pair(it, done);
}

template <typename M, typename>
inline MsgPack& MsgPack::operator[](const M& o) {
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

template <typename M, typename>
inline const MsgPack& MsgPack::operator[](const M& o) const {
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

inline MsgPack& MsgPack::operator[](const std::string& s) {
	return _at_or_create(s);
}

inline const MsgPack& MsgPack::operator[](const std::string& s) const {
	return _at_or_create(s);
}

inline MsgPack& MsgPack::operator[](size_t pos) {
	return _at_or_create(pos);
}

inline const MsgPack& MsgPack::operator[](size_t pos) const {
	return _at_or_create(pos);
}

template <typename M, typename>
inline MsgPack& MsgPack::at(const M& o) {
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

template <typename M, typename>
inline const MsgPack& MsgPack::at(const M& o) const {
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

inline MsgPack& MsgPack::at(const std::string& s) {
	return _at(s);
}

inline const MsgPack& MsgPack::at(const std::string& s) const {
	return _at(s);
}

inline MsgPack& MsgPack::at(size_t pos) {
	return _at(pos);
}

inline const MsgPack& MsgPack::at(size_t pos) const {
	return _at(pos);
}

template <typename M, typename>
inline MsgPack::iterator MsgPack::find(const M& o) {
	try {
		switch (o._body->_obj->type) {
			case msgpack::type::STR:
				return MsgPack::iterator(this, _find(std::string(o._body->_obj->via.str.ptr, o._body->_obj->via.str.size))._body->_pos);
			case msgpack::type::NEGATIVE_INTEGER:
				if (o._body->_obj->via.i64 < 0) throw msgpack::type_error();
				return MsgPack::iterator(this, _find(static_cast<size_t>(o._body->_obj->via.i64))._body->_pos);
			case msgpack::type::POSITIVE_INTEGER:
				return MsgPack::iterator(this, _find(static_cast<size_t>(o._body->_obj->via.u64))._body->_pos);
			default:
				throw msgpack::type_error();
		}
	} catch (const std::out_of_range&) {
		return end();
	}
}

template <typename M, typename>
inline MsgPack::const_iterator MsgPack::find(const M& o) const {
	try {
		switch (o._body->_obj->type) {
			case msgpack::type::STR:
				return MsgPack::const_iterator(this, _find(std::string(o._body->_obj->via.str.ptr, o._body->_obj->via.str.size))._body->_pos);
			case msgpack::type::NEGATIVE_INTEGER:
				if (o._body->_obj->via.i64 < 0) throw msgpack::type_error();
				return MsgPack::const_iterator(this, _find(static_cast<size_t>(o._body->_obj->via.i64))._body->_pos);
			case msgpack::type::POSITIVE_INTEGER:
				return MsgPack::const_iterator(this, _find(static_cast<size_t>(o._body->_obj->via.u64))._body->_pos);
			default:
				throw msgpack::type_error();
		}
	} catch (const std::out_of_range&) {
		return cend();
	}
}

inline MsgPack::iterator MsgPack::find(const std::string& s) {
	try {
		return MsgPack::iterator(this, _find(s)._body->_pos);
	} catch (const std::out_of_range&) {
		return end();
	}
}

inline MsgPack::const_iterator MsgPack::find(const std::string& s) const {
	try {
		return MsgPack::const_iterator(this, _find(s)._body->_pos);
	} catch (const std::out_of_range&) {
		return cend();
	}
}

inline MsgPack::iterator MsgPack::find(size_t pos) {
	try {
		return MsgPack::iterator(this, _find(pos)._body->_pos);
	} catch (const std::out_of_range&) {
		return end();
	}
}

inline MsgPack::const_iterator MsgPack::find(size_t pos) const {
	try {
		return MsgPack::const_iterator(this, _find(pos)._body->_pos);
	} catch (const std::out_of_range&) {
		return cend();
	}
}

inline MsgPack& MsgPack::parent() {
	return _body->_parent;
}

inline const MsgPack& MsgPack::parent() const {
	return _body->_parent;
}

inline size_t MsgPack::count(const MsgPack& o) const {
	return (find(o) == end()) ? 0 : 1;
}

template <typename M, typename>
inline size_t MsgPack::erase(const M& o) {
	try {
		switch (o._body->_obj->type) {
			case msgpack::type::STR:
				_erase(std::string(o._body->_obj->via.str.ptr, o._body->_obj->via.str.size));
				return 1;
			case msgpack::type::NEGATIVE_INTEGER:
				if (o._body->_obj->via.i64 < 0) throw msgpack::type_error();
				_erase(static_cast<size_t>(o._body->_obj->via.i64));
				return 1;
			case msgpack::type::POSITIVE_INTEGER:
				_erase(static_cast<size_t>(o._body->_obj->via.u64));
				return 1;
			default:
				throw msgpack::type_error();
		}
	} catch (const last_in_range&) {
		return 1;
	} catch (const std::out_of_range&) {
		return 0;
	}
}

inline size_t MsgPack::erase(const std::string& s) {
	try {
		_erase(s);
		return 1;
	} catch (const last_in_range&) {
		return 1;
	} catch (const std::out_of_range&) {
		return 0;
	}
}

inline size_t MsgPack::erase(size_t pos) {
	try {
		_erase(pos);
		return 1;
	} catch (const last_in_range&) {
		return 1;
	} catch (const std::out_of_range&) {
		return 0;
	}
}

inline MsgPack::iterator MsgPack::erase(const MsgPack::iterator& it) {
	try {
		return MsgPack::iterator(this, _erase(it._off)._body->_pos);
	} catch (const std::out_of_range&) {
		return end();
	}
}

inline void MsgPack::clear() noexcept {
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

inline MsgPack::operator bool() const {
	if (_body == nullptr) {
		return false;
	}

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

inline void MsgPack::reserve(size_t n) {
	switch (_body->_obj->type) {
		case msgpack::type::MAP:
			return _reserve_map(n);
		case msgpack::type::ARRAY:
			return _reserve_array(n);
		default:
			return;
	}
}

inline size_t MsgPack::capacity() const noexcept {
	return _body->_capacity;
}

inline size_t MsgPack::size() const noexcept {
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

inline bool MsgPack::empty() const noexcept {
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

inline uint64_t MsgPack::as_u64() const {
	switch (_body->_obj->type) {
		case msgpack::type::NEGATIVE_INTEGER:
			return _body->_obj->via.i64;
		case msgpack::type::POSITIVE_INTEGER:
			return _body->_obj->via.u64;
		default:
			throw msgpack::type_error();
	}
}

inline int64_t MsgPack::as_i64() const {
	switch (_body->_obj->type) {
		case msgpack::type::NEGATIVE_INTEGER:
			return _body->_obj->via.i64;
		case msgpack::type::POSITIVE_INTEGER:
			return _body->_obj->via.u64;
		default:
			throw msgpack::type_error();
	}
}

inline double MsgPack::as_f64() const {
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

inline std::string MsgPack::as_string() const {
	switch (_body->_obj->type) {
		case msgpack::type::STR:
			return std::string(_body->_obj->via.str.ptr, _body->_obj->via.str.size);
		default:
			throw msgpack::type_error();
	}
}

inline bool MsgPack::as_bool() const {
	switch (_body->_obj->type) {
		case msgpack::type::BOOLEAN:
			return _body->_obj->via.boolean;
		default:
			throw msgpack::type_error();
	}
}

inline rapidjson::Document MsgPack::as_document() const {
	rapidjson::Document doc;
	_body->_obj->convert(&doc);
	return doc;
}

inline bool MsgPack::is_null() const {
	return _body == nullptr || _body->_obj->type == msgpack::type::NIL;
}

inline bool MsgPack::is_boolean() const {
	return _body->_obj->type == msgpack::type::BOOLEAN;
}

inline bool MsgPack::is_number() const {
	switch (_body->_obj->type) {
		case msgpack::type::NEGATIVE_INTEGER:
		case msgpack::type::POSITIVE_INTEGER:
		case msgpack::type::FLOAT:
			return true;
		default:
			return false;
	}
}

inline bool MsgPack::is_map() const {
	return _body->_obj->type == msgpack::type::MAP;
}

inline bool MsgPack::is_array() const {
	return _body->_obj->type == msgpack::type::ARRAY;
}

inline bool MsgPack::is_string() const {
	return _body->_obj->type == msgpack::type::STR;
}

inline msgpack::type::object_type MsgPack::type() const {
	return _body->_obj->type;
}

inline bool MsgPack::operator==(const MsgPack& other) const {
	return *_body->_obj == *other._body->_obj;
}

inline bool MsgPack::operator!=(const MsgPack& other) const {
	return *_body->_obj != *other._body->_obj;
}

inline MsgPack MsgPack::operator +(long val) {
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

inline MsgPack& MsgPack::operator +=(long val) {
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

inline std::string MsgPack::to_string(bool prettify) const {
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

inline std::ostream& MsgPack::operator<<(std::ostream& s) const {
	s << *_body->_obj;
	return s;
}

inline std::string MsgPack::serialise() const {
	msgpack::sbuffer sbuf;
	msgpack::pack(&sbuf, *_body->_obj);
	return std::string(sbuf.data(), sbuf.size());
}

inline MsgPack MsgPack::unserialise(const std::string& s) {
	return MsgPack(msgpack::unpack(s.data(), s.size()).get());
}


inline std::ostream& operator<<(std::ostream& s, const MsgPack& o){
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
