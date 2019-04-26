/*
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

#include <utility>                                // for std::forward
#include <string>                                 // for std::string

#include "length.h"                               // for serialise_length, unserialise_length
#include "xapian/common/serialise-double.h"       // for serialise_double, unserialise_double


/*
 * Interface for implement soundex with diferent languages.
 */

template <typename Impl>
class Soundex {
protected:
	std::string _code_str;

public:
	Soundex() = default;

	template <typename T>
	Soundex(T&& code_str)
		: _code_str(std::forward<T>(code_str)) { }

	template <typename T>
	std::string encode(T&& str) const {
		return static_cast<const Impl*>(this)->_encode(str);
	}

	std::string encode() const {
		return _code_str;
	}

	virtual std::string serialise() const {
		std::string serialised;
		serialised += serialise_string(_code_str);
		return serialised;
	}
	virtual void unserialise(const char** p, const char* p_end) {
		_code_str = unserialise_string(p, p_end);
	}

	std::string_view name() const noexcept {
		return static_cast<const Impl*>(this)->_name();
	}

	std::string description() const noexcept {
		return static_cast<const Impl*>(this)->_description();
	}
};


/*
 * Auxiliary functions.
 */

template <typename Container>
inline void replace(std::string& str, size_t pos, const Container& patterns) {
	for (const auto& pattern : patterns) {
		auto _pos = str.find(pattern.first, pos);
		while (_pos != std::string::npos) {
			str.replace(_pos, pattern.first.length(), pattern.second);
			_pos = str.find(pattern.first, _pos + pattern.second.length());
		}
	}
}


template <typename Iterator>
inline void replace(std::string& str, size_t pos, Iterator begin, Iterator end) {
	while (begin != end) {
		auto _pos = str.find(begin->first, pos);
		while (_pos != std::string::npos) {
			str.replace(_pos, begin->first.length(), begin->second);
			_pos = str.find(begin->first, _pos + begin->second.length());
		}
		++begin;
	}
}


template <typename Container>
inline void replace_prefix(std::string& str, const Container& prefixes) {
	for (const auto& prefix : prefixes) {
		if (std::mismatch(prefix.first.begin(), prefix.first.end(), str.begin()).first == prefix.first.end()) {
			str.replace(0, prefix.first.length(), prefix.second);
			return;
		}
	}
}
