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

// Unit tests for the engine seam's sort keys and match spies: Xapiand's key maker and
// aggregation spy read a DocView, and reach Xapian only through the two adapters.

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
#include "engine/xapian/callbacks.h"
#include "aggregations/aggregations.h"
#include "database/schema.h"
#include "engine/xapian/convert.h"
#include "exception_xapian.h"
#include "msgpack.h"
#include "multivalue/keymaker.h"
#include "serialise.h"
#include "serialise_list.h"
#include "engine/xapian/xapian.h"

namespace {

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

Xapian::WritableDatabase make_db(const std::vector<Row>& rows) {
	Xapian::WritableDatabase db(std::string(), Xapian::DB_BACKEND_INMEMORY);
	for (const auto& row : rows) {
		Xapian::Document doc;
		doc.add_term("t");
		if (!row.ints.empty()) {
			std::vector<std::string> values;
			for (auto n : row.ints) {
				values.push_back(Serialise::integer(n));
			}
			doc.add_value(1, multi(values));
		}
		doc.add_value(2, Serialise::floating(row.real));
		doc.add_value(3, row.text);
		doc.set_data("d");
		db.add_document(doc);
	}
	return db;
}

std::vector<Xapian::docid> sorted(const Xapian::Database& db, Xapian::KeyMaker* sorter, Xapian::MatchSpy* spy = nullptr) {
	Xapian::Enquire enquire(db);
	enquire.set_query(Xapian::Query("t"));
	if (sorter) {
		enquire.set_sort_by_key_then_relevance(sorter, false);
	}
	if (spy) {
		enquire.add_matchspy(spy);
	}
	std::vector<Xapian::docid> dids;
	auto mset = enquire.get_mset(0, 20, 20);
	for (auto it = mset.begin(); it != mset.end(); ++it) {
		dids.push_back(*it);
	}
	return dids;
}

// The ids ordered by a key (smallest first), ties by id: the order a sort by that key gives.
std::vector<Xapian::docid> order_by(std::size_t n, const std::function<std::string(std::size_t)>& key) {
	std::vector<std::pair<std::string, Xapian::docid>> keyed;
	for (std::size_t i = 0; i < n; ++i) {
		keyed.emplace_back(key(i), static_cast<Xapian::docid>(i + 1));
	}
	std::stable_sort(keyed.begin(), keyed.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
	std::vector<Xapian::docid> dids;
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
		auto d = std::llabs(n - ref);
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
			maker.add_integer(1, reverse, ref);
			engine::xapian::KeyMakerAdapter adapter(maker);
			CHECK_EQ(sorted(db, &adapter), order_by(rows.size(), [&](std::size_t i) { return integer_key(rows[i], ref, reverse); }));
		}
	}
}

