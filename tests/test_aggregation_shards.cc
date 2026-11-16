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

// One corpus indexed into 1, 2 and 5 shards, with deletes, answers every aggregation kind Xapiand serves
// with the same values: through one spy over all the shards (the search path), through per-shard spies
// merged from their serialised results (the remote path) and merged as objects. The kinds, and what
// differs by design, are in docs/dev/agg-shard-parity.md.
//
// AGG_PARITY_DROP_SHARD=k leaves shard k out of every n-shard run (the mutant that proves the test sees it).
// AGG_PARITY_ORDER_INSENSITIVE=1 compares buckets by key, not by position (a diagnosis; the test compares positions).

#include "test_harness.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "aggregations/aggregations.h"
#include "database/schema.h"
#include "engine/callbacks.h"
#include "engine/search.h"
#include "engine_support.h"
#include "msgpack.h"
#include "serialise.h"
#include "serialise_list.h"

namespace {

constexpr engine::valueno AGE = 1001, SCORE = 1002, CAT = 1003, UNIQ = 1004;

struct Lcg {
	std::uint64_t state;
	std::uint32_t next(std::uint32_t bound) {
		state = state * 6364136223846793005ULL + 1442695040888963407ULL;
		return static_cast<std::uint32_t>((state >> 33) % bound);
	}
};

// One document: age (integer, several values or none), score (floating, one value or none, a multiple of
// 0.25 so sums are exact in any order), cat (keyword, one to three values) and uniq (keyword).
struct Row {
	std::vector<std::int64_t> ages;
	std::vector<double> scores;
	std::vector<std::string> cats;
	std::string uniq;
};

std::vector<Row> main_rows() {
	std::vector<Row> rows;
	Lcg rng{11};
	for (int i = 0; i < 240; ++i) {
		Row row;
		if (i % 7 != 0) {
			for (auto n = 1 + rng.next(3); n > 0; --n) {
				row.ages.push_back(static_cast<std::int64_t>(rng.next(41)) - 20);
			}
			std::sort(row.ages.begin(), row.ages.end());
			row.ages.erase(std::unique(row.ages.begin(), row.ages.end()), row.ages.end());
		}
		if (i % 9 != 0) {
			row.scores.push_back(static_cast<double>(rng.next(101)) / 4.0 - 12.5);
		}
		for (auto n = 1 + rng.next(3); n > 0; --n) {
			row.cats.push_back("c" + std::to_string(rng.next(12)));
		}
		std::sort(row.cats.begin(), row.cats.end());
		row.cats.erase(std::unique(row.cats.begin(), row.cats.end()), row.cats.end());
		rows.push_back(std::move(row));
	}
	return rows;
}

// Hot values with well separated counts among many values seen once: past `_shard_size` the counts are approximate.
std::vector<Row> heavy_rows() {
	const std::pair<const char*, int> hot[] = { { "h0", 200 }, { "h1", 150 }, { "h2", 100 }, { "h3", 60 }, { "h4", 40 } };
	std::vector<std::string> values;
	for (const auto& h : hot) {
		values.insert(values.end(), static_cast<std::size_t>(h.second), h.first);
	}
	for (int i = 0; i < 350; ++i) {
		values.push_back("u" + std::to_string(i));
	}
	Lcg rng{5};
	for (auto i = values.size(); i > 1; --i) {
		std::swap(values[i - 1], values[rng.next(static_cast<std::uint32_t>(i))]);
	}
	std::vector<Row> rows;
	for (auto& value : values) {
		Row row;
		row.uniq = std::move(value);
		rows.push_back(std::move(row));
	}
	return rows;
}

std::shared_ptr<Schema> make_schema() {
	MsgPack props = MsgPack::MAP();
	props["age"] = MsgPack({ { "_type", "integer" }, { "_slot", 1001 } });
	props["score"] = MsgPack({ { "_type", "floating" }, { "_slot", 1002 } });
	props["cat"] = MsgPack({ { "_type", "keyword" }, { "_slot", 1003 }, { "_index", "field_values" } });
	props["uniq"] = MsgPack({ { "_type", "keyword" }, { "_slot", 1004 }, { "_index", "field_values" } });
	return std::make_shared<Schema>(std::make_shared<const MsgPack>(std::move(props)), nullptr, "");
}

std::string multi(const std::vector<std::string>& values) {
	return StringList::serialise(values.begin(), values.end());
}

engine::Document make_document(const Row& row) {
	engine::Document doc;
	doc.add_term(doc.field("f"), "t");
	if (!row.ages.empty()) {
		std::vector<std::string> values;
		for (auto n : row.ages) {
			values.push_back(Serialise::integer(n));
		}
		doc.add_value(AGE, multi(values));
	}
	if (!row.scores.empty()) {
		std::vector<std::string> values;
		for (auto x : row.scores) {
			values.push_back(Serialise::floating(static_cast<long double>(x)));
		}
		doc.add_value(SCORE, multi(values));
	}
	if (!row.cats.empty()) {
		doc.add_value(CAT, multi(row.cats));
	}
	if (!row.uniq.empty()) {
		doc.add_value(UNIQ, multi({ row.uniq }));
	}
	doc.set_data("d");
	return doc;
}

// The rows over `n` shards (row i to shard i % n), then the rows `dead` says erased.
struct Layout {
	std::size_t n = 0;
	std::vector<std::unique_ptr<engine::Shard>> shards;
};

Layout build(const std::vector<Row>& rows, std::size_t n, const std::function<bool(std::size_t)>& dead, const std::string& tag) {
	Layout layout;
	layout.n = n;
	for (std::size_t s = 0; s < n; ++s) {
		layout.shards.push_back(testsupport::scratch_shard("agg_parity_" + tag + "_" + std::to_string(n) + "_" + std::to_string(s)));
	}
	for (std::size_t i = 0; i < rows.size(); ++i) {
		auto doc = make_document(rows[i]);
		testsupport::add_keyed(*layout.shards[i % n], doc, "K" + std::to_string(i));
	}
	for (std::size_t i = 0; i < rows.size(); ++i) {
		if (dead(i)) {
			engine::sync_wait(layout.shards[i % n]->erase_by_key("K" + std::to_string(i), true, std::nullopt));
		}
	}
	for (auto& shard : layout.shards) {
		engine::sync_wait(shard->commit(engine::Transaction::none));
	}
	return layout;
}

// The shard the mutant leaves out of an n-shard run, or -1.
long dropped_shard() {
	const char* env = std::getenv("AGG_PARITY_DROP_SHARD");
	return env != nullptr ? std::atol(env) : -1;
}

bool is_dropped(std::size_t shard) {
	return dropped_shard() >= 0 && static_cast<std::size_t>(dropped_shard()) == shard;
}

void search_all(std::vector<engine::Shard*> shards, engine::MatchSpy& spy) {
	testsupport::Find find;
	find.maxitems = 1;
	find.check_at_least = engine::CHECK_ALL;
	find.spy = &spy;
	testsupport::search(std::move(shards), engine::Query::match_all(), find);
}

// One spy sees the documents of all the shards, as a search over them does.
MsgPack by_one_spy(const Layout& layout, const MsgPack& config, const std::shared_ptr<Schema>& schema) {
	std::vector<engine::Shard*> shards;
	for (std::size_t s = 0; s < layout.n; ++s) {
		if (!is_dropped(s)) {
			shards.push_back(layout.shards[s].get());
		}
	}
	AggregationMatchSpy spy(config, schema);
	search_all(shards, spy);
	return spy.get_aggregation();
}

// A spy per shard, their serialised results merged (`as_bytes`) or merged as objects.
MsgPack by_merge(const Layout& layout, const MsgPack& config, const std::shared_ptr<Schema>& schema, bool as_bytes) {
	AggregationMatchSpy merged(config, schema);
	for (std::size_t s = 0; s < layout.n; ++s) {
		if (is_dropped(s)) {
			continue;
		}
		AggregationMatchSpy part(config, schema);
		search_all({ layout.shards[s].get() }, part);
		if (as_bytes) {
			merged.merge_results(part.serialise_results());
		} else {
			merged.merge_results(part);
		}
	}
	return merged.get_aggregation();
}

// The values that differ between two results, by path; floating values to one part in 1e9.
struct Diff {
	std::vector<std::string> paths;
	double worst = 0;
	std::size_t inexact = 0;
};

void compare(const MsgPack& a, const MsgPack& b, const std::string& path, Diff& diff) {
	if (a.is_map() && b.is_map()) {
		std::map<std::string, MsgPack> left, right;
		for (auto it = a.begin(); it != a.end(); ++it) { left.emplace(std::string(it->str_view()), it.value()); }
		for (auto it = b.begin(); it != b.end(); ++it) { right.emplace(std::string(it->str_view()), it.value()); }
		for (const auto& [key, value] : left) {
			const auto other = right.find(key);
			if (other == right.end()) { diff.paths.push_back(path + "/" + key + " (missing)"); continue; }
			compare(value, other->second, path + "/" + key, diff);
		}
		for (const auto& entry : right) {
			if (left.find(entry.first) == left.end()) { diff.paths.push_back(path + "/" + entry.first + " (extra)"); }
		}
	} else if (a.is_array() && b.is_array()) {
		if (a.size() != b.size()) {
			diff.paths.push_back(path + " (" + std::to_string(a.size()) + " buckets vs " + std::to_string(b.size()) + ")");
			return;
		}
		std::vector<MsgPack> left, right;
		for (const auto& v : a) { left.push_back(v); }
		for (const auto& v : b) { right.push_back(v); }
		if (std::getenv("AGG_PARITY_ORDER_INSENSITIVE") != nullptr) {  // a diagnosis: do the buckets agree but for their order
			auto by_key = [](const MsgPack& x, const MsgPack& y) { return x.is_map() && y.is_map() && x.at("_key").to_string() < y.at("_key").to_string(); };
			std::sort(left.begin(), left.end(), by_key);
			std::sort(right.begin(), right.end(), by_key);
		}
		for (std::size_t i = 0; i < left.size(); ++i) {
			compare(left[i], right[i], path + "[" + std::to_string(i) + "]", diff);
		}
	} else if (a.is_number() && b.is_number()) {
		if (a.is_integer() && b.is_integer()) {
			if (a.to_string() != b.to_string()) { diff.paths.push_back(path + " " + a.to_string() + " vs " + b.to_string()); }
			return;
		}
		const double x = a.as_f64(), y = b.as_f64();
		const double scale = std::max({ 1.0, std::fabs(x), std::fabs(y) });
		const double rel = std::fabs(x - y) / scale;
		if (rel != 0) { ++diff.inexact; diff.worst = std::max(diff.worst, rel); }
		if (rel > 1e-9) { diff.paths.push_back(path + " " + a.to_string() + " vs " + b.to_string()); }
	} else if (a.to_string() != b.to_string()) {
		diff.paths.push_back(path + " " + a.to_string() + " vs " + b.to_string());
	}
}

// ---- the aggregation requests, one per kind (and shape) Xapiand serves ----

struct Spec {
	std::string name;
	MsgPack config;
};

MsgPack wrap(const char* name, MsgPack body) {
	MsgPack aggs = MsgPack::MAP();
	aggs[name] = std::move(body);
	MsgPack config = MsgPack::MAP();
	config["_aggs"] = std::move(aggs);
	return config;
}

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

template <typename T>
MsgPack range_of(const char* key, std::optional<T> from, std::optional<T> to) {
	MsgPack range = MsgPack::MAP();
	range["_key"] = key;
	if (from) { range["_from"] = *from; }
	if (to) { range["_to"] = *to; }
	return range;
}

MsgPack list_of(std::vector<MsgPack> items) {
	MsgPack list = MsgPack::ARRAY();
	for (auto& item : items) {
		list.push_back(item);
	}
	return list;
}

MsgPack filter_by(const char* field, MsgPack value) {
	MsgPack term = MsgPack::MAP();
	term[field] = std::move(value);
	MsgPack body = MsgPack::MAP();
	body["_filter"] = MsgPack({ { "_term", std::move(term) } });
	return body;
}

std::vector<Spec> specs() {
	std::vector<Spec> out;
	auto add = [&](const std::string& name, MsgPack body) { out.push_back({ name, wrap("x", std::move(body)) }); };
	const MsgPack all = MsgPack(1000);
	const std::vector<std::pair<const char*, MsgPack>> limit = { { "_limit", all } };

	// Metrics, over the integer field and the floating one.
	for (const char* metric : { "_count", "_sum", "_avg", "_min", "_max", "_variance", "_std_deviation", "_median", "_mode", "_stats", "_extended_stats" }) {
		for (const char* field : { "age", "score" }) {
			add(std::string(metric) + " " + field, kind(metric, field));
		}
	}

	// Buckets of every kind, over each type they take.
	add("_values age", kind("_values", "age", limit));
	add("_values score", kind("_values", "score", limit));
	add("_values cat", kind("_values", "cat", limit));
	add("_value cat (deprecated name)", kind("_value", "cat", limit));
	add("_terms age", kind("_terms", "age", limit));
	add("_terms score", kind("_terms", "score", limit));
	add("_terms cat", kind("_terms", "cat", limit));
	add("_term cat (deprecated name)", kind("_term", "cat", limit));
	add("_histogram age", kind("_histogram", "age", { { "_interval", MsgPack(5) }, { "_limit", all } }));
	add("_histogram age shift", kind("_histogram", "age", { { "_interval", MsgPack(4) }, { "_shift", MsgPack(1) }, { "_limit", all } }));
	add("_histogram score", kind("_histogram", "score", { { "_interval", MsgPack(2.5) }, { "_limit", all } }));
	add("_range age", kind("_range", "age", { { "_ranges", list_of({ range_of<int>("neg", std::nullopt, 0), range_of<int>("low", 0, 10), range_of<int>("high", 10, std::nullopt) }) } }));
	add("_range score", kind("_range", "score", { { "_ranges", list_of({ range_of<double>("a", std::nullopt, -2.5), range_of<double>("b", -2.5, 5.0), range_of<double>("c", 5.0, std::nullopt) }) } }));
	add("_filter cat", with(filter_by("cat", MsgPack("c3")), { { "n", kind("_count", "age") }, { "s", kind("_stats", "score") } }));
	add("_filter age list", with(filter_by("age", list_of({ MsgPack(3), MsgPack(4), MsgPack(5) })), { { "m", kind("_median", "score") }, { "v", kind("_values", "cat", limit) } }));

	// Buckets with sub-aggregations, nested.
	add("_values age > stats score", with(kind("_values", "age", limit), { { "s", kind("_stats", "score") } }));
	add("_terms cat > stats, histogram, values > avg", with(kind("_terms", "cat", limit), {
		{ "s", kind("_stats", "age") },
		{ "h", kind("_histogram", "score", { { "_interval", MsgPack(2.5) }, { "_limit", all } }) },
		{ "v", with(kind("_values", "age", limit), { { "a", kind("_avg", "score") } }) },
	}));
	add("_histogram age > median, mode, extended_stats", with(kind("_histogram", "age", { { "_interval", MsgPack(5) }, { "_limit", all } }), {
		{ "m", kind("_median", "score") }, { "o", kind("_mode", "age") }, { "e", kind("_extended_stats", "score") },
	}));
	add("_range score > terms cat > min, max", with(kind("_range", "score", { { "_ranges", list_of({ range_of<double>("lo", std::nullopt, 0.0), range_of<double>("hi", 0.0, std::nullopt) }) } }), {
		{ "t", with(kind("_terms", "cat", limit), { { "lo", kind("_min", "age") }, { "hi", kind("_max", "age") } }) },
	}));
	add("_filter cat > terms age > variance", with(filter_by("cat", MsgPack("c5")), {
		{ "t", with(kind("_terms", "age", limit), { { "v", kind("_variance", "score") } }) },
	}));

	// Several at once, as a request makes them.
	MsgPack many = MsgPack::MAP(), aggs = MsgPack::MAP();
	aggs["a"] = kind("_stats", "age");
	aggs["b"] = kind("_terms", "cat", limit);
	aggs["c"] = kind("_histogram", "score", { { "_interval", MsgPack(2.5) }, { "_limit", all } });
	aggs["d"] = kind("_median", "age");
	many["_aggs"] = aggs;
	out.push_back({ "several at once", many });
	return out;
}

// The layouts of one scenario, 1 shard first.
struct Scenario {
	const char* name;
	std::function<bool(std::size_t)> dead;
};

std::vector<Scenario> scenarios() {
	return {
		{ "no deletes", [](std::size_t) { return false; } },
		{ "deletes", [](std::size_t i) { return i % 5 == 2 || i % 11 == 0; } },
		// With five shards, shard 4 holds nothing live: an empty shard has a partial too.
		{ "deletes, a shard emptied", [](std::size_t i) { return i % 5 == 2 || i % 11 == 0 || i % 5 == 4; } },
	};
}

std::size_t g_compared = 0, g_inexact = 0;
double g_worst = 0;

void check_equal(const MsgPack& want, const MsgPack& got, const std::string& label) {
	Diff diff;
	compare(want, got, "", diff);
	++g_compared;
	g_inexact += diff.inexact;
	g_worst = std::max(g_worst, diff.worst);
	CHECK_TRUE(diff.paths.empty());
	if (!diff.paths.empty()) {
		std::fprintf(stderr, "  DIFF %s: %zu values differ, first %s\n", label.c_str(), diff.paths.size(), diff.paths.front().c_str());
	}
}

}  // namespace

