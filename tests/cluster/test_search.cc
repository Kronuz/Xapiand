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

// Distributed search (src/cluster/search.h) over in-process nodes that hold real strata shards, each
// placement against one node holding the same shards (engine::search over all of them): hits, their
// weights to the bit, counts and every aggregation kind equal. Indexes "a" (3, 5 or 6 shards) and
// "b" (2 shards), with deletes and replaces. Cases: equal, aggregations, expansions, failover.

#include "test_harness.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "aggregations/aggregations.h"
#include "cluster/search.h"
#include "cluster/shard_map_view.h"
#include "database/schema.h"
#include "engine/error.h"
#include "engine_support.h"
#include "msgpack.h"
#include "serialize.h"
#include "serialize_list.h"

namespace {

using engine::Op;
using engine::Query;
using cluster::NodeId;
using cluster::GroupId;

constexpr engine::valueno AGE = 1001, SCORE = 1002, CAT = 1003;

struct Lcg {
	std::uint64_t state;
	std::uint32_t next(std::uint32_t bound) {
		state = state * 6364136223846793005ULL + 1442695040888963407ULL;
		return static_cast<std::uint32_t>((state >> 33) % bound);
	}
};

// ---- the corpus -------------------------------------------------------------------------------

const char* const fruit[] = {"apple", "apply", "ample", "maple", "amply", "apt", "ape", "apex", "grape", "gape", "aple", "apples"};

// Row i of a corpus whose index has `shards` shards. "ux<k>", k = i % (2 * shards), holds two words per shard and
// grows more frequent with k, so a shard's own expansion of "ux" under a limit differs from the index's.
engine::Document make_document(std::uint64_t seed, std::size_t i, std::size_t shards) {
	Lcg rng{seed * 1000003 + i};
	engine::Document doc;
	const auto f = doc.field("f");
	engine::termpos pos = 1;
	for (auto n = 2 + rng.next(6); n > 0; --n) {
		const auto r = rng.next(100);
		if (r < 30) {
			doc.add_posting(f, fruit[rng.next(12)], pos++);
		} else if (r < 40) {
			doc.add_posting(f, "red", pos++);
			doc.add_posting(f, "apple", pos++);
		} else {
			// Zipf-like: w0 is the most common.
			const auto w = rng.next(20);
			doc.add_posting(f, "w" + std::to_string(w * w / 20), pos++);
		}
	}
	const std::size_t ux = 2 * shards;
	const std::size_t k = i % ux;
	if ((i / ux) % (ux - k) == 0) {
		doc.add_posting(f, "ux" + std::to_string(k), pos++);
	}
	std::vector<std::string> ages;
	for (auto n = rng.next(3); n > 0; --n) {
		ages.push_back(Serialize::integer(static_cast<std::int64_t>(rng.next(41)) - 20));
	}
	std::sort(ages.begin(), ages.end());
	ages.erase(std::unique(ages.begin(), ages.end()), ages.end());
	if (!ages.empty()) {
		doc.add_value(AGE, StringList::serialize(ages.begin(), ages.end()));
	}
	if (rng.next(9) != 0) {
		const std::vector<std::string> score{Serialize::floating(static_cast<double>(rng.next(101)) / 4.0 - 12.5)};
		doc.add_value(SCORE, StringList::serialize(score.begin(), score.end()));
	}
	std::vector<std::string> cats{"c" + std::to_string(rng.next(8))};
	doc.add_value(CAT, StringList::serialize(cats.begin(), cats.end()));
	doc.set_data("d");
	return doc;
}

std::shared_ptr<Schema> make_schema() {
	MsgPack props = MsgPack::MAP();
	props["age"] = MsgPack({{"_type", "integer"}, {"_slot", 1001}});
	props["score"] = MsgPack({{"_type", "floating"}, {"_slot", 1002}});
	props["cat"] = MsgPack({{"_type", "keyword"}, {"_slot", 1003}, {"_index", "field_values"}});
	return std::make_shared<Schema>(std::make_shared<const MsgPack>(std::move(props)), nullptr, "");
}

// One index: its rows over `shards` shards (row i to shard i % shards), each shard in `copies` identical
// copies, then rows replaced and erased.
struct Index {
	std::string name;
	std::size_t shards = 0;
	std::vector<std::vector<std::unique_ptr<engine::Shard>>> copies;  // [shard][copy]
};

Index build(const std::string& name, std::uint64_t seed, std::size_t rows, std::size_t shards, std::size_t copies, const std::string& tag) {
	Index index;
	index.name = name;
	index.shards = shards;
	index.copies.resize(shards);
	for (std::size_t s = 0; s < shards; ++s) {
		for (std::size_t c = 0; c < copies; ++c) {
			index.copies[s].push_back(testsupport::scratch_shard("cluster_search_" + tag + "_" + name + std::to_string(shards) + "_" + std::to_string(s) + "_" + std::to_string(c)));
		}
	}
	for (std::size_t i = 0; i < rows; ++i) {
		for (auto& copy : index.copies[i % shards]) {
			auto doc = make_document(seed, i, shards);
			testsupport::add_keyed(*copy, doc, "K" + std::to_string(i));
		}
	}
	for (std::size_t i = 0; i < rows; ++i) {
		for (auto& copy : index.copies[i % shards]) {
			if (i % 7 == 3) {
				auto doc = make_document(seed + 1, i, shards);
				doc.set_key("K" + std::to_string(i));
				engine::sync_wait(copy->replace_by_key("K" + std::to_string(i), doc, {}));
			} else if (i % 11 == 0) {
				engine::sync_wait(copy->erase_by_key("K" + std::to_string(i), true, std::nullopt));
			}
		}
	}
	for (auto& shard : index.copies) {
		for (auto& copy : shard) {
			engine::sync_wait(copy->commit(engine::Transaction::none));
		}
	}
	return index;
}

// ---- the cluster ------------------------------------------------------------------------------

GroupId group_of(std::size_t index, std::size_t shard) {
	return 1000 * (index + 1) + shard + 1;
}

class Node final : public cluster::LocalShards {
public:
	std::map<GroupId, engine::Shard*> held;
	task::task<engine::Shard*> shard(GroupId group, std::uint64_t incarnation) override {
		const auto found = held.find(group);
		co_return found != held.end() && incarnation == 1 ? found->second : nullptr;
	}
};

// Nodes in process. A node in `lost_stats` or `lost_search` dies in that round after doing its work: the
// answer never reaches the coordinator.
class Cluster final : public cluster::Nodes {
public:
	std::map<NodeId, Node> nodes;
	std::set<NodeId> lost_stats, lost_search;
	std::size_t stats_asks = 0, search_asks = 0;

