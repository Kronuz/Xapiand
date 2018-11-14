/*
 * Copyright (C) 2015-2018 Dubalu LLC. All rights reserved.
 * Copyright (C) 2014 furan
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

#include <string>


enum class TokenType : uint8_t {
	Not=1,
	Or,
	And,
	Maybe,
	Xor,
	LeftParenthesis,
	RightParenthesis,
	Id,
	EndOfFile
};


class Token {
	std::string lexeme;
	TokenType type;

public:
	Token() = default;

	void set_type(TokenType type_) {
		type = type_;
	}

	TokenType get_type() const noexcept {
		return type;
	}

	void set_lexeme(const std::string& lexeme_) {
		lexeme = lexeme_;
	}

	const std::string& get_lexeme() const noexcept {
		return lexeme;
	}
};
