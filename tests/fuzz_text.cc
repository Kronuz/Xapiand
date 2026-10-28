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


// Fuzzes the text parser (engine::parse_text): grammar-aware and byte-mutated query strings under
// random settings. Every text must either raise QueryParserError or lower to an engine query whose
// Xapian description is the one queryparser gives its own tree; a second parse must agree.
//
//   xapiand_fuzz_text SECONDS [SEED]
//
// Seeds its corpus from the vector files' queries. Exits non-zero on the first violation and
// prints the text and settings that caused it. Under -fsanitize it also finds memory errors.

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <new>
#include <string>
#include <vector>

#include "engine/query.h"
#include "engine/text.h"
#include "engine/xapian/query.h"
#include "exception_engine.h"
#include "jsonl.hh"

namespace {

struct Rng {
	std::uint64_t s;
	std::uint64_t next() {
		s += 0x9e3779b97f4a7c15ULL;
		std::uint64_t z = s;
		z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
		z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
		return z ^ (z >> 31);
	}
	std::size_t below(std::size_t n) { return n == 0 ? 0 : static_cast<std::size_t>(next() % n); }
	bool chance(unsigned percent) { return below(100) < percent; }
};

const char* const pieces[] = {
	"quick", "Brown", "fox", "the", "and", "of", "barack", "obama", "Michelle", "running", "dogs", "jumped",
	"a", "b", "x1", "e.g.", "AT&T", "a-b", "1990s", "3.14", "foo_bar", "caf\xc3\xa9", "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e",
	"\xd0\xbf\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82", "AND", "OR", "NOT", "XOR", "NEAR", "ADJ", "NEAR/3", "ADJ/2", "NEAR/0",
	"and", "or", "not", "+", "-", "(", ")", "((", "))", "\"", "\"quick brown\"", "\"a b c\"", "*", "?", "he*", "br?wn", "q*k",
	"~", "~1", "~2", "quick~", "fox~3", "#", "c++", "a+", "field:", "t_some:", ":", ",", ".", "\\", "/", "'", "\t", "\n",
	"\xe2\x80\x9c", "\xe2\x80\x9d", "\xef\xbc\x88", "\xef\xbc\x89", "\xc3\x28", "\xff", "$", "&", "@", "{", "}", "[", "]", "<", ">", "=",
};

std::string
random_text(Rng& rng, const std::vector<std::string>& corpus)
{
	std::string text;
	switch (rng.below(4)) {
		case 0:
			if (!corpus.empty()) {
				text = corpus[rng.below(corpus.size())];
				break;
			}
			[[fallthrough]];
		case 1:
		case 2: {
			const auto n = 1 + rng.below(14);
			for (std::size_t i = 0; i < n; ++i) {
				text += pieces[rng.below(sizeof(pieces) / sizeof(*pieces))];
				if (rng.chance(80)) {
					text += ' ';
				}
			}
			break;
		}
		default: {
			const auto n = rng.below(40);
			for (std::size_t i = 0; i < n; ++i) {
				text += static_cast<char>(rng.below(256));
			}
		}
	}
	for (auto edits = rng.below(4); edits > 0; --edits) {
		const auto at = rng.below(text.size() + 1);
		switch (rng.below(4)) {
			case 0: text.insert(at, pieces[rng.below(sizeof(pieces) / sizeof(*pieces))]); break;
			case 1: if (at < text.size()) { text.erase(at, 1 + rng.below(3)); } break;
			case 2: if (at < text.size()) { text[at] = static_cast<char>(rng.below(256)); } break;
			default: text += text.substr(rng.below(text.size() + 1)); break;
		}
		if (text.size() > 4000) {
			text.resize(4000);
		}
	}
	return text;
}

engine::TextConfig
random_config(Rng& rng)
{
	static const unsigned bits[] = {
		engine::text::BOOLEAN, engine::text::PHRASE, engine::text::LOVEHATE, engine::text::WILDCARD, engine::text::WILDCARD_SINGLE,
		engine::text::WILDCARD_MULTI, engine::text::PARTIAL, engine::text::FUZZY, engine::text::CJK_NGRAM, engine::text::WORD_BREAKS,
	};
	static const engine::Op ops[] = {
		engine::Op::INVALID, engine::Op::AND, engine::Op::OR, engine::Op::XOR, engine::Op::AND_MAYBE, engine::Op::NEAR,
		engine::Op::PHRASE, engine::Op::ELITE_SET, engine::Op::SYNONYM, engine::Op::MAX,
	};
	engine::TextConfig c;
	c.prefix = rng.chance(20) ? "" : (rng.chance(50) ? "XTfield" : "t_some.S");
	c.default_op = ops[rng.below(sizeof(ops) / sizeof(*ops))];
	for (auto bit : bits) {
		if (rng.chance(55)) {
			c.flags |= bit;
		}
	}
	if (rng.chance(60)) {
		c.stop_language = rng.chance(85) ? "en" : "es";
	}
	if (rng.chance(60)) {
		c.stem_language = rng.chance(85) ? "en" : "fr";
		c.stem = static_cast<engine::Stem>(rng.below(4));
	}
	c.partial_max_expansion = static_cast<engine::termcount>(rng.below(3) * 25);
	c.fuzzy_max_expansion = static_cast<engine::termcount>(rng.below(3));
	return c;
}

std::string
show(const std::string& text)
{
	std::string out;
	for (unsigned char ch : text) {
		char buf[8];
		if (ch >= 0x20 && ch < 0x7f && ch != '\\') {
			out += static_cast<char>(ch);
		} else {
			std::snprintf(buf, sizeof(buf), "\\x%02x", ch);
			out += buf;
		}
	}
	return out;
}

// A wildcard of '*' (at least one) and at most one '?' matches every term: the engine query has match-all.
std::string
match_all_wildcards(std::string tree)
{
	static const std::string head = "WILDCARD SYNONYM ";
	for (std::size_t at = 0; (at = tree.find(head, at)) != std::string::npos; ++at) {
		const auto from = at + head.size();
		auto to = tree.find_first_not_of("*?", from);
		if (to == std::string::npos) {
			to = tree.size();
		}
		const auto pattern = tree.substr(from, to - from);
		if ((to == tree.size() || tree[to] == ' ' || tree[to] == ')') && pattern.find('*') != std::string::npos &&
			std::count(pattern.begin(), pattern.end(), '?') <= 1) {
			tree.replace(at, to - at, "<alldocuments>");
		}
	}
	return tree;
}

// Parses once: the description of the lowered query, an error marker, or the tree's own description.
struct Reading {
	std::string lowered;
	std::string tree;
	std::string error;
};

Reading
reading(const std::string& text, const engine::TextConfig& c)
{
	Reading r;
	try {
		const auto query = engine::parse_text(text, c, &r.tree);
		r.lowered = engine::xapian::to_xapian(query).get_description();
		r.tree = match_all_wildcards(r.tree);
	} catch (const QueryParserError& exc) {
		r.error = "syntax: " + exc.reason();
	} catch (const InvalidTextConfigError& exc) {
		r.error = "invalid: " + exc.reason();
	} catch (const std::bad_alloc&) {
		r.error = "bad_alloc";
	}
	return r;
}

}  // namespace


