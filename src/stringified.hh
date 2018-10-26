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

#ifndef STRINGIFIED_HH
#define STRINGIFIED_HH

#include <string>                // for std::string
#include "string_view.hh"        // for std::string_view
#include <ostream>               // for std::ostream

/*
 * The stringified class is a wrapper for std::string_view which makes
 * it so it ensures c_str() always returns a null-terminated c string.
 * This class only copies the data to ensure this when is needed.
 */
class stringified {
    mutable std::string _str;
    mutable std::string_view _str_view;

public:
    stringified(const stringified& v)
        : _str_view(v._str_view)
    { }

    stringified(stringified&& v) noexcept
        : _str(std::move(v._str)),
          _str_view(std::move(v._str_view))
    { }

    stringified& operator=(const stringified& o) {
        _str_view = o._str_view;
        return *this;
    }

    stringified& operator=(stringified&& o) noexcept {
        _str = std::move(o._str);
        _str_view = std::move(o._str_view);
        return *this;
    }

    explicit stringified(const std::string& str)
        : _str_view(str)
    { }

    explicit stringified(std::string&& str)
        : _str(str),
          _str_view(_str)
    { }

    stringified(const std::string_view& str_view)
        : _str_view(str_view)
    { }

    stringified(std::string_view&& str_view)
        : _str_view(std::move(str_view))
    { }

    auto empty() const noexcept {
        return _str_view.empty();
    }

    auto length() const noexcept {
        return _str_view.size();
    }

    auto c_str() const {
        auto str_data = _str.data();
        auto str_view_data = _str_view.data();
        if (str_data != str_view_data && *(str_view_data + _str_view.size()) != '\0') {
            _str.assign(_str_view.data(), _str_view.size());
            _str_view = _str;
            str_view_data = str_data;
        }
        return str_view_data;
    }

    auto size() const noexcept {
        return _str_view.size();
    }

    auto data() const {
        return _str_view.data();
    }

    auto operator[](std::size_t pos) const noexcept {
        return _str_view[pos];
    }

    auto at(std::size_t pos) const {
        return _str_view.at(pos);
    }

    auto front() const noexcept {
        return _str_view.front();
    }

    auto back() const noexcept {
        return _str_view.back();
    }

    auto begin() const noexcept {
        return _str_view.begin();
    }

    auto end() const noexcept {
        return _str_view.end();
    }

    auto cbegin() const noexcept {
        return _str_view.cbegin();
    }

    auto cend() const noexcept {
        return _str_view.cend();
    }

    operator std::string_view() const noexcept {
        return _str_view;
    }

	friend std::ostream& operator<<(std::ostream& os, const stringified& obj) {
		return os << obj._str_view;
	}
};

#endif // STRINGIFIED_HH
