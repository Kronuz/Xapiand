/*
 * Copyright (C) 2016 deipi.com LLC and contributors. All rights reserved.
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

#include "xapiand.h"

#include <set>
#include <unordered_set>

#include "geo/cartesian.h"
#include "geo/htm.h"
#include "length.h"
#include "serialise.h"


#define STL_MAGIC '\0'

constexpr long SIZE_RANGE = 2 * SIZE_BYTES_ID;


/*
 * Class for serialise a STL of strings.
 * i.e
 * STL = {a, ..., b}
 * serialised = STL_MAGIC + serialise_length(size STL) + serialise_length(a.length()) + a + ... + serialise_length(b.length()) + b
 * symbol '+' means concatenate
 */
template <typename Impl>
class STLString {
protected:
	inline void _reserve(size_t count) {
		return static_cast<Impl*>(this)->_reserve(count);
	}

	inline void _clear() noexcept {
		return static_cast<Impl*>(this)->clear();
	}

	inline void _add(const char* pos, size_t length) {
		return static_cast<Impl*>(this)->_add(pos, length);
	}

	inline const std::string& _front() const {
		return *static_cast<const Impl*>(this)->cbegin();
	}

	inline size_t _size() const noexcept {
		return static_cast<const Impl*>(this)->size();
	}

public:
	void unserialise(const char** ptr, const char* end) {
		const char* pos = *ptr;
		if (pos != end && *pos++ == STL_MAGIC) {
			try {
				_clear();
				auto length = unserialise_length(&pos, end, true);
				_reserve(length);
				while (pos != end) {
					length = unserialise_length(&pos, end, true);
					_add(pos, length);
					pos += length;
				}
				return;
			} catch (const Xapian::SerialisationError&) { }
		}

		_clear();
		pos = *ptr;
		if (pos != end) {
			_add(pos, end - pos);
		}
	}

	inline void unserialise(const std::string& serialised) {
		const char* ptr = serialised.data();
		const char* end = serialised.data() + serialised.length();
		unserialise(&ptr, end);
	}

	void add_unserialise(const char** ptr, const char* end) {
		const char* pos = *ptr;
		if (pos != end && *pos++ == STL_MAGIC) {
			try {
				auto length = unserialise_length(&pos, end, true);
				_reserve(length);
				while (pos != end) {
					length = unserialise_length(&pos, end, true);
					_add(pos, length);
					pos += length;
				}
				return;
			} catch (const Xapian::SerialisationError&) { }
		}

		pos = *ptr;
		if (pos != end) {
			_add(pos, end - pos);
		}
	}

	inline void add_unserialise(const std::string& serialised) {
		const char* ptr = serialised.data();
		const char* end = ptr + serialised.length();
		add_unserialise(&ptr, end);
	}


	std::string serialise() const {
		std::string serialised;

		auto size = _size();
		if (size > 1) {
			serialised.assign(1, STL_MAGIC);
			serialised.append(serialise_length(size));
			for (const auto& str : *static_cast<const Impl*>(this)) {
				serialised.append(serialise_length(str.length()));
				serialised.append(str);
			}
		} else if (size == 1) {
			serialised.assign(_front());
		}

		return serialised;
	}
};


class StringList : public std::vector<std::string>, public STLString<StringList> {
	inline void _reserve(size_t count) {
		reserve(count);
	}

	inline void _add(const char* pos, size_t length) {
		emplace_back(pos, length);
	}

	friend class STLString<StringList>;
};


class StringSet : public std::set<std::string>, public STLString<StringSet> {
	inline void _reserve(size_t) { }

	inline void _add(const char* pos, size_t length) {
		insert(std::string(pos, length));
	}

	friend class STLString<StringSet>;
};


class StringUSet : public std::unordered_set<std::string>, public STLString<StringUSet> {
	inline void _reserve(size_t) { }

	inline void _add(const char* pos, size_t length) {
		insert(std::string(pos, length));
	}

	friend class STLString<StringUSet>;
};


