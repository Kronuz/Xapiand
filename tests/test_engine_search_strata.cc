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


// Unit tests for engine::search on both engines. One corpus is written to a Xapian and a strata shard
// set; every query runs on both through the seam and is checked against a model of the corpus: the
// documents found and the estimate are exact, the order is by weight, and the strata engine's weights
// stay within a stated tolerance of Xapian's. Then what is the strata engine's own: deletes and
// replacements, segments, shards that interleave, and what it refuses.

#include "test_harness.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <set>
#include <thread>
#include <sys/stat.h>
#include <unistd.h>
#include <string>
#include <vector>

#include "engine/callbacks.h"
#include "engine/error.h"
#include "engine/kind.h"
#include "engine/open.h"
#include "engine/search.h"
#include "engine/sync.h"

namespace {

using engine::Kind;
using engine::Op;
using engine::Query;

// The strata engine's weights are Xapian's BM25 over the same statistics; they differ in the last
// digits of the sums, so a relative tolerance covers them, and hits whose weights lie within it of
// each other may swap.
constexpr double tolerance = 1e-9;

constexpr int vocabulary = 6;

struct Model {
	// Per shard, per document number (1-based), the positions of each word and the other terms.
	struct Doc {
		bool live = false;
		std::map<std::string, std::vector<std::uint32_t>> positions;
		std::set<std::string> terms;
		std::map<std::string, std::uint32_t> wdf;   // within-document frequency of each term
		std::uint32_t length = 0;
	};
	std::vector<std::vector<Doc>> shards;
};

std::string path_of(const char* name, Kind kind, int shard) {
	const auto path = (std::filesystem::current_path() / (std::string("engine_search_strata_") + name + "_" + engine::to_string(kind) + std::to_string(shard) + ".db")).string();
	std::filesystem::remove_all(path);
	return path;
}

engine::Document make(Kind kind, int shard, int i, const Model::Doc& model) {
	engine::Document doc(kind);
	doc.set_key("Q" + std::to_string(shard) + "_" + std::to_string(i));
	for (const auto& [term, positions] : model.positions) {
		for (const auto position : positions) {
			doc.add_posting(doc.field(""), term, position);
		}
	}
	for (const auto& term : model.terms) {
		if (!model.positions.contains(term)) {
			if (model.wdf.at(term) == 0) {
				doc.add_boolean_term(doc.field(""), term);
			} else {
				doc.add_term(doc.field(""), term, model.wdf.at(term));
			}
		}
	}
	doc.set_data("d" + std::to_string(i));
	return doc;
}

Model::Doc random_doc(std::mt19937& rng, int i) {
	Model::Doc doc;
	doc.live = true;
	const int length = 3 + static_cast<int>(rng() % 8);
	for (int p = 1; p <= length; ++p) {
		doc.positions["w" + std::to_string(rng() % vocabulary)].push_back(static_cast<std::uint32_t>(p));
	}
	for (const auto& [term, positions] : doc.positions) {
		doc.terms.insert(term);
	}
	doc.terms.insert("common");
	doc.terms.insert("tag" + std::to_string(rng() % 5));
	if (rng() % 3 == 0) {
		doc.terms.insert("tag" + std::to_string(rng() % 5));
	}
	if (i % 2 == 0) {
		doc.terms.insert("even");
	}
	doc.terms.insert("bool" + std::to_string(i % 3));   // a boolean term: present, weighing nothing
	doc.terms.insert("bool" + std::to_string(3 + rng() % 2));
	for (const auto& term : doc.terms) {
		const auto it = doc.positions.find(term);
		doc.wdf[term] = it != doc.positions.end() ? static_cast<std::uint32_t>(it->second.size()) : term.starts_with("bool") ? 0 : 1 + static_cast<std::uint32_t>(term.size() % 3);
		doc.length += doc.wdf[term];
	}
	return doc;
}

struct Index {
	Kind kind;
	std::vector<std::unique_ptr<engine::Shard>> owned;

