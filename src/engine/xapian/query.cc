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

#include "engine/xapian/query.h"

#include <utility>                 // for std::move
#include <vector>                  // for std::vector

#include "engine/callbacks.h"      // for PostingSource
#include "engine/xapian/callbacks.h"  // for PostingSourceAdapter
#include "snowball_stem.h"         // for makeStem
#include "stopper.h"               // for getStopper


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


unsigned
parser_flags(unsigned flags)
{
	struct Pair { unsigned engine; unsigned xapian; };
	static constexpr Pair pairs[] = {
		{ text::BOOLEAN, Xapian::QueryParser::FLAG_BOOLEAN },
		{ text::PHRASE, Xapian::QueryParser::FLAG_PHRASE },
		{ text::LOVEHATE, Xapian::QueryParser::FLAG_LOVEHATE },
		{ text::WILDCARD, Xapian::QueryParser::FLAG_WILDCARD },
		{ text::WILDCARD_SINGLE, Xapian::QueryParser::FLAG_WILDCARD_SINGLE },
		{ text::WILDCARD_MULTI, Xapian::QueryParser::FLAG_WILDCARD_MULTI },
		{ text::PARTIAL, Xapian::QueryParser::FLAG_PARTIAL },
		{ text::FUZZY, Xapian::QueryParser::FLAG_FUZZY },
		{ text::CJK_NGRAM, Xapian::QueryParser::FLAG_CJK_NGRAM },
		{ text::WORD_BREAKS, Xapian::QueryParser::FLAG_WORD_BREAKS },
	};
	unsigned result = 0;
	for (const auto& pair : pairs) {
		if (flags & pair.engine) {
			result |= pair.xapian;
		}
	}
	return result;
}


Xapian::QueryParser::stem_strategy
stem_strategy(Stem stem)
{
	switch (stem) {
		case Stem::none: return Xapian::QueryParser::STEM_NONE;
		case Stem::some: return Xapian::QueryParser::STEM_SOME;
		case Stem::all: return Xapian::QueryParser::STEM_ALL;
		case Stem::all_z: return Xapian::QueryParser::STEM_ALL_Z;
	}
	return Xapian::QueryParser::STEM_NONE;
}


Xapian::Query
parse_text(const QueryNode& node)
{
	const auto& config = node.config;
	Xapian::QueryParser parser;
	if (config.default_op != Op::INVALID) {
		parser.set_default_op(to_xapian(config.default_op));
	}
	if (!config.stop_language.empty()) {
		parser.set_stopper(getStopper(config.stop_language).get());
	}
	if (!config.stem_language.empty()) {
		parser.set_stemmer(makeStem(config.stem_language));
		parser.set_stemming_strategy(stem_strategy(config.stem));
	}
	parser.set_max_expansion(config.partial_max_expansion, Xapian::Query::WILDCARD_LIMIT_MOST_FREQUENT, Xapian::QueryParser::FLAG_PARTIAL);
	parser.set_max_expansion(config.fuzzy_max_expansion, Xapian::Query::WILDCARD_LIMIT_ERROR, Xapian::QueryParser::FLAG_FUZZY);
	return parser.parse_query(node.str, parser_flags(config.flags), config.prefix);
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


Xapian::Query
to_xapian(const Query& query)
{
	const auto* node = query.node();
	if (node == nullptr) {
		return Xapian::Query();
	}

	switch (node->kind) {
		case QueryKind::term:
			return Xapian::Query(node->str, node->wqf, node->pos);

		case QueryKind::combine: {
			if (node->binary) {
				return Xapian::Query(to_xapian(node->op), to_xapian(node->subqueries[0]), to_xapian(node->subqueries[1]));
			}
			std::vector<Xapian::Query> subqueries;
			subqueries.reserve(node->subqueries.size());
			for (const auto& sub : node->subqueries) {
				subqueries.push_back(to_xapian(sub));
			}
			return Xapian::Query(to_xapian(node->op), subqueries.begin(), subqueries.end(), node->parameter);
		}

		case QueryKind::append: {
			// Folds a chain of appends left to right, so a long chain costs no stack.
			std::vector<const QueryNode*> chain;
			const Query* base = &query;
			while (base->node() != nullptr && base->node()->kind == QueryKind::append) {
				chain.push_back(base->node());
				base = &base->node()->subqueries[0];
			}
			auto accumulated = to_xapian(*base);
			for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
				auto next = to_xapian((*it)->subqueries[1]);
				if (accumulated.empty()) {
					accumulated = std::move(next);
				} else {
					accumulated = Xapian::Query(to_xapian((*it)->op), accumulated, next);
				}
			}
			return accumulated;
		}

		case QueryKind::scale:
			return Xapian::Query(Xapian::Query::OP_SCALE_WEIGHT, to_xapian(node->subqueries[0]), node->factor);

		case QueryKind::wildcard:
			return Xapian::Query(Xapian::Query::OP_WILDCARD, node->str, node->parameter, wildcard_flags(node->limit, node->pattern_flags), to_xapian(node->op));

		case QueryKind::edit_distance:
			return Xapian::Query(Xapian::Query::OP_EDIT_DISTANCE, node->str, node->parameter, wildcard_flags(node->limit, 0), to_xapian(node->op), node->distance, node->prefix_length);

		case QueryKind::source: {
			// release() hands the adapter to the Query, which owns it.
			auto* adapter = new PostingSourceAdapter(node->src->clone());
			return Xapian::Query(adapter->release());
		}

		case QueryKind::text:
			return parse_text(*node);
	}
	return Xapian::Query();
}

}  // namespace engine::xapian