	task::task<std::vector<engine::IndexStatistics>> stats(NodeId node, const cluster::StatsPart& part) override {
		++stats_asks;
		auto answer = co_await cluster::serve_stats(nodes.at(node), part);
		if (lost_stats.contains(node)) {
			throw engine::Error(engine::errc::network, "the connection to node " + std::to_string(node) + " was lost");
		}
		co_return answer;
	}
	task::task<cluster::PartAnswer> search(NodeId node, const cluster::SearchPart& part) override {
		++search_asks;
		auto answer = co_await cluster::serve_search(nodes.at(node), part);
		if (lost_search.contains(node)) {
			throw engine::Error(engine::errc::network, "the connection to node " + std::to_string(node) + " was lost");
		}
		co_return answer;
	}
};

// A placement: how many nodes, and the replicas of each shard (copy r on the r-th node named).
struct Placement {
	const char* name;
	std::size_t nodes;
	std::size_t shards;   // of index "a"; "b" has 2
	std::size_t rf;
	NodeId self;
	std::function<NodeId(std::size_t index, std::size_t shard, std::size_t copy)> node;
};

std::vector<Placement> placements() {
	return {
		{"3 nodes", 3, 3, 1, 1, [](std::size_t x, std::size_t s, std::size_t) { return NodeId((s + x) % 3 + 1); }},
		{"5 nodes, rf 2", 5, 5, 2, 1, [](std::size_t x, std::size_t s, std::size_t c) { return NodeId((s + x + c) % 5 + 1); }},
		{"two shards per node", 3, 6, 1, 2, [](std::size_t x, std::size_t s, std::size_t) { return NodeId((s + x) % 3 + 1); }},
	};
}

// The indexes of a placement, the shard map over them and the nodes that hold them.
struct World {
	explicit World(NodeId self) : map(self) { }
	std::vector<Index> indexes;
	cluster::StaticShardMap map;
	Cluster cluster;
	std::vector<std::string> names;
};

// `test` names the case, so cases run in parallel keep their own shard directories.
std::unique_ptr<World> lay_out(const Placement& placement, const char* test) {
	auto world = std::make_unique<World>(placement.self);
	const std::string tag = std::string(test) + "_" + std::to_string(placement.nodes) + "n" + std::to_string(placement.shards) + "s";
	world->indexes.push_back(build("a", 7, 360, placement.shards, placement.rf, tag));
	world->indexes.push_back(build("b", 8, 120, 2, placement.rf, tag));
	for (std::size_t n = 1; n <= placement.nodes; ++n) {
		world->cluster.nodes[NodeId(n)];
	}
	for (std::size_t x = 0; x < world->indexes.size(); ++x) {
		auto& index = world->indexes[x];
		std::vector<cluster::ShardReplicas> shards;
		for (std::size_t s = 0; s < index.shards; ++s) {
			cluster::ShardReplicas replicas{group_of(x, s), 1, {}};
			for (std::size_t c = 0; c < placement.rf; ++c) {
				const auto node = placement.node(x, s, c);
				replicas.replicas.push_back(node);
				world->cluster.nodes.at(node).held[group_of(x, s)] = index.copies[s][c].get();
			}
			shards.push_back(std::move(replicas));
		}
		world->map.set(index.name, std::move(shards));
		world->names.push_back(index.name);
	}
	return world;
}

// ---- the requests -----------------------------------------------------------------------------

class BySlot final : public engine::KeyMaker {
public:
	explicit BySlot(engine::valueno slot) : slot_(slot) { }
	std::string key(const engine::DocView& doc) const override { return doc.value(slot_); }
	std::unique_ptr<engine::KeyMaker> clone() const override { return std::make_unique<BySlot>(slot_); }
	std::string name() const override { return "BySlot"; }
	std::string serialize() const override { return std::to_string(slot_); }
	std::unique_ptr<engine::KeyMaker> unserialize(std::string_view) const override { return clone(); }

private:
	engine::valueno slot_;
};

struct Shape {
	const char* name;
	engine::doccount first = 0, maxitems = 10;
	bool sorted = false;
	bool collapsed = false;
	engine::DocidOrder order = engine::DocidOrder::ascending;
};

std::vector<Shape> shapes() {
	return {
		{"top 10"},
		{"window 7..19", 7, 12},
		{"count", 0, 0},
		{"all", 0, 1000},
		{"sorted", 0, 25, true},
		{"collapsed", 0, 30, false, true},
		{"sorted, collapsed, descending", 3, 20, true, true, engine::DocidOrder::descending},
	};
}

Query t(const char* term) {
	return Query::term("f", term);
}

std::vector<std::pair<const char*, Query>> queries() {
	return {
		{"term", t("w3")},
		{"or", Query::combine(Op::OR, std::vector<Query>{t("w1"), t("w7"), t("apple")})},
		{"and", Query::combine(Op::AND, t("w0"), t("w2"))},
		{"phrase", Query::combine(Op::PHRASE, std::vector<Query>{Query::term("f", "red", 1, 1), Query::term("f", "apple", 1, 2)})},
		{"partial", Query::wildcard("f", "ap")},
		{"wildcard", Query::wildcard("f", "a*e", 0, engine::Expansion::error, engine::PATTERN_MULTI)},
		{"fuzzy", Query::edit_distance("f", "aple", 0, engine::Expansion::error, Op::SYNONYM, 1, 0)},
		{"and_maybe", Query::combine(Op::AND_MAYBE, t("w0"), Query::wildcard("f", "gr"))},
		{"match_all", Query::match_all()},
		{"no match", t("nothing")},
	};
}

// The searches whose expansion limits differ between one shard and the index.
std::vector<std::pair<const char*, Query>> limited() {
	return {
		{"ux first 2", Query::wildcard("f", "ux", 2, engine::Expansion::first)},
		{"ux most_frequent 2", Query::wildcard("f", "ux", 2, engine::Expansion::most_frequent)},
		{"ux most_frequent 3 in an OR", Query::combine(Op::OR, t("w1"), Query::wildcard("f", "ux", 3, engine::Expansion::most_frequent))},
		{"fuzzy ux1 most_frequent 2", Query::edit_distance("f", "ux1", 2, engine::Expansion::most_frequent, Op::SYNONYM, 1, 0)},
		{"partial ux1 first 1", Query::wildcard("f", "ux1", 1, engine::Expansion::first)},
	};
}

engine::SearchRequest make_request(const engine::PreparedQuery& query, const Shape& shape, const engine::KeyMaker* sorter, engine::MatchSpy* spy) {
	engine::SearchRequest request;
	request.query = &query;
	request.first = shape.first;
	request.maxitems = shape.maxitems;
	request.check_at_least = engine::CHECK_ALL;
	request.sorter = shape.sorted ? sorter : nullptr;
	request.collapse_key = shape.collapsed ? CAT : engine::BAD_VALUENO;
	request.collapse_max = shape.collapsed ? 1 : 0;
	request.order = shape.order;
	request.spy = spy;
	return request;
}

// One node holding copy 0 of every shard: engine::search over all of them, as a node's search runs.
engine::SearchResult one_node(const World& world, engine::SearchRequest request, std::optional<std::pair<std::size_t, std::size_t>> without = std::nullopt) {
	std::vector<engine::Shard*> shards;
	std::vector<std::uint32_t> index_of;
	for (std::size_t x = 0; x < world.indexes.size(); ++x) {
		for (std::size_t s = 0; s < world.indexes[x].shards; ++s) {
			if (without && without->first == x && without->second == s) {
				continue;
			}
			shards.push_back(world.indexes[x].copies[s][0].get());
			index_of.push_back(static_cast<std::uint32_t>(x));
		}
	}
	request.index_of = index_of;
	return engine::sync_wait(engine::search(shards, request));
}

cluster::FanoutResult fan_out(World& world, const engine::SearchRequest& request) {
	return engine::sync_wait(cluster::search(world.map, world.cluster, world.names, request));
}

std::uint64_t bits(double x) {
	std::uint64_t b;
	std::memcpy(&b, &x, sizeof b);
	return b;
}

std::size_t g_compared = 0;

// Hits (ids, ranks, percents, weights to the bit), counts and the top weight.
void check_same(const engine::SearchResult& want, const engine::SearchResult& got, const std::string& label) {
	++g_compared;
	bool same = want.estimated == got.estimated && bits(want.top) == bits(got.top) && want.hits.size() == got.hits.size();
	for (std::size_t i = 0; same && i < want.hits.size(); ++i) {
		const auto& a = want.hits[i];
		const auto& b = got.hits[i];
		same = a.did == b.did && a.rank == b.rank && a.percent == b.percent && bits(a.weight) == bits(b.weight);
	}
	CHECK_TRUE(same);
	if (!same) {
		std::fprintf(stderr, "  DIFF %s: estimated %u vs %u, %zu vs %zu hits\n", label.c_str(), want.estimated, got.estimated, want.hits.size(), got.hits.size());
		for (std::size_t i = 0; i < std::max(want.hits.size(), got.hits.size()) && i < 5; ++i) {
			const auto* a = i < want.hits.size() ? &want.hits[i] : nullptr;
			const auto* b = i < got.hits.size() ? &got.hits[i] : nullptr;
			std::fprintf(stderr, "    %zu: %llu %.17g | %llu %.17g\n", i, a ? (unsigned long long)a->did : 0ULL, a ? a->weight : 0.0, b ? (unsigned long long)b->did : 0ULL, b ? b->weight : 0.0);
		}
	}
}

void check_whole(const cluster::FanoutResult& got, const World& world, const std::string& label) {
	std::size_t total = 0;
	for (const auto& index : world.indexes) {
		total += index.shards;
	}
	CHECK_EQ(got.total, total);
	CHECK_EQ(got.successful, total);
	CHECK_TRUE(got.failed.empty());
	if (!got.failed.empty()) {
		std::fprintf(stderr, "  FAILED %s: %s shard %zu: %s\n", label.c_str(), got.failed[0].index.c_str(), got.failed[0].shard, got.failed[0].reason.c_str());
	}
}

// ---- aggregations -----------------------------------------------------------------------------

MsgPack kind(const char* name, const char* field, const std::vector<std::pair<const char*, MsgPack>>& options = {}) {
	MsgPack inner = MsgPack::MAP();
	inner["_field"] = field;
	for (const auto& [key, value] : options) {
		inner[key] = value;
	}
	MsgPack body = MsgPack::MAP();
	body[name] = std::move(inner);
	return body;
}

MsgPack with(MsgPack body, std::vector<std::pair<const char*, MsgPack>> subs) {
	MsgPack aggs = MsgPack::MAP();
	for (auto& [name, sub] : subs) {
		aggs[name] = std::move(sub);
	}
	body["_aggs"] = std::move(aggs);
	return body;
}

MsgPack range(const char* key, std::optional<double> from, std::optional<double> to) {
	MsgPack r = MsgPack::MAP();
	r["_key"] = key;
	if (from) { r["_from"] = *from; }
	if (to) { r["_to"] = *to; }
	return r;
}

// Every kind Xapiand serves in one request, several nested.
MsgPack every_kind() {
	MsgPack aggs = MsgPack::MAP();
	const MsgPack all = MsgPack(1000);
	for (const char* metric : {"_count", "_sum", "_avg", "_min", "_max", "_variance", "_std_deviation", "_median", "_mode", "_stats", "_extended_stats"}) {
		for (const char* field : {"age", "score"}) {
			aggs[std::string(metric + 1) + "_" + field] = kind(metric, field);  // a name never starts with '_'
		}
	}
	aggs["values_cat"] = kind("_values", "cat", {{"_limit", all}});
	aggs["values_age"] = kind("_values", "age", {{"_limit", all}});
	aggs["terms_cat"] = with(kind("_terms", "cat", {{"_limit", all}}), {{"s", kind("_stats", "score")}});
	aggs["terms_age"] = kind("_terms", "age", {{"_limit", all}});
	aggs["histogram_score"] = with(kind("_histogram", "score", {{"_interval", MsgPack(2.5)}, {"_limit", all}}), {{"m", kind("_median", "age")}});
	aggs["histogram_age"] = kind("_histogram", "age", {{"_interval", MsgPack(4)}, {"_shift", MsgPack(1)}, {"_limit", all}});
	MsgPack ranges = MsgPack::ARRAY();
	ranges.push_back(range("lo", std::nullopt, -2.5));
	ranges.push_back(range("mid", -2.5, 5.0));
	ranges.push_back(range("hi", 5.0, std::nullopt));
	aggs["range_score"] = with(kind("_range", "score", {{"_ranges", ranges}}), {{"t", kind("_terms", "cat", {{"_limit", all}})}});
	MsgPack term = MsgPack::MAP();
	term["cat"] = MsgPack("c3");
	MsgPack filter = MsgPack::MAP();
	filter["_filter"] = MsgPack({{"_term", term}});
	aggs["filter_cat"] = with(filter, {{"e", kind("_extended_stats", "age")}, {"v", kind("_values", "age", {{"_limit", all}})}});
	MsgPack config = MsgPack::MAP();
	config["_aggs"] = aggs;
	return config;
}

// The values that differ between two results, by path; floating values to one part in 1e9, as the
// aggregation merge's own parity test allows (tests/test_aggregation_shards.cc).
void compare(const MsgPack& a, const MsgPack& b, const std::string& path, std::vector<std::string>& diff) {
	if (a.is_map() && b.is_map()) {
		std::map<std::string, MsgPack> left, right;
		for (auto it = a.begin(); it != a.end(); ++it) { left.emplace(std::string(it->str_view()), it.value()); }
		for (auto it = b.begin(); it != b.end(); ++it) { right.emplace(std::string(it->str_view()), it.value()); }
		if (left.size() != right.size()) {
			diff.push_back(path + " (keys differ)");
		}
		for (const auto& [key, value] : left) {
			const auto other = right.find(key);
			if (other == right.end()) { diff.push_back(path + "/" + key + " (missing)"); continue; }
			compare(value, other->second, path + "/" + key, diff);
		}
	} else if (a.is_array() && b.is_array()) {
		if (a.size() != b.size()) {
			diff.push_back(path + " (" + std::to_string(a.size()) + " vs " + std::to_string(b.size()) + ")");
			return;
		}
		for (std::size_t i = 0; i < a.size(); ++i) {
			compare(a.at(i), b.at(i), path + "[" + std::to_string(i) + "]", diff);
		}
	} else if (a.is_number() && b.is_number() && !(a.is_integer() && b.is_integer())) {
		const double x = a.as_f64(), y = b.as_f64();
		if (std::fabs(x - y) / std::max({1.0, std::fabs(x), std::fabs(y)}) > 1e-9) { diff.push_back(path + " " + a.to_string() + " vs " + b.to_string()); }
	} else if (a.to_string() != b.to_string()) {
		diff.push_back(path + " " + a.to_string() + " vs " + b.to_string());
	}
}

void check_aggregations(const MsgPack& want, const MsgPack& got, const std::string& label) {
	std::vector<std::string> diff;
	compare(want, got, "", diff);
	CHECK_TRUE(diff.empty());
	if (!diff.empty()) {
		std::fprintf(stderr, "  DIFF %s: %zu values differ, first %s\n", label.c_str(), diff.size(), diff.front().c_str());
	}
}

}  // namespace

