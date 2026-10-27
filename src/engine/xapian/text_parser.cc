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

#include "engine/xapian/text_parser.h"

#include <cstdio>                  // for std::fputs
#include <cstdlib>                 // for std::getenv
#include <cstring>                 // for std::strcmp
#include <new>                     // for std::bad_alloc
#include <exception>               // for std::exception
#include <unordered_map>           // for std::unordered_map
#include <utility>                 // for std::move
#include <vector>                  // for std::vector

#include "analysis.hh"             // for analysis::stem_strategy
#include "engine/xapian/query.h"   // for to_xapian
#include "queryparser.hh"          // for queryparser::parse
#include "snowball.hh"             // for snowball::stemmer
#include "stopper.h"               // for getStopWords


namespace engine::xapian {

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


Query
leaf(const queryparser::query& tree, const queryparser::node& n)
{
	using queryparser::kind;
	switch (n.k) {
		case kind::term:
			return Query::term(std::string(tree.text(n)), n.wqf, n.pos);
		case kind::wildcard: {
			auto pattern = std::string(tree.text(n));
			unsigned pattern_flags = 0;
			if (pattern.find('?') != std::string::npos) {
				pattern_flags |= PATTERN_SINGLE;
			}
			if (pattern.find('*') != std::string::npos) {
				pattern_flags |= PATTERN_MULTI;
			}
			return Query::wildcard(std::move(pattern), n.max_expansion, expansion_limit(n.limit), pattern_flags, Op::SYNONYM);
		}
		case kind::edit_distance:
			return Query::edit_distance(std::string(tree.text(n)), n.max_expansion, expansion_limit(n.limit), Op::SYNONYM, n.window, n.pos);
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
convert(const queryparser::query& tree)
{
	using queryparser::kind;
	using queryparser::no_node;
	if (tree.empty()) {
		return Query();
	}
	if (is_leaf(tree.at(tree.root()).k)) {
		return leaf(tree, tree.at(tree.root()));
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
				frame.kids.push_back(leaf(tree, n));
			} else {
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


bool
text_parser_enabled()
{
	static const bool enabled = [] {
		const char* value = std::getenv("XAPIAND_QUERYPARSER");
		return value != nullptr && std::strcmp(value, "1") == 0;
	}();
	return enabled;
}


std::optional<ParsedText>
parse_text_queryparser(std::string_view query_text, const TextConfig& config)
{
	if (config.flags & text::WORD_BREAKS) {
		return std::nullopt;
	}
	const auto op = default_op(config.default_op);
	if (!op) {
		return std::nullopt;
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
			return std::nullopt;
		}
		cfg.stem = stem_strategy(config.stem);
	} else {
		cfg.stem = analysis::stem_strategy::none;
	}
	cfg.default_prefix = config.prefix;
	cfg.max_word_length = std::size_t(1) << 20;  // Xapian keeps long words; the index side bounds them
	cfg.partial_expansion = { config.partial_max_expansion, queryparser::expansion_limit::most_frequent };
	cfg.fuzzy_expansion = { config.fuzzy_max_expansion, queryparser::expansion_limit::error };

	auto parsed = queryparser::parse(query_text, cfg);
	ParsedText result;
	if (parsed.err) {
		result.failed = true;
		result.unimplemented = parsed.err->kind == queryparser::error_kind::unsupported;
		result.out_of_memory = parsed.err->kind == queryparser::error_kind::out_of_memory;
		result.error = std::move(parsed.err->message);
		return result;
	}
#ifdef XAPIAND_QP_AB
	result.description = parsed.tree.describe();
#endif
	result.query = convert(parsed.tree);
	return result;
}


void
throw_text_error(const ParsedText& parsed)
{
	if (parsed.out_of_memory) {
		throw std::bad_alloc();
	}
	if (parsed.unimplemented) {
		throw Xapian::UnimplementedError(parsed.error);
	}
	throw Xapian::QueryParserError(parsed.error);
}


#ifdef XAPIAND_QP_AB

namespace {

void
json_string(std::string& out, std::string_view s)
{
	out += '"';
	for (unsigned char c : s) {
		switch (c) {
			case '"': out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			case '\n': out += "\\n"; break;
			case '\r': out += "\\r"; break;
			case '\t': out += "\\t"; break;
			default:
				if (c < 0x20 || c == 0x7f) {
					static const char hex[] = "0123456789abcdef";
					out += "\\u00";
					out += hex[c >> 4];
					out += hex[c & 15];
				} else {
					out += static_cast<char>(c);
				}
		}
	}
	out += '"';
}

}  // namespace


void
text_parser_compare(std::string_view query_text, const TextConfig& config, const Xapian::Query* xapian, const std::string* xapian_error)
{
	try {
		const char* verdict = "ok";
		std::string want, got, adapted;
		const auto parsed = parse_text_queryparser(query_text, config);
		if (!parsed) {
			verdict = "skip";
		} else {
			want = xapian_error != nullptr ? "ERROR " + *xapian_error : xapian->get_description();
			if (parsed->failed) {
				got = std::string("ERROR ") + (parsed->unimplemented ? "UnimplementedError: " : "QueryParserError: ") + parsed->error;
				adapted = got;
			} else {
				got = parsed->description;
				adapted = to_xapian(parsed->query).get_description();
			}
			if (got != want) {
				verdict = adapted == want ? "describe" : "parser";
			} else if (adapted != want) {
				verdict = "adapter";
			}
		}
		std::string line = "QPAB {\"r\":\"";
		line += verdict;
		line += "\",\"q\":";
		json_string(line, query_text);
		line += ",\"field\":";
		json_string(line, config.prefix);
		line += ",\"flags\":" + std::to_string(config.flags) + ",\"op\":\"" + to_string(config.default_op) + "\",\"stem\":" + std::to_string(static_cast<int>(config.stem)) + ",\"stem_language\":";
		json_string(line, config.stem_language);
		line += ",\"stop_language\":";
		json_string(line, config.stop_language);
		if (std::strcmp(verdict, "ok") != 0) {
			line += ",\"xapian\":";
			json_string(line, want);
			line += ",\"queryparser\":";
			json_string(line, got);
			line += ",\"adapted\":";
			json_string(line, adapted);
		}
		line += "}\n";
		std::fputs(line.c_str(), stderr);
	} catch (const std::exception&) {
		std::fputs("QPAB {\"r\":\"exception\"}\n", stderr);
	}
}

#endif

}  // namespace engine::xapian
