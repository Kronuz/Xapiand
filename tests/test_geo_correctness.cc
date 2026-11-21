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

// Correct geospatial answers where the first coverage tests found wrong ones (findings F18-F22, and F23 found beside them): the unparenthesized
// MULTIPOINT form, a collection of an empty intersection, wide circles in a convex, a ring whose circles are not
// added smallest first, and circles that never meet. Each test names the finding it guards. Design notes: docs/design/geo-correctness.md.

#include "test_harness.h"

#include <cmath>
#include <cstdio>
#include <exception>
#include <memory>
#include <string>
#include <typeinfo>
#include <vector>

#include "cartesian.h"
#include "geospatial/circle.h"
#include "geospatial/collection.h"
#include "geospatial/convex.h"
#include "geospatial/ewkt.h"
#include "geospatial/exception.h"
#include "geospatial/geospatial.h"
#include "geospatial/intersection.h"
#include "geospatial/multiconvex.h"
#include "geospatial/multipoint.h"
#include "geospatial/point.h"
#include "database/utils.h"
#include "htm.h"
#include "msgpack.h"
#include "rapidjson/document.h"

namespace {

constexpr double error = HTM_MIN_ERROR;
constexpr double half_circumference = M_PI * M_PER_RADIUS_EARTH;

Cartesian at(double lat, double lon) {
	return Cartesian(lat, lon, 0, Cartesian::Units::DEGREES);
}

bool covers(const std::vector<range_t>& ranges, const Cartesian& c) {
	const auto id = HTM::getId(c);
	for (const auto& r : ranges) {
		if (r.start <= id && id <= r.end) {
			return true;
		}
	}
	return false;
}

// The point `degrees` of arc from (lat, lon) along `bearing`, in degrees.
Cartesian toward(double lat, double lon, double bearing, double degrees) {
	const double rad = M_PI / 180.0;
	const double phi = lat * rad, lambda = lon * rad, theta = bearing * rad, delta = degrees * rad;
	const double phi2 = std::asin(std::sin(phi) * std::cos(delta) + std::cos(phi) * std::sin(delta) * std::cos(theta));
	const double lambda2 = lambda + std::atan2(std::sin(theta) * std::sin(delta) * std::cos(phi), std::cos(delta) - std::sin(phi) * std::sin(phi2));
	double lon2 = lambda2 / rad;
	while (lon2 > 180.0) { lon2 -= 360.0; }
	while (lon2 < -180.0) { lon2 += 360.0; }
	return at(phi2 / rad, lon2);
}

// What parsing `text` raises: its message when it is an EWKTError, "<other error>" for anything else, empty when it parses.
std::string ewkt_error(const std::string& text) {
	try {
		(void)EWKT(text).getGeometry();
	} catch (const EWKTError& e) {
		return e.what();
	} catch (...) {
		return "<other error>";
	}
	return {};
}

std::vector<range_t> ewkt_ranges(const std::string& text) {
	return EWKT(text).getGeometry()->getRanges(true, error);
}

// --- F18: the MULTIPOINT form without inner parentheses ------------------------------------------

void test_multipoint_without_parentheses_parses() {
	// It is the documented form '(lon lat [height], ..., lon lat [height])'; it covers what the parenthesized form does.
	const std::string parenthesized = "MULTIPOINT((1 2), (3 4))";
	for (const char* text : { "MULTIPOINT(1 2, 3 4)", "MULTIPOINT(1 2,3 4)" }) {
		CHECK_EQ(ewkt_error(text), std::string());
		CHECK(ewkt_ranges(text) == ewkt_ranges(parenthesized));
		CHECK(EWKT(text).getGeometry()->getType() == Geometry::Type::MULTIPOINT);
	}
	CHECK_EQ(ewkt_error("MULTIPOINT(1 2)"), std::string());
	CHECK(ewkt_ranges("MULTIPOINT(1 2)") == ewkt_ranges("MULTIPOINT((1 2))"));
	CHECK(ewkt_ranges("MULTIPOINT(1 2 3, 4 5 6)") == ewkt_ranges("MULTIPOINT((1 2 3), (4 5 6))"));
	CHECK(covers(ewkt_ranges("MULTIPOINT(1 2, 3 4, 5 6)"), at(4, 3)));
}

void test_multipoint_errors_are_ewkt_errors() {
	// A malformed point raises an EWKTError (a 400), not another type (a 500).
	for (const char* text : { "MULTIPOINT(1 2, 3)", "MULTIPOINT(1 2,)", "MULTIPOINT()", "MULTIPOINT(a b)", "MULTIPOINT(1 2, 3 b)", "MULTIPOINT((1 2), (3))", "POINT(1)", "CIRCLE(1, 5)" }) {
		const auto message = ewkt_error(text);
		if (message.empty() || message == "<other error>") {
			std::fprintf(stderr, "  FAIL: '%s' raised '%s'\n", text, message.c_str());
		}
		CHECK(!message.empty() && message != "<other error>");
	}
	CHECK(ewkt_error("MULTIPOINT(1 2, 3)").find("Specification for MULTIPOINT is") != std::string::npos);
}

// --- F19: a collection of an empty intersection ------------------------------------------------

void test_collection_of_empty_intersection_is_empty() {
	Collection collection;
	collection.add_intersection(Intersection());
	CHECK(collection.empty());
	CHECK_EQ(collection.toWKT(), std::string("GEOMETRYCOLLECTION EMPTY"));
	CHECK(collection.getRanges(true, error).empty());

	// It writes and parses back as the empty collection it is.
	const auto reparsed = EWKT(collection.toEWKT()).getGeometry();
	REQUIRE(reparsed != nullptr);
	CHECK(reparsed->getType() == Geometry::Type::COLLECTION);
	CHECK(reparsed->getRanges(true, error).empty());
}

void test_collection_writes_its_non_empty_members_only() {
	MultiPoint multipoint;
	multipoint.add(Point(at(19.702778, -101.192222)));
	Collection only_point;
	only_point.add_multipoint(multipoint);

	Collection with_empty;
	with_empty.add_multipoint(multipoint);
	with_empty.add_intersection(Intersection());
	CHECK(!with_empty.empty());
	CHECK_EQ(with_empty.toWKT(), only_point.toWKT());
	CHECK(with_empty.getRanges(true, error) == only_point.getRanges(true, error));
}

// --- F20: the lens of two wide circles -------------------------------------------------------------

// The lens holds the point between the two centers, and the convex covers it wherever it is, off the mesh edges and on
// them (HTM::getId places a point on an edge of a root trixel in a trixel that holds it: docs/design/geo-correctness.md).
void test_lens_of_wide_circles_covers_its_center() {
	for (const double lat : { 0.37, 20.3, -33.7, 60.1, -80.2 }) {
		for (const double lon : { 0.13, 33.3, 91.7, 135.2, 179.5, -77.7 }) {
			for (const double separation : { 20.0, 45.0 }) {
				Convex lens;
				lens.add(Circle(toward(lat, lon, 90.0, separation), 9000000));
				lens.add(Circle(toward(lat, lon, 270.0, separation), 9000000));
				for (const bool partials : { true, false }) {
					CHECK(covers(lens.getRanges(partials, error), at(lat, lon)));
				}
			}
		}
	}

	// Around a root vertex the lens covers the points of all four octants that meet there.
	Convex lens;
	lens.add(Circle(at(0, -45.0), 9000000));
	lens.add(Circle(at(0, 45.0), 9000000));
	const auto ranges = lens.getRanges(true, error);
	for (const double dlat : { -0.01, 0.01 }) {
		for (const double dlon : { -0.01, 0.01 }) {
			CHECK(covers(ranges, at(dlat, dlon)));
		}
	}
	CHECK_FALSE(covers(ranges, at(0.01, 180.0)));

	// F20: and the vertex itself, whatever the longitude it sits at (HTM::getId places points on a mesh edge).
	for (const double lon : { 0.0, 90.0, 180.0, -90.0 }) {
		Convex vertex_lens;
		vertex_lens.add(Circle(at(0, lon - 45.0 < -180.0 ? lon + 315.0 : lon - 45.0), 9000000));
		vertex_lens.add(Circle(at(0, lon + 45.0 > 180.0 ? lon - 315.0 : lon + 45.0), 9000000));
		for (const bool partials : { true, false }) {
			CHECK(covers(vertex_lens.getRanges(partials, error), at(0, lon)));
		}
	}
}

// --- F21: a circle past a hemisphere in a convex -----------------------------------------------------

void test_negative_circle_in_a_convex_covers_its_center_and_not_its_hole() {
	const auto center = at(19.702778, -101.192222);
	const auto antipode = at(-19.702778, 78.807778);

	// The complement of the 5,003 km cap around the antipode, alone and added through a convex.
	Convex alone;
	alone.add(Circle(center, 15011335.5));
	Convex through;
	through.add(alone);
	for (const bool partials : { true, false }) {
		for (const auto* convex : { &alone, &through }) {
			const auto ranges = convex->getRanges(partials, error);
			CHECK(covers(ranges, center));
			CHECK_FALSE(covers(ranges, antipode));
			auto trixels = convex->getTrixels(partials, error);
			HTM::simplifyTrixels(trixels);
			CHECK(HTM::getTrixels(ranges) == trixels);
		}
	}

	// The hole is the cap around the antipode: a hole inside one trixel's corners still leaves that trixel out.
	for (const double hole_km : { 200.0, 1000.0, 2000.0 }) {
		Convex holed;
		holed.add(Circle(center, half_circumference - hole_km * 1000.0));
		for (const bool partials : { true, false }) {
			const auto ranges = holed.getRanges(partials, error);
			CHECK(covers(ranges, center));
			if (hole_km >= 1000.0 || !partials) {
				CHECK_FALSE(covers(ranges, antipode));
			}
		}
	}
}

// --- F22: a ring whose circles are added largest first --------------------------------------------------

void test_ring_covers_its_band_whatever_the_order_of_its_circles() {
	const auto center = at(19.702778, -101.192222);
	const auto antipode = at(-19.702778, 78.807778);

	// 5,003 km to 6,000 km around the antipode: the complement of the inner cap, within the outer circle.
	Convex largest_first;
	largest_first.add(Circle(center, 15011335.5));
	largest_first.add(Circle(antipode, 6000000));
	Convex smallest_first;
	smallest_first.add(Circle(antipode, 6000000));
	smallest_first.add(Circle(center, 15011335.5));
	Convex merged;
	merged.add(largest_first);

	for (const auto* ring : { &largest_first, &smallest_first, &merged }) {
		for (const bool partials : { true, false }) {
			const auto ranges = ring->getRanges(partials, error);
			for (int bearing = 0; bearing < 360; bearing += 15) {
				// 5,500 km out: in both circles.
				CHECK(covers(ranges, toward(-19.702778, 78.807778, bearing, 5500.0 / 111.1949)));
				// 7,000 km out is beyond the outer circle.
				CHECK_FALSE(covers(ranges, toward(-19.702778, 78.807778, bearing, 7000.0 / 111.1949)));
			}
			CHECK_FALSE(covers(ranges, antipode));
			CHECK_FALSE(covers(ranges, center));
			auto trixels = ring->getTrixels(partials, error);
			HTM::simplifyTrixels(trixels);
			CHECK(HTM::getTrixels(ranges) == trixels);
		}
	}
	// The same circles give the same cover in either order.
	for (const bool partials : { true, false }) {
		CHECK(largest_first.getRanges(partials, error) == smallest_first.getRanges(partials, error));
		CHECK(merged.getRanges(partials, error) == smallest_first.getRanges(partials, error));
	}
}

// --- R2: a ring's cover never takes in the interior of its hole, at any error -----------------------------

void test_ring_cover_leaves_its_hole_out_at_every_error() {
	const auto center = at(19.702778, -101.192222);
	const auto antipode = at(-19.702778, 78.807778);

	// 5,003 km to 6,000 km around the antipode.
	Convex ring;
	ring.add(Circle(center, 15011335.5));
	ring.add(Circle(antipode, 6000000));
	for (const double e : { 0.1, 0.2, 0.3 }) {
		for (const bool partials : { true, false }) {
			const auto ranges = ring.getRanges(partials, e);
			for (int bearing = 0; bearing < 360; bearing += 5) {
				// Well inside the hole: 2,000 km or more from its edge.
				for (const double km : { 0.0, 500.0, 1500.0, 2500.0, 3000.0 }) {
					CHECK_FALSE(covers(ranges, toward(-19.702778, 78.807778, bearing, km / 111.1949)));
				}
				// In the ring.
				CHECK(covers(ranges, toward(-19.702778, 78.807778, bearing, 5500.0 / 111.1949)));
			}
		}
	}
}

// --- circles that never meet: the intersection is empty and covers nothing ---------------------------

void test_circles_that_never_meet_cover_nothing() {
	// Through the parser, which simplifies: an empty intersection used to cover the whole sphere.
	const auto geometry = EWKT("CONVEX((-101.19 19.70, 1000),(2.35 48.85, 1000))").getGeometry();
	for (const bool partials : { true, false }) {
		for (const double e : { error, 0.3 }) {
			const auto ranges = geometry->getRanges(partials, e);
			CHECK(ranges.empty());
			CHECK_FALSE(covers(ranges, at(19.702778, -101.192222)));
			CHECK_FALSE(covers(ranges, at(48.8566, 2.3522)));
			CHECK(geometry->getTrixels(partials, e).empty());
		}
	}

	Convex apart;
	apart.add(Circle(at(19.702778, -101.192222), 1000));
	apart.add(Circle(at(48.8566, 2.3522), 1000));
	apart.simplify();
	CHECK(apart.empty());
	CHECK(apart.getRanges(true, error).empty());

	Convex none;
	CHECK(none.getRanges(true, error).empty());
	CHECK(none.getTrixels(true, error).empty());
}


// --- D130: `_multiconvex` in the map form ----------------------------------------------------------

std::shared_ptr<Geometry> from_json(const std::string& json) {
	rapidjson::Document doc;
	json_load(doc, json);
	return GeoSpatial(MsgPack(doc)).getGeometry();
}

void test_multiconvex_serves_in_the_map_form() {
	const std::string text = "MULTICONVEX(((-101.19 19.70, 3000), (-101.20 19.71, 3000)), ((-101.0 19.0, 3000), (-101.01 19.01, 3000)))";
	const std::string one = "{\"_latitude\":[19.70,19.71],\"_longitude\":[-101.19,-101.20],\"_radius\":[3000,3000]}";
	const std::string two = "{\"_latitude\":[19.0,19.01],\"_longitude\":[-101.0,-101.01],\"_radius\":[3000,3000]}";
	const std::string nested = "{\"_latitude\":[[19.70,19.71],[19.0,19.01]],\"_longitude\":[[-101.19,-101.20],[-101.0,-101.01]],"
		"\"_radius\":[[3000,3000],[3000,3000]]}";

	// One list of lists, one convex per inner list: the same geometry as the EWKT form.
	const auto geometry = from_json("{\"_multiconvex\":" + nested + "}");
	CHECK(geometry->getType() == Geometry::Type::MULTICONVEX);
	CHECK_EQ(geometry->toEWKT(), EWKT(text).getGeometry()->toEWKT());
	CHECK(geometry->getRanges(true, error) == ewkt_ranges(text));
	CHECK(covers(geometry->getRanges(true, error), at(19.705, -101.195)));
	CHECK(covers(geometry->getRanges(true, error), at(19.005, -101.005)));
	CHECK_FALSE(covers(geometry->getRanges(true, error), at(48.85, 2.35)));

	// A flat list is one convex.
	const auto single = from_json("{\"_multiconvex\":" + one + "}");
	CHECK(single->getType() == Geometry::Type::MULTICONVEX);
	CHECK(single->getRanges(true, error) == from_json("{\"_convex\":" + one + "}")->getRanges(true, error));

	// Inside a collection and an intersection, beside the other members.
	const auto collection = from_json("{\"_geometrycollection\":{\"_multiconvex\":" + nested + "}}");
	CHECK(collection->getType() == Geometry::Type::COLLECTION);
	CHECK(collection->getRanges(true, error) == ewkt_ranges("GEOMETRYCOLLECTION(" + text + ")"));
	const auto intersection = from_json("{\"_geometryintersection\":{\"_multiconvex\":" + nested + ",\"_circle\":{\"_latitude\":19.705,\"_longitude\":-101.195,\"_radius\":1000}}}");
	CHECK(intersection->getType() == Geometry::Type::INTERSECTION);
	CHECK(covers(intersection->getRanges(true, error), at(19.705, -101.195)));
	CHECK_FALSE(covers(intersection->getRanges(true, error), at(19.005, -101.005)));

	// Malformed input is a GeoSpatialError, a 400.
	for (const char* bad : {
		"{\"_multiconvex\":1}",
		"{\"_multiconvex\":{\"_latitude\":[[1,2]],\"_longitude\":[[1,2]]}}",
		"{\"_multiconvex\":{\"_latitude\":[[1,2],[3,4]],\"_longitude\":[[1,2]],\"_radius\":[[5,5]]}}",
		"{\"_multiconvex\":{\"_latitude\":[[1,2]],\"_longitude\":[[1,2]],\"_radius\":[[5]]}}",
	}) {
		CHECK_THROW(from_json(bad), GeoSpatialError);
	}
}

}  // namespace