	std::vector<engine::Shard*> shards(std::size_t n) const {
		std::vector<engine::Shard*> list;
		for (std::size_t i = 0; i < n && i < owned.size(); ++i) {
			list.push_back(owned[i].get());
		}
		return list;
	}
};

// Writes document numbers [from, to) of the shard's model to the index.
void write_docs(Index& index, int shard, const Model& model, std::size_t from, std::size_t to) {
	const auto& list = model.shards[static_cast<std::size_t>(shard)];
	for (std::size_t i = from; i < to; ++i) {
		auto doc = make(index.kind, shard, static_cast<int>(i + 1), list[i]);
		engine::sync_wait(index.owned[static_cast<std::size_t>(shard)]->add(doc, {}));
	}
}

// Two indexes, one per engine, holding the same documents; `commits` splits each shard's load into
// that many commits, which on the strata engine leaves several segments.
void build(const char* name, int shard_count, int count, int commits, Index& xapian, Index& strata, Model& model) {
	std::mt19937 rng(1234);
	model.shards.assign(static_cast<std::size_t>(shard_count), {});
	for (auto* index : {&xapian, &strata}) {
		index->owned.clear();
		for (int s = 0; s < shard_count; ++s) {
			auto opened = engine::sync_wait(engine::open_local(index->kind, path_of(name, index->kind, s), true, true));
			index->owned.push_back(std::move(opened.shard));
		}
	}
	for (int s = 0; s < shard_count; ++s) {
		auto& list = model.shards[static_cast<std::size_t>(s)];
		for (int c = 0; c < commits; ++c) {
			const std::size_t from = list.size();
			for (int n = 0; n < count / commits; ++n) {
				list.push_back(random_doc(rng, static_cast<int>(list.size()) + 1));
			}
			for (auto* index : {&xapian, &strata}) {
				write_docs(*index, s, model, from, list.size());
				engine::sync_wait(index->owned[static_cast<std::size_t>(s)]->commit(engine::Transaction::none));
			}
		}
	}
}

// --- the model's answer ---

using Ids = std::set<std::uint64_t>;

std::uint64_t id_of(std::size_t shard, std::size_t did, std::size_t shards) {
	return (did - 1) * shards + shard + 1;
}

bool phrase_in(const Model::Doc& doc, const std::vector<std::string>& words) {
	const auto first = doc.positions.find(words[0]);
	if (first == doc.positions.end()) {
		return false;
	}
	for (const auto start : first->second) {
		bool all = true;
		for (std::size_t k = 1; k < words.size() && all; ++k) {
			const auto it = doc.positions.find(words[k]);
			all = it != doc.positions.end() && std::binary_search(it->second.begin(), it->second.end(), start + static_cast<std::uint32_t>(k));
		}
		if (all) {
			return true;
		}
	}
	return false;
}

// Terms at distinct positions, in any order, spanning fewer than `window` positions.
bool near_in(const Model::Doc& doc, const std::vector<std::string>& words, std::uint32_t window) {
	std::vector<std::uint32_t> chosen;
	std::function<bool(std::size_t)> place = [&](std::size_t k) {
		if (k == words.size()) {
			const auto [lo, hi] = std::minmax_element(chosen.begin(), chosen.end());
			return *hi - *lo < window;
		}
		const auto it = doc.positions.find(words[k]);
		if (it == doc.positions.end()) {
			return false;
		}
		for (const auto position : it->second) {
			if (std::find(chosen.begin(), chosen.end(), position) != chosen.end()) {
				continue;
			}
			chosen.push_back(position);
			if (place(k + 1)) {
				return true;
			}
			chosen.pop_back();
		}
		return false;
	};
	return place(0);
}

// --- the model's weights: Xapian's BM25 (k1 = 1, k3 = 1, b = 0.5, minimum normalised length 0.5) ---

struct Statistics {
	double docs = 0, length = 0;
	std::map<std::string, double> df;
};

Statistics statistics_of(const Model& model, std::size_t shards) {
	Statistics st;
	for (std::size_t s = 0; s < shards; ++s) {
		for (const auto& doc : model.shards[s]) {
			if (!doc.live) { continue; }
			st.docs += 1;
			st.length += doc.length;
			for (const auto& term : doc.terms) { st.df[term] += 1; }
		}
	}
	return st;
}

double bm25(const Statistics& st, const Model::Doc& doc, const std::string& term, double wqf) {
	const auto it = doc.wdf.find(term);
	if (it == doc.wdf.end() || term.empty()) { return 0.0; }
	const double df = st.df.at(term);
	double tw = (st.docs - df + 0.5) / (df + 0.5);
	if (tw < 2) { tw = tw * 0.5 + 1; }
	const double idf = std::log(tw) * 2.0 * wqf / (1.0 + wqf) * 2.0;
	const double K = 1.0 * (0.5 + 0.5 * std::max(doc.length / (st.length / st.docs), 0.5));
	return idf * it->second / (K + it->second);
}

// Whether the document matches and, if so, its weight.
std::pair<bool, double> weigh(const Statistics& st, const Model::Doc& doc, const Query& query) {
	const auto* node = query.node();
	if (node == nullptr || !doc.live) { return {false, 0.0}; }
	switch (node->kind) {
		case engine::QueryKind::term:
			if (node->str.empty()) { return {true, 0.0}; }
			return {doc.terms.contains(node->str), bm25(st, doc, node->str, node->wqf)};
		case engine::QueryKind::scale: {
			auto [hit, weight] = weigh(st, doc, node->subqueries[0]);
			return {hit, weight * node->factor};
		}
		case engine::QueryKind::combine:
		case engine::QueryKind::append: {
			const auto& subs = node->subqueries;
			if (node->kind == engine::QueryKind::append && subs[0].empty()) { return weigh(st, doc, subs[1]); }
			switch (node->op) {
				case Op::AND:
				case Op::OR: {
					bool any = false, every = true;
					double sum = 0;
					for (const auto& sub : subs) {
						const auto [hit, weight] = weigh(st, doc, sub);
						any = any || hit;
						every = every && hit;
						sum += hit ? weight : 0.0;
					}
					return {node->op == Op::AND ? every : any, sum};
				}
				case Op::AND_NOT: {
					auto left = weigh(st, doc, subs[0]);
					for (std::size_t i = 1; i < subs.size() && left.first; ++i) {
						left.first = !weigh(st, doc, subs[i]).first;
					}
					return left;
				}
				case Op::AND_MAYBE: {
					auto left = weigh(st, doc, subs[0]);
					const auto right = weigh(st, doc, subs[1]);
					if (left.first && right.first) { left.second += right.second; }
					return left;
				}
				case Op::FILTER: {
					auto left = weigh(st, doc, subs[0]);
					left.first = left.first && weigh(st, doc, subs[1]).first;
					return left;
				}
				case Op::PHRASE:
				case Op::NEAR: {
					std::vector<std::string> words;
					double sum = 0;
					for (const auto& sub : subs) {
						words.push_back(sub.node()->str);
						sum += bm25(st, doc, sub.node()->str, sub.node()->wqf);
					}
					const bool hit = node->op == Op::PHRASE ? phrase_in(doc, words) : near_in(doc, words, node->parameter ? node->parameter : static_cast<std::uint32_t>(words.size()));
					return {hit, sum};
				}
				default:
					return {false, 0.0};
			}
		}
		default:
			return {false, 0.0};
	}
}

// The ids of the matching documents and their weights.
std::map<std::uint64_t, double> model_hits(const Model& model, const Query& query, std::size_t shards) {
	const auto st = statistics_of(model, shards);
	std::map<std::uint64_t, double> hits;
	for (std::size_t s = 0; s < shards; ++s) {
		const auto& docs = model.shards[s];
		for (std::size_t d = 0; d < docs.size(); ++d) {
			const auto [hit, weight] = weigh(st, docs[d], query);
			if (hit) {
				hits[id_of(s, d + 1, shards)] = weight;
			}
		}
	}
	return hits;
}

bool has_optional(const Query& query) {
	const auto* node = query.node();
	if (node == nullptr) { return false; }
	if ((node->kind == engine::QueryKind::combine || node->kind == engine::QueryKind::append) && (node->op == Op::AND_MAYBE || node->op == Op::FILTER || node->op == Op::PHRASE || node->op == Op::NEAR)) {
		return true;
	}
	for (const auto& sub : node->subqueries) {
		if (has_optional(sub)) { return true; }
	}
	return false;
}

// Records every document it is shown.
class CountingSpy final : public engine::MatchSpy {
public:
	std::vector<engine::docid> seen;
	std::vector<double> weights;
	std::vector<std::string> data;

