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

#include <ctype.h>  // for isspace


#define COLON ':'
#define DOUBLE_QUOTE '"'
#define SINGLE_QUOTE '\''
#define SQUARE_BRACKET_LEFT '['
#define SQUARE_BRACKET_RIGHT ']'
#define COMMA ','


FieldParser::FieldParser(const std::string& p)
	: fstr(p),
	  len_field(0), off_field(nullptr),
	  len_field_colon(0), off_field_colon(nullptr),
	  len_value(0), off_value(nullptr),
	  len_double_quote_value(0), off_double_quote_value(nullptr),
	  len_single_quote_value(0), off_single_quote_value(nullptr),
	  skip_quote(false), isrange(false) { }


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
					case SQUARE_BRACKET_LEFT:
						currentState = FieldParser::State::SQUARE_BRACKET_INIT;
						off_value = currentSymbol;
						++len_value;
						isrange = true;
						break;
					case DOUBLE_QUOTE:
						currentState = FieldParser::State::QUOTE;
						quote = DOUBLE_QUOTE;
						off_double_quote_value = currentSymbol;
						off_value = currentSymbol + 1;
						++len_double_quote_value;
						++len_value;
						break;
					case SINGLE_QUOTE:
						currentState = FieldParser::State::QUOTE;
						quote = SINGLE_QUOTE;
						off_single_quote_value = currentSymbol;
						off_value = currentSymbol + 1;
						++len_single_quote_value;
						++len_value;
						break;
					case '\0':
						currentState = FieldParser::State::END;
						break;
					default:
						switch (*currentSymbol) {
							case ' ':
							case '\r':
							case '\n':
							case '\t':
								currentState = FieldParser::State::INIT;
								break;
							default:
								if (++len_field >= 1024) {
									THROW(FieldParserError, "Syntax error in query");
								}
								++len_field_colon;
								currentState = FieldParser::State::FIELD;
								off_field = currentSymbol;
								off_field_colon = currentSymbol;
								break;
						}
						break;
				}
				break;

			case FieldParser::State::FIELD:
				switch (*currentSymbol) {
					case COLON:
						currentState = FieldParser::State::START_VALUE;
						++len_field_colon;
						break;
					case '\0':
						len_value = len_field;
						off_value = off_field;
						len_field = len_field_colon = 0;
						off_field = off_field_colon = nullptr;
						return;
					case ' ':
						break;
					default:
						++len_field;
						++len_field_colon;
						break;
				}
				break;

			case FieldParser::State::START_VALUE:
				switch (*currentSymbol) {
					case DOUBLE_QUOTE:
						currentState = FieldParser::State::QUOTE;
						quote = DOUBLE_QUOTE;
						off_double_quote_value = currentSymbol;
						off_value = off_double_quote_value + 1;
						++len_double_quote_value;
						++len_value;
						break;
					case SINGLE_QUOTE:
						currentState = FieldParser::State::QUOTE;
						quote = SINGLE_QUOTE;
						off_single_quote_value = currentSymbol;
						off_value = off_single_quote_value + 1;
						++len_single_quote_value;
						++len_value;
						break;
					case SQUARE_BRACKET_LEFT:
						currentState = FieldParser::State::SQUARE_BRACKET_INIT;
						off_value = currentSymbol;
						++len_value;
						isrange = true;
						break;
					case '\0':
						currentState = FieldParser::State::END;
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
							case DOUBLE_QUOTE:
								++len_double_quote_value;
								break;
							case SINGLE_QUOTE:
								++len_single_quote_value;
								break;
						}
						break;
					case '\0':
						THROW(FieldParserError, "Expected symbol: '%c'", quote);
					default:
						if (*currentSymbol == quote) {
							currentState = FieldParser::State::COLON_OR_END;
							switch (quote) {
								case DOUBLE_QUOTE:
									++len_double_quote_value;
									--len_value;	// subtract the last quote count
									break;
								case SINGLE_QUOTE:
									++len_single_quote_value;
									--len_value;	// subtract the last quote count
									break;
							}
						} else {
							++len_value;
							switch (quote) {
								case DOUBLE_QUOTE:
									++len_double_quote_value;
									break;
								case SINGLE_QUOTE:
									++len_single_quote_value;
									break;
							}
						}
						break;
				}
				break;

			case FieldParser::State::COLON_OR_END:
				switch (*currentSymbol) {
					case '\0':
						currentState = FieldParser::State::END;
						break;

					case COLON:
						currentState = FieldParser::State::START_VALUE;
						switch (quote) {
							case SINGLE_QUOTE:
								off_field = off_value;
								len_field = len_value;
								off_value = nullptr;
								len_value = 0;
								off_single_quote_value = nullptr;
								break;

							case DOUBLE_QUOTE:
								off_field = off_value;
								len_field = len_value;
								off_value = nullptr;
								len_value = 0;
								off_double_quote_value = nullptr;
								break;
						}
						skip_quote = true;
						break;

					default:
						 THROW(FieldParserError, "Unexpected symbol: %c", *currentSymbol);
						break;
				}

				break;

			case FieldParser::State::ESCAPE:
				if (*currentSymbol != '\0') {
					currentState = oldState;
					switch(currentState) {
						case FieldParser::State::QUOTE:
							++len_value;
							if (quote == DOUBLE_QUOTE) {
								++len_double_quote_value;
							} else if (quote == SINGLE_QUOTE) {
								++len_single_quote_value;
							}
							break;
						case FieldParser::State::SQUARE_BRACKET_FIRST_QUOTE:
							start += *currentSymbol;
							++len_value;
							break;
						case FieldParser::State::SQUARE_BRACKET_SECOND_QUOTE:
							end += *currentSymbol;
							++len_value;
							break;
						default:
							break;
					}
				} else {
					THROW(FieldParserError, "Syntax error in query escaped");
				}
				break;

			case FieldParser::State::VALUE:
				if (*currentSymbol == '\0') {
					currentState = FieldParser::State::END;
				} else if (!isspace(*currentSymbol)) {
					++len_value;
				} else {
					THROW(FieldParserError, "Syntax error in query");
				}
				break;

			case FieldParser::State::SQUARE_BRACKET_INIT:
				switch (*currentSymbol) {
					case DOUBLE_QUOTE:
						currentState = FieldParser::State::SQUARE_BRACKET_FIRST_QUOTE;
						quote = DOUBLE_QUOTE;
						++len_value;
						break;
					case SINGLE_QUOTE:
						currentState = FieldParser::State::SQUARE_BRACKET_FIRST_QUOTE;
						quote = SINGLE_QUOTE;
						++len_value;
						break;
					case COMMA:
						currentState = FieldParser::State::SQUARE_BRACKET;
						++len_value;
						break;
					case SQUARE_BRACKET_RIGHT:
						currentState = FieldParser::State::END;
						++len_value;
						break;
					case '\0':
						THROW(FieldParserError, "Syntax error in query");
					default:
						start += *currentSymbol;
						++len_value;
						break;
				}
				break;

			case FieldParser::State::SQUARE_BRACKET:
				switch (*currentSymbol) {
					case DOUBLE_QUOTE:
						currentState = FieldParser::State::SQUARE_BRACKET_SECOND_QUOTE;
						quote = DOUBLE_QUOTE;
						++len_value;
						break;
					case SINGLE_QUOTE:
						currentState = FieldParser::State::SQUARE_BRACKET_SECOND_QUOTE;
						quote = SINGLE_QUOTE;
						++len_value;
						break;
					case SQUARE_BRACKET_RIGHT:
						currentState = FieldParser::State::END;
						++len_value;
						break;
					case '\0':
						THROW(FieldParserError, "Expected symbol: ']'");
					default:
						end += *currentSymbol;
						++len_value;
						break;
				}
				break;

			case FieldParser::State::SQUARE_BRACKET_FIRST_QUOTE:
				switch (*currentSymbol) {
					case '\\':
						currentState = FieldParser::State::ESCAPE;
						oldState = FieldParser::State::SQUARE_BRACKET_FIRST_QUOTE;
						++len_value;
						break;
					case '\0':
						break;
					default:
						if (*currentSymbol == quote) {
							currentState = FieldParser::State::SQUARE_BRACKET_COMMA_OR_END;
						} else {
							start += *currentSymbol;
						}
						++len_value;
						break;
				}
				break;

			case FieldParser::State::SQUARE_BRACKET_COMMA_OR_END:
					switch (*currentSymbol) {
						case COMMA:
							currentState = FieldParser::State::SQUARE_BRACKET;
							++len_value;
							break;
						case SQUARE_BRACKET_RIGHT:
							currentState = FieldParser::State::END;
							++len_value;
							break;
						default:
							THROW(FieldParserError, "Unexpected symbol: %c", *currentSymbol);
					}
				break;

			case FieldParser::State::SQUARE_BRACKET_SECOND_QUOTE:
				switch (*currentSymbol) {
					case '\\':
						currentState = FieldParser::State::ESCAPE;
						oldState = FieldParser::State::SQUARE_BRACKET_SECOND_QUOTE;
						++len_value;
						break;
					case '\0':
						break;
					default:
						if (*currentSymbol == quote) {
							currentState = FieldParser::State::SQUARE_BRACKET_END;
						} else {
							end += *currentSymbol;
						}
						++len_value;
						break;
				}
				break;

			case FieldParser::State::SQUARE_BRACKET_END:
				if (*currentSymbol == SQUARE_BRACKET_RIGHT) {
					currentState = FieldParser::State::END;
					++len_value;
				} else {
					THROW(FieldParserError, "Expected symbol: ']'");
				}
				break;

			case FieldParser::State::END: {
				return;
			}
		}

		++currentSymbol;
	}
}