/*
 * This class serializes a STL of Cartesian.
 * i.e
 * STL = {a, ..., b}
 * serialised = STL_MAGIC + serialise_length(size STL) + serialise_cartesian(a) + ... + serialise_cartesian(b)
 * symbol '+' means concatenate.
 * It is not necessary to save the size because it's SIZE_SERIALISE_CARTESIAN for all.
 */
template <typename Impl>
class STLCartesian {
protected:
	inline void _reserve(size_t count) {
		return static_cast<Impl*>(this)->_reserve(count);
	}

	inline void _clear() noexcept {
		return static_cast<Impl*>(this)->clear();
	}

	inline void _add(Cartesian&& c) {
		return static_cast<Impl*>(this)->_add(std::move(c));
	}

	inline const Cartesian& _front() const {
		return *static_cast<const Impl*>(this)->cbegin();
	}

	inline size_t _size() const noexcept {
		return static_cast<const Impl*>(this)->size();
	}

public:
	void unserialise(const char** ptr, const char* end) {
		_clear();
		const char* pos = *ptr;
		if (pos != end && *pos++ == STL_MAGIC) {
			try {
				auto size = unserialise_length(&pos, end, true);
				_reserve(size);
				while (end - pos >= SIZE_SERIALISE_CARTESIAN) {
					_add(Unserialise::cartesian(std::string(pos, SIZE_SERIALISE_CARTESIAN)));
					pos += SIZE_SERIALISE_CARTESIAN;
				}
				if (pos != end || _size() != size) {
					_clear();
				}
			} catch (const Xapian::SerialisationError&) {
				_clear();
			}
		}
	}

	inline void unserialise(const std::string& serialised) {
		const char* ptr = serialised.data();
		const char* end = serialised.data() + serialised.length();
		unserialise(&ptr, end);
	}

	void add_unserialise(const char** ptr, const char* end) {
		const char* pos = *ptr;
		if (pos != end && *pos++ == STL_MAGIC) {
			try {
				auto size = unserialise_length(&pos, end, true);
				_reserve(size);
				while (end - pos >= SIZE_SERIALISE_CARTESIAN) {
					_add(Unserialise::cartesian(std::string(pos, SIZE_SERIALISE_CARTESIAN)));
					pos += SIZE_SERIALISE_CARTESIAN;
				}
			} catch (const Xapian::SerialisationError&) { }
		}
	}

	inline void add_unserialise(const std::string& serialised) {
		const char* ptr = serialised.data();
		const char* end = ptr + serialised.length();
		add_unserialise(&ptr, end);
	}


	std::string serialise() const {
		std::string serialised;

		auto size = _size();
		if (size) {
			std::string header(1, STL_MAGIC);
			header.append(serialise_length(size));

			serialised.reserve(SIZE_SERIALISE_CARTESIAN * size + header.length());
			serialised.assign(std::move(header));

			for (const auto& c : *static_cast<const Impl*>(this)) {
				serialised.append(Serialise::cartesian(c));
			}
		}

		return serialised;
	}
};


class CartesianList : public std::vector<Cartesian>, public STLCartesian<CartesianList> {
	inline void _reserve(size_t count) {
		reserve(count);
	}

	inline void _add(Cartesian&& c) {
		push_back(std::move(c));
	}

	friend class STLCartesian<CartesianList>;
};


class CartesianSet : public std::set<Cartesian>, public STLCartesian<CartesianSet> {
	inline void _reserve(size_t) { }

	inline void _add(Cartesian&& c) {
		insert(std::move(c));
	}

	friend class STLCartesian<CartesianSet>;
};


class CartesianUSet : public std::unordered_set<Cartesian>, public STLCartesian<CartesianUSet> {
	inline void _reserve(size_t) { }

	inline void _add(Cartesian&& c) {
		insert(std::move(c));
	}

	friend class STLCartesian<CartesianUSet>;
};


