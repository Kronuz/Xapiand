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

// Value slots (slot 1 is the shard's own version slot).
constexpr engine::valueno KEY = 21, GROUP = 22, NUMBER = 23, EMPTY = 29;

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

struct Spec {
	std::map<std::string, unsigned> terms;
	std::string key, group, number, data;
	bool has_number = true;
	bool live = true;
};

struct Corpus {
	Index strata;
	Index merged;   // every document of every shard in one shard: the whole-index statistics
	std::vector<std::vector<Spec>> specs;
};

// The terms a wildcard or an edit-distance node stands for, within its limit; none and `error` set when
// there are more than the limit allows under the `error` policy.
struct Expansion {
	std::set<std::string> terms;
	bool error = false;
};

// The stored data of the live documents of the first `shards` shards that the query matches; `error` when an
// expansion past its limit under the `error` policy fails the search.
struct Expected {
	std::set<std::string> datas;
	bool error = false;
};

using Tweak = std::function<void(engine::SearchRequest&)>;

std::string key_of(int shard, int i);

engine::Document make(int shard, int i, const Spec& spec);

// The documents on `shard_count` shards, `commits` commits per shard (several segments), and all of them in one more shard.
void build(const char* name, int shard_count, int count, int commits, Corpus& corpus);

Expected expected_of(const Corpus& corpus, std::size_t shards, const Query& query);

engine::SearchResult run(const Index& index, std::size_t shards, const Query& query, const Tweak& tweak = [](engine::SearchRequest&) { });

bool close(double a, double b);

// A search checked against the model: the documents and the estimate are the model's, the ranking is by
// weight, and (when `weights`) the weights are those of one shard holding every document. With a window
// the hits are that slice of the full ranking.
engine::SearchResult both(const Corpus& corpus, std::size_t shards, const Query& query, const char* what, const Tweak& tweak = [](engine::SearchRequest&) { }, bool weights = true);

// Like both(), but for searches whose terms weigh with statistics that are not the whole index's on
// several shards (a wildcard or edit-distance expansion): the documents and the estimate are the model's,
// and the weights those of the search of one shard holding every document.
void same_documents(const Corpus& corpus, std::size_t shards, const Query& query, const char* what);

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

Query t(const char* term, engine::termcount wqf = 1);

// The key of the document with this id in the search over `shards` shards.
std::string key_value(const Index& index, std::size_t shards, engine::docid did);

std::string group_value(const Index& index, std::size_t shards, engine::docid did);

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
