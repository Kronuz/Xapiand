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

// What the pieces of the engine::search tests share: the model, its index, and the helpers that run a query.

#include "../test_harness.h"

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
#include "engine/open.h"
#include "engine/search.h"
#include "engine/sync.h"

namespace detail {

using engine::Op;
using engine::Query;

// The weights are the model's BM25 over the same statistics; they differ in the last digits of the
// sums, so a relative tolerance covers them, and hits whose weights lie within it of each other may swap.
constexpr double tolerance = 1e-9;

constexpr int vocabulary = 6;

struct Model {
	// Per shard, per document number (1-based), the positions of each word and the other terms.
	struct Doc {
		bool live = false;
		std::map<std::string, std::vector<std::uint32_t>> positions;
		std::set<std::string> terms;
		std::map<std::string, std::uint32_t> wdf;   // within-document frequency of each term
		std::uint32_t length = 0;                    // the sum of every wdf, the key field's included
		std::uint32_t key_wdf = 0;                   // the frequency the host gives the document's id term
	};
	std::vector<std::vector<Doc>> shards;
};

inline std::string path_of(const char* name, int shard) {
	const auto path = (std::filesystem::current_path() / (std::string("engine_search_strata_") + name + "_" + std::to_string(shard) + ".db")).string();
	std::filesystem::remove_all(path);
	return path;
}

inline engine::Document make(int shard, int i, const Model::Doc& model) {
	engine::Document doc;
	const auto key = "Q" + std::to_string(shard) + "_" + std::to_string(i);
	doc.set_key(key);
	// Slot 2 groups the documents (every seventh has no group), slot 3 orders them.
	if (i % 7 != 0) {
		doc.add_value(2, "g" + std::to_string((shard * 3 + i) % 4));
	}
	doc.add_value(3, std::string(1, static_cast<char>('a' + (shard * 5 + i * 3) % 11)));
	if (model.key_wdf != 0) {
		// The id term's frequency counts in the document's length on both engines (D5).
		doc.add_term(doc.field(engine::KEY_FIELD), key, model.key_wdf);
	}
	for (const auto& [term, positions] : model.positions) {
		for (const auto position : positions) {
			doc.add_posting(doc.field("f"), term, position);
		}
	}
	for (const auto& term : model.terms) {
		if (!model.positions.contains(term)) {
			if (model.wdf.at(term) == 0) {
				doc.add_boolean_term(doc.field("f"), term);
			} else {
				doc.add_term(doc.field("f"), term, model.wdf.at(term));
			}
		}
	}
	doc.set_data("d" + std::to_string(i));
	return doc;
}

inline Model::Doc random_doc(std::mt19937& rng, int i) {
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
	doc.key_wdf = 1;
	doc.length += doc.key_wdf;
	return doc;
}

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

// Writes document numbers [from, to) of the shard's model to the index.
inline void write_docs(Index& index, int shard, const Model& model, std::size_t from, std::size_t to) {
	const auto& list = model.shards[static_cast<std::size_t>(shard)];
	for (std::size_t i = from; i < to; ++i) {
		auto doc = make(shard, static_cast<int>(i + 1), list[i]);
		engine::sync_wait(index.owned[static_cast<std::size_t>(shard)]->add(doc, {}));
	}
}

// An index holding the model's documents; `commits` splits each shard's load into that many commits,
// which leaves several segments.
// `host` opens the shards in host mode: their writes take turns (the caller's), a read sees what the last
// refresh flushed, and nothing a reader holds blocks a write.
inline void build(const char* name, int shard_count, int count, int commits, Index& strata, Model& model, bool host = false) {
	std::mt19937 rng(1234);
	model.shards.assign(static_cast<std::size_t>(shard_count), {});
	strata.owned.clear();
	engine::OpenOptions options;
	options.host = host;
	for (int s = 0; s < shard_count; ++s) {
		auto opened = engine::sync_wait(engine::open_local(path_of(name, s), true, true, options));
		strata.owned.push_back(std::move(opened.shard));
	}
	for (int s = 0; s < shard_count; ++s) {
		auto& list = model.shards[static_cast<std::size_t>(s)];
		for (int c = 0; c < commits; ++c) {
			const std::size_t from = list.size();
			for (int n = 0; n < count / commits; ++n) {
				list.push_back(random_doc(rng, static_cast<int>(list.size()) + 1));
			}
			write_docs(strata, s, model, from, list.size());
			engine::sync_wait(strata.owned[static_cast<std::size_t>(s)]->commit(engine::Transaction::none));
		}
	}
}

// --- the model's answer ---

using Ids = std::set<std::uint64_t>;

inline std::uint64_t id_of(std::size_t shard, std::size_t did, std::size_t shards) {
	return (did - 1) * shards + shard + 1;
}

inline bool phrase_in(const Model::Doc& doc, const std::vector<std::string>& words) {
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
inline bool near_in(const Model::Doc& doc, const std::vector<std::string>& words, std::uint32_t window) {
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

// --- the model's weights: BM25 (k1 = 1, k3 = 1, b = 0.5, minimum normalised length 0.5) ---

struct Statistics {
	double docs = 0, length = 0;
	std::map<std::string, double> df;
};

inline Statistics statistics_of(const Model& model, std::size_t shards) {
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

// The BM25 weight of a term of document frequency df occurring tf times in the document.
inline double bm25_of(const Statistics& st, const Model::Doc& doc, double df, double tf, double wqf) {
	double tw = (st.docs - df + 0.5) / (df + 0.5);
	if (tw < 2) { tw = tw * 0.5 + 1; }
	const double idf = std::log(tw) * 2.0 * wqf / (1.0 + wqf) * 2.0;
	const double K = 1.0 * (0.5 + 0.5 * std::max(doc.length / (st.length / st.docs), 0.5));
	return idf * tf / (K + tf);
}

inline double bm25(const Statistics& st, const Model::Doc& doc, const std::string& term, double wqf) {
	const auto it = doc.wdf.find(term);
	if (it == doc.wdf.end() || term.empty()) { return 0.0; }
	return bm25_of(st, doc, st.df.at(term), it->second, wqf);
}

// The terms a SYNONYM counts as one weigh as one term: of the size of their union (as independent sets,
// rounded), occurring as often as all of them together, at most the document's length.
inline double synonym_weight(const Statistics& st, const Model::Doc& doc, const std::vector<std::string>& terms) {
	double miss = 1, wdf = 0;
	for (const auto& term : terms) {
		miss *= 1.0 - st.df.at(term) / st.docs;
		const auto it = doc.wdf.find(term);
		wdf += it == doc.wdf.end() ? 0 : it->second;
	}
	const double df = terms.size() == 1 ? st.df.at(terms[0]) : std::floor(st.docs * (1.0 - miss) + 0.5);
	return wdf == 0 ? 0.0 : bm25_of(st, doc, df, std::min<double>(wdf, doc.length), 1.0);
}

// What fills one position of a phrase: where it occurs in the document and what it weighs.
struct Slot {
	std::vector<std::uint32_t> positions;
	double weight = 0;
};

inline std::size_t edit_distance_of(const std::string& a, const std::string& b) {
	std::vector<std::size_t> row(b.size() + 1);
	for (std::size_t j = 0; j <= b.size(); ++j) { row[j] = j; }
	for (std::size_t i = 1; i <= a.size(); ++i) {
		std::size_t diagonal = row[0];
		row[0] = i;
		for (std::size_t j = 1; j <= b.size(); ++j) {
			const std::size_t up = row[j];
			row[j] = std::min({row[j] + 1, row[j - 1] + 1, diagonal + (a[i - 1] == b[j - 1] ? 0 : 1)});
			diagonal = up;
		}
	}
	return row[b.size()];
}

inline void add_positions(const Model::Doc& doc, const std::string& term, std::vector<std::uint32_t>& into) {
	if (const auto it = doc.positions.find(term); it != doc.positions.end()) {
		into.insert(into.end(), it->second.begin(), it->second.end());
	}
}

// The slot a word, an expansion, or an OR or SYNONYM of those fills.
inline Slot slot_in(const Statistics& st, const Model::Doc& doc, const Query& query) {
	const auto& node = *query.node();
	Slot slot;
	std::vector<std::string> members;
	switch (node.kind) {
		case engine::QueryKind::term:
			add_positions(doc, node.str, slot.positions);
			slot.weight = bm25(st, doc, node.str, node.wqf);
			break;
		case engine::QueryKind::wildcard:
		case engine::QueryKind::edit_distance:
			for (const auto& [term, df] : st.df) {
				const bool near = node.kind == engine::QueryKind::wildcard ? term.starts_with(node.str) : edit_distance_of(term, node.str) <= node.distance;
				if (near) { members.push_back(term); }
			}
			break;
		default:
			if (node.op == Op::SYNONYM) {
				for (const auto& sub : node.subqueries) { members.push_back(sub.node()->str); }
			} else {
				for (const auto& sub : node.subqueries) {
					const auto part = slot_in(st, doc, sub);
					slot.positions.insert(slot.positions.end(), part.positions.begin(), part.positions.end());
					slot.weight += part.weight;
				}
			}
	}
	if (!members.empty()) {
		for (const auto& term : members) { add_positions(doc, term, slot.positions); }
		if (node.kind == engine::QueryKind::combine || node.op == Op::SYNONYM) {
			slot.weight = synonym_weight(st, doc, members);
		} else {
			for (const auto& term : members) { slot.weight += bm25(st, doc, term, 1); }
		}
	}
	std::sort(slot.positions.begin(), slot.positions.end());
	slot.positions.erase(std::unique(slot.positions.begin(), slot.positions.end()), slot.positions.end());
	return slot;
}

// A phrase over slots: consecutive positions.
inline bool phrase_of(const std::vector<Slot>& slots) {
	for (const auto start : slots[0].positions) {
		bool all = true;
		for (std::size_t k = 1; k < slots.size() && all; ++k) {
			all = std::binary_search(slots[k].positions.begin(), slots[k].positions.end(), start + static_cast<std::uint32_t>(k));
		}
		if (all) { return true; }
	}
	return false;
}

// One distinct position per slot, spanning fewer than `window` positions.
inline bool near_of(const std::vector<Slot>& slots, std::uint32_t window) {
	std::vector<std::uint32_t> chosen;
	std::function<bool(std::size_t)> place = [&](std::size_t k) {
		if (k == slots.size()) {
			const auto [lo, hi] = std::minmax_element(chosen.begin(), chosen.end());
			return *hi - *lo < window;
		}
		for (const auto position : slots[k].positions) {
			if (std::find(chosen.begin(), chosen.end(), position) != chosen.end()) { continue; }
			chosen.push_back(position);
			if (place(k + 1)) { return true; }
			chosen.pop_back();
		}
		return false;
	};
	return place(0);
}

// Whether the document matches and, if so, its weight.
inline std::pair<bool, double> weigh(const Statistics& st, const Model::Doc& doc, const Query& query) {
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
					if (query.alternatives()) {
						std::vector<Slot> slots;
						double total = 0;
						for (const auto& sub : subs) {
							slots.push_back(slot_in(st, doc, sub));
							total += slots.back().weight;
							if (slots.back().positions.empty()) { return {false, 0.0}; }
						}
						const auto window = node->parameter ? node->parameter : static_cast<std::uint32_t>(slots.size());
						return {node->op == Op::PHRASE ? phrase_of(slots) : near_of(slots, window), total};
					}
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
inline std::map<std::uint64_t, double> model_hits(const Model& model, const Query& query, std::size_t shards) {
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
	std::string serialize() const override { return ""; }
	std::unique_ptr<engine::MatchSpy> unserialize(std::string_view) const override { return std::make_unique<CountingSpy>(); }
	std::string serialize_results() const override { return ""; }
	void merge_results(std::string_view) override { }
	std::string description() const override { return "CountingSpy"; }
};

// --- running a query ---

inline engine::SearchResult run(const Index& index, std::size_t shards, const Query& query, engine::doccount first, engine::doccount maxitems, engine::DocidOrder order = engine::DocidOrder::ascending) {
	auto prepared = engine::prepare(query);
	engine::SearchRequest request;
	request.query = prepared.get();
	request.first = first;
	request.maxitems = maxitems;
	request.check_at_least = 100000;
	request.order = order;
	return engine::sync_wait(engine::search(index.shards(shards), request));
}

inline bool close(double a, double b) {
	return std::fabs(a - b) <= tolerance * std::max(1.0, std::fabs(b));
}

inline Ids ids_of(const engine::SearchResult& result) {
	Ids ids;
	for (const auto& hit : result.hits) {
		ids.insert(hit.did);
	}
	return ids;
}

// One query against the model: the documents, the estimate, the weights, the order and the ranks.
inline void compare(const Index& strata, const Model& model, std::size_t shards, const Query& query) {
	const auto weights = model_hits(model, query, shards);
	Ids expected;
	for (const auto& [id, weight] : weights) { expected.insert(id); }
	const auto s = run(strata, shards, query, 0, 100000);
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
	for (std::size_t i = 0; i < s.hits.size(); ++i) {
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

// The terms a query may use. With `distinct`, no term is used twice in a query and no boolean term is
// used; otherwise a query may repeat terms and use the boolean ones.
struct Terms {
	std::mt19937& rng;
	bool distinct;
	std::set<std::string> used;
	int missing = 0;

	std::string pick() {
		static const char* const pool[] = {"w0", "w1", "w2", "w3", "w4", "w5", "common", "even", "tag0", "tag1", "tag2", "tag3", "tag4", "bool0", "bool1", "bool2", "bool3", "bool4"};
		for (int attempt = 0; attempt < 40; ++attempt) {
			std::string term = pool[rng() % (distinct ? 13 : 18)];   // the first 13 have no boolean terms
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

inline Query random_query(Terms& terms, int depth) {
	auto& rng = terms.rng;
	if (depth == 0 || rng() % 4 == 0) {
		return Query::term("f", terms.pick(), 1 + static_cast<engine::termcount>(rng() % 3));
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
			for (unsigned n = 2 + rng() % 2; n > 0; --n) { subs.push_back(Query::term("f", terms.word())); }
			const auto n = static_cast<engine::termcount>(subs.size());
			if (rng() % 2) {
				return Query::combine(Op::PHRASE, std::move(subs), rng() % 2 ? 0 : n);
			}
			return Query::combine(Op::NEAR, std::move(subs), rng() % 2 ? 0 : n + static_cast<engine::termcount>(rng() % 3));
		}
		default: return Query::append(Op::OR, random_query(terms, depth - 1), random_query(terms, depth - 1));
	}
}


inline void random_queries(const Index& strata, const Model& model, std::size_t shards, unsigned seed, int count, bool distinct) {
	std::mt19937 rng(seed);
	for (int i = 0; i < count; ++i) {
		Terms terms{rng, distinct, {}, 0};
		const auto query = random_query(terms, 3);
		const int before = testh::failures;
		compare(strata, model, shards, query);
		if (testh::failures != before) {
			std::fprintf(stderr, "  query %d: %s\n", i, query.description().c_str());
		}
	}
}

template <typename F>
engine::errc error_kind(F&& f) {
	try {
		f();
	} catch (const engine::Error& error) {
		return error.kind;
	}
	return engine::errc::internal;
}

// shards.cc
void test_one_shard_impl();
void test_positional_impl();
void test_append_impl();
void test_two_shards_impl();
void test_spy_impl();
void test_five_shards_impl();
void test_no_shards_and_empty_impl();

// refusals.cc
void test_refusals_impl();
void test_invalid_shapes_impl();

// documents.cc
void test_writes_and_deletes_impl();
void test_phrase_alternatives_impl();
void test_fields_impl();

// reads.cc
void test_long_chains_impl();
void test_concurrent_searches_impl();
void test_snapshot_consistency_impl();

// views.cc
void test_view_holds_nothing_impl();
void test_pin_after_failed_reopen_impl();

// windows_collapse.cc
void test_huge_window_impl();
void test_percent_infinite_impl();
void test_collapse_impl();

}  // namespace detail