int
main(int argc, char** argv)
{
	::setenv("XAPIAND_DATA_PATH", XAPIAND_SOURCE_DIR, 1);
	const double seconds = argc > 1 ? std::atof(argv[1]) : 10;
	Rng rng{argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 1};

	std::vector<std::string> corpus;
#ifdef QUERYPARSER_VECTORS
	for (const char* file : { "parse-ab.jsonl", "parse-v2.jsonl", "parse-v3.jsonl" }) {
		std::ifstream in(std::string(QUERYPARSER_VECTORS) + "/" + file);
		for (std::string line; std::getline(in, line);) {
			corpus.push_back(jsonl::row(line)["query"]);
		}
	}
#endif

	const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(static_cast<long>(seconds * 1000));
	std::size_t runs = 0, parsed = 0, rejected = 0;
	while (std::chrono::steady_clock::now() < end) {
		for (int i = 0; i < 200; ++i, ++runs) {
			const auto text = random_text(rng, corpus);
			const auto c = random_config(rng);
			const auto first = reading(text, c);
			const auto second = reading(text, c);
			bool bad = first.lowered != second.lowered || first.error != second.error;
			if (first.error.empty()) {
				++parsed;
				bad = bad || first.lowered != first.tree;
			} else {
				++rejected;
			}
			if (bad) {
				std::fprintf(stderr, "FAIL seed=%s text=\"%s\" flags=%u op=%s stop=%s stem=%s/%d prefix=%s\n  lowered: %s\n  tree:    %s\n  error:   %s\n",
					argc > 2 ? argv[2] : "1", show(text).c_str(), c.flags, engine::to_string(c.default_op), c.stop_language.c_str(),
					c.stem_language.c_str(), static_cast<int>(c.stem), c.prefix.c_str(), show(first.lowered).c_str(), show(first.tree).c_str(), first.error.c_str());
				return 1;
			}
		}
	}

	// A text as deep as it is long, in each bracket form: parsed and lowered without growing the stack.
	for (const char* open : { "(", "a AND (", "a OR (", "+(", "-(", "\"", "NOT (", "((a ", "x NEAR (" }) {
		std::string deep;
		for (int i = 0; i < 100000; ++i) {
			deep += open;
		}
		deep += "z";
		for (int i = 0; i < 100000; ++i) {
			deep += ")";
		}
		auto c = random_config(rng);
		c.flags |= engine::text::BOOLEAN;
		try {
			engine::parse_text(deep, c);
		} catch (const QueryParserError&) {
		} catch (const InvalidTextConfigError&) {
		}
	}

	std::printf("fuzz_text: %zu texts, %zu parsed, %zu rejected, no violation\n", runs, parsed, rejected);
	return 0;
}
