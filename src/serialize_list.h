/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
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

#include <string>                                 // for std::string
#include <string_view>                            // for std::string_view

#include "exception_engine.h"                     // for SerializationError
#include "length.h"                               // for serialize_string
#include "serialize.h"


constexpr char SERIALIZED_LIST_MAGIC  = '\0';


template <class Impl, class T>
class SerializeList {
	template <typename Value>
	class Iterator : public std::iterator<std::forward_iterator_tag, Value> {
		using SerializeImpl = SerializeList<Impl, T>;

		friend SerializeImpl;

		const Impl* owner;
		const char* pos;
		size_t length;
		mutable T value;

		Iterator()
			: owner(nullptr),
			  pos(nullptr),
			  length(0) { }

		Iterator(const SerializeImpl* owner_, const char* ptr_)
			: owner(static_cast<const Impl*>(owner_)),
			  pos(ptr_),
			  length(0)
		{
			if (pos != owner->_end) {
				if (owner->single()) {
					length = owner->_serialized.size();
				} else {
					length = owner->get_length(&pos);
				}
			}
		}

		Iterator(const SerializeImpl* owner_, const char* pos_, size_t length_)
			: owner(static_cast<const Impl*>(owner_)),
			  pos(pos_),
			  length(length_) { }

		void next() {
			pos += length;
			if (pos == owner->_end) {
				length = 0;
			} else {
				length = owner->get_length(&pos);
			}
		}

	public:
		Iterator& operator++() {
			next();
			return *this;
		}

		Iterator operator++(int) {
			Iterator it = *this;
			next();
			return it;
		}

		Iterator operator+=(size_t n) {
			while (n) {
				next();
				--n;
			}
			return *this;
		}

		Iterator operator+(size_t n) const {
			auto it = *this;
			while (n) {
				it.next();
				--n;
			}
			return it;
		}

		Value& operator*() const {
			value = owner->get_value(pos, length);
			return value;
		}

		Value& operator*() {
			value = owner->get_value(pos, length);
			return value;
		}

		Value* operator->() const {
			return &operator*();
		}

		Value* operator->() {
			return &operator*();
		}

		int compare(std::string_view ref) const noexcept {
			return -ref.compare(0, ref.size(), pos, length);
		}

		bool operator==(const Iterator& other) const noexcept {
			return pos == other.pos;
		}

		bool operator!=(const Iterator& other) const noexcept {
			return !operator==(other);
		}

		explicit operator bool() const noexcept {
			return pos != owner->_end;
		}
	};

public:
	using iterator = Iterator<T>;
	using const_iterator = Iterator<const T>;

private:
	iterator _i_begin;
	const_iterator _i_cbegin;
	iterator _i_end;
	const_iterator _i_cend;
	mutable iterator _i_last;
	mutable const_iterator _i_clast;

	iterator& get_last() const {
		if (!_i_last.owner) {
			if (!_i_clast.owner) {
				auto it = _i_cbegin;
				_i_clast = it;
				while (++it) {
					_i_clast = it;
				}
			}
			_i_last = iterator(_i_clast.owner, _i_clast.pos, _i_clast.length);
		}
		return _i_last;
	}

	const_iterator& get_clast() const {
		if (!_i_clast.owner) {
			auto it = _i_cbegin;
			_i_clast = it;
			while (++it) {
				_i_clast = it;
			}
		}
		return _i_clast;
	}

protected:
	std::string_view _serialized;
	const char *_ptr;
	const char *_end;
	bool _single;

	void init() {
		_ptr = _serialized.data();
		_end = _ptr + _serialized.size();
		_single = true;

		if (!_serialized.empty()) {
			if (*_ptr == SERIALIZED_LIST_MAGIC) {
				++_ptr;
				_single = false;
			}
		}
		_i_begin = iterator(this, _ptr);
		_i_end = iterator(this, _end);
		_i_cbegin = const_iterator(_i_begin.owner, _i_begin.pos, _i_begin.length);
		_i_cend = const_iterator(_i_end.owner, _i_end.pos, _i_end.length);
	}

	SerializeList(std::string_view serialized)
		: _serialized(serialized)
	{
		init();
	}

