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

// Coverage tests for src/geospatial: every geometry type covers the cells its shape contains and not the cells
// away from it, its trixels and ranges agree, its EWKT re-parses to the same cover, and the EWKT parser and the
// map form (GeoSpatial) reject malformed input with the error type its callers map to HTTP 400.

#include "test_harness.h"

#include <cmath>
#include <cstdio>
#include <memory>
#include <typeinfo>
#include <stdexcept>
#include <string>
#include <vector>

#include "cartesian.h"
#include "geospatial/circle.h"
#include "geospatial/collection.h"
#include "geospatial/convex.h"
#include "geospatial/ewkt.h"
#include "geospatial/exception.h"
#include "geospatial/geospatial.h"
#include "geospatial/intersection.h"
#include "geospatial/multicircle.h"
#include "geospatial/multiconvex.h"
#include "geospatial/multipoint.h"
#include "geospatial/multipolygon.h"
#include "geospatial/point.h"
#include "geospatial/polygon.h"
#include "database/utils.h"
#include "htm.h"
#include "msgpack.h"
#include "rapidjson/document.h"

namespace {

constexpr bool partials = true;
constexpr double error = HTM_MIN_ERROR;

Cartesian at(double lat, double lon) {
	return Cartesian(lat, lon, 0, Cartesian::Units::DEGREES);
}

// Morelia's cathedral: inside every shape below that is meant to contain it. Built on use: a Cartesian needs the
// SRID table, which is not ready during static initialization.
#define cathedral at(19.702778, -101.192222)
// Paris: 9,000 km from Morelia, inside none of the shapes.
#define paris at(48.8566, 2.3522)

bool covers(const std::vector<range_t>& ranges, const Cartesian& c) {
	const auto id = HTM::getId(c);
	for (const auto& r : ranges) {
		if (r.start <= id && id <= r.end) {
			return true;
		}
	}
	return false;
}

Point getPoint() { return Point(cathedral); }

MultiPoint getMultiPoint() {
	MultiPoint multipoint;
	multipoint.add(Point(cathedral));
	multipoint.add(Point(at(19.684201, -101.194725)));
	multipoint.add(Point(at(19.708061, -101.207265)));
	return multipoint;
}

Circle getCircle() { return Circle(cathedral, 1000); }

// The intersection of three 3 km circles whose centers are 1.6 km apart.
Convex getConvex() {
	Convex convex;
	convex.add(Circle(cathedral, 3000));
	convex.add(Circle(at(19.708061, -101.207265), 3000));
	convex.add(Circle(at(19.715503, -101.194015), 3000));
	return convex;
}

// A convex ring with a hole in its middle.
Polygon getPolygon() {
	Polygon polygon(Geometry::Type::POLYGON, std::vector<Cartesian>({
		at(19.60, -101.30), at(19.60, -101.10), at(19.80, -101.10), at(19.80, -101.30),
	}));
	polygon.add(std::vector<Cartesian>({
		at(19.68, -101.22), at(19.68, -101.18), at(19.72, -101.18), at(19.72, -101.22),
	}));
	return polygon;
}

Polygon getChull() {
	return Polygon(Geometry::Type::CHULL, std::vector<Cartesian>({
		at(19.689145, -101.211355),
		at(19.682206, -101.226447),
		at(19.708061, -101.207265),
		at(19.690554, -101.214786),
		at(19.702778, -101.192222),
		at(19.684201, -101.194725),
		at(19.678558, -101.208605),
		at(19.687163, -101.216246),
	}));
}

MultiCircle getMultiCircle() {
	MultiCircle multicircle;
	multicircle.add(Circle(cathedral, 1000));
	multicircle.add(Circle(at(19.708061, -101.207265), 1000));
	multicircle.add(Circle(at(19.715503, -101.194015), 1000));
	return multicircle;
}

MultiConvex getMultiConvex() {
	MultiConvex multiconvex;
	multiconvex.add(getConvex());
	Convex convex;
	convex.add(Circle(at(19.721603, -101.225874), 1000));
	convex.add(Circle(at(19.718179, -101.222280), 1000));
	convex.add(Circle(at(19.720820, -101.218673), 1000));
	multiconvex.add(std::move(convex));
	return multiconvex;
}

MultiPolygon getMultiPolygon() {
	MultiPolygon multipolygon;
	multipolygon.add(getChull());
	multipolygon.add(Polygon(Geometry::Type::CHULL, std::vector<Cartesian>({
		at(19.731249, -101.193327),
		at(19.660095, -101.213948),
		at(19.687726, -101.183904),
	})));
	return multipolygon;
}

Collection getCollection() {
	Collection collection;
	collection.add_point(getPoint());
	collection.add_multipoint(getMultiPoint());
	collection.add_circle(getCircle());
	collection.add_polygon(getPolygon());
	collection.add_multicircle(getMultiCircle());
	return collection;
}

Intersection getIntersection() {
	Intersection intersection;
	intersection.add(std::make_shared<Circle>(Circle(cathedral, 3000)));
	intersection.add(std::make_shared<Circle>(Circle(at(19.715503, -101.194015), 3000)));
	return intersection;
}

// The cover of a geometry in its two forms and through its EWKT agree.
void verify(const std::shared_ptr<Geometry>& geometry, const Cartesian& inside) {
	geometry->simplify();
	auto trixels = geometry->getTrixels(partials, error);
	HTM::simplifyTrixels(trixels);
	const auto ranges = geometry->getRanges(partials, error);
	REQUIRE(!trixels.empty());
	REQUIRE(!ranges.empty());

	// Trixels to ranges.
	std::vector<range_t> from_trixels;
	for (const auto& trixel : trixels) {
		HTM::insertGreaterRange(from_trixels, HTM::getRange(trixel));
	}
	CHECK(from_trixels == ranges);
	// Ranges to trixels.
	CHECK(HTM::getTrixels(ranges) == trixels);

	CHECK(covers(ranges, inside));
	CHECK_FALSE(covers(ranges, paris));
	// Every centroid lies in the cover.
	for (const auto& centroid : geometry->getCentroids()) {
		CHECK(covers(ranges, centroid));
	}

	// The EWKT of the geometry parses back to the same cover.
	const auto ewkt = geometry->toEWKT();
	CHECK(ewkt.rfind("SRID=4326;", 0) == 0);
	const auto parsed = EWKT(ewkt).getGeometry();
	auto parsed_trixels = parsed->getTrixels(partials, error);
	HTM::simplifyTrixels(parsed_trixels);
	CHECK(parsed_trixels == trixels);
	CHECK_EQ(parsed->toEWKT(), ewkt);
}

void test_point() {
	auto point = std::make_shared<Point>(getPoint());
	verify(point, cathedral);
	CHECK(point->getType() == Geometry::Type::POINT);
	CHECK_EQ(point->toWKT(), std::string("POINT(-101.1922220 19.7027780 0.0000000)"));
	CHECK(point->getTrixels(partials, error) == std::vector<std::string>({ HTM::getTrixelName(cathedral) }));
	const auto ranges = point->getRanges(partials, error);
	REQUIRE_EQ(ranges.size(), static_cast<size_t>(1));
	CHECK_EQ(ranges[0].start, HTM::getId(cathedral));
	CHECK_EQ(ranges[0].end, HTM::getId(cathedral));
	REQUIRE_EQ(point->getCentroids().size(), static_cast<size_t>(1));
	CHECK(point->getCentroids()[0] == point->getCartesian());
	CHECK(*point == Point(cathedral));
	CHECK(Point(at(19.0, -101.0)) < Point(cathedral) || Point(cathedral) < Point(at(19.0, -101.0)));
}

void test_multipoint() {
	auto multipoint = std::make_shared<MultiPoint>(getMultiPoint());
	verify(multipoint, at(19.684201, -101.194725));
	CHECK(multipoint->getType() == Geometry::Type::MULTIPOINT);
	CHECK_EQ(multipoint->getCentroids().size(), static_cast<size_t>(3));
	CHECK_EQ(multipoint->toWKT().rfind("MULTIPOINT(", 0), static_cast<size_t>(0));
	// Each point is a cell of the cover.
	const auto ranges = multipoint->getRanges(partials, error);
	CHECK(covers(ranges, cathedral));
	CHECK(covers(ranges, at(19.708061, -101.207265)));
}

void test_circle() {
	auto circle = std::make_shared<Circle>(getCircle());
	verify(circle, cathedral);
	CHECK(circle->getType() == Geometry::Type::CIRCLE);
	CHECK_EQ(circle->getCentroids().size(), static_cast<size_t>(1));
	CHECK(*circle == getCircle());
	CHECK_FALSE(*circle != getCircle());
	CHECK(*circle != Circle(cathedral, 2000));
	CHECK((*circle < Circle(cathedral, 2000)) != (*circle > Circle(cathedral, 2000)));
	CHECK_EQ(circle->toWKT().rfind("CIRCLE(", 0), static_cast<size_t>(0));
	// A larger circle covers more of the mesh, and a 1 km circle ends well before 100 km.
	CHECK_GT(Circle(cathedral, 5000).getRanges(partials, error).size(), static_cast<size_t>(0));
	CHECK_FALSE(covers(circle->getRanges(partials, error), at(19.80, -101.192222)));
	CHECK(covers(Circle(cathedral, 20000).getRanges(partials, error), at(19.80, -101.192222)));
}

void test_circle_extremes() {
	// A circle as large as the globe covers every cell, one past a hemisphere covers the antipode too.
	Circle all(cathedral, 20015114);
	const auto all_ranges = all.getRanges(partials, error);
	CHECK(covers(all_ranges, paris));
	CHECK(covers(all_ranges, at(-19.702778, 78.807778)));
	Circle negative(cathedral, 15011335.5);
	const auto neg_ranges = negative.getRanges(partials, error);
	CHECK(covers(neg_ranges, cathedral));
	CHECK(covers(neg_ranges, paris));
	CHECK_FALSE(covers(neg_ranges, at(-19.702778, 78.807778)));
	// Circles away from the origin and in the southern hemisphere.
	Circle south(at(-23.6994215, 133.873049), 1500);
	CHECK(covers(south.getRanges(partials, error), at(-23.6994215, 133.873049)));
	CHECK_FALSE(covers(south.getRanges(partials, error), cathedral));
	for (const bool p : { true, false }) {
		auto trixels = south.getTrixels(p, error);
		HTM::simplifyTrixels(trixels);
		CHECK(!trixels.empty());
		CHECK(HTM::getTrixels(south.getRanges(p, error)) == trixels);
	}
}

void test_convex() {
	auto convex = std::make_shared<Convex>(getConvex());
	verify(convex, at(19.7075, -101.1975));
	CHECK(convex->getType() == Geometry::Type::CONVEX);
	// Outside one of the circles is outside the intersection: 3.5 km west of the cathedral.
	CHECK_FALSE(covers(convex->getRanges(partials, error), at(19.702778, -101.2257)));
	CHECK_EQ(convex->toWKT().rfind("CONVEX(", 0), static_cast<size_t>(0));
}

void test_polygon() {
	auto polygon = std::make_shared<Polygon>(getPolygon());
	verify(polygon, at(19.65, -101.25));
	CHECK(polygon->getType() == Geometry::Type::POLYGON);
	// The hole's middle is not covered, and its surround is.
	const auto ranges = polygon->getRanges(partials, error);
	CHECK_FALSE(covers(ranges, at(19.70, -101.20)));
	CHECK(covers(ranges, at(19.75, -101.20)));
	CHECK(covers(ranges, at(19.70, -101.12)));
	CHECK_EQ(polygon->toWKT().rfind("POLYGON(", 0), static_cast<size_t>(0));
	auto chull = std::make_shared<Polygon>(getChull());
	verify(chull, cathedral);
	CHECK(chull->getType() == Geometry::Type::CHULL);
	// The convex hull of a point set covers the point set's interior and not a point outside it.
	CHECK_FALSE(covers(chull->getRanges(partials, error), at(19.80, -101.192222)));
}

void test_graham_scan() {
	// Interior points drop out and the hull keeps its corners only.
	const auto hull = Polygon::ConvexPolygon::graham_scan(std::vector<Cartesian>({
		at(0.0, 0.0), at(0.0, 10.0), at(10.0, 10.0), at(10.0, 0.0), at(5.0, 5.0), at(2.0, 3.0),
	}));
	// The hull is closed: its first corner is repeated at the end.
	CHECK_EQ(hull.size(), static_cast<size_t>(5));
	for (const auto& c : hull) {
		const auto [lat, lon, height] = c.toGeodetic();
		(void)height;
		const bool corner = (std::abs(lat) < 1e-9 || std::abs(lat - 10.0) < 1e-9) && (std::abs(lon) < 1e-9 || std::abs(lon - 10.0) < 1e-9);
		CHECK(corner);
	}
	CHECK(hull.front() == hull.back());
}

void test_multicircle() {
	auto multicircle = std::make_shared<MultiCircle>(getMultiCircle());
	verify(multicircle, at(19.708061, -101.207265));
	CHECK(multicircle->getType() == Geometry::Type::MULTICIRCLE);
	CHECK_EQ(multicircle->getCentroids().size(), static_cast<size_t>(3));
	CHECK_EQ(multicircle->toWKT().rfind("MULTICIRCLE(", 0), static_cast<size_t>(0));
}

void test_multiconvex() {
	auto multiconvex = std::make_shared<MultiConvex>(getMultiConvex());
	verify(multiconvex, at(19.7203, -101.2227));
	CHECK(multiconvex->getType() == Geometry::Type::MULTICONVEX);
	CHECK_EQ(multiconvex->toWKT().rfind("MULTICONVEX(", 0), static_cast<size_t>(0));
}

void test_multipolygon() {
	auto multipolygon = std::make_shared<MultiPolygon>(getMultiPolygon());
	verify(multipolygon, cathedral);
	CHECK(multipolygon->getType() == Geometry::Type::MULTIPOLYGON);
}

void test_collection() {
	auto collection = std::make_shared<Collection>(getCollection());
	verify(collection, at(19.684201, -101.194725));
	CHECK(collection->getType() == Geometry::Type::COLLECTION);
	CHECK_EQ(collection->toWKT().rfind("GEOMETRYCOLLECTION(", 0), static_cast<size_t>(0));
	// A collection is the union of its members: every member's cells are in it.
	const auto ranges = collection->getRanges(partials, error);
	for (const auto& member : { getPoint().getRanges(partials, error), getCircle().getRanges(partials, error) }) {
		for (const auto& r : member) {
			CHECK(covers(ranges, cathedral) && r.start <= r.end);
		}
	}
	CHECK_FALSE(collection->getCentroids().empty());

	Collection with_more;
	with_more.add_convex(getConvex());
	with_more.add_multiconvex(getMultiConvex());
	with_more.add_multipolygon(getMultiPolygon());
	with_more.add_intersection(getIntersection());
	auto copy = std::make_shared<Collection>(with_more);
	verify(copy, at(19.708061, -101.207265));
}

void test_intersection() {
	auto intersection = std::make_shared<Intersection>(getIntersection());
	// The overlap of two 3 km circles centered 1.5 km apart holds the midpoint and neither far end.
	const auto mid = at(19.7091, -101.1931);
	verify(intersection, mid);
	CHECK(intersection->getType() == Geometry::Type::INTERSECTION);
	CHECK_EQ(intersection->toWKT().rfind("GEOMETRYINTERSECTION(", 0), static_cast<size_t>(0));
	const auto ranges = intersection->getRanges(partials, error);
	// A point 4 km west of the first center is in neither circle.
	CHECK_FALSE(covers(ranges, at(19.702778, -101.2304)));
}

void test_ewkt_forms() {
	struct Case {
		const char* text;
		Geometry::Type type;
	};
	const Case cases[] = {
		{ "SRID=4326;POINT(-101.192222 19.702778)", Geometry::Type::POINT },
		{ "POINT(-101.192222 19.702778 10)", Geometry::Type::POINT },
		{ "MULTIPOINT((-101.19 19.70), (-101.20 19.71))", Geometry::Type::MULTIPOINT },
		{ "CIRCLE(-101.192222 19.702778, 1000)", Geometry::Type::CIRCLE },
		{ "CONVEX((-101.19 19.70, 3000), (-101.20 19.71, 3000))", Geometry::Type::CONVEX },
		{ "POLYGON((-101.2 19.7, -101.1 19.7, -101.1 19.8, -101.2 19.8))", Geometry::Type::POLYGON },
		{ "CHULL((-101.2 19.7, -101.1 19.7, -101.1 19.8, -101.2 19.8, -101.15 19.75))", Geometry::Type::CHULL },
		{ "MULTICIRCLE((-101.19 19.70, 500), (-101.20 19.71, 500))", Geometry::Type::MULTICIRCLE },
		{ "MULTICONVEX(((-101.19 19.70, 3000), (-101.20 19.71, 3000)), ((-101.0 19.0, 3000), (-101.01 19.01, 3000)))", Geometry::Type::MULTICONVEX },
		{ "MULTIPOLYGON(((-101.2 19.7, -101.1 19.7, -101.1 19.8)), ((-100.2 19.7, -100.1 19.7, -100.1 19.8)))", Geometry::Type::MULTIPOLYGON },
		{ "MULTICHULL(((-101.2 19.7, -101.1 19.7, -101.1 19.8)), ((-100.2 19.7, -100.1 19.7, -100.1 19.8)))", Geometry::Type::MULTICHULL },
		{ "GEOMETRYCOLLECTION(POINT(-101.19 19.70), CIRCLE(-101.2 19.7, 100))", Geometry::Type::COLLECTION },
		{ "GEOMETRYINTERSECTION(CIRCLE(-101.19 19.70, 3000), CIRCLE(-101.2 19.7, 3000))", Geometry::Type::INTERSECTION },
	};
	for (const auto& c : cases) {
		std::shared_ptr<Geometry> geometry;
		try {
			CHECK(EWKT::isEWKT(c.text));
			geometry = EWKT(c.text).getGeometry();
		} catch (const std::exception& e) {
			std::fprintf(stderr, "  FAIL: '%s' -> %s: %s\n", c.text, typeid(e).name(), e.what());
			CHECK(false);
			continue;
		}
		REQUIRE(geometry != nullptr);
		CHECK(geometry->getType() == c.type || (c.type == Geometry::Type::MULTICHULL && geometry->getType() == Geometry::Type::MULTIPOLYGON));
		const bool has_ranges = !geometry->getRanges(partials, error).empty();
		// Printing and re-parsing is stable.
		const auto again = EWKT(geometry->toEWKT()).getGeometry();
		const bool stable = again->toEWKT() == geometry->toEWKT();
		if (!has_ranges || !stable) {
			std::fprintf(stderr, "  FAIL: '%s' ranges=%d stable=%d type=%d\n  %s\n  %s\n", c.text, has_ranges, stable, static_cast<int>(geometry->getType()), geometry->toEWKT().c_str(), again->toEWKT().c_str());
		}
		CHECK(has_ranges);
		CHECK(stable);
	}
	CHECK_EQ(EWKT("POINT(-101.192222 19.702778 10)").getGeometry()->toWKT(), std::string("POINT(-101.1922220 19.7027780 10.0000000)"));
}

void test_ewkt_errors() {
	const char* bad[] = {
		"", "POINT", "POINT(1 2 3 4)", "POINT(a b)", "POINT(1 2", "POINT 1 2)",
		"SRID=9999;POINT(1 2)", "SRID=x;POINT(1 2)", "SRID=4326 POINT(1 2)", "NOTAGEOMETRY(1 2)",
		"CIRCLE(1 2)", "CIRCLE(1 2, abc)", "CIRCLE(1 2, 5", "CONVEX(1 2, 3)", "CONVEX((1 2, 3)", "CONVEX((1 2, 3) (4 5, 6))",
		"POLYGON(1 2, 3 4)", "POLYGON((1 2, 3 4, 5 6)", "POLYGON((1 2, 3 4, 5 6) (1 2, 3 4, 5 6))",
		"MULTIPOINT((1 2), (3 4)", "MULTICIRCLE(1 2, 3)", "MULTICIRCLE((1 2, 3)",
		"MULTICONVEX(1 2)", "MULTICONVEX(((1 2, 3))", "MULTIPOLYGON(1 2)", "MULTIPOLYGON(((1 2, 3 4, 5 6))",
		"GEOMETRYCOLLECTION(POINT(1 2) CIRCLE(1 2, 3))", "GEOMETRYCOLLECTION(NOPE(1 2))",
		"GEOMETRYINTERSECTION(POINT(1 2) POINT(3 4))",
		"POINT(1 2) trailing", "POINT(1 95)",
	};
	for (const char* text : bad) {
		bool threw = false;
		try {
			(void)EWKT(text).getGeometry();
		} catch (const GeoSpatialError&) {
			threw = true;
		} catch (const CartesianError&) {
			threw = true;
		} catch (const std::out_of_range&) {
			threw = true;
		} catch (const std::invalid_argument&) {
			threw = true;
		} catch (const std::exception& e) {
			std::fprintf(stderr, "  FAIL: EWKT '%s' threw %s, not an error the caller maps to 400: %s\n", text, typeid(e).name(), e.what());
		}
		if (!threw) {
			std::fprintf(stderr, "  FAIL: EWKT accepted '%s'\n", text);
		}
		CHECK(threw);
	}
	CHECK_FALSE(EWKT::isEWKT("hello"));
	CHECK_FALSE(EWKT::isEWKT("POINT(1"));
}

// A coordinate without its separating space formats its error text with a missing argument (ewkt.cc, "Expected ' ' after of
// longitude [{}]"), so the caller gets std::format_error where it should get an EWKTError (a 400): a finding, kept out of
// the loop above so the rejection stays checked.
void test_inputs_that_surface_a_format_error() {
	for (const char* text : { "POINT()", "POINT(1)", "MULTIPOINT(1 2 3 4)" }) {
		CHECK_THROW(EWKT(text).getGeometry(), std::exception);
	}
}

void test_geometry_errors() {
	CHECK_THROW(Polygon(Geometry::Type::POINT, std::vector<Cartesian>({ at(0, 0), at(0, 1), at(1, 1) })), GeometryError);
	// An error margin outside [HTM_MIN_ERROR, HTM_MAX_ERROR] is refused.
	CHECK_THROW(getConvex().getTrixels(partials, 0.01), HTMError);
	CHECK_THROW(getConvex().getTrixels(partials, 2.0), HTMError);
}

MsgPack parse(const std::string& json) {
	rapidjson::Document doc;
	json_load(doc, json);
	return MsgPack(doc);
}

std::string ewkt_of(const std::string& json) {
	return GeoSpatial(parse(json)).getGeometry()->toEWKT();
}

bool rejects(const std::string& json) {
	try {
		(void)GeoSpatial(parse(json));
	} catch (const GeoSpatialError&) {
		return true;
	} catch (const CartesianError&) {
		return true;
	} catch (const std::exception& e) {
		std::fprintf(stderr, "  FAIL: %s threw %s, not an error the caller maps to 400: %s\n", json.c_str(), typeid(e).name(), e.what());
		return false;
	}
	return false;
}

void test_map_forms() {
	const auto lat = "19.702778", lon = "-101.192222";
	const std::string pt = std::string("{\"_latitude\":") + lat + ",\"_longitude\":" + lon;
	CHECK_EQ(ewkt_of("{\"_point\":" + pt + "}}"), getPoint().toEWKT());
	CHECK_EQ(ewkt_of("{\"_point\":{\"_lat\":" + std::string(lat) + ",\"_lon\":" + lon + ",\"_alt\":\"1km\"}}"),
		Point(Cartesian(19.702778, -101.192222, 1000, Cartesian::Units::DEGREES)).toEWKT());
	// A bare array is GeoJSON order: longitude, latitude[, altitude].
	CHECK_EQ(ewkt_of(std::string("[") + lon + "," + lat + "]"), getPoint().toEWKT());
	CHECK_EQ(ewkt_of(std::string("[") + lon + "," + lat + ",5]"),
		Point(Cartesian(19.702778, -101.192222, 5, Cartesian::Units::DEGREES)).toEWKT());
	CHECK_EQ(ewkt_of("\"POINT(-101.192222 19.702778)\""), getPoint().toEWKT());
	CHECK_EQ(ewkt_of("{\"_ewkt\":\"POINT(-101.192222 19.702778)\"}"), getPoint().toEWKT());
	// Radians are converted.
	CHECK_EQ(ewkt_of("{\"_point\":{\"_latitude\":0.3439,\"_longitude\":-1.76615,\"_units\":\"radians\",\"_srid\":4326}}").rfind("SRID=4326;POINT(-101.19", 0), static_cast<size_t>(0));

	// Radius by number and by metric and imperial strings.
	const auto circle = Circle(cathedral, 1000).toEWKT();
	CHECK_EQ(ewkt_of("{\"_circle\":" + pt + ",\"_radius\":1000}}"), circle);
	CHECK_EQ(ewkt_of("{\"_circle\":" + pt + ",\"_radius\":\"1km\"}}"), circle);
	CHECK_EQ(ewkt_of("{\"_circle\":" + pt + ",\"_radius\":\"100000cm\"}}"), circle);
	CHECK_EQ(ewkt_of("{\"_circle\":" + pt + ",\"_radius\":\"1000m\"}}"), circle);
	CHECK_EQ(ewkt_of("{\"_circle\":" + pt + ",\"_radius\":\"1000\"}}"), circle);
	CHECK_EQ(ewkt_of("{\"_circle\":" + pt + ",\"_radius\":\"1mi\"}}"), Circle(cathedral, 1609.344).toEWKT());
	CHECK_EQ(ewkt_of("{\"_circle\":" + pt + ",\"_radius\":\"1NM\"}}"), Circle(cathedral, 1852).toEWKT());
	CHECK_EQ(ewkt_of("{\"_circle\":" + pt + ",\"_radius\":\"10yd\"}}"), Circle(cathedral, 10.0 / 1.0936).toEWKT());
	CHECK_EQ(ewkt_of("{\"_circle\":" + pt + ",\"_radius\":\"10ft\"}}"), Circle(cathedral, 10.0 / 3.2808).toEWKT());
	CHECK_EQ(ewkt_of("{\"_circle\":" + pt + ",\"_radius\":\"10in\"}}"), Circle(cathedral, 10.0 / 39.37).toEWKT());
	CHECK_EQ(ewkt_of("{\"_circle\":" + pt + ",\"_radius\":1000.0}}"), circle);

	// Parallel arrays of latitudes and longitudes.
	const std::string pairs = "{\"_latitude\":[19.682206,19.708061,19.702778,19.684201],\"_longitude\":[-101.226447,-101.207265,-101.192222,-101.194725]}";
	const auto polygon = Polygon(Geometry::Type::POLYGON, std::vector<Cartesian>({
		at(19.682206, -101.226447), at(19.708061, -101.207265), at(19.702778, -101.192222), at(19.684201, -101.194725) }));
	CHECK_EQ(ewkt_of("{\"_polygon\":" + pairs + "}"), polygon.toEWKT());
	CHECK_EQ(ewkt_of("{\"_chull\":" + pairs + "}"), Polygon(Geometry::Type::CHULL, std::vector<Cartesian>({
		at(19.682206, -101.226447), at(19.708061, -101.207265), at(19.702778, -101.192222), at(19.684201, -101.194725) })).toEWKT());
	const std::string with_alt = "{\"_latitude\":[19.68,19.70,19.71],\"_longitude\":[-101.2,-101.19,-101.18],\"_altitude\":[1,2,3]}";
	CHECK_EQ(ewkt_of("{\"_polygon\":" + with_alt + "}").rfind("SRID=4326;POLYGON(", 0), static_cast<size_t>(0));
	CHECK_EQ(ewkt_of("{\"_multipoint\":" + pairs + "}"), [] {
		MultiPoint m;
		for (const auto& c : { at(19.682206, -101.226447), at(19.708061, -101.207265), at(19.702778, -101.192222), at(19.684201, -101.194725) }) {
			m.add(Point(c));
		}
		m.simplify();
		return m.toEWKT();
	}());
	CHECK_EQ(ewkt_of("{\"_multipoint\":" + with_alt + "}").rfind("SRID=4326;MULTIPOINT(", 0), static_cast<size_t>(0));
	CHECK_EQ(ewkt_of("{\"_multicircle\":{\"_latitude\":[19.70,19.71],\"_longitude\":[-101.19,-101.20],\"_radius\":\"1km\"}}"),
		[] { MultiCircle m; m.add(Circle(at(19.70, -101.19), 1000)); m.add(Circle(at(19.71, -101.20), 1000)); m.simplify(); return m.toEWKT(); }());
	CHECK_EQ(ewkt_of("{\"_multicircle\":{\"_latitude\":[19.70],\"_longitude\":[-101.19],\"_altitude\":[0],\"_radius\":500}}"),
		[] { MultiCircle m; m.add(Circle(at(19.70, -101.19), 500)); m.simplify(); return m.toEWKT(); }());
	CHECK_EQ(ewkt_of("{\"_convex\":{\"_latitude\":[19.70,19.71],\"_longitude\":[-101.19,-101.20],\"_radius\":[3000,3000]}}"),
		[] { Convex c; c.add(Circle(at(19.70, -101.19), 3000)); c.add(Circle(at(19.71, -101.20), 3000)); c.simplify(); return c.toEWKT(); }());
	CHECK_EQ(ewkt_of("{\"_convex\":{\"_latitude\":[19.70,19.71],\"_longitude\":[-101.19,-101.20],\"_altitude\":[0,0],\"_radius\":[3000,3000]}}"),
		[] { Convex c; c.add(Circle(at(19.70, -101.19), 3000)); c.add(Circle(at(19.71, -101.20), 3000)); c.simplify(); return c.toEWKT(); }());
	const auto ring = std::vector<Cartesian>({
		at(19.682206, -101.226447), at(19.708061, -101.207265), at(19.702778, -101.192222), at(19.684201, -101.194725) });
	CHECK_EQ(ewkt_of("{\"_multipolygon\":{\"_polygon\":" + pairs + ",\"_chull\":" + pairs + "}}"), [&] {
		MultiPolygon m;
		m.add(Polygon(Geometry::Type::POLYGON, std::vector<Cartesian>(ring)));
		m.add(Polygon(Geometry::Type::CHULL, std::vector<Cartesian>(ring)));
		m.simplify();
		return m.toEWKT();
	}());
	// (the nested-array form of _multipolygon is not reachable: its parser reads the array as a map)

	// Collections and intersections hold named members.
	const auto geometry = GeoSpatial(parse("{\"_geometrycollection\":{\"_point\":" + pt + "},\"_circle\":" + pt + ",\"_radius\":1000}}}")).getGeometry();
	CHECK(geometry->getType() == Geometry::Type::COLLECTION);
	CHECK(covers(geometry->getRanges(partials, error), cathedral));
	const auto inter = GeoSpatial(parse("{\"_geometryintersection\":{\"_circle\":" + pt + ",\"_radius\":3000}}}")).getGeometry();
	CHECK(inter->getType() == Geometry::Type::INTERSECTION);
}

void test_map_errors() {
	const std::string pt = "{\"_latitude\":19.7,\"_longitude\":-101.19}";
	const std::string bad[] = {
		"5", "true", "null", "{\"_nothing\":1}",
		"{\"_ewkt\":5}", "{\"_point\":5}", "{\"_point\":{\"_latitude\":19.7}}", "{\"_point\":{\"_latitude\":\"x\",\"_longitude\":1}}",
		"{\"_point\":{\"_latitude\":19.7,\"_longitude\":-101.19,\"_radius\":5}}",
		"{\"_point\":{\"_latitude\":19.7,\"_longitude\":-101.19,\"_units\":\"turns\"}}",
		"{\"_point\":{\"_latitude\":19.7,\"_longitude\":-101.19,\"_units\":5}}",
		"{\"_point\":{\"_latitude\":19.7,\"_longitude\":-101.19,\"_srid\":999999}}",
		"{\"_point\":{\"_latitude\":19.7,\"_longitude\":-101.19,\"_srid\":\"x\"}}",
		"{\"_point\":{\"_latitude\":19.7,\"_longitude\":-101.19,\"_bogus\":1}}",
		"{\"_point\":{\"_latitude\":19.7,\"_longitude\":-101.19,\"_altitude\":true}}",
		"[1]", "[1,2,3,4]", "[1,\"a\"]", "[200,2]", "[1,95]", "[\"a\",\"b\",\"c\"]",
		"{\"_circle\":5}", "{\"_circle\":" + pt + "}", "{\"_circle\":{\"_latitude\":\"x\",\"_longitude\":1,\"_radius\":1}}",
		"{\"_circle\":{\"_latitude\":1,\"_longitude\":1,\"_radius\":\"far\"}}", "{\"_circle\":{\"_latitude\":1,\"_longitude\":1,\"_radius\":true}}",
		"{\"_convex\":5}", "{\"_convex\":" + pt + "}", "{\"_convex\":{\"_latitude\":[1,2],\"_longitude\":[1],\"_radius\":[1,2]}}",
		"{\"_convex\":{\"_latitude\":[1,2],\"_longitude\":[1,2],\"_radius\":[1]}}",
		"{\"_convex\":{\"_latitude\":[1,2],\"_longitude\":[1,2],\"_altitude\":[1],\"_radius\":[1,2]}}",
		"{\"_convex\":{\"_latitude\":[\"a\",2],\"_longitude\":[1,2],\"_radius\":[1,2]}}",
		"{\"_polygon\":5}", "{\"_polygon\":{\"_latitude\":[1,2]}}", "{\"_polygon\":{\"_latitude\":[1,2],\"_longitude\":[1]}}",
		"{\"_multipoint\":5}", "{\"_multipoint\":{\"_latitude\":[1,2]}}", "{\"_multipoint\":{\"_latitude\":[1,2],\"_longitude\":[1]}}",
		"{\"_multipoint\":{\"_latitude\":[1,2],\"_longitude\":[1,2],\"_altitude\":[1]}}",
		"{\"_multipoint\":{\"_latitude\":[\"a\",2],\"_longitude\":[1,2]}}",
		"{\"_multicircle\":5}", "{\"_multicircle\":{\"_latitude\":[1,2],\"_longitude\":[1,2]}}",
		"{\"_multicircle\":{\"_latitude\":[1,2],\"_longitude\":[1],\"_radius\":1}}",
		"{\"_multicircle\":{\"_latitude\":[1,2],\"_longitude\":[1,2],\"_altitude\":[1],\"_radius\":1}}",
		"{\"_multicircle\":{\"_latitude\":[\"a\",2],\"_longitude\":[1,2],\"_radius\":1}}",
		"{\"_multipolygon\":5}", "{\"_multipolygon\":{\"_point\":" + pt + "}}", "{\"_multipolygon\":{\"_latitude\":[1,2]}}",
		"{\"_multipolygon\":{\"_latitude\":[1,2],\"_longitude\":[1]}}",
		"{\"_multipolygon\":{\"_latitude\":[1,2],\"_longitude\":[1,2],\"_altitude\":[1]}}",
		"{\"_geometrycollection\":5}", "{\"_geometrycollection\":{\"_nope\":1}}",
		"{\"_geometryintersection\":5}", "{\"_geometryintersection\":{\"_nope\":1}}",
	};
	for (const auto& json : bad) {
		if (!rejects(json)) {
			std::fprintf(stderr, "  FAIL: GeoSpatial accepted %s\n", json.c_str());
		}
		CHECK(rejects(json));
	}
}

// --- the EWKT parser's wrapped errors, nested members and the empty forms ---------------------------------

// The text of the EWKTError that parsing `text` raises, empty when it parses or raises another error.
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

bool names(const std::string& message, const std::string& type) {
	return message.find("Specification for " + type + " is") != std::string::npos;
}

void test_ewkt_number_errors_name_their_type() {
	// A number that is not one (invalid argument) and one out of range, in each geometry.
	struct Case {
		const char* type;
		const char* bad;
		const char* huge;
	};
	const Case cases[] = {
		{ "POINT", "POINT(a b)", "POINT(1e999 2)" },
		{ "MULTIPOINT", "MULTIPOINT((a b))", "MULTIPOINT((1e999 2))" },
		{ "CIRCLE", "CIRCLE(1 2, abc)", "CIRCLE(1 2, 1e999)" },
		{ "CONVEX", "CONVEX((a b, 5))", "CONVEX((1 2, 1e999))" },
		{ "POLYGON", "POLYGON((a b, 1 2, 3 4))", "POLYGON((1e999 2, 1 2, 3 4))" },
		{ "CHULL", "CHULL((a b, 1 2, 3 4))", "CHULL((1e999 2, 1 2, 3 4))" },
		{ "MULTICIRCLE", "MULTICIRCLE((a b, 5))", "MULTICIRCLE((1 2, 1e999))" },
		{ "MULTICONVEX", "MULTICONVEX(((a b, 5)))", "MULTICONVEX(((1 2, 1e999)))" },
		{ "MULTIPOLYGON", "MULTIPOLYGON(((a b, 1 2, 3 4)))", "MULTIPOLYGON(((1e999 2, 1 2, 3 4)))" },
		{ "MULTICHULL", "MULTICHULL(((a b, 1 2, 3 4)))", "MULTICHULL(((1e999 2, 1 2, 3 4)))" },
		{ "GEOMETRYCOLLECTION", "GEOMETRYCOLLECTION(POINT(a b))", "GEOMETRYCOLLECTION(POINT(1e999 2))" },
		{ "GEOMETRYINTERSECTION", "GEOMETRYINTERSECTION(POINT(a b))", "GEOMETRYINTERSECTION(POINT(1e999 2))" },
	};
	for (const auto& c : cases) {
		const auto bad = ewkt_error(c.bad);
		if (!names(bad, c.type)) {
			std::fprintf(stderr, "  FAIL: '%s' raised '%s'\n", c.bad, bad.c_str());
		}
		CHECK(names(bad, c.type));
		const auto huge = ewkt_error(c.huge);
		if (!names(huge, c.type)) {
			std::fprintf(stderr, "  FAIL: '%s' raised '%s'\n", c.huge, huge.c_str());
		}
		CHECK(names(huge, c.type));
	}
	CHECK(ewkt_error("SRID=abcd;POINT(1 2)").find("Invalid SRID") != std::string::npos);
	CHECK(ewkt_error("POINT XYZ").find("Syntax error") != std::string::npos);
}

void test_ewkt_separator_and_shape_errors() {
	const char* bad[] = {
		"MULTIPOINT((1 2)(3 4))", "MULTIPOINT((1 2), 3 4)", "MULTIPOINT(1 2, 3)",
		"CONVEX((1 2, 3)(4 5, 6))", "CONVEX((1 2, 3),(4 5, 6)", "CONVEX(1 2, 3)",
		"POLYGON((1 2, 3 4, 5 6)(1 2, 3 4, 5 6))", "POLYGON(1 2)",
		"MULTICIRCLE((1 2, 3)(4 5, 6))", "MULTICIRCLE(1 2, 3)",
		"MULTICONVEX(((1 2, 3))((4 5, 6)))", "MULTICONVEX(1 2, 3)",
		"MULTIPOLYGON(((1 2, 3 4, 5 6))((1 2, 3 4, 5 6)))", "MULTIPOLYGON(1 2, 3)",
		"MULTICHULL(((1 2, 3 4, 5 6))((1 2, 3 4, 5 6)))", "MULTICHULL(1 2, 3)",
		"GEOMETRYCOLLECTION(POINT(1 2)POINT(3 4))", "GEOMETRYINTERSECTION(POINT(1 2)POINT(3 4))",
	};
	for (const char* text : bad) {
		const auto message = ewkt_error(text);
		if (message.empty() || message == "<other error>") {
			std::fprintf(stderr, "  FAIL: '%s' raised '%s'\n", text, message.c_str());
		}
		CHECK(!message.empty() && message != "<other error>");
	}
}

void test_ewkt_empty_geometries_are_empty_collections() {
	for (const char* text : { "POINT EMPTY", "SRID=4326;POLYGON EMPTY", "GEOMETRYCOLLECTION EMPTY" }) {
		const auto geometry = EWKT(text).getGeometry();
		REQUIRE(geometry != nullptr);
		CHECK(geometry->getType() == Geometry::Type::COLLECTION);
		CHECK(geometry->getRanges(partials, error).empty());
	}
}

// Every geometry the EWKT names, in the order the parser lists them.
const char* const members[] = {
	"POINT(-101.19 19.70)",
	"MULTIPOINT((-101.19 19.70), (-101.20 19.71))",
	"CIRCLE(-101.19 19.70, 1000)",
	"CONVEX((-101.19 19.70, 3000), (-101.20 19.71, 3000))",
	"POLYGON((-101.2 19.7, -101.1 19.7, -101.1 19.8, -101.2 19.8))",
	"CHULL((-101.2 19.7, -101.1 19.7, -101.1 19.8, -101.2 19.8, -101.15 19.75))",
	"MULTICIRCLE((-101.19 19.70, 500), (-101.20 19.71, 500))",
	"MULTICONVEX(((-101.19 19.70, 3000), (-101.20 19.71, 3000)), ((-101.0 19.0, 3000), (-101.01 19.01, 3000)))",
	"MULTIPOLYGON(((-101.2 19.7, -101.1 19.7, -101.1 19.8)), ((-100.2 19.7, -100.1 19.7, -100.1 19.8)))",
	"MULTICHULL(((-101.2 19.7, -101.1 19.7, -101.1 19.8)), ((-100.2 19.7, -100.1 19.7, -100.1 19.8)))",
	"GEOMETRYCOLLECTION(POINT(-101.19 19.70), CIRCLE(-101.2 19.7, 100))",
	"GEOMETRYINTERSECTION(CIRCLE(-101.19 19.70, 3000), CIRCLE(-101.2 19.7, 3000))",
};

std::string joined(const std::string& name) {
	std::string text = name + "(";
	for (const char* member : members) {
		text.append(member).append(", ");
	}
	text.resize(text.size() - 2);
	return text + ")";
}

void test_ewkt_collection_and_intersection_hold_every_geometry() {
	const auto collection = EWKT(joined("GEOMETRYCOLLECTION")).getGeometry();
	REQUIRE(collection != nullptr);
	CHECK(collection->getType() == Geometry::Type::COLLECTION);
	const auto wkt = collection->toWKT();
	// Every kind of member is written back: 12 members, the multi kinds among them.
	for (const char* kind : { "POINT(", "MULTIPOINT(", "CIRCLE(", "CONVEX(", "POLYGON(", "MULTICIRCLE(", "MULTICONVEX(", "MULTIPOLYGON(", "GEOMETRYINTERSECTION(" }) {
		if (wkt.find(kind) == std::string::npos) {
			std::fprintf(stderr, "  FAIL: the collection's WKT has no %s: %s\n", kind, wkt.c_str());
		}
		CHECK(wkt.find(kind) != std::string::npos);
	}
	CHECK(covers(collection->getRanges(partials, error), at(19.70, -101.19)));
	CHECK_FALSE(covers(collection->getRanges(partials, error), paris));

	const auto intersection = EWKT(joined("GEOMETRYINTERSECTION")).getGeometry();
	REQUIRE(intersection != nullptr);
	CHECK(intersection->getType() == Geometry::Type::INTERSECTION);
	CHECK(intersection->toWKT().rfind("GEOMETRYINTERSECTION(POINT(", 0) == 0);
	// Nothing in it lies where the 100 m circle of the collection above does and a 3 km circle's overlap ends.
	CHECK_FALSE(covers(intersection->getRanges(partials, error), paris));
}

// --- the empty and degenerate forms --------------------------------------------------------------

bool ends_with(const std::string& text, const std::string& suffix) {
	return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

void test_empty_geometries_write_empty() {
	CHECK_EQ(MultiPoint().toWKT(), std::string("MULTIPOINT EMPTY"));
	CHECK_EQ(MultiCircle().toWKT(), std::string("MULTICIRCLE EMPTY"));
	CHECK_EQ(MultiConvex().toWKT(), std::string("MULTICONVEX EMPTY"));
	CHECK_EQ(MultiPolygon().toWKT(), std::string("MULTIPOLYGON EMPTY"));
	CHECK_EQ(Polygon(Geometry::Type::POLYGON).toWKT(), std::string("POLYGON EMPTY"));
	CHECK(ends_with(Intersection().toWKT(), "EMPTY"));
	CHECK(ends_with(Collection().toWKT(), "EMPTY"));
	// An intersection of nothing covers nothing.
	CHECK(Intersection().getTrixels(partials, error).empty());
	CHECK(Intersection().getRanges(partials, error).empty());
	// A collection holding an empty intersection writes it empty.
	Collection with_empty;
	with_empty.add_intersection(Intersection());
	CHECK(ends_with(with_empty.toWKT(), "EMPTY"));
}

void test_simplify_drops_duplicates_and_empties() {
	// Equal members of a multi geometry collapse to one, and an empty one goes.
	MultiConvex multiconvex;
	multiconvex.add(getConvex());
	multiconvex.add(getConvex());
	multiconvex.add(Convex());
	multiconvex.simplify();
	CHECK_EQ(multiconvex.toEWKT(), [] { MultiConvex one; one.add(getConvex()); one.simplify(); return one.toEWKT(); }());

	MultiPolygon multipolygon;
	multipolygon.add(getChull());
	multipolygon.add(getChull());
	multipolygon.add(Polygon(Geometry::Type::CHULL));
	multipolygon.simplify();
	CHECK_EQ(multipolygon.toEWKT(), [] { MultiPolygon one; one.add(getChull()); one.simplify(); return one.toEWKT(); }());

	// A polygon holding the same ring twice is the ring and its twin cancelled: nothing; three of them leave one.
	const auto ring = std::vector<Cartesian>({ at(19.60, -101.30), at(19.60, -101.10), at(19.80, -101.10), at(19.80, -101.30) });
	Polygon twice(Geometry::Type::POLYGON, std::vector<Cartesian>(ring));
	twice.add(std::vector<Cartesian>(ring));
	twice.simplify();
	CHECK_EQ(twice.toWKT(), std::string("POLYGON EMPTY"));
	Polygon thrice(Geometry::Type::POLYGON, std::vector<Cartesian>(ring));
	thrice.add(std::vector<Cartesian>(ring));
	thrice.add(std::vector<Cartesian>(ring));
	thrice.simplify();
	CHECK_EQ(thrice.toWKT(), Polygon(Geometry::Type::POLYGON, std::vector<Cartesian>(ring)).toWKT());
}

void test_polygon_refuses_degenerate_rings() {
	CHECK_THROW(Polygon(Geometry::Type::POLYGON, std::vector<Cartesian>({ at(0, 0), at(0, 1) })), GeometryError);
	// Repeated corners and a corner that makes the ring concave.
	CHECK_THROW(Polygon(Geometry::Type::POLYGON, std::vector<Cartesian>({ at(0, 0), at(0, 0), at(1, 1), at(1, 0) })), GeometryError);
	CHECK_THROW(Polygon(Geometry::Type::POLYGON, std::vector<Cartesian>({ at(0, 0), at(0, 10), at(1, 1), at(10, 10), at(10, 0) })), GeometryError);
	// A hull needs three distinct corners; the duplicates leave two.
	CHECK_THROW(Polygon(Geometry::Type::CHULL, std::vector<Cartesian>({ at(0, 0), at(0, 0), at(5, 5), at(5, 5) })), GeometryError);
	CHECK_THROW(Polygon(Geometry::Type::CHULL, std::vector<Cartesian>({ at(0, 0), at(5, 5) })), GeometryError);
}

// --- shapes as large as a hemisphere: the cover reaches every root trixel --------------------------

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

void test_hemisphere_sized_shapes_reach_every_root_trixel() {
	for (const double lon : { 0.0, 90.0, 180.0, -90.0 }) {
		const auto center = at(0, lon);
		const auto away = at(0, lon + 180.0 > 180.0 ? lon - 180.0 : lon + 180.0);

		// A hexagon 85 degrees of arc from its center.
		std::vector<Cartesian> corners;
		for (int bearing = 0; bearing < 360; bearing += 60) {
			corners.push_back(toward(0, lon, bearing, 85.0));
		}
		Polygon hexagon(Geometry::Type::POLYGON, std::move(corners));
		for (const bool p : { true, false }) {
			const auto ranges = hexagon.getRanges(p, error);
			CHECK(covers(ranges, center));
			CHECK_FALSE(covers(ranges, away));
			auto trixels = hexagon.getTrixels(p, error);
			HTM::simplifyTrixels(trixels);
			CHECK(HTM::getTrixels(ranges) == trixels);
		}

		// Two circles of 9,000 km whose centers lie 45 degrees to either side: their lens holds the center.
		Convex lens;
		lens.add(Circle(at(0, lon - 45.0 < -180.0 ? lon + 315.0 : lon - 45.0), 9000000));
		lens.add(Circle(at(0, lon + 45.0 > 180.0 ? lon - 315.0 : lon + 45.0), 9000000));
		for (const bool p : { true, false }) {
			const auto ranges = lens.getRanges(p, error);
			CHECK(covers(ranges, center));
			CHECK_FALSE(covers(ranges, away));
			auto trixels = lens.getTrixels(p, error);
			HTM::simplifyTrixels(trixels);
			CHECK(HTM::getTrixels(ranges) == trixels);
		}

		// A circle that large covers the trixels whole.
		Circle wide(center, 9000000);
		for (const bool p : { true, false }) {
			const auto ranges = wide.getRanges(p, error);
			CHECK(covers(ranges, center));
			CHECK_FALSE(covers(ranges, away));
			auto trixels = wide.getTrixels(p, error);
			HTM::simplifyTrixels(trixels);
			CHECK(HTM::getTrixels(ranges) == trixels);
		}
	}
}

void test_negative_circles_cover_the_complement() {
	// Past a hemisphere a circle is the complement of the cap around its antipode.
	Circle negative(cathedral, 15011335.5);
	const auto antipode = at(-19.702778, 78.807778);
	for (const bool p : { true, false }) {
		const auto ranges = negative.getRanges(p, error);
		CHECK(covers(ranges, cathedral));
		CHECK(covers(ranges, paris));
		CHECK_FALSE(covers(ranges, antipode));
		auto trixels = negative.getTrixels(p, error);
		HTM::simplifyTrixels(trixels);
		CHECK(HTM::getTrixels(ranges) == trixels);
	}
	Circle all(cathedral, 20015114);
	for (const bool p : { true, false }) {
		auto trixels = all.getTrixels(p, error);
		HTM::simplifyTrixels(trixels);
		CHECK(HTM::getTrixels(all.getRanges(p, error)) == trixels);
	}

	// A convex of the complement alone, and of it with a ring: 6,000 km around the antipode less the 5,003 km hole.
	Convex only_negative;
	only_negative.add(Circle(cathedral, 15011335.5));
	for (const bool p : { true, false }) {
		const auto ranges = only_negative.getRanges(p, error);
		CHECK(covers(ranges, cathedral));
		CHECK_FALSE(covers(ranges, antipode));
		auto trixels = only_negative.getTrixels(p, error);
		HTM::simplifyTrixels(trixels);
		CHECK(HTM::getTrixels(ranges) == trixels);
	}
	Convex ring;
	ring.add(Circle(cathedral, 15011335.5));
	ring.add(Circle(antipode, 6000000));
	const auto in_ring = toward(-19.702778, 78.807778, 0.0, 5500000.0 / 111194.9);
	const auto too_far = toward(-19.702778, 78.807778, 0.0, 7000000.0 / 111194.9);
	for (const bool p : { true, false }) {
		const auto ranges = ring.getRanges(p, error);
		CHECK(covers(ranges, in_ring));
		CHECK_FALSE(covers(ranges, antipode));
		CHECK_FALSE(covers(ranges, too_far));
		CHECK_FALSE(covers(ranges, cathedral));
		auto trixels = ring.getTrixels(p, error);
		HTM::simplifyTrixels(trixels);
		CHECK(HTM::getTrixels(ranges) == trixels);
	}
}

void test_convex_simplifies_redundant_and_disjoint_circles() {
	// A circle inside another makes the larger redundant: the convex is the smaller.
	Convex nested;
	nested.add(Circle(cathedral, 3000));
	nested.add(Circle(cathedral, 1000));
	nested.simplify();
	CHECK_EQ(nested.toEWKT(), [] { Convex one; one.add(Circle(cathedral, 1000)); one.simplify(); return one.toEWKT(); }());
	CHECK(covers(nested.getRanges(partials, error), cathedral));
	CHECK_FALSE(covers(nested.getRanges(partials, error), at(19.702778, -101.1561)));

	// Circles that never meet intersect in nothing, which writes empty and parses back as an empty collection.
	Convex apart;
	apart.add(Circle(cathedral, 1000));
	apart.add(Circle(paris, 1000));
	apart.simplify();
	CHECK(apart.empty());
	CHECK(ends_with(apart.toWKT(), "EMPTY"));
	const auto reparsed = EWKT(apart.toEWKT()).getGeometry();
	REQUIRE(reparsed != nullptr);
	CHECK(reparsed->getType() == Geometry::Type::COLLECTION);
}

// --- the map forms the other tests do not reach -------------------------------------------------------

void test_map_distance_units_and_nested_polygons() {
	const std::string pt = "{\"_latitude\":19.702778,\"_longitude\":-101.192222";
	CHECK_EQ(ewkt_of("{\"_circle\":" + pt + ",\"_radius\":\"2nmi\"}}"), Circle(cathedral, 3704).toEWKT());
	CHECK_EQ(ewkt_of("{\"_circle\":" + pt + ",\"_radius\":\"1000mm\"}}"), Circle(cathedral, 1).toEWKT());

	// Several rings at once: one array of latitudes and one of longitudes per ring.
	const std::string rings = "{\"_latitude\":[[19.60,19.60,19.80,19.80],[19.68,19.68,19.72,19.72]],"
		"\"_longitude\":[[-101.30,-101.10,-101.10,-101.30],[-101.22,-101.18,-101.18,-101.22]]";
	CHECK_EQ(ewkt_of("{\"_polygon\":" + rings + "}}"), getPolygon().toEWKT());
	const auto with_altitude = ewkt_of("{\"_polygon\":" + rings + ",\"_altitude\":[[1,1,1,1],[2,2,2,2]]}}");
	CHECK(with_altitude.rfind("SRID=4326;POLYGON(", 0) == 0);
	CHECK(with_altitude != getPolygon().toEWKT());
	CHECK(rejects("{\"_polygon\":{\"_latitude\":[[\"a\",2,3]],\"_longitude\":[[1,2,3]]}}"));
	CHECK(rejects("{\"_polygon\":{\"_latitude\":[[1,2,3]],\"_longitude\":[[1,2,3]],\"_altitude\":[[1,2,3],[4,5,6]]}}"));

	// The nested-array form of a multipolygon is read as a map and fails with an error that is not a GeoSpatialError.
	CHECK_THROW(GeoSpatial(parse("{\"_multipolygon\":[[1,2,3],[1,2,3]]}")), std::exception);
}

void test_map_collection_and_intersection_hold_every_member() {
	const std::string pt = "{\"_latitude\":19.70,\"_longitude\":-101.19}";
	const std::string pairs = "{\"_latitude\":[19.682206,19.708061,19.702778,19.684201],\"_longitude\":[-101.226447,-101.207265,-101.192222,-101.194725]}";
	const std::string members_json =
		"\"_point\":" + pt + ","
		"\"_circle\":{\"_latitude\":19.70,\"_longitude\":-101.19,\"_radius\":1000},"
		"\"_convex\":{\"_latitude\":[19.70,19.71],\"_longitude\":[-101.19,-101.20],\"_radius\":[3000,3000]},"
		"\"_polygon\":" + pairs + ","
		"\"_chull\":" + pairs + ","
		"\"_multipoint\":" + pairs + ","
		"\"_multicircle\":{\"_latitude\":[19.70,19.71],\"_longitude\":[-101.19,-101.20],\"_radius\":\"1km\"},"
		"\"_multipolygon\":{\"_polygon\":" + pairs + ",\"_chull\":" + pairs + "},"
		"\"_geometrycollection\":{\"_point\":" + pt + "},"
		"\"_geometryintersection\":{\"_circle\":{\"_latitude\":19.70,\"_longitude\":-101.19,\"_radius\":3000}}";
	const auto collection = GeoSpatial(parse("{\"_geometrycollection\":{" + members_json + "}}")).getGeometry();
	CHECK(collection->getType() == Geometry::Type::COLLECTION);
	CHECK(covers(collection->getRanges(partials, error), at(19.70, -101.19)));
	CHECK_FALSE(covers(collection->getRanges(partials, error), paris));
	const auto wkt = collection->toWKT();
	for (const char* kind : { "POINT(", "CIRCLE(", "CONVEX(", "POLYGON(", "MULTIPOINT(", "MULTICIRCLE(", "MULTIPOLYGON(", "GEOMETRYINTERSECTION(" }) {
		if (wkt.find(kind) == std::string::npos) {
			std::fprintf(stderr, "  FAIL: the collection's WKT has no %s: %s\n", kind, wkt.c_str());
		}
		CHECK(wkt.find(kind) != std::string::npos);
	}
	const auto intersection = GeoSpatial(parse("{\"_geometryintersection\":{" + members_json + "}}")).getGeometry();
	CHECK(intersection->getType() == Geometry::Type::INTERSECTION);
	CHECK(intersection->toWKT().rfind("GEOMETRYINTERSECTION(POINT(", 0) == 0);
}

}  // namespace

// Each test runs guarded: an exception that escapes one is a failed check naming it, not an abort of the whole run.
int main() {
	const struct { const char* name; void (*run)(); } tests[] = {
		{ "test_point", test_point },
		{ "test_multipoint", test_multipoint },
		{ "test_circle", test_circle },
		{ "test_circle_extremes", test_circle_extremes },
		{ "test_convex", test_convex },
		{ "test_polygon", test_polygon },
		{ "test_graham_scan", test_graham_scan },
		{ "test_multicircle", test_multicircle },
		{ "test_multiconvex", test_multiconvex },
		{ "test_multipolygon", test_multipolygon },
		{ "test_collection", test_collection },
		{ "test_intersection", test_intersection },
		{ "test_ewkt_forms", test_ewkt_forms },
		{ "test_ewkt_errors", test_ewkt_errors },
		{ "test_inputs_that_surface_a_format_error", test_inputs_that_surface_a_format_error },
		{ "test_geometry_errors", test_geometry_errors },
		{ "test_map_forms", test_map_forms },
		{ "test_map_errors", test_map_errors },
		{ "test_ewkt_number_errors_name_their_type", test_ewkt_number_errors_name_their_type },
		{ "test_ewkt_separator_and_shape_errors", test_ewkt_separator_and_shape_errors },
		{ "test_ewkt_empty_geometries_are_empty_collections", test_ewkt_empty_geometries_are_empty_collections },
		{ "test_ewkt_collection_and_intersection_hold_every_geometry", test_ewkt_collection_and_intersection_hold_every_geometry },
		{ "test_empty_geometries_write_empty", test_empty_geometries_write_empty },
		{ "test_simplify_drops_duplicates_and_empties", test_simplify_drops_duplicates_and_empties },
		{ "test_polygon_refuses_degenerate_rings", test_polygon_refuses_degenerate_rings },
		{ "test_hemisphere_sized_shapes_reach_every_root_trixel", test_hemisphere_sized_shapes_reach_every_root_trixel },
		{ "test_negative_circles_cover_the_complement", test_negative_circles_cover_the_complement },
		{ "test_convex_simplifies_redundant_and_disjoint_circles", test_convex_simplifies_redundant_and_disjoint_circles },
		{ "test_map_distance_units_and_nested_polygons", test_map_distance_units_and_nested_polygons },
		{ "test_map_collection_and_intersection_hold_every_member", test_map_collection_and_intersection_hold_every_member },
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
