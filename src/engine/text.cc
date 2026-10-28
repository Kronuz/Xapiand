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

#include "engine/text.h"

#include <algorithm>
#include <cstdint>                 // for std::uint32_t
#include <new>                     // for std::bad_alloc
#include <optional>                // for std::optional
#include <string>                  // for std::string
#include <string_view>             // for std::string_view
#include <unordered_map>           // for std::unordered_map
#include <utility>                 // for std::move
#include <vector>                  // for std::vector

#include "analysis.hh"             // for analysis::stem_strategy
#include "exception_engine.h"        // for QueryParserError, InvalidTextConfigError
#include "queryparser.hh"          // for queryparser::parse
#include "snowball.hh"             // for snowball::stemmer
#include "stopper.h"               // for getStopWords


namespace engine {

namespace {

// The queryparser flags for the text:: flags; WORD_BREAKS has none.
std::uint32_t
parser_flags(unsigned flags)
{
	struct Pair { unsigned engine; std::uint32_t parser; };
	static constexpr Pair pairs[] = {
		{ text::BOOLEAN, queryparser::flag::boolean },
		{ text::PHRASE, queryparser::flag::phrase },
		{ text::LOVEHATE, queryparser::flag::lovehate },
		{ text::WILDCARD, queryparser::flag::wildcard },
		{ text::WILDCARD_SINGLE, queryparser::flag::wildcard_single },
		{ text::WILDCARD_MULTI, queryparser::flag::wildcard_multi },
		{ text::PARTIAL, queryparser::flag::partial },
		{ text::FUZZY, queryparser::flag::fuzzy },
		{ text::CJK_NGRAM, queryparser::flag::cjk_ngram },
	};
	std::uint32_t result = 0;
	for (const auto& pair : pairs) {
		if (flags & pair.engine) {
			result |= pair.parser;
		}
	}
	return result;
}


std::optional<queryparser::kind>
default_op(Op op)
{
	switch (op) {
		case Op::INVALID: return queryparser::kind::or_;
		case Op::AND: return queryparser::kind::and_;
		case Op::OR: return queryparser::kind::or_;
		case Op::XOR: return queryparser::kind::xor_;
		case Op::AND_MAYBE: return queryparser::kind::and_maybe;
		case Op::NEAR: return queryparser::kind::near;
		case Op::PHRASE: return queryparser::kind::phrase;
		case Op::ELITE_SET: return queryparser::kind::elite_set;
		case Op::SYNONYM: return queryparser::kind::synonym;
		case Op::MAX: return queryparser::kind::max;
		default: return std::nullopt;
	}
}


analysis::stem_strategy
stem_strategy(Stem stem)
{
	switch (stem) {
		case Stem::none: return analysis::stem_strategy::none;
		case Stem::some: return analysis::stem_strategy::some;
		case Stem::all: return analysis::stem_strategy::all;
		case Stem::all_z: return analysis::stem_strategy::all_z;
	}
	return analysis::stem_strategy::none;
}


Expansion
expansion_limit(queryparser::expansion_limit limit)
{
	switch (limit) {
		case queryparser::expansion_limit::error: return Expansion::error;
		case queryparser::expansion_limit::first: return Expansion::first;
		case queryparser::expansion_limit::most_frequent: return Expansion::most_frequent;
	}
	return Expansion::error;
}


// One stemmer per language and thread: a snowball::stemmer is not thread-safe.
snowball::stemmer*
stemmer_for(const std::string& language)
{
	thread_local std::unordered_map<std::string, snowball::stemmer> stemmers;
	auto it = stemmers.find(language);
	if (it == stemmers.end()) {
		if (snowball::stemmer::canonical(language).empty()) {
			return nullptr;
		}
		it = stemmers.emplace(language, snowball::stemmer(language)).first;
	}
	return &it->second;
}


// The parser leads a term with the field's name; the seam takes the two apart.
std::pair<std::string, std::string>
split_field(const std::string& field, std::string_view text)
{
	if (!field.empty() && text.starts_with(field)) {
		return { field, std::string(text.substr(field.size())) };
	}
	return { std::string(), std::string(text) };
}


Query
leaf(const queryparser::query& tree, const queryparser::node& n, const std::string& field)
{
	using queryparser::kind;
	switch (n.k) {
		case kind::term:
			{
			auto [name, bytes] = split_field(field, tree.text(n));
			return Query::term(std::move(name), std::move(bytes), n.wqf, n.pos);
		}
		case kind::wildcard: {
			auto [name, pattern] = split_field(field, tree.text(n));
			// Every term has a character: a pattern of '*' (at least one) and at most one '?' matches them all.
			if (name.empty() && pattern.find_first_not_of("*?") == std::string::npos && pattern.find('*') != std::string::npos &&
				std::count(pattern.begin(), pattern.end(), '?') <= 1) {
				return Query::match_all();
			}
			unsigned pattern_flags = 0;
			if (pattern.find('?') != std::string::npos) {
				pattern_flags |= PATTERN_SINGLE;
			}
			if (pattern.find('*') != std::string::npos) {
				pattern_flags |= PATTERN_MULTI;
			}
			return Query::wildcard(std::move(name), std::move(pattern), n.max_expansion, expansion_limit(n.limit), pattern_flags, Op::SYNONYM);
		}
		case kind::edit_distance:
			{
			auto [name, bytes] = split_field(field, tree.text(n));
			const auto prefix_length = name.empty() || n.pos < name.size() ? n.pos : n.pos - name.size();
			return Query::edit_distance(std::move(name), std::move(bytes), n.max_expansion, expansion_limit(n.limit), Op::SYNONYM, n.window, prefix_length);
		}
		default:
			return Query::match_all();
	}
}


bool
is_leaf(queryparser::kind k)
{
	using queryparser::kind;
	return k == kind::term || k == kind::wildcard || k == kind::edit_distance || k == kind::match_all;
}


Op
op_of(queryparser::kind k)
{
	using queryparser::kind;
	switch (k) {
		case kind::and_: return Op::AND;
		case kind::or_: return Op::OR;
		case kind::and_not: return Op::AND_NOT;
		case kind::xor_: return Op::XOR;
		case kind::and_maybe: return Op::AND_MAYBE;
		case kind::near: return Op::NEAR;
		case kind::phrase: return Op::PHRASE;
		case kind::synonym: return Op::SYNONYM;
		case kind::elite_set: return Op::ELITE_SET;
		case kind::max: return Op::MAX;
		default: return Op::INVALID;
	}
}


// The tree as an engine query, bottom-up on an explicit stack: nesting is as deep as the text.
Query
convert(const queryparser::query& tree, const std::string& field)
{
	using queryparser::kind;
	using queryparser::no_node;
	if (tree.empty()) {
		return Query();
	}
	if (is_leaf(tree.at(tree.root()).k)) {
		return leaf(tree, tree.at(tree.root()), field);
	}
	struct Frame {
		queryparser::node_id id;
		queryparser::node_id cursor;
		std::vector<Query> kids;
	};
	std::vector<Frame> stack;
	stack.push_back({ tree.root(), tree.at(tree.root()).first, {} });
	Query result;
	while (!stack.empty()) {
		auto& frame = stack.back();
		if (frame.cursor != no_node) {
			const auto child = frame.cursor;
			const auto& n = tree.at(child);
			frame.cursor = n.next;
			if (is_leaf(n.k)) {
				frame.kids.push_back(leaf(tree, n, field));
			} else {
				if (stack.size() >= MAX_QUERY_DEPTH) {
					// The factories refuse this bottom-up; refusing here skips building the rest.
					throw QueryDepthError(__func__, __FILE__, __LINE__, "QueryDepthError", "Query is nested too deeply: the limit is {} levels", MAX_QUERY_DEPTH);
				}
				stack.push_back({ child, n.first, {} });
			}
			continue;
		}
		const auto& n = tree.at(frame.id);
		Query built;
		if (n.k == kind::scale) {
			built = Query::scale(std::move(frame.kids[0]), n.factor);
		} else {
			const auto op = op_of(n.k);
			const bool windowed = n.k == kind::near || n.k == kind::phrase || n.k == kind::elite_set;
			if (!windowed && frame.kids.size() == 2 && (op == Op::AND || op == Op::OR || op == Op::AND_NOT || op == Op::XOR || op == Op::AND_MAYBE)) {
				built = Query::combine(op, std::move(frame.kids[0]), std::move(frame.kids[1]));
			} else {
				built = Query::combine(op, std::move(frame.kids), windowed ? n.window : 0);
			}
		}
		stack.pop_back();
		if (stack.empty()) {
			result = std::move(built);
		} else {
			stack.back().kids.push_back(std::move(built));
		}
	}
	return result;
}

}  // namespace


Query
parse_text(std::string_view text, const TextConfig& config, std::string* description)
{
	const auto op = default_op(config.default_op);
	if (!op) {
		throw InvalidTextConfigError(__func__, __FILE__, __LINE__, "InvalidTextConfigError", "Unsupported default operator for text: {}", to_string(config.default_op));
	}
	queryparser::config cfg;
	cfg.flags = parser_flags(config.flags);
	cfg.default_op = *op;
	if (!config.stop_language.empty()) {
		cfg.stopper = getStopWords(config.stop_language);
	}
	if (!config.stem_language.empty()) {
		cfg.stemmer = stemmer_for(config.stem_language);
		if (cfg.stemmer == nullptr) {
			throw InvalidTextConfigError(__func__, __FILE__, __LINE__, "InvalidTextConfigError", "Language code {} unknown", config.stem_language);
		}
		cfg.stem = stem_strategy(config.stem);
	} else {
		cfg.stem = analysis::stem_strategy::none;
	}
	cfg.default_prefix = config.field;
	cfg.max_word_length = std::size_t(1) << 20;  // long words are kept; the index side bounds them
	cfg.partial_expansion = { config.partial_max_expansion, queryparser::expansion_limit::most_frequent };
	cfg.fuzzy_expansion = { config.fuzzy_max_expansion, queryparser::expansion_limit::error };

	auto parsed = queryparser::parse(text, cfg);
	if (parsed.err) {
		if (parsed.err->kind == queryparser::error_kind::out_of_memory) {
			throw std::bad_alloc();
		}
		throw QueryParserError(__func__, __FILE__, __LINE__, "QueryParserError", "{}", parsed.err->message);
	}
	if (description != nullptr) {
		*description = parsed.tree.describe();
	}
	return convert(parsed.tree, config.field);
}

}  // namespace engine
