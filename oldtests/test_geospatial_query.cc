/*
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

#include "test_geospatial_query.h"

#include "utils.h"


const std::string path_test_geo = std::string(FIXTURES_PATH) + "/examples/json/";


const std::vector<test_geo_t> geo_range_tests({
	// The range search always is sort by centroids' search.
	{
		// Search: The polygon's search  describes North Dakota.
		"location:..\"POLYGON((-104.026930 48.998427, -104.039833 45.931363, -96.569131 45.946643, -97.228311 48.990383))\"",
		{ "North Dakota", "Bismarck", "Minot", "North Dakota and South Dakota" }
	},
	{
		// Search: The Multipolygon's search  describes North Dakota and South Dakota.
		"location:..\"MULTIPOLYGON(((-104.026930 48.998427, -104.039833 45.931363, -96.569131 45.946643, -97.228311 48.990383)), ((-104.039833 45.931363, -104.050903 43.005315, -96.514283 42.513275, -96.569131 45.946643)))\"",
		{ "North Dakota", "Bismarck", "Minot", "Rapid City", "Wyoming", "North Dakota and South Dakota" }
	},
	{
		// Search: The polygon's search  describes Wyoming.
		"location:..\"POLYGON((-111.038993 44.991571, -111.039795 41.002575, -104.044008 41.000901, -104.055265 44.988552))\"",
		{ "Utah", "Wyoming", "Mountain View, Wyoming", "North Dakota and South Dakota" }
	},
	// Empty regions inside.
	{
		"location:..\"CIRCLE(-100 40, 1000)\"", { }
	}
});


const std::vector<test_geo_t> geo_terms_tests({
	// Test for search by terms.
	{
		"location:\"POLYGON((-104.026930 48.998427, -104.039833 45.931363, -96.569131 45.946643, -97.228311 48.990383))\"",
		{ "North Dakota" }
	},
	{
		"location:\"POINT(-100.783990 46.808598)\"",
		{ "Bismarck" }
	},
	{
		"location:\"POINT(-101.293014 48.233434)\"",
		{ "Minot" }
	},
	{
		"location:\"POINT(-103.237178 44.079583)\"",
		{ "Rapid City" }
	},
	{
		"location:\"MULTIPOLYGON(((-114.0475 41, -114.0475 42, -111.01 42, -111.01 41, -114.0475 41)), ((-114.0475 37, -114.0475 41, -109.0475 41, -109.0475 37, -114.0475 37)))\"",
		{ "Utah" }
	},
	{
		"location:\"POLYGON((-111.038993 44.991571, -111.039795 41.002575, -104.044008 41.000901, -104.055265 44.988552))\"",
		{ "Wyoming" }
	},
	{
		"location:\"POINT(-110.34118652 41.2695495)\"",
		{ "Mountain View, Wyoming" }
	},
	{
		"location:\"MULTIPOLYGON(((-104.026930 48.998427, -104.039833 45.931363, -96.569131 45.946643, -97.228311 48.990383)), ((-104.039833 45.931363, -104.050903 43.005315, -96.514283 42.513275, -96.569131 45.946643)))\"",
		{ "North Dakota and South Dakota" }
	},
	{
		"attraction_location:\"POINT(-110.58837891 44.42789588)\"",
		{ "Wyoming" }
	},
	// There are not terms.
	{
		"location:\"POINT(-100 40)\"", { }
	}
});


static int make_search(const std::vector<test_geo_t> _tests) {
	static DB_Test db_geo(".db_geo.db", std::vector<std::string>({
			path_test_geo + "geo_1.txt",
			path_test_geo + "geo_2.txt",
			path_test_geo + "geo_3.txt",
			path_test_geo + "geo_4.txt",
			path_test_geo + "geo_5.txt",
			path_test_geo + "geo_6.txt",
			path_test_geo + "geo_7.txt",
			path_test_geo + "geo_8.txt"
		}), DB_WRITABLE | DB_CREATE_OR_OPEN | DB_NO_WAL);

	int cont = 0;
	query_field_t query;
	query.sort.push_back(ID_FIELD_NAME);

	for (const auto& test : _tests) {
		query.query.clear();
		query.query.push_back(test.query);

		try {
			std::vector<std::string> suggestions;
			auto mset = db_geo.db_handler.get_mset(query, nullptr, nullptr, suggestions);
			if (mset.size() != test.expect_datas.size()) {
				++cont;
				L_ERR("ERROR: Different number of documents. Obtained %d. Expected: %zu.", mset.size(), test.expect_datas.size());
			} else {
				auto it = test.expect_datas.begin();
				for (auto m = mset.begin(); m != mset.end(); ++it, ++m) {
					auto document = db_geo.db_handler.get_document(*m);
					auto region = document.get_obj().at("region").str();
					if (region != *it) {
						++cont;
						L_ERR("Different regions.\n\t  Result: %s\n\tExpected: %s", region, *it);
					}
				}
			}
		} catch (const std::exception& exc) {
			++cont;
			L_EXC("ERROR: %s\n", exc.what());
		}
	}

	return cont;
}


int geo_range_test() {
	INIT_LOG
	try {
		int cont = make_search(geo_range_tests);
		if (cont == 0) {
			L_DEBUG("Testing search range geospatials is correct!");
		} else {
			L_ERR("ERROR: Testing search range geospatials has mistakes.");
		}
		RETURN(cont);
	} catch (const Xapian::Error& exc) {
		L_EXC("ERROR: %s", exc.get_description());
		RETURN(1);
	} catch (const std::exception& exc) {
		L_EXC("ERROR: %s", exc.what());
		RETURN(1);
	}
}


int geo_terms_test() {
	INIT_LOG
	try {
		int cont = make_search(geo_terms_tests);
		if (cont == 0) {
			L_DEBUG("Testing search by geospatial terms is correct!");
		} else {
			L_ERR("ERROR: Testing search by geospatial terms has mistakes.");
		}
		RETURN(cont);
	} catch (const Xapian::Error& exc) {
		L_EXC("ERROR: %s", exc.get_description());
		RETURN(1);
	} catch (const std::exception& exc) {
		L_EXC("ERROR: %s", exc.what());
		RETURN(1);
	}
}
