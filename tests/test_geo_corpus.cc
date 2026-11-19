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

// A fixed corpus of points and a fixed list of geo queries: how many corpus points each query hits, with a hash of
// which. The values are the answers before the fixes of findings F18-F22; `changed` lists the queries whose answer
// the fixes change, with the answer they give now. Every other query keeps its answer, so a change to the geometry
// code that moves a hit shows here. Run it with the argument `print` to list every query's values.

#include "test_harness.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "cartesian.h"
#include "geospatial/circle.h"
#include "geospatial/collection.h"
#include "geospatial/convex.h"
#include "geospatial/ewkt.h"
#include "geospatial/intersection.h"
#include "geospatial/multipoint.h"
#include "geospatial/point.h"
#include "htm.h"

namespace {

Cartesian at(double lat, double lon) {
	return Cartesian(lat, lon, 0, Cartesian::Units::DEGREES);
}

struct Settings {
	bool partials;
	double error;
};

// The engine's defaults first, then the finest cover, with and without partial trixels.
constexpr Settings settings[] = { { true, 0.3 }, { true, 0.05 }, { false, 0.05 } };

// The ids of the corpus: a uniform scatter, two dense clusters, and points on the mesh's root edges and poles.
std::vector<uint64_t> corpus_ids() {
	std::mt19937_64 rng(20261118);
	const auto unit = [&rng] { return static_cast<double>(rng() % 1000001) / 1000000.0; };
	std::vector<uint64_t> ids;
	for (int i = 0; i < 4000; ++i) {
		ids.push_back(HTM::getId(at(std::asin(unit() * 2.0 - 1.0) * 180.0 / M_PI, unit() * 360.0 - 180.0)));
	}
	for (int i = 0; i < 1000; ++i) {
		ids.push_back(HTM::getId(at(19.7028 + (unit() - 0.5), -101.1922 + (unit() - 0.5))));
		ids.push_back(HTM::getId(at(48.8566 + (unit() - 0.5), 2.3522 + (unit() - 0.5))));
	}
	for (const double lat : { 0.0, 90.0, -90.0 }) {
		for (const double lon : { 0.0, 10.0, 90.0, 180.0, -90.0 }) {
			ids.push_back(HTM::getId(at(lat, lon)));
		}
	}
	return ids;
}

// The geometry the text names, or null when it does not parse.
std::shared_ptr<Geometry> parse(const std::string& text) {
	try {
		return EWKT(text).getGeometry();
	} catch (const std::exception&) {
		return nullptr;
	}
}

std::shared_ptr<Geometry> wide_collection() {
	auto collection = std::make_shared<Collection>();
	collection->add_intersection(Intersection());
	MultiPoint multipoint;
	multipoint.add(Point(at(19.7028, -101.1922)));
	collection->add_multipoint(multipoint);
	return collection;
}

struct Query {
	const char* name;
	std::string ewkt;  // empty: the geometry comes from `build`
	std::shared_ptr<Geometry> (*build)();
};

const std::vector<Query>& queries() {
	static const std::vector<Query> list = {
		{ "circle 5km", "CIRCLE(-101.1922 19.7028, 5000)", nullptr },
		{ "circle 50km", "CIRCLE(-101.1922 19.7028, 50000)", nullptr },
		{ "circle 1000km", "CIRCLE(-101.1922 19.7028, 1000000)", nullptr },
		{ "circle 5000km", "CIRCLE(-101.1922 19.7028, 5000000)", nullptr },
		{ "circle 9000km", "CIRCLE(-101.1922 19.7028, 9000000)", nullptr },
		{ "circle 15011km", "CIRCLE(-101.1922 19.7028, 15011335.5)", nullptr },
		{ "circle equator 3000km", "CIRCLE(10 0, 3000000)", nullptr },
		{ "circle paris 300km", "CIRCLE(2.3522 48.8566, 300000)", nullptr },
		{ "convex overlap", "CONVEX((-101.30 19.70, 20000),(-101.10 19.70, 20000))", nullptr },
		{ "convex three", "CONVEX((-101.30 19.70, 30000),(-101.10 19.70, 30000),(-101.20 19.85, 30000))", nullptr },
		{ "convex nested", "CONVEX((-101.19 19.70, 30000),(-101.19 19.70, 10000))", nullptr },
		{ "convex apart", "CONVEX((-101.19 19.70, 1000),(2.35 48.85, 1000))", nullptr },
		{ "convex lens 9000km", "CONVEX((-45.5 0.5, 9000000),(45.5 0.5, 9000000))", nullptr },
		{ "convex lens 5000km", "CONVEX((-3.5 40.5, 5000000),(41.5 40.5, 5000000))", nullptr },
		{ "convex negative", "CONVEX((-101.1922 19.7028, 15011335.5))", nullptr },
		{ "convex negative hole 1000km", "CONVEX((-101.1922 19.7028, 19015000))", nullptr },
		{ "convex ring largest first", "CONVEX((-101.1922 19.7028, 15011335.5),(78.8078 -19.7028, 6000000))", nullptr },
		{ "convex ring smallest first", "CONVEX((78.8078 -19.7028, 6000000),(-101.1922 19.7028, 15011335.5))", nullptr },
		{ "convex cap and negative", "CONVEX((-101.1922 19.7028, 12000000),(10 0, 8000000))", nullptr },
		{ "polygon square", "POLYGON((-101.30 19.60, -101.10 19.60, -101.10 19.80, -101.30 19.80))", nullptr },
		{ "polygon big", "POLYGON((-120 10, -80 10, -80 40, -120 40))", nullptr },
		{ "polygon with hole", "POLYGON((-102 19, -100 19, -100 21, -102 21),(-101.5 19.5, -100.5 19.5, -100.5 20.5, -101.5 20.5))", nullptr },
		{ "chull", "CHULL((-101.30 19.60, -101.10 19.60, -101.20 19.80, -101.35 19.70))", nullptr },
		{ "multipoint parenthesized", "MULTIPOINT((-101.1922 19.7028), (2.3522 48.8566))", nullptr },
		{ "multipoint plain", "MULTIPOINT(-101.1922 19.7028, 2.3522 48.8566)", nullptr },
		{ "multipoint plain one", "MULTIPOINT(-101.1922 19.7028)", nullptr },
		{ "multipoint malformed", "MULTIPOINT(1 2, 3)", nullptr },
		{ "multicircle", "MULTICIRCLE((-101.1922 19.7028, 30000), (2.3522 48.8566, 30000))", nullptr },
		{ "multiconvex", "MULTICONVEX(((-101.30 19.70, 20000),(-101.10 19.70, 20000)), ((2.30 48.85, 20000),(2.40 48.85, 20000)))", nullptr },
		{ "multipolygon", "MULTIPOLYGON(((-101.30 19.60, -101.10 19.60, -101.10 19.80, -101.30 19.80)), ((2.0 48.5, 2.8 48.5, 2.8 49.2, 2.0 49.2)))", nullptr },
		{ "collection", "GEOMETRYCOLLECTION(POINT(-101.1922 19.7028), CIRCLE(2.3522 48.8566, 30000))", nullptr },
		{ "intersection of circles", "GEOMETRYINTERSECTION(CIRCLE(-101.30 19.70, 30000), CIRCLE(-101.10 19.70, 30000))", nullptr },
		{ "collection of empty intersection", "", wide_collection },
	};
	return list;
}

// What a query hits at a setting: "error" when it does not parse, else "<hits> <hash of the hit indexes>".
std::string answer(const Query& query, const Settings& s, const std::vector<uint64_t>& ids) {
	const auto geometry = query.build != nullptr ? query.build() : parse(query.ewkt);
	if (geometry == nullptr) {
		return "error";
	}
	const auto ranges = geometry->getRanges(s.partials, s.error);
	size_t hits = 0;
	uint64_t hash = 1469598103934665603ULL;
	for (size_t i = 0; i < ids.size(); ++i) {
		for (const auto& r : ranges) {
			if (r.start <= ids[i] && ids[i] <= r.end) {
				++hits;
				hash = (hash ^ i) * 1099511628211ULL;
				break;
			}
		}
	}
	char text[64];
	std::snprintf(text, sizeof(text), "%zu %016llx", hits, static_cast<unsigned long long>(hash));
	return text;
}

}  // namespace

int main(int argc, char** argv) {
	const auto ids = corpus_ids();
	const bool print = argc > 1 && std::strcmp(argv[1], "print") == 0;
	for (const auto& query : queries()) {
		for (size_t k = 0; k < sizeof(settings) / sizeof(settings[0]); ++k) {
			const auto value = answer(query, settings[k], ids);
			if (print) {
				std::printf("%s | %zu | %s\n", query.name, k, value.c_str());
			}
		}
	}
	return 0;
}
