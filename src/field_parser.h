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


class FieldParserError : public ClientError {
public:
	template<typename... Args>
	FieldParserError(Args&&... args) : ClientError(std::forward<Args>(args)...) { }
};


#define MSG_FieldParserError(...) FieldParserError(__FILE__, __LINE__, __VA_ARGS__)


class FieldParser {
	std::string fstr;
	size_t len_field;
	const char* off_field;
	size_t len_fieldot;
	const char* off_fieldot;
	size_t len_value;
	const char* off_value;
	size_t len_double_quote_value;
	const char* off_double_quote_value;
	size_t len_single_quote_value;
	const char* off_single_quote_value;

	bool skip_quote;

public:
	enum class State : uint8_t {
		INIT,
		FIELD,
		QUOTE,
		ESCAPE,
		STARTVALUE,
		VALUE,
		INIT_SQUARE_BRACKET,
		END_SQUARE_BRACKET,
		SQUARE_BRACKET,
		COMMA_OR_END,
		DOUBLE_DOTS_OR_END,
		FIRST_QUOTE_SQUARE_BRACKET,
		SECOND_QUOTE_SQUARE_BRACKET,
		END
	};

	bool isrange;
	std::string start;
	std::string end;

	FieldParser(const std::string& p);

	void parse();

	inline std::string get_field() const {
		if (off_field) {
			return std::string(off_field, len_field);
		}
		return std::string();
	}

	inline std::string get_field_dot() const {
		if (skip_quote) {
			return std::string(off_field, len_field) + ":";
		} else if (off_fieldot) {
			return std::string(off_fieldot, len_fieldot);
		}
		return std::string();
	}

	inline std::string get_value() const {
		if (off_value) {
			return std::string(off_value, len_value);
		}
		return std::string();
	}

	inline std::string get_doubleq_value() const {
		if (off_double_quote_value) {
			return std::string(off_double_quote_value, len_double_quote_value);
		}
		return std::string();
	}

	inline std::string get_singleq_value() const {
		if (off_single_quote_value) {
			return std::string(off_single_quote_value, len_single_quote_value);
		}
		return std::string();
	}

	inline bool is_double_quote_value() const noexcept {
		return off_double_quote_value != 0;
	}
};
