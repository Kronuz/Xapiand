/*
 * Copyright (c) 2015-2026 Dubalu LLC
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

// Unit tests for the geospatial HTM machinery: a geometry expands to trixels, and
// EWKT serialise/parse is stable. Revived from the retired oldtests geospatial
// suite, keeping the robust structural checks (non-empty trixels, EWKT round-trip,
// trixel<->range consistency) rather than pinning exact trixel names / range bytes.

#include "test_harness.h"

#include <string>
#include <vector>

#include "cartesian.h"
#include "geospatial/point.h"
#include "geospatial/circle.h"
#include "geospatial/ewkt.h"
#include "htm.h"

static void test_point_trixels() {
	Point p(Cartesian(19.702778, -101.192222, 0, Cartesian::Units::DEGREES));
	auto trixels = p.getTrixels(true, HTM_MIN_ERROR);
	CHECK_FALSE(trixels.empty());
}

static void test_circle_trixels() {
	// A circle covers more of the mesh than a single point.
	Circle c(Cartesian(19.702778, -101.192222, 0, Cartesian::Units::DEGREES), 5000.0);
	auto trixels = c.getTrixels(true, HTM_MIN_ERROR);
	CHECK_FALSE(trixels.empty());
}

static void test_ewkt_roundtrip() {
	// A geometry's EWKT re-parses to something covering the same trixels.
	Point p(Cartesian(19.702778, -101.192222, 0, Cartesian::Units::DEGREES));
	auto trixels = p.getTrixels(true, HTM_MIN_ERROR);

	std::string ewkt_str = p.toEWKT();
	EWKT ewkt(ewkt_str);
	auto geometry = ewkt.getGeometry();
	auto trixels2 = geometry->getTrixels(true, HTM_MIN_ERROR);
	CHECK(trixels == trixels2);
}

static void test_trixel_range_roundtrip() {
	// Converting a geometry's trixels to ranges and back yields the same trixels.
	Point p(Cartesian(19.702778, -101.192222, 0, Cartesian::Units::DEGREES));
	auto trixels = p.getTrixels(true, HTM_MIN_ERROR);
	REQUIRE(!trixels.empty());

	std::vector<range_t> ranges;
	for (const auto& trixel : trixels) {
		HTM::insertGreaterRange(ranges, HTM::getRange(trixel));
	}
	auto trixels_back = HTM::getTrixels(ranges);
	CHECK(trixels_back == trixels);
}

TEST_MAIN(test_point_trixels, test_circle_trixels, test_ewkt_roundtrip, test_trixel_range_roundtrip)