// Every query and shape, every placement: the cluster's answer is one node's.
static void test_equal() {
	const BySlot sorter(SCORE);
	for (const auto& placement : placements()) {
		auto world = lay_out(placement, "equal");
		for (const auto& [name, query] : queries()) {
			const auto prepared = engine::prepare(query);
			for (const auto& shape : shapes()) {
				const auto label = std::string(placement.name) + " | " + name + " | " + shape.name;
				const auto request = make_request(*prepared, shape, &sorter, nullptr);
				const auto want = one_node(*world, request);
				const auto got = fan_out(*world, request);
				check_same(want, got.result, label);
				check_whole(got, *world, label);
			}
		}
	}
	std::fprintf(stderr, "  compared %zu searches\n", g_compared);
	CHECK_GT(g_compared, std::size_t{150});
}

// Every aggregation kind, over every placement and a query that matches a part of the corpus.
static void test_aggregations() {
	const auto schema = make_schema();
	const auto config = every_kind();
	for (const auto& placement : placements()) {
		auto world = lay_out(placement, "aggregations");
		for (const auto& [name, query] : std::vector<std::pair<const char*, Query>>{{"match_all", Query::match_all()}, {"partial", Query::wildcard("f", "ap")}, {"or", Query::combine(Op::OR, t("w0"), t("red"))}}) {
			const auto prepared = engine::prepare(query);
			const auto label = std::string(placement.name) + " | " + name;
			AggregationMatchSpy want_spy(config, schema), got_spy(config, schema);
			const Shape shape{"aggs", 0, 5};
			const auto want = one_node(*world, make_request(*prepared, shape, nullptr, &want_spy));
			const auto got = fan_out(*world, make_request(*prepared, shape, nullptr, &got_spy));
			check_same(want, got.result, label);
			check_whole(got, *world, label);
			check_aggregations(want_spy.get_aggregation(), got_spy.get_aggregation(), label);
			CHECK_GT(want_spy.get_aggregation().size(), std::size_t{10});
		}
	}
}

