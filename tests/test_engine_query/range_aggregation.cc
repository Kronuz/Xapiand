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

// Range aggregations with open bounds, in each field type, against a filter's counts.

#include "support.h"

namespace detail {

namespace {

// Slots of the range corpus: one field per type.
constexpr engine::valueno INT_SLOT = 1201, POS_SLOT = 1202, FLT_SLOT = 1203, DATE_SLOT = 1204;

std::shared_ptr<Schema> range_schema() {
	MsgPack props = MsgPack::MAP();
	props["i"] = MsgPack({ { "_type", "integer" }, { "_slot", 1201 } });
	props["p"] = MsgPack({ { "_type", "positive" }, { "_slot", 1202 } });
	props["f"] = MsgPack({ { "_type", "floating" }, { "_slot", 1203 } });
	props["d"] = MsgPack({ { "_type", "date" }, { "_slot", 1204 } });
	return std::make_shared<Schema>(std::make_shared<const MsgPack>(std::move(props)), nullptr, "");
}

// A range as the request gives it: an end that is not set is open.
template <typename T>
struct Rng {
	std::optional<T> from, to;
};

template <typename T>
MsgPack ranges_of(const std::vector<Rng<T>>& ranges) {
	MsgPack list = MsgPack::ARRAY();
	std::size_t n = 0;
	for (const auto& r : ranges) {
		MsgPack range = MsgPack::MAP();
		range["_key"] = MsgPack("r" + std::to_string(n++));
		if (r.from) {
			range["_from"] = *r.from;
		}
		if (r.to) {
			range["_to"] = *r.to;
		}
		list.push_back(range);
	}
	return list;
}

// The documents counted in each range ("r0", "r1", ...) by a filter over the values; an empty range has no entry.
template <typename T>
std::map<std::string, std::int64_t> brute_counts(const std::vector<Rng<T>>& ranges, const std::vector<T>& values) {
	std::map<std::string, std::int64_t> counts;
	for (std::size_t n = 0; n < ranges.size(); ++n) {
		for (const auto value : values) {
			if ((!ranges[n].from || value >= *ranges[n].from) && (!ranges[n].to || value < *ranges[n].to)) {
				++counts["r" + std::to_string(n)];
			}
		}
	}
	return counts;
}

// The buckets a range aggregation over `field` makes, one document per serialized value.
std::map<std::string, std::int64_t> range_counts(const char* field, const MsgPack& ranges, engine::valueno slot, const std::vector<std::string>& serialized) {
	const auto config = MsgPack({ { "_aggs", { { "r", { { "_range", { { "_field", field }, { "_ranges", ranges } } } } } } } });
	AggregationMatchSpy spy(config, range_schema());
	for (const auto& value : serialized) {
		const std::vector<std::string> one = { value };
		const testsupport::MapView view({ { slot, StringList::serialize(one.begin(), one.end()) } });
		spy.see(view, 1.0);
	}
	std::map<std::string, std::int64_t> counts;
	for (const auto& bucket : spy.get_aggregation().at("_aggregations").at("r")) {
		counts[std::string(bucket.at("_key").str_view())] = bucket.at("_doc_count").as_i64();
	}
	return counts;
}

// The keys of the buckets a range aggregation makes, in the order the aggregation returns them under `sort`.
std::vector<std::string> range_order(const char* field, const MsgPack& ranges, engine::valueno slot, const std::vector<std::string>& serialized, const MsgPack& sort) {
	const auto config = MsgPack({ { "_aggs", { { "r", { { "_range", { { "_field", field }, { "_ranges", ranges }, { "_sort", sort } } } } } } } });
	AggregationMatchSpy spy(config, range_schema());
	for (const auto& value : serialized) {
		const std::vector<std::string> one = { value };
		const testsupport::MapView view({ { slot, StringList::serialize(one.begin(), one.end()) } });
		spy.see(view, 1.0);
	}
	std::vector<std::string> keys;
	for (const auto& bucket : spy.get_aggregation().at("_aggregations").at("r")) {
		keys.emplace_back(bucket.at("_key").str_view());
	}
	return keys;
}

template <typename T, typename Serializer>
void check_ranges(const char* field, engine::valueno slot, const std::vector<T>& values, const std::vector<std::vector<Rng<T>>>& requests, Serializer serialize) {
	std::vector<std::string> serialized;
	for (const auto value : values) {
		serialized.push_back(serialize(value));
	}
	for (const auto& request : requests) {
		const auto got = range_counts(field, ranges_of(request), slot, serialized);
		const auto want = brute_counts(request, values);
		if (got != want) {
			std::fprintf(stderr, "  FAIL: ranges over %s differ from the filter:", field);
			for (const auto& r : request) {
				std::fprintf(stderr, " [%s, %s)", r.from ? std::to_string(*r.from).c_str() : "", r.to ? std::to_string(*r.to).c_str() : "");
			}
			std::fprintf(stderr, "\n");
			++testh::failures;
		}
	}
}

}  // namespace

// A range bound left out covers every value on its side, in the field's own type: counts equal a filter's, at
// the extremes of int64 and with negative bounds, for integer, positive, float and date fields.
void test_range_aggregation_open_bounds() {
	constexpr auto none = std::nullopt;
	constexpr std::int64_t lo = std::numeric_limits<std::int64_t>::min(), hi = std::numeric_limits<std::int64_t>::max(), big = std::int64_t(1) << 62;
	using I = Rng<std::int64_t>;

	const std::vector<std::int64_t> ints = { lo, -big, -5, 0, 3, 10, big, hi };
	check_ranges<std::int64_t>("i", INT_SLOT, ints, {
		{ I{ none, 10 }, I{ 0, none } },
		{ I{ none, 0 } }, { I{ 0, none } }, { I{ -10, none } }, { I{ none, -10 } },
		{ I{ none, 3 }, I{ 3, none } },
		{ I{ 0, 10 } },
		{ I{ none, none } },
		{ I{ lo, none } }, { I{ none, hi } }, { I{ hi, none } }, { I{ none, lo } },
		{ I{ -big, big } }, { I{ lo, hi } },
		{ I{ none, big }, I{ big, none } },
		{ I{ none, -5 }, I{ -5, 3 }, I{ 3, none } },
	}, [](std::int64_t v) { return Serialize::integer(v); });

	// The case of the audit, with its default keys.
	{
		const std::vector<std::int64_t> values = { 3, 10, big, -big };
		MsgPack list = MsgPack::ARRAY();
		MsgPack to = MsgPack::MAP(), from = MsgPack::MAP();
		to["_to"] = 10;
		from["_from"] = 0;
		list.push_back(to);
		list.push_back(from);
		std::vector<std::string> serialized;
		for (auto v : values) {
			serialized.push_back(Serialize::integer(v));
		}
		const auto got = range_counts("i", list, INT_SLOT, serialized);
		CHECK_EQ(got, (std::map<std::string, std::int64_t>{ { "..10", 2 }, { "0..", 3 } }));
	}

	// A bound beyond int64, or fractional, is placed exactly: past the values it is open or empty.
	{
		const std::vector<std::int64_t> values = { lo, -5, 0, 3, 10, hi };
		std::vector<std::string> serialized;
		for (auto v : values) {
			serialized.push_back(Serialize::integer(v));
		}
		const auto count = [&](std::optional<MsgPack> from, std::optional<MsgPack> to) {
			MsgPack range = MsgPack::MAP();
			range["_key"] = MsgPack("r");
			if (from) {
				range["_from"] = *from;
			}
			if (to) {
				range["_to"] = *to;
			}
			MsgPack list = MsgPack::ARRAY();
			list.push_back(range);
			const auto got = range_counts("i", list, INT_SLOT, serialized);
			return got.empty() ? std::int64_t(0) : got.at("r");
		};
		const MsgPack past_max(std::uint64_t(1) << 63), huge(1e30), neg_huge(-1e30);
		CHECK_EQ(count(none, past_max), std::int64_t(6));
		CHECK_EQ(count(past_max, none), std::int64_t(0));
		CHECK_EQ(count(none, huge), std::int64_t(6));
		CHECK_EQ(count(huge, none), std::int64_t(0));
		CHECK_EQ(count(neg_huge, none), std::int64_t(6));
		CHECK_EQ(count(none, neg_huge), std::int64_t(0));
		CHECK_EQ(count(MsgPack(2.5), MsgPack(10.5)), std::int64_t(2));    // 3 and 10
		CHECK_EQ(count(MsgPack(-0.5), MsgPack(3.0)), std::int64_t(1));    // 0
		CHECK_EQ(count(none, MsgPack(-4.5)), std::int64_t(2));            // lo and -5
	}

	// Positive fields: a negative bound is open (from) or empty (to).
	{
		using U = Rng<std::uint64_t>;
		constexpr std::uint64_t ubig = std::uint64_t(1) << 62, ubigger = std::uint64_t(1) << 63;
		const std::vector<std::uint64_t> values = { 0, 3, 10, ubig, ubigger };
		check_ranges<std::uint64_t>("p", POS_SLOT, values, {
			{ U{ none, 10 }, U{ 3, none } },
			{ U{ none, 0 } }, { U{ 0, none } }, { U{ none, none } },
			{ U{ ubig, none } }, { U{ none, ubig } }, { U{ ubigger, none } },
			{ U{ 3, ubig } },
		}, [](std::uint64_t v) { return Serialize::positive(v); });

		std::vector<std::string> serialized;
		for (auto v : values) {
			serialized.push_back(Serialize::positive(v));
		}
		MsgPack below = MsgPack::MAP(), within = MsgPack::MAP(), empty = MsgPack::MAP();
		below["_key"] = MsgPack("below");
		below["_from"] = -5;
		within["_key"] = MsgPack("within");
		within["_from"] = -5;
		within["_to"] = 4;
		empty["_key"] = MsgPack("empty");
		empty["_to"] = -1;
		MsgPack list = MsgPack::ARRAY();
		list.push_back(below);
		list.push_back(within);
		list.push_back(empty);
		const auto got = range_counts("p", list, POS_SLOT, serialized);
		CHECK_EQ(got, (std::map<std::string, std::int64_t>{ { "below", 5 }, { "within", 2 } }));
	}

	// Default keys show the bounds as written, so ranges that differ as written never share a bucket.
	{
		const auto bound = [](const char* name, MsgPack value) {
			MsgPack r = MsgPack::MAP();
			r[name] = value;
			return r;
		};
		const auto both = [](MsgPack from, MsgPack to) {
			MsgPack r = MsgPack::MAP();
			r["_from"] = from;
			r["_to"] = to;
			return r;
		};
		using Counts = std::map<std::string, std::int64_t>;
		const std::vector<std::int64_t> ints = { 1, 3, 5 };
		std::vector<std::string> serialized;
		for (auto v : ints) {
			serialized.push_back(Serialize::integer(v));
		}
		MsgPack list = MsgPack::ARRAY();
		list.push_back(bound("_from", MsgPack(2.5)));
		list.push_back(bound("_from", MsgPack(3)));
		CHECK_EQ(range_counts("i", list, INT_SLOT, serialized), (Counts{ { "2.5..", 2 }, { "3..", 2 } }));

		const std::vector<std::uint64_t> naturals = { 1, 3, 5 };
		serialized.clear();
		for (auto v : naturals) {
			serialized.push_back(Serialize::positive(v));
		}
		list = MsgPack::ARRAY();
		list.push_back(both(MsgPack(-5), MsgPack(4)));
		list.push_back(bound("_to", MsgPack(4)));
		CHECK_EQ(range_counts("p", list, POS_SLOT, serialized), (Counts{ { "-5..4", 2 }, { "..4", 2 } }));
	}

	// Sorted by key, an open lower bound comes first (below every bound), last when descending.
	{
		const std::vector<std::int64_t> values = { -100, -20, 0, 7 };
		std::vector<std::string> serialized;
		for (auto v : values) {
			serialized.push_back(Serialize::integer(v));
		}
		const auto range = [](const char* key, std::optional<int> from, std::optional<int> to) {
			MsgPack r = MsgPack::MAP();
			r["_key"] = MsgPack(key);
			if (from) {
				r["_from"] = *from;
			}
			if (to) {
				r["_to"] = *to;
			}
			return r;
		};
		MsgPack list = MsgPack::ARRAY();
		list.push_back(range("mid", -5, 5));
		list.push_back(range("high", 5, none));
		list.push_back(range("open", none, -50));
		list.push_back(range("neg", -50, -10));
		const auto order = [&](const char* direction) {
			return range_order("i", list, INT_SLOT, serialized, MsgPack({ { "_key", direction } }));
		};
		CHECK_EQ(order("asc"), (std::vector<std::string>{ "open", "neg", "mid", "high" }));
		CHECK_EQ(order("desc"), (std::vector<std::string>{ "high", "mid", "neg", "open" }));
	}

	// Float and date fields: open sides cover the negative values too.
	{
		using F = Rng<double>;
		const std::vector<double> values = { -100.5, -2.5, 0.0, 1.5, 3.0, 1000.25 };
		check_ranges<double>("f", FLT_SLOT, values, {
			{ F{ none, 0.0 }, F{ 0.0, none } },
			{ F{ none, -2.5 } }, { F{ -2.5, none } }, { F{ none, none } }, { F{ -3.0, 1.5 } }, { F{ 1.5, 3.0 } },
		}, [](double v) { return Serialize::floating(v); });
		const std::vector<double> times = { -1.0e9, -1.0, 0.0, 1.5e9, 4.0e9 };
		check_ranges<double>("d", DATE_SLOT, times, {
			{ F{ none, 0.0 }, F{ 0.0, none } },
			{ F{ none, -1.0 } }, { F{ -1.0, none } }, { F{ none, none } }, { F{ -1.0e9, 1.5e9 } },
		}, [](double v) { return Serialize::timestamp(v); });
	}
}

}  // namespace detail
