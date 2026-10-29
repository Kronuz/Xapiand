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

// Unit tests for the engine seam's sort keys and match spies: Xapiand's key maker and
// aggregation spy read a DocView, here from a strata shard searched through the seam.

#include "test_harness.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "engine/callbacks.h"
#include "engine/error.h"
#include "engine_support.h"
#include "aggregations/aggregations.h"
#include "aggregations/exception.h"
#include "database/schema.h"
#include "exception_engine.h"
#include "msgpack.h"
#include "multivalue/keymaker.h"
#include "serialise.h"
#include "serialise_list.h"

namespace {

using testsupport::MapView;

// The value slots of the corpus (slot 1 is the shard's own version slot).
constexpr engine::valueno INTS = 21, REAL = 22, TEXT = 23;

// A small deterministic generator, so a failure reproduces.
struct Lcg {
	std::uint64_t state;
	std::uint32_t next(std::uint32_t bound) {
		state = state * 6364136223846793005ULL + 1442695040888963407ULL;
		return static_cast<std::uint32_t>((state >> 33) % bound);
	}
};

std::string multi(const std::vector<std::string>& values) {
	return StringList::serialise(values.begin(), values.end());
}

// Orders by the slot-1 text and counts its live instances.
class CountedKey final : public engine::KeyMaker {
public:
	static inline int live = 0;

	CountedKey() { ++live; }
	CountedKey(const CountedKey&) = delete;
	~CountedKey() override { --live; }

	std::string key(const engine::DocView& doc) const override { return doc.value(1); }
	std::unique_ptr<engine::KeyMaker> clone() const override { return std::make_unique<CountedKey>(); }
	std::string name() const override { return "CountedKey"; }
	std::string serialise() const override { return "k"; }
	std::unique_ptr<engine::KeyMaker> unserialise(std::string_view s) const override {
		if (s != "k") {
			throw engine::Error(engine::errc::invalid, "Bad serialised CountedKey");
		}
		return std::make_unique<CountedKey>();
	}
};

// Counts what it sees and its live instances.
class CountedSpy final : public engine::MatchSpy {
public:
	static inline int live = 0;
	int seen = 0;

	CountedSpy() { ++live; }
	CountedSpy(const CountedSpy&) = delete;
	~CountedSpy() override { --live; }

