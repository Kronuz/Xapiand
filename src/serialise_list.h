/*
 * Copyright (c) 2015-2018 Dubalu LLC
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

#include "string_view.hh"  // for std::string_view

#include "length.h"
#include "serialise.h"


constexpr char SERIALISED_LIST_MAGIC  = '\0';


template <class Impl, class T>
class SerialiseList {
	template <typename Value>
	class Iterator : public std::iterator<std::forward_iterator_tag, Value> {
		using SerialiseImpl = SerialiseList<Impl, T>;

		friend SerialiseImpl;

		const Impl* owner;
		const char* pos;
		size_t length;
		mutable T value;

		Iterator()
			: owner(nullptr),
			  pos(nullptr),
			  length(0) { }

		Iterator(const SerialiseImpl* owner_, const char* ptr_)
			: owner(static_cast<const Impl*>(owner_)),
			  pos(ptr_),
			  length(0)
		{
			if (pos != owner->_end) {
				if (owner->single()) {
					length = owner->_serialised.length();
				} else {
					length = owner->get_length(&pos);
				}
			}
		}

		Iterator(const SerialiseImpl* owner_, const char* pos_, size_t length_)
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
	std::string _serialised;
	const char *_ptr;
	const char *_end;
	bool _single;

	void init() {
		_ptr = _serialised.data();
		_end = _ptr + _serialised.length();
		_single = true;

		if (!_serialised.empty()) {
			if (*_ptr == SERIALISED_LIST_MAGIC) {
				++_ptr;
				_single = false;
			}
		}
		_i_begin = iterator(this, _ptr);
		_i_end = iterator(this, _end);
		_i_cbegin = const_iterator(_i_begin.owner, _i_begin.pos, _i_begin.length);
		_i_cend = const_iterator(_i_end.owner, _i_end.pos, _i_end.length);
	}

	template <typename S, typename = std::enable_if_t<std::is_same<std::string, std::decay_t<S>>::value or std::is_same<std::string_view, std::decay_t<S>>::value>>
	SerialiseList(S&& serialised)
		: _serialised(std::forward<S>(serialised))
	{
		init();
	}

	SerialiseList(SerialiseList&& o) noexcept
		: _serialised(std::move(o._serialised))
	{
		init();
	}

	SerialiseList(const SerialiseList& o) noexcept
		: _serialised(o._serialised)
	{
		init();
	}

	SerialiseList& operator=(SerialiseList&& o) noexcept {
		_serialised = std::move(o._serialised);
		init();
	}

	SerialiseList& operator=(const SerialiseList& o) noexcept {
		_serialised = o._serialised;
		init();
	}

public:
	bool empty() const noexcept {
		return _serialised.empty();
	}

	bool single() const noexcept {
		return _single;
	}

	size_t size() const noexcept {
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


class StringList : public SerialiseList<StringList, std::string> {
	size_t get_length(const char** pos) const {
		return unserialise_length(pos, _end, true);
	}

	std::string get_value(const char* pos, size_t length) const {
		return std::string(pos, length);
	}

	friend class SerialiseList<StringList, std::string>;

public:
	template <typename S, typename = std::enable_if_t<std::is_same<std::string, std::decay_t<S>>::value or std::is_same<std::string_view, std::decay_t<S>>::value>>
	StringList(S&& serialised)
		: SerialiseList<StringList, std::string>(std::forward<S>(serialised)) { }

	template <typename InputIt>
	static std::string serialise(InputIt first, InputIt last) {
		const auto size = std::distance(first, last);
		if (size == 1) {
			return *first;
		} else if (size > 1) {
			std::string serialised(1, SERIALISED_LIST_MAGIC);
			for ( ; first != last; ++first) {
				serialised.append(serialise_length(first->length())).append(*first);
			}
			return serialised;
		}

		return std::string();
	}

	template <typename OutputIt>
	static void unserialise(const char** ptr, const char* end, OutputIt d_first) {
		const char* pos = *ptr;
		if (pos != end) {
			if (*pos == SERIALISED_LIST_MAGIC) {
				++pos;
				for ( ; pos != end; ++d_first) {
					const auto length = unserialise_length(&pos, end, true);
					*d_first = std::string(pos, length);
					pos += length;
				}
			} else {
				*d_first = std::string(pos, end - pos);
			}
		}
	}

	template <typename OutputIt>
	static void unserialise(std::string_view serialised, OutputIt d_first) {
		auto ptr = serialised.data();
		auto end = ptr + serialised.size();
		unserialise(&ptr, end, d_first);
	}
};


class CartesianList : public SerialiseList<CartesianList, Cartesian> {
	size_t get_length(const char**) const {
		return SERIALISED_LENGTH_CARTESIAN;
	}

	Cartesian get_value(const char* pos, size_t length) const {
		return Unserialise::cartesian(std::string(pos, length));
	}

	friend class SerialiseList<CartesianList, Cartesian>;

public:
	template <typename S, typename = std::enable_if_t<std::is_same<std::string, std::decay_t<S>>::value or std::is_same<std::string_view, std::decay_t<S>>::value>>
	CartesianList(S&& serialised)
		: SerialiseList<CartesianList, Cartesian>(std::forward<S>(serialised))
	{
		if (((_end - _ptr) % SERIALISED_LENGTH_CARTESIAN) != 0) {
			THROW(SerialisationError, "Bad encoded length: insufficient data");
		}
	}

	size_t size() const noexcept {
		return _serialised.length() / SERIALISED_LENGTH_CARTESIAN;
	}

	template <typename InputIt>
	static std::string serialise(InputIt first, InputIt last) {
		const auto size = std::distance(first, last);
		if (size == 1) {
			return Serialise::cartesian(*first);
		} else if (size > 1) {
			std::string serialised;
			serialised.reserve(SERIALISED_LENGTH_CARTESIAN * size + 1);
			serialised.push_back(SERIALISED_LIST_MAGIC);
			for ( ; first != last; ++first) {
				serialised.append(Serialise::cartesian(*first));
			}
			return serialised;
		}

		return std::string();
	}

	template <typename OutputIt>
	static void unserialise(const char** ptr, const char* end, OutputIt d_first) {
		const char* pos = *ptr;
		if (pos != end) {
			if (*pos == SERIALISED_LIST_MAGIC) {
				++pos;
				if ((end - pos) % SERIALISED_LENGTH_CARTESIAN == 0) {
					for ( ; pos != end; ++d_first) {
						*d_first = Unserialise::cartesian(std::string(pos, SERIALISED_LENGTH_CARTESIAN));
						pos += SERIALISED_LENGTH_CARTESIAN;
					}
				} else {
					THROW(SerialisationError, "Bad encoded length: insufficient data");
				}
			} else if ((end - pos) == SERIALISED_LENGTH_CARTESIAN) {
				*d_first = Unserialise::cartesian(std::string(pos, SERIALISED_LENGTH_CARTESIAN));
			} else {
				THROW(SerialisationError, "Bad encoded length: insufficient data");
			}
		}
	}

	template <typename OutputIt>
	static void unserialise(std::string_view serialised, OutputIt d_first) {
		auto ptr = serialised.data();
		auto end = ptr + serialised.size();
		unserialise(&ptr, end, d_first);
	}
};


class RangeList : public SerialiseList<RangeList, range_t> {
	size_t get_length(const char**) const {
		return SERIALISED_LENGTH_RANGE;
	}

	range_t get_value(const char* pos, size_t length) const {
		return Unserialise::range(std::string(pos, length));
	}

	friend class SerialiseList<RangeList, range_t>;

public:
	template <typename S, typename = std::enable_if_t<std::is_same<std::string, std::decay_t<S>>::value or std::is_same<std::string_view, std::decay_t<S>>::value>>
	RangeList(S&& serialised)
		: SerialiseList<RangeList, range_t>(std::forward<S>(serialised))
	{
		if (((_end - _ptr) % SERIALISED_LENGTH_RANGE) != 0) {
			THROW(SerialisationError, "Bad encoded length: insufficient data");
		}
	}

	size_t size() const noexcept {
		return _serialised.length() / SERIALISED_LENGTH_RANGE;
	}

	template <typename InputIt>
	static std::string serialise(InputIt first, InputIt last) {
		const auto size = std::distance(first, last);
		if (size == 1) {
			return Serialise::range(*first);
		} else if (size > 1) {
			std::string serialised;
			serialised.reserve(SERIALISED_LENGTH_RANGE * size + 1);
			serialised.push_back(SERIALISED_LIST_MAGIC);
			for ( ; first != last; ++first) {
				serialised.append(Serialise::range(*first));
			}
			return serialised;
		}

		return std::string();
	}

	template <typename OutputIt>
	static void unserialise(const char** ptr, const char* end, OutputIt d_first) {
		const char* pos = *ptr;
		if (pos != end) {
			if (*pos == SERIALISED_LIST_MAGIC) {
				++pos;
				if ((end - pos) % SERIALISED_LENGTH_RANGE == 0) {
					for ( ; pos != end; ++d_first) {
						*d_first = Unserialise::range(std::string(pos, SERIALISED_LENGTH_RANGE));
						pos += SERIALISED_LENGTH_RANGE;
					}
				} else {
					THROW(SerialisationError, "Bad encoded length: insufficient data");
				}
			} else if ((end - pos) == SERIALISED_LENGTH_RANGE) {
				*d_first = Unserialise::range(std::string(pos, SERIALISED_LENGTH_RANGE));
			} else {
				THROW(SerialisationError, "Bad encoded length: insufficient data");
			}
		}
	}

	template <typename OutputIt>
	static void unserialise(std::string_view serialised, OutputIt d_first) {
		auto ptr = serialised.data();
		auto end = ptr + serialised.size();
		unserialise(&ptr, end, d_first);
	}
};
