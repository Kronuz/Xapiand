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

#pragma once

// What the pieces of the callback tests share: the corpus, its model, and the helpers that run a search.

#include "../test_harness.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <set>
#include <sstream>
#include <tuple>
#include <string>
#include <vector>

#include "engine/callbacks.h"
#include "engine/error.h"
#include "engine/open.h"
#include "engine/search.h"
#include "engine/sync.h"

#include "callbacks.h"

namespace detail {

using engine::Op;
using engine::Query;

constexpr double tolerance = 1e-9;

// --- the corpus ---

struct Index {
	std::vector<std::unique_ptr<engine::Shard>> owned;

	std::vector<engine::Shard*> shards(std::size_t n) const {
		std::vector<engine::Shard*> list;
		for (std::size_t i = 0; i < n && i < owned.size(); ++i) {
			list.push_back(owned[i].get());
		}
		return list;
	}
};

// The corpus's words; defined in tests/test_engine_callbacks.cc, since its initializer is dynamic.
extern const std::vector<std::string> words;

inline std::string pad(unsigned n, int width) {
	char buffer[32];
	std::snprintf(buffer, sizeof buffer, "%0*u", width, n);
	return buffer;
}

inline std::string path_of(const char* name, int shard) {
	const auto path = (std::filesystem::current_path() / (std::string("engine_callbacks_") + name + "_" + std::to_string(shard) + ".db")).string();
	std::filesystem::remove_all(path);
	return path;
}

struct Spec {
	std::map<std::string, unsigned> terms;
	std::string key, group, number, data;
	bool has_number = true;
	bool live = true;
};

inline Spec random_spec(std::mt19937& rng, int shard, int i) {
	Spec spec;
	const int k = 1 + static_cast<int>(rng() % 4);
	for (int n = 0; n < k; ++n) {
		spec.terms[words[rng() % words.size()]] += 1 + rng() % 3;
	}
	spec.terms["common"] = 1;
	spec.terms["tag" + std::to_string(i % 5)] = 1 + i % 3;
	if (i % 2 == 0) {
		spec.terms["even"] = 2;
	}
	spec.key = pad(rng() % 1000000, 6) + "-" + std::to_string(shard) + "-" + std::to_string(i);
	spec.group = "g" + std::to_string(rng() % 4);
	spec.has_number = i % 7 != 0;
	spec.number = pad(rng() % 1000, 5);
	spec.data = "d" + std::to_string(shard) + "_" + std::to_string(i);
	return spec;
}

inline std::string key_of(int shard, int i) {
	return "Q" + std::to_string(shard) + "_" + std::to_string(i);
}

inline engine::Document make(int shard, int i, const Spec& spec) {
	engine::Document doc;
	doc.set_key(key_of(shard, i));
	const auto flat = doc.field("f");   // the terms are flat strings; no field name is joined to them
	for (const auto& [term, wdf] : spec.terms) {
		doc.add_term(flat, term, wdf);
	}
	doc.add_boolean_term(flat, "B" + spec.group);   // a term without frequency, as the fields of a schema make
	doc.add_value(KEY, spec.key);
	doc.add_value(GROUP, spec.group);
	if (spec.has_number) {
		doc.add_value(NUMBER, spec.number);
	}
	doc.set_data(spec.data);
	return doc;
}

struct Corpus {
	Index strata;
	Index merged;   // every document of every shard in one shard: the whole-index statistics
	std::vector<std::vector<Spec>> specs;
};

// The documents on `shard_count` shards, `commits` commits per shard (several segments), and all of them in one more shard.
inline void build(const char* name, int shard_count, int count, int commits, Corpus& corpus) {
	std::mt19937 rng(4321);
	corpus.specs.assign(static_cast<std::size_t>(shard_count), {});
	corpus.strata.owned.clear();
	for (int s = 0; s < shard_count; ++s) {
		corpus.strata.owned.push_back(engine::sync_wait(engine::open_local(path_of(name, s), true, true)).shard);
	}
	corpus.merged.owned.clear();
	corpus.merged.owned.push_back(engine::sync_wait(engine::open_local(path_of(name, 99), true, true)).shard);
	for (int s = 0; s < shard_count; ++s) {
		auto& list = corpus.specs[static_cast<std::size_t>(s)];
		for (int c = 0; c < commits; ++c) {
			for (int n = 0; n < count / commits; ++n) {
				const int i = static_cast<int>(list.size()) + 1;
				list.push_back(random_spec(rng, s, i));
				// Shard 0 also holds a few terms of known, distinct frequencies.
				if (s == 0) {
					for (const auto& [term, upto] : {std::pair{"mf_a", 5}, {"mf_b", 3}, {"mf_c", 8}, {"mf_d", 1}}) {
						if (i <= upto) {
							list.back().terms[term] = 1;
						}
					}
				}
				for (auto* index : {&corpus.strata, &corpus.merged}) {
					auto doc = make(s, i, list.back());
					engine::sync_wait(index->owned[index == &corpus.merged ? 0 : static_cast<std::size_t>(s)]->add(doc, {}));
				}
			}
			engine::sync_wait(corpus.strata.owned[static_cast<std::size_t>(s)]->commit(engine::Transaction::none));
		}
	}
	engine::sync_wait(corpus.merged.owned[0]->commit(engine::Transaction::none));
}

// --- the model: which documents a query matches ---

inline bool has_term(const Spec& spec, const std::string& term) {
	return spec.terms.contains(term) || term == "B" + spec.group;
}

// The terms of the field "f" over the live documents of the first `shards` shards, with the number of documents holding each.
using Dictionary = std::map<std::string, std::size_t>;

inline Dictionary dictionary_of(const Corpus& corpus, std::size_t shards) {
	Dictionary dictionary;
	for (std::size_t s = 0; s < shards; ++s) {
		for (const auto& spec : corpus.specs[s]) {
			if (!spec.live) { continue; }
			for (const auto& [term, wdf] : spec.terms) { ++dictionary[term]; }
			++dictionary["B" + spec.group];
		}
	}
	return dictionary;
}

inline std::vector<char32_t> code_points(const std::string& text) {
	std::vector<char32_t> out;
	for (std::size_t i = 0; i < text.size();) {
		const auto lead = static_cast<unsigned char>(text[i]);
		const std::size_t n = lead < 0x80 ? 1 : lead < 0xe0 ? 2 : lead < 0xf0 ? 3 : 4;
		char32_t cp = n == 1 ? lead : lead & (0xff >> (n + 1));
		for (std::size_t k = 1; k < n; ++k) { cp = (cp << 6) | (static_cast<unsigned char>(text[i + k]) & 0x3f); }
		out.push_back(cp);
		i += n;
	}
	return out;
}

// Whether the term matches a pattern whose '?' (one character) and '*' (any run) count as the flags say, whole.
inline bool glob(const std::vector<char32_t>& pattern, std::size_t p, const std::vector<char32_t>& term, std::size_t t, unsigned flags) {
	if (p == pattern.size()) { return t == term.size(); }
	if (pattern[p] == U'*' && (flags & engine::PATTERN_MULTI)) {
		for (std::size_t skip = t; skip <= term.size(); ++skip) {
			if (glob(pattern, p + 1, term, skip, flags)) { return true; }
		}
		return false;
	}
	if (t == term.size()) { return false; }
	if (pattern[p] == U'?' && (flags & engine::PATTERN_SINGLE)) { return glob(pattern, p + 1, term, t + 1, flags); }
	return pattern[p] == term[t] && glob(pattern, p + 1, term, t + 1, flags);
}

// The optimal-string-alignment distance: insertions, deletions, substitutions and adjacent swaps count one.
inline std::size_t distance_of(const std::vector<char32_t>& a, const std::vector<char32_t>& b) {
	std::vector<std::vector<std::size_t>> d(a.size() + 1, std::vector<std::size_t>(b.size() + 1));
	for (std::size_t i = 0; i <= a.size(); ++i) { d[i][0] = i; }
	for (std::size_t j = 0; j <= b.size(); ++j) { d[0][j] = j; }
	for (std::size_t i = 1; i <= a.size(); ++i) {
		for (std::size_t j = 1; j <= b.size(); ++j) {
			d[i][j] = std::min({d[i - 1][j] + 1, d[i][j - 1] + 1, d[i - 1][j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1)});
			if (i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1]) { d[i][j] = std::min(d[i][j], d[i - 2][j - 2] + 1); }
		}
	}
	return d[a.size()][b.size()];
}

