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

#include <cstddef>                 // for std::size_t
#include <cstdint>                 // for std::uint8_t
#include <memory>                  // for std::shared_ptr, std::unique_ptr
#include <string>                  // for std::string
#include <string_view>             // for std::string_view
#include <utility>                 // for std::move
#include <vector>                  // for std::vector

#include "engine/types.h"          // for termcount, termpos


namespace engine {

class PostingSource;


// How query nodes combine their subqueries (the operators of Xapiand's query language).
enum class Op : std::uint8_t {
	INVALID,        // no operator
	AND,
	OR,
	AND_NOT,        // left, but not right
	XOR,
	AND_MAYBE,      // left, and right only improves the weight
	FILTER,         // left, restricted to right, which adds no weight
	NEAR,           // terms within a window, in any order
	PHRASE,         // terms within a window, in order
	ELITE_SET,      // the best n subqueries
	SYNONYM,        // subqueries counted as one term
	MAX,            // the best subquery weight
	SCALE_WEIGHT,
	WILDCARD,
	EDIT_DISTANCE,
};

constexpr const char* to_string(Op op) noexcept {
	switch (op) {
		case Op::INVALID: return "INVALID";
		case Op::AND: return "AND";
		case Op::OR: return "OR";
		case Op::AND_NOT: return "AND_NOT";
		case Op::XOR: return "XOR";
		case Op::AND_MAYBE: return "AND_MAYBE";
		case Op::FILTER: return "FILTER";
		case Op::NEAR: return "NEAR";
		case Op::PHRASE: return "PHRASE";
		case Op::ELITE_SET: return "ELITE_SET";
		case Op::SYNONYM: return "SYNONYM";
		case Op::MAX: return "MAX";
		case Op::SCALE_WEIGHT: return "SCALE_WEIGHT";
		case Op::WILDCARD: return "WILDCARD";
		case Op::EDIT_DISTANCE: return "EDIT_DISTANCE";
	}
	return "INVALID";
}

// What a wildcard or edit-distance node does when it matches more terms than its limit.
enum class Expansion : std::uint8_t {
	error,           // fail the search
	first,           // keep the first terms in order
	most_frequent,   // keep the most frequent terms
};

// Pattern characters a wildcard understands, besides a trailing '*'.
inline constexpr unsigned PATTERN_SINGLE = 1;   // '?' matches one character
inline constexpr unsigned PATTERN_MULTI  = 2;   // '*' matches any run anywhere in the pattern

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
inline constexpr unsigned WORD_BREAKS    = 1U << 9;   // CJK text split with word-break rules
}  // namespace text

// Which words of a query text are stemmed.
enum class Stem : std::uint8_t {
	none,
	some,           // not words that are capitalized, or follow a capitalized one
	all,
	all_z,          // all, with the stem marked with a 'Z' prefix
};

// How the text node of one field is analysed. An engine reads the text with its own query parser.
struct TextConfig {
	std::string prefix;                     // the field's term prefix
	Op default_op = Op::INVALID;            // joins words that have no operator; INVALID keeps the parser's default
	unsigned flags = 0;                     // text:: flags
	std::string stop_language;              // stop words language; empty for none
	std::string stem_language;              // stemmer language; empty for none
	Stem stem = Stem::none;
	termcount partial_max_expansion = 0;    // terms a partial word expands to (the most frequent)
	termcount fuzzy_max_expansion = 0;      // terms a fuzzy word expands to (an error when exceeded)
};


struct QueryNode;

// A query, as a value: an immutable tree that shares its subtrees when copied. Each factory
// has the semantics of the constructor of the same shape in Xapian's Query, so a tree built here
// translates to the Xapian tree it replaces. A default Query matches nothing.
class Query {
public:
	Query() = default;

	// Documents containing the term; an empty term matches every document.
	static Query term(std::string term, termcount wqf = 1, termpos pos = 0);
	static Query match_all();

	// op applied to two subqueries.
	static Query combine(Op op, Query left, Query right);

	// op applied to any number of subqueries; parameter is the window of NEAR and PHRASE, and
	// the number of terms ELITE_SET keeps.
	static Query combine(Op op, std::vector<Query> subqueries, termcount parameter = 0);

	// next when the accumulated query matches nothing, else op applied to both. The engine decides
	// that at search time, so a text node that analyses to nothing (only stop words) is skipped.
	static Query append(Op op, Query accumulated, Query next);

	static Query scale(Query subquery, double factor);

	// The terms starting with, or (with PATTERN flags) matching, the pattern, combined by combiner.
	static Query wildcard(std::string pattern, termcount max_expansion = 0, Expansion limit = Expansion::error, unsigned pattern_flags = 0, Op combiner = Op::SYNONYM);

	// The terms within an edit distance of the pattern's tail (from prefix_length on).
	static Query edit_distance(std::string pattern, termcount max_expansion, Expansion limit, Op combiner, unsigned distance, std::size_t prefix_length);

	// Documents a value source accepts. The query keeps the source and hands each engine a clone.
	static Query source(std::unique_ptr<PostingSource> source);

	// Free text of one field, read by the engine's query parser.
	static Query text(std::string text, TextConfig config);

	// True for a query that matches nothing by its structure, with the simplifications Xapian makes:
	// an AND with a subquery that matches nothing matches nothing, an OR of such subqueries too,
	// and so on. A text node is never empty here: the engine's parser decides what it analyses to.
	bool empty() const noexcept;

	const QueryNode* node() const noexcept {
		return node_.get();
	}

	// Readable form, for logs.
	std::string description() const;

private:
	friend struct QueryNode;

	explicit Query(std::shared_ptr<const QueryNode> node) : node_(std::move(node)) { }

	std::shared_ptr<const QueryNode> node_;
};


enum class QueryKind : std::uint8_t {
	term,
	combine,
	append,
	scale,
	wildcard,
	edit_distance,
	source,
	text,
};

// The fields each kind uses are named on its line; the rest are unset.
struct QueryNode {
	// Releases a deep chain of nodes without recursing.
	~QueryNode();

	QueryKind kind = QueryKind::term;
	std::string str;                              // term, wildcard and edit_distance: the term or pattern; text: the text
	termcount wqf = 1;                            // term
	termpos pos = 0;                              // term
	Op op = Op::INVALID;                          // combine, append: the operator; wildcard, edit_distance: the combiner
	std::vector<Query> subqueries;                // combine; append: accumulated, next; scale: one
	termcount parameter = 0;                      // combine: window or elite set size; wildcard, edit_distance: max expansion
	double factor = 1.0;                          // scale
	Expansion limit = Expansion::error;           // wildcard, edit_distance
	unsigned pattern_flags = 0;                   // wildcard
	unsigned distance = 0;                        // edit_distance
	std::size_t prefix_length = 0;                // edit_distance
	std::shared_ptr<const PostingSource> src;     // source
	TextConfig config;                            // text
	bool binary = false;                          // combine: built from two operands
	bool matches_nothing = false;                 // combine, append, scale: simplified to nothing
};

}  // namespace engine
