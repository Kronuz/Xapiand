/*
 * Copyright (C) 2016 deipi.com LLC and contributors.
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

#include <list>
#include <memory>
#include <vector>

#include <xapian.h>

#include "ContentReader.h"
#include "Lexer.h"
#include "Node.h"


class BooleanTree {
public:
	BooleanTree(const std::string& input);
    ~BooleanTree() = default;

	std::unique_ptr<BaseNode> root;

	void Parse();
	void postorder(BaseNode* p, int indent=0);
	void PrintTree();
	Xapian::Query get_query();

	inline bool empty() {
		return stack_output.empty();
	}

	inline size_t size() {
		return stack_output.size();
	}

	inline Token& front() {
		return stack_output.front();
	}

	inline Token& back() {
		return stack_output.back();
	}

	inline void pop_front() {
		stack_output.pop_front();
	}

	inline void pop_back() {
		stack_output.pop_back();
	}


private:
	void toRPN();
	std::unique_ptr<BaseNode> BuildTree();
	unsigned precedence(TokenType type);

    std::list<Token> stack_output;
    std::vector<Token> stack_operator;

    std::unique_ptr<Lexer> lexer;
	std::unique_ptr<char[]> input;
    Token currentToken;
};