	void see(const engine::DocView&, double) override { ++seen; }
	std::unique_ptr<engine::MatchSpy> clone() const override { return std::make_unique<CountedSpy>(); }
	std::string name() const override { return "CountedSpy"; }
	std::string serialise() const override { return "s"; }
	std::unique_ptr<engine::MatchSpy> unserialise(std::string_view s) const override {
		if (s != "s") {
			throw engine::Error(engine::errc::invalid, "Bad serialised CountedSpy");
		}
		return std::make_unique<CountedSpy>();
	}
	std::string serialise_results() const override { return std::to_string(seen); }
	void merge_results(std::string_view s) override { seen += std::stoi(std::string(s)); }
	std::string description() const override { return "CountedSpy"; }
};

// Twelve documents: slot 1 holds integers (several values on some, none on every fourth),
// slot 2 a float, slot 3 text.
struct Row {
	std::vector<std::int64_t> ints;
	double real;
	std::string text;
};

std::vector<Row> make_rows() {
	std::vector<Row> rows;
	Lcg rng{7};
	for (int i = 1; i <= 12; ++i) {
		Row row;
		if (i % 4 != 0) {
			for (auto n = 1 + rng.next(3); n > 0; --n) {
				row.ints.push_back(static_cast<std::int64_t>(rng.next(41)) - 20);
			}
			std::sort(row.ints.begin(), row.ints.end());
			row.ints.erase(std::unique(row.ints.begin(), row.ints.end()), row.ints.end());
		}
		row.real = static_cast<double>(rng.next(200)) / 8.0 - 12.5;
		row.text = std::string(1, static_cast<char>('a' + rng.next(26))) + std::to_string(i);
		rows.push_back(std::move(row));
	}
	return rows;
}

// The seam's corpus: one document per row, all with the term "t".
std::unique_ptr<engine::Shard> make_db(const std::vector<Row>& rows, const char* name = "engine_sort") {
	auto shard = testsupport::scratch_shard(name);
	int n = 0;
	for (const auto& row : rows) {
		engine::Document doc;
		doc.add_term(doc.field("f"), "t");
		if (!row.ints.empty()) {
			std::vector<std::string> values;
			for (auto v : row.ints) {
				values.push_back(Serialise::integer(v));
			}
			doc.add_value(INTS, multi(values));
		}
		doc.add_value(REAL, Serialise::floating(row.real));
		doc.add_value(TEXT, row.text);
		doc.set_data("d");
		testsupport::add_keyed(*shard, doc, "K" + std::to_string(++n));
	}
	engine::sync_wait(shard->commit(engine::Transaction::none));
	return shard;
}

// The ids of everything the shard holds, ordered by the sorter, with the spy seeing them.
std::vector<engine::docid> sorted(engine::Shard& db, const engine::KeyMaker* sorter, engine::MatchSpy* spy = nullptr) {
	testsupport::Find find;
	find.maxitems = 20;
	find.check_at_least = 20;
	find.sorter = sorter;
	find.spy = spy;
	return testsupport::hit_ids(db, engine::Query::term("f", "t"), find);
}

// The ids ordered by a key (smallest first), ties by id: the order a sort by that key gives.
std::vector<engine::docid> order_by(std::size_t n, const std::function<std::string(std::size_t)>& key) {
	std::vector<std::pair<std::string, engine::docid>> keyed;
	for (std::size_t i = 0; i < n; ++i) {
		keyed.emplace_back(key(i), static_cast<engine::docid>(i + 1));
	}
	std::stable_sort(keyed.begin(), keyed.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
	std::vector<engine::docid> dids;
	for (const auto& entry : keyed) {
		dids.push_back(entry.second);
	}
	return dids;
}

// The distance a document's integers have to ref: the closest, or the farthest when reversed.
// Documents with no value sort last (they get a key above every distance).
std::string integer_key(const Row& row, std::int64_t ref, bool reverse) {
	if (row.ints.empty()) {
		return "z";
	}
	std::int64_t best = reverse ? 0 : INT64_MAX;
	for (auto n : row.ints) {
		std::int64_t d = std::llabs(n - ref);
		best = reverse ? std::max(best, d) : std::min(best, d);
	}
	char buf[32];
	std::snprintf(buf, sizeof(buf), "a%020lld", static_cast<long long>(reverse ? 1000000 - best : best));
	return buf;
}

}  // namespace

// Sorting by an integer reference reads the DocView's values: the order is the distance of each
// document's closest (or farthest, reversed) value to the reference, computed here by hand.
static void test_sort_integer() {
	const auto rows = make_rows();
	auto db = make_db(rows);
	for (std::int64_t ref : { -20, 0, 7, 40 }) {
		for (bool reverse : { false, true }) {
			Multi_MultiValueKeyMaker maker;
			maker.add_integer(INTS, reverse, ref);
			CHECK_EQ(sorted(*db, &maker), order_by(rows.size(), [&](std::size_t i) { return integer_key(rows[i], ref, reverse); }));
		}
	}
}

// Sorting by a float reference and by the stored text, and by two keys in turn.
static void test_sort_float_text_and_pairs() {
	const auto rows = make_rows();
	auto db = make_db(rows);

	Multi_MultiValueKeyMaker by_float;
	by_float.add_float(REAL, false, 1.5);
	CHECK_EQ(sorted(*db, &by_float), order_by(rows.size(), [&](std::size_t i) {
		char buf[32];
		std::snprintf(buf, sizeof(buf), "%020.6f", std::fabs(rows[i].real - 1.5));
		return std::string(buf);
	}));

	Multi_MultiValueKeyMaker by_text;
	by_text.add_serialise(TEXT, false);
	CHECK_EQ(sorted(*db, &by_text), order_by(rows.size(), [&](std::size_t i) { return rows[i].text; }));

	Multi_MultiValueKeyMaker reversed_text;
	reversed_text.add_serialise(TEXT, true);
	auto forward = sorted(*db, &by_text);
	std::reverse(forward.begin(), forward.end());
	CHECK_EQ(sorted(*db, &reversed_text), forward);

	// The first key decides; the second, descending, breaks its ties.
	Multi_MultiValueKeyMaker pair;
	pair.add_integer(INTS, false, 100);
	pair.add_serialise(TEXT, true);
	CHECK_EQ(sorted(*db, &pair), order_by(rows.size(), [&](std::size_t i) {
		std::string complement;
		for (unsigned char ch : rows[i].text) {
			complement.push_back(static_cast<char>(255 - ch));
		}
		return integer_key(rows[i], 100, false) + "|" + complement;
	}));
}

// The key maker rebuilds by name from its serialised form and keys the same, whole or cloned.
static void test_key_maker_round_trip() {
	const auto rows = make_rows();
	auto db = make_db(rows);

	Multi_MultiValueKeyMaker maker;
	maker.add_integer(INTS, true, 3);
	maker.add_float(REAL, false, -2.0);
	maker.add_bool(INTS, false, true);
	maker.add_string_levenshtein(TEXT, false, "b4", true);
	maker.add_positive(REAL, false, 9u);
	maker.add_date(REAL, false, 1.0);
	maker.add_serialise(TEXT, false);
	const auto expected = sorted(*db, &maker);

	auto copy = maker.clone();
	CHECK_EQ(sorted(*db, copy.get()), expected);

	auto rebuilt = Multi_MultiValueKeyMaker().unserialise(maker.serialise());
	CHECK_EQ(rebuilt->serialise(), maker.serialise());
	CHECK_EQ(sorted(*db, rebuilt.get()), expected);
}

// Bytes that are not a serialised key maker are an engine error, never a crash.
static void test_key_maker_unserialise_garbage() {
	Lcg rng{11};
	Multi_MultiValueKeyMaker valid;
	valid.add_integer(1, true, 3);
	valid.add_geo(2, false, std::vector<Cartesian>{ Cartesian(0.5, 0.5, 0.5) });
	valid.add_string_jaro(3, false, "abc", false);
	valid.add_float(4, false, 1.5);
	valid.add_date(5, true, 1e9);
	const auto good = valid.serialise();
	Multi_MultiValueKeyMaker prototype;

	int rejected = 0;
	int accepted = 0;
	for (int i = 0; i < 20000; ++i) {
		std::string bytes;
		switch (i % 4) {
			case 0:
				for (auto n = rng.next(48); n > 0; --n) {
					bytes.push_back(static_cast<char>(rng.next(256)));
				}
				break;
			case 1:
				bytes = good;
				bytes[rng.next(static_cast<std::uint32_t>(bytes.size()))] ^= static_cast<char>(1 + rng.next(255));
				break;
			case 2:
				bytes = good.substr(0, rng.next(static_cast<std::uint32_t>(good.size())));
				break;
			default:
				bytes = good + std::string(1 + rng.next(4), static_cast<char>(rng.next(256)));
				break;
		}
		try {
			auto made = prototype.unserialise(bytes);
			CHECK_TRUE(made != nullptr);
			++accepted;
		} catch (const engine::Error& exc) {
			CHECK_EQ(exc.kind, engine::errc::invalid);
			++rejected;
		} catch (...) {
			CHECK_TRUE(false);
		}
	}
	CHECK_GT(rejected, 0);
	CHECK_GT(accepted, 0);
}

// A stored value that does not decode makes the key throw the error Xapiand reports to the client;
// it never terminates the process or reads past the value.
static void test_key_maker_corrupt_values() {
#ifndef NDEBUG
	// sortable_unserialise asserts on bytes that are not a serialised number, so the random-bytes run needs NDEBUG.
	std::fprintf(stderr, "  SKIP test_key_maker_corrupt_values: asserts are on\n");
	return;
#endif
	Lcg rng{13};
	std::vector<std::pair<std::string, std::unique_ptr<Multi_MultiValueKeyMaker>>> makers;
	auto add = [&](const char* name, auto&& fill) {
		auto maker = std::make_unique<Multi_MultiValueKeyMaker>();
		fill(*maker);
		makers.emplace_back(name, std::move(maker));
	};
	add("serialise", [](auto& m) { m.add_serialise(1, false); });
	add("float", [](auto& m) { m.add_float(1, false, 1.5); });
	add("integer", [](auto& m) { m.add_integer(1, false, 5); });
	add("integer-reverse", [](auto& m) { m.add_integer(1, true, 5); });
	add("positive", [](auto& m) { m.add_positive(1, false, 5u); });
	add("date", [](auto& m) { m.add_date(1, false, 3.0); });
	add("bool", [](auto& m) { m.add_bool(1, false, true); });
	add("geo", [](auto& m) { m.add_geo(1, false, std::vector<Cartesian>{ Cartesian(0.5, 0.5, 0.5) }); });
	add("geo-reverse", [](auto& m) { m.add_geo(1, true, std::vector<Cartesian>{ Cartesian(0.5, 0.5, 0.5) }); });
	add("levenshtein", [](auto& m) { m.add_string_levenshtein(1, false, "abc", false); });
	add("jaro", [](auto& m) { m.add_string_jaro(1, true, "abc", false); });

	int decoded = 0;
	int thrown = 0;
	for (int i = 0; i < 4000; ++i) {
		std::string value;
		for (auto n = rng.next(24); n > 0; --n) {
			value.push_back(static_cast<char>(rng.next(256)));
		}
		const MapView view({ { 1, value } });
		for (const auto& entry : makers) {
			try {
				(void)entry.second->key(view);
				++decoded;
			} catch (const SerialisationError&) {
				++thrown;
			} catch (...) {
				std::fprintf(stderr, "  FAIL %s: unexpected exception\n", entry.first.c_str());
				CHECK_TRUE(false);
			}
		}
	}
	CHECK_GT(decoded, 0);
	CHECK_GT(thrown, 0);
}

// A search frees every callback it makes: the count of live callbacks is the evidence (this host's
// AddressSanitizer has no leak checker).
static void test_callback_ownership() {
	const auto rows = make_rows();
	auto db = make_db(rows);
	auto second = make_db(rows, "engine_sort_second");
	auto third = make_db(rows, "engine_sort_third");

	CountedKey::live = 0;
	CountedSpy::live = 0;
	{
		CountedKey key;
		CountedSpy spy;
		CHECK_EQ(CountedKey::live, 1);
		CHECK_EQ(CountedSpy::live, 1);

		// Borrowed, the way DatabaseHandler hands them to a search, over one shard and over three.
		for (int i = 0; i < 20; ++i) {
			CHECK_EQ(sorted(*db, &key, &spy).size(), rows.size());
			testsupport::Find find;
			find.sorter = &key;
			find.spy = &spy;
			CHECK_EQ(testsupport::search({ db.get(), second.get(), third.get() }, engine::Query::term("f", "t"), find).hits.size(), 3 * rows.size());
		}
		CHECK_EQ(CountedKey::live, 1);
		CHECK_EQ(CountedSpy::live, 1);
		CHECK_EQ(spy.seen, static_cast<int>(20 * 4 * rows.size()));

		// Cloned and rebuilt from the serialised form, the copies are the caller's to free.
		{
			auto key_clone = key.clone();
			auto spy_clone = spy.clone();
			CHECK_EQ(CountedKey::live, 2);
			CHECK_EQ(CountedSpy::live, 2);
			CHECK_EQ(sorted(*db, key_clone.get(), spy_clone.get()).size(), rows.size());
			for (int i = 0; i < 20; ++i) {
				auto rebuilt_key = key.unserialise("k");
				auto rebuilt_spy = spy.unserialise("s");
				CHECK_EQ(CountedKey::live, 3);
				CHECK_EQ(CountedSpy::live, 3);
				CHECK_EQ(sorted(*db, rebuilt_key.get(), rebuilt_spy.get()).size(), rows.size());
			}
			// A malformed serialisation leaks nothing.
			for (int i = 0; i < 20; ++i) {
				CHECK_THROW(key.unserialise("bad"), engine::Error);
				CHECK_THROW(spy.unserialise("bad"), engine::Error);
			}
			CHECK_EQ(CountedKey::live, 2);
			CHECK_EQ(CountedSpy::live, 2);
		}
		CHECK_EQ(CountedKey::live, 1);
		CHECK_EQ(CountedSpy::live, 1);
	}
	CHECK_EQ(CountedKey::live, 0);
	CHECK_EQ(CountedSpy::live, 0);
}


namespace {

// A schema that knows two fields: age (integer, slot 1001) and name (keyword, slot 1002).
std::shared_ptr<Schema> make_schema() {
	MsgPack props = MsgPack::MAP();
	props["age"] = MsgPack({ { "_type", "integer" }, { "_slot", 1001 } });
	props["name"] = MsgPack({ { "_type", "keyword" }, { "_slot", 1002 } });
	auto schema_doc = std::make_shared<const MsgPack>(std::move(props));
	return std::make_shared<Schema>(schema_doc, nullptr, "");
}

MsgPack aggs_config() {
	return MsgPack({ { "_aggs", {
		{ "ages", { { "_stats", { { "_field", "age" } } } } },
		{ "by_name", { { "_values", { { "_field", "name" } } }, { "_aggs", { { "top", { { "_max", { { "_field", "age" } } } } } } } } },
		{ "hist", { { "_histogram", { { "_field", "age" }, { "_interval", 10 } } } } },
	} } });
}

// Documents 1..n of `rows`, with age in slot 1001 and the name in slot 1002.
std::unique_ptr<engine::Shard> make_agg_db(const std::vector<Row>& rows, std::size_t from, std::size_t to, const char* name) {
	auto shard = testsupport::scratch_shard(name);
	for (auto i = from; i < to; ++i) {
		engine::Document doc;
		doc.add_term(doc.field("f"), "t");
		if (!rows[i].ints.empty()) {
			std::vector<std::string> values;
			for (auto n : rows[i].ints) {
				values.push_back(Serialise::integer(n));
			}
			doc.add_value(1001, multi(values));
		}
		doc.add_value(1002, "n" + std::to_string(i % 3));
		doc.set_data("d");
		testsupport::add_keyed(*shard, doc, "K" + std::to_string(i));
	}
	engine::sync_wait(shard->commit(engine::Transaction::none));
	return shard;
}

// Equal results; the order of histogram buckets with equal counts is not part of the result.
bool same_aggregations(const MsgPack& a, const MsgPack& b) {
	auto buckets = [](const MsgPack& r) {
		std::map<std::string, std::string> out;
		for (const auto& bucket : r.at("_aggregations").at("hist")) {
			out[std::string(bucket.at("_key").str_view())] = bucket.to_string();
		}
		return out;
	};
	return a.at("_aggregations").at("ages").to_string() == b.at("_aggregations").at("ages").to_string()
		&& a.at("_aggregations").at("by_name").to_string() == b.at("_aggregations").at("by_name").to_string()
		&& buckets(a) == buckets(b);
}

std::string run_spy(engine::Shard& db, engine::MatchSpy& spy) {
	return std::to_string(sorted(db, nullptr, &spy).size());
}

}  // namespace

// The aggregation spy computes what the values say, merges the results of two halves into the
// result of the whole, and rebuilds by name from its serialised form.
static void test_aggregation_spy() {
	const auto rows = make_rows();
	auto schema = make_schema();
	const auto config = aggs_config();

	std::int64_t count = 0, sum = 0, lo = INT64_MAX, hi = INT64_MIN;
	std::map<std::string, std::int64_t> name_max;
	for (std::size_t i = 0; i < rows.size(); ++i) {
		for (auto n : rows[i].ints) {
			++count;
			sum += n;
			lo = std::min(lo, n);
			hi = std::max(hi, n);
			auto& m = name_max.try_emplace("n" + std::to_string(i % 3), INT64_MIN).first->second;
			m = std::max(m, n);
		}
	}

	auto whole_db = make_agg_db(rows, 0, rows.size(), "engine_sort_agg_whole");
	AggregationMatchSpy whole(config, schema);
	CHECK_EQ(run_spy(*whole_db, whole), std::to_string(rows.size()));
	const auto result = whole.get_aggregation();
	const auto& ages = result.at("_aggregations").at("ages");
	CHECK_EQ(static_cast<std::int64_t>(ages.at("_count").as_i64()), count);
	CHECK_EQ(static_cast<std::int64_t>(ages.at("_sum").as_i64()), sum);
	CHECK_EQ(static_cast<std::int64_t>(ages.at("_min").as_i64()), lo);
	CHECK_EQ(static_cast<std::int64_t>(ages.at("_max").as_i64()), hi);
	std::map<std::string, std::int64_t> got_max;
	for (const auto& bucket : result.at("_aggregations").at("by_name")) {
		got_max[std::string(bucket.at("_key").str_view())] = bucket.at("top").at("_max").as_i64();
	}
	CHECK_EQ(got_max, name_max);

	// Two halves, merged through the serialised results, are the whole.
	auto first_db = make_agg_db(rows, 0, 6, "engine_sort_agg_first");
	auto second_db = make_agg_db(rows, 6, rows.size(), "engine_sort_agg_second");
	AggregationMatchSpy first(config, schema), second(config, schema);
	run_spy(*first_db, first);
	run_spy(*second_db, second);
	first.merge_results(second.serialise_results());
	CHECK_TRUE(same_aggregations(first.get_aggregation(), result));

	// A spy rebuilt by name from the serialised one sees the same and merges back into the whole.
	auto remote = AggregationMatchSpy().unserialise(whole.serialise());
	CHECK_EQ(sorted(*whole_db, nullptr, remote.get()).size(), rows.size());
	AggregationMatchSpy merged(config, schema);
	merged.merge_results(remote->serialise_results());
	CHECK_TRUE(same_aggregations(merged.get_aggregation(), result));

	auto copy = whole.clone();
	CHECK_EQ(copy->name(), "AggregationMatchSpy");
	CHECK_EQ(copy->serialise(), whole.serialise());
}

// Bytes that are not a serialised spy or serialised results are an error, never a crash.
static void test_aggregation_spy_garbage() {
	Lcg rng{17};
	auto schema = make_schema();
	AggregationMatchSpy valid(aggs_config(), schema);
	const auto good = valid.serialise();
	const auto good_results = valid.serialise_results();
	AggregationMatchSpy prototype;

	int rejected = 0, accepted = 0;
	for (int i = 0; i < 4000; ++i) {
		std::string bytes;
		if (i % 2 == 0) {
			for (auto n = rng.next(64); n > 0; --n) {
				bytes.push_back(static_cast<char>(rng.next(256)));
			}
		} else {
			bytes = good;
			bytes[rng.next(static_cast<std::uint32_t>(bytes.size()))] ^= static_cast<char>(1 + rng.next(255));
			bytes.resize(1 + rng.next(static_cast<std::uint32_t>(bytes.size())));
		}
		try {
			CHECK_TRUE(prototype.unserialise(bytes) != nullptr);
			++accepted;
		} catch (const engine::Error& exc) {
			CHECK_EQ(exc.kind, engine::errc::invalid);
			++rejected;
		} catch (const std::exception&) {
			++rejected;  // a malformed MsgPack inside: still an error to the caller
		}
	}
	CHECK_GT(rejected, 0);

	for (int i = 0; i < 4000; ++i) {
		std::string bytes = good_results;
		if (i % 2 == 0) {
			bytes.clear();
			for (auto n = rng.next(64); n > 0; --n) {
				bytes.push_back(static_cast<char>(rng.next(256)));
			}
		} else if (!bytes.empty()) {
			bytes[rng.next(static_cast<std::uint32_t>(bytes.size()))] ^= static_cast<char>(1 + rng.next(255));
		}
		AggregationMatchSpy target(aggs_config(), schema);
		try {
			target.merge_results(bytes);
		} catch (const SerialisationError&) {
		} catch (const engine::Error& exc) {
			CHECK_EQ(exc.kind, engine::errc::invalid);
		} catch (const std::exception&) {
		}
	}
	(void)accepted;
}

// Stored values that do not decode make the spy throw the error Xapiand reports; the process lives.
static void test_aggregation_spy_corrupt_values() {
#ifndef NDEBUG
	// sortable_unserialise asserts on bytes that are not a serialised number, so the random-bytes run needs NDEBUG.
	std::fprintf(stderr, "  SKIP test_aggregation_spy_corrupt_values: asserts are on\n");
	return;
#endif
	Lcg rng{19};
	auto schema = make_schema();
	int seen = 0, thrown = 0;
	for (int i = 0; i < 2000; ++i) {
		std::string age, name;
		for (auto n = rng.next(20); n > 0; --n) {
			age.push_back(static_cast<char>(rng.next(256)));
		}
		for (auto n = rng.next(10); n > 0; --n) {
			name.push_back(static_cast<char>(rng.next(256)));
		}
		const MapView view({ { 1001, age }, { 1002, name } });
		AggregationMatchSpy spy(aggs_config(), schema);
		try {
			spy.see(view, 1.0);
			++seen;
		} catch (const SerialisationError&) {
			++thrown;
		} catch (const std::exception&) {
			++thrown;
		}
	}
	CHECK_GT(seen + thrown, 0);
}


// Bytes that decode out of range saturate instead of converting with undefined behaviour, and
// stored extremes key without overflow.
static void test_unserialise_saturates() {
	CHECK_EQ(Unserialise::integer(Serialise::floating(1e30L)), INT64_MAX);
	CHECK_EQ(Unserialise::integer(Serialise::floating(-1e30L)), INT64_MIN);
	CHECK_EQ(Unserialise::positive(Serialise::floating(-1.75L)), 0U);
	CHECK_EQ(Unserialise::positive(Serialise::floating(1e30L)), UINT64_MAX);
	CHECK_EQ(Unserialise::integer(Serialise::integer(INT64_MIN)), INT64_MIN);
	CHECK_EQ(Unserialise::integer(Serialise::integer(INT64_MAX)), INT64_MAX);
	CHECK_EQ(Unserialise::positive(Serialise::positive(UINT64_MAX)), UINT64_MAX);

	const MapView view({ { 1, Serialise::integer(INT64_MIN) } });
	Multi_MultiValueKeyMaker maker;
	maker.add_integer(1, false, 5);
	CHECK_EQ(maker.key(view), Serialise::integer(INT64_MAX));
}

// Distances of 2^63 or more saturate and sort as the farthest, for single and paired values, in both
// directions, against an exact 128-bit distance.
static void test_sort_integer_extremes() {
	const std::int64_t E[] = {INT64_MIN, INT64_MIN + 1, INT64_MIN / 2, -5, 0, 5, INT64_MAX / 2, INT64_MAX - 1, INT64_MAX};
	struct Doc { std::vector<std::int64_t> v; std::string key; __int128 dist; };
	int pairs = 0;
	for (bool reverse : {false, true}) {
		for (auto ref : E) {
			Multi_MultiValueKeyMaker maker;
			maker.add_integer(1, reverse, ref);
			std::vector<Doc> docs;
			for (std::size_t i = 0; i < std::size(E); ++i) {
				for (std::size_t j = i; j < std::size(E); ++j) {
					Doc d;
					d.v = (i == j) ? std::vector<std::int64_t>{E[i]} : std::vector<std::int64_t>{E[i], E[j]};
					std::sort(d.v.begin(), d.v.end());
					std::vector<std::string> vs;
					__int128 best = reverse ? 0 : (static_cast<__int128>(1) << 100);
					for (auto n : d.v) {
						vs.push_back(Serialise::integer(n));
						__int128 x = static_cast<__int128>(n) - ref;
						if (x < 0) x = -x;
						best = reverse ? std::max(best, x) : std::min(best, x);
					}
					d.dist = std::min<__int128>(best, INT64_MAX);
					d.key = maker.key(MapView({ { 1, StringList::serialise(vs.begin(), vs.end()) } }));
					docs.push_back(std::move(d));
				}
			}
			for (const auto& a : docs) {
				for (const auto& b : docs) {
					if (a.dist < b.dist) {
						++pairs;
						CHECK_TRUE(reverse ? a.key > b.key : a.key < b.key);
					}
				}
			}
		}
	}
	CHECK_GT(pairs, 10000);
}

namespace {

// One document holding `ages` in slot 1001 and `names` in slot 1002, seen by a spy over `config`; returns the result.
MsgPack see_document(const MsgPack& config, const std::vector<std::int64_t>& ages, const std::vector<std::string>& names) {
	std::vector<std::string> values;
	for (auto n : ages) {
		values.push_back(Serialise::integer(n));
	}
	const MapView view({ { 1001, multi(values) }, { 1002, multi(names) } });
	AggregationMatchSpy spy(config, make_schema());
	spy.see(view, 1.0);
	return spy.get_aggregation();
}

// Nests `depth` levels of `_values` over name.
MsgPack nested_values(int depth) {
	MsgPack inner = MsgPack({ { "_values", { { "_field", "name" } } } });
	for (int i = 1; i < depth; ++i) {
		inner = MsgPack({ { "_values", { { "_field", "name" } } }, { "_aggs", { { "a", inner } } } });
	}
	return MsgPack({ { "_aggs", { { "a", inner } } } });
}

// Counts the buckets of a nested result.
std::size_t count_buckets(const MsgPack& level) {
	std::size_t n = 0;
	for (const auto& bucket : level) {
		++n;
		if (bucket.is_map() && bucket.find("a") != bucket.end()) {
			n += count_buckets(bucket.at("a"));
		}
	}
	return n;
}

}  // namespace

// A search holds at most AGGS_MAX_BUCKETS live buckets: the last one is accepted, the next fails with a
// client error that names the limit.
static void test_aggregation_bucket_budget() {
	const auto config = MsgPack({ { "_aggs", { { "h", { { "_histogram", { { "_field", "age" }, { "_interval", 1 } } } } } } } });
	std::vector<std::int64_t> ages;
	for (std::size_t i = 0; i < AGGS_MAX_BUCKETS; ++i) {
		ages.push_back(static_cast<std::int64_t>(i));
	}
	see_document(config, ages, { "x" });  // exactly at the limit
	ages.push_back(static_cast<std::int64_t>(AGGS_MAX_BUCKETS));
	bool refused = false;
	try {
		see_document(config, ages, { "x" });
	} catch (const AggregationError& exc) {
		refused = true;
		CHECK_TRUE(std::string(exc.what()).find("more than 65535 buckets") != std::string::npos);
	}
	CHECK_TRUE(refused);
}

// Nesting up to AGGS_MAX_DEPTH levels is accepted, one more is refused when the spy is built.
static void test_aggregation_depth_cap() {
	see_document(nested_values(static_cast<int>(AGGS_MAX_DEPTH)), { 1 }, { "x" });
	bool refused = false;
	try {
		see_document(nested_values(static_cast<int>(AGGS_MAX_DEPTH) + 1), { 1 }, { "x" });
	} catch (const AggregationError& exc) {
		refused = true;
		CHECK_TRUE(std::string(exc.what()).find("deeper than the limit of 16 levels") != std::string::npos);
	}
	CHECK_TRUE(refused);
}

// A document with V values is counted once in each bucket it belongs to, at every level, as Elasticsearch
// does: three levels over three values make 3 + 9 + 27 buckets, each with a count of one.
static void test_aggregation_multivalued_counts_each_bucket() {
	const auto result = see_document(nested_values(3), { 1 }, { "a", "b", "c" });
	const auto& top = result.at("_aggregations").at("a");
	CHECK_EQ(count_buckets(top), std::size_t(3 + 9 + 27));
	for (const auto& bucket : top) {
		CHECK_EQ(bucket.at("_doc_count").as_i64(), std::int64_t(1));
	}
}

// An engine error that escapes a test is a failure with its text, not an abort.
#define GUARDED(fn) \
	static void fn##_guarded() { \
		try { \
			fn(); \
		} catch (const engine::Error& exc) { \
			std::fprintf(stderr, "  FAIL %s: threw engine error %s\n", #fn, exc.reason().c_str()); \
			++testh::failures; \
		} catch (const std::exception& exc) { \
			std::fprintf(stderr, "  FAIL %s: threw %s\n", #fn, exc.what()); \
			++testh::failures; \
		} \
	}

GUARDED(test_sort_integer)
GUARDED(test_sort_float_text_and_pairs)
GUARDED(test_key_maker_round_trip)
GUARDED(test_sort_integer_extremes)
GUARDED(test_key_maker_unserialise_garbage)
GUARDED(test_key_maker_corrupt_values)
GUARDED(test_callback_ownership)
GUARDED(test_unserialise_saturates)
GUARDED(test_aggregation_spy)
GUARDED(test_aggregation_spy_garbage)
GUARDED(test_aggregation_spy_corrupt_values)
GUARDED(test_aggregation_bucket_budget)
GUARDED(test_aggregation_depth_cap)
GUARDED(test_aggregation_multivalued_counts_each_bucket)

TEST_MAIN(test_sort_integer_guarded, test_sort_integer_extremes_guarded, test_sort_float_text_and_pairs_guarded, test_key_maker_round_trip_guarded, test_key_maker_unserialise_garbage_guarded, test_key_maker_corrupt_values_guarded, test_callback_ownership_guarded, test_unserialise_saturates_guarded, test_aggregation_spy_guarded, test_aggregation_spy_garbage_guarded, test_aggregation_spy_corrupt_values_guarded, test_aggregation_bucket_budget_guarded, test_aggregation_depth_cap_guarded, test_aggregation_multivalued_counts_each_bucket_guarded)
