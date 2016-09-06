/*
 * Copyright (C) 2014 furan
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

#include <iostream>
#include <unordered_map>
#include <string>

#include "ContentReader.h"
#include "Token.h"
#include "LexicalException.h"

using namespace std;

enum class LexerState {
	INIT,
	TOKEN,
	TOKEN_DOUBLEQ,
	TOKEN_SINGLEQ,
	ESCAPE,
	SYMBOL_OP,
	EOFILE
};


class Lexer{
private:
	Symbol currentSymbol;
	ContentReader contentReader;
	unordered_map<string, TokenType> singleSymbolDictionary;
	void InitDictionary();
	void IsStringOperator(Token& token);
	bool IsSymbolOp(char c);

public:
	Lexer(ContentReader cr);
	Lexer(char * input);
	Token NextToken();
};