// The terms a wildcard or an edit-distance node stands for, within its limit; none and `error` set when
// there are more than the limit allows under the `error` policy.
struct Expansion {
	std::set<std::string> terms;
	bool error = false;
};

inline Expansion expand(const engine::QueryNode& node, const Dictionary& dictionary) {
	std::vector<std::string> found;
	for (const auto& [term, count] : dictionary) {
		bool match;
		if (node.kind == engine::QueryKind::wildcard) {
			match = node.pattern_flags == 0 ? term.starts_with(node.str) : glob(code_points(node.str), 0, code_points(term), 0, node.pattern_flags);
		} else {
			const auto prefix = node.str.substr(0, node.prefix_length);
			match = term.starts_with(prefix) && distance_of(code_points(node.str.substr(prefix.size())), code_points(term.substr(prefix.size()))) <= node.distance;
		}
		if (match) { found.push_back(term); }
	}
	Expansion result;
	if (node.parameter != 0 && found.size() > node.parameter) {
		switch (node.limit) {
			case engine::Expansion::error:
				result.error = true;
				return result;
			case engine::Expansion::first:
				found.resize(node.parameter);
				break;
			case engine::Expansion::most_frequent:
				std::stable_sort(found.begin(), found.end(), [&](const auto& x, const auto& y) { return dictionary.at(x) > dictionary.at(y); });
				found.resize(node.parameter);
				break;
		}
	}
	result.terms.insert(found.begin(), found.end());
	return result;
}