	void see(const engine::DocView& doc, double weight) override {
		seen.push_back(doc.did());
		weights.push_back(weight);
		data.push_back(doc.data());
	}
	std::unique_ptr<engine::MatchSpy> clone() const override { return std::make_unique<CountingSpy>(); }
	std::string name() const override { return "CountingSpy"; }
	std::string serialise() const override { return ""; }
	std::unique_ptr<engine::MatchSpy> unserialise(std::string_view) const override { return std::make_unique<CountingSpy>(); }
	std::string serialise_results() const override { return ""; }
	void merge_results(std::string_view) override { }
	std::string description() const override { return "CountingSpy"; }
};

// --- running a query ---

engine::SearchResult run(const Index& index, std::size_t shards, const Query& query, engine::doccount first, engine::doccount maxitems, engine::DocidOrder order = engine::DocidOrder::ascending) {
	auto prepared = engine::prepare(query);
	engine::SearchRequest request;
	request.query = prepared.get();
	request.first = first;
	request.maxitems = maxitems;
	request.check_at_least = 100000;
	request.order = order;
	request.origin = "test";
	return engine::sync_wait(engine::search(index.shards(shards), request));
}

bool close(double a, double b) {
	return std::fabs(a - b) <= tolerance * std::max(1.0, std::fabs(b));
}

// Hits in the same order up to ties: the weights agree position by position, and the ids of each
// run of equal weights agree as sets.
bool same_ranking(const std::vector<engine::Hit>& a, const std::vector<engine::Hit>& b) {
	if (a.size() != b.size()) {
		return false;
	}
	for (std::size_t i = 0; i < a.size();) {
		std::size_t j = i;
		std::set<engine::docid> left, right;
		while (j < a.size() && close(a[j].weight, a[i].weight)) {
			if (!close(a[j].weight, b[j].weight)) {
				return false;
			}
			left.insert(a[j].did);
			right.insert(b[j].did);
			++j;
		}
		if (j == i || left != right) {
			// A run cut by the window may end inside a run of the other: compare what is there.
			if (j == i) { return false; }
			return false;
		}
		i = j;
	}
	return true;
}

Ids ids_of(const engine::SearchResult& result) {
	Ids ids;
	for (const auto& hit : result.hits) {
		ids.insert(hit.did);
	}
	return ids;
}

double worst_difference = 0;
int ranking_differs = 0, optional_differs = 0, xapian_differs = 0, xapian_asserts = 0;

// One query on both engines against the model and each other.
void compare(const Index& xapian, const Index& strata, const Model& model, std::size_t shards, const Query& query, bool distinct, bool force = false) {
	const bool rank = force || (distinct && !has_optional(query));
	const auto weights = model_hits(model, query, shards);
	Ids expected;
	for (const auto& [id, weight] : weights) { expected.insert(id); }
	// A Debug build of Xapian asserts inside its own matcher on a few shapes; that query is not compared with it.
	// With NO_XAPIAN_COMPARE the Xapian engine is not searched (its matcher misbehaves under ASan and Debug asserts).
	engine::SearchResult x;
	bool xapian_ok = std::getenv("NO_XAPIAN_COMPARE") == nullptr;
	try {
		if (xapian_ok) {
			x = run(xapian, shards, query, 0, 100000);
		}
	} catch (const engine::Error& error) {
		if (error.kind != engine::errc::internal) { throw; }
		xapian_ok = false;
		++xapian_asserts;
	}
	const auto s = run(strata, shards, query, 0, 100000);
	if (xapian_ok && distinct) {
		CHECK_TRUE(ids_of(x) == expected);
		if (ids_of(x) != expected && std::getenv("DIAG")) {
			std::fprintf(stderr, "DIAG %s\n", query.description().c_str());
			for (const auto id : expected) {
				if (!ids_of(x).contains(id)) {
					const auto& d = model.shards[(id - 1) % shards][(id - 1) / shards];
					std::fprintf(stderr, "  model only: id %llu terms:", static_cast<unsigned long long>(id));
					for (const auto& t : d.terms) { std::fprintf(stderr, " %s", t.c_str()); }
					std::fprintf(stderr, "\n");
				}
			}
			for (const auto id : ids_of(x)) {
				if (!expected.contains(id)) {
					const auto& d = model.shards[(id - 1) % shards][(id - 1) / shards];
					std::fprintf(stderr, "  xapian only: id %llu terms:", static_cast<unsigned long long>(id));
					for (const auto& t : d.terms) { std::fprintf(stderr, " %s", t.c_str()); }
					std::fprintf(stderr, "\n");
				}
			}
		}
	} else if (xapian_ok && ids_of(x) != expected) {
		++xapian_differs;
	}
	CHECK_TRUE(ids_of(s) == expected);
	CHECK_EQ(s.estimated, expected.size());
	CHECK_EQ(s.hits.size(), expected.size());
	for (const auto& hit : s.hits) {
		const auto it = weights.find(hit.did);
		if (it == weights.end() || !close(hit.weight, it->second)) {
			CHECK_TRUE(false);
			break;
		}
	}
	for (std::size_t i = 1; i < s.hits.size(); ++i) {
		CHECK_GE(s.hits[i - 1].weight + 1e-12, s.hits[i].weight);
	}
	const bool ranked = !xapian_ok || same_ranking(x.hits, s.hits);
	if (rank) {
		CHECK_TRUE(ranked);
		for (std::size_t i = 0; i < std::min(x.hits.size(), s.hits.size()); ++i) {
			worst_difference = std::max(worst_difference, std::fabs(x.hits[i].weight - s.hits[i].weight) / std::max(1.0, std::fabs(x.hits[i].weight)));
		}
	} else if (!ranked) {
		++(distinct ? optional_differs : ranking_differs);
	}
	for (std::size_t i = 0; i < std::min(x.hits.size(), s.hits.size()); ++i) {
		CHECK_EQ(s.hits[i].rank, i);
	}
	// Windows: the same documents as the full ranking.
	for (const auto& [first, count] : {std::pair{0u, 3u}, {2u, 5u}, {7u, 4u}, {0u, 0u}, {40u, 10u}}) {
		const auto window = run(strata, shards, query, first, count);
		CHECK_EQ(window.estimated, expected.size());
		const std::size_t begin = std::min<std::size_t>(first, s.hits.size());
		const std::size_t end = std::min<std::size_t>(begin + count, s.hits.size());
		REQUIRE_EQ(window.hits.size(), end - begin);
		for (std::size_t i = begin; i < end; ++i) {
			CHECK_EQ(window.hits[i - begin].did, s.hits[i].did);
			CHECK_EQ(window.hits[i - begin].rank, i);
		}
	}
}

// --- random queries ---

// The terms a query may use. With `distinct`, no term is used twice in a query: Xapian weighs a term that
// a query uses in several places (or in several roles) in ways the strata engine does not copy, so the
// ranking is compared only for queries of distinct terms; the documents found are always compared with the model.
struct Terms {
	std::mt19937& rng;
	bool distinct;
	std::set<std::string> used;
	int missing = 0;

