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


#include "test_query.h"
#include "../src/cJSON.h"
#include "../src/utils.h"
#include "../src/serialise.h"
#include "../src/database.h"
#include "../src/endpoint.h"

#include <sstream>
#include <fstream>


#define NUMERIC_RANGE 1
#define TIMESTAMP_RANGE 2
#define TIMEISO_RANGE 3

#define NUMERIC_QUERY "year:1900..1984"
#define TIMESTAMP_QUERY "released:1968-01-02..1985-07-02"
#define TIMEISO_QUERY "dummy_date:..20150810T10:12:12||/w+2w/M+3M/M-3M+2M/M-2M//M+1w-1d" /*ISO with date math*/

#define NUMERIC_MSN "numeric"
#define TIMESTAMP_MSN "timestamp"
#define TIMEISO_MSN "time iso"

int test_query()
{
	int exit_success = 9;

	/*
	 *	The database used in the test is local
	 *	so the Endpoints and local_node are manipulated
	 */

	local_node.name.assign("node_test");
	local_node.binary_port = XAPIAND_BINARY_SERVERPORT;

	Endpoints endpoints;
	Endpoint e;
	e.node_name.assign("node_test");
	e.port = XAPIAND_BINARY_SERVERPORT;
	e.path.assign(".db_test2.db");
	e.host.assign("0.0.0.0");
	endpoints.insert(e);

	DatabaseQueue *queue = new DatabaseQueue();
	Database *database = new Database(queue,endpoints, DB_WRITABLE|DB_SPAWN);

	std::string data;
	std::string result;
	Xapian::Document _document;
	Xapian::docid docid;

	/* TEST index */

	std::ifstream fstream("examples/Json_example_1.txt");
	std::stringstream buffer;
	buffer << fstream.rdbuf();
	unique_cJSON document(cJSON_Parse(buffer.str().c_str()), cJSON_Delete);
	if (database->index(document.get(), "1", true)) {
		exit_success--;
	} else {
		LOG(NULL, "index Json_example_1 failed\n");
	}


	buffer.str(std::string());
	fstream.close();
	fstream.open("examples/Json_example_2.txt");
	buffer << fstream.rdbuf();
	unique_cJSON document2(cJSON_Parse(buffer.str().c_str()), cJSON_Delete);

	if (not database->index(document2.get(), "2", true)) {
		LOG(NULL, "index Json_example_2 failed\n");
	}

	/* TEST query */

	query_t query_elements;
	query_elements.offset = 0;
	query_elements.limit = 10;
	query_elements.check_at_least = 0;
	query_elements.unique_doc = false;
	query_elements.spelling = false;
	query_elements.synonyms = false;
	query_elements.is_fuzzy = false;
	query_elements.is_nearest = false;
	query_elements.query.push_back("description:American teenager");

	Xapian::MSet mset;
	std::vector<std::string> suggestions;
	std::vector<std::pair<std::string, std::unique_ptr<MultiValueCountMatchSpy>>> spies;

	int rmset = database->get_mset(query_elements, mset, spies, suggestions);
	if (rmset == 0 && !mset.empty()) {
		Xapian::MSetIterator m = mset.begin();
		if (m.get_document().get_data().find("Back to the Future") != std::string::npos) {
			exit_success--;
		} else {
			LOG(NULL, "search query failed, unintended result\n");
		}
	} else {
		LOG(NULL, "search query failed, database error\n");
	}


	/* TEST terms */

	suggestions.clear();
	spies.clear();
	query_elements.query.clear();
	query_elements.terms.push_back("actors__male:Michael J. Fox");

	rmset = database->get_mset(query_elements, mset, spies, suggestions);
	if (rmset == 0 && !mset.empty()) {
		Xapian::MSetIterator m = mset.begin();
		if (m.get_document().get_data().find("Back to the Future") != std::string::npos) {
			exit_success--;
		} else {
			LOG(NULL, "search terms failed, unintended result\n");
		}
	} else {
		LOG(NULL, "search terms failed, database error\n");
	}

	/* TEST partial */

	suggestions.clear();
	spies.clear();
	query_elements.terms.clear();
	query_elements.partial.push_back("directed_by:Rob");

	rmset = database->get_mset(query_elements, mset, spies, suggestions);
	if (rmset == 0 && !mset.empty()) {
		Xapian::MSetIterator m = mset.begin();
		if (m.get_document().get_data().find("Back to the Future") != std::string::npos) {
			exit_success--;
		} else {
			LOG(NULL, "search partial failed, unintended result\n");
		}
	} else {
		LOG(NULL, "search partial failed, database error\n");
	}

	/* TEST facets */

	suggestions.clear();
	spies.clear();
	query_elements.partial.clear();
	query_elements.query.push_back("description:American teenager");
	query_elements.facets.push_back("actors__male");

	rmset = database->get_mset(query_elements, mset, spies, suggestions);
	LOG(NULL, "rmset %d mset %d spies %d\n", rmset, mset.size(), spies.size());
	if (rmset == 0 && !mset.empty() && !spies.empty()) {
		std::vector<std::pair<std::string, std::unique_ptr<MultiValueCountMatchSpy>>>::const_iterator spy(spies.begin());
		Xapian::TermIterator facet = (*spy).second->values_begin();
		data_field_t field_t = database->get_data_field((*spy).first);
		if (Unserialise::unserialise(field_t.type, (*spy).first, *facet).compare("Charlton Heston") == 0) {
			exit_success--;
		} else {
			LOG(NULL, "search facets failed, unintended result\n");
		}
	} else {
		LOG(NULL, "search facets failed, database error\n");
	}


	/*
	 * TEST Fuzzy and nearest (get_similar)
	 * in this test case the documents returned by the fuzzy search
	 * are the same for the nearest search, the difference lies in fuzzy
	 * return results mixed with the query and the input RSet
	 * and nearest only documents related in the input RSet
	 */

	suggestions.clear();
	spies.clear();
	query_elements.query.clear();
	query_elements.facets.clear();
	query_elements.is_fuzzy = true;
	query_elements.fuzzy.n_term = 20;
	query_elements.fuzzy.type.push_back("string"); /* Filter other types except string */
	query_elements.query.push_back("description:future");

	rmset = database->get_mset(query_elements, mset, spies, suggestions);
	LOG(NULL, "rmset %d mset %d\n", rmset, mset.size());
	if (rmset == 0 && !mset.empty()) {
		Xapian::MSetIterator m = mset.begin();
		if (m.get_document().get_data().find("Back to the Future") != std::string::npos) {
			exit_success--;
		} else {
			LOG(NULL, "search similar failed, unintended result\n");
		}
	} else {
		LOG(NULL, "search similar failed, database error\n");
	}

	suggestions.clear();
	spies.clear();
	query_elements.is_fuzzy = false;
	query_elements.fuzzy.type.clear();

	/* TEST Range */

	std::string query_range;
	std::string msn_range;

	for (int i = 1; i <= 3; i++) {

		switch(i) {
			case NUMERIC_RANGE:
				query_range.assign(NUMERIC_QUERY);
				msn_range.assign(NUMERIC_MSN);
				break;

			case TIMESTAMP_RANGE:
				query_range.assign(TIMESTAMP_QUERY);
				msn_range.assign(TIMESTAMP_MSN);
				break;

			case TIMEISO_RANGE:
				query_range.assign(TIMEISO_QUERY);
				msn_range.assign(TIMEISO_MSN);
				break;
		}

		query_elements.query.clear();
		query_elements.query.push_back(query_range);

		rmset = database->get_mset(query_elements, mset, spies, suggestions);
		if (rmset == 0 && !mset.empty()) {
			Xapian::MSetIterator m = mset.begin();
			if (m.get_document().get_data().find("Planet Apes") != std::string::npos) {
				exit_success--;
			} else {
				LOG(NULL, "search %s range failed, unintended result\n",msn_range.c_str());
			}
		} else {
			LOG(NULL, "search %s range failed, database error\n",msn_range.c_str());
		}
	}

	LOG(NULL, "exit success %d\n", exit_success);

	return exit_success;
}