/*
 * Copyright (c) 2026 Dubalu LLC
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

#include <optional>                // for std::optional
#include <string>                  // for std::string
#include <string_view>             // for std::string_view

#include "engine/query.h"          // for Query, TextConfig
#include "xapian.h"                // for Xapian::Query


namespace engine::xapian {

// True when XAPIAND_QUERYPARSER=1: text nodes are read by queryparser instead of Xapian.
bool text_parser_enabled();

// A text node read by queryparser.
struct ParsedText {
	Query query;              // the tree, as an engine query
	std::string description;  // the tree in Xapian's description format
	std::string error;        // set when the text has a syntax error: Xapian's message
	bool failed = false;
	bool unimplemented = false;  // the error is Xapian::UnimplementedError, not QueryParserError
	bool out_of_memory = false;
};

// Reads text with queryparser configured like Xapian::QueryParser for config. Empty when
// queryparser cannot read that configuration (word breaks, an unknown language).
std::optional<ParsedText> parse_text_queryparser(std::string_view query_text, const TextConfig& config);

// The error a failed ParsedText stands for, as Xapian throws it.
[[noreturn]] void throw_text_error(const ParsedText& parsed);

#ifdef XAPIAND_QP_AB
// Compares Xapian's reading of a text (a query, or the message of the error it threw) with
// queryparser's, and prints one `QPAB {json}` line on stderr.
void text_parser_compare(std::string_view query_text, const TextConfig& config, const Xapian::Query* xapian, const std::string* xapian_error);
#endif

}  // namespace engine::xapian