// Every kind, every scenario: the answer over 2 and 5 shards equals the answer over one, through each path.
static void test_every_kind_equal_over_shards() {
	const auto rows = main_rows();
	const auto schema = make_schema();
	const auto all = specs();
	std::size_t index = 0;
	for (const auto& scenario : scenarios()) {
		const auto tag = "s" + std::to_string(index++);
		const auto one = build(rows, 1, scenario.dead, tag);
		std::vector<Layout> many;
		for (std::size_t n : { std::size_t{2}, std::size_t{5} }) {
			many.push_back(build(rows, n, scenario.dead, tag));
		}
		for (const auto& spec : all) {
			MsgPack want;
			try {
				want = by_one_spy(one, spec.config, schema);
			} catch (const std::exception& exc) {
				std::fprintf(stderr, "  FAIL %s over one shard: %s\n", spec.name.c_str(), exc.what());
				++testh::failures;
				continue;
			}
			for (const auto& layout : many) {
				const auto label = spec.name + " | " + scenario.name + " | " + std::to_string(layout.n) + " shards";
				try {
					check_equal(want, by_one_spy(layout, spec.config, schema), label + " | one spy");
					check_equal(want, by_merge(layout, spec.config, schema, true), label + " | merged bytes");
					check_equal(want, by_merge(layout, spec.config, schema, false), label + " | merged objects");
				} catch (const std::exception& exc) {
					std::fprintf(stderr, "  FAIL %s: %s\n", label.c_str(), exc.what());
					++testh::failures;
				}
			}
		}
	}
	std::fprintf(stderr, "  compared %zu results of %zu requests; %zu floating values differ by rounding, worst %.3g\n", g_compared, all.size(), g_inexact, g_worst);
	CHECK_GT(g_compared, std::size_t{100});
}

