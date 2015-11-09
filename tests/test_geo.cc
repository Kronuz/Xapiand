/*
 * Copyright (C) 2015 deipi.com LLC and contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "test_geo.h"

#include "../src/length.h"


std::shared_ptr<DatabaseQueue>d_queue;
static Database *database = nullptr;
static std::string name_database(".db_testgeo.db");


const test_geo_t geo_range_tests[] {
	// The range search always is sort by centroids' search.
	{
		// Search: The polygon's search  describes North Dakota.
		"location:\"..POLYGON ((48.574789910928864 -103.53515625, 48.864714761802794 -97.2509765625, 45.89000815866182 -96.6357421875, 45.89000815866182 -103.974609375, 48.574789910928864 -103.53515625))\"",
		{ "North Dakota and South Dakota", "North Dakota", "Bismarck", "Minot" }
	},
	{
		// Search: The Multipolygon's search  describes North Dakota and South Dakota.
		"location:\"..MULTIPOLYGON (((48.574789910928864 -103.53515625, 48.864714761802794 -97.2509765625, 45.89000815866182 -96.6357421875, 45.89000815866182 -103.974609375, 48.574789910928864 -103.53515625)), ((45.89000815866182 -103.974609375, 45.89000815866182 -96.6357421875, 42.779275360241904 -96.6796875, 43.03677585761058 -103.9306640625)))\"",
		{ "North Dakota and South Dakota", "North Dakota", "Bismarck", "Minot", "Rapid City", "Wyoming" }
	},
	// { 0.073730, 0.073730, 0.094108, 0.122473, 0.122473, 0.122925, 0.273593, 0.273593, 0.648657, 0.648657 }
	{
		// Search: The polygon's search  describes Wyoming but the corners with a different heights.
		"location:\"..POLYGON ((44.96479793 -111.02783203, 44.96479793 -104.08447266, 41.04621681 -104.08447266, 41.00477542 -111.02783203))\"",
		{ "Wyoming", "Mountain View, Wyoming", "Utah", "North Dakota and South Dakota" }
	},
	// Search for all documents with location.
	{
		"location:..", { "North Dakota", "Bismarck", "Minot", "Rapid City", "Utah", "Wyoming", "Mountain View, Wyoming", "North Dakota and South Dakota" }
	},
	// There are not regions inside.
	{
		"location:\"..CIRCLE (40 -100, 1000)\"", { }
	}
};


const test_geo_t geo_terms_tests[] {
	// Test for search by terms.
	{
		"location:\"POLYGON ((48.574789910928864 -103.53515625, 48.864714761802794 -97.2509765625, 45.89000815866182 -96.6357421875, 45.89000815866182 -103.974609375, 48.574789910928864 -103.53515625))\"",
		{ "North Dakota" }
	},
	{
		"location:\"POINT ((46.84516443029276 -100.78857421875))\"",
		{ "Bismarck" }
	},
	{
		"location:\"POINT ((48.25394114463431 -101.2939453125))\"",
		{ "Minot" }
	},
	{
		"location:\"POINT ((43.992814500489914 -103.18359375))\"",
		{ "Rapid City" }
	},
	{
		"location:\"CHULL ((41.89409956 -113.93920898 1987, 42.02481361 -111.12670898 2095, 41.00477542 -111.02783203 2183, 40.95501133 -109.0612793 2606, 37.01132594 -109.03930664 1407, 37.02886945 -114.00512695 696))\"",
		{ "Utah" }
	},
	{
		"location:\"POLYGON ((44.96479793 -111.02783203 2244, 44.96479793 -104.08447266 969, 41.04621681 -104.08447266 1654, 41.00477542 -111.02783203 2183))\"",
		{ "Wyoming" }
	},
	{
		"location:\"POINT (41.2695495 -110.34118652)\"",
		{ "Mountain View, Wyoming" }
	},
	{
		"location:\"MULTIPOLYGON (((48.574789910928864 -103.53515625, 48.864714761802794 -97.2509765625, 45.89000815866182 -96.6357421875, 45.89000815866182 -103.974609375, 48.574789910928864 -103.53515625)), ((45.89000815866182 -103.974609375, 45.89000815866182 -96.6357421875, 42.779275360241904 -96.6796875, 43.03677585761058 -103.9306640625)))\"",
		{ "North Dakota and South Dakota" }
	},
	{
		"attraction_location:\"POINT (44.42789588, -110.58837891)\"",
		{ "Wyoming" }
	},
	{
		"location:..", { "North Dakota", "Bismarck", "Minot", "Rapid City", "Utah", "Wyoming", "Mountain View, Wyoming", "North Dakota and South Dakota" }
	},
	// There are not terms.
	{
		"location:\"POINT (40, -100)\"", { }
	}
};


int create_test_db() {
	/*
	 *	The database used in the test is local
	 *	so the Endpoints and local_node are manipulated.
	 */

	int cont = 0;
	local_node.name.assign("node_test");
	local_node.binary_port = XAPIAND_BINARY_SERVERPORT;

	Endpoints endpoints;
	Endpoint e;
	e.node_name.assign("node_test");
	e.port = XAPIAND_BINARY_SERVERPORT;
	e.path.assign(name_database);
	e.host.assign("0.0.0.0");
	endpoints.insert(e);

	// There are delete in the make_search.
	database = new Database(d_queue, endpoints, DB_WRITABLE | DB_SPAWN);

	std::vector<std::string> _docs({
		"examples/geo_search/Json_geo_1.txt",
		"examples/geo_search/Json_geo_2.txt",
		"examples/geo_search/Json_geo_3.txt",
		"examples/geo_search/Json_geo_4.txt",
		"examples/geo_search/Json_geo_5.txt",
		"examples/geo_search/Json_geo_6.txt",
		"examples/geo_search/Json_geo_7.txt",
		"examples/geo_search/Json_geo_8.txt"
	});

	// Index documents in the database.
	size_t i = 1;
	for (std::vector<std::string>::iterator it(_docs.begin()); it != _docs.end(); it++) {
		std::ifstream fstream(*it);
		std::stringstream buffer;
		buffer << fstream.rdbuf();
		if (database->index(buffer.str(), std::to_string(i), true, "application/json", std::to_string(fstream.tellg())) == 0) {
			cont++;
			LOG_ERR(nullptr, "ERROR: File %s can not index\n", it->c_str());
		}
		fstream.close();
		++i;
	}

	return cont;
}