	std::string pick() {
		static const char* const pool[] = {"w0", "w1", "w2", "w3", "w4", "w5", "common", "even", "tag0", "tag1", "tag2", "tag3", "tag4", "bool0", "bool1", "bool2", "bool3", "bool4"};
		for (int attempt = 0; attempt < 40; ++attempt) {
			std::string term = pool[rng() % (distinct ? 13 : 18)];   // Xapian is compared only on queries without boolean terms
			if (rng() % 12 == 0) {
				term = "zzz" + std::to_string(missing++);
			}
			if (!distinct || used.insert(term).second) {
				return term;
			}
		}
		return "zzz" + std::to_string(missing++);
	}
	std::string word() {
		for (int attempt = 0; attempt < 40; ++attempt) {
			std::string term = "w" + std::to_string(rng() % vocabulary);
			if (!distinct || used.insert(term).second) {
				return term;
			}
		}
		return "zzz" + std::to_string(missing++);
	}
};

Query random_query(Terms& terms, int depth) {
	auto& rng = terms.rng;
	if (depth == 0 || rng() % 4 == 0) {
		return Query::term("", terms.pick(), 1 + static_cast<engine::termcount>(rng() % 3));
	}
	switch (rng() % 9) {
		case 0:
		case 1: {
			std::vector<Query> subs;
			for (unsigned n = 2 + rng() % 2; n > 0; --n) { subs.push_back(random_query(terms, depth - 1)); }
			return Query::combine(Op::AND, std::move(subs));
		}
		case 2:
		case 3: {
			std::vector<Query> subs;
			for (unsigned n = 2 + rng() % 3; n > 0; --n) { subs.push_back(random_query(terms, depth - 1)); }
			return Query::combine(Op::OR, std::move(subs));
		}
		case 4: return Query::combine(Op::AND_NOT, random_query(terms, depth - 1), random_query(terms, depth - 1));
		case 5: return Query::combine(Op::AND_MAYBE, random_query(terms, depth - 1), random_query(terms, depth - 1));
		case 6: return Query::combine(Op::FILTER, random_query(terms, depth - 1), random_query(terms, depth - 1));
		case 7: {
			std::vector<Query> subs;
			for (unsigned n = 2 + rng() % 2; n > 0; --n) { subs.push_back(Query::term("", terms.word())); }
			const auto n = static_cast<engine::termcount>(subs.size());
			if (rng() % 2) {
				return Query::combine(Op::PHRASE, std::move(subs), rng() % 2 ? 0 : n);
			}
			return Query::combine(Op::NEAR, std::move(subs), rng() % 2 ? 0 : n + static_cast<engine::termcount>(rng() % 3));
		}
		default: return Query::append(Op::OR, random_query(terms, depth - 1), random_query(terms, depth - 1));
	}
}


void random_queries(const Index& xapian, const Index& strata, const Model& model, std::size_t shards, unsigned seed, int count, bool distinct) {
	std::mt19937 rng(seed);
	for (int i = 0; i < count; ++i) {
		Terms terms{rng, distinct, {}, 0};
		const auto query = random_query(terms, 3);
		const int before = testh::failures;
		compare(xapian, strata, model, shards, query, distinct);
		if (testh::failures != before) {
			std::fprintf(stderr, "  query %d: %s\n", i, query.description().c_str());
		}
	}
}

void test_one_shard_impl() {
	Index xapian{Kind::xapian, {}}, strata{Kind::strata, {}};
	Model model;
	build("one", 1, 60, 3, xapian, strata, model);
	random_queries(xapian, strata, model, 1, 7, 300, true);
	random_queries(xapian, strata, model, 1, 8, 300, false);
	// A single term finds what the model says, heaviest first.
	const auto found = run(strata, 1, Query::term("", "w1"), 0, 10);
	CHECK_GT(found.hits.size(), 0u);
	for (std::size_t i = 1; i < found.hits.size(); ++i) {
		CHECK_GE(found.hits[i - 1].weight, found.hits[i].weight);
	}
	CHECK_EQ(found.hits.front().percent, 100);
	std::printf("one shard: worst relative weight difference %.3g\n", worst_difference);
}

// Positional operators on their own and under AND rank as in Xapian.
void test_positional_impl() {
	Index xapian{Kind::xapian, {}}, strata{Kind::strata, {}};
	Model model;
	build("positional", 2, 80, 2, xapian, strata, model);
	const auto t = [](const char* w) { return Query::term("", w); };
	for (const std::size_t shards : {1u, 2u}) {
		for (int a = 0; a < vocabulary; ++a) {
			for (int b = 0; b < vocabulary; ++b) {
				if (a == b) { continue; }
				const auto wa = "w" + std::to_string(a), wb = "w" + std::to_string(b);
				const auto phrase = Query::combine(Op::PHRASE, {t(wa.c_str()), t(wb.c_str())});
				const auto near = Query::combine(Op::NEAR, {t(wa.c_str()), t(wb.c_str())}, 4);
				for (const auto& query : {phrase, near, Query::combine(Op::AND, phrase, t("common")), Query::combine(Op::AND, near, t("even"))}) {
					const int before = testh::failures;
					compare(xapian, strata, model, shards, query, true, true);
					if (testh::failures != before) { std::fprintf(stderr, "  %s\n", query.description().c_str()); }
				}
			}
		}
	}
}

// An append onto a query that matches nothing by its structure (a text that analysed to nothing) is the
// appended query; a term that no segment holds is a different matter and joins as nothing.
void test_append_impl() {
	Index xapian{Kind::xapian, {}}, strata{Kind::strata, {}};
	Model model;
	build("append", 2, 40, 2, xapian, strata, model);
	const auto t = [](const char* w) { return Query::term("", w); };
	std::vector<Query> queries;
	queries.push_back(Query::append(Op::AND, Query(), t("w1")));
	queries.push_back(Query::append(Op::AND, Query::append(Op::AND, Query(), t("w1")), t("tag2")));
	queries.push_back(Query::append(Op::OR, Query::append(Op::AND, Query(), t("w1")), t("tag2")));
	queries.push_back(Query::append(Op::AND, Query::append(Op::AND, Query(), Query()), t("w3")));
	queries.push_back(Query::append(Op::AND, t("zzz"), t("w1")));
	queries.push_back(Query::append(Op::AND, t("w1"), t("zzz")));
	queries.push_back(Query::append(Op::AND_NOT, Query(), t("w1")));
	for (std::size_t shards = 1; shards <= 2; ++shards) {
		for (const auto& query : queries) {
			compare(xapian, strata, model, shards, query, true);
		}
	}
	CHECK_EQ(run(strata, 2, queries[0], 0, 1000).estimated, model_hits(model, t("w1"), 2).size());
	CHECK_EQ(run(strata, 2, queries[4], 0, 1000).estimated, 0u);
}

void test_two_shards_impl() {
	Index xapian{Kind::xapian, {}}, strata{Kind::strata, {}};
	Model model;
	build("two", 2, 50, 2, xapian, strata, model);
	random_queries(xapian, strata, model, 2, 11, 300, true);
	random_queries(xapian, strata, model, 2, 12, 300, false);
	random_queries(xapian, strata, model, 1, 13, 50, true);
	std::printf("two shards: worst relative weight difference %.3g\n", worst_difference);
	std::printf("Xapian asserted inside its own matcher on %d queries (a Debug build)\n", xapian_asserts);
	std::printf("Xapian's ranking differs from the strata engine's (which matches the model) in %d queries with repeated terms and %d of distinct terms with AND_MAYBE, FILTER, PHRASE or NEAR; its documents differ from the model in %d\n", ranking_differs, optional_differs, xapian_differs);
}

// A spy is shown every match once, with its id in its shard, its data and its weight, whatever window or
// order the request asks for and whether or not any hit is wanted.
void test_spy_impl() {
	Index xapian{Kind::xapian, {}}, strata{Kind::strata, {}};
	Model model;
	build("spy", 3, 30, 2, xapian, strata, model);
	const auto query = Query::combine(Op::OR, Query::term("", "w1"), Query::term("", "tag2"));
	const auto expected = model_hits(model, query, 3);
	// What the spy must be shown: the (id in its shard, weight) of every match, sorted.
	std::vector<std::pair<engine::docid, double>> want;
	for (const auto& [id, weight] : expected) { want.emplace_back(static_cast<engine::docid>((id - 1) / 3 + 1), weight); }
	std::sort(want.begin(), want.end());
	struct Variant { engine::doccount first, maxitems; engine::DocidOrder order; };
	for (auto* index : {&xapian, &strata}) {
		for (const auto& variant : {Variant{0, 3, engine::DocidOrder::ascending}, Variant{0, 0, engine::DocidOrder::ascending}, Variant{2, 4, engine::DocidOrder::descending}}) {
			if (index == &xapian && variant.maxitems == 0) { continue; }   // Xapian shows a spy nothing when no hit is wanted
			CountingSpy spy;
			auto prepared = engine::prepare(query);
			engine::SearchRequest request;
			request.query = prepared.get();
			request.first = variant.first;
			request.maxitems = variant.maxitems;
			request.order = variant.order;
			request.check_at_least = 100000;   // Xapian shows its spies only the documents it examines
			request.spy = &spy;
			const auto result = engine::sync_wait(engine::search(index->shards(3), request));
			CHECK_EQ(spy.seen.size(), expected.size());
			CHECK_EQ(result.estimated, expected.size());
			std::vector<std::pair<engine::docid, double>> got;
			for (std::size_t i = 0; i < spy.seen.size(); ++i) {
				CHECK_EQ(spy.data[i], "d" + std::to_string(spy.seen[i]));
				got.emplace_back(spy.seen[i], spy.weights[i]);
			}
			std::sort(got.begin(), got.end());
			REQUIRE_EQ(got.size(), want.size());
			for (std::size_t i = 0; i < got.size(); ++i) {
				CHECK_EQ(got[i].first, want[i].first);
				CHECK_TRUE(close(got[i].second, want[i].second));
			}
		}
	}
	// A deleted document is not shown: the common term matches every live document of the three shards.
	engine::sync_wait(strata.owned[0]->erase_by_key("Q0_5", true, std::nullopt));
	CountingSpy spy;
	auto prepared = engine::prepare(Query::term("", "common"));
	engine::SearchRequest request;
	request.query = prepared.get();
	request.maxitems = 1;
	request.spy = &spy;
	const auto result = engine::sync_wait(engine::search(strata.shards(3), request));
	CHECK_EQ(spy.seen.size(), 89u);
	CHECK_EQ(result.estimated, 89u);
	CHECK_EQ(std::count(spy.seen.begin(), spy.seen.end(), 5u), 2);
}

void test_five_shards_impl() {
	Index xapian{Kind::xapian, {}}, strata{Kind::strata, {}};
	Model model;
	build("five", 5, 40, 2, xapian, strata, model);
	random_queries(xapian, strata, model, 5, 17, 200, true);
	random_queries(xapian, strata, model, 5, 18, 200, false);
	// Ties go to the lower id: match_all weighs every document alike.
	const auto all = run(strata, 5, Query::match_all(), 0, 1000);
	REQUIRE_EQ(all.hits.size(), 200u);
	for (std::size_t i = 0; i < all.hits.size(); ++i) {
		CHECK_EQ(all.hits[i].did, i + 1);
	}
	const auto descending = run(strata, 5, Query::match_all(), 0, 1000, engine::DocidOrder::descending);
	REQUIRE_EQ(descending.hits.size(), 200u);
	for (std::size_t i = 0; i < descending.hits.size(); ++i) {
		CHECK_EQ(descending.hits[i].did, 200 - i);
	}
}

void test_no_shards_and_empty_impl() {
	Index xapian{Kind::xapian, {}}, strata{Kind::strata, {}};
	Model model;
	build("empty", 1, 4, 1, xapian, strata, model);
	for (auto* index : {&xapian, &strata}) {
		auto none = engine::prepare(Query());
		engine::SearchRequest request;
		request.query = none.get();
		CHECK_EQ(engine::sync_wait(engine::search(index->shards(1), request)).hits.size(), 0u);
		auto some = engine::prepare(Query::match_all());
		request.query = some.get();
		CHECK_EQ(engine::sync_wait(engine::search(std::span<engine::Shard* const>{}, request)).hits.size(), 0u);
		CHECK_EQ(engine::sync_wait(engine::search(index->shards(1), request)).hits.size(), 4u);
	}
}


// --- what is the strata engine's own ---

template <typename F>
engine::errc error_kind(F&& f) {
	try {
		f();
	} catch (const engine::Error& error) {
		return error.kind;
	}
	return engine::errc::internal;
}

engine::SearchResult search_with(const Index& index, const Query& query, const std::function<void(engine::SearchRequest&)>& tweak) {
	auto prepared = engine::prepare(query);
	engine::SearchRequest request;
	request.query = prepared.get();
	request.maxitems = 10;
	tweak(request);
	return engine::sync_wait(engine::search(index.shards(1), request));
}

// Writes become visible to a search with no commit, deletes and replacements are honoured, and a
// replaced document is found once, under its old id.
void test_writes_and_deletes_impl() {
	Index xapian{Kind::xapian, {}}, strata{Kind::strata, {}};
	Model model;
	build("writes", 1, 40, 2, xapian, strata, model);
	std::mt19937 rng(99);
	for (auto* index : {&xapian, &strata}) {
		auto& shard = *index->owned[0];
		// A write with no commit: searched at once.
		auto added = random_doc(rng, 41);
		added.terms.insert("fresh");
		added.wdf["fresh"] = 1;
		added.length += 1;
		auto doc = make(index->kind, 0, 41, added);
		engine::sync_wait(shard.add(doc, {}));
		CHECK_EQ(run(*index, 1, Query::term("", "fresh"), 0, 10).estimated, 1u);
		CHECK_EQ(run(*index, 1, Query::term("", "common"), 0, 10).estimated, 41u);
		// A delete.
		engine::sync_wait(shard.erase_by_key("Q0_5", true, std::nullopt));
		CHECK_EQ(run(*index, 1, Query::term("", "common"), 0, 100).estimated, 40u);
		CHECK_EQ(run(*index, 1, Query::term("", "common"), 0, 100).hits.size(), 40u);
		const auto gone = engine::sync_wait(shard.find_key("Q0_5"));
		CHECK_EQ(gone, 0u);
		for (const auto& hit : run(*index, 1, Query::term("", "common"), 0, 100).hits) {
			CHECK_NE(hit.did, 5u);
		}
		// A replacement: new content under the key, found once.
		engine::Document replacement(index->kind);
		replacement.set_key("Q0_7");
		replacement.add_posting(replacement.field(""), "renewed", 1);
		replacement.add_posting(replacement.field(""), "common", 2);
		engine::sync_wait(shard.replace_by_key("Q0_7", replacement, {}));
		const auto renewed = run(*index, 1, Query::term("", "renewed"), 0, 10);
		CHECK_EQ(renewed.estimated, 1u);
		REQUIRE_EQ(renewed.hits.size(), 1u);
		CHECK_EQ(renewed.hits[0].did, engine::sync_wait(shard.find_key("Q0_7")));
		CHECK_EQ(run(*index, 1, Query::term("", "common"), 0, 100).estimated, 40u);
		// The old content of the replaced document is gone from the terms it no longer has.
		std::size_t had = 0;
		for (const auto& term : model.shards[0][6].terms) {
			if (term.starts_with("tag")) {
				++had;
				const auto found = run(*index, 1, Query::term("", term), 0, 100);
				for (const auto& hit : found.hits) {
					CHECK_NE(hit.did, engine::sync_wait(shard.find_key("Q0_7")));
				}
			}
		}
		CHECK_GT(had, 0u);
	}
}

void test_refusals_impl() {
	Index xapian{Kind::xapian, {}}, strata{Kind::strata, {}};
	Model model;
	build("refusals", 1, 10, 1, xapian, strata, model);
	const auto t = [](const char* w) { return Query::term("", w); };
	const auto none = [](engine::SearchRequest&) { };
	const auto kind = [&](const Query& query, const std::function<void(engine::SearchRequest&)>& tweak = [](engine::SearchRequest&) { }) {
		return error_kind([&] { search_with(strata, query, tweak); });
	};
	const auto syntax = engine::errc::syntax;
	CHECK_EQ(search_with(strata, t("w1"), none).hits.size() > 0, true);
	CHECK_EQ(kind(t("w1"), [&](engine::SearchRequest& r) { r.collapse_key = 2; r.collapse_max = 1; }), syntax);
	CHECK_EQ(kind(t("w1"), [&](engine::SearchRequest& r) { r.percent_threshold = 50; }), syntax);
	CHECK_EQ(kind(t("w1"), [&](engine::SearchRequest& r) { r.weight_threshold = 0.1; }), syntax);
	CHECK_EQ(kind(t("w1"), [&](engine::SearchRequest& r) { r.nearest = engine::Similar{}; }), syntax);
	CHECK_EQ(kind(t("w1"), [&](engine::SearchRequest& r) { r.fuzzy = engine::Similar{}; }), syntax);
	CHECK_EQ(kind(Query::combine(Op::SYNONYM, {t("w1"), Query::combine(Op::OR, t("w2"), t("w3"))})), syntax);
	CHECK_EQ(kind(Query::combine(Op::ELITE_SET, {t("w1"), Query::combine(Op::OR, t("w2"), t("w3"))}, 1)), syntax);
	CHECK_EQ(kind(Query::combine(Op::PHRASE, {t("w1"), t("w2")}, 5)), syntax);
	CHECK_EQ(kind(Query::combine(Op::PHRASE, {t("w1"), Query::combine(Op::OR, t("w2"), t("w3"))})), syntax);
	CHECK_EQ(kind(Query::combine(Op::NEAR, {t("w1"), Query::combine(Op::OR, t("w2"), t("w3"))})), syntax);
	// A refusal leaves nothing behind: the same shard answers the next search.
	CHECK_EQ(search_with(strata, t("w1"), none).hits.size() > 0, true);
	// A search over the two engines is refused, and a query nested too deeply.
	{
		auto prepared = engine::prepare(t("w1"));
		engine::SearchRequest request;
		request.query = prepared.get();
		std::vector<engine::Shard*> both{xapian.owned[0].get(), strata.owned[0].get()};
		CHECK_EQ(error_kind([&] { engine::sync_wait(engine::search(both, request)); }), engine::errc::invalid);
	}
	// A tree nested past the cap is refused when it is built, before any engine sees it.
	CHECK_EQ(error_kind([&] {
		Query deep = t("w1");
		for (int i = 0; i < 1200; ++i) {
			deep = Query::combine(Op::AND, deep, t("common"));
		}
	}), syntax);
	// An index with no segment refuses the same: the query is checked before any segment is read.
	Index xapian_empty{Kind::xapian, {}}, strata_empty{Kind::strata, {}};
	Model model_empty;
	build("refusals_empty", 1, 0, 1, xapian_empty, strata_empty, model_empty);
	CHECK_EQ(error_kind([&] { search_with(strata_empty, t("w1"), [&](engine::SearchRequest& r) { r.collapse_key = 2; r.collapse_max = 1; }); }), syntax);
}

// A term is looked up under its field: two fields holding the same word are found each under its own, a
// marker term (a field with an empty term) matches the documents that have the field, not every
// document, and a phrase reads its words under their field. Both engines agree.
void test_fields_impl() {
	Index xapian{Kind::xapian, {}}, strata{Kind::strata, {}};
	for (auto* index : {&xapian, &strata}) {
		auto opened = engine::sync_wait(engine::open_local(index->kind, path_of("fields", index->kind, 0), true, true));
		index->owned.clear();
		index->owned.push_back(std::move(opened.shard));
		auto& shard = *index->owned[0];
		const auto add = [&](const char* key, const std::function<void(engine::Document&)>& fill) {
			engine::Document doc(index->kind);
			doc.set_key(key);
			fill(doc);
			doc.set_data(key);
			engine::sync_wait(shard.add(doc, {}));
		};
		add("Q1", [](engine::Document& d) {   // title: same, "first second"; the tags marker
			const auto title = d.field("title"), tags = d.field("tags");
			d.add_term(title, "same");
			d.add_posting(title, "first", 1);
			d.add_posting(title, "second", 2);
			d.add_term(tags, "");
		});
		add("Q2", [](engine::Document& d) {   // body: same, "first second" in the other order
			const auto body = d.field("body");
			d.add_term(body, "same");
			d.add_posting(body, "second", 1);
			d.add_posting(body, "first", 2);
		});
		add("Q3", [](engine::Document& d) {   // both fields, the words split across fields
			const auto title = d.field("title"), body = d.field("body");
			d.add_term(title, "same");
			d.add_term(body, "same");
			d.add_posting(title, "first", 1);
			d.add_posting(body, "second", 1);
		});
		engine::sync_wait(shard.commit(engine::Transaction::none));
	}
	const auto ids = [](const Index& index, const Query& query) {
		std::set<engine::docid> found;
		for (const auto& hit : run(index, 1, query, 0, 100).hits) { found.insert(hit.did); }
		return found;
	};
	using Set = std::set<engine::docid>;
	const auto t = [](const char* f, const char* w) { return Query::term(f, w); };
	const std::pair<Query, Set> cases[] = {
		{t("title", "same"), {1, 3}},
		{t("body", "same"), {2, 3}},
		{t("", "same"), {}},
		{t("tags", ""), {1}},
		{t("title", ""), {}},
		{Query::match_all(), {1, 2, 3}},
		{Query::combine(Op::AND, t("title", "same"), t("body", "same")), {3}},
		{Query::combine(Op::PHRASE, {t("title", "first"), t("title", "second")}), {1}},
		{Query::combine(Op::PHRASE, {t("body", "second"), t("body", "first")}), {2}},
		{Query::combine(Op::PHRASE, {t("title", "first"), t("body", "second")}), {}},
	};
	for (const auto* index : {&xapian, &strata}) {
		for (const auto& [query, expected] : cases) {
			const int before = testh::failures;
			CHECK_TRUE(ids(*index, query) == expected);
			if (testh::failures != before) { std::fprintf(stderr, "  %s: %s\n", engine::to_string(index->kind), query.description().c_str()); }
		}
	}
}

// A query's whole answer against the model: the documents, their weights and their order (heavier first,
// ties by id in the requested order), and the estimate.
void check_against_model(const Index& index, const Model& model, std::size_t shards, const Query& query, engine::DocidOrder order = engine::DocidOrder::ascending) {
	const auto expected = model_hits(model, query, shards);
	const auto found = run(index, shards, query, 0, 100000, order);
	CHECK_EQ(found.estimated, expected.size());
	REQUIRE_EQ(found.hits.size(), expected.size());
	for (std::size_t i = 0; i < found.hits.size(); ++i) {
		const auto& hit = found.hits[i];
		const auto it = expected.find(hit.did);
		REQUIRE(it != expected.end());
		CHECK_TRUE(close(hit.weight, it->second));
		if (i > 0) {
			const auto& before = found.hits[i - 1];
			CHECK_TRUE(before.weight >= hit.weight);
			if (before.weight == hit.weight) {
				CHECK_TRUE(order == engine::DocidOrder::descending ? before.did > hit.did : before.did < hit.did);
			}
		}
	}
}

// Long chains of appends are built, run and freed without recursion that grows with the chain: the
// scorers of a run of one operator are one flat scorer, as the query tree frees iteratively.
void test_long_chains_impl() {
	Index xapian{Kind::xapian, {}}, strata{Kind::strata, {}};
	Model model;
	build("chains", 2, 40, 2, xapian, strata, model);
	const auto t = [](const char* w) { return Query::term("", w); };
	const int kChain = static_cast<int>(engine::MAX_QUERY_CLAUSES) - 100;   // the longest chain a query may hold
	Query ors = t("w1");
	for (int i = 0; i < kChain; ++i) {
		ors = Query::append(Op::OR, ors, i % 2 ? t("w1") : t("zzz"));
	}
	check_against_model(strata, model, 2, ors);
	Query ands = t("common");
	for (int i = 0; i < kChain; ++i) {
		ands = Query::append(Op::AND, ands, t("common"));
	}
	check_against_model(strata, model, 2, ands);
	Query mixed = t("w1");
	for (int i = 0; i < kChain; ++i) {
		mixed = Query::append(i % 3 == 0 ? Op::AND : Op::OR, mixed, i % 2 ? t("common") : t("w1"));
	}
	check_against_model(strata, model, 2, mixed);   // ends on OR common
	// A chain of exclusions nests one scorer per link; thousands of links run.
	Query excluded = t("common");
	for (int i = 0; i < kChain; ++i) {
		excluded = Query::append(Op::AND_NOT, excluded, t("zzz"));
	}
	check_against_model(strata, model, 2, excluded);
	// A wide OR and a wide AND in one node.
	std::vector<Query> wide;
	for (int i = 0; i < kChain; ++i) { wide.push_back(t("w1")); }
	check_against_model(strata, model, 2, Query::combine(Op::OR, wide));
	check_against_model(strata, model, 2, Query::combine(Op::AND, wide));
}

// Searches that name the same shards in opposite orders, with writers, neither deadlock nor lose a document.
void test_concurrent_searches_impl() {
	Index xapian{Kind::xapian, {}}, strata{Kind::strata, {}};
	Model model;
	build("threads", 2, 40, 2, xapian, strata, model);
	std::atomic<bool> stop{false};
	std::atomic<int> failures{0};
	const auto searcher = [&](bool reversed) {
		std::vector<engine::Shard*> shards{strata.owned[0].get(), strata.owned[1].get()};
		if (reversed) { std::swap(shards[0], shards[1]); }
		auto prepared = engine::prepare(Query::term("", "common"));
		engine::SearchRequest request;
		request.query = prepared.get();
		request.maxitems = 5;
		for (int i = 0; i < 300; ++i) {
			const auto result = engine::sync_wait(engine::search(shards, request));
			if (result.estimated < 80 || result.hits.size() != 5) { ++failures; }
		}
	};
	std::thread writer([&] {
		std::mt19937 rng(5);
		for (int i = 0; i < 100 && !stop; ++i) {
			auto doc = random_doc(rng, 1000 + i);
			auto written = make(Kind::strata, 1, 1000 + i, doc);
			engine::sync_wait(strata.owned[i % 2]->add(written, {}));
		}
	});
	std::thread a(searcher, false), b(searcher, true), c(searcher, false);
	a.join();
	b.join();
	c.join();
	stop = true;
	writer.join();
	CHECK_EQ(failures.load(), 0);
	// The same shard named twice is searched once and counted twice.
	std::vector<engine::Shard*> twice{strata.owned[0].get(), strata.owned[0].get()};
	auto prepared = engine::prepare(Query::term("", "common"));
	engine::SearchRequest request;
	request.query = prepared.get();
	request.maxitems = 1000;
	const auto result = engine::sync_wait(engine::search(twice, request));
	const auto once = engine::sync_wait(engine::search(std::vector<engine::Shard*>{strata.owned[0].get()}, request));
	CHECK_EQ(result.estimated, 2 * once.estimated);
}


// The BM25 weight of "common" in an index of n identical documents: the weight every hit has when the
// searched snapshot holds exactly n documents.
double weight_in(std::size_t n, const Model::Doc& doc) {
	Model sized;
	sized.shards.assign(3, {});
	sized.shards[0].assign(n, doc);
	return weigh(statistics_of(sized, 3), doc, Query::term("", "common")).second;
}

// A search that runs while another thread writes and commits sees one snapshot of the whole index.
// The writer adds document g to shard 0, then 1, then 2, so the states that ever existed are
// (g,g,g), (g+1,g,g), (g+1,g+1,g) and back to (g+1,g+1,g+1); a search holds every shard's lock before it
// reads any, so what it finds is one of them: the same documents in every shard as of one moment, with the
// statistics (and so the weights) of exactly those documents. Searches name the shards in opposite orders.
void test_snapshot_consistency_impl() {
	Index xapian{Kind::xapian, {}}, strata{Kind::strata, {}};
	Model model;
	build("snapshot", 3, 0, 1, xapian, strata, model);
	Model::Doc doc;
	doc.live = true;
	doc.terms = {"common", "pad"};
	doc.wdf = {{"common", 1}, {"pad", 2}};
	doc.length = 3;
	constexpr int generations = 150;
	std::atomic<bool> done{false};
	std::atomic<int> bad{0}, searches{0};
	std::mutex guard;
	std::map<std::size_t, double> weights;   // n documents: their weight
	std::set<std::size_t> sizes;
	const auto weight_for = [&](std::size_t n) {
		std::lock_guard lock(guard);
		sizes.insert(n);
		const auto it = weights.find(n);
		return it != weights.end() ? it->second : weights[n] = weight_in(n, doc);
	};
	const auto fail = [&](const char* what, std::size_t a, std::size_t b, std::size_t c) {
		if (bad++ == 0) { std::fprintf(stderr, "snapshot: %s (%zu %zu %zu)\n", what, a, b, c); }
	};
	// kind 0: shards in order; 1: reversed, descending ids; 2: in order with a spy that sees every match.
	const auto searcher = [&](int kind) {
		std::vector<engine::Shard*> shards = strata.shards(3);
		std::vector<std::size_t> shard_at{0, 1, 2};   // the writer's shard at each position of the request
		if (kind == 1) { std::reverse(shards.begin(), shards.end()); std::reverse(shard_at.begin(), shard_at.end()); }
		auto prepared = engine::prepare(Query::term("", "common"));
		while (!done || searches < 40) {
			CountingSpy spy;
			engine::SearchRequest request;
			request.query = prepared.get();
			request.maxitems = 100000;
			request.order = kind == 1 ? engine::DocidOrder::descending : engine::DocidOrder::ascending;
			if (kind == 2) { request.spy = &spy; }
			const auto result = engine::sync_wait(engine::search(shards, request));
			++searches;
			std::size_t count[3] = {0, 0, 0}, top[3] = {0, 0, 0};
			for (const auto& hit : result.hits) {
				const auto position = (hit.did - 1) % 3;
				++count[shard_at[position]];
				top[shard_at[position]] = std::max<std::size_t>(top[shard_at[position]], (hit.did - 1) / 3 + 1);
			}
			const std::size_t n = result.hits.size();
			if (result.estimated != n) { fail("estimate differs from the hits", result.estimated, n, 0); }
			for (std::size_t s = 0; s < 3; ++s) {
				if (top[s] != count[s]) { fail("a shard's documents are not 1..count", s, top[s], count[s]); }
			}
			if (!(count[0] >= count[1] && count[1] >= count[2] && count[2] + 1 >= count[0])) { fail("shards from different moments", count[0], count[1], count[2]); }
			if (n > 0) {
				const double want = weight_for(n);
				for (const auto& hit : result.hits) {
					if (!close(hit.weight, want)) { fail("weights of another snapshot", n, 0, 0); break; }
				}
			}
			if (kind == 2) {
				if (spy.seen.size() != n) { fail("the spy saw other documents than the hits", spy.seen.size(), n, 0); }
				for (const auto weight : spy.weights) {
					if (n > 0 && !close(weight, weight_for(n))) { fail("spy weights of another snapshot", n, 0, 0); break; }
				}
			}
		}
	};
	std::thread a(searcher, 0), b(searcher, 1), c(searcher, 2), d(searcher, 0);
	std::thread writer([&] {
		for (int g = 1; g <= generations; ++g) {
			for (std::size_t s = 0; s < 3; ++s) {
				auto written = make(Kind::strata, static_cast<int>(s), g, doc);
				engine::sync_wait(strata.owned[s]->add(written, {}));
				if (g % 7 == 0) { engine::sync_wait(strata.owned[s]->commit(engine::Transaction::none)); }
			}
		}
		done = true;
	});
	writer.join();
	a.join();
	b.join();
	c.join();
	d.join();
	CHECK_EQ(bad.load(), 0);
	CHECK_GT(sizes.size(), 3u);   // the searches overlapped the writes
	const auto last = run(strata, 3, Query::term("", "common"), 0, 100000);
	CHECK_EQ(last.estimated, 3u * generations);
	for (const auto& hit : last.hits) { CHECK_TRUE(close(hit.weight, weight_for(3u * generations))); }
}


// A spy that stops inside its first see() until released, with every shard's lock held by the search.
class BlockingSpy final : public engine::MatchSpy {
public:
	std::promise<void> inside;
	std::shared_future<void> gate;
	std::atomic<bool> first{true};
	std::atomic<int> seen{0};

