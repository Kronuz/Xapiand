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

#include <cassert>                                // for assert
#include <string>                                 // for std::string
#include <string_view>                            // for std::string_view


template <typename S = std::string, typename T = char>
class Split {
	using size_type = typename S::size_type;
	using dispatch_search = size_type (Split::*)(size_type) const;

	S str;
	T sep;
	bool skip_blank;
	size_t inc;

	dispatch_search search_func;

	size_type find(size_type pos=0) const noexcept {
		return str.find(sep, pos);
	}

	size_type find_first_of(size_type pos=0) const noexcept {
		return str.find_first_of(sep, pos);
	}

	size_type next(size_type pos=0) const noexcept {
		return (this->*search_func)(pos);
	}

	template <typename V>
	class Iterator : public std::iterator<std::input_iterator_tag, V> {
		friend class Split;

		const Split* split;
		size_type start;
		size_type end;
		size_t inc;

		size_type next_start;
		size_type next_end;

		Iterator(const Split* split_, size_type pos_ = 0)
			: split(split_),
			  start(pos_),
			  end(pos_),
			  inc(0)
		{
			if (end != S::npos) {
				do {
					start = end + inc;
					if (start == split->str.size()) {
						start = S::npos;
						end = S::npos;
						return;
					}
					end = split->next(start);
					inc = split->inc;
				} while (split->skip_blank && start == end && end != S::npos);
			}
			next();
		}

		void next() {
			next_start = start;
			next_end = end;
			if (next_end == S::npos) {
				next_start = S::npos;
			} else {
				do {
					next_start = next_end + inc;
					if (next_start == split->str.size()) {
						next_start = S::npos;
						next_end = S::npos;
						break;
					}
					next_end = split->next(next_start);
				} while (split->skip_blank && next_start == next_end && next_end != S::npos);
			}
		}

	public:
		Iterator& operator++() {
			start = next_start;
			end = next_end;
			next();
			return *this;
		}

		Iterator operator++(int) {
			auto it = *this;
			start = next_start;
			end = next_end;
			next();
			return it;
		}

		V operator*() const {
			assert(start != S::npos);
			if (end == S::npos) {
				return V(split->str).substr(start);
			}
			return V(split->str).substr(start, end - start);
		}

		V operator*() {
			assert(start != S::npos);
			if (end == S::npos) {
				return V(split->str).substr(start);
			}
			return V(split->str).substr(start, end - start);
		}

		V operator->() const {
			return &operator*();
		}

		V operator->() {
			return &operator*();
		}

		bool operator==(const Iterator& other) const {
			return split == other.split && start == other.start && end == other.end;
		}

		bool operator!=(const Iterator& other) const {
			return !operator==(other);
		}

		explicit operator bool() const noexcept {
			return start != S::npos;
		}

		bool last() const noexcept {
			return next_end == S::npos;
		}
	};

public:
	enum class Type : uint8_t {
		FIND,
		FIND_FIRST_OF,
		SKIP_BLANK_FIND,
		SKIP_BLANK_FIND_FIRST_OF,
	};

	Split() = default;

	template <typename String, typename = std::enable_if_t<std::is_same<S, std::decay_t<String>>::value && std::is_same<T, std::decay_t<String>>::value>>
	Split(String&& str_, String&& sep_, Type type=Type::FIND)
		: str(std::forward<String>(str_)),
		  sep(std::forward<String>(sep_)),
		  skip_blank(true)
	{
		switch (type) {
			case Type::SKIP_BLANK_FIND:
				skip_blank = false;
			case Type::FIND:
				inc = sep.size();
				search_func = &Split::find;
				break;
			case Type::SKIP_BLANK_FIND_FIRST_OF:
				skip_blank = false;
			case Type::FIND_FIRST_OF:
				inc = 1;
				search_func = &Split::find_first_of;
				break;
			default:
				inc = sep.size();
				search_func = &Split::find;
				break;
		}
	}

	template <typename String, typename Sep, typename = std::enable_if_t<std::is_same<char, std::decay_t<Sep>>::value && std::is_same<T, std::decay_t<Sep>>::value>>
	Split(String&& str_, Sep&& sep_, Type type=Type::FIND)
		: str(std::forward<String>(str_)),
		  sep(std::forward<Sep>(sep_)),
		  skip_blank(true),
		  inc(1)
	{
		switch (type) {
			case Type::SKIP_BLANK_FIND:
				skip_blank = false;
				[[fallthrough]];
			case Type::FIND:
				search_func = &Split::find;
				break;
			case Type::SKIP_BLANK_FIND_FIRST_OF:
				skip_blank = false;
				[[fallthrough]];
			case Type::FIND_FIRST_OF:
				search_func = &Split::find_first_of;
				break;
			default:
				search_func = &Split::find;
				break;
		}
	}

	using iterator = Iterator<std::string_view>;
	using const_iterator = Iterator<const std::string_view>;

	iterator begin() {
		return iterator(this);
	}

	const_iterator begin() const {
		return const_iterator(this);
	}

	const_iterator cbegin() const {
		return const_iterator(this);
	}

	iterator end() {
		return iterator(this, S::npos);
	}

	const_iterator end() const {
		return const_iterator(this, S::npos);
	}

	const_iterator cend() const {
		return const_iterator(this, S::npos);
	}

	bool empty() const noexcept {
		return str.empty();
	}

	size_t size() const noexcept {
		return std::distance(begin(), end());
	}

	const auto& get_str() const noexcept {
		return str;
	}

	template <typename OutputIt>
	static void split(const S& str, const S& delimiter, OutputIt d_first, bool skip_blank=true) {
		size_t prev = 0, next = 0;

		while ((next = str.find(delimiter, prev)) != S::npos) {
			size_t len = next - prev;
			if (!skip_blank || len > 0) {
				*d_first = str.substr(prev, len);
				++d_first;
			}
			prev = next + delimiter.size();
		}

		if (prev < str.size()) {
			*d_first = str.substr(prev);
		}
	}

	template <typename OutputIt>
	static void split(const S& str, char delimiter, OutputIt d_first, bool skip_blank=true) {
		size_t prev = 0, next = 0;

		while ((next = str.find(delimiter, prev)) != S::npos) {
			size_t len = next - prev;
			if (!skip_blank || len > 0) {
				*d_first = str.substr(prev, len);
				++d_first;
			}
			prev = next + 1;
		}

		if (prev < str.size()) {
			*d_first = str.substr(prev);
		}
	}

	template <typename OutputIt>
	static void split_first_of(const S& str, const S& delimiter, OutputIt d_first, bool skip_blank=true) {
		size_t prev = 0, next = 0;

		while ((next = str.find_first_of(delimiter, prev)) != S::npos) {
			size_t len = next - prev;
			if (!skip_blank || len > 0) {
				*d_first = str.substr(prev, len);
				++d_first;
			}
			prev = next + 1;
		}

		if (prev < str.size()) {
			*d_first = str.substr(prev);
		}
	}

	template <typename OutputIt>
	static void split_first_of(const S& str, char delimiter, OutputIt d_first, bool skip_blank=true) {
		return split(str, delimiter, d_first, skip_blank);
	}
};