// Wildcard, partial and fuzzy terms expand once, at the coordinator, over the whole index: a limit
// keeps the index's first or most frequent terms, not each shard's, and `error` sees the index's count.
static void test_expansions() {
	for (const auto& placement : placements()) {
		auto world = lay_out(placement, "expansions");
		for (const auto& [name, query] : limited()) {
			const auto prepared = engine::prepare(query);
			for (const auto& shape : {Shape{"top 10"}, Shape{"all", 0, 1000}}) {
				const auto label = std::string(placement.name) + " | " + name + " | " + shape.name;
				const auto request = make_request(*prepared, shape, nullptr, nullptr);
				const auto want = one_node(*world, request);
				const auto got = fan_out(*world, request);
				check_same(want, got.result, label);
				check_whole(got, *world, label);
			}
		}
		// Each shard holds two "ux" terms; the index holds more than two, which `error` refuses.
		const auto refused = engine::prepare(Query::wildcard("f", "ux", 2, engine::Expansion::error));
		const auto request = make_request(*refused, Shape{"top 10"}, nullptr, nullptr);
		bool one_refused = false, cluster_refused = false;
		try { one_node(*world, request); } catch (const engine::Error& e) { one_refused = e.kind == engine::errc::invalid; }
		try { fan_out(*world, request); } catch (const engine::Error& e) { cluster_refused = e.kind == engine::errc::invalid; }
		CHECK_TRUE(one_refused);
		CHECK_TRUE(cluster_refused);
	}
}

