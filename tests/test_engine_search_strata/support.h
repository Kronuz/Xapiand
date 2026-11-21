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


using Ids = std::set<std::uint64_t>;

struct Statistics {
	double docs = 0, length = 0;
	std::map<std::string, double> df;
};

std::string path_of(const char* name, int shard);

engine::Document make(int shard, int i, const Model::Doc& model);

Model::Doc random_doc(std::mt19937& rng, int i);

// An index holding the model's documents; `commits` splits each shard's load into that many commits,
// which leaves several segments.
// `host` opens the shards in host mode: their writes take turns (the caller's), a read sees what the last
// refresh flushed, and nothing a reader holds blocks a write.
void build(const char* name, int shard_count, int count, int commits, Index& strata, Model& model, bool host = false);

Statistics statistics_of(const Model& model, std::size_t shards);

// Whether the document matches and, if so, its weight.
std::pair<bool, double> weigh(const Statistics& st, const Model::Doc& doc, const Query& query);

// The ids of the matching documents and their weights.
std::map<std::uint64_t, double> model_hits(const Model& model, const Query& query, std::size_t shards);

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

engine::SearchResult run(const Index& index, std::size_t shards, const Query& query, engine::doccount first, engine::doccount maxitems, engine::DocidOrder order = engine::DocidOrder::ascending);

bool close(double a, double b);

// One query against the model: the documents, the estimate, the weights, the order and the ranks.
void compare(const Index& strata, const Model& model, std::size_t shards, const Query& query);

void random_queries(const Index& strata, const Model& model, std::size_t shards, unsigned seed, int count, bool distinct);


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