	SerializeList(SerializeList&& o) noexcept
		: _serialized(std::move(o._serialized))
	{
		init();
	}

	SerializeList(const SerializeList& o) noexcept
		: _serialized(o._serialized)
	{
		init();
	}

	SerializeList& operator=(SerializeList&& o) noexcept {
		_serialized = std::move(o._serialized);
		init();
	}

	SerializeList& operator=(const SerializeList& o) noexcept {
		_serialized = o._serialized;
		init();
	}

public:
	bool empty() const noexcept {
		return _serialized.empty();
	}

	bool single() const noexcept {
		return _single;
	}

	size_t size() const {
		return std::distance(begin(), end());
	}

	iterator begin() {
		return _i_begin;
	}

	const_iterator begin() const {
		return _i_cbegin;
	}

	const_iterator cbegin() const {
		return _i_cbegin;
	}

	iterator last() {
		return get_last();
	}

	const_iterator last() const {
		return get_clast();
	}

	const_iterator clast() const {
		return get_clast();
	}

	iterator end() {
		return _i_end;
	}

	const_iterator end() const {
		return _i_cend;
	}

	const_iterator cend() const {
		return _i_cend;
	}

	T& front() {
		return *_i_begin;
	}

	const T& front() const {
		return *_i_cbegin;
	}

	T& back() {
		return *get_last();
	}

	const T& back() const {
		return *get_clast();
	}
};


class StringList : public SerializeList<StringList, std::string_view> {
	size_t get_length(const char** pos) const {
		return unserialize_length_and_check(pos, _end);
	}

	std::string_view get_value(const char* pos, size_t length) const {
		return std::string_view(pos, length);
	}

	friend class SerializeList<StringList, std::string_view>;

public:
	template <typename S>
	StringList(S&& serialized)
		: SerializeList<StringList, std::string_view>(std::forward<S>(serialized)) { }

	template <typename InputIt>
	static std::string serialize(InputIt first, InputIt last) {
		const auto size = std::distance(first, last);
		if (size == 1) {
			return *first;
		} else if (size > 1) {
			std::string serialized(1, SERIALIZED_LIST_MAGIC);
			for ( ; first != last; ++first) {
				serialized.append(serialize_string(*first));
			}
			return serialized;
		}

		return std::string();
	}

	template <typename OutputIt>
	static void unserialize(const char** ptr, const char* end, OutputIt d_first) {
		const char* pos = *ptr;
		if (pos != end) {
			if (*pos == SERIALIZED_LIST_MAGIC) {
				++pos;
				for ( ; pos != end; ++d_first) {
					const auto length = unserialize_length_and_check(&pos, end);
					*d_first = typename OutputIt::container_type::value_type(std::string_view(pos, length));
					pos += length;
				}
			} else {
				*d_first = typename OutputIt::container_type::value_type(std::string_view(pos, end - pos));
			}
		}
	}

	template <typename OutputIt>
	static void unserialize(std::string_view serialized, OutputIt d_first) {
		auto ptr = serialized.data();
		auto end = ptr + serialized.size();
		unserialize(&ptr, end, d_first);
	}
};


class CartesianList : public SerializeList<CartesianList, Cartesian> {
	size_t get_length(const char**) const {
		return SERIALIZED_LENGTH_CARTESIAN;
	}

	Cartesian get_value(const char* pos, size_t length) const {
		return Unserialize::cartesian(std::string_view(pos, length));
	}

	friend class SerializeList<CartesianList, Cartesian>;

public:
	template <typename S>
	CartesianList(S&& serialized)
		: SerializeList<CartesianList, Cartesian>(std::forward<S>(serialized))
	{
		if (((_end - _ptr) % SERIALIZED_LENGTH_CARTESIAN) != 0) {
			THROW(SerializationError, "Bad encoded length: insufficient data");
		}
	}

	size_t size() const noexcept {
		return _serialized.size() / SERIALIZED_LENGTH_CARTESIAN;
	}

