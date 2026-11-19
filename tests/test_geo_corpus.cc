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
// which. The table holds each answer now; where the fixes of findings F18-F23 changed it, `before` holds the answer
// before them. Every other query keeps the answer it had, so a change to the geometry code that moves a hit shows
// here. Run it with the argument `print` to list every query's values.

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
	// Each random draw is its own statement: the order of evaluation of call arguments differs between compilers.
	for (int i = 0; i < 4000; ++i) {
		const double sin_lat = unit() * 2.0 - 1.0;
		const double lon = unit() * 360.0 - 180.0;
		ids.push_back(HTM::getId(at(std::asin(sin_lat) * 180.0 / M_PI, lon)));
	}
	for (int i = 0; i < 1000; ++i) {
		const double dlat = unit() - 0.5;
		const double dlon = unit() - 0.5;
		ids.push_back(HTM::getId(at(19.7028 + dlat, -101.1922 + dlon)));
		const double plat = unit() - 0.5;
		const double plon = unit() - 0.5;
		ids.push_back(HTM::getId(at(48.8566 + plat, 2.3522 + plon)));
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

struct Expected {
	const char* query;
	size_t setting;
	const char* now;
	const char* before;  // null: the answer is the same as before the fixes
};

const Expected expected[] = {
	{ "circle 5km", 0, "14 8611342563437d95", nullptr },
	{ "circle 5km", 1, "12 992331c2f96928c5", nullptr },
	{ "circle 5km", 2, "12 992331c2f96928c5", nullptr },
	{ "circle 50km", 0, "794 d9a123e8b5327873", nullptr },
	{ "circle 50km", 1, "691 0957a9fc46c7c1bf", nullptr },
	{ "circle 50km", 2, "627 ff285499e8f7129d", nullptr },
	{ "circle 1000km", 0, "1031 b4012a3a97ca3800", nullptr },
	{ "circle 1000km", 1, "1030 6bb178f692dd504f", nullptr },
	{ "circle 1000km", 2, "1026 67ad1a21c5ade005", nullptr },
	{ "circle 5000km", 0, "1798 1684aca47ac59478", nullptr },
	{ "circle 5000km", 1, "1616 830cab0d5a5fded4", nullptr },
	{ "circle 5000km", 2, "1576 65836a4286cad946", nullptr },
	{ "circle 9000km", 0, "4139 c14c8bf4f0ef7034", nullptr },
	{ "circle 9000km", 1, "2734 33cf289faa6cbfe5", nullptr },
	{ "circle 9000km", 2, "2625 7f58789897ab3b7e", nullptr },
	{ "circle 15011km", 0, "5745 062d1a85c5edad9b", nullptr },
	{ "circle 15011km", 1, "5517 9b2ff0c1b5d71be0", nullptr },
	{ "circle 15011km", 2, "5340 a44a422063fe78d4", nullptr },
	{ "circle equator 3000km", 0, "258 dc07773831a674bc", nullptr },
	{ "circle equator 3000km", 1, "230 182caee2375e1e11", nullptr },
	{ "circle equator 3000km", 2, "215 2a40112f0014c021", nullptr },
	{ "circle paris 300km", 0, "1001 1b4dda9fc9a8d1e5", nullptr },
	{ "circle paris 300km", 1, "1001 1b4dda9fc9a8d1e5", nullptr },
	{ "circle paris 300km", 2, "1001 1b4dda9fc9a8d1e5", nullptr },
	{ "convex overlap", 0, "69 e75bf76b98489fef", nullptr },
	{ "convex overlap", 1, "49 15d97c3f6f518373", nullptr },
	{ "convex overlap", 2, "45 c452694f445dcd0b", nullptr },
	{ "convex three", 0, "139 fbabe2eabba5989d", nullptr },
	{ "convex three", 1, "116 b101031fb338a0ef", nullptr },
	{ "convex three", 2, "98 7a233d7d1f4fd4bd", nullptr },
	{ "convex nested", 0, "41 50679c7f72990361", nullptr },
	{ "convex nested", 1, "32 2978e07f82435e4b", nullptr },
	{ "convex nested", 2, "31 df2b246929e0a4dd", nullptr },
	{ "convex apart", 0, "0 14650fb0739d0383", "6015 56a3e7cf26f4618e" },
	{ "convex apart", 1, "0 14650fb0739d0383", "6015 56a3e7cf26f4618e" },
	{ "convex apart", 2, "0 14650fb0739d0383", "6015 56a3e7cf26f4618e" },
	{ "convex lens 9000km", 0, "2320 85ec887a66f27882", nullptr },
	{ "convex lens 9000km", 1, "1739 edcab474227754a7", nullptr },
	{ "convex lens 9000km", 2, "1630 969adea45c03347d", nullptr },
	{ "convex lens 5000km", 0, "1532 4b23c9d6d08415fd", nullptr },
	{ "convex lens 5000km", 1, "1356 e0f1ac729540c320", nullptr },
	{ "convex lens 5000km", 2, "1319 67c5fe285bca1c41", nullptr },
	{ "convex negative", 0, "5788 da7f1e68a32cb08d", nullptr },
	{ "convex negative", 1, "5539 1dcfb87910f95952", nullptr },
	{ "convex negative", 2, "5340 a44a422063fe78d4", "4396 847af69f69987c1c" },
	{ "convex negative hole 1000km", 0, "6015 56a3e7cf26f4618e", nullptr },
	{ "convex negative hole 1000km", 1, "6007 58cec41bd1dd545c", "6015 56a3e7cf26f4618e" },
	{ "convex negative hole 1000km", 2, "5978 f2ed48116b02ad88", "5071 17550bb06e5ddcc2" },
	{ "convex ring largest first", 0, "725 cf81ebe4718c8277", nullptr },
	{ "convex ring largest first", 1, "311 10758e85e3e4f295", nullptr },
	{ "convex ring largest first", 2, "196 7ea25fcdbc6c79f7", nullptr },
	{ "convex ring smallest first", 0, "725 cf81ebe4718c8277", nullptr },
	{ "convex ring smallest first", 1, "311 10758e85e3e4f295", nullptr },
	{ "convex ring smallest first", 2, "196 7ea25fcdbc6c79f7", nullptr },
	{ "convex cap and negative", 0, "2031 b30878b765ee7913", nullptr },
	{ "convex cap and negative", 1, "1805 e635d2cf443b07f5", nullptr },
	{ "convex cap and negative", 2, "1668 ebedd8539de9a7b4", nullptr },
	{ "polygon square", 0, "56 df8fa6740ae03d11", nullptr },
	{ "polygon square", 1, "49 a3d81d282cca1265", nullptr },
	{ "polygon square", 2, "44 fe8ea213f6d7a3bf", nullptr },
	{ "polygon big", 0, "1171 6115c16e3ff40255", nullptr },
	{ "polygon big", 1, "1110 0bd9a7615074dd7d", nullptr },
	{ "polygon big", 2, "1099 87fb1ecfb764aa6f", nullptr },
	{ "polygon with hole", 0, "260 05d50f555662144f", nullptr },
	{ "polygon with hole", 1, "438 9c17bab28353ad33", nullptr },
	{ "polygon with hole", 2, "476 b61c6f357b08707f", nullptr },
	{ "chull", 0, "61 fb8978dc1c1ac29f", nullptr },
	{ "chull", 1, "37 af93e9b07f22c125", nullptr },
	{ "chull", 2, "31 9ceceb81796dea03", nullptr },
	{ "multipoint parenthesized", 0, "0 14650fb0739d0383", nullptr },
	{ "multipoint parenthesized", 1, "0 14650fb0739d0383", nullptr },
	{ "multipoint parenthesized", 2, "0 14650fb0739d0383", nullptr },
	{ "multipoint plain", 0, "0 14650fb0739d0383", "error" },
	{ "multipoint plain", 1, "0 14650fb0739d0383", "error" },
	{ "multipoint plain", 2, "0 14650fb0739d0383", "error" },
	{ "multipoint plain one", 0, "0 14650fb0739d0383", "error" },
	{ "multipoint plain one", 1, "0 14650fb0739d0383", "error" },
	{ "multipoint plain one", 2, "0 14650fb0739d0383", "error" },
	{ "multipoint malformed", 0, "error", nullptr },
	{ "multipoint malformed", 1, "error", nullptr },
	{ "multipoint malformed", 2, "error", nullptr },
	{ "multicircle", 0, "696 0110236783c1a64d", nullptr },
	{ "multicircle", 1, "615 6a14c77a5d66d178", nullptr },
	{ "multicircle", 2, "559 04880606f5426ab0", nullptr },
	{ "multiconvex", 0, "239 1481ddfd46af54d7", nullptr },
	{ "multiconvex", 1, "179 d0c625427b628421", nullptr },
	{ "multiconvex", 2, "163 8821e01eed200003", nullptr },
	{ "multipolygon", 0, "816 cd4452ab3b9c3df7", nullptr },
	{ "multipolygon", 1, "631 f6d6e5b7189205ed", nullptr },
	{ "multipolygon", 2, "579 9a173808e6464660", nullptr },
	{ "collection", 0, "412 fa1dabac5427ecb7", nullptr },
	{ "collection", 1, "361 3743328f27d58d02", nullptr },
	{ "collection", 2, "331 86e44f38e2baa9ac", nullptr },
	{ "intersection of circles", 0, "170 f2f67b410d011a9b", nullptr },
	{ "intersection of circles", 1, "149 159d9d7fa1b89a13", nullptr },
	{ "intersection of circles", 2, "133 6bad5cd55800682d", nullptr },
	{ "collection of empty intersection", 0, "0 14650fb0739d0383", nullptr },
	{ "collection of empty intersection", 1, "0 14650fb0739d0383", nullptr },
	{ "collection of empty intersection", 2, "0 14650fb0739d0383", nullptr },
};

}  // namespace

int main(int argc, char** argv) {
	const auto ids = corpus_ids();
	const bool print = argc > 1 && std::strcmp(argv[1], "print") == 0;
	size_t found = 0;
	for (const auto& query : queries()) {
		for (size_t k = 0; k < sizeof(settings) / sizeof(settings[0]); ++k) {
			const auto value = answer(query, settings[k], ids);
			if (print) {
				std::printf("%s | %zu | %s\n", query.name, k, value.c_str());
				continue;
			}
			for (const auto& e : expected) {
				if (e.setting == k && std::strcmp(e.query, query.name) == 0) {
					++found;
					if (value != e.now) {
						std::fprintf(stderr, "  FAIL: '%s' at setting %zu hits '%s', expected '%s'\n", query.name, k, value.c_str(), e.now);
					}
					CHECK_EQ(value, std::string(e.now));
				}
			}
		}
	}
	if (print) {
		return 0;
	}
	CHECK_EQ(found, sizeof(expected) / sizeof(expected[0]));
	if (testh::failures != 0) {
		std::fprintf(stderr, "%d of %d checks FAILED\n", testh::failures, testh::checks);
		return 1;
	}
	std::printf("OK: %d checks passed\n", testh::checks);
	return 0;
}
