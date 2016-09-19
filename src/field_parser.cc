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

#include "field_parser.h"


#define DOUBLEDOTS ':'
#define DOUBLEQUOTE '"'
#define SINGLEQUOTE '\''
#define LEFT_SQUARE_BRACKET '['
#define RIGHT_SQUARE_BRACKET ']'
#define COMMA ','


FieldParser::FieldParser(const std::string& p)
	: fstr(p), isEnd(false),
	  len_field(0), off_field(nullptr),
	  len_fieldot(0), off_fieldot(nullptr),
	  len_value(0), off_value(nullptr),
	  len_double_quote_value(0), off_double_quote_value(nullptr),
	  len_single_quote_value(0), off_single_quote_value(nullptr),
	  isrange(false) { }


void
FieldParser::parse()
{
	auto currentState = FieldParser::State::INIT;
	auto oldState = currentState;
	auto currentSymbol = fstr.data();
	char quote;

	while (true) {
		switch (currentState) {
			case FieldParser::State::INIT:
				switch (*currentSymbol) {
					case '_':
						currentState = FieldParser::State::FIELD;
						off_field = currentSymbol;
						off_fieldot = currentSymbol;
						++len_field;
						++len_fieldot;
						break;
					case LEFT_SQUARE_BRACKET:
						currentState = FieldParser::State::INIT_SQUARE_BRACKET;
						isrange = true;
						break;
					case DOUBLEQUOTE:
						currentState = FieldParser::State::QUOTE;
						quote = DOUBLEQUOTE;
						off_double_quote_value = currentSymbol;
						off_value = currentSymbol + 1;
						++len_double_quote_value;
						++len_value;
						break;
					case SINGLEQUOTE:
						currentState = FieldParser::State::QUOTE;
						quote = SINGLEQUOTE;
						off_single_quote_value = currentSymbol;
						off_value = currentSymbol + 1;
						++len_single_quote_value;
						++len_value;
						break;
					default:
						if (isalnum(*currentSymbol)) {
							currentState = FieldParser::State::FIELD;
							off_field = currentSymbol;
							off_fieldot = currentSymbol;
							++len_field;
							++len_fieldot;
						} else if (isspace(*currentSymbol)) {
							currentState = FieldParser::State::INIT;
						} else {
							throw MSG_FieldParserError("Syntax error in query");
						}
						break;
				}
				break;

			case FieldParser::State::FIELD:
				switch (*currentSymbol) {
					case DOUBLEDOTS:
						currentState = FieldParser::State::STARTVALUE;
						++len_fieldot;
						break;
					case '\0':
						len_value = len_field;
						off_value = off_field;
						len_field = len_fieldot = 0;
						off_field = off_fieldot = nullptr;
						return;
					case ' ':
						break
					default:
						++len_field;
						++len_fieldot;
						break;
				}
				break;

			case FieldParser::State::STARTVALUE:
				switch (*currentSymbol) {
					case DOUBLEQUOTE:
						currentState = FieldParser::State::QUOTE;
						quote = DOUBLEQUOTE;
						off_double_quote_value = currentSymbol;
						off_value = off_double_quote_value + 1;
						++len_double_quote_value;
						++len_value;
						break;
					case SINGLEQUOTE:
						currentState = FieldParser::State::QUOTE;
						quote = SINGLEQUOTE;
						off_single_quote_value = currentSymbol;
						off_value = off_single_quote_value + 1;
						++len_single_quote_value;
						++len_value;
						break;
					case LEFT_SQUARE_BRACKET:
						currentState = FieldParser::State::INIT_SQUARE_BRACKET;
						isrange = true;
						break;
					default:
						currentState = FieldParser::State::VALUE;
						off_value = currentSymbol;
						++len_value;
						break;
				}
				break;

			case FieldParser::State::QUOTE:
				switch (*currentSymbol) {
					case '\\':
						currentState = FieldParser::State::ESCAPE;
						oldState = FieldParser::State::QUOTE;
						++len_value;
						switch (quote) {
							case DOUBLEQUOTE:
								++len_double_quote_value;
								break;
							case SINGLEQUOTE:
								++len_single_quote_value;
								break;
						}
						break;
					case '\0':
						throw MSG_FieldParserError("Expected symbol: '%c'", quote);
					default:
						if (*currentSymbol == quote) {
							currentState = FieldParser::State::END;
							switch (quote) {
								case DOUBLEQUOTE:
									++len_double_quote_value;
									--len_value;	// subtract the last quote count
									break;
								case SINGLEQUOTE:
									++len_single_quote_value;
									--len_value;	// subtract the last quote count
									break;
							}
						} else {
							++len_value;
							switch (quote) {
								case DOUBLEQUOTE:
									++len_double_quote_value;
									break;
								case SINGLEQUOTE:
									++len_single_quote_value;
									break;
							}
						}
						break;
				}
				break;

			case FieldParser::State::ESCAPE:
				if (*currentSymbol != '\0') {
					currentState = oldState;
					switch(currentState) {
						case FieldParser::State::QUOTE:
							++len_value;
							if (quote == DOUBLEQUOTE) {
								++len_double_quote_value;
							} else if (quote == SINGLEQUOTE) {
								++len_single_quote_value;
							}
							break;
						case FieldParser::State::QUOTE_SQUARE_BRACKET:
							if (isEnd) {
								end += *currentSymbol;
							} else {
								start += *currentSymbol;
							}
							break;
						default:
							break;
					}
				} else {
					throw MSG_FieldParserError("Syntax error in query escaped");
				}
				break;

			case FieldParser::State::VALUE:
				if (*currentSymbol == '\0') {
					currentState = FieldParser::State::END;
				} else if (!isspace(*currentSymbol)) {
					++len_value;
				} else {
					throw MSG_FieldParserError("Syntax error in query");
				}
				break;

			case FieldParser::State::INIT_SQUARE_BRACKET:
				switch (*currentSymbol) {
					case DOUBLEQUOTE:
						currentState = FieldParser::State::QUOTE_SQUARE_BRACKET;
						quote = DOUBLEQUOTE;
						break;
					case SINGLEQUOTE:
						currentState = FieldParser::State::QUOTE_SQUARE_BRACKET;
						quote = SINGLEQUOTE;
						break;
					case COMMA:
						currentState = FieldParser::State::SQUARE_BRACKET;
						isEnd = true;
						break;
					case RIGHT_SQUARE_BRACKET:
						currentState = FieldParser::State::END;
						break;
					case '\0':
						throw MSG_FieldParserError("Syntax error in query");
					default:
						start += *currentSymbol;
						break;
				}
				break;

			case FieldParser::State::SQUARE_BRACKET:
				switch (*currentSymbol) {
					case DOUBLEQUOTE:
						currentState = FieldParser::State::QUOTE_SQUARE_BRACKET;
						quote = DOUBLEQUOTE;
						break;
					case SINGLEQUOTE:
						currentState = FieldParser::State::QUOTE_SQUARE_BRACKET;
						quote = SINGLEQUOTE;
						break;
					case RIGHT_SQUARE_BRACKET:
						currentState = FieldParser::State::END;
						break;
					case '\0':
						throw MSG_FieldParserError("Expected symbol: ']'");
					default:
						end += *currentSymbol;
						break;
				}
				break;

			case FieldParser::State::QUOTE_SQUARE_BRACKET:
				switch (*currentSymbol) {
					case '\\':
						currentState = FieldParser::State::ESCAPE;
						oldState = FieldParser::State::QUOTE_SQUARE_BRACKET;
						break;
					case '\0':
						break;
					default:
						if (*currentSymbol == quote) {
							currentState = FieldParser::State::END_SQUARE_BRACKET;
						} else if (isEnd) {
							end += *currentSymbol;
						} else {
							start += *currentSymbol;
						}
						break;
				}
				break;

			case FieldParser::State::END_SQUARE_BRACKET:
				if (*currentSymbol == RIGHT_SQUARE_BRACKET) {
					currentState = FieldParser::State::END;
				} else {
					throw MSG_FieldParserError("Expected symbol: ']'");
				}
				break;

			case FieldParser::State::END: {
				return;
			}
		}

		++currentSymbol;
	}
}
