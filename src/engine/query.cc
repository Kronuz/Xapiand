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

#include "engine/query.h"

#include <algorithm>               // for std::all_of, std::any_of
#include <cstdint>                 // for std::uint64_t

#include "engine/callbacks.h"      // for PostingSource
#include "exception_engine.h"      // for QueryDepthError


namespace engine {

namespace {

// Whether op applied to the subqueries matches nothing, as Xapian simplifies it.
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
Query::term(std::string term, termcount wqf, termpos pos)
{
	auto node = std::make_shared<QueryNode>();
	node->kind = QueryKind::term;
	node->str = std::move(term);
	node->wqf = wqf;
	node->pos = pos;
	return Query(std::move(node));
}


Query
Query::match_all()
{
	return term(std::string());
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
	node->matches_nothing = combines_to_nothing(op, subqueries);
	node->subqueries = std::move(subqueries);
	set_extent(*node, true);
	return Query(std::move(node));
}


Query
Query::append(Op op, Query accumulated, Query next)
{
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
Query::wildcard(std::string pattern, termcount max_expansion, Expansion limit, unsigned pattern_flags, Op combiner)
{
	auto node = std::make_shared<QueryNode>();
	node->kind = QueryKind::wildcard;
	node->str = std::move(pattern);
	node->parameter = max_expansion;
	node->limit = limit;
	node->pattern_flags = pattern_flags;
	node->op = combiner;
	return Query(std::move(node));
}


Query
Query::edit_distance(std::string pattern, termcount max_expansion, Expansion limit, Op combiner, unsigned distance, std::size_t prefix_length)
{
	auto node = std::make_shared<QueryNode>();
	node->kind = QueryKind::edit_distance;
	node->str = std::move(pattern);
	node->parameter = max_expansion;
	node->limit = limit;
	node->op = combiner;
	node->distance = distance;
	node->prefix_length = prefix_length;
	return Query(std::move(node));
}


Query
Query::source(std::unique_ptr<PostingSource> source)
{
	auto node = std::make_shared<QueryNode>();
	node->kind = QueryKind::source;
	node->src = std::move(source);
	return Query(std::move(node));
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
				return n.str.empty() ? "<all>" : n.str;
			case QueryKind::wildcard:
				return "wildcard(" + n.str + ")";
			case QueryKind::edit_distance:
				return "edit_distance(" + n.str + ", " + std::to_string(n.distance) + ")";
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
