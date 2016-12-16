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

#include <string>
#include <vector>


class Split {
	using dispatch_search = std::string::size_type (Split::*)(std::string::size_type) const;

	std::string sep;
	std::string str;
	size_t inc;

	dispatch_search search_func;

	std::string::size_type find(std::string::size_type pos=0) const noexcept {
		return str.find(sep, pos);
	}

	std::string::size_type find_first_of(std::string::size_type pos=0) const noexcept {
		return str.find_first_of(sep, pos);
	}

	std::string::size_type next(std::string::size_type pos=0) const noexcept {
		return (this->*search_func)(pos);
	}

public:
	class iterator : public std::iterator<std::input_iterator_tag, std::string> {
		friend class Split;

		const Split* split;
		std::string::size_type start;
		std::string::size_type end;
		std::string str;
		size_t inc;

		iterator(const Split* split_, std::string::size_type pos_=0)
			: split(split_),
			  start(pos_),
			  end(pos_),
			  inc(0)
		{
			if (end != std::string::npos) {
				do {
					start = end + inc;
					if (start == split->str.length()) {
						start = std::string::npos;
						end = std::string::npos;
						return;
					}
					end = split->next(start);
					inc = split->inc;
				} while (start == end && end != std::string::npos);
			}
		}

		void next() {
			if (end == std::string::npos) {
				start = std::string::npos;
			} else {
				do {
					start = end + inc;
					if (start == split->str.length()) {
						start = std::string::npos;
						end = std::string::npos;
						return;
					}
					end = split->next(start);
				} while (start == end && end != std::string::npos);
			}
		}

	public:
		iterator& operator++() {
			next();
			return *this;
		}

		iterator operator++(int) {
			iterator it = *this;
			next();
			return it;
		}

		std::string& operator*() {
			if (end == std::string::npos) {
				str.assign(split->str.substr(start));
			} else {
				str.assign(split->str.substr(start, end - start));
			}
			return str;
		}

		bool operator==(const iterator& other) const {
			return split == other.split && start == other.start && end == other.end;
		}

		bool operator!=(const iterator& other) const {
			return !operator==(other);
		}
	};

	enum class Type : uint8_t {
		FIND,
		FIND_FIRST_OF,
	};

	Split(const std::string& str_, const std::string& sep_, Type type=Type::FIND)
		: sep(sep_),
		  str(str_)
	{
		switch (type) {
			case Type::FIND:
				inc = sep.length();
				search_func = &Split::find;
				break;
			case Type::FIND_FIRST_OF:
				inc = 1;
				search_func = &Split::find_first_of;
				break;
			default:
				inc = sep.length();
				search_func = &Split::find;
				break;
		}
	}

	iterator begin() const {
		return iterator(this);
	}

	iterator end() const {
		return iterator(this, std::string::npos);
	}

	static std::vector<std::string> split(const std::string& str, const std::string& delimiter) {
		std::vector<std::string> tokens;
		size_t prev = 0, next = 0, len;

		while ((next = str.find(delimiter, prev)) != std::string::npos) {
			len = next - prev;
			if (len > 0) {
				tokens.push_back(str.substr(prev, len));
			}
			prev = next + delimiter.length();
		}

		if (prev < str.length()) {
			tokens.push_back(str.substr(prev));
		}

		return tokens;
	}

	static std::vector<std::string> split_first_of(const std::string& str, const std::string& delimiter) {
		std::vector<std::string> tokens;
		size_t prev = 0, next = 0, len;

		while ((next = str.find_first_of(delimiter, prev)) != std::string::npos) {
			len = next - prev;
			if (len > 0) {
				tokens.push_back(str.substr(prev, len));
			}
			prev = next + 1;
		}

		if (prev < str.length()) {
			tokens.push_back(str.substr(prev));
		}

		return tokens;
	}
};
