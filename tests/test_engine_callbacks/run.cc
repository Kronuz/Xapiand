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

// Running a search and comparing it with the model: the documents, the estimate, the weights and the ranking.

#include "support.h"

namespace detail {

// --- running and comparing ---

constexpr double tolerance = 1e-9;

bool close(double a, double b) {
	return std::fabs(a - b) <= tolerance * std::max(1.0, std::fabs(b));
}

namespace {

// The stored data of a document of the search over `shards` shards of the index.
std::string data_of(const Index& index, std::size_t shards, engine::docid did) {
	const std::size_t shard = (did - 1) % shards;
	const auto local = static_cast<engine::docid>((did - 1) / shards + 1);
	return engine::sync_wait(index.owned[shard]->get(local))->data();
}

}  // namespace

namespace {

// The documents' weights by stored data, for comparing indexes that number their documents differently.
std::map<std::string, double> weights_by_data(const Index& index, std::size_t shards, const engine::SearchResult& result) {
	std::map<std::string, double> weights;
	for (const auto& hit : result.hits) {
		weights[data_of(index, shards, hit.did)] = hit.weight;
	}
	return weights;
}

}  // namespace

namespace {

std::set<std::string> datas_of(const Index& index, std::size_t shards, const engine::SearchResult& result) {
	std::set<std::string> datas;
	for (const auto& [data, weight] : weights_by_data(index, shards, result)) { datas.insert(data); }
	return datas;
}

}  // namespace

namespace {

// The ranks and the order of the hits of a full search: weight order, ranks counting from zero.
void check_ranking(const engine::SearchResult& result) {
	for (std::size_t i = 0; i < result.hits.size(); ++i) {
		CHECK_EQ(result.hits[i].rank, i);
		if (i > 0) { CHECK_GE(result.hits[i - 1].weight + 1e-12, result.hits[i].weight); }
	}
}

}  // namespace

engine::SearchResult run(const Index& index, std::size_t shards, const Query& query, const Tweak& tweak) {
	auto prepared = engine::prepare(query);
	engine::SearchRequest request;
	request.query = prepared.get();
	request.maxitems = 100000;
	request.check_at_least = 100000;
	tweak(request);
	return engine::sync_wait(engine::search(index.shards(shards), request));
}

// A search checked against the model: the documents and the estimate are the model's, the ranking is by
// weight, and (when `weights`) the weights are those of one shard holding every document. With a window
// the hits are that slice of the full ranking.
engine::SearchResult both(const Corpus& corpus, std::size_t shards, const Query& query, const char* what, const Tweak& tweak, bool weights) {
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
void same_documents(const Corpus& corpus, std::size_t shards, const Query& query, const char* what) {
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

Query t(const char* term, engine::termcount wqf) {
	return Query::term("f", term, wqf);
}

// The key of the document with this id in the search over `shards` shards.
std::string key_value(const Index& index, std::size_t shards, engine::docid did) {
	const std::size_t shard = (did - 1) % shards;
	return engine::sync_wait(index.owned[shard]->get(static_cast<engine::docid>((did - 1) / shards + 1)))->value(KEY);
}

std::string group_value(const Index& index, std::size_t shards, engine::docid did) {
	const std::size_t shard = (did - 1) % shards;
	return engine::sync_wait(index.owned[shard]->get(static_cast<engine::docid>((did - 1) / shards + 1)))->value(GROUP);
}

}  // namespace detail