// Sorting by a float reference and by the stored text, and by two keys in turn.
static void test_sort_float_text_and_pairs() {
	const auto rows = make_rows();
	auto db = make_db(rows);

	Multi_MultiValueKeyMaker by_float;
	by_float.add_float(2, false, 1.5);
	engine::xapian::KeyMakerAdapter float_adapter(by_float);
	CHECK_EQ(sorted(db, &float_adapter), order_by(rows.size(), [&](std::size_t i) {
		char buf[32];
		std::snprintf(buf, sizeof(buf), "%020.6f", std::fabs(rows[i].real - 1.5));
		return std::string(buf);
	}));

	Multi_MultiValueKeyMaker by_text;
	by_text.add_serialise(3, false);
	engine::xapian::KeyMakerAdapter text_adapter(by_text);
	CHECK_EQ(sorted(db, &text_adapter), order_by(rows.size(), [&](std::size_t i) { return rows[i].text; }));

	Multi_MultiValueKeyMaker reversed_text;
	reversed_text.add_serialise(3, true);
	engine::xapian::KeyMakerAdapter reversed_adapter(reversed_text);
	auto forward = sorted(db, &text_adapter);
	std::reverse(forward.begin(), forward.end());
	CHECK_EQ(sorted(db, &reversed_adapter), forward);

	// The first key decides; the second, descending, breaks its ties.
	Multi_MultiValueKeyMaker pair;
	pair.add_integer(1, false, 100);
	pair.add_serialise(3, true);
	engine::xapian::KeyMakerAdapter pair_adapter(pair);
	CHECK_EQ(sorted(db, &pair_adapter), order_by(rows.size(), [&](std::size_t i) {
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
	maker.add_integer(1, true, 3);
	maker.add_float(2, false, -2.0);
	maker.add_bool(1, false, true);
	maker.add_string_levenshtein(3, false, "b4", true);
	maker.add_positive(2, false, 9u);
	maker.add_date(2, false, 1.0);
	maker.add_serialise(3, false);
	engine::xapian::KeyMakerAdapter adapter(maker);
	const auto expected = sorted(db, &adapter);

	auto copy = maker.clone();
	engine::xapian::KeyMakerAdapter copy_adapter(*copy);
	CHECK_EQ(sorted(db, &copy_adapter), expected);

	auto rebuilt = Multi_MultiValueKeyMaker().unserialise(maker.serialise());
	CHECK_EQ(rebuilt->serialise(), maker.serialise());
	engine::xapian::KeyMakerAdapter rebuilt_adapter(*rebuilt);
	CHECK_EQ(sorted(db, &rebuilt_adapter), expected);

	// Through a Xapian registry, the way the remote server rebuilds it.
	Xapian::Registry registry;
	engine::xapian::register_key_maker(registry, Multi_MultiValueKeyMaker());
	const auto* prototype = registry.get_key_maker("Multi_MultiValueKeyMaker");
	CHECK_TRUE(prototype != nullptr);
	std::unique_ptr<Xapian::KeyMaker> remote(prototype->unserialise(maker.serialise(), registry));
	CHECK_EQ(sorted(db, remote.get()), expected);
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
		Xapian::Document doc;
		std::string value;
		for (auto n = rng.next(24); n > 0; --n) {
			value.push_back(static_cast<char>(rng.next(256)));
		}
		doc.add_value(1, value);
		auto view = engine::xapian::make_view(doc);
		for (const auto& entry : makers) {
			try {
				(void)entry.second->key(*view);
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

// Every adapter handed to Xapian frees what it owns. The count of live callbacks is the evidence
// (this host's AddressSanitizer has no leak checker).
static void test_adapter_ownership() {
	const auto rows = make_rows();
	auto db = make_db(rows);

	CountedKey::live = 0;
	CountedSpy::live = 0;
	{
		CountedKey key;
		CountedSpy spy;
		CHECK_EQ(CountedKey::live, 1);
		CHECK_EQ(CountedSpy::live, 1);

		// Borrowed, the way DatabaseHandler hands them to an Enquire.
		for (int i = 0; i < 20; ++i) {
			engine::xapian::KeyMakerAdapter key_adapter(key);
			engine::xapian::MatchSpyAdapter spy_adapter(spy);
			CHECK_EQ(sorted(db, &key_adapter, &spy_adapter).size(), rows.size());
		}
		CHECK_EQ(CountedKey::live, 1);
		CHECK_EQ(CountedSpy::live, 1);
		CHECK_EQ(spy.seen, static_cast<int>(20 * rows.size()));

		// Owning, with Xapian's clone.
		{
			engine::xapian::KeyMakerAdapter owned_key(key.clone());
			engine::xapian::MatchSpyAdapter owned_spy(spy.clone());
			CHECK_EQ(CountedKey::live, 2);
			CHECK_EQ(CountedSpy::live, 2);
			std::unique_ptr<Xapian::KeyMaker> key_clone(owned_key.clone());
			std::unique_ptr<Xapian::MatchSpy> spy_clone(owned_spy.clone());
			CHECK_EQ(CountedKey::live, 3);
			CHECK_EQ(CountedSpy::live, 3);
			CHECK_EQ(sorted(db, key_clone.get(), spy_clone.get()).size(), rows.size());
		}
		CHECK_EQ(CountedKey::live, 1);
		CHECK_EQ(CountedSpy::live, 1);

		// Through the registry the remote server rebuilds them from.
		{
			Xapian::Registry registry;
			engine::xapian::register_key_maker(registry, key);
			engine::xapian::register_match_spy(registry, spy);
			CHECK_EQ(CountedKey::live, 2);
			CHECK_EQ(CountedSpy::live, 2);
			for (int i = 0; i < 20; ++i) {
				std::unique_ptr<Xapian::KeyMaker> rebuilt_key(registry.get_key_maker("CountedKey")->unserialise("k", registry));
				std::unique_ptr<Xapian::MatchSpy> rebuilt_spy(registry.get_match_spy("CountedSpy")->unserialise("s", registry));
				CHECK_EQ(CountedKey::live, 3);
				CHECK_EQ(CountedSpy::live, 3);
				CHECK_EQ(sorted(db, rebuilt_key.get(), rebuilt_spy.get()).size(), rows.size());
			}
			// A malformed serialisation leaks nothing.
			for (int i = 0; i < 20; ++i) {
				try {
					registry.get_key_maker("CountedKey")->unserialise("bad", registry);
					CHECK_TRUE(false);
				} catch (const Xapian::NetworkError&) { }
				try {
					registry.get_match_spy("CountedSpy")->unserialise("bad", registry);
					CHECK_TRUE(false);
				} catch (const Xapian::NetworkError&) { }
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

// The terms of a DocView with a prefix are the terms the full list gives, in order, with their wdf.
static void test_doc_view_terms() {
	Xapian::Document doc;
	const char* names[] = { "A1", "Aa", "B", "Ba", "Bb", "C1", "a", "ab", "b" };
	Lcg rng{3};
	for (const auto* name : names) {
		doc.add_term(name, 1 + rng.next(5));
	}
	auto view = engine::xapian::make_view(doc);
	for (std::string prefix : { "", "A", "B", "Ba", "C", "D", "a", "ab", "abc", "b", "c", "\xff" }) {
		std::vector<std::pair<std::string, engine::termcount>> got;
		view->terms(prefix, [&](std::string_view term, engine::termcount wdf) { got.emplace_back(term, wdf); });
		std::vector<std::pair<std::string, engine::termcount>> expected;
		for (auto it = doc.termlist_begin(); it != doc.termlist_end(); ++it) {
			if ((*it).compare(0, prefix.size(), prefix) == 0) {
				expected.emplace_back(*it, it.get_wdf());
			}
		}
		CHECK_EQ(got, expected);
	}
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
Xapian::WritableDatabase make_agg_db(const std::vector<Row>& rows, std::size_t from, std::size_t to) {
	Xapian::WritableDatabase db(std::string(), Xapian::DB_BACKEND_INMEMORY);
	for (auto i = from; i < to; ++i) {
		Xapian::Document doc;
		doc.add_term("t");
		if (!rows[i].ints.empty()) {
			std::vector<std::string> values;
			for (auto n : rows[i].ints) {
				values.push_back(Serialise::integer(n));
			}
			doc.add_value(1001, multi(values));
		}
		doc.add_value(1002, "n" + std::to_string(i % 3));
		doc.set_data("d");
		db.add_document(doc);
	}
	return db;
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

std::string run_spy(const Xapian::Database& db, engine::MatchSpy& spy) {
	engine::xapian::MatchSpyAdapter adapter(spy);
	return std::to_string(sorted(db, nullptr, &adapter).size());
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

	auto whole_db = make_agg_db(rows, 0, rows.size());
	AggregationMatchSpy whole(config, schema);
	CHECK_EQ(run_spy(whole_db, whole), std::to_string(rows.size()));
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
	auto first_db = make_agg_db(rows, 0, 6);
	auto second_db = make_agg_db(rows, 6, rows.size());
	AggregationMatchSpy first(config, schema), second(config, schema);
	run_spy(first_db, first);
	run_spy(second_db, second);
	first.merge_results(second.serialise_results());
	CHECK_TRUE(same_aggregations(first.get_aggregation(), result));

	// The adapter forwards the merge, which is how Xapian folds in a remote shard's results.
	AggregationMatchSpy third(config, schema), fourth(config, schema);
	run_spy(first_db, third);
	run_spy(second_db, fourth);
	engine::xapian::MatchSpyAdapter merging(third);
	merging.merge_results(fourth.serialise_results());
	CHECK_TRUE(same_aggregations(third.get_aggregation(), result));

	// By name through a registry, the way the remote server rebuilds it.
	Xapian::Registry registry;
	engine::xapian::register_match_spy(registry, AggregationMatchSpy());
	const auto* prototype = registry.get_match_spy("AggregationMatchSpy");
	CHECK_TRUE(prototype != nullptr);
	std::unique_ptr<Xapian::MatchSpy> remote(prototype->unserialise(whole.serialise(), registry));
	CHECK_EQ(sorted(whole_db, nullptr, remote.get()).size(), rows.size());
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
	Lcg rng{19};
	auto schema = make_schema();
	int seen = 0, thrown = 0;
	for (int i = 0; i < 2000; ++i) {
		Xapian::Document doc;
		std::string age, name;
		for (auto n = rng.next(20); n > 0; --n) {
			age.push_back(static_cast<char>(rng.next(256)));
		}
		for (auto n = rng.next(10); n > 0; --n) {
			name.push_back(static_cast<char>(rng.next(256)));
		}
		doc.add_value(1001, age);
		doc.add_value(1002, name);
		auto view = engine::xapian::make_view(doc);
		AggregationMatchSpy spy(aggs_config(), schema);
		try {
			spy.see(*view, 1.0);
			++seen;
		} catch (const SerialisationError&) {
			++thrown;
		} catch (const Xapian::Error&) {
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

	Xapian::Document doc;
	doc.add_value(1, Serialise::integer(INT64_MIN));
	auto view = engine::xapian::make_view(doc);
	Multi_MultiValueKeyMaker maker;
	maker.add_integer(1, false, 5);
	CHECK_EQ(maker.key(*view), Serialise::integer(INT64_MAX));
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
					Xapian::Document doc;
					doc.add_value(1, StringList::serialise(vs.begin(), vs.end()));
					d.key = maker.key(*engine::xapian::make_view(doc));
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

// Xapian's exceptions are not std::exception: report them as a failure of the test.
#define GUARDED(fn) \
	static void fn##_guarded() { \
		try { \
			fn(); \
		} catch (const Xapian::Error& exc) { \
			std::fprintf(stderr, "  FAIL %s: threw %s\n", #fn, exc.get_description().c_str()); \
			++testh::failures; \
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
GUARDED(test_adapter_ownership)
GUARDED(test_doc_view_terms)
GUARDED(test_unserialise_saturates)
GUARDED(test_aggregation_spy)
GUARDED(test_aggregation_spy_garbage)
GUARDED(test_aggregation_spy_corrupt_values)

TEST_MAIN(test_sort_integer_guarded, test_sort_integer_extremes_guarded, test_sort_float_text_and_pairs_guarded, test_key_maker_round_trip_guarded, test_key_maker_unserialise_garbage_guarded, test_key_maker_corrupt_values_guarded, test_adapter_ownership_guarded, test_doc_view_terms_guarded, test_unserialise_saturates_guarded, test_aggregation_spy_guarded, test_aggregation_spy_garbage_guarded, test_aggregation_spy_corrupt_values_guarded)
