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

// A geometry's exact-match term (Serialize::ranges_hash) is equal exactly when its trixel ranges are: distinct points,
// and range lists whose start-xor-end values cancel, get distinct terms.

#include "test_harness.h"

#include <cstdio>
#include <map>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include "database/schema.h"
#include "database/utils.h"
#include "geospatial/geospatial.h"
#include "msgpack.h"
#include "rapidjson/document.h"
#include "serialize.h"

namespace {

MsgPack js(const std::string& json) {
	rapidjson::Document doc;
	json_load(doc, json);
	return MsgPack(doc);
}

std::vector<range_t> ranges_of(const std::string& ewkt) {
	return EWKT(ewkt).getGeometry()->getRanges(DEFAULT_GEO_PARTIALS, DEFAULT_GEO_ERROR);
}

std::string point(double lon, double lat) {
	char buffer[64];
	std::snprintf(buffer, sizeof buffer, "POINT(%.6f %.6f)", lon, lat);
	return buffer;
}

void test_distinct_points_get_distinct_terms() {
	CHECK_NE(Serialize::geospatial(std::string_view("POINT(-101.19 19.70)")), Serialize::geospatial(std::string_view("POINT(2.35 48.85)")));
	CHECK_NE(Serialize::ranges_hash({ range_t(7, 7) }), Serialize::ranges_hash({ range_t(8, 8) }));
	// The same point spelled as EWKT and as a cast object is one term.
	CHECK_EQ(Serialize::geospatial(js(R"({"_point":{"_latitude":19.70,"_longitude":-101.19}})")),
		Serialize::geospatial(std::string_view("POINT(-101.19 19.70)")));
}

// Lists whose ranges xor to the same value, or repeat a range, get distinct terms.
void test_range_lists_that_xor_alike_get_distinct_terms() {
	const std::vector<std::vector<range_t>> lists = {
		{},
		{ range_t(1, 2) },
		{ range_t(1, 2), range_t(1, 2) },
		{ range_t(1, 2), range_t(3, 4) },
		{ range_t(1, 4), range_t(2, 3) },
		{ range_t(1, 1), range_t(2, 2) },
		{ range_t(3, 3) },
		{ range_t(0, 3) },
	};
	for (std::size_t i = 0; i < lists.size(); ++i) {
		for (std::size_t j = 0; j < lists.size(); ++j) {
			CHECK_EQ(Serialize::ranges_hash(lists[i]) == Serialize::ranges_hash(lists[j]), i == j);
		}
	}
}

// 2,000 random points and 200 random circles: two terms are equal exactly when the trixel ranges are, and every
// term is 16 bytes, so none equals a term the length-prefixed 64-bit hash wrote (at most 10 bytes).
void test_terms_are_equal_exactly_when_ranges_are() {
	std::mt19937_64 rng(0x6e0ULL);
	std::uniform_real_distribution<double> lon(-180.0, 180.0);
	std::uniform_real_distribution<double> lat(-90.0, 90.0);
	std::uniform_real_distribution<double> radius(10.0, 5000.0);
	std::vector<std::string> shapes;
	for (int i = 0; i < 2000; ++i) {
		shapes.push_back(point(lon(rng), lat(rng)));
	}
	for (int i = 0; i < 200; ++i) {
		char buffer[96];
		std::snprintf(buffer, sizeof buffer, "CIRCLE(%.6f %.6f, %.1f)", lon(rng), lat(rng), radius(rng));
		shapes.push_back(buffer);
	}
	std::map<std::string, std::string> ranges_by_term;
	int mismatches = 0;
	for (const auto& shape : shapes) {
		const auto ranges = ranges_of(shape);
		const auto term = Serialize::geospatial(std::string_view(shape));
		CHECK_EQ(term, Serialize::ranges_hash(ranges));
		CHECK_EQ(term.size(), static_cast<std::size_t>(16));
		const auto serialized = Serialize::ranges(ranges);
		const auto [it, inserted] = ranges_by_term.emplace(term, serialized);
		if (!inserted && it->second != serialized) {
			if (++mismatches <= 5) {
				std::fprintf(stderr, "  FAIL: %s shares its term with other ranges\n", shape.c_str());
			}
		}
	}
	CHECK_EQ(mismatches, 0);
	CHECK_GT(ranges_by_term.size(), static_cast<std::size_t>(2100));
}

}  // namespace

TEST_MAIN(test_distinct_points_get_distinct_terms, test_range_lists_that_xor_alike_get_distinct_terms, test_terms_are_equal_exactly_when_ranges_are)