struct Matcher {
	const Dictionary& dictionary;
	const std::string& key;
	const Spec& spec;
	bool errored = false;

	bool value_accepted(const engine::PostingSource& source) const {
		std::string value;
		if (source.slot() == KEY) { value = spec.key; }
		else if (source.slot() == GROUP) { value = spec.group; }
		else if (source.slot() == NUMBER && spec.has_number) { value = spec.number; }
		return !value.empty() && source.accept(value);
	}

	bool operator()(const Query& query) {
		const auto* node = query.node();
		if (node == nullptr) { return false; }
		switch (node->kind) {
			case engine::QueryKind::term:
				if (node->field.empty() && node->str.empty()) { return true; }
				if (node->field == engine::KEY_FIELD) { return node->str == key; }
				return node->field == "f" && has_term(spec, node->str);
			case engine::QueryKind::wildcard:
			case engine::QueryKind::edit_distance: {
				const auto expansion = expand(*node, dictionary);
				errored = errored || expansion.error;
				return std::any_of(expansion.terms.begin(), expansion.terms.end(), [&](const auto& term) { return has_term(spec, term); });
			}
			case engine::QueryKind::source:
				return value_accepted(*node->src);
			case engine::QueryKind::scale:
				return (*this)(node->subqueries[0]);
			case engine::QueryKind::append:
				if (node->subqueries[0].empty()) { return (*this)(node->subqueries[1]); }
				return combine(node->op, node->subqueries);
			case engine::QueryKind::combine:
				return combine(node->op, node->subqueries);
		}
		return false;
	}

	bool combine(Op op, const std::vector<Query>& subs) {
		std::vector<bool> hit;
		for (const auto& sub : subs) { hit.push_back((*this)(sub)); }
		const auto count = static_cast<std::size_t>(std::count(hit.begin(), hit.end(), true));
		switch (op) {
			case Op::AND: return count == hit.size();
			case Op::OR:
			case Op::MAX:
			case Op::SYNONYM: return count != 0;
			case Op::AND_NOT: return hit[0] && count == 1;
			case Op::AND_MAYBE: return hit[0];
			case Op::FILTER: return hit[0] && hit[1];
			case Op::XOR: return count % 2 == 1;
			default: return false;
		}
	}
};

