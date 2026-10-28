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

struct QueryNode;

// The largest query a factory builds: levels of nested operators and nodes in all. A larger one
// throws QueryDepthError (a 400). Both bound every recursion over a tree, Xapian's matcher's included.
inline constexpr unsigned MAX_QUERY_DEPTH = 100;
inline constexpr unsigned MAX_QUERY_CLAUSES = 4096;
// Xapian's matcher recurses once per phrase or near group that an AND (or FILTER) joins, however flat the
// query is, so the number it joins is bounded apart from the clause count. See Query::chain().
inline constexpr unsigned MAX_QUERY_CHAIN = 256;

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
	// that at search time, so a text that analyses to nothing (only stop words) is skipped.
	static Query append(Op op, Query accumulated, Query next);

	static Query scale(Query subquery, double factor);

	// The terms starting with, or (with PATTERN flags) matching, the pattern, combined by combiner.
	static Query wildcard(std::string pattern, termcount max_expansion = 0, Expansion limit = Expansion::error, unsigned pattern_flags = 0, Op combiner = Op::SYNONYM);

	// The terms within an edit distance of the pattern's tail (from prefix_length on).
	static Query edit_distance(std::string pattern, termcount max_expansion, Expansion limit, Op combiner, unsigned distance, std::size_t prefix_length);

	// Documents a value source accepts. The query keeps the source and hands each engine a clone.
	static Query source(std::unique_ptr<PostingSource> source);

	// True for a query that matches nothing by its structure, with the simplifications Xapian makes:
	// an AND with a subquery that matches nothing matches nothing, an OR of such subqueries too,
	// and so on.
	bool empty() const noexcept;

	const QueryNode* node() const noexcept {
		return node_.get();
	}

	// Readable form, for logs.
	std::string description() const;

	// Levels of nested operators: a leaf is 1 and a chain of appends adds none.
	unsigned depth() const noexcept;

	// Nodes in the tree: a leaf is 1, an operator adds 1, an append adds none.
	unsigned size() const noexcept;

	// The matcher's recursion over the query, in phrases and near groups: a phrase or near group is 1;
	// AND and FILTER add up their operands, every other operator takes its deepest operand.
	unsigned chain() const noexcept;

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
};

// The fields each kind uses are named on its line; the rest are unset.
struct QueryNode {
	// Releases a deep chain of nodes without recursing.
	~QueryNode();

	QueryKind kind = QueryKind::term;
	std::string str;                              // term, wildcard and edit_distance: the term or pattern
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
	bool binary = false;                          // combine: built from two operands
	bool matches_nothing = false;                 // combine, append, scale: simplified to nothing
	unsigned depth = 1;                           // levels of nested operators, at most MAX_QUERY_DEPTH
	unsigned size = 1;                            // nodes in the tree, shared subtrees counted each time, at most MAX_QUERY_CLAUSES
	unsigned chain = 0;                           // see Query::chain(), at most MAX_QUERY_CHAIN
};

}  // namespace engine