// The reference is the rows themselves: the one-shard answer is not wrong in the same way as the others.
static void test_one_shard_matches_the_rows() {
	const auto rows = main_rows();
	const auto schema = make_schema();
	const auto dead = scenarios()[1].dead;
	const auto one = build(rows, 1, dead, "m");
	std::int64_t live = 0, count = 0, sum = 0, lo = INT64_MAX, hi = INT64_MIN;
	std::map<std::string, std::int64_t> cats;
	for (std::size_t i = 0; i < rows.size(); ++i) {
		if (dead(i)) {
			continue;
		}
		++live;
		for (auto n : rows[i].ages) {
			++count;
			sum += n;
			lo = std::min(lo, n);
			hi = std::max(hi, n);
		}
		for (const auto& cat : rows[i].cats) {
			++cats[cat];
		}
	}
	auto aggs = MsgPack::MAP();
	aggs["a"] = kind("_stats", "age");
	aggs["c"] = kind("_values", "cat", { { "_limit", MsgPack(1000) } });
	auto config = MsgPack::MAP();
	config["_aggs"] = aggs;
	const auto result = by_one_spy(one, config, schema);
	const auto& top = result.at("_aggregations");
	CHECK_EQ(top.at("_doc_count").as_i64(), live);
	CHECK_EQ(top.at("a").at("_count").as_i64(), count);
	CHECK_EQ(top.at("a").at("_sum").as_i64(), sum);
	CHECK_EQ(top.at("a").at("_min").as_i64(), lo);
	CHECK_EQ(top.at("a").at("_max").as_i64(), hi);
	std::map<std::string, std::int64_t> got;
	for (const auto& bucket : top.at("c")) {
		got[std::string(bucket.at("_key").str_view())] = bucket.at("_doc_count").as_i64();
	}
	CHECK_EQ(got, cats);
	CHECK_GT(live, std::int64_t{100});
	CHECK_EQ(cats.size(), std::size_t{12});
}