/*
 * This class serializes a STL of range_t.
 * i.e
 * STL = {{a,b}, ..., {c,d}}
 * serialised = STL_MAGIC + serialise_length(size STL) + Serialise::trixel_id(a) + Serialise::trixel_id(b) ... + Serialise::trixel_id(d)
 * symbol '+' means concatenate.
 * It is not necessary to save the size because it's SIZE_BYTES_ID for all.
 */
template <typename Impl>
class STLRange {
protected:
	inline void _reserve(size_t count) {
		return static_cast<Impl*>(this)->_reserve(count);
	}

	inline void _clear() noexcept {
		return static_cast<Impl*>(this)->clear();
	}

	inline void _add(uint64_t start, uint64_t end) {
		return static_cast<Impl*>(this)->_add(start, end);
	}

	inline const range_t& _front() const {
		return *static_cast<const Impl*>(this)->cbegin();
	}

	inline size_t _size() const noexcept {
		return static_cast<const Impl*>(this)->size();
	}

public:
	void unserialise(const char** ptr, const char* end) {
		_clear();
		const char* pos = *ptr;
		if (pos != end && *pos++ == STL_MAGIC) {
			try {
				auto size = unserialise_length(&pos, end, true);
				_reserve(size);
				while (end - pos >= SIZE_RANGE) {
					_add(Unserialise::trixel_id(std::string(pos, SIZE_BYTES_ID)), Unserialise::trixel_id(std::string(pos + SIZE_BYTES_ID, SIZE_BYTES_ID)));
					pos += SIZE_RANGE;
				}
				if (pos != end || _size() != size) {
					_clear();
				}
			} catch (const Xapian::SerialisationError&) {
				_clear();
			}
		}
	}

	inline void unserialise(const std::string& serialised) {
		const char* ptr = serialised.data();
		const char* end = serialised.data() + serialised.length();
		unserialise(&ptr, end);
	}

	void add_unserialise(const char** ptr, const char* end) {
		const char* pos = *ptr;
		if (pos != end && *pos++ == STL_MAGIC) {
			try {
				auto size = unserialise_length(&pos, end, true);
				_reserve(size);
				while (end - pos >= SIZE_RANGE) {
					_add(Unserialise::trixel_id(std::string(pos, SIZE_BYTES_ID)), Unserialise::trixel_id(std::string(pos + SIZE_BYTES_ID, SIZE_BYTES_ID)));
					pos += SIZE_RANGE;
				}
			} catch (const Xapian::SerialisationError&) { }
		}
	}

	inline void add_unserialise(const std::string& serialised) {
		const char* ptr = serialised.data();
		const char* end = ptr + serialised.length();
		add_unserialise(&ptr, end);
	}


	std::string serialise() const {
		std::string serialised;

		auto size = _size();
		if (size) {
			std::string header(1, STL_MAGIC);
			header.append(serialise_length(size));

			serialised.reserve(SIZE_RANGE * size + header.length());
			serialised.assign(std::move(header));

			for (const auto& range : *static_cast<const Impl*>(this)) {
				serialised.append(Serialise::trixel_id(range.start));
				serialised.append(Serialise::trixel_id(range.end));
			}
		}

		return serialised;
	}
};


class RangeList : public std::vector<range_t>, public STLRange<RangeList> {
	inline void _reserve(size_t count) {
		reserve(count);
	}

	inline void _add(uint64_t start, uint64_t end) {
		push_back({ start, end });
	}

	friend class STLRange<RangeList>;
};


class RangeSet : public std::set<range_t>, public STLRange<RangeSet> {
	inline void _reserve(size_t) { }

	inline void _add(uint64_t start, uint64_t end) {
		insert({ start, end });
	}

	friend class STLRange<RangeSet>;
};


class RangeUSet : public std::unordered_set<range_t>, public STLRange<RangeUSet> {
	inline void _reserve(size_t) { }

	inline void _add(uint64_t start, uint64_t end) {
		insert({ start, end });
	}

	friend class STLRange<RangeUSet>;
};