	template <typename InputIt>
	static std::string serialize(InputIt first, InputIt last) {
		const auto size = std::distance(first, last);
		if (size == 1) {
			return Serialize::cartesian(*first);
		} else if (size > 1) {
			std::string serialized;
			serialized.reserve(SERIALIZED_LENGTH_CARTESIAN * size + 1);
			serialized.push_back(SERIALIZED_LIST_MAGIC);
			for ( ; first != last; ++first) {
				serialized.append(Serialize::cartesian(*first));
			}
			return serialized;
		}

		return std::string();
	}

	template <typename OutputIt>
	static void unserialize(const char** ptr, const char* end, OutputIt d_first) {
		const char* pos = *ptr;
		if (pos != end) {
			if (*pos == SERIALIZED_LIST_MAGIC) {
				++pos;
				if ((end - pos) % SERIALIZED_LENGTH_CARTESIAN == 0) {
					for ( ; pos != end; ++d_first) {
						*d_first = Unserialize::cartesian(std::string_view(pos, SERIALIZED_LENGTH_CARTESIAN));
						pos += SERIALIZED_LENGTH_CARTESIAN;
					}
				} else {
					THROW(SerializationError, "Bad encoded length: insufficient data");
				}
			} else if ((end - pos) == SERIALIZED_LENGTH_CARTESIAN) {
				*d_first = Unserialize::cartesian(std::string_view(pos, SERIALIZED_LENGTH_CARTESIAN));
			} else {
				THROW(SerializationError, "Bad encoded length: insufficient data");
			}
		}
	}

	template <typename OutputIt>
	static void unserialize(std::string_view serialized, OutputIt d_first) {
		auto ptr = serialized.data();
		auto end = ptr + serialized.size();
		unserialize(&ptr, end, d_first);
	}
};


class RangeList : public SerializeList<RangeList, range_t> {
	size_t get_length(const char**) const {
		return SERIALIZED_LENGTH_RANGE;
	}

	range_t get_value(const char* pos, size_t length) const {
		return Unserialize::range(std::string_view(pos, length));
	}

	friend class SerializeList<RangeList, range_t>;

public:
	template <typename S>
	RangeList(S&& serialized)
		: SerializeList<RangeList, range_t>(std::forward<S>(serialized))
	{
		if (((_end - _ptr) % SERIALIZED_LENGTH_RANGE) != 0) {
			THROW(SerializationError, "Bad encoded length: insufficient data");
		}
	}

	size_t size() const noexcept {
		return _serialized.size() / SERIALIZED_LENGTH_RANGE;
	}

	template <typename InputIt>
	static std::string serialize(InputIt first, InputIt last) {
		const auto size = std::distance(first, last);
		if (size == 1) {
			return Serialize::range(*first);
		} else if (size > 1) {
			std::string serialized;
			serialized.reserve(SERIALIZED_LENGTH_RANGE * size + 1);
			serialized.push_back(SERIALIZED_LIST_MAGIC);
			for ( ; first != last; ++first) {
				serialized.append(Serialize::range(*first));
			}
			return serialized;
		}

		return std::string();
	}

	template <typename OutputIt>
	static void unserialize(const char** ptr, const char* end, OutputIt d_first) {
		const char* pos = *ptr;
		if (pos != end) {
			if (*pos == SERIALIZED_LIST_MAGIC) {
				++pos;
				if ((end - pos) % SERIALIZED_LENGTH_RANGE == 0) {
					for ( ; pos != end; ++d_first) {
						*d_first = Unserialize::range(std::string_view(pos, SERIALIZED_LENGTH_RANGE));
						pos += SERIALIZED_LENGTH_RANGE;
					}
				} else {
					THROW(SerializationError, "Bad encoded length: insufficient data");
				}
			} else if ((end - pos) == SERIALIZED_LENGTH_RANGE) {
				*d_first = Unserialize::range(std::string_view(pos, SERIALIZED_LENGTH_RANGE));
			} else {
				THROW(SerializationError, "Bad encoded length: insufficient data");
			}
		}
	}

	template <typename OutputIt>
	static void unserialize(std::string_view serialized, OutputIt d_first) {
		auto ptr = serialized.data();
		auto end = ptr + serialized.size();
		unserialize(&ptr, end, d_first);
	}
};
