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

#include "engine/callbacks.h"      // for PostingSource


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

}  // namespace


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


Query
Query::text(std::string text, TextConfig config)
{
	auto node = std::make_shared<QueryNode>();
	node->kind = QueryKind::text;
	node->str = std::move(text);
	node->config = std::move(config);
	return Query(std::move(node));
}


bool
Query::empty() const noexcept
{
	return !node_ || node_->matches_nothing;
}


std::string
Query::description() const
{
	if (!node_) {
		return "<nothing>";
	}
	const auto& n = *node_;
	switch (n.kind) {
		case QueryKind::term:
			return n.str.empty() ? "<all>" : n.str;
		case QueryKind::combine:
		case QueryKind::append: {
			const char* name = to_string(n.op);
			std::string result("(");
			for (std::size_t i = 0; i < n.subqueries.size(); ++i) {
				if (i != 0) {
					result.append(" ").append(name).append(" ");
				}
				result.append(n.subqueries[i].description());
			}
			if (n.parameter != 0) {
				result.append(" ").append(std::to_string(n.parameter));
			}
			return result.append(")");
		}
		case QueryKind::scale:
			return "(" + n.subqueries.front().description() + " SCALE " + std::to_string(n.factor) + ")";
		case QueryKind::wildcard:
			return "wildcard(" + n.str + ")";
		case QueryKind::edit_distance:
			return "edit_distance(" + n.str + ", " + std::to_string(n.distance) + ")";
		case QueryKind::source:
			return n.src->description();
		case QueryKind::text:
			return "text(" + n.config.prefix + ": " + n.str + ")";
	}
	return "?";
}

}  // namespace engine
