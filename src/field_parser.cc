/*
 * Copyright (c) 2015-2018 Dubalu LLC
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

#include "field_parser.h"

#include <cstring>  // for memset
#include <cctype>  // for isspace


#define TOKEN_COLON                 ':'
#define TOKEN_COMMA                 ','
#define TOKEN_DOT                   '.'
#define TOKEN_DOUBLE_QUOTE          '"'
#define TOKEN_PARENTHESIS_LEFT      '('
#define TOKEN_PARENTHESIS_RIGHT     ')'
#define TOKEN_SINGLE_QUOTE          '\''
#define TOKEN_SQUARE_BRACKET_LEFT   '['
#define TOKEN_SQUARE_BRACKET_RIGHT  ']'


FieldParser::FieldParser(std::string_view p)
	: fstr(p),
	  len_field(0), off_field(nullptr),
	  len_field_colon(0), off_field_colon(nullptr),
	  off_values(nullptr),
	  lvl(0),
	  lens{}, offs{},
	  lens_single_quote{}, offs_single_quote{},
	  lens_double_quote{}, offs_double_quote{},
	  skip_quote(false), range(Range::none) { }


void
FieldParser::parse(size_t lvl_max)
{
	auto currentState = FieldParser::State::INIT;
	auto currentSymbol = fstr.data();
	char quote;

	if (lvl_max > LVL_MAX) {
		lvl_max = LVL_MAX;
	}

	off_values = currentSymbol;

	while (true) {
		switch (currentState) {
			case FieldParser::State::INIT:
				switch (*currentSymbol) {
					case TOKEN_PARENTHESIS_LEFT:
						currentState = FieldParser::State::SQUARE_BRACKET_INIT;
						range = Range::open;
						break;
					case TOKEN_SQUARE_BRACKET_LEFT:
						currentState = FieldParser::State::SQUARE_BRACKET_INIT;
						range = Range::closed;
						break;
					case TOKEN_DOUBLE_QUOTE:
						currentState = FieldParser::State::QUOTE;
						quote = TOKEN_DOUBLE_QUOTE;
						offs_double_quote[lvl] = currentSymbol;
						offs[lvl] = currentSymbol + 1;
						++lens_double_quote[lvl];
						break;
					case TOKEN_SINGLE_QUOTE:
						currentState = FieldParser::State::QUOTE;
						quote = TOKEN_SINGLE_QUOTE;
						offs_single_quote[lvl] = currentSymbol;
						offs[lvl] = currentSymbol + 1;
						++lens_single_quote[lvl];
						break;
					case '\0':
						currentState = FieldParser::State::END;
						break;
					case ' ':
					case '\r':
					case '\n':
					case '\t':
						currentState = FieldParser::State::INIT;
						break;
					case TOKEN_DOT:
						if (*(currentSymbol + 1) == TOKEN_DOT) {
							currentState = FieldParser::State::DOT_DOT_INIT;
							range = Range::closed;
							if (++lvl > lvl_max) {
								THROW(FieldParserError, "Too many levels!");
							}
							++currentSymbol;
							break;
						}
						/* FALLTHROUGH */
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

			case FieldParser::State::FIELD:
				switch (*currentSymbol) {
					case TOKEN_COLON:
						currentState = FieldParser::State::VALUE_INIT;
						off_values = currentSymbol + 1;
						++len_field_colon;
						memset(lens, 0, sizeof(lens));
						memset(offs, 0, sizeof(offs));
						lvl = 0;
						break;
					case '\0':
						lens[lvl] = len_field;
						offs[lvl] = off_field;
						len_field = len_field_colon = 0;
						off_field = off_field_colon = nullptr;
						return;
					case ' ':
						break;
					case TOKEN_DOT:
						if (*(currentSymbol + 1) == TOKEN_DOT) {
							lens[lvl] = len_field;
							offs[lvl] = off_field;
							len_field = len_field_colon = 0;
							off_field = off_field_colon = nullptr;
							currentState = FieldParser::State::DOT_DOT_INIT;
							range = Range::closed;
							if (++lvl > lvl_max) {
								THROW(FieldParserError, "Too many levels!");
							}
							++currentSymbol;
							break;
						}
						/* FALLTHROUGH */
					default:
						++len_field;
						++len_field_colon;
						break;
				}
				break;

			case FieldParser::State::VALUE_INIT:
			case FieldParser::State::DOT_DOT_INIT:
				switch (*currentSymbol) {
					case TOKEN_DOUBLE_QUOTE:
						currentState = FieldParser::State::QUOTE;
						quote = TOKEN_DOUBLE_QUOTE;
						offs_double_quote[lvl] = currentSymbol;
						offs[lvl] = currentSymbol + 1;
						++lens_double_quote[lvl];
						break;
					case TOKEN_SINGLE_QUOTE:
						currentState = FieldParser::State::QUOTE;
						quote = TOKEN_SINGLE_QUOTE;
						offs_single_quote[lvl] = currentSymbol;
						offs[lvl] = currentSymbol + 1;
						++lens_single_quote[lvl];
						break;
					case TOKEN_PARENTHESIS_LEFT:
						currentState = FieldParser::State::SQUARE_BRACKET_INIT;
						range = Range::open;
						break;
					case TOKEN_SQUARE_BRACKET_LEFT:
						currentState = FieldParser::State::SQUARE_BRACKET_INIT;
						range = Range::closed;
						break;
					case '\0':
						currentState = FieldParser::State::END;
						break;
					case TOKEN_DOT:
						if (*(currentSymbol + 1) == TOKEN_DOT) {
							currentState = FieldParser::State::DOT_DOT_INIT;
							range = Range::closed;
							if (++lvl > lvl_max) {
								THROW(FieldParserError, "Too many levels!");
							}
							++currentSymbol;
							break;
						}
						/* FALLTHROUGH */
					default:
						currentState = (currentState == FieldParser::State::VALUE_INIT) ? FieldParser::State::VALUE : FieldParser::State::DOT_DOT;
						offs[lvl] = currentSymbol;
						++lens[lvl];
						break;
				}
				break;

			case FieldParser::State::QUOTE:
				switch (*currentSymbol) {
					case '\0':
						THROW(FieldParserError, "Expected symbol: '%c'", quote);
					case '\\':
						if (*(currentSymbol + 1) == '\0') {
							THROW(FieldParserError, "Syntax error: EOL while scanning quoted string");
						}
						++lens[lvl];
						switch (quote) {
							case TOKEN_DOUBLE_QUOTE:
								++lens_double_quote[lvl];
								break;
							case TOKEN_SINGLE_QUOTE:
								++lens_single_quote[lvl];
								break;
						}
						++currentSymbol;
						/* FALLTHROUGH */
					default:
						if (*currentSymbol == quote) {
							currentState = FieldParser::State::COLON;
							switch (quote) {
								case TOKEN_DOUBLE_QUOTE:
									++lens_double_quote[lvl];
									break;
								case TOKEN_SINGLE_QUOTE:
									++lens_single_quote[lvl];
									break;
							}
							break;
						}
						++lens[lvl];
						switch (quote) {
							case TOKEN_DOUBLE_QUOTE:
								++lens_double_quote[lvl];
								break;
							case TOKEN_SINGLE_QUOTE:
								++lens_single_quote[lvl];
								break;
						}
						break;
				}
				break;

			case FieldParser::State::COLON:
				switch (*currentSymbol) {
					case '\0':
						currentState = FieldParser::State::END;
						break;

					case TOKEN_COLON:
						currentState = FieldParser::State::VALUE_INIT;
						off_values = currentSymbol + 1;
						switch (quote) {
							case TOKEN_SINGLE_QUOTE:
								off_field = offs[lvl];
								len_field = lens[lvl];
								offs[lvl] = nullptr;
								lens[lvl] = 0;
								offs_single_quote[lvl] = nullptr;
								break;

							case TOKEN_DOUBLE_QUOTE:
								off_field = offs[lvl];
								len_field = lens[lvl];
								offs[lvl] = nullptr;
								lens[lvl] = 0;
								offs_single_quote[lvl] = nullptr;
								break;
						}
						skip_quote = true;
						memset(lens, 0, sizeof(lens));
						memset(offs, 0, sizeof(offs));
						lvl = 0;
						break;

					case TOKEN_DOT:
						if (*(currentSymbol + 1) == TOKEN_DOT) {
							currentState = FieldParser::State::DOT_DOT_INIT;
							range = Range::closed;
							if (++lvl > lvl_max) {
								THROW(FieldParserError, "Too many levels!");
							}
							++currentSymbol;
							break;
						}
						/* FALLTHROUGH */
					default:
						THROW(FieldParserError, "Unexpected symbol: '%c'", *currentSymbol);
				}
				break;

			case FieldParser::State::VALUE:
				switch (*currentSymbol) {
					case '\0':
						currentState = FieldParser::State::END;
						break;
					case ' ':
					case '\r':
					case '\n':
					case '\t':
						THROW(FieldParserError, "Syntax error in query");
					case TOKEN_DOT:
						if (*(currentSymbol + 1) == TOKEN_DOT) {
							currentState = FieldParser::State::DOT_DOT_INIT;
							range = Range::closed;
							if (++lvl > lvl_max) {
								THROW(FieldParserError, "Too many levels!");
							}
							++currentSymbol;
							break;
						}
						/* FALLTHROUGH */
					default:
						++lens[lvl];
						break;
				}
				break;

			case FieldParser::State::DOT_DOT:
				switch (*currentSymbol) {
					case '\0':
						currentState = FieldParser::State::END;
						break;
					case ' ':
					case '\r':
					case '\n':
					case '\t':
						THROW(FieldParserError, "Syntax error in query");
					case TOKEN_DOT:
						if (*(currentSymbol + 1) == TOKEN_DOT) {
							currentState = FieldParser::State::DOT_DOT_INIT;
							range = Range::closed;
							if (++lvl > lvl_max) {
								THROW(FieldParserError, "Too many levels!");
							}
							++currentSymbol;
							break;
						}
						/* FALLTHROUGH */
					default:
						++lens[lvl];
						break;
				}
				break;

			case FieldParser::State::SQUARE_BRACKET_INIT:
				switch (*currentSymbol) {
					case TOKEN_DOUBLE_QUOTE:
						currentState = FieldParser::State::SQUARE_BRACKET_QUOTE;
						quote = TOKEN_DOUBLE_QUOTE;
						offs_double_quote[lvl] = currentSymbol;
						offs[lvl] = currentSymbol + 1;
						++lens_double_quote[lvl];
						break;
					case TOKEN_SINGLE_QUOTE:
						currentState = FieldParser::State::SQUARE_BRACKET_QUOTE;
						quote = TOKEN_SINGLE_QUOTE;
						offs_single_quote[lvl] = currentSymbol;
						offs[lvl] = currentSymbol + 1;
						++lens_single_quote[lvl];
						break;
					case '\0':
						THROW(FieldParserError, "Syntax error in query");
					case TOKEN_COMMA:
						currentState = FieldParser::State::SQUARE_BRACKET_INIT;
						if (++lvl > lvl_max) {
							THROW(FieldParserError, "Too many levels!");
						}
						break;
					case TOKEN_PARENTHESIS_RIGHT:
						if (range == Range::closed) {
							range = Range::closed_left;
						}
						currentState = FieldParser::State::END;
						break;
					case TOKEN_SQUARE_BRACKET_RIGHT:
						if (range == Range::open) {
							range = Range::closed_right;
						}
						currentState = FieldParser::State::END;
						break;
					default:
						currentState = FieldParser::State::SQUARE_BRACKET;
						offs[lvl] = currentSymbol;
						++lens[lvl];
						break;
				}
				break;

			case FieldParser::State::SQUARE_BRACKET:
				switch (*currentSymbol) {
					case TOKEN_COMMA:
						currentState = FieldParser::State::SQUARE_BRACKET_INIT;
						if (++lvl > lvl_max) {
							THROW(FieldParserError, "Too many levels!");
						}
						break;
					case TOKEN_PARENTHESIS_RIGHT:
						if (range == Range::closed) {
							range = Range::closed_left;
						}
						currentState = FieldParser::State::END;
						break;
					case TOKEN_SQUARE_BRACKET_RIGHT:
						if (range == Range::open) {
							range = Range::closed_right;
						}
						currentState = FieldParser::State::END;
						break;
					case '\0':
						THROW(FieldParserError, "Expected symbol: ']'");
					default:
						++lens[lvl];
						break;
				}
				break;

			case FieldParser::State::SQUARE_BRACKET_QUOTE:
				switch (*currentSymbol) {
					case '\0':
						break;
					case '\\':
						if (*(currentSymbol + 1) == '\0') {
							THROW(FieldParserError, "Syntax error: EOL while scanning quoted string");
						}
						++lens[lvl];
						switch (quote) {
							case TOKEN_DOUBLE_QUOTE:
								++lens_double_quote[lvl];
								break;
							case TOKEN_SINGLE_QUOTE:
								++lens_single_quote[lvl];
								break;
						}
						++currentSymbol;
						/* FALLTHROUGH */
					default:
						if (*currentSymbol == quote) {
							currentState = FieldParser::State::SQUARE_BRACKET_COMMA;
							switch (quote) {
								case TOKEN_DOUBLE_QUOTE:
									++lens_double_quote[lvl];
									break;
								case TOKEN_SINGLE_QUOTE:
									++lens_single_quote[lvl];
									break;
							}
							break;
						}
						++lens[lvl];
						switch (quote) {
							case TOKEN_DOUBLE_QUOTE:
								++lens_double_quote[lvl];
								break;
							case TOKEN_SINGLE_QUOTE:
								++lens_single_quote[lvl];
								break;
						}
						break;
				}
				break;

			case FieldParser::State::SQUARE_BRACKET_COMMA:
				switch (*currentSymbol) {
					case TOKEN_COMMA:
						currentState = FieldParser::State::SQUARE_BRACKET_INIT;
						if (++lvl > lvl_max) {
							THROW(FieldParserError, "Too many levels!");
						}
						break;
					case TOKEN_PARENTHESIS_RIGHT:
						if (range == Range::closed) {
							range = Range::closed_left;
						}
						currentState = FieldParser::State::END;
						break;
					case TOKEN_SQUARE_BRACKET_RIGHT:
						if (range == Range::open) {
							range = Range::closed_right;
						}
						currentState = FieldParser::State::END;
						break;
					default:
						THROW(FieldParserError, "Unexpected symbol: '%c'", *currentSymbol);
				}
				break;

			case FieldParser::State::SQUARE_BRACKET_END:
				switch (*currentSymbol) {
					case TOKEN_PARENTHESIS_RIGHT:
						if (range == Range::closed) {
							range = Range::closed_left;
						}
						currentState = FieldParser::State::END;
						++lens[lvl];
						break;
					case TOKEN_SQUARE_BRACKET_RIGHT:
						if (range == Range::open) {
							range = Range::closed_right;
						}
						currentState = FieldParser::State::END;
						++lens[lvl];
						break;
					default:
						THROW(FieldParserError, "Expected symbol: ']'");
				}
				break;

			case FieldParser::State::END:
				return;
		}

		if (*currentSymbol != 0) {
			++currentSymbol;
		}
	}
}
