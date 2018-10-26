/*
 * Copyright (C) 2015-2018 Dubalu LLC. All rights reserved.
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

#include <string>                // for std::string
#include <utility>               // for std::forward

#include "string.hh"             // for string::upper


/*
 * Interface for make string metrics.
 */
template <typename Impl>
class StringMetric {
protected:
	bool _icase;
	std::string _str;

public:
	StringMetric(bool icase)
		: _icase(icase) { }

	template <typename T>
	StringMetric(T&& str, bool icase)
		: _icase(icase),
		  _str(_icase ? string::upper(std::forward<T>(str)) : std::forward<T>(str)) { }

	template <typename T>
	double distance(T&& str1, T&& str2) const {
		// Check base cases.
		if (str1.empty() || str2.empty()) {
			return 1.0;
		}

		if (str1 == str2) {
			return 0.0;
		}

		return static_cast<const Impl*>(this)->_distance(
			_icase ? string::upper(std::forward<T>(str1)) : std::forward<T>(str1),
			_icase ? string::upper(std::forward<T>(str2)) : std::forward<T>(str2)
		);
	}

	template <typename T>
	double distance(T&& str2) const {
		// Check base cases.
		if (_str.empty() || str2.empty()) {
			return 1.0;
		}

		if (_str == str2) {
			return 0.0;
		}

		return static_cast<const Impl*>(this)->_distance(
			_icase ? string::upper(std::forward<T>(str2)) : std::forward<T>(str2)
		);
	}

	template <typename T>
	double similarity(T&& str1, T&& str2) const {
		// Check base cases.
		if (str1.empty() || str2.empty()) {
			return 0.0;
		}

		if (str1 == str2) {
			return 1.0;
		}

		return static_cast<const Impl*>(this)->_similarity(
			_icase ? string::upper(std::forward<T>(str1)) : std::forward<T>(str1),
			_icase ? string::upper(std::forward<T>(str2)) : std::forward<T>(str2)
		);
	}

	template <typename T>
	double similarity(T&& str2) const {
		// Check base cases.
		if (_str.empty() || str2.empty()) {
			return 0.0;
		}

		if (_str == str2) {
			return 1.0;
		}

		return static_cast<const Impl*>(this)->_similarity(
			_icase ? string::upper(std::forward<T>(str2)) : std::forward<T>(str2)
		);
	}

	std::string description() const noexcept {
		auto desc = static_cast<const Impl*>(this)->_description();
		desc.append(_icase ? " ignore case" : " case sensitive");
		return desc;
	}
};


/*
 * Struct used for counting the times that push_back is called.
 *
 * Designed for using with std::set_intersection.
 */
struct Counter {
	size_t count{0};

	struct value_type {
		template<typename T>
		value_type(const T&) { }
	};

	void push_back(const value_type&) {
		++count;
	}
};
