/*
 * Copyright (c) 2015-2019 Dubalu LLC
 * Copyright (c) 2014 furan
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

#include "Lexer.h"

#include <cstring>
#include <string>


constexpr const char AND[]    = "AND";
constexpr const char MAYBE[]  = "MAYBE";
constexpr const char OR[]     = "OR";
constexpr const char NOT[]    = "NOT";
constexpr const char XOR[]    = "XOR";


constexpr char DOUBLEQUOTE            = '"';
constexpr char SINGLEQUOTE            = '\'';
constexpr char LEFT_SQUARE_BRACKET    = '[';
constexpr char RIGHT_SQUARE_BRACKET   = ']';


Lexer::Lexer(char* input)
	: contentReader(ContentReader(input)),
	  currentSymbol(contentReader.NextSymbol()) { }


Token
Lexer::NextToken()
{
	std::string lexeme;
	LexerState currentState = LexerState::INIT;
	Token token;
	char quote;

	auto upState = currentState;

	std::string symbol;
	std::string lcSymbol;

	while (true) {
		symbol.clear();
		symbol += currentSymbol.symbol;
		switch (currentState) {
			case LexerState::INIT:
				switch(currentSymbol.symbol) {
					case LEFT_SQUARE_BRACKET:
						lexeme += currentSymbol.symbol;
						currentState = LexerState::INIT_SQUARE_BRACKET;
						currentSymbol = contentReader.NextSymbol();
						break;

					case SINGLEQUOTE:
						lexeme += currentSymbol.symbol;
						currentState = LexerState::TOKEN_QUOTE;
						quote = SINGLEQUOTE;
						currentSymbol = contentReader.NextSymbol();
						break;

					case DOUBLEQUOTE:
						lexeme += currentSymbol.symbol;
						currentState = LexerState::TOKEN_QUOTE;
						quote = DOUBLEQUOTE;
						currentSymbol = contentReader.NextSymbol();
						break;

					case '\0':
						currentState = LexerState::EOFILE;
						break;

					default:
						switch (currentSymbol.symbol) {
							case ' ':
							case '\n':
							case '\t':
							case '\r':
								currentState = LexerState::INIT;
								currentSymbol = contentReader.NextSymbol();
								break;
							case '(':
							case ')':
							case '&':
							case '|':
							case '!':
								lexeme += currentSymbol.symbol;
								currentState = LexerState::SYMBOL_OP;
								currentSymbol = contentReader.NextSymbol();
								break;
							default:
								lexeme += currentSymbol.symbol;
								if (lexeme.size() >= 1024) {
									std::string msj = "Symbol " + symbol + " not expected";
									throw LexicalException(msj);
								}
								currentState = LexerState::TOKEN;
								currentSymbol = contentReader.NextSymbol();
								break;
						}
				}
				break;

			case LexerState::TOKEN:
				switch(currentSymbol.symbol) {
					case DOUBLEQUOTE:
						lexeme += currentSymbol.symbol;
						currentState = LexerState::TOKEN_QUOTE;
						quote = DOUBLEQUOTE;
						currentSymbol = contentReader.NextSymbol();
						break;

					case SINGLEQUOTE:
						lexeme += currentSymbol.symbol;
						currentState = LexerState::TOKEN_QUOTE;
						quote = SINGLEQUOTE;
						currentSymbol = contentReader.NextSymbol();
						break;

					default:
						if (!IsSymbolOp(currentSymbol.symbol) && currentSymbol.symbol != ' ' && currentSymbol.symbol != '\0') {
							lexeme += currentSymbol.symbol;
							currentState =  LexerState::TOKEN;
							currentSymbol = contentReader.NextSymbol();
						}  else {
							token.set_lexeme(lexeme);
							token.set_type(TokenType::Id);
							IsStringOperator(token);
							return token;
						}
				}
				break;

			case LexerState::TOKEN_QUOTE:
				switch (currentSymbol.symbol) {
					case '\\':
						lexeme += currentSymbol.symbol;
						currentState =  LexerState::ESCAPE;
						currentSymbol = contentReader.NextSymbol();
						break;

					case '\0': {
						std::string msj = "Symbol double quote expected";
						throw LexicalException(msj);
					}

					default:
						if (currentSymbol.symbol == quote) {
							lexeme += currentSymbol.symbol;
							upState == LexerState::INIT_SQUARE_BRACKET ? currentState = LexerState::END_SQUARE_BRACKET : currentState = LexerState::TOKEN;
							currentSymbol = contentReader.NextSymbol();
						} else {
							lexeme += currentSymbol.symbol;
							currentSymbol = contentReader.NextSymbol();
						}
				}
				break;

			case LexerState::ESCAPE:
				switch(currentSymbol.symbol) {
					case '\0': {
						std::string msj = "Symbol EOF not expected";
						throw LexicalException(msj);
					}

					default:
						lexeme += currentSymbol.symbol;
						currentState = LexerState::TOKEN_QUOTE;
						currentSymbol = contentReader.NextSymbol();
						break;

				}
				break;

			case LexerState::INIT_SQUARE_BRACKET:
				switch (currentSymbol.symbol) {
					case DOUBLEQUOTE:
						lexeme += currentSymbol.symbol;
						currentState = LexerState::TOKEN_QUOTE;
						upState = LexerState::INIT_SQUARE_BRACKET;
						quote = DOUBLEQUOTE;
						currentSymbol = contentReader.NextSymbol();
						continue;

					case SINGLEQUOTE:
						lexeme += currentSymbol.symbol;
						currentState = LexerState::TOKEN_QUOTE;
						upState = LexerState::INIT_SQUARE_BRACKET;
						quote = SINGLEQUOTE;
						currentSymbol = contentReader.NextSymbol();
						continue;

					default:
						if (currentSymbol.symbol != RIGHT_SQUARE_BRACKET && currentSymbol.symbol != '\0') {
							lexeme += currentSymbol.symbol;
							currentSymbol = contentReader.NextSymbol();
							continue;
						}
				}
				[[fallthrough]];

			case LexerState::END_SQUARE_BRACKET:
				switch (currentSymbol.symbol) {
					case RIGHT_SQUARE_BRACKET:
						lexeme += currentSymbol.symbol;
						currentState = LexerState::TOKEN;
						currentSymbol = contentReader.NextSymbol();
						break;

					case ',':
						lexeme += currentSymbol.symbol;
						currentState = LexerState::INIT_SQUARE_BRACKET;
						currentSymbol = contentReader.NextSymbol();
						break;

					default:
						std::string msj = "Symbol ] expected";
						throw LexicalException(msj);

				}
				break;

			case LexerState::SYMBOL_OP:
				token.set_lexeme(lexeme);
				switch(lexeme.at(0)) {
					case '(':
						token.set_type(TokenType::LeftParenthesis);
						break;
					case ')':
						token.set_type(TokenType::RightParenthesis);
						break;
					case '&':
						token.set_type(TokenType::And);
						break;
					case '|':
						token.set_type(TokenType::Or);
						break;
					case '!':
						token.set_type(TokenType::Not);
						break;
				}
				return token;
			case LexerState::EOFILE:
				token.set_type(TokenType::EndOfFile);
				return token;
			default:
				break;
		}
	}

	return token;
}


void
Lexer::IsStringOperator(Token& token) const
{
	auto lexeme = token.get_lexeme();
	if (!lexeme.empty()) {
		switch (lexeme.at(0)) {
			case 'a':
			case 'A':
				if (strcasecmp(lexeme.data(), AND) == 0) {
					token.set_type(TokenType::And);
				}
				break;
			case 'm':
			case 'M':
				if (strcasecmp(lexeme.data(), MAYBE) == 0) {
					token.set_type(TokenType::Maybe);
				}
				break;
			case 'o':
			case 'O':
				if (strcasecmp(lexeme.data(), OR) == 0) {
					token.set_type(TokenType::Or);
				}
				break;
			case 'n':
			case 'N':
				if (strcasecmp(lexeme.data(), NOT) == 0) {
					token.set_type(TokenType::Not);
				}
				break;
			case 'x':
			case 'X':
				if (strcasecmp(lexeme.data(), XOR) == 0) {
					token.set_type(TokenType::Xor);
				}
				break;
			default:
				return;
		}
	}
}


bool
Lexer::IsSymbolOp(char c) const
{
	switch (c) {
		case '(':
		case ')':
		case '&':
		case '|':
		case '!':
			return true;
		default:
			return false;
	}
}