int make_search(const test_geo_t _tests[], int len) {
	int cont = 0;
	query_field query;
	query.offset = 0;
	query.limit = 10;
	query.check_at_least = 0;
	query.spelling = false;
	query.synonyms = false;
	query.is_fuzzy = false;
	query.is_nearest = false;

	for (int i = 0; i < len; ++i) {
		test_geo_t p = _tests[i];
		query.query.clear();
		query.query.push_back(p.query);

		Xapian::MSet mset;
		std::vector<std::string> suggestions;
		std::vector<std::pair<std::string, std::unique_ptr<MultiValueCountMatchSpy>>> spies;

		int rmset = database->get_mset(query, mset, spies, suggestions);
		if (rmset != 0) {
			cont++;
			LOG_ERR(nullptr, "ERROR: Failed in get_mset\n");
		} else if (mset.size() != p.expect_datas.size()) {
			cont++;
			LOG_ERR(nullptr, "ERROR: Different number of documents obtained %zu  %zu\n", mset.size(), p.expect_datas.size());
		} else {
			std::vector<std::string>::const_iterator it(p.expect_datas.begin());
			Xapian::MSetIterator m = mset.begin();
			for ( ; m != mset.end(); ++it, ++m) {
				std::string data = m.get_document().get_data().data();
				const char *p = data.data();
				const char *p_end = p + data.size();
				size_t length = decode_length(&p, p_end, true);
				data = std::string(p, length);

				unique_cJSON object(cJSON_Parse(data.c_str()), cJSON_Delete);
				cJSON* object_data = cJSON_GetObjectItem(object.get(), RESERVED_DATA);
				if (object_data && it->compare(object_data->valuestring) != 0) {
					cont++;
					LOG_ERR(nullptr, "ERROR: Result = %s:%s   Expected = %s:%s\n", RESERVED_DATA, data.c_str(), RESERVED_DATA, it->c_str());
				}
			}
		}
	}

	// Delete the databases and release memory.
	delete_files(name_database);
	delete database;

	return cont;
}


int geo_range_test() {
	try {
		int cont = create_test_db();
		if (cont == 0 && make_search(geo_range_tests, arraySize(geo_range_tests)) == 0) {
			LOG(nullptr, "Testing search range geospatials is correct!\n");
			return 0;
		} else {
			LOG_ERR(nullptr, "ERROR: Testing search range geospatials has mistakes.\n");
			return 1;
		}
	} catch (const Xapian::Error &err) {
		LOG_ERR(nullptr, "ERROR: %s\n", err.get_msg().c_str());
		return 1;
	} catch (const std::exception &err) {
		LOG_ERR(nullptr, "ERROR: %s\n", err.what());
		return 1;
	}
}


int geo_terms_test() {
	try {
		int cont = create_test_db();
		if (cont == 0 && make_search(geo_terms_tests, arraySize(geo_terms_tests)) == 0) {
			LOG(nullptr, "Testing search by geospatial terms is correct!\n");
			return 0;
		} else {
			LOG_ERR(nullptr, "ERROR: Testing search by geospatial terms has mistakes.\n");
			return 1;
		}
	} catch (const Xapian::Error &err) {
		LOG_ERR(nullptr, "ERROR: %s\n", err.get_msg().c_str());
		return 1;
	} catch (const std::exception &err) {
		LOG_ERR(nullptr, "ERROR: %s\n", err.what());
		return 1;
	}
}
