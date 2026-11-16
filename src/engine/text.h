/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
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

#include <cstdint>                 // for std::uint8_t
#include <string>                  // for std::string
#include <string_view>             // for std::string_view

#include "engine/query.h"          // for Query, Op
#include "engine/types.h"          // for termcount


namespace engine {

// Flags for the text of a field: how its query string is read.
namespace text {
inline constexpr unsigned BOOLEAN        = 1U << 0;   // AND, OR, NOT, XOR in the text
inline constexpr unsigned PHRASE         = 1U << 1;   // "quoted phrases"
inline constexpr unsigned LOVEHATE       = 1U << 2;   // +required and -excluded words
inline constexpr unsigned WILDCARD       = 1U << 3;   // trailing '*'
inline constexpr unsigned WILDCARD_SINGLE = 1U << 4;  // '?' inside a pattern
inline constexpr unsigned WILDCARD_MULTI = 1U << 5;   // '*' inside a pattern
inline constexpr unsigned PARTIAL        = 1U << 6;   // the last word may be incomplete
inline constexpr unsigned FUZZY          = 1U << 7;   // words ending in '~' match within an edit distance
inline constexpr unsigned CJK_NGRAM      = 1U << 8;   // CJK text as n-grams
}  // namespace text

// Which words of a query text are stemmed.
enum class Stem : std::uint8_t {
	none,
	some,           // not words that are capitalized, or follow a capitalized one
	all,
	all_z,          // all, with the stem marked with a 'Z' prefix
};

// How the text of one field is read.
struct TextConfig {
	std::string field;                      // the field the words belong to
	Op default_op = Op::INVALID;            // joins words that have no operator; INVALID keeps the parser's default
	unsigned flags = 0;                     // text:: flags
	std::string stop_language;              // stop words language; empty for none
	std::string stem_language;              // stemmer language; empty for none
	Stem stem = Stem::none;
	termcount partial_max_expansion = 0;    // terms a partial word expands to (the most frequent)
	termcount fuzzy_max_expansion = 0;      // terms a fuzzy word expands to (an error when exceeded)
};

// Reads the query string of one field into an engine query, on the host, whatever engine runs
// it. Bottom-up and iterative: the call stack does not grow with the nesting of the text. A text
// without words (empty, or only punctuation) gives a query that matches nothing; a group of only
// stop words keeps its words.
//
// Throws QueryParserError (errc::syntax, a client error) for text the parser rejects, including
// an operator that cannot take its operand, InvalidTextConfigError (errc::invalid) for settings
// it cannot read (an unknown stemmer language, a default operator the parser has no word for),
// and std::bad_alloc when memory runs out.
//
// `description`, when given, receives a description of the tree, for tests.
Query parse_text(std::string_view text, const TextConfig& config, std::string* description = nullptr);

}  // namespace engine
