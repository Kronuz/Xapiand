/*
 * Copyright (C) 2015,2016,2017 deipi.com LLC and contributors. All rights reserved.
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

#include <memory>
#include <sstream>
#include <unordered_map>

#include "exception.h"
#include "msgpack.hpp"
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"
#include "xchange/rapidjson.hpp"
#if XAPIAND_CHAISCRIPT
#include "xchange/chaiscript.hpp"
#endif


constexpr size_t  MSGPACK_MAP_INIT_SIZE   = 4;
constexpr size_t  MSGPACK_ARRAY_INIT_SIZE = 4;
constexpr double  MSGPACK_GROWTH_FACTOR   = 1.5;  // Choosing 1.5 as the factor allows memory reuse after 4 reallocations (https://github.com/facebook/folly/blob/master/folly/docs/FBVector.md)
constexpr uint8_t MSGPACK_EXT_BEGIN       = 0x80;
constexpr uint8_t MSGPACK_EXT_MASK        = 0x7f;


class MsgPack {
	struct Body;

	template <typename T>
	class Iterator;

	const std::shared_ptr<Body> _body;
	const Body* const _const_body;

	MsgPack(const std::shared_ptr<Body>& b);
	MsgPack(std::shared_ptr<Body>&& b);

	void _initializer_array(std::initializer_list<MsgPack> list);
	void _initializer_map(std::initializer_list<MsgPack> list);
	void _initializer(std::initializer_list<MsgPack> list);

	void _assignment(const msgpack::object& obj);

public:
	enum class Type : uint8_t {
		NIL                 = msgpack::type::NIL,               //0x00
		BOOLEAN             = msgpack::type::BOOLEAN,           //0x01
		POSITIVE_INTEGER    = msgpack::type::POSITIVE_INTEGER,  //0x02
		NEGATIVE_INTEGER    = msgpack::type::NEGATIVE_INTEGER,  //0x03
		FLOAT               = msgpack::type::FLOAT,             //0x04
		STR                 = msgpack::type::STR,               //0x05
		ARRAY               = msgpack::type::ARRAY,             //0x06
		MAP                 = msgpack::type::MAP,               //0x07
		BIN                 = msgpack::type::BIN,               //0x08

		// Custom external types follow:
		UNDEFINED           = MSGPACK_EXT_BEGIN,
	};

	struct duplicate_key : public BaseException, public std::out_of_range {
		template<typename... Args>
		duplicate_key(Args&&... args) : BaseException(std::forward<Args>(args)...), std::out_of_range(message) { }
	};

	struct out_of_range : public BaseException, public std::out_of_range {
		template<typename... Args>
		out_of_range(Args&&... args) : BaseException(std::forward<Args>(args)...), std::out_of_range(message) { }
	};

	struct invalid_argument : public BaseException, public std::invalid_argument {
		template<typename... Args>
		invalid_argument(Args&&... args) : BaseException(std::forward<Args>(args)...), std::invalid_argument(message) { }
	};

	static msgpack::object _undefined() {
		static const char data = (char)Type::UNDEFINED & MSGPACK_EXT_MASK;
		msgpack::object o;
		o.type = msgpack::type::EXT;
		o.via.ext.ptr = &data;
		o.via.ext.size = 1;
		return o;
	}

	static const MsgPack& undefined() {
		static const MsgPack undefined(MsgPack::_undefined());
		return undefined;
	}

	using iterator = Iterator<MsgPack>;
	using const_iterator = Iterator<const MsgPack>;

	iterator begin();
	const_iterator begin() const;
	const_iterator cbegin() const;
	iterator end();
	const_iterator end() const;
	const_iterator cend() const;

	MsgPack();
	MsgPack(const MsgPack& other);
	MsgPack(MsgPack&& other);
	MsgPack(std::initializer_list<MsgPack> list);
	MsgPack(Type type);

	template <typename T, typename = std::enable_if_t<not std::is_same<std::shared_ptr<Body>, std::decay_t<T>>::value>>
	MsgPack(T&& v);

	MsgPack& operator=(const MsgPack& other);
	MsgPack& operator=(MsgPack&& other);
	MsgPack& operator=(std::initializer_list<MsgPack> list);

	template <typename T>
	MsgPack& operator=(T&& v);

private:
	MsgPack* _init_map(size_t pos);
	void _update_map(size_t pos);

	MsgPack* _init_array(size_t pos);
	void _update_array(size_t pos);

	void _init();
	void _deinit();

	void _reserve_map(size_t rsize);
	void _reserve_array(size_t rsize);

	MsgPack::iterator _find(const std::string& key);
	MsgPack::const_iterator _find(const std::string& key) const;
	MsgPack::iterator _find(size_t pos);
	MsgPack::const_iterator _find(size_t pos) const;

	template <typename M, typename = std::enable_if_t<std::is_same<MsgPack, std::decay_t<M>>::value>>
	std::pair<size_t, MsgPack::iterator> _erase(M&& o);
	std::pair<size_t, MsgPack::iterator> _erase(const std::string& key);
	std::pair<size_t, MsgPack::iterator> _erase(size_t pos);

	template <typename M, typename T, typename = std::enable_if_t<std::is_same<MsgPack, std::decay_t<M>>::value>>
	std::pair<MsgPack*, bool> _put(M&& o, T&& val, bool overwrite);
	template <typename T>
	std::pair<MsgPack*, bool> _put(const std::string& key, T&& val, bool overwrite);
	template <typename T>
	std::pair<MsgPack*, bool> _put(size_t pos, T&& val, bool overwrite);

	template <typename M, typename T, typename = std::enable_if_t<std::is_same<MsgPack, std::decay_t<M>>::value>>
	std::pair<MsgPack*, bool> _insert(M&& o, T&& val, bool overwrite);
	template <typename T>
	std::pair<MsgPack*, bool> _insert(size_t pos, T&& val, bool overwrite);

	void _fill(bool recursive, bool lock);
	void _fill(bool recursive, bool lock) const;

public:
	void lock() const;

	MsgPack& path(const std::vector<std::string>& path);
	const MsgPack& path(const std::vector<std::string>& path) const;

	template <typename M, typename T, typename = std::enable_if_t<std::is_same<MsgPack, std::decay_t<M>>::value>>
	MsgPack::iterator put(M&& o, T&& val);
	template <typename T>
	MsgPack::iterator put(const std::string& s, T&& val);
	template <typename T>
	MsgPack::iterator put(size_t pos, T&& val);

	template <typename K, typename V>
	std::pair<MsgPack::iterator, bool> insert(const std::pair<K&&, V&&>& val);

	template <typename M, typename T, typename = std::enable_if_t<std::is_same<MsgPack, std::decay_t<M>>::value>>
	std::pair<MsgPack::iterator, bool> emplace(M&& o, T&& val);
	template <typename T>
	std::pair<MsgPack::iterator, bool> emplace(const std::string& s, T&& val);
	template <typename T>
	std::pair<MsgPack::iterator, bool> emplace(size_t pos, T&& val);

	template <typename T>
	void push_back(T&& v);

	template <typename T>
	decltype(auto) external(std::function<T(const msgpack::object&)>) const;

	template <typename M, typename = std::enable_if_t<std::is_same<MsgPack, std::decay_t<M>>::value>>
	MsgPack& operator[](M&& o);
	template <typename M, typename = std::enable_if_t<std::is_same<MsgPack, std::decay_t<M>>::value>>
	const MsgPack& operator[](M&& o) const;
	MsgPack& operator[](const std::string& s);
	const MsgPack& operator[](const std::string& s) const;
	MsgPack& operator[](size_t pos);
	const MsgPack& operator[](size_t pos) const;

	template <typename M, typename = std::enable_if_t<std::is_same<MsgPack, std::decay_t<M>>::value>>
	MsgPack& at(M&& o);
	template <typename M, typename = std::enable_if_t<std::is_same<MsgPack, std::decay_t<M>>::value>>
	const MsgPack& at(M&& o) const;
	MsgPack& at(const std::string& s);
	const MsgPack& at(const std::string& s) const;
	MsgPack& at(size_t pos);
	const MsgPack& at(size_t pos) const;

	template <typename M, typename = std::enable_if_t<std::is_same<MsgPack, std::decay_t<M>>::value>>
	iterator find(M&& o);
	template <typename M, typename = std::enable_if_t<std::is_same<MsgPack, std::decay_t<M>>::value>>
	const_iterator find(M&& o) const;
	iterator find(const std::string& s);
	const_iterator find(const std::string& s) const;
	iterator find(size_t pos);
	const_iterator find(size_t pos) const;

	template <typename M, typename = std::enable_if_t<std::is_same<MsgPack, std::decay_t<M>>::value>>
	void update(M&& o);

	template <typename T>
	size_t count(T&& v) const;

	template <typename M, typename = std::enable_if_t<std::is_same<MsgPack, std::decay_t<M>>::value>>
	size_t erase(M&& o);
	size_t erase(const std::string& s);
	size_t erase(size_t pos);
	iterator erase(iterator it);

	void clear() noexcept;

	std::size_t hash() const;

	explicit operator bool() const;

	void reserve(size_t n);

	size_t capacity() const noexcept;

	size_t size() const noexcept;

	bool empty() const noexcept;

	uint64_t u64() const;
	int64_t i64() const;
	double f64() const;
	std::string str() const;
	bool boolean() const;

	uint64_t as_u64() const;
	int64_t as_i64() const;
	double as_f64() const;
	std::string as_str() const;
	bool as_boolean() const;
	rapidjson::Document as_document() const;

	bool is_undefined() const noexcept;
	bool is_null() const noexcept;
	bool is_boolean() const noexcept;
	bool is_number() const noexcept;
	bool is_integer() const noexcept;
	bool is_float() const noexcept;
	bool is_map() const noexcept;
	bool is_array() const noexcept;
	bool is_string() const noexcept;

	MsgPack::Type getType() const noexcept;
	std::string getStrType() const noexcept;

	bool operator==(const MsgPack& other) const;
	bool operator!=(const MsgPack& other) const;
	MsgPack operator+(double val);
	MsgPack& operator+=(double val);
	std::ostream& operator<<(std::ostream& s) const;

	std::string unformatted_string() const;
	std::string to_string(int indent=-1) const;

	template <typename B=msgpack::sbuffer>
	std::string serialise() const;
	static MsgPack unserialise(const std::string& s);

	friend msgpack::adaptor::convert<MsgPack>;
	friend msgpack::adaptor::pack<MsgPack>;
	friend msgpack::adaptor::object<MsgPack>;
	friend msgpack::adaptor::object_with_zone<MsgPack>;
};


template <typename T>
class MsgPack::Iterator : public std::iterator<std::input_iterator_tag, MsgPack> {
	friend MsgPack;

	T* _mobj;
	off_t _off;

	Iterator(T* o, off_t off)
		: _mobj(o),
		  _off(off)
	{
		switch (_mobj->_const_body->getType()) {
			case MsgPack::Type::MAP:
			case MsgPack::Type::ARRAY:
			case MsgPack::Type::UNDEFINED:
				return;
			default:
				THROW(msgpack::type_error);
		}
	}

public:
	Iterator(const Iterator& it)
		: _mobj(it._mobj),
		  _off(it._off)	{ }

	Iterator& operator++() {
		++_off;
		return *this;
	}

	Iterator operator++(int) {
		Iterator tmp(*this);
		++_off;
		return tmp;
	}

	Iterator& operator+=(int off) {
		_off += off;
		return *this;
	}

	Iterator operator+(int off) const {
		Iterator tmp(*this);
		tmp._off += off;
		return tmp;
	}

	Iterator operator=(const Iterator& other) {
		_mobj = other._mobj;
		_off = other._off;
		return *this;
	}

	T& operator*() {
		if (_mobj->_const_body->getType() == MsgPack::Type::MAP) {
			return _mobj->at(_off)._body->_key;
		} else {
			return _mobj->at(_off);
		}
	}

	T* operator->() {
		return &operator*();
	}

	T& operator*() const {
		if (_mobj->_const_body->getType() == MsgPack::Type::MAP) {
			return _mobj->at(_off)._const_body->_key;
		} else {
			return _mobj->at(_off);
		}
	}

	T* operator->() const {
		return &operator*();
	}

	T& value() {
		return _mobj->at(_off);
	}

	T& value() const {
		return _mobj->at(_off);
	}

	bool operator==(const Iterator& other) const {
		return this == &other || (*_mobj == *other._mobj && _off == other._off);
	}

	bool operator!=(const Iterator& other) const {
		return !operator==(other);
	}
};


struct MsgPack::Body {
	std::unordered_map<std::string, std::pair<MsgPack, MsgPack>> map;
	std::vector<MsgPack> array;

	std::atomic_bool _lock;
	bool _initialized;

	const std::shared_ptr<msgpack::zone> _zone;
	const std::shared_ptr<msgpack::object> _base;
	std::weak_ptr<Body> _parent;
	bool _is_key;
	size_t _pos;
	MsgPack _key;
	msgpack::object* _obj;
	size_t _capacity;

	Body(const std::shared_ptr<msgpack::zone>& zone,
		 const std::shared_ptr<msgpack::object>& base,
		 const std::shared_ptr<Body>& parent,
		 bool is_key,
		 size_t pos,
		 const std::shared_ptr<Body>& key,
		 msgpack::object* obj
		) : _lock(false),
			_initialized(false),
			_zone(zone),
			_base(base),
			_parent(parent),
			_is_key(is_key),
			_pos(pos),
			_key(key),
			_obj(obj),
			_capacity(size()) { }

	Body(const std::shared_ptr<msgpack::zone>& zone,
		 const std::shared_ptr<msgpack::object>& base,
		 const std::shared_ptr<Body>& parent,
		 msgpack::object* obj
		) : _lock(false),
			_initialized(false),
			_zone(zone),
			_base(base),
			_parent(parent),
			_is_key(false),
			_pos(-1),
			_key(std::shared_ptr<Body>()),
			_obj(obj),
			_capacity(size()) { }

	template <typename T>
	Body(T&& v)
		: _lock(false),
		  _initialized(false),
		  _zone(std::make_shared<msgpack::zone>()),
		  _base(std::make_shared<msgpack::object>(std::forward<T>(v), *_zone)),
		  _parent(std::shared_ptr<Body>()),
		  _is_key(false),
		  _pos(-1),
		  _key(std::shared_ptr<Body>()),
		  _obj(_base.get()),
		  _capacity(size()) { }

	MsgPack& at(size_t pos) {
		return array.at(pos);
	}

	const MsgPack& at(size_t pos) const {
		return array.at(pos);
	}

	MsgPack& at(const std::string& key) {
		return map.at(key).second;
	}

	const MsgPack& at(const std::string& key) const {
		return map.at(key).second;
	}

	size_t size() const noexcept {
		switch (_obj->type) {
			case msgpack::type::MAP:
				return _obj->via.map.size;
			case msgpack::type::ARRAY:
				return _obj->via.array.size;
			case msgpack::type::STR:
				return _obj->via.str.size;
			default:
				return 0;
		}
	}

	Type getType() const noexcept {
		return _obj->type == msgpack::type::EXT ? (Type)(_obj->via.ext.type() | MSGPACK_EXT_BEGIN) : (Type)_obj->type;
	}

	std::string getStrType() const noexcept {
		switch (getType()) {
			case Type::NIL: return "NIL";
			case Type::BOOLEAN: return "BOOLEAN";
			case Type::POSITIVE_INTEGER: return "POSITIVE_INTEGER";
			case Type::NEGATIVE_INTEGER: return "NEGATIVE_INTEGER";
			case Type::FLOAT: return "FLOAT";
			case Type::STR: return "STR";
			case Type::ARRAY: return "ARRAY";
			case Type::MAP: return "MAP";
			case Type::BIN: return "BIN";
			case Type::UNDEFINED: return "UNDEFINED";
			default: return "<UNKNOWN>";
		}
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
	: _body(b),
	  _const_body(_body.get()) { }


inline MsgPack::MsgPack(std::shared_ptr<MsgPack::Body>&& b)
	: _body(std::move(b)),
	  _const_body(_body.get()) { }


inline void MsgPack::_initializer_array(std::initializer_list<MsgPack> list) {
	if (_body->_obj->type != msgpack::type::ARRAY) {
		_body->_capacity = 0;
	}
	_deinit();
	_body->_obj->type = msgpack::type::ARRAY;
	_body->_obj->via.array.ptr = nullptr;
	_body->_obj->via.array.size = 0;
	_reserve_array(list.size());
	for (const auto& val : list) {
		push_back(val);
	}
}


inline void MsgPack::_initializer_map(std::initializer_list<MsgPack> list) {
	if (_body->_obj->type != msgpack::type::MAP) {
		_body->_capacity = 0;
	}
	_deinit();
	_body->_obj->type = msgpack::type::MAP;
	_body->_obj->via.map.ptr = nullptr;
	_body->_obj->via.map.size = 0;
	_reserve_map(list.size());
	for (const auto& val : list) {
		put(val.at(0), val.at(1));
	}
}


inline void MsgPack::_initializer(std::initializer_list<MsgPack> list) {
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


inline void MsgPack::_assignment(const msgpack::object& obj) {
	if (_body->_is_key) {
		if (obj.type != msgpack::type::STR) {
			THROW(msgpack::type_error);
		}
		if (auto parent_body = _body->_parent.lock()) {
			if (parent_body->_initialized) {
				// Change key in the parent's map:
				auto val = std::string(_body->_obj->via.str.ptr, _body->_obj->via.str.size);
				auto str_key = std::string(obj.via.str.ptr, obj.via.str.size);
				if (str_key == val) {
					return;
				}
				auto it = parent_body->map.find(val);
				if (it != parent_body->map.end()) {
					if (parent_body->map.emplace(str_key, std::move(it->second)).second) {
						parent_body->map.erase(it);
					} else {
						THROW(duplicate_key, "Duplicate 1 key: " + str_key);
					}
					assert(parent_body->_obj->via.map.size == parent_body->map.size());
				}
			}
		}
	}
	_deinit();
	*_body->_obj = obj;
	_body->_capacity = _body->size();
}


inline MsgPack::MsgPack()
	: MsgPack(MsgPack::undefined()) { }


inline MsgPack::MsgPack(const MsgPack& other)
	: _body(std::make_shared<Body>(other)),
	  _const_body(_body.get()) { }


inline MsgPack::MsgPack(MsgPack&& other)
	: _body(std::move(other._body)),
	  _const_body(_body.get()) { }


template <typename T, typename>
inline MsgPack::MsgPack(T&& v)
	: _body(std::make_shared<Body>(std::forward<T>(v))),
	  _const_body(_body.get()) { }


inline MsgPack::MsgPack(std::initializer_list<MsgPack> list)
	: MsgPack(MsgPack::undefined())
{
	_initializer(list);
}


inline MsgPack::MsgPack(Type type)
	: MsgPack(MsgPack::undefined())
{
	_deinit();
	switch (type) {
		case Type::NIL:
			_body->_obj->type = msgpack::type::NIL;
			break;
		case Type::BOOLEAN:
			_body->_obj->type = msgpack::type::BOOLEAN;
			_body->_obj->via.boolean = false;
			break;
		case Type::POSITIVE_INTEGER:
			_body->_obj->type = msgpack::type::POSITIVE_INTEGER;
			_body->_obj->via.u64 = 0;
			break;
		case Type::NEGATIVE_INTEGER:
			_body->_obj->type = msgpack::type::NEGATIVE_INTEGER;
			_body->_obj->via.i64 = 0;
			break;
		case Type::FLOAT:
			_body->_obj->type = msgpack::type::FLOAT;
			_body->_obj->via.f64 = 0;
			break;
		case Type::STR:
			_body->_obj->type = msgpack::type::STR;
			_body->_obj->via.str.ptr = nullptr;
			_body->_obj->via.str.size = 0;
			break;
		case Type::BIN:
			_body->_obj->type = msgpack::type::BIN;
			_body->_obj->via.bin.ptr = nullptr;
			_body->_obj->via.bin.size = 0;
			break;
		case Type::ARRAY:
			_body->_obj->type = msgpack::type::ARRAY;
			_body->_obj->via.array.ptr = nullptr;
			_body->_obj->via.array.size = 0;
			break;
		case Type::MAP:
			_body->_obj->type = msgpack::type::MAP;
			_body->_obj->via.map.ptr = nullptr;
			_body->_obj->via.map.size = 0;
			break;
		case Type::UNDEFINED:
			break;
		default:
			THROW(msgpack::type_error);
	}
}


inline MsgPack& MsgPack::operator=(const MsgPack& other) {
	_assignment(msgpack::object(other, *_body->_zone));
	return *this;
}


inline MsgPack& MsgPack::operator=(MsgPack&& other) {
	_assignment(msgpack::object(std::move(other), *_body->_zone));
	return *this;
}


inline MsgPack& MsgPack::operator=(std::initializer_list<MsgPack> list) {
	_initializer(list);
	return *this;
}


template <typename T>
inline MsgPack& MsgPack::operator=(T&& v) {
	_assignment(msgpack::object(std::forward<T>(v), *_body->_zone));
	return *this;
}


inline MsgPack* MsgPack::_init_map(size_t pos) {
	MsgPack* ret = nullptr;
	_body->map.reserve(_body->_capacity);
	const auto pend = &_body->_obj->via.map.ptr[_body->_obj->via.map.size];
	for (auto p = &_body->_obj->via.map.ptr[pos]; p != pend; ++p, ++pos) {
		if (p->key.type != msgpack::type::STR) {
			THROW(msgpack::type_error);
		}
		auto last_key = MsgPack(std::make_shared<Body>(_body->_zone, _body->_base, _body, true, 0, nullptr, &p->key));
		auto last_val = MsgPack(std::make_shared<Body>(_body->_zone, _body->_base, _body, false, pos, last_key._body, &p->val));
		auto str_key = std::string(p->key.via.str.ptr, p->key.via.str.size);
		auto inserted = _body->map.emplace(str_key, std::make_pair(std::move(last_key), std::move(last_val)));
		if (!inserted.second) {
			THROW(duplicate_key, "Duplicate key: " + str_key);
		}
		ret = &inserted.first->second.second;
	}
	assert(_body->_obj->via.map.size == _body->map.size());
	_body->_initialized = true;
	return ret;
}


inline void MsgPack::_update_map(size_t pos) {
	const auto pend = &_body->_obj->via.map.ptr[_body->_obj->via.map.size];
	for (auto p = &_body->_obj->via.map.ptr[pos]; p != pend; ++p, ++pos) {
		std::string str_key(p->key.via.str.ptr, p->key.via.str.size);
		auto it = _body->map.find(str_key);
		assert(it != _body->map.end());
		auto& elem = it->second;
		elem.first._body->_obj = &p->key;
		elem.second._body->_obj = &p->val;
		elem.second._body->_capacity = elem.second._body->size();
		elem.second._body->_pos = pos;
	}
}


inline MsgPack* MsgPack::_init_array(size_t pos) {
	if (pos < _body->array.size()) {
		// Destroy the previous objects to update
		_body->array.resize(pos);
	}

	MsgPack* ret = nullptr;
	_body->array.reserve(_body->_capacity);
	const auto pend = &_body->_obj->via.array.ptr[_body->_obj->via.array.size];
	for (auto p = &_body->_obj->via.array.ptr[pos]; p != pend; ++p, ++pos) {
		auto last_val = MsgPack(std::make_shared<Body>(_body->_zone, _body->_base, _body, false, pos, nullptr, p));
		ret = &*_body->array.insert(_body->array.end(), std::move(last_val));
	}
	assert(_body->_obj->via.array.size == _body->array.size());
	_body->_initialized = true;
	return ret;
}


inline void MsgPack::_update_array(size_t pos) {
	const auto pend = &_body->_obj->via.array.ptr[_body->_obj->via.array.size];
	for (auto p = &_body->_obj->via.array.ptr[pos]; p != pend; ++p, ++pos) {
		// If the previous item was a MAP, force map update.
		_body->array.at(pos)._body->map.clear();
		auto& mobj = _body->at(pos);
		mobj._body->_pos = pos;
		mobj._body->_obj = p;
		mobj._body->_capacity = mobj._body->size();
	}
}


inline void MsgPack::_init() {
	switch (_body->getType()) {
		case Type::MAP:
			_init_map(0);
			break;
		case Type::ARRAY:
			_init_array(0);
			break;
		default:
			_body->_initialized = true;
			break;
	}
}


inline void MsgPack::_deinit() {
	_body->_initialized = false;

	switch (_body->getType()) {
		case Type::MAP:
			_body->map.clear();
			break;
		case Type::ARRAY:
			_body->array.clear();
			break;
		default:
			break;
	}
}


inline void MsgPack::_reserve_map(size_t rsize) {
	if (_body->_capacity <= rsize) {
		size_t nsize = _body->_capacity < MSGPACK_MAP_INIT_SIZE ? MSGPACK_MAP_INIT_SIZE : _body->_capacity * MSGPACK_GROWTH_FACTOR;
		while (nsize < rsize) {
			nsize *= MSGPACK_GROWTH_FACTOR;
		}
		auto ptr = static_cast<msgpack::object_kv*>(_body->_zone->allocate_align(nsize * sizeof(msgpack::object_kv)));
		if (_body->_obj->via.map.ptr != nullptr && _body->_capacity > 0) {
			std::memcpy(ptr, _body->_obj->via.map.ptr, _body->_obj->via.map.size * sizeof(msgpack::object_kv));
			// std::memset(_body->_obj->via.map.ptr, 0, _body->_obj->via.map.size * sizeof(msgpack::object_kv));  // clear memory for debugging
		}
		_body->_obj->via.map.ptr = ptr;
		_body->_capacity = nsize;
		if (_body->_initialized) {
			_init_map(_body->map.size());
			_update_map(0);
		}
	}
}


inline void MsgPack::_reserve_array(size_t rsize) {
	if (_body->_capacity <= rsize) {
		size_t nsize = _body->_capacity >= MSGPACK_ARRAY_INIT_SIZE ? _body->_capacity * MSGPACK_GROWTH_FACTOR : MSGPACK_ARRAY_INIT_SIZE;
		while (nsize < rsize) {
			nsize *= MSGPACK_GROWTH_FACTOR;
		}
		auto ptr = static_cast<msgpack::object*>(_body->_zone->allocate_align(nsize * sizeof(msgpack::object)));
		if (_body->_obj->via.array.ptr != nullptr && _body->_capacity > 0) {
			std::memcpy(ptr, _body->_obj->via.array.ptr, _body->_obj->via.array.size * sizeof(msgpack::object));
			// std::memset(_body->_obj->via.array.ptr, 0, _body->_obj->via.array.size * sizeof(msgpack::object));  // clear memory for debugging
		}
		_body->_obj->via.array.ptr = ptr;
		_body->_capacity = nsize;
		if (_body->_initialized) {
			_init_array(_body->array.size());
			_update_array(0);
		}
	}
}


inline MsgPack::iterator MsgPack::_find(const std::string& key) {
	switch (_body->getType()) {
		case Type::UNDEFINED:
			return end();
		case Type::MAP: {
			auto it = _body->map.find(key);
			if (it == _body->map.end()) {
				return end();
			}
			return MsgPack::iterator(this, it->second.second._body->_pos);
		}
		default:
			THROW(msgpack::type_error);
	}
}


inline MsgPack::const_iterator MsgPack::_find(const std::string& key) const {
	switch (_const_body->getType()) {
		case Type::UNDEFINED:
			return cend();
		case Type::MAP: {
			auto it = _const_body->map.find(key);
			if (it == _const_body->map.end()) {
				return cend();
			}
			return MsgPack::const_iterator(this, it->second.second._const_body->_pos);
		}
		default:
			THROW(msgpack::type_error);
	}
}


inline MsgPack::iterator MsgPack::_find(size_t pos) {
	switch (_body->getType()) {
		case Type::UNDEFINED:
			return end();
		case Type::MAP: {
			if (pos >= _body->_obj->via.map.size) {
				return end();
			}
			auto it = _body->map.find(std::string(_body->_obj->via.map.ptr[pos].key.via.str.ptr, _body->_obj->via.map.ptr[pos].key.via.str.size));
			if (it == _body->map.end()) {
				return end();
			}
			return MsgPack::iterator(this, it->second.second._body->_pos);
		}
		case Type::ARRAY: {
			if (pos >= _body->_obj->via.array.size) {
				return end();
			}
			auto it = _body->array.begin() + pos;
			if (it >= _body->array.end()) {
				return end();
			}
			return MsgPack::iterator(this, it->_body->_pos);
		}
		default:
			THROW(msgpack::type_error);
	}
}


inline MsgPack::const_iterator MsgPack::_find(size_t pos) const {
	switch (_const_body->getType()) {
		case Type::UNDEFINED:
			return cend();
		case Type::MAP: {
			if (pos >= _const_body->_obj->via.map.size) {
				return cend();
			}
			auto it = _const_body->map.find(std::string(_const_body->_obj->via.map.ptr[pos].key.via.str.ptr, _const_body->_obj->via.map.ptr[pos].key.via.str.size));
			if (it == _const_body->map.end()) {
				return cend();
			}
			return MsgPack::const_iterator(this, it->second.second._const_body->_pos);
		}
		case Type::ARRAY: {
			if (pos >= _const_body->_obj->via.array.size) {
				return cend();
			}
			auto it = _const_body->array.begin() + pos;
			if (it >= _const_body->array.end()) {
				return cend();
			}
			return MsgPack::const_iterator(this, it->_const_body->_pos);
		}
		default:
			THROW(msgpack::type_error);
	}
}


template <typename M, typename>
inline std::pair<size_t, MsgPack::iterator> MsgPack::_erase(M&& o) {
	switch (o._body->getType()) {
		case Type::STR:
			return _erase(std::string(o._body->_obj->via.str.ptr, o._body->_obj->via.str.size));
		case Type::NEGATIVE_INTEGER:
			if (o._body->_obj->via.i64 < 0) {
				THROW(msgpack::type_error);
			}
			return _erase(static_cast<size_t>(o._body->_obj->via.i64));
		case Type::POSITIVE_INTEGER:
			return _erase(static_cast<size_t>(o._body->_obj->via.u64));
		default:
			THROW(msgpack::type_error);
	}
}


inline std::pair<size_t, MsgPack::iterator> MsgPack::_erase(const std::string& key) {
	switch (_body->getType()) {
		case Type::UNDEFINED:
			return std::make_pair(0, end());
		case Type::MAP: {
			auto it = _body->map.find(key);
			if (it == _body->map.end()) {
				return std::make_pair(0, end());
			}

			auto& mobj = it->second.second;
			auto pos_ = mobj._body->_pos;
			assert(pos_ < _body->_obj->via.map.size);
			auto p = &_body->_obj->via.map.ptr[pos_];
			std::memmove(p, p + 1, (_body->_obj->via.map.size - pos_ - 1) * sizeof(msgpack::object_kv));
			--_body->_obj->via.map.size;
			// Erase from map:
			_body->map.erase(it);
			_update_map(pos_);

			auto next = _body->map.find(std::string(p->key.via.str.ptr, p->key.via.str.size));
			if (next == _body->map.end()) {
				return std::make_pair(0, end());
			}

			return std::make_pair(1, MsgPack::iterator(this, next->second.second._body->_pos));
		}
		default:
			THROW(msgpack::type_error);
	}
}


inline std::pair<size_t, MsgPack::iterator> MsgPack::_erase(size_t pos) {
	switch (_body->getType()) {
		case Type::UNDEFINED:
			return std::make_pair(0, end());
		case Type::MAP: {
			if (pos >= _body->_obj->via.map.size) {
				return std::make_pair(0, end());
			}
			return _erase(std::string(_body->_obj->via.map.ptr[pos].key.via.str.ptr, _body->_obj->via.map.ptr[pos].key.via.str.size));
		}
		case Type::ARRAY: {
			if (pos >= _body->_obj->via.array.size) {
				return std::make_pair(0, end());
			}
			auto it = _body->array.begin() + pos;
			if (it >= _body->array.end()) {
				return std::make_pair(0, end());
			}

			auto& mobj = *it;
			auto pos_ = mobj._body->_pos;
			assert(pos_ < _body->_obj->via.array.size);
			auto p = &_body->_obj->via.array.ptr[pos_];
			std::memmove(p, p + 1, (_body->_obj->via.array.size - pos_ - 1) * sizeof(msgpack::object));
			--_body->_obj->via.array.size;
			// Erase from array:
			_body->array.pop_back();
			_update_array(pos_);

			auto next = _body->array.begin() + pos_;
			if (next >= _body->array.end()) {
				return std::make_pair(0, end());
			}

			return std::make_pair(1, MsgPack::iterator(this, next->_body->_pos));
		}
		default:
			THROW(msgpack::type_error);
	}
}


template <typename M, typename T, typename>
inline std::pair<MsgPack*, bool> MsgPack::_put(M&& o, T&& val, bool overwrite) {
	switch (o._body->getType()) {
		case Type::STR:
			return _put(std::string(o._body->_obj->via.str.ptr, o._body->_obj->via.str.size), std::forward<T>(val), overwrite);
		case Type::NEGATIVE_INTEGER:
			if (o._body->_obj->via.i64 < 0) {
				THROW(msgpack::type_error);
			}
			return _put(static_cast<size_t>(o._body->_obj->via.i64), std::forward<T>(val), overwrite);
		case Type::POSITIVE_INTEGER:
			return _put(static_cast<size_t>(o._body->_obj->via.u64), std::forward<T>(val), overwrite);
		default:
			THROW(msgpack::type_error);
	}
}


template <typename T>
inline std::pair<MsgPack*, bool> MsgPack::_put(const std::string& key, T&& val, bool overwrite) {
	switch (_body->getType()) {
		case Type::UNDEFINED:
			_body->_obj->type = msgpack::type::MAP;
			_body->_obj->via.map.ptr = nullptr;
			_body->_obj->via.map.size = 0;
		case Type::MAP: {
			auto it = _body->map.find(key);
			if (it == _body->map.end()) {
				_reserve_map(_body->_obj->via.map.size + 1);
				auto p = &_body->_obj->via.map.ptr[_body->_obj->via.map.size];
				p->key.type = msgpack::type::STR;
				p->key.via.str.size = static_cast<uint32_t>(key.size());
				auto ptr = static_cast<char*>(_body->_zone->allocate_align(p->key.via.str.size));
				std::memcpy(ptr, key.data(), p->key.via.str.size);
				p->key.via.str.ptr = ptr;
				p->val = msgpack::object(std::forward<T>(val), *_body->_zone);
				++_body->_obj->via.map.size;
				return std::make_pair(_init_map(_body->map.size()), true);
			} else {
				auto pos = it->second.second._body->_pos;
				if (overwrite) {
					auto p = &_body->_obj->via.map.ptr[pos];
					p->val = msgpack::object(std::forward<T>(val), *_body->_zone);
					return std::make_pair(&_body->at(pos), true);
				}
				return std::make_pair(&_body->at(pos), false);
			}
		}
		default:
			THROW(msgpack::type_error);
	}
}


template <typename T>
inline std::pair<MsgPack*, bool> MsgPack::_put(size_t pos, T&& val, bool overwrite) {
	switch (_body->getType()) {
		case Type::UNDEFINED:
			_body->_obj->type = msgpack::type::ARRAY;
			_body->_obj->via.array.ptr = nullptr;
			_body->_obj->via.array.size = 0;
		case Type::ARRAY:
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
				return std::make_pair(_init_array(_body->array.size()), true);
			} else {
				if (overwrite) {
					auto p = &_body->_obj->via.array.ptr[pos];
					*p = msgpack::object(std::forward<T>(val), *_body->_zone);
					return std::make_pair(&_body->at(pos), true);
				}
				return std::make_pair(&_body->at(pos), false);
			}
		default:
			THROW(msgpack::type_error);
	}
}


template <typename M, typename T, typename>
inline std::pair<MsgPack*, bool> MsgPack::_insert(M&& o, T&& val, bool overwrite) {
	switch (o._body->getType()) {
		case Type::STR:
			return _put(std::string(o._body->_obj->via.str.ptr, o._body->_obj->via.str.size), std::forward<T>(val), overwrite);
		case Type::NEGATIVE_INTEGER:
			if (o._body->_obj->via.i64 < 0) {
				THROW(msgpack::type_error);
			}
			return _insert(static_cast<size_t>(o._body->_obj->via.i64), std::forward<T>(val), overwrite);
		case Type::POSITIVE_INTEGER:
			return _insert(static_cast<size_t>(o._body->_obj->via.u64), std::forward<T>(val), overwrite);
		default:
			THROW(msgpack::type_error);
	}
}


template <typename T>
inline std::pair<MsgPack*, bool> MsgPack::_insert(size_t pos, T&& val, bool overwrite) {
	switch (_body->getType()) {
		case Type::UNDEFINED:
			_body->_obj->type = msgpack::type::ARRAY;
			_body->_obj->via.array.ptr = nullptr;
			_body->_obj->via.array.size = 0;
		case Type::ARRAY:
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
				return std::make_pair(_init_array(_body->array.size()), true);
			} else {
				if (overwrite) {
					_reserve_array(_body->_obj->via.array.size + 1);

					auto p = &_body->_obj->via.array.ptr[pos];
					std::memmove(p + 1, p, (_body->_obj->via.array.size - pos) * sizeof(msgpack::object));
					*p = msgpack::object(std::forward<T>(val), *_body->_zone);
					++_body->_obj->via.array.size;
					return std::make_pair(_init_array(_body->array.size()), true);
				} else {
					return std::make_pair(&_body->at(pos), false);
				}
			}
			break;
		default:
			THROW(msgpack::type_error);
	}
}


inline void MsgPack::_fill(bool recursive, bool lock) {
	if (_body->_lock) {
		return;
	}
	_body->_lock = lock;
	if (!_body->_initialized) {
		_init();
	}
	if (recursive) {
		switch (_body->getType()) {
			case Type::MAP:
				for (auto& item : _body->map) {
					item.second.second._fill(recursive, lock);
				}
				break;
			case Type::ARRAY:
				for (auto& item : _body->array) {
					item._fill(recursive, lock);
				}
				break;
			default:
				break;
		}
	}
}


inline void MsgPack::_fill(bool recursive, bool lock) const {
	const_cast<MsgPack*>(this)->_fill(recursive, lock);
}


template <typename M, typename T, typename>
inline MsgPack::iterator MsgPack::put(M&& o, T&& val) {
	_fill(false, false);
	auto pair = _put(std::forward<M>(o), std::forward<T>(val), true);
	return MsgPack::Iterator<MsgPack>(this, pair.first->_body->_pos);
}


template <typename T>
inline MsgPack::iterator MsgPack::put(const std::string& s, T&& val) {
	_fill(false, false);
	auto pair = _put(s, std::forward<T>(val), true);
	return MsgPack::Iterator<MsgPack>(this, pair.first->_body->_pos);
}


template <typename T>
inline MsgPack::iterator MsgPack::put(size_t pos, T&& val) {
	_fill(false, false);
	auto pair = _put(pos, std::forward<T>(val), true);
	return MsgPack::Iterator<MsgPack>(this, pair.first->_body->_pos);
}


template <typename M, typename T, typename>
inline std::pair<MsgPack::iterator, bool> MsgPack::emplace(M&& o, T&& val) {
	_fill(false, false);
	auto pair = _insert(std::forward<M>(o), std::forward<T>(val), false);
	return std::make_pair(MsgPack::Iterator<MsgPack>(this, pair.first->_body->_pos), pair.second);
}


template <typename T>
inline std::pair<MsgPack::iterator, bool> MsgPack::emplace(const std::string& s, T&& val) {
	_fill(false, false);
	auto pair = _put(s, std::forward<T>(val), false);
	return std::make_pair(MsgPack::Iterator<MsgPack>(this, pair.first->_body->_pos), pair.second);
}


template <typename T>
inline std::pair<MsgPack::iterator, bool> MsgPack::emplace(size_t pos, T&& val) {
	_fill(false, false);
	auto pair = _insert(pos, std::forward<T>(val), false);
	return std::make_pair(MsgPack::Iterator<MsgPack>(this, pair.first->_body->_pos), pair.second);
}


template <typename K, typename V>
inline std::pair<MsgPack::iterator, bool> MsgPack::insert(const std::pair<K&&, V&&>& val) {
	return emplace(std::forward<K>(val.first), std::forward<V>(val.second));
}


template <typename T>
inline void MsgPack::push_back(T&& v) {
	_fill(false, false);
	_put(size(), std::forward<T>(v), false);
}


inline void MsgPack::lock() const {
	_fill(true, true);
}


template <typename T>
inline decltype(auto) MsgPack::external(std::function<T(const msgpack::object&)> f) const {
	return f(*_body->_obj);
}


template <typename M, typename>
inline MsgPack& MsgPack::operator[](M&& o) {
	_fill(false, false);
	switch (o._body->getType()) {
		case Type::STR:
			return operator[](std::string(o._body->_obj->via.str.ptr, o._body->_obj->via.str.size));
		case Type::NEGATIVE_INTEGER:
			if (o._body->_obj->via.i64 < 0) {
				THROW(msgpack::type_error);
			}
			return operator[](static_cast<size_t>(o._body->_obj->via.i64));
		case Type::POSITIVE_INTEGER:
			return operator[](static_cast<size_t>(o._body->_obj->via.u64));
		default:
			THROW(msgpack::type_error);
	}
}


template <typename M, typename>
inline const MsgPack& MsgPack::operator[](M&& o) const {
	_fill(false, false);
	switch (o._const_body->getType()) {
		case Type::STR:
			return operator[](std::string(o._const_body->_obj->via.str.ptr, o._const_body->_obj->via.str.size));
		case Type::NEGATIVE_INTEGER:
			if (o._const_body->_obj->via.i64 < 0) {
				THROW(msgpack::type_error);
			}
			return operator[](static_cast<size_t>(o._const_body->_obj->via.i64));
		case Type::POSITIVE_INTEGER:
			return operator[](static_cast<size_t>(o._const_body->_obj->via.u64));
		default:
			THROW(msgpack::type_error);
	}
}


inline MsgPack& MsgPack::operator[](const std::string& key) {
	auto it = find(key);
	if (it == end()) {
		return *_put(key, MsgPack::undefined(), false).first;
	}
	return _body->at(key);
}


inline const MsgPack& MsgPack::operator[](const std::string& key) const {
	auto it = find(key);
	if (it == cend()) {
		return MsgPack::undefined();
	}
	return _const_body->at(key);
}


inline MsgPack& MsgPack::operator[](size_t pos) {
	auto it = find(pos);
	if (it == end()) {
		return *_put(pos, MsgPack::undefined(), false).first;
	}
	return _body->at(pos);
}


inline const MsgPack& MsgPack::operator[](size_t pos) const {
	auto it = find(pos);
	if (it == cend()) {
		return MsgPack::undefined();
	}
	return _const_body->at(pos);
}


template <typename M, typename>
inline MsgPack& MsgPack::at(M&& o) {
	_fill(false, false);
	switch (o._body->getType()) {
		case Type::STR:
			return at(std::string(o._body->_obj->via.str.ptr, o._body->_obj->via.str.size));
		case Type::NEGATIVE_INTEGER:
			if (o._body->_obj->via.i64 < 0) {
				THROW(msgpack::type_error);
			}
			return at(static_cast<size_t>(o._body->_obj->via.i64));
		case Type::POSITIVE_INTEGER:
			return at(static_cast<size_t>(o._body->_obj->via.u64));
		default:
			THROW(msgpack::type_error);
	}
}


template <typename M, typename>
inline const MsgPack& MsgPack::at(M&& o) const {
	_fill(false, false);
	switch (o._const_body->getType()) {
		case Type::STR:
			return at(std::string(o._const_body->_obj->via.str.ptr, o._const_body->_obj->via.str.size));
		case Type::NEGATIVE_INTEGER:
			if (o._const_body->_obj->via.i64 < 0) {
				THROW(msgpack::type_error);
			}
			return at(static_cast<size_t>(o._const_body->_obj->via.i64));
		case Type::POSITIVE_INTEGER:
			return at(static_cast<size_t>(o._const_body->_obj->via.u64));
		default:
			THROW(msgpack::type_error);
	}
}


inline MsgPack& MsgPack::at(const std::string& key) {
	_fill(false, false);
	switch (_body->getType()) {
		case Type::UNDEFINED:
			THROW(out_of_range, "undefined");
		case Type::MAP:
			return _body->at(key);
		default:
			THROW(msgpack::type_error);
	}
}

inline const MsgPack& MsgPack::at(const std::string& key) const {
	_fill(false, false);
	switch (_const_body->getType()) {
		case Type::UNDEFINED:
			THROW(out_of_range, "undefined");
		case Type::MAP:
			return _const_body->at(key);
		default:
			THROW(msgpack::type_error);
	}
}


inline MsgPack& MsgPack::at(size_t pos) {
	_fill(false, false);
	switch (_body->getType()) {
		case Type::UNDEFINED:
			THROW(out_of_range, "undefined");
		case Type::MAP:
			if (pos >= _body->_obj->via.map.size) {
				THROW(out_of_range, "The map only contains " + std::to_string(_body->_obj->via.map.size) + " elements");
			}
			return at(std::string(_body->_obj->via.map.ptr[pos].key.via.str.ptr, _body->_obj->via.map.ptr[pos].key.via.str.size));
		case Type::ARRAY:
			return _body->at(pos);
		default:
			THROW(msgpack::type_error);
	}
}


inline const MsgPack& MsgPack::at(size_t pos) const {
	_fill(false, false);
	switch (_const_body->getType()) {
		case Type::UNDEFINED:
			THROW(out_of_range, "undefined");
		case Type::MAP:
			if (pos >= _const_body->_obj->via.map.size) {
				THROW(out_of_range, "The map only contains " + std::to_string(_const_body->_obj->via.map.size) + " elements");
			}
			return at(std::string(_const_body->_obj->via.map.ptr[pos].key.via.str.ptr, _const_body->_obj->via.map.ptr[pos].key.via.str.size));
		case Type::ARRAY:
			return _const_body->at(pos);
		default:
			THROW(msgpack::type_error);
	}
}


inline MsgPack& MsgPack::path(const std::vector<std::string>& path) {
	auto current = this;
	for (const auto& s : path) {
		switch (current->_const_body->getType()) {
			case Type::MAP: {
				current = &current->at(s);
				break;
			}
			case Type::ARRAY: {
				std::string::size_type sz;
				int pos = std::stoi(s, &sz);
				if (pos < 0 || sz != s.size()) {
					THROW(invalid_argument, "The index for the array must be a positive integer, it is: " + s);
				}
				current = &current->at(pos);
				break;
			}

			default:
				THROW(invalid_argument, "The container must be a map or an array to access: " + s);
		}
	}

	return *current;
}


inline const MsgPack& MsgPack::path(const std::vector<std::string>& path) const {
	auto current = this;
	for (const auto& s : path) {
		switch (current->getType()) {
			case Type::MAP: {
				current = &current->at(s);
				break;
			}
			case Type::ARRAY: {
				std::string::size_type sz;
				int pos = std::stoi(s, &sz);
				if (pos < 0 || sz != s.size()) {
					THROW(invalid_argument, "The index for the array must be a positive integer, it is: " + s);
				}
				current = &current->at(pos);
				break;
			}

			default:
				THROW(invalid_argument, "The container must be a map or an array to access: " + s);
		}
	}

	return *current;
}


template <typename M, typename>
inline MsgPack::iterator MsgPack::find(M&& o) {
	_fill(false, false);
	switch (o._body->getType()) {
		case Type::STR:
			return _find(std::string(o._body->_obj->via.str.ptr, o._body->_obj->via.str.size));
		case Type::NEGATIVE_INTEGER:
			if (o._body->_obj->via.i64 < 0) {
				THROW(msgpack::type_error);
			}
			return _find(static_cast<size_t>(o._body->_obj->via.i64));
		case Type::POSITIVE_INTEGER:
			return _find(static_cast<size_t>(o._body->_obj->via.u64));
		default:
			THROW(msgpack::type_error);
	}
}


template <typename M, typename>
inline MsgPack::const_iterator MsgPack::find(M&& o) const {
	_fill(false, false);
	switch (o._const_body->getType()) {
		case Type::STR:
			return _find(std::string(o._const_body->_obj->via.str.ptr, o._const_body->_obj->via.str.size));
		case Type::NEGATIVE_INTEGER:
			if (o._const_body->_obj->via.i64 < 0) {
				THROW(msgpack::type_error);
			}
			return _find(static_cast<size_t>(o._const_body->_obj->via.i64));
		case Type::POSITIVE_INTEGER:
			return _find(static_cast<size_t>(o._const_body->_obj->via.u64));
		default:
			THROW(msgpack::type_error);
	}
}


inline MsgPack::iterator MsgPack::find(const std::string& s) {
	_fill(false, false);
	return _find(s);
}


inline MsgPack::const_iterator MsgPack::find(const std::string& s) const {
	_fill(false, false);
	return _find(s);
}


inline MsgPack::iterator MsgPack::find(size_t pos) {
	_fill(false, false);
	return _find(pos);
}


inline MsgPack::const_iterator MsgPack::find(size_t pos) const {
	_fill(false, false);
	return _find(pos);
}


template <typename M, typename>
inline void MsgPack::update(M&& o) {
	switch (o._body->getType()) {
		case Type::MAP:
			for (const auto& key : o) {
				const auto& val = o.at(key);
				if (find(key) == end()) {
					put(key, val);
				} else {
					auto& item = at(key);
					if (item.is_map()) {
						item.update(val);
					} else {
						item = val;
					}
				}
			}
			break;
		default:
			THROW(msgpack::type_error);
	}
}


template <typename T>
inline size_t MsgPack::count(T&& v) const {
	return find(std::forward<T>(v)) == end() ? 0 : 1;
}


template <typename M, typename>
inline size_t MsgPack::erase(M&& o) {
	_fill(false, false);
	return _erase(std::forward<M>(o)).first;
}


inline size_t MsgPack::erase(const std::string& s) {
	_fill(false, false);
	return _erase(s).first;
}


inline size_t MsgPack::erase(size_t pos) {
	_fill(false, false);
	return _erase(pos).first;
}


inline MsgPack::iterator MsgPack::erase(MsgPack::iterator it) {
	_fill(false, false);
	return _erase(it._off).second;
}


inline void MsgPack::clear() noexcept {
	_body->_initialized = false;

	switch (_body->getType()) {
		case Type::MAP:
			_body->_obj->via.map.size = 0;
			_body->map.clear();
			break;
		case Type::ARRAY:
			_body->_obj->via.array.size = 0;
			_body->array.clear();
			break;
		case Type::STR:
			_body->_obj->via.str.size = 0;
			break;
		default:
			break;
	}
}


inline std::size_t MsgPack::hash() const {
	switch (_body->getType()) {
		case Type::MAP: {
			_fill(false, false);
			size_t pos = 0;
			std::size_t hash = 0;
			const auto pend = &_body->_obj->via.map.ptr[_body->_obj->via.map.size];
			for (auto p = &_body->_obj->via.map.ptr[pos]; p != pend; ++p, ++pos) {
				if (p->key.type != msgpack::type::STR) {
					THROW(msgpack::type_error);
				}
				auto val = MsgPack(std::make_shared<Body>(_body->_zone, _body->_base, _body, true, 0, nullptr, &p->val));
				hash ^= std::hash<std::string>{}(std::string(p->key.via.str.ptr, p->key.via.str.size));
				hash ^= val.hash();
			}
			return hash;
		}
		default:
			return std::hash<std::string>{}(serialise());
	}
}


inline MsgPack::operator bool() const {
	switch (_const_body->getType()) {
		case Type::MAP:
		case Type::ARRAY:
		case Type::STR:
			return size() > 0;
		case Type::NEGATIVE_INTEGER:
			return _const_body->_obj->via.i64 != 0;
		case Type::POSITIVE_INTEGER:
			return _const_body->_obj->via.u64 != 0;
		case Type::FLOAT:
			return _const_body->_obj->via.f64 != 0;
		case Type::BOOLEAN:
			return _const_body->_obj->via.boolean;
		default:
			return false;
	}
}


inline void MsgPack::reserve(size_t n) {
	switch (_body->getType()) {
		case Type::MAP:
			return _reserve_map(n);
		case Type::ARRAY:
			return _reserve_array(n);
		default:
			return;
	}
}


inline size_t MsgPack::capacity() const noexcept {
	return _const_body->_capacity;
}


inline size_t MsgPack::size() const noexcept {
	return _const_body->size();
}


inline bool MsgPack::empty() const noexcept {
	return size() == 0;
}


inline uint64_t MsgPack::u64() const {
	switch (_const_body->getType()) {
		case Type::NEGATIVE_INTEGER: {
			auto val = _const_body->_obj->via.i64;
			if (val < 0) {
				THROW(msgpack::type_error);
			}
			return val;
		}
		case Type::POSITIVE_INTEGER:
			return _const_body->_obj->via.u64;
		default:
			THROW(msgpack::type_error);
	}
}


inline int64_t MsgPack::i64() const {
	switch (_const_body->getType()) {
		case Type::NEGATIVE_INTEGER:
			return _const_body->_obj->via.i64;
		case Type::POSITIVE_INTEGER: {
			auto val = _const_body->_obj->via.u64;
			if (val > INT64_MAX) {
				THROW(msgpack::type_error);
			}
			return val;
		}
		default:
			THROW(msgpack::type_error);
	}
}


inline double MsgPack::f64() const {
	switch (_const_body->getType()) {
		case Type::NEGATIVE_INTEGER:
			return _const_body->_obj->via.i64;
		case Type::POSITIVE_INTEGER:
			return _const_body->_obj->via.u64;
		case Type::FLOAT:
			return _const_body->_obj->via.f64;
		default:
			THROW(msgpack::type_error);
	}
}


inline std::string MsgPack::str() const {
	if (_const_body->getType() == Type::STR) {
		return std::string(_const_body->_obj->via.str.ptr, _const_body->_obj->via.str.size);
	}

	THROW(msgpack::type_error);
}


inline bool MsgPack::boolean() const {
	if (_const_body->getType() == Type::BOOLEAN) {
		return _const_body->_obj->via.boolean;
	}

	THROW(msgpack::type_error);
}


inline uint64_t MsgPack::as_u64() const {
	switch (_const_body->getType()) {
		case Type::NIL:
			return 0;
		case Type::BOOLEAN:
			return _const_body->_obj->via.boolean ? 1 : 0;
		case Type::POSITIVE_INTEGER:
			return _const_body->_obj->via.u64;
		case Type::NEGATIVE_INTEGER:
			return _const_body->_obj->via.i64;
		case Type::FLOAT:
			return _const_body->_obj->via.f64;
		case Type::STR:
			try {
				return std::stoull(std::string(_const_body->_obj->via.str.ptr, _const_body->_obj->via.str.size));
			} catch (const std::out_of_range&) {
			} catch (const std::invalid_argument&) { }
			return 0;
		case Type::BIN:
			try {
				return std::stoull(std::string(_const_body->_obj->via.bin.ptr, _const_body->_obj->via.bin.size));
			} catch (const std::out_of_range&) {
			} catch (const std::invalid_argument&) { }
			return 0;
		default:
			return 0;
	}
}


inline int64_t MsgPack::as_i64() const {
	switch (_const_body->getType()) {
		case Type::NIL:
			return 0;
		case Type::BOOLEAN:
			return _const_body->_obj->via.boolean ? 1 : 0;
		case Type::POSITIVE_INTEGER:
			return _const_body->_obj->via.u64;
		case Type::NEGATIVE_INTEGER:
			return _const_body->_obj->via.i64;
		case Type::FLOAT:
			return _const_body->_obj->via.f64;
		case Type::STR:
			try {
				return std::stoll(std::string(_const_body->_obj->via.str.ptr, _const_body->_obj->via.str.size));
			} catch (const std::out_of_range&) {
			} catch (const std::invalid_argument&) { }
			return 0;
		case Type::BIN:
			try {
				return std::stoll(std::string(_const_body->_obj->via.bin.ptr, _const_body->_obj->via.bin.size));
			} catch (const std::out_of_range&) {
			} catch (const std::invalid_argument&) { }
			return 0;
		default:
			return 0;
	}
}


inline double MsgPack::as_f64() const {
	switch (_const_body->getType()) {
		case Type::NIL:
			return 0;
		case Type::BOOLEAN:
			return _const_body->_obj->via.boolean ? 1 : 0;
		case Type::POSITIVE_INTEGER:
			return _const_body->_obj->via.u64;
		case Type::NEGATIVE_INTEGER:
			return _const_body->_obj->via.i64;
		case Type::FLOAT:
			return _const_body->_obj->via.f64;
		case Type::STR:
			try {
				return std::stod(std::string(_const_body->_obj->via.str.ptr, _const_body->_obj->via.str.size));
			} catch (const std::out_of_range&) {
			} catch (const std::invalid_argument&) { }
			return 0;
		case Type::BIN:
			try {
				return std::stod(std::string(_const_body->_obj->via.bin.ptr, _const_body->_obj->via.bin.size));
			} catch (const std::out_of_range&) {
			} catch (const std::invalid_argument&) { }
			return 0;
		default:
			return 0;
	}
}


inline std::string MsgPack::as_str() const {
	std::ostringstream oss;

	switch (_const_body->getType()) {
		case Type::NIL:
			return "nil";
		case Type::BOOLEAN:
			return _const_body->_obj->via.boolean ? "true" : "false";
		case Type::POSITIVE_INTEGER:
			return std::to_string(_const_body->_obj->via.u64);
		case Type::NEGATIVE_INTEGER:
			return std::to_string(_const_body->_obj->via.i64);
		case Type::FLOAT:
			return std::to_string(_const_body->_obj->via.f64);
		case Type::STR:
			return std::string(_const_body->_obj->via.str.ptr, _const_body->_obj->via.str.size);
		case Type::ARRAY:
			oss << *_const_body->_obj;
			return oss.str();
		case Type::MAP:
			oss << *_const_body->_obj;
			return oss.str();
		case Type::BIN:
			return std::string(_const_body->_obj->via.str.ptr, _const_body->_obj->via.str.size);
		case Type::UNDEFINED:
			return "undefined";
	}
}


inline bool MsgPack::as_boolean() const {
	return operator bool();
}


inline rapidjson::Document MsgPack::as_document() const {
	rapidjson::Document doc;
	_const_body->_obj->convert(&doc);
	return doc;
}


inline bool MsgPack::is_undefined() const noexcept {
	return _const_body->getType() == Type::UNDEFINED;
}


inline bool MsgPack::is_null() const noexcept {
	return _const_body->getType() == Type::NIL;
}


inline bool MsgPack::is_boolean() const noexcept {
	return _const_body->getType() == Type::BOOLEAN;
}


inline bool MsgPack::is_number() const noexcept {
	switch (_const_body->getType()) {
		case Type::NEGATIVE_INTEGER:
		case Type::POSITIVE_INTEGER:
		case Type::FLOAT:
			return true;
		default:
			return false;
	}
}


inline bool MsgPack::is_integer() const noexcept {
	switch (_const_body->getType()) {
		case Type::NEGATIVE_INTEGER:
		case Type::POSITIVE_INTEGER:
			return true;
		default:
			return false;
	}
}


inline bool MsgPack::is_float() const noexcept {
	return _const_body->getType() == Type::FLOAT;
}


inline bool MsgPack::is_map() const noexcept {
	return _const_body->getType() == Type::MAP;
}


inline bool MsgPack::is_array() const noexcept {
	return _const_body->getType() == Type::ARRAY;
}


inline bool MsgPack::is_string() const noexcept {
	return _const_body->getType() == Type::STR;
}


inline MsgPack::Type MsgPack::getType() const noexcept {
	return _const_body->getType();
}


inline std::string MsgPack::getStrType() const noexcept {
	return _const_body->getStrType();
}


inline bool MsgPack::operator==(const MsgPack& other) const {
	return this == &other || *_const_body->_obj == *other._const_body->_obj;
}


inline bool MsgPack::operator!=(const MsgPack& other) const {
	return !operator==(other);
}


inline MsgPack MsgPack::operator+(double val) {
	MsgPack o(_body);
	switch (o.getType()) {
		case Type::NEGATIVE_INTEGER:
			o._body->_obj->via.i64 += val;
			return o;
		case Type::POSITIVE_INTEGER:
			o._body->_obj->via.u64 += val;
			return o;
		case Type::FLOAT:
			o._body->_obj->via.f64 += val;
			return o;
		default:
			THROW(msgpack::type_error);
	}
}


inline MsgPack& MsgPack::operator+=(double val) {
	switch (_body->getType()) {
		case Type::NEGATIVE_INTEGER:
			_body->_obj->via.i64 += val;
			return *this;
		case Type::POSITIVE_INTEGER:
			_body->_obj->via.u64 += val;
			return *this;
		case Type::FLOAT:
			_body->_obj->via.f64 += val;
			return *this;
		default:
			THROW(msgpack::type_error);
	}
}


inline std::string MsgPack::to_string(int indent) const {
	if (indent >= 0) {
		rapidjson::Document doc = as_document();
		rapidjson::StringBuffer buffer;
		rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
		writer.SetIndent(' ', indent);
		doc.Accept(writer);
		return std::string(buffer.GetString(), buffer.GetSize());
	} else {
		rapidjson::Document doc = as_document();
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		doc.Accept(writer);
		return std::string(buffer.GetString(), buffer.GetSize());
	}
}


inline std::ostream& MsgPack::operator<<(std::ostream& s) const {
	s << *_const_body->_obj;
	return s;
}


inline std::string MsgPack::unformatted_string() const {
	if (_body->getType() == Type::STR) {
		return std::string(_body->_obj->via.str.ptr, _body->_obj->via.str.size);
	}
	THROW(msgpack::type_error);
}


template <typename B>
inline std::string MsgPack::serialise() const {
	B buf;
	msgpack::pack(&buf, *_const_body->_obj);
	return std::string(buf.data(), buf.size());
}


inline MsgPack MsgPack::unserialise(const std::string& s) {
	return MsgPack(msgpack::unpack(s.data(), s.size()).get());
}


inline std::ostream& operator<<(std::ostream& s, const MsgPack& o){
	return o.operator<<(s);
}


namespace std {
	template<>
	struct hash<MsgPack> {
		std::size_t operator()(const MsgPack &m) const {
			return m.hash();
		}
	};
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
					o = msgpack::object(*v._const_body->_obj);
				}
			};

			template <>
			struct object_with_zone<MsgPack> {
				void operator()(msgpack::object::with_zone& o, const MsgPack& v) const {
					msgpack::object obj(*v._const_body->_obj, o.zone);
					o.type = obj.type;
					o.via = obj.via;
				}
			};

			template <>
			struct object<std::nullptr_t> {
				void operator()(msgpack::object& o, std::nullptr_t) const {
					msgpack::object nil;
					o = msgpack::object(nil);
				}
			};

			template <>
			struct object_with_zone<std::nullptr_t> {
				void operator()(msgpack::object::with_zone& o, std::nullptr_t) const {
					msgpack::object nil;
					msgpack::object obj(nil, o.zone);
					o.type = obj.type;
					o.via = obj.via;
				}
			};
		} // namespace adaptor
	} // MSGPACK_API_VERSION_NAMESPACE(MSGPACK_DEFAULT_API_NS)
} // namespace msgpack
