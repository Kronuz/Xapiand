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

#include "Lexer.h"

#define AND "AND"
#define OR "OR"
#define NOT "NOT"
#define XOR "XOR"

#define DOUBLEQUOTE '"'
#define SINGLEQUOTE '\''
#define LEFT_SQUARE_BRACKET '['
#define RIGHT_SQUARE_BRACKET ']'
#define COMMA ','


Lexer::Lexer(ContentReader contentReader)
{
	this->contentReader = contentReader;
	currentSymbol = this->contentReader.NextSymbol();

	InitDictionary();
}


Lexer::Lexer(char * input)
{
	ContentReader cr(input);
	contentReader = cr;
	currentSymbol = this->contentReader.NextSymbol();

	InitDictionary();

}


void
Lexer::InitDictionary()
{
	singleSymbolDictionary["("] = TokenType::LeftParenthesis;
	singleSymbolDictionary[")"] = TokenType::RightParenthesis;
	singleSymbolDictionary["&"] = TokenType::And;
	singleSymbolDictionary["|"] = TokenType::Or;
	singleSymbolDictionary["~"] = TokenType::Not;
}


Token
Lexer::NextToken()
{
	string lexeme = "";
	LexerState currentState = LexerState::INIT;
	Token token;
	char quote;

	auto upState = currentState;

	string symbol;
	string lcSymbol;

	while (true) {
		symbol.clear();
		symbol += currentSymbol.symbol;
		switch (currentState) {
			case LexerState::INIT:
				if (currentSymbol.symbol == LEFT_SQUARE_BRACKET)
				{
					lexeme += currentSymbol.symbol;
					currentState = LexerState::INIT_SQUARE_BRACKET;
					currentSymbol = contentReader.NextSymbol();
				}
				else if (currentSymbol.symbol == SINGLEQUOTE)
				{
					lexeme += currentSymbol.symbol;
					currentState = LexerState::TOKEN_QUOTE;
					quote = SINGLEQUOTE;
					currentSymbol = contentReader.NextSymbol();
				}
				else if (currentSymbol.symbol == DOUBLEQUOTE)
				{
					lexeme += currentSymbol.symbol;
					currentState = LexerState::TOKEN_QUOTE;
					quote = DOUBLEQUOTE;
					currentSymbol = contentReader.NextSymbol();
				}
				else if(isalpha(currentSymbol.symbol))
				{
					lexeme += currentSymbol.symbol;
					currentState = LexerState::TOKEN;
					currentSymbol = contentReader.NextSymbol();
				}
				else if (currentSymbol.symbol == '\0')
				{
					currentState = LexerState::EOFILE;
				}
				else if (isspace(currentSymbol.symbol))
				{
					currentState = LexerState::INIT;
					currentSymbol = contentReader.NextSymbol();
				}
				else if(singleSymbolDictionary.find(symbol) != singleSymbolDictionary.end())
				{
					lexeme += currentSymbol.symbol;
					currentState = LexerState::SYMBOL_OP;
					currentSymbol = contentReader.NextSymbol();
				}
				else
				{
					string msj = "Symbol " + symbol + " is not recognized";
					throw LexicalException(msj.c_str());
				}
				break;
			case LexerState::TOKEN:
				if (currentSymbol.symbol == DOUBLEQUOTE)
				{
					lexeme += currentSymbol.symbol;
					currentState = LexerState::TOKEN_QUOTE;
					quote = DOUBLEQUOTE;
					currentSymbol = contentReader.NextSymbol();
				}
				else if (currentSymbol.symbol == SINGLEQUOTE)
				{
					lexeme += currentSymbol.symbol;
					currentState = LexerState::TOKEN_QUOTE;
					quote = SINGLEQUOTE;
					currentSymbol = contentReader.NextSymbol();
				}
				else if (!IsSymbolOp(currentSymbol.symbol) && currentSymbol.symbol != ' ' && currentSymbol.symbol != '\0')
				{
					lexeme += currentSymbol.symbol;
					currentState =  LexerState::TOKEN;
					currentSymbol = contentReader.NextSymbol();
				} else
				{
					token.lexeme = lexeme;
					token.type = TokenType::Id;
					IsStringOperator(token);
					return token;
				}
				break;
			case LexerState::TOKEN_QUOTE:
				if (currentSymbol.symbol == quote)
				{
					lexeme += currentSymbol.symbol;
					upState == LexerState::INIT_SQUARE_BRACKET ? currentState = LexerState::END_SQUARE_BRACKET : currentState = LexerState::TOKEN;
					currentSymbol = contentReader.NextSymbol();
				}
				else if (currentSymbol.symbol == '\\')
				{
					lexeme += currentSymbol.symbol;
					currentState =  LexerState::ESCAPE;
					currentSymbol = contentReader.NextSymbol();
				}
				else if (currentSymbol.symbol != '\0')
				{
					lexeme += currentSymbol.symbol;
					currentSymbol = contentReader.NextSymbol();
				}
				else
				{
					string msj = "Symbol double quote expected";
					throw LexicalException(msj.c_str());
				}
				break;
			case LexerState::ESCAPE:
				if (currentSymbol.symbol != '\0')
				{
					lexeme += currentSymbol.symbol;
					currentState = LexerState::TOKEN_QUOTE;
					currentSymbol = contentReader.NextSymbol();
				}
				else
				{
					string msj = "Symbol EOF not expected";;
					throw LexicalException(msj.c_str());
				}
				break;
			case LexerState::INIT_SQUARE_BRACKET:
				if (currentSymbol.symbol == DOUBLEQUOTE)
				{
					lexeme += currentSymbol.symbol;
					currentState = LexerState::TOKEN_QUOTE;
					upState = LexerState::INIT_SQUARE_BRACKET;
					quote = DOUBLEQUOTE;
					currentSymbol = contentReader.NextSymbol();
					continue;
				}
				else if (currentSymbol.symbol == SINGLEQUOTE)
				{
					lexeme += currentSymbol.symbol;
					currentState = LexerState::TOKEN_QUOTE;
					upState = LexerState::INIT_SQUARE_BRACKET;
					quote = SINGLEQUOTE;
					currentSymbol = contentReader.NextSymbol();
					continue;
				}
				else if (currentSymbol.symbol != RIGHT_SQUARE_BRACKET && currentSymbol.symbol != '\0')
				{
					lexeme += currentSymbol.symbol;
					currentSymbol = contentReader.NextSymbol();
					continue;
				}
			case LexerState::END_SQUARE_BRACKET:
				if (currentSymbol.symbol == RIGHT_SQUARE_BRACKET)
				{
					lexeme += currentSymbol.symbol;
					currentState = LexerState::TOKEN;
					currentSymbol = contentReader.NextSymbol();
				}
				else if (currentSymbol.symbol == ',')
				{
					lexeme += currentSymbol.symbol;
					currentState = LexerState::INIT_SQUARE_BRACKET;
					currentSymbol = contentReader.NextSymbol();
				}
				else
				{
					string msj = "Symbol ] expected";
					throw LexicalException(msj.c_str());
				}
				break;
			case LexerState::SYMBOL_OP:
				token.lexeme = lexeme;
				token.type = singleSymbolDictionary.at(lexeme);
				return token;
			case LexerState::EOFILE:
				token.type = TokenType::EndOfFile;
				return token;
			default:
				break;
		}
	}

	return token;
}


void
Lexer::IsStringOperator(Token& token)
{
	if (!token.lexeme.empty()) {
		switch (token.lexeme.at(0)) {
			 case 'A':
				if (strcmp(token.lexeme.data(), AND) == 0) {
					token.type = TokenType::And;
				}
				break;
			 case 'O':
				if (strcmp(token.lexeme.data(), OR) == 0) {
					token.type = TokenType::Or;
				}
				break;
			 case 'N':
				if (strcmp(token.lexeme.data(), NOT) == 0) {
					token.type = TokenType::Not;
				}
				break;
			 case 'X':
				if (strcmp(token.lexeme.data(), XOR) == 0) {
					token.type = TokenType::Xor;
				}
				break;
			 default:
				return;
		}
	}
}


bool
Lexer::IsSymbolOp(char c)
{
	try {
		singleSymbolDictionary.at(std::string(1, c));
		return true;
	} catch (std::out_of_range) {
		return false;
	}
}