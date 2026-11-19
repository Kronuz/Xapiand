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

// Coverage tests for src/multivalue/keymaker.cc: the sort key of a multi-valued slot. Each key type picks the
// value of a list that is closest to (ascending) or farthest from (descending) its reference, a missing slot
// sorts last, several slots join into one byte string with the escapes below, and the maker rebuilds by name from
// its serialized form or refuses bytes that are not one with an engine error.

#include "test_harness.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "cartesian.h"
#include "engine/doc_view.h"
#include "engine/error.h"
#include "length.h"
#include "multivalue/keymaker.h"
#include "serialize.h"
#include "serialize_list.h"
#include "sortable.hh"

namespace {

using namespace std::string_literals;

// A document that holds only the values a test gives it.
class Doc final : public engine::DocView {
	std::map<engine::valueno, std::string> _values;

public:
	Doc& set(engine::valueno slot, std::string value) {
		_values[slot] = std::move(value);
		return *this;
	}

	engine::docid did() const override { return 1; }
	std::string data() const override { return {}; }
	std::string value(engine::valueno slot) const override {
		auto it = _values.find(slot);
		return it == _values.end() ? std::string() : it->second;
	}
	engine::rev version() const override { return 1; }
	void values(const engine::ValueSink& sink) const override {
		for (const auto& [slot, value] : _values) {
			sink(slot, value);
		}
	}
	std::string serialize() const override { return {}; }
};

std::string multi(const std::vector<std::string>& values) {
	return StringList::serialize(values.begin(), values.end());
}

// The key of a one-slot maker, ascending: the slot's smallest key, byte for byte.
template <typename Add>
std::string smallest(const Doc& doc, Add&& add) {
	Multi_MultiValueKeyMaker maker;
	add(maker, false);
	return maker.key(doc);
}

// The same descending: the key undone (each byte complemented, a NUL escaped as 0xff 0x00, 0xff 0xff at the end).
template <typename Add>
std::string biggest(const Doc& doc, Add&& add) {
	Multi_MultiValueKeyMaker maker;
	add(maker, true);
	const auto key = maker.key(doc);
	if (key.size() < 2 || key.substr(key.size() - 2) != "\xff\xff"s) {
		return "<no terminator>";
	}
	std::string out;
	for (std::size_t i = 0; i + 2 < key.size(); ++i) {
		const auto ch = static_cast<unsigned char>(255 - static_cast<unsigned char>(key[i]));
		out += static_cast<char>(ch);
		if (ch == 0) {
			++i;  // the escaping NUL
		}
	}
	return out;
}

constexpr engine::valueno SLOT = 5;

struct Floating {
	using V = double;
	static std::string ser(V v) { return Serialize::floating(v); }
	static V dec(std::string_view s) { return Unserialize::floating(s); }
	static void add(Multi_MultiValueKeyMaker& m, bool reverse, V ref) { m.add_float(SLOT, reverse, ref); }
};

struct Integer {
	using V = std::int64_t;
	static std::string ser(V v) { return Serialize::integer(v); }
	static V dec(std::string_view s) { return Unserialize::integer(s); }
	static void add(Multi_MultiValueKeyMaker& m, bool reverse, V ref) { m.add_integer(SLOT, reverse, ref); }
};

struct Positive {
	using V = std::uint64_t;
	static std::string ser(V v) { return Serialize::positive(v); }
	static V dec(std::string_view s) { return Unserialize::positive(s); }
	static void add(Multi_MultiValueKeyMaker& m, bool reverse, V ref) { m.add_positive(SLOT, reverse, ref); }
};

struct Date {
	using V = double;
	static std::string ser(V v) { return Serialize::timestamp(v); }
	static V dec(std::string_view s) { return Unserialize::timestamp(s); }
	static void add(Multi_MultiValueKeyMaker& m, bool reverse, V ref) { m.add_date(SLOT, reverse, ref); }
};

// What a list of values gives, ascending and descending, for the reference.
template <typename T>
struct Case {
	std::vector<typename T::V> values;
	typename T::V smallest;
	typename T::V biggest;
};

template <typename T>
void check_cases(typename T::V ref, const std::vector<Case<T>>& cases) {
	auto add = [&](Multi_MultiValueKeyMaker& m, bool reverse) { T::add(m, reverse, ref); };
	for (const auto& c : cases) {
		std::vector<std::string> sers;
		for (auto v : c.values) {
			sers.push_back(T::ser(v));
		}
		Doc doc;
		doc.set(SLOT, multi(sers));
		const auto lo = smallest(doc, add);
		const auto hi = biggest(doc, add);
		CHECK_EQ(T::dec(lo), c.smallest);
		CHECK_EQ(T::dec(hi), c.biggest);
	}

	// A document without the slot sorts after every value ascending, and before every value descending.
	Doc none;
	CHECK_EQ(smallest(none, add), std::string(sortable::max_finite));
	CHECK_EQ(biggest(none, add), std::string(sortable::min_finite));
}

// Reference 10: the distance of the closest and of the farthest value, for every position of the reference
// among the values (below all, above all, between, equal to a value, one value).
template <typename T>
void check_numeric() {
	check_cases<T>(10, {
		{ { 7 }, 3, 3 },
		{ { 15 }, 5, 5 },
		{ { 12, 15 }, 2, 5 },
		{ { 10, 12 }, 0, 2 },
		{ { 2, 6 }, 4, 8 },
		{ { 8, 10 }, 0, 2 },
		{ { 2, 8, 13, 30 }, 2, 20 },
		{ { 2, 10, 30 }, 0, 20 },
		{ { 2, 5, 8, 13, 30 }, 2, 20 },
		{ { 7, 13 }, 3, 3 },
		{ { 4, 13 }, 3, 6 },
		{ { 8, 19 }, 2, 9 },
	});
}

void test_numeric_keys() {
	check_numeric<Floating>();
	check_numeric<Integer>();
	check_numeric<Positive>();
	check_numeric<Date>();
}

// A fractional distance and a reference between negative values.
void test_floating_and_integer_signs() {
	check_cases<Floating>(10.0, {
		{ { 2.5, 8.25, 13.5 }, 1.75, 7.5 },
		{ { -4.5, -2.25 }, 12.25, 14.5 },
	});
	check_cases<Integer>(-10, {
		{ { -30, -12, -5, 0 }, 2, 20 },
		{ { -7, -4 }, 3, 6 },
		{ { -40, -30 }, 20, 30 },
	});
}

// A distance of 2^63 or more saturates at the largest key and never wraps to a negative one.
void test_integer_saturation() {
	constexpr auto lo = INT64_MIN;
	constexpr auto hi = INT64_MAX;
	check_cases<Integer>(5, { { { lo }, hi, hi }, { { lo, -10 }, 15, hi } });
	check_cases<Integer>(-5, { { { hi }, hi, hi }, { { 0, hi }, 5, hi } });
	check_cases<Integer>(0, { { { lo, hi }, hi, hi } });
}

void test_serialize_key() {
	auto add = [](Multi_MultiValueKeyMaker& m, bool reverse) { m.add_serialize(SLOT, reverse); };

	Doc doc;
	doc.set(SLOT, multi({ "apple", "mango", "pear" }));
	CHECK_EQ(smallest(doc, add), "apple");
	CHECK_EQ(biggest(doc, add), "pear");

	Doc one;
	one.set(SLOT, "solo");
	CHECK_EQ(smallest(one, add), "solo");
	CHECK_EQ(biggest(one, add), "solo");

	// A missing slot sorts after every value ascending. Descending it gives the empty string: the constant's
	// "\x00" literal builds an empty std::string, so it does not hold the NUL the name says (finding M1).
	Doc none;
	CHECK_EQ(smallest(none, add), "\xff"s);
	CHECK_EQ(biggest(none, add), ""s);
}

void test_bool_key() {
	auto yes = [](Multi_MultiValueKeyMaker& m, bool reverse) { m.add_bool(SLOT, reverse, true); };
	auto no = [](Multi_MultiValueKeyMaker& m, bool reverse) { m.add_bool(SLOT, reverse, false); };
	const auto zero = Serialize::floating(0.0);
	const auto one = Serialize::floating(1.0);
	const auto t = Serialize::boolean(true);
	const auto f = Serialize::boolean(false);

	Doc only_true, only_false, both;
	only_true.set(SLOT, t);
	only_false.set(SLOT, f);
	both.set(SLOT, multi({ f, t }));
	Doc two_true;
	two_true.set(SLOT, multi({ t, t }));
	Doc two_false;
	two_false.set(SLOT, multi({ f, f }));

	// Ascending: 0 when some value equals the reference, else 1.
	CHECK_EQ(smallest(only_true, yes), zero);
	CHECK_EQ(smallest(only_false, yes), one);
	CHECK_EQ(smallest(both, yes), zero);
	CHECK_EQ(smallest(both, no), zero);
	CHECK_EQ(smallest(two_true, no), one);
	CHECK_EQ(smallest(two_false, no), zero);
	// Descending: 1 when some value differs from the reference, else 0.
	CHECK_EQ(biggest(only_true, yes), zero);
	CHECK_EQ(biggest(only_false, yes), one);
	CHECK_EQ(biggest(both, yes), one);
	CHECK_EQ(biggest(both, no), one);
	CHECK_EQ(biggest(two_true, yes), zero);
	CHECK_EQ(biggest(two_false, no), zero);

	Doc none;
	CHECK_EQ(smallest(none, yes), std::string(sortable::max_finite));
	CHECK_EQ(biggest(none, yes), std::string(sortable::min_finite));
}

Cartesian at(double lat, double lon) {
	return Cartesian(lat, lon, 0, Cartesian::Units::DEGREES);
}

void test_geo_key() {
	const auto origin = at(0, 0);
	auto add = [&](Multi_MultiValueKeyMaker& m, bool reverse) { m.add_geo(SLOT, reverse, std::vector<Cartesian>{ origin }); };

	// Centroids 10 and 30 degrees north of the reference: the nearest is 10 degrees away, the farthest 30.
	Doc doc;
	doc.set(SLOT, Serialize::centroids({ at(30, 0), at(10, 0) }));
	CHECK_EQ(Unserialize::floating(smallest(doc, add)), origin.distance(at(10, 0)));
	CHECK_EQ(Unserialize::floating(biggest(doc, add)), origin.distance(at(30, 0)));
	CHECK_LT(std::fabs(Unserialize::floating(smallest(doc, add)) - M_PI / 18), 1e-9);
	CHECK_LT(std::fabs(Unserialize::floating(biggest(doc, add)) - M_PI / 6), 1e-9);

	// Several reference centroids: the closest pair, the farthest pair.
	auto two = [&](Multi_MultiValueKeyMaker& m, bool reverse) { m.add_geo(SLOT, reverse, std::vector<Cartesian>{ origin, at(0, 90) }); };
	Doc east;
	east.set(SLOT, Serialize::centroids({ at(0, 80), at(0, -90) }));
	CHECK_LT(std::fabs(Unserialize::floating(smallest(east, two)) - M_PI / 18), 1e-9);
	CHECK_LT(std::fabs(Unserialize::floating(biggest(east, two)) - M_PI), 1e-9);

	Doc none;
	CHECK_EQ(smallest(none, add), std::string(sortable::max_finite));
	CHECK_EQ(biggest(none, add), std::string(sortable::min_finite));
}

// A string key's distance to the reference: the smallest over the values (0 for an exact match) ascending,
// the largest descending.
template <typename Metric, typename Add>
void check_string_metric(Add&& add_key) {
	Metric ref("martha", false);
	const double near = ref.distance(std::string("marhta")), far = ref.distance(std::string("zzz"));
	auto add = [&](Multi_MultiValueKeyMaker& m, bool reverse) { add_key(m, reverse, "martha", false); };

	Doc some;
	some.set(SLOT, multi({ "marhta", "zzz" }));
	CHECK_EQ(smallest(some, add), Serialize::floating(std::min(near, far)));
	CHECK_EQ(biggest(some, add), Serialize::floating(std::max(near, far)));

	Doc exact;
	exact.set(SLOT, multi({ "marhta", "martha", "zzz" }));
	CHECK_EQ(smallest(exact, add), Serialize::floating(0.0));
	CHECK_EQ(biggest(exact, add), Serialize::floating(std::max({ near, far, ref.distance(std::string("martha")) })));

	Doc none;
	CHECK_EQ(smallest(none, add), std::string(sortable::max_finite));
	CHECK_EQ(biggest(none, add), std::string(sortable::min_finite));
}

void test_string_keys() {
	check_string_metric<SerializableLevenshtein>([](auto& m, bool r, std::string_view v, bool i) { m.add_string_levenshtein(SLOT, r, v, i); });
	check_string_metric<SerializableMetric<Jaro>>([](auto& m, bool r, std::string_view v, bool i) { m.add_string_jaro(SLOT, r, v, i); });
	check_string_metric<SerializableJaroWinkler>([](auto& m, bool r, std::string_view v, bool i) { m.add_string_jaro_winkler(SLOT, r, v, i); });
	check_string_metric<SerializableMetric<Sorensen_Dice>>([](auto& m, bool r, std::string_view v, bool i) { m.add_string_sorensen_dice(SLOT, r, v, i); });
	check_string_metric<SerializableMetric<Jaccard>>([](auto& m, bool r, std::string_view v, bool i) { m.add_string_jaccard(SLOT, r, v, i); });
	check_string_metric<SerializableMetric<LCSubstr>>([](auto& m, bool r, std::string_view v, bool i) { m.add_string_lcs(SLOT, r, v, i); });
	check_string_metric<SerializableMetric<LCSubsequence>>([](auto& m, bool r, std::string_view v, bool i) { m.add_string_lcsq(SLOT, r, v, i); });
	check_string_metric<SerializablePhoneticMetric<SoundexEnglish, LCSubsequence>>([](auto& m, bool r, std::string_view v, bool i) { m.add_string_soundex_en(SLOT, r, v, i); });
	check_string_metric<SerializablePhoneticMetric<SoundexFrench, LCSubsequence>>([](auto& m, bool r, std::string_view v, bool i) { m.add_string_soundex_fr(SLOT, r, v, i); });
	check_string_metric<SerializablePhoneticMetric<SoundexGerman, LCSubsequence>>([](auto& m, bool r, std::string_view v, bool i) { m.add_string_soundex_de(SLOT, r, v, i); });
	check_string_metric<SerializablePhoneticMetric<SoundexSpanish, LCSubsequence>>([](auto& m, bool r, std::string_view v, bool i) { m.add_string_soundex_es(SLOT, r, v, i); });
	check_string_metric<SerializablePhoneticMetric<DoubleMetaphone, LCSubsequence>>([](auto& m, bool r, std::string_view v, bool i) { m.add_string_double_metaphone(SLOT, r, v, i); });

	// The case flag: "MARTHA" matches "martha" exactly only when the comparison ignores case.
	Doc doc;
	doc.set(SLOT, "martha");
	auto fold = [](Multi_MultiValueKeyMaker& m, bool reverse) { m.add_string_levenshtein(SLOT, reverse, "MARTHA", true); };
	auto exact = [](Multi_MultiValueKeyMaker& m, bool reverse) { m.add_string_levenshtein(SLOT, reverse, "MARTHA", false); };
	CHECK_EQ(smallest(doc, fold), Serialize::floating(0.0));
	CHECK_NE(smallest(doc, exact), Serialize::floating(0.0));
}

// The language picks the soundex encoder by name; any other language is English.
void test_soundex_language() {
	const std::vector<std::pair<std::string, std::string>> cases = {
		{ "english", "SoundexEnglish" }, { "en", "SoundexEnglish" },
		{ "french", "SoundexFrench" }, { "fr", "SoundexFrench" },
		{ "german", "SoundexGerman" }, { "de", "SoundexGerman" },
		{ "spanish", "SoundexSpanish" }, { "es", "SoundexSpanish" },
		{ "klingon", "SoundexEnglish" }, { "", "SoundexEnglish" },
	};
	for (const auto& [language, name] : cases) {
		Multi_MultiValueKeyMaker maker;
		maker.add_string_soundex(language, SLOT, false, "robert", false);
		const auto serialized = maker.serialize();
		const char* p = serialized.data();
		CHECK_EQ(std::string(unserialize_string(&p, p + serialized.size())), name);
	}
}

// Slots join into one byte string: a forward slot (but the last) ends with 00 00 after its bytes with each NUL
// turned into 00 ff; a descending slot is complemented byte by byte, a NUL becoming ff 00, and ends with ff ff.
void test_key_joins_slots() {
	Multi_MultiValueKeyMaker empty;
	CHECK_EQ(empty.key(Doc()), ""s);

	Doc doc;
	doc.set(1, "a\0b"s).set(2, "xy"s);

	Multi_MultiValueKeyMaker forward_then_reverse;
	forward_then_reverse.add_serialize(1, false);
	forward_then_reverse.add_serialize(2, true);
	CHECK_EQ(forward_then_reverse.key(doc), "a\0\xff""b\0\0\x87\x86\xff\xff"s);

	Multi_MultiValueKeyMaker reverse_then_forward;
	reverse_then_forward.add_serialize(1, true);
	reverse_then_forward.add_serialize(2, false);
	CHECK_EQ(reverse_then_forward.key(doc), "\x9e\xff\0\x9d\xff\xff""xy"s);

	Multi_MultiValueKeyMaker forward_twice;
	forward_twice.add_serialize(1, false);
	forward_twice.add_serialize(2, false);
	CHECK_EQ(forward_twice.key(doc), "a\0\xff""b\0\0xy"s);

	Multi_MultiValueKeyMaker reverse_twice;
	reverse_twice.add_serialize(1, true);
	reverse_twice.add_serialize(2, true);
	CHECK_EQ(reverse_twice.key(doc), "\x9e\xff\0\x9d\xff\xff\x87\x86\xff\xff"s);

	// The joined bytes order documents by the first slot, then by the second.
	Multi_MultiValueKeyMaker pair;
	pair.add_serialize(1, false);
	pair.add_serialize(2, false);
	Doc a, b, c;
	a.set(1, "a"s).set(2, "z"s);
	b.set(1, "ab"s).set(2, "a"s);
	c.set(1, "a"s).set(2, "b"s);
	CHECK_LT(pair.key(a), pair.key(b));
	CHECK_LT(pair.key(c), pair.key(b));
	CHECK_LT(pair.key(c), pair.key(a));
}

Multi_MultiValueKeyMaker every_kind() {
	Multi_MultiValueKeyMaker m;
	m.add_serialize(1, false);
	m.add_float(2, true, 1.5);
	m.add_integer(3, false, -4);
	m.add_positive(4, true, 9u);
	m.add_date(5, false, 86400.0);
	m.add_bool(6, true, true);
	m.add_geo(7, false, std::vector<Cartesian>{ at(10, 20), at(-5, 40) });
	m.add_string_levenshtein(8, false, "alpha", true);
	m.add_string_jaro(9, true, "beta", false);
	m.add_string_jaro_winkler(10, false, "gamma", false);
	m.add_string_sorensen_dice(11, true, "delta", true);
	m.add_string_jaccard(12, false, "epsilon", false);
	m.add_string_lcs(13, true, "zeta", false);
	m.add_string_lcsq(14, false, "eta", false);
	m.add_string_soundex("english", 15, true, "theta", false);
	m.add_string_soundex("french", 16, false, "iota", false);
	m.add_string_soundex("german", 17, true, "kappa", false);
	m.add_string_soundex("spanish", 18, false, "lambda", false);
	m.add_string_double_metaphone(19, true, "mu", false);
	return m;
}

Doc every_doc() {
	Doc doc;
	doc.set(1, "text"s);
	doc.set(2, multi({ Serialize::floating(0.5), Serialize::floating(4.0) }));
	doc.set(3, Serialize::integer(7));
	doc.set(4, multi({ Serialize::positive(3u), Serialize::positive(30u) }));
	doc.set(5, Serialize::timestamp(1000.0));
	doc.set(6, Serialize::boolean(false));
	doc.set(7, Serialize::centroids({ at(11, 21) }));
	for (engine::valueno slot = 8; slot <= 19; ++slot) {
		doc.set(slot, multi({ "alpha", "gamma", "omega" }));
	}
	return doc;
}

// Every key type rebuilds from its serialized form: the same bytes back, the same key, and a clone keys the same.
void test_round_trip_every_kind() {
	const auto maker = every_kind();
	const auto doc = every_doc();
	const auto key = maker.key(doc);
	CHECK_FALSE(key.empty());
	CHECK_EQ(maker.name(), "Multi_MultiValueKeyMaker");

	const auto serialized = maker.serialize();
	auto rebuilt = Multi_MultiValueKeyMaker().unserialize(serialized);
	CHECK_EQ(rebuilt->serialize(), serialized);
	CHECK_EQ(rebuilt->key(doc), key);
	CHECK_EQ(rebuilt->name(), "Multi_MultiValueKeyMaker");

	auto copy = maker.clone();
	CHECK_EQ(copy->serialize(), serialized);
	CHECK_EQ(copy->key(doc), key);

	// Every slot's name is in the serialized form, in the order the slots were added.
	std::vector<std::string> names;
	const char* p = serialized.data();
	const char* end = p + serialized.size();
	while (p != end) {
		names.emplace_back(unserialize_string(&p, end));
		unserialize_string(&p, end);
	}
	CHECK_EQ(names.size(), 19u);
	CHECK_EQ(names.front(), "SerializeKey");
	CHECK_EQ(names[1], "FloatKey");
	CHECK_EQ(names[2], "IntegerKey");
	CHECK_EQ(names[3], "PositiveKey");
	CHECK_EQ(names[4], "DateKey");
	CHECK_EQ(names[5], "BoolKey");
	CHECK_EQ(names[6], "GeoKey");
	CHECK_EQ(names[14], "SoundexEnglish");
	CHECK_EQ(names[15], "SoundexFrench");
	CHECK_EQ(names[16], "SoundexGerman");
	CHECK_EQ(names[17], "SoundexSpanish");
	CHECK_EQ(names.back(), "dmetaphone");
}

// The rebuilt maker orders with the reference it was built with, not a default one.
void test_round_trip_keeps_references() {
	Multi_MultiValueKeyMaker maker;
	maker.add_integer(SLOT, false, 100);
	auto rebuilt = Multi_MultiValueKeyMaker().unserialize(maker.serialize());
	Doc near, far;
	near.set(SLOT, Serialize::integer(98));
	far.set(SLOT, Serialize::integer(10));
	CHECK_LT(rebuilt->key(near), rebuilt->key(far));
	CHECK_EQ(Unserialize::integer(rebuilt->key(near)), 2);
	CHECK_EQ(Unserialize::integer(rebuilt->key(far)), 90);
}

void check_refused(const std::string& bytes) {
	++testh::checks;
	try {
		Multi_MultiValueKeyMaker().unserialize(bytes);
	} catch (const engine::Error& e) {
		if (e.kind != engine::errc::invalid || std::string(e.reason()).find("Bad serialized Multi_MultiValueKeyMaker") != 0) {
			std::fprintf(stderr, "  FAIL %s:%d: wrong error: %s\n", __FILE__, __LINE__, e.reason().c_str());
			++testh::failures;
		}
		return;
	} catch (...) {
		std::fprintf(stderr, "  FAIL %s:%d: not an engine::Error\n", __FILE__, __LINE__);
		++testh::failures;
		return;
	}
	std::fprintf(stderr, "  FAIL %s:%d: accepted\n", __FILE__, __LINE__);
	++testh::failures;
}

void test_unserialize_refuses_bad_bytes() {
	// Nothing serialized is a maker with no slots.
	CHECK_EQ(Multi_MultiValueKeyMaker().unserialize("")->key(Doc()), ""s);

	// An unknown key name.
	check_refused(serialize_string("NoSuchKey") + serialize_string(""));

	// Bytes after the key's own fields.
	Multi_MultiValueKeyMaker one;
	one.add_serialize(1, false);
	const auto name = serialize_string("SerializeKey");
	const auto body = serialize_string(serialize_length(1) + serialize_length(0));
	CHECK_EQ(one.serialize(), name + body);
	check_refused(name + serialize_string(serialize_length(1) + serialize_length(0) + "x"));

	// A key whose fields are cut short, a name with no body, and a body cut inside its length.
	check_refused(name + serialize_string(serialize_length(1)));
	check_refused(name);
	check_refused(name + body.substr(0, body.size() - 1));

	Multi_MultiValueKeyMaker floats;
	floats.add_float(1, false, 2.0);
	const auto good = floats.serialize();
	for (std::size_t cut = 1; cut < good.size(); ++cut) {
		bool refused = false;
		try {
			Multi_MultiValueKeyMaker().unserialize(good.substr(0, cut));
		} catch (const engine::Error&) {
			refused = true;
		}
		CHECK(refused);
	}
}

}  // namespace

TEST_MAIN(
	test_numeric_keys,
	test_floating_and_integer_signs,
	test_integer_saturation,
	test_serialize_key,
	test_bool_key,
	test_geo_key,
	test_string_keys,
	test_soundex_language,
	test_key_joins_slots,
	test_round_trip_every_kind,
	test_round_trip_keeps_references,
	test_unserialize_refuses_bad_bytes
)