	void see(const engine::DocView&, double) override {
		++seen;
		if (first.exchange(false)) {
			inside.set_value();
			gate.wait();
		}
	}
	std::unique_ptr<engine::MatchSpy> clone() const override { return std::make_unique<CountingSpy>(); }
	std::string name() const override { return "BlockingSpy"; }
	std::string serialise() const override { return ""; }
	std::unique_ptr<engine::MatchSpy> unserialise(std::string_view) const override { return std::make_unique<CountingSpy>(); }
	std::string serialise_results() const override { return ""; }
	void merge_results(std::string_view) override { }
	std::string description() const override { return "BlockingSpy"; }
};

// While a spy is inside see(), every shard's lock is held: a write, a commit and a delete on each shard (the
// one pinned last included) wait until the search ends, and the search sees the state before them.
void test_pin_holds_impl() {
	Index xapian{Kind::xapian, {}}, strata{Kind::strata, {}};
	Model model;
	build("pin_holds", 3, 20, 2, xapian, strata, model);
	std::promise<void> release;
	BlockingSpy spy;
	spy.gate = release.get_future().share();
	engine::SearchResult result;
	std::thread searcher([&] {
		// Asked for out of order, so the locks are not taken in the order of the request.
		std::vector<engine::Shard*> order{strata.owned[2].get(), strata.owned[0].get(), strata.owned[1].get()};
		auto prepared = engine::prepare(Query::term("", "common"));
		engine::SearchRequest request;
		request.query = prepared.get();
		request.maxitems = 1000;
		request.spy = &spy;
		result = engine::sync_wait(engine::search(order, request));
	});
	spy.inside.get_future().wait();
	std::atomic<int> finished{0};
	std::vector<std::thread> writers;
	for (std::size_t s = 0; s < 3; ++s) {
		writers.emplace_back([&, s] {
			auto doc = make(Kind::strata, static_cast<int>(s), 777, model.shards[0][0]);
			engine::sync_wait(strata.owned[s]->add(doc, {}));
			++finished;
		});
		writers.emplace_back([&, s] {
			engine::sync_wait(strata.owned[s]->commit(engine::Transaction::none));
			++finished;
		});
		writers.emplace_back([&, s] {
			engine::sync_wait(strata.owned[s]->erase_by_key("Q" + std::to_string(s) + "_3", true, std::nullopt));
			++finished;
		});
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(700));
	const int during = finished.load();
	release.set_value();
	searcher.join();
	for (auto& writer : writers) { writer.join(); }
	CHECK_EQ(during, 0);
	CHECK_EQ(result.hits.size(), 60u);
	CHECK_EQ(result.estimated, 60u);
	CHECK_EQ(spy.seen.load(), 60);
	CHECK_EQ(finished.load(), 9);
}

// A pin after a failed reopen raises a database error and releases the other shards' locks; it never reads the
// closed index. After a reopen the shard searches again.
void test_pin_after_failed_reopen_impl() {
	if (::geteuid() == 0) {
		return;
	}
	Index xapian{Kind::xapian, {}}, strata{Kind::strata, {}};
	Model model;
	build("pin_failed", 2, 20, 1, xapian, strata, model);
	auto& broken = *strata.owned[0];
	engine::sync_wait(broken.begin_transaction(true));
	auto pending = make(Kind::strata, 0, 556, model.shards[0][0]);
	engine::sync_wait(broken.add(pending, {}));
	// path_of() clears the path it names; this is the directory build() made for shard 0.
	const auto dir = (std::filesystem::current_path() / "engine_search_strata_pin_failed_strata0.db" / "strata").string();
	::chmod(dir.c_str(), 0000);
	CHECK_EQ(error_kind([&] { engine::sync_wait(broken.cancel_transaction()); }), engine::errc::database);
	::chmod(dir.c_str(), 0700);

	auto prepared = engine::prepare(Query::term("", "common"));
	engine::SearchRequest request;
	request.query = prepared.get();
	request.maxitems = 1000;
	CHECK_EQ(error_kind([&] { engine::sync_wait(engine::search(strata.shards(2), request)); }), engine::errc::database);
	CHECK_EQ(error_kind([&] { engine::sync_wait(engine::search(strata.shards(1), request)); }), engine::errc::database);
	// The other shard's lock was released with the failed search: a write to it goes through.
	auto doc = make(Kind::strata, 1, 555, model.shards[1][0]);
	engine::sync_wait(strata.owned[1]->add(doc, {}));
	CHECK_EQ(engine::sync_wait(engine::search(std::vector<engine::Shard*>{strata.owned[1].get()}, request)).hits.size(), 21u);

	CHECK_TRUE(engine::sync_wait(broken.reopen()));
	CHECK_EQ(engine::sync_wait(engine::search(std::vector<engine::Shard*>{strata.owned[0].get()}, request)).hits.size(), 20u);
}

// A window larger than the index is clamped to it: it answers as a window of the index's size does, at
// once, and holds no memory for the number asked.
void test_huge_window_impl() {
	Index xapian{Kind::xapian, {}}, strata{Kind::strata, {}};
	Model model;
	build("huge_window", 2, 40, 2, xapian, strata, model);
	const auto query = Query::term("", "common");
	const auto all = run(strata, 2, query, 0, 1000);
	CHECK_EQ(all.hits.size(), 80u);
	constexpr engine::doccount big = 0xFFFFFFFFu;
	const auto begin = std::chrono::steady_clock::now();
	for (const auto order : {engine::DocidOrder::ascending, engine::DocidOrder::descending}) {
		const auto reference = run(strata, 2, query, 0, 1000, order);
		for (const auto& [first, maxitems] : {std::pair{0u, big}, std::pair{big, 10u}, std::pair{10u, big}, std::pair{big, big}}) {
			const auto found = run(strata, 2, query, first, maxitems, order);
			CHECK_EQ(found.estimated, all.estimated);
			const std::size_t skip = std::min<std::size_t>(first, reference.hits.size());
			CHECK_EQ(found.hits.size(), reference.hits.size() - skip);
			for (std::size_t i = 0; i < found.hits.size(); ++i) {
				CHECK_EQ(found.hits[i].did, reference.hits[skip + i].did);
				CHECK_EQ(found.hits[i].rank, reference.hits[skip + i].rank);
			}
		}
	}
	CountingSpy spy;
	auto prepared = engine::prepare(query);
	engine::SearchRequest request;
	request.query = prepared.get();
	request.maxitems = big;
	request.spy = &spy;
	CHECK_EQ(engine::sync_wait(engine::search(strata.shards(2), request)).hits.size(), 80u);
	CHECK_EQ(spy.seen.size(), 80u);
	CHECK_TRUE(std::chrono::steady_clock::now() - begin < std::chrono::seconds(10));
}

// A hit's percent stays in [0, 100] when the top weight overflows to infinity: the infinite hits are 100, the
// rest 0 (a finite weight against an infinite top).
void test_percent_infinite_impl() {
	Index xapian{Kind::xapian, {}}, strata{Kind::strata, {}};
	Model model;
	build("percent", 2, 40, 2, xapian, strata, model);
	const auto query = Query::combine(Op::OR, Query::scale(Query::term("", "w1"), HUGE_VAL), Query::term("", "w2"));
	for (const bool spied : {false, true}) {
		CountingSpy spy;
		auto prepared = engine::prepare(query);
		engine::SearchRequest request;
		request.query = prepared.get();
		request.maxitems = 100;
		request.spy = spied ? &spy : nullptr;
		const auto found = engine::sync_wait(engine::search(strata.shards(2), request));
		std::size_t infinite = 0;
		CHECK_TRUE(!found.hits.empty());
		for (const auto& hit : found.hits) {
			infinite += std::isinf(hit.weight);
			CHECK_EQ(hit.percent, std::isinf(hit.weight) ? 100 : 0);
		}
		CHECK_TRUE(infinite > 0);
	}
	// Finite weights keep their ratio to the top.
	const auto plain = run(strata, 2, Query::combine(Op::OR, Query::term("", "w1"), Query::term("", "w2")), 0, 100);
	CHECK_TRUE(!plain.hits.empty());
	CHECK_EQ(plain.hits.front().percent, 100);
	for (const auto& hit : plain.hits) {
		CHECK_EQ(hit.percent, static_cast<int>(hit.weight / plain.hits.front().weight * 100.0 + 0.5));
	}
}

// Reports an engine error with its text, which the bare terminate message of an uncaught one lacks.
template <typename F>
void guarded(const char* name, F&& test) {
	if (const char* only = std::getenv("ONLY_TEST"); only != nullptr && std::string(only) != name) {
		return;
	}
	try {
		test();
	} catch (const engine::Error& error) {
		std::fprintf(stderr, "%s: engine error (%s): %s\n", name, engine::to_string(error.kind), error.description().c_str());
		CHECK_TRUE(false);
	}
}

void test_one_shard() { guarded("test_one_shard", test_one_shard_impl); }
void test_positional() { guarded("test_positional", test_positional_impl); }
void test_append() { guarded("test_append", test_append_impl); }
void test_fields() { guarded("test_fields", test_fields_impl); }
void test_two_shards() { guarded("test_two_shards", test_two_shards_impl); }
void test_spy() { guarded("test_spy", test_spy_impl); }
void test_five_shards() { guarded("test_five_shards", test_five_shards_impl); }
void test_no_shards_and_empty() { guarded("test_no_shards_and_empty", test_no_shards_and_empty_impl); }
void test_writes_and_deletes() { guarded("test_writes_and_deletes", test_writes_and_deletes_impl); }
void test_refusals() { guarded("test_refusals", test_refusals_impl); }
void test_long_chains() { guarded("test_long_chains", test_long_chains_impl); }
void test_snapshot_consistency() { guarded("test_snapshot_consistency", test_snapshot_consistency_impl); }
void test_concurrent_searches() { guarded("test_concurrent_searches", test_concurrent_searches_impl); }
void test_pin_holds() { guarded("test_pin_holds", test_pin_holds_impl); }
void test_pin_after_failed_reopen() { guarded("test_pin_after_failed_reopen", test_pin_after_failed_reopen_impl); }
void test_huge_window() { guarded("test_huge_window", test_huge_window_impl); }
void test_percent_infinite() { guarded("test_percent_infinite", test_percent_infinite_impl); }

}  // namespace

TEST_MAIN(test_one_shard, test_positional, test_append, test_fields, test_two_shards, test_spy, test_five_shards, test_no_shards_and_empty, test_writes_and_deletes, test_refusals, test_long_chains, test_concurrent_searches, test_snapshot_consistency, test_pin_holds, test_pin_after_failed_reopen, test_huge_window, test_percent_infinite)
