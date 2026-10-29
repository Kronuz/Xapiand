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

#include "engine/query.h"

#include <algorithm>               // for std::all_of, std::any_of
#include <cstdint>                 // for std::uint64_t

#include "engine/callbacks.h"      // for PostingSource
#include "exception_engine.h"      // for QueryDepthError


namespace engine {

namespace {

// A word: a term of a field, not the term that matches every document.
bool
is_word(const Query& query)
{
	const auto* node = query.node();
	return node != nullptr && node->kind == QueryKind::term && !(node->field.empty() && node->str.empty());
}

// Whether the query can fill one position of a phrase: a word, an expansion, or an OR or SYNONYM of those.
bool
is_alternative(const Query& query)
{
	// A chain of appends is walked along its spine, so its length costs no stack.
	const Query* at = &query;
	while (at->node() != nullptr && at->node()->kind == QueryKind::append) {
		const auto& node = *at->node();
		if (!node.subqueries[0].empty() && node.op != Op::OR) {
			return false;
		}
		if (!is_alternative(node.subqueries[1])) {
			return false;
		}
		at = &node.subqueries[0];
	}
	const auto* node = at->node();
	if (node == nullptr) {
		return true;
	}
	switch (node->kind) {
		case QueryKind::term:
			return is_word(*at);
		case QueryKind::wildcard:
		case QueryKind::edit_distance:
			return true;
		case QueryKind::combine:
			return (node->op == Op::OR || node->op == Op::SYNONYM) && std::all_of(node->subqueries.begin(), node->subqueries.end(), is_alternative);
		default:
			return false;
	}
}

// Whether the operator joins queries (the rest take words).
bool
is_joining(Op op)
{
	switch (op) {
		case Op::AND:
		case Op::OR:
		case Op::AND_NOT:
		case Op::AND_MAYBE:
		case Op::FILTER:
		case Op::XOR:
		case Op::MAX:
			return true;
		default:
			return false;
	}
}

[[noreturn]] void
invalid(const std::string& what)
{
	throw QueryParserError(__func__, __FILE__, __LINE__, "QueryParserError", "{} is not a valid query", what);
}

// Refuses op applied to the subqueries when it is not a valid query.
void
check_combine(Op op, const std::vector<Query>& subqueries)
{
	switch (op) {
		case Op::PHRASE:
		case Op::NEAR:
			if (!std::all_of(subqueries.begin(), subqueries.end(), is_alternative)) {
				invalid(std::string("A ") + to_string(op) + " of anything but words and their alternatives (wildcards, fuzzy words, and ORs of them)");
			}
			return;
		case Op::SYNONYM:
			if (!std::all_of(subqueries.begin(), subqueries.end(), is_word)) {
				invalid("A SYNONYM of anything but words");
			}
			return;
		default:
			if (!is_joining(op)) {
				invalid(std::string("The ") + to_string(op) + " operator over a list of queries");
			}
	}
}

// Whether a PHRASE or NEAR in the tree has a position that holds alternatives.
bool
has_alternatives(const QueryNode& node)
{
	if ((node.op == Op::PHRASE || node.op == Op::NEAR) && node.kind == QueryKind::combine &&
		!std::all_of(node.subqueries.begin(), node.subqueries.end(), [](const Query& sub) { return sub.empty() || is_word(sub); })) {
		return true;
	}
	return std::any_of(node.subqueries.begin(), node.subqueries.end(), [](const Query& sub) { return sub.alternatives(); });
}

// Refuses a combiner of an expansion other than SYNONYM, OR or MAX.
void
check_combiner(Op combiner)
{
	if (combiner != Op::SYNONYM && combiner != Op::OR && combiner != Op::MAX) {
		invalid(std::string("An expansion combined by ") + to_string(combiner));
	}
}

// Whether op applied to the subqueries matches nothing.
bool
combines_to_nothing(Op op, const std::vector<Query>& subqueries)
{
	if (subqueries.empty()) {
		return true;
	}
	const auto is_empty = [](const Query& q) { return q.empty(); };
	switch (op) {
		case Op::AND:
		case Op::PHRASE:
		case Op::NEAR:
		case Op::FILTER:
			return std::any_of(subqueries.begin(), subqueries.end(), is_empty);
		case Op::AND_NOT:
		case Op::AND_MAYBE:
			return subqueries.front().empty();
		default:
			return std::all_of(subqueries.begin(), subqueries.end(), is_empty);
	}
}

// Sets the node's depth, size and chain from its subqueries (an operator adds a level and a node, an append
// neither), and refuses a tree beyond MAX_QUERY_DEPTH, MAX_QUERY_CLAUSES or MAX_QUERY_CHAIN.
void
set_extent(QueryNode& node, bool operator_node)
{
	unsigned deepest = 0;
	std::uint64_t total = operator_node ? 1 : 0;
	for (const auto& sub : node.subqueries) {
		deepest = std::max(deepest, sub.depth());
		total += sub.size();
	}
	node.depth = deepest + (operator_node ? 1 : 0);
	if (node.depth > MAX_QUERY_DEPTH) {
		throw QueryDepthError(__func__, __FILE__, __LINE__, "QueryDepthError", "Query is nested too deeply: the limit is {} levels", MAX_QUERY_DEPTH);
	}
	if (total > MAX_QUERY_CLAUSES) {
		throw QueryDepthError(__func__, __FILE__, __LINE__, "QueryDepthError", "Query is too large: the limit is {} clauses", MAX_QUERY_CLAUSES);
	}
	node.size = static_cast<unsigned>(total);

	// A phrase or near group is one link; AND and FILTER join their operands' links, other operators run them apart.
	std::uint64_t links = 0;
	if (node.kind == QueryKind::combine && (node.op == Op::PHRASE || node.op == Op::NEAR)) {
		links = 1;
	} else {
		const bool joins = node.op == Op::AND || node.op == Op::FILTER;
		for (const auto& sub : node.subqueries) {
			links = joins ? links + sub.chain() : std::max<std::uint64_t>(links, sub.chain());
		}
	}
	if (links > MAX_QUERY_CHAIN) {
		throw QueryDepthError(__func__, __FILE__, __LINE__, "QueryDepthError", "Query joins too many phrases: the limit is {}", MAX_QUERY_CHAIN);
	}
	node.chain = static_cast<unsigned>(links);
	node.alternatives = has_alternatives(node);
}

}  // namespace


QueryNode::~QueryNode()
{
	// The last owner of a node hands its children to a list, so a long chain frees in a loop.
	std::vector<std::shared_ptr<const QueryNode>> pending;
	const auto take = [&pending](std::vector<Query>& subqueries) {
		for (auto& sub : subqueries) {
			if (sub.node_ && sub.node_.use_count() == 1) {
				pending.push_back(std::move(sub.node_));
			}
		}
	};
	take(subqueries);
	while (!pending.empty()) {
		const auto node = std::move(pending.back());
		pending.pop_back();
		take(const_cast<QueryNode&>(*node).subqueries);
	}
}


Query
Query::term(std::string field, std::string term, termcount wqf, termpos pos)
{
	auto node = std::make_shared<QueryNode>();
	node->kind = QueryKind::term;
	node->field = std::move(field);
	node->str = std::move(term);
	node->wqf = wqf;
	node->pos = pos;
	return Query(std::move(node));
}


Query
Query::match_all()
{
	return term(std::string(), std::string());
}


Query
Query::combine(Op op, Query left, Query right)
{
	auto node = std::make_shared<QueryNode>();
	node->kind = QueryKind::combine;
	node->op = op;
	node->binary = true;
	node->subqueries.reserve(2);
	node->subqueries.push_back(std::move(left));
	node->subqueries.push_back(std::move(right));
	check_combine(op, node->subqueries);
	node->matches_nothing = combines_to_nothing(op, node->subqueries);
	set_extent(*node, true);
	return Query(std::move(node));
}


Query
Query::combine(Op op, std::vector<Query> subqueries, termcount parameter)
{
	auto node = std::make_shared<QueryNode>();
	node->kind = QueryKind::combine;
	node->op = op;
	node->parameter = parameter;
	check_combine(op, subqueries);
	node->matches_nothing = combines_to_nothing(op, subqueries);
	node->subqueries = std::move(subqueries);
	set_extent(*node, true);
	return Query(std::move(node));
}


Query
Query::append(Op op, Query accumulated, Query next)
{
	if (!accumulated.empty() && !is_joining(op)) {
		invalid(std::string("The ") + to_string(op) + " operator onto a query");
	}
	auto node = std::make_shared<QueryNode>();
	node->kind = QueryKind::append;
	node->op = op;
	node->subqueries.reserve(2);
	node->subqueries.push_back(std::move(accumulated));
	node->subqueries.push_back(std::move(next));
	node->matches_nothing = node->subqueries[0].empty() ? node->subqueries[1].empty() : combines_to_nothing(op, node->subqueries);
	set_extent(*node, false);
	return Query(std::move(node));
}


Query
Query::scale(Query subquery, double factor)
{
	auto node = std::make_shared<QueryNode>();
	node->kind = QueryKind::scale;
	node->factor = factor;
	node->matches_nothing = subquery.empty();
	node->subqueries.push_back(std::move(subquery));
	set_extent(*node, true);
	return Query(std::move(node));
}


Query
Query::wildcard(std::string field, std::string pattern, termcount max_expansion, Expansion limit, unsigned pattern_flags, Op combiner)
{
	check_combiner(combiner);
	auto node = std::make_shared<QueryNode>();
	node->kind = QueryKind::wildcard;
	node->field = std::move(field);
	node->str = std::move(pattern);
	node->parameter = max_expansion;
	node->limit = limit;
	node->pattern_flags = pattern_flags;
	node->op = combiner;
	return Query(std::move(node));
}


Query
Query::edit_distance(std::string field, std::string pattern, termcount max_expansion, Expansion limit, Op combiner, unsigned distance, std::size_t prefix_length)
{
	check_combiner(combiner);
	auto node = std::make_shared<QueryNode>();
	node->kind = QueryKind::edit_distance;
	node->field = std::move(field);
	node->str = std::move(pattern);
	node->parameter = max_expansion;
	node->limit = limit;
	node->op = combiner;
	node->distance = distance;
	node->prefix_length = prefix_length;
	return Query(std::move(node));
}


Query
Query::field(std::string field)
{
	return wildcard(std::move(field), std::string());
}


Query
Query::source(std::unique_ptr<PostingSource> source)
{
	if (!source) {
		throw Error(errc::invalid, "A source node has no source");
	}
	auto node = std::make_shared<QueryNode>();
	node->kind = QueryKind::source;
	node->src = std::move(source);
	return Query(std::move(node));
}


bool
Query::alternatives() const noexcept
{
	return node_ && node_->alternatives;
}


unsigned
Query::depth() const noexcept
{
	return node_ ? node_->depth : 0;
}


unsigned
Query::size() const noexcept
{
	return node_ ? node_->size : 0;
}


unsigned
Query::chain() const noexcept
{
	return node_ ? node_->chain : 0;
}


bool
Query::empty() const noexcept
{
	return !node_ || node_->matches_nothing;
}


std::string
Query::description() const
{
	// Post-order on an explicit stack: the call stack does not grow with the depth of the tree.
	struct Frame {
		const QueryNode* node;
		std::size_t cursor;
		std::vector<std::string> parts;
	};
	const auto leaf = [](const QueryNode& n) -> std::string {
		switch (n.kind) {
			case QueryKind::term:
				return n.field.empty() && n.str.empty() ? "<all>" : n.field + "/" + n.str;
			case QueryKind::wildcard:
				return "wildcard(" + n.field + "/" + n.str + ")";
			case QueryKind::edit_distance:
				return "edit_distance(" + n.field + "/" + n.str + ", " + std::to_string(n.distance) + ")";
			case QueryKind::source:
				return n.src->description();
			default:
				return "?";
		}
	};
	const auto is_inner = [](const QueryNode& n) {
		return n.kind == QueryKind::combine || n.kind == QueryKind::append || n.kind == QueryKind::scale;
	};
	if (!node_) {
		return "<nothing>";
	}
	if (!is_inner(*node_)) {
		return leaf(*node_);
	}
	std::vector<Frame> stack;
	stack.push_back({ node_.get(), 0, {} });
	std::string result;
	while (!stack.empty()) {
		auto& frame = stack.back();
		const auto& n = *frame.node;
		if (frame.cursor < n.subqueries.size()) {
			const auto* child = n.subqueries[frame.cursor++].node();
			if (child == nullptr) {
				frame.parts.emplace_back("<nothing>");
			} else if (is_inner(*child)) {
				stack.push_back({ child, 0, {} });
			} else {
				frame.parts.push_back(leaf(*child));
			}
			continue;
		}
		std::string text;
		if (n.kind == QueryKind::scale) {
			text = "(" + frame.parts.front() + " SCALE " + std::to_string(n.factor) + ")";
		} else {
			const char* name = to_string(n.op);
			text = "(";
			for (std::size_t i = 0; i < frame.parts.size(); ++i) {
				if (i != 0) {
					text.append(" ").append(name).append(" ");
				}
				text.append(frame.parts[i]);
			}
			if (n.parameter != 0) {
				text.append(" ").append(std::to_string(n.parameter));
			}
			text.append(")");
		}
		stack.pop_back();
		if (stack.empty()) {
			result = std::move(text);
		} else {
			stack.back().parts.push_back(std::move(text));
		}
	}
	return result;
}

}  // namespace engine
