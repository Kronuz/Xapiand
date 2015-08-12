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


int geo_test_area()
{
	/*
	 *	The database used in the test is local
	 *	so the Endpoints and local_node are manipulated
	 */

	int exit_success = 4;

	local_node.name.assign("node_test");
	local_node.binary_port = XAPIAND_BINARY_SERVERPORT;

	Endpoints endpoints;
	Endpoint e;
	e.node_name.assign("node_test");
	e.port = XAPIAND_BINARY_SERVERPORT;
	e.path.assign(".db_test1.db");
	e.host.assign("0.0.0.0");
	endpoints.insert(e);

	DatabaseQueue *queue = new DatabaseQueue();
	Database *database = new Database(queue, endpoints, DB_WRITABLE | DB_SPAWN);

	/*
	 *	TEST query Geolocation area
	 *	searching for North Dakota area
	 *	of the four documents indexed it will return 3 that fit into that area
	 *	(Range query)
	 */

	std::stringstream buffer;
	std::ifstream fstream("examples/Json_geo_1.txt");
	buffer << fstream.rdbuf();
	unique_cJSON document1(cJSON_Parse(buffer.str().c_str()), cJSON_Delete);

	if (not database->index(document1.get(), "1", true)) {
		LOG(NULL, "index Json_geo_1 failed\n");
	}

	buffer.str(std::string());
	fstream.close();
	fstream.open("examples/Json_geo_2.txt");
	buffer << fstream.rdbuf();
	unique_cJSON document2(cJSON_Parse(buffer.str().c_str()), cJSON_Delete);

	if (not database->index(document2.get(), "2", true)) {
		LOG(NULL, "index Json_geo_2 failed\n");
	}

	buffer.str(std::string());
	fstream.close();
	fstream.open("examples/Json_geo_3.txt");
	buffer << fstream.rdbuf();
	unique_cJSON document3(cJSON_Parse(buffer.str().c_str()), cJSON_Delete);

	if (not database->index(document3.get(), "3", true)) {
		LOG(NULL, "index Json_geo_3 failed\n");
	}

	buffer.str(std::string());
	fstream.close();
	fstream.open("examples/Json_geo_4.txt");
	buffer << fstream.rdbuf();
	unique_cJSON document4(cJSON_Parse(buffer.str().c_str()), cJSON_Delete);

	if (not database->index(document4.get(), "4", true)) {
		LOG(NULL, "index Json_geo_4 failed\n");
	}

	buffer.str(std::string());
	fstream.close();
	fstream.open("examples/Json_geo_1_2.txt");
	buffer << fstream.rdbuf();
	unique_cJSON document5(cJSON_Parse(buffer.str().c_str()), cJSON_Delete);

	if (not database->index(document5.get(), "1", true)) {
		LOG(NULL, "index Json_geo_1_2 failed\n");
	}

	buffer.str(std::string());
	fstream.close();
	fstream.open("examples/Json_geo_5.txt");
	buffer << fstream.rdbuf();
	unique_cJSON document6(cJSON_Parse(buffer.str().c_str()), cJSON_Delete);

	if (not database->index(document6.get(), "5", true)) {
		LOG(NULL, "index Json_geo_5 failed\n");
	}

	buffer.str(std::string());
	fstream.close();
	fstream.open("examples/Json_geo_6.txt");
	buffer << fstream.rdbuf();
	unique_cJSON document7(cJSON_Parse(buffer.str().c_str()), cJSON_Delete);

	if (not database->index(document7.get(), "6", true)) {
		LOG(NULL, "index Json_geo_6 failed\n");
	}

	buffer.str(std::string());
	fstream.close();
	fstream.open("examples/Json_geo_7.txt");
	buffer << fstream.rdbuf();
	unique_cJSON document8(cJSON_Parse(buffer.str().c_str()), cJSON_Delete);

	if (not database->index(document8.get(), "7", true)) {
		LOG(NULL, "index Json_geo_7 failed\n");
	}

	query_t query_elements;
	query_elements.offset = 0;
	query_elements.limit = 10;
	query_elements.check_at_least = 0;
	query_elements.spelling = false;
	query_elements.synonyms = false;
	query_elements.is_fuzzy = false;
	query_elements.is_nearest = false;
	query_elements.terms.push_back("location:\"..POLYGON ((48.574789910928864 -103.53515625, 48.864714761802794 -97.2509765625, 45.89000815866182 -96.6357421875, 45.89000815866182 -103.974609375, 48.574789910928864 -103.53515625))\"");

	Xapian::MSet mset;
	std::vector<std::string> suggestions;
	std::vector<std::pair<std::string, std::unique_ptr<MultiValueCountMatchSpy>>> spies;

	int rmset = database->get_mset(query_elements, mset, spies, suggestions);
	if (mset.size() == 3) {
		exit_success--;
	} else {
		LOG(NULL, "search area failed, database error\n");
	}

	/*
	 *	TEST query geolocation multi area
	 *	searching for North Dakota and South Dakota area
	 *	of the four documents indexed it will return 5 that fit into that area
	 *	(Range query)
	 */

	query_elements.terms.clear();
	query_elements.terms.push_back("location:\"..MULTIPOLYGON (((48.574789910928864 -103.53515625, 48.864714761802794 -97.2509765625, 45.89000815866182 -96.6357421875, 45.89000815866182 -103.974609375, 48.574789910928864 -103.53515625)), ((45.89000815866182 -103.974609375, 45.89000815866182 -96.6357421875, 42.779275360241904 -96.6796875, 43.03677585761058 -103.9306640625)))\"");

	Xapian::MSet mset2;
	rmset = database->get_mset(query_elements, mset2, spies, suggestions);
	LOG(NULL,"mset2 size %d\n",mset2.size());
	if (mset2.size() == 5) {
		exit_success--;
	} else {
		LOG(NULL, "search multi area failed, database error\n");
	}

	/*
	 *	TEST query Geolocation with a chull location area
	 *	searching for Wyoming area, it will return Utah too because
	 *  it was indexed with convex hull and fit in the Wyoming area
	 *	(Range query)
	 */

	query_elements.terms.clear();
	query_elements.terms.push_back("location:\"..POLYGON ((44.96479793 -111.02783203, 44.96479793 -104.08447266, 41.04621681 -104.08447266, 41.00477542 -111.02783203))\"");
	Xapian::MSet mset3;
	rmset = database->get_mset(query_elements, mset3, spies, suggestions);
	if (mset3.size() == 4) {
		exit_success--;
	} else {
		LOG(NULL, "search area with a chull location failed, database error\n");
	}

	/*
	 *	TEST query Geolocation with a term
	 */

	query_elements.terms.clear();
	query_elements.terms.push_back("attraction_location:\"POINT (44.42789588 -110.58837891)\"");
	Xapian::MSet mset4;
	rmset = database->get_mset(query_elements, mset4, spies, suggestions);
	if (mset4.size() == 1) {
		exit_success--;
	} else {
		LOG(NULL, "search term area location failed, database error\n");
	}

	return exit_success;
}