// The stored data of the live documents of the first `shards` shards that the query matches; `error` when an
// expansion past its limit under the `error` policy fails the search.
struct Expected {
	std::set<std::string> datas;
	bool error = false;
};

inline Expected expected_of(const Corpus& corpus, std::size_t shards, const Query& query) {
	const auto dictionary = dictionary_of(corpus, shards);
	Expected expected;
	for (std::size_t s = 0; s < shards; ++s) {
		for (std::size_t i = 0; i < corpus.specs[s].size(); ++i) {
			const auto& spec = corpus.specs[s][i];
			if (!spec.live) { continue; }
			const auto key = key_of(static_cast<int>(s), static_cast<int>(i + 1));
			Matcher matcher{dictionary, key, spec};
			if (matcher(query)) { expected.datas.insert(spec.data); }
			expected.error = expected.error || matcher.errored;
		}
	}
	return expected;
}

// --- running and comparing ---

using Tweak = std::function<void(engine::SearchRequest&)>;

inline engine::SearchResult run(const Index& index, std::size_t shards, const Query& query, const Tweak& tweak = [](engine::SearchRequest&) { }) {
	auto prepared = engine::prepare(query);
	engine::SearchRequest request;
	request.query = prepared.get();
	request.maxitems = 100000;
	request.check_at_least = 100000;
	tweak(request);
	return engine::sync_wait(engine::search(index.shards(shards), request));
}

inline bool close(double a, double b) {
	return std::fabs(a - b) <= tolerance * std::max(1.0, std::fabs(b));
}

// The stored data of a document of the search over `shards` shards of the index.
inline std::string data_of(const Index& index, std::size_t shards, engine::docid did) {
	const std::size_t shard = (did - 1) % shards;
	const auto local = static_cast<engine::docid>((did - 1) / shards + 1);
	return engine::sync_wait(index.owned[shard]->get(local))->data();
}

// The documents' weights by stored data, for comparing indexes that number their documents differently.
inline std::map<std::string, double> weights_by_data(const Index& index, std::size_t shards, const engine::SearchResult& result) {
	std::map<std::string, double> weights;
	for (const auto& hit : result.hits) {
		weights[data_of(index, shards, hit.did)] = hit.weight;
	}
	return weights;
}

inline std::set<std::string> datas_of(const Index& index, std::size_t shards, const engine::SearchResult& result) {
	std::set<std::string> datas;
	for (const auto& [data, weight] : weights_by_data(index, shards, result)) { datas.insert(data); }
	return datas;
}

// The ranks and the order of the hits of a full search: weight order, ranks counting from zero.
inline void check_ranking(const engine::SearchResult& result) {
	for (std::size_t i = 0; i < result.hits.size(); ++i) {
		CHECK_EQ(result.hits[i].rank, i);
		if (i > 0) { CHECK_GE(result.hits[i - 1].weight + 1e-12, result.hits[i].weight); }
	}
}

