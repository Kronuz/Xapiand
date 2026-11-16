/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
 * Copyright (c) 2015-2018 Dubalu LLC
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

// Unit tests for value serialisation round-trips. Revived from the retired
// oldtests serialise suite, keeping the robust round-trip checks (serialise ->
// unserialise is stable / recovers the input) and dropping the byte-exact repr()
// assertions, which pinned internal encodings. Cartesian / range serialisation is
// geospatial and belongs with the geospatial tests.

#include "test_harness.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "datetime.h"
#include "exact.hh"   // sortable's codec-independent comparator (its test/ directory)
#include "exception_engine.h"
#include "serialise.h"

static void test_datetime_timestamp() {
	// The Unix epoch is timestamp 0 (a fixed, timezone-independent anchor).
	auto tm = Datetime::DatetimeParser(std::string_view("1970-01-01T00:00:00.000"));
	CHECK_EQ(Datetime::timestamp(tm), 0.0);
}

static void test_datetime_roundtrip() {
	// Serialising the unserialised form must be stable: serialise -> unserialise
	// -> serialise reproduces the same bytes for any accepted datetime.
	const char* datetimes[] = {
		"2015-10-10T23:03:03.000",
		"1970-01-01T00:00:00.000",
		"2000-02-29T12:00:00.000",
		"2019-06-17T08:15:30.500",
	};
	for (const auto* d : datetimes) {
		std::string serialised = Serialise::datetime(std::string_view(d));
		std::string canonical = Unserialise::datetime(serialised);
		std::string reserialised = Serialise::datetime(std::string_view(canonical));
		CHECK_EQ(serialised, reserialised);
	}
}

static void test_uuid_roundtrip() {
	// A textual UUID serialises and unserialises back to the same textual UUID under the
	// default (v6) codec: version-4 UUIDs keep their full 16 bytes, and version-6 UUIDs
	// round-trip through the condensed wire. A version-1 UUID does NOT round-trip here: the
	// condensed wire carries no version marker, so it decodes with the running codec (v6 by
	// default) and renders as v6, the documented 1.0 id break (see docs/.../upgrade.md).
	// --legacy-ids (the v1 codec) round-trips a v1 id exactly.
	const char* uuids[] = {
		"3c0f2be3-ff4f-40ab-b157-c51a81eff176",   // v4, full form
		"e47fcfdf-8db6-4469-a97f-57146dc41ced",   // v4, full form
		"00000000-0000-6000-8000-010000000000",   // v6, condensed (the reserved nil-ish id)
		"2280898d-9810-6800-88b4-038db356b8d1",   // v6, condensed + compacted
	};
	for (const auto* u : uuids) {
		std::string serialised = Serialise::uuid(std::string_view(u));
		std::string back = Unserialise::uuid(serialised);
		CHECK_EQ(back, std::string(u));
	}
}

static std::string unhex(std::string_view hex) {
	std::string out;
	for (std::size_t i = 0; i + 1 < hex.size(); i += 2) {
		out.push_back(static_cast<char>(std::strtoul(std::string(hex.substr(i, 2)).c_str(), nullptr, 16)));
	}
	return out;
}

// Every line of the pinned sortable's vectors.txt through Serialise::/Unserialise:: (design T1): the bytes,
// the round trip, one encoding per number whatever its type, and memcmp order equal to exact order.
static void test_number_vectors() {
	std::ifstream in(SORTABLE_TEST_DIR "/vectors.txt");
	CHECK_TRUE(in.good());
	std::vector<std::pair<exact::Key, std::string>> seen;
	std::string line;
	while (std::getline(in, line)) {
		std::istringstream ss(line);
		std::string kind, text, hex;
		ss >> kind >> text >> hex;
		const std::string want = unhex(hex);
		if (kind == "i") {
			const auto v = static_cast<std::int64_t>(std::strtoll(text.c_str(), nullptr, 10));
			CHECK_EQ(exact::hex(Serialise::integer(v)), hex);
			CHECK_EQ(Unserialise::integer(want), v);
			seen.emplace_back(exact::from_int64(v), want);
		} else if (kind == "u") {
			const auto v = static_cast<std::uint64_t>(std::strtoull(text.c_str(), nullptr, 10));
			CHECK_EQ(exact::hex(Serialise::positive(v)), hex);
			CHECK_EQ(Unserialise::positive(want), v);
			if (v <= static_cast<std::uint64_t>(INT64_MAX)) {
				CHECK_EQ(exact::hex(Serialise::integer(static_cast<std::int64_t>(v))), hex);
			}
			seen.emplace_back(exact::from_uint64(v), want);
		} else {
			const double v = std::strtod(text.c_str(), nullptr);
			CHECK_EQ(exact::hex(Serialise::floating(v)), hex);
			const double back = Unserialise::floating(want);
			if (std::isnan(v)) {
				CHECK_TRUE(std::isnan(back));
			} else {
				CHECK_EQ(std::bit_cast<std::uint64_t>(back), std::bit_cast<std::uint64_t>(v == 0 ? 0.0 : v));
			}
			if (v == std::floor(v) && std::fabs(v) < 9.2e18) {
				CHECK_EQ(exact::hex(Serialise::integer(static_cast<std::int64_t>(v))), hex);
			}
			seen.emplace_back(exact::from_double(v), want);
		}
	}
	CHECK_GE(seen.size(), 113000U);
	std::stable_sort(seen.begin(), seen.end(), [](const auto& a, const auto& b) { return exact::compare(a.first, b.first) < 0; });
	for (std::size_t i = 1; i < seen.size(); ++i) {
		const int order = exact::compare(seen[i - 1].first, seen[i].first);
		CHECK_EQ(exact::memcmp_order(seen[i - 1].second, seen[i].second), order);
	}
}

// Bytes that are not a serialised number raise SerialisationError, never a value read past the input.
static void test_number_rejects() {
	for (std::string_view bad : { std::string_view(), std::string_view("\x00", 1), std::string_view("\x7f", 1), std::string_view("\x81", 1) }) {
		bool thrown = false;
		try {
			(void)Unserialise::floating(bad);
		} catch (const SerialisationError&) {
			thrown = true;
		}
		CHECK_TRUE(thrown);
	}
}

TEST_MAIN(test_datetime_timestamp, test_datetime_roundtrip, test_uuid_roundtrip, test_number_vectors, test_number_rejects)