// Each test runs guarded: an exception that escapes one is a failed check naming it, not an abort of the whole run.
int main() {
	const struct { const char* name; void (*run)(); } tests[] = {
		{ "test_multipoint_without_parentheses_parses", test_multipoint_without_parentheses_parses },
		{ "test_multipoint_errors_are_ewkt_errors", test_multipoint_errors_are_ewkt_errors },
		{ "test_collection_of_empty_intersection_is_empty", test_collection_of_empty_intersection_is_empty },
		{ "test_collection_writes_its_non_empty_members_only", test_collection_writes_its_non_empty_members_only },
		{ "test_lens_of_wide_circles_covers_its_center", test_lens_of_wide_circles_covers_its_center },
		{ "test_negative_circle_in_a_convex_covers_its_center_and_not_its_hole", test_negative_circle_in_a_convex_covers_its_center_and_not_its_hole },
		{ "test_ring_covers_its_band_whatever_the_order_of_its_circles", test_ring_covers_its_band_whatever_the_order_of_its_circles },
		{ "test_ring_cover_leaves_its_hole_out_at_every_error", test_ring_cover_leaves_its_hole_out_at_every_error },
		{ "test_circles_that_never_meet_cover_nothing", test_circles_that_never_meet_cover_nothing },
		{ "test_multiconvex_serves_in_the_map_form", test_multiconvex_serves_in_the_map_form },
	};
	for (const auto& t : tests) {
		try {
			t.run();
		} catch (const std::exception& e) {
			std::fprintf(stderr, "  FAIL: %s threw %s: %s\n", t.name, typeid(e).name(), e.what());
			++testh::failures;
		}
	}
	if (testh::failures != 0) {
		std::fprintf(stderr, "%d of %d checks FAILED\n", testh::failures, testh::checks);
		return 1;
	}
	std::printf("OK: %d checks passed\n", testh::checks);
	return 0;
}