// A search checked against the model: the documents and the estimate are the model's, the ranking is by
// weight, and (when `weights`) the weights are those of one shard holding every document. With a window
// the hits are that slice of the full ranking.
inline engine::SearchResult both(const Corpus& corpus, std::size_t shards, const Query& query, const char* what, const Tweak& tweak = [](engine::SearchRequest&) { }, bool weights = true) {
	const auto want = expected_of(corpus, shards, query);
	const auto full = run(corpus.strata, shards, query);
	const bool ok = datas_of(corpus.strata, shards, full) == want.datas && full.estimated == want.datas.size() && full.hits.size() == want.datas.size();
	if (!ok) {
		std::fprintf(stderr, "  differs: %s (%zu shards): %zu hits est %u, model %zu\n", what, shards, full.hits.size(), full.estimated, want.datas.size());
	}
	CHECK_TRUE(ok);
	check_ranking(full);
	if (weights && shards == corpus.specs.size()) {
		const auto whole = run(corpus.merged, 1, query);
		const auto mine = weights_by_data(corpus.strata, shards, full), theirs = weights_by_data(corpus.merged, 1, whole);
		bool same = mine.size() == theirs.size();
		for (const auto& [data, weight] : mine) {
			const auto it = theirs.find(data);
			same = same && it != theirs.end() && close(weight, it->second);
		}
		if (!same) { std::fprintf(stderr, "  weights differ from the whole index's: %s (%zu shards)\n", what, shards); }
		CHECK_TRUE(same);
	}
	engine::SearchRequest window;
	window.maxitems = 100000;   // as run() starts every request
	tweak(window);
	const auto windowed = run(corpus.strata, shards, query, tweak);
	CHECK_EQ(windowed.estimated, full.estimated);
	const std::size_t begin = std::min<std::size_t>(window.first, full.hits.size());
	const std::size_t end = std::min<std::size_t>(begin + window.maxitems, full.hits.size());
	CHECK_EQ(windowed.hits.size(), end - begin);
	if (windowed.hits.size() != end - begin) { return full; }
	for (std::size_t i = begin; i < end; ++i) {
		CHECK_EQ(windowed.hits[i - begin].did, full.hits[i].did);
		CHECK_EQ(windowed.hits[i - begin].rank, i);
	}
	return full;
}

// Like both(), but for searches whose terms weigh with statistics that are not the whole index's on
// several shards (a wildcard or edit-distance expansion): the documents and the estimate are the model's,
// and the weights those of the search of one shard holding every document.
inline void same_documents(const Corpus& corpus, std::size_t shards, const Query& query, const char* what) {
	const auto want = expected_of(corpus, shards, query);
	if (want.error) {
		CHECK_THROW(run(corpus.strata, shards, query), engine::Error);
		return;
	}
	const auto s = run(corpus.strata, shards, query);
	const auto m = run(corpus.merged, 1, query);
	bool ok = datas_of(corpus.strata, shards, s) == want.datas && s.estimated == want.datas.size() && m.estimated == s.estimated;
	check_ranking(s);
	const auto sw = weights_by_data(corpus.strata, shards, s);
	const auto mw = weights_by_data(corpus.merged, 1, m);
	ok = ok && sw.size() == mw.size();
	if (shards == 1) {
		for (const auto& [data, w] : sw) {
			const auto it = mw.find(data);
			ok = ok && it != mw.end() && close(w, it->second);
		}
	}
	if (!ok) {
		std::fprintf(stderr, "  differs: %s (%zu shards)\n", what, shards);
	}
	CHECK_TRUE(ok);
}

// The kind of the engine error the call raises; none when it raises nothing.
template <typename F>
std::optional<engine::errc> error_kind(F&& f) {
	try {
		f();
	} catch (const engine::Error& error) {
		return error.kind;
	}
	return std::nullopt;
}

// Whether the call raises a T, not translated to an engine error.
template <typename T, typename F>
bool throws_as(F&& f) {
	try {
		f();
	} catch (const T&) {
		return true;
	} catch (...) {
	}
	return false;
}

inline Query t(const char* term, engine::termcount wqf = 1) {
	return Query::term("f", term, wqf);
}

// The key of the document with this id in the search over `shards` shards.
inline std::string key_value(const Index& index, std::size_t shards, engine::docid did) {
	const std::size_t shard = (did - 1) % shards;
	return engine::sync_wait(index.owned[shard]->get(static_cast<engine::docid>((did - 1) / shards + 1)))->value(KEY);
}

inline std::string group_value(const Index& index, std::size_t shards, engine::docid did) {
	const std::size_t shard = (did - 1) % shards;
	return engine::sync_wait(index.owned[shard]->get(static_cast<engine::docid>((did - 1) / shards + 1)))->value(GROUP);
}

// search_callbacks.cc
void test_sorter();
void test_spy();
void test_sources();

// operators.cc
void test_boolean_terms();
void test_single_documents();
void test_appends();
void test_operators();
void test_expansions();

}  // namespace detail