// A replica lost in either round is replaced by the shard's other one, and the answer is still one
// node's (a lost part's aggregations are never merged); with no replica left, the shard is named.
static void test_failover() {
	const auto schema = make_schema();
	const auto config = every_kind();
	const auto all = placements();
	const auto& placement = all[1];   // 5 nodes, rf 2
	auto world = lay_out(placement, "failover");
	const auto prepared = engine::prepare(Query::combine(Op::OR, t("w0"), Query::wildcard("f", "ap")));
	const Shape shape{"top 20", 0, 20};
	AggregationMatchSpy want_spy(config, schema);
	const auto want = one_node(*world, make_request(*prepared, shape, nullptr, &want_spy));

	for (const char* round : {"statistics", "search"}) {
		for (NodeId lost = 1; lost <= 5; ++lost) {
			const auto label = std::string("lost node ") + std::to_string(lost) + " in the " + round + " round";
			world->cluster.lost_stats.clear();
			world->cluster.lost_search.clear();
			(std::strcmp(round, "statistics") == 0 ? world->cluster.lost_stats : world->cluster.lost_search).insert(lost);
			AggregationMatchSpy got_spy(config, schema);
			const auto got = fan_out(*world, make_request(*prepared, shape, nullptr, &got_spy));
			check_same(want, got.result, label);
			check_whole(got, *world, label);
			check_aggregations(want_spy.get_aggregation(), got_spy.get_aggregation(), label);
		}
	}

	// Both replicas of a.shard 3 (nodes 4 and 5) lost in the statistics round: shard 3 is named, and the rest
	// answers as one node holding the other shards (node 5's other shards go to their first replica).
	world->cluster.lost_search.clear();
	world->cluster.lost_stats = {4, 5};
	const auto match_all = engine::prepare(Query::match_all());
	AggregationMatchSpy rest_spy(config, schema), got_spy(config, schema);
	const auto rest = one_node(*world, make_request(*match_all, Shape{"count", 0, 0}, nullptr, &rest_spy), std::pair<std::size_t, std::size_t>{0, 3});
	const auto got = fan_out(*world, make_request(*match_all, Shape{"count", 0, 0}, nullptr, &got_spy));
	CHECK_TRUE(got.partial());
	CHECK_EQ(got.total, std::size_t{7});
	CHECK_EQ(got.successful, std::size_t{6});
	CHECK_EQ(got.failed.size(), std::size_t{1});
	if (got.failed.size() == 1) {
		CHECK_EQ(got.failed[0].index, std::string("a"));
		CHECK_EQ(got.failed[0].shard, std::size_t{3});
		CHECK_EQ(got.failed[0].node, NodeId{5});
		CHECK_TRUE(got.failed[0].reason.find("was lost") != std::string::npos);
	}
	CHECK_EQ(got.result.estimated, rest.estimated);
	check_aggregations(rest_spy.get_aggregation(), got_spy.get_aggregation(), "a.3 lost");

	// Lost in the search round, after its statistics were counted: still named, the rest still served.
	world->cluster.lost_stats.clear();
	world->cluster.lost_search = {4, 5};
	const auto late = fan_out(*world, make_request(*match_all, Shape{"count", 0, 0}, nullptr, nullptr));
	CHECK_EQ(late.failed.size(), std::size_t{1});
	CHECK_EQ(late.result.estimated, rest.estimated);

	// A node marked down is asked last: its shards go to their other replica first, so a dead node 4
	// costs no ask and no retry (nodes 1, 2, 3 and 5 are asked once each).
	world->cluster.lost_search.clear();
	world->map.set_down(4, true);
	world->cluster.lost_stats = {4};
	const auto asks = world->cluster.stats_asks;
	const auto down = fan_out(*world, make_request(*prepared, shape, nullptr, nullptr));
	CHECK_TRUE(down.failed.empty());
	CHECK_EQ(world->cluster.stats_asks - asks, std::size_t{4});
	check_same(want, down.result, "node 4 down");
	world->map.set_down(4, false);
	world->cluster.lost_stats.clear();
}

int main(int argc, char** argv) {
	struct Case {
		const char* name;
		void (*run)();
	};
	const Case cases[] = {{"equal", test_equal}, {"aggregations", test_aggregations}, {"expansions", test_expansions}, {"failover", test_failover}};
	for (const auto& c : cases) {
		if (argc > 1 && std::strcmp(argv[1], c.name) != 0) {
			continue;
		}
		std::printf("%s\n", c.name);
		try {
			c.run();
		} catch (const engine::Error& e) {
			std::fprintf(stderr, "  %s threw %s: %s\n", c.name, engine::to_string(e.kind), e.description().c_str());
			++testh::failures;
		} catch (const std::exception& e) {
			std::fprintf(stderr, "  %s threw: %s\n", c.name, e.what());
			++testh::failures;
		}
	}
	if (testh::failures) {
		std::fprintf(stderr, "%d of %d checks FAILED\n", testh::failures, testh::checks);
		return 1;
	}
	std::printf("OK: %d checks passed\n", testh::checks);
	return 0;
}
