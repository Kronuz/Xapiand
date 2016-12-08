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

#include <cstdio>       // for size_t
#include <string>       // for string, allocator, operator+, basic_string
#include <type_traits>  // for forward

#include "exception.h"  // for ClientError

#define LVL_MAX 10

class FieldParserError : public ClientError {
public:
	template<typename... Args>
	FieldParserError(Args&&... args) : ClientError(std::forward<Args>(args)...) { }
};


class FieldParser {
	std::string fstr;
	size_t len_field;
	const char* off_field;
	size_t len_field_colon;
	const char* off_field_colon;
	const char* off_values;
	size_t lvl;
	size_t lens[LVL_MAX];
	const char* offs[LVL_MAX];
	size_t lens_single_quote[LVL_MAX];
	const char* offs_single_quote[LVL_MAX];
	size_t lens_double_quote[LVL_MAX];
	const char* offs_double_quote[LVL_MAX];

	bool skip_quote;

public:
	enum class State : uint8_t {
		INIT,
		FIELD,
		QUOTE,
		VALUE_INIT,
		VALUE,
		COLON,
		SQUARE_BRACKET,
		SQUARE_BRACKET_INIT,
		SQUARE_BRACKET_END,
		SQUARE_BRACKET_COMMA,
		SQUARE_BRACKET_QUOTE,
		DOT_DOT_INIT,
		DOT_DOT,
		END
	};

	enum class Range : uint8_t {
		none,
		open,          // i.e. ()
		closed_right,  // i.e. (]
		closed_left,   // i.e. [)
		closed,        // i.e. []
	};

	Range range;

	FieldParser(const std::string& p);

	void parse(size_t lvl_max=2);

	std::string get_field_name() const {
		if (off_field) {
			return std::string(off_field, len_field);
		}
		return std::string();
	}

	std::string get_field_name_colon() const {
		if (skip_quote) {
			return std::string(off_field, len_field) + ":";
		} else if (off_field_colon) {
			return std::string(off_field_colon, len_field_colon);
		}
		return std::string();
	}

	std::string get_values() const {
		return std::string(off_values, fstr.size() - (off_values - fstr.data()));
	}

	std::string get_value(size_t l=0) const {
		if (l <= lvl && offs[l]) {
			return std::string(offs[l], lens[l]);
		}
		return std::string();
	}

	bool is_double_quoted_value(size_t l=0) const noexcept {
		return l <= lvl && offs_double_quote[l] != 0;
	}

	bool is_single_quoted_value(size_t l=0) const noexcept {
		return l <= lvl && offs_double_quote[l] != 0;
	}

	std::string get_double_quoted_value(size_t l=0) const {
		if (l <= lvl && offs_double_quote[l]) {
			return std::string(offs_double_quote[l], lens_double_quote[l]);
		}
		return std::string();
	}

	std::string get_single_quoted_value(size_t l=0) const {
		if (l <= lvl && offs_single_quote[l]) {
			return std::string(offs_single_quote[l], lens_single_quote[l]);
		}
		return std::string();
	}

	bool is_range() const {
		return range != Range::none;
	}

	std::string get_start() const {
		if (is_range()) {
			return get_value(0);
		}
		return std::string();
	}

	std::string get_end() const {
		if (is_range()) {
			return get_value(1);
		}
		return std::string();
	}
};