// Past `_shard_size` the counts of a `_terms` are approximate by design: a value dropped and seen again starts
// over, so a count falls short of the true one, never above it; the hot values still lead, in order.
static void test_terms_past_shard_size_are_approximate() {
	const auto rows = heavy_rows();
	const auto schema = make_schema();
	const auto dead = [](std::size_t i) { return i % 7 == 3; };
	std::map<std::string, std::int64_t> exact;
	for (std::size_t i = 0; i < rows.size(); ++i) {
		if (!dead(i)) {
			++exact[rows[i].uniq];
		}
	}
	std::vector<std::pair<std::int64_t, std::string>> ranked;
	for (const auto& [value, count] : exact) {
		ranked.emplace_back(count, value);
	}
	std::sort(ranked.begin(), ranked.end(), std::greater<>());

	constexpr std::size_t top = 3;
	const auto config = wrap("x", kind("_terms", "uniq", { { "_limit", MsgPack(5) }, { "_shard_size", MsgPack(20) } }));
	std::vector<Layout> layouts;
	for (std::size_t n : { std::size_t{1}, std::size_t{2}, std::size_t{5} }) {
		layouts.push_back(build(rows, n, dead, "h"));
	}
	std::size_t checked = 0;
	for (const auto& layout : layouts) {
		for (int path = 0; path < 3; ++path) {
			const auto result = path == 0 ? by_one_spy(layout, config, schema) : by_merge(layout, config, schema, path == 1);
			const auto& buckets = result.at("_aggregations").at("x");
			REQUIRE_EQ(buckets.size(), std::size_t{5});
			std::size_t rank = 0;
			for (const auto& bucket : buckets) {
				const std::string key(bucket.at("_key").str_view());
				const auto got = bucket.at("_doc_count").as_i64();
				CHECK_LE(got, exact.at(key));
				if (rank < top) {
					CHECK_EQ(key, ranked[rank].second);
					CHECK_GE(got * 100, exact.at(key) * 95);
					++checked;
				}
				++rank;
			}
		}
	}
	CHECK_EQ(checked, std::size_t{3 * 3 * top});
}

TEST_MAIN(test_every_kind_equal_over_shards, test_one_shard_matches_the_rows, test_terms_past_shard_size_are_approximate)
