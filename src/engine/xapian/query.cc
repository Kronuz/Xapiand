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

#include "engine/xapian/query.h"

#include <utility>                 // for std::move
#include <vector>                  // for std::vector

#include "engine/callbacks.h"      // for PostingSource
#include "engine/xapian/callbacks.h"  // for PostingSourceAdapter


namespace engine::xapian {

namespace {

int
wildcard_flags(Expansion limit, unsigned pattern_flags)
{
	int flags = 0;
	switch (limit) {
		case Expansion::error: flags = Xapian::Query::WILDCARD_LIMIT_ERROR; break;
		case Expansion::first: flags = Xapian::Query::WILDCARD_LIMIT_FIRST; break;
		case Expansion::most_frequent: flags = Xapian::Query::WILDCARD_LIMIT_MOST_FREQUENT; break;
	}
	if (pattern_flags & PATTERN_SINGLE) {
		flags |= Xapian::Query::WILDCARD_PATTERN_SINGLE;
	}
	if (pattern_flags & PATTERN_MULTI) {
		flags |= Xapian::Query::WILDCARD_PATTERN_MULTI;
	}
	return flags;
}

}  // namespace


Xapian::Query::op
to_xapian(Op op)
{
	switch (op) {
		case Op::INVALID: return Xapian::Query::OP_INVALID;
		case Op::AND: return Xapian::Query::OP_AND;
		case Op::OR: return Xapian::Query::OP_OR;
		case Op::AND_NOT: return Xapian::Query::OP_AND_NOT;
		case Op::XOR: return Xapian::Query::OP_XOR;
		case Op::AND_MAYBE: return Xapian::Query::OP_AND_MAYBE;
		case Op::FILTER: return Xapian::Query::OP_FILTER;
		case Op::NEAR: return Xapian::Query::OP_NEAR;
		case Op::PHRASE: return Xapian::Query::OP_PHRASE;
		case Op::ELITE_SET: return Xapian::Query::OP_ELITE_SET;
		case Op::SYNONYM: return Xapian::Query::OP_SYNONYM;
		case Op::MAX: return Xapian::Query::OP_MAX;
		case Op::SCALE_WEIGHT: return Xapian::Query::OP_SCALE_WEIGHT;
		case Op::WILDCARD: return Xapian::Query::OP_WILDCARD;
		case Op::EDIT_DISTANCE: return Xapian::Query::OP_EDIT_DISTANCE;
	}
	return Xapian::Query::OP_INVALID;
}


namespace {

bool
is_inner(const QueryNode& node)
{
	return node.kind == QueryKind::combine || node.kind == QueryKind::append || node.kind == QueryKind::scale;
}


Xapian::Query
leaf(const QueryNode& node)
{
	switch (node.kind) {
		case QueryKind::term:
			return Xapian::Query(node.str, node.wqf, node.pos);

		case QueryKind::wildcard:
			return Xapian::Query(Xapian::Query::OP_WILDCARD, node.str, node.parameter, wildcard_flags(node.limit, node.pattern_flags), to_xapian(node.op));

		case QueryKind::edit_distance:
			return Xapian::Query(Xapian::Query::OP_EDIT_DISTANCE, node.str, node.parameter, wildcard_flags(node.limit, 0), to_xapian(node.op), node.distance, node.prefix_length);

		case QueryKind::source: {
			// release() hands the adapter to the Query, which owns it.
			auto* adapter = new PostingSourceAdapter(node.src->clone());
			return Xapian::Query(adapter->release());
		}

		default:
			return Xapian::Query();
	}
}


// An operator node over its converted subqueries.
Xapian::Query
inner(const QueryNode& node, std::vector<Xapian::Query>& subqueries)
{
	switch (node.kind) {
		case QueryKind::combine:
			if (node.binary) {
				return Xapian::Query(to_xapian(node.op), subqueries[0], subqueries[1]);
			}
			return Xapian::Query(to_xapian(node.op), subqueries.begin(), subqueries.end(), node.parameter);

		case QueryKind::append:
			// The accumulated query when the next matches nothing is skipped, and the reverse.
			if (subqueries[0].empty()) {
				return std::move(subqueries[1]);
			}
			return Xapian::Query(to_xapian(node.op), subqueries[0], subqueries[1]);

		default:
			return Xapian::Query(Xapian::Query::OP_SCALE_WEIGHT, subqueries[0], node.factor);
	}
}

}  // namespace


Xapian::Query
to_xapian(const Query& query)
{
	// Post-order on an explicit stack: the call stack does not grow with the depth of the tree.
	struct Frame {
		const QueryNode* node;
		std::size_t cursor;
		std::vector<Xapian::Query> subqueries;
	};
	const auto* root = query.node();
	if (root == nullptr) {
		return Xapian::Query();
	}
	if (!is_inner(*root)) {
		return leaf(*root);
	}
	std::vector<Frame> stack;
	stack.push_back({ root, 0, {} });
	Xapian::Query result;
	while (!stack.empty()) {
		auto& frame = stack.back();
		const auto& node = *frame.node;
		if (frame.cursor < node.subqueries.size()) {
			const auto* child = node.subqueries[frame.cursor++].node();
			if (child == nullptr) {
				frame.subqueries.emplace_back();
			} else if (is_inner(*child)) {
				stack.push_back({ child, 0, {} });
			} else {
				frame.subqueries.push_back(leaf(*child));
			}
			continue;
		}
		auto built = inner(node, frame.subqueries);
		stack.pop_back();
		if (stack.empty()) {
			result = std::move(built);
		} else {
			stack.back().subqueries.push_back(std::move(built));
		}
	}
	return result;
}

}  // namespace engine::xapian
