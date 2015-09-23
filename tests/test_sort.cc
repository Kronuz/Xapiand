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

#include "test_sort.h"


static DatabaseQueue *queue = NULL;
static Database *database = NULL;
static std::string name_database(".db_testsort.db");


sort_t string_tests[] {
	/*
	 * Table reference data to verify the ordering
	 * levens(fieldname:value) -> levenshtein_distance(get_value(fieldname), value)
	 * value for sort -> It is the value's field that is selected for the ordering when in the slot
	 *                   there are several values (in arrays).
	 * In arrays, for ascending order we take the smallest value and for descending order we take the largest.
	 *
	 * "_id"	levens(_id:10)	"name"						levens(name:hola)	value for sort (ASC)	value for sort (DESC)
	 * "1"		1				["cook", "cooked"]			[3, 5]				"cook"					"cooked"
	 * "2"		2				["book store", "book"]		[9, 3]				"book"					"book store"
	 * "3"		2				["cooking", "hola mundo"]   [6, 6]				"cooking"				"hola mundo"
	 * "4"		2				"hola"							0				"hola"					"hola"
	 * "5"		2				"mundo"							5				"mundo"					"mundo"
	 * "6"		2				"mundo"							5				"mundo"					"mundo"
	 * "7"		2				"hola"							0				"hola"					"hola"
	 * "8"		2				["cooking", "hola mundo"]	[6, 6]				"cooking"				"hola mundo"
	 * "9"		2				"computer"						7				"computer"				"computer"
	 * "10"		0				Does not have				MAX_DBL				"\xff"					"\xff"
	 *
	 * The documents are indexed as the value of "_id" indicates.
	*/
	{ "*", { "_id" }, 				 { "1", "10", "2", "3", "4", "5", "6", "7", "8", "9" } },
	{ "*", { "-_id" }, 				 { "9", "8", "7", "6", "5", "4", "3", "2", "10", "1" } },
	// { 0, 1, 2, 2, 2, 2, 2, 2, 2, 2 }
	{ "*", { "_id:10" }, 			 { "10", "1", "2", "3", "4", "5", "6", "7", "8", "9" } },
	// { 2, 2, 2, 2, 2, 2, 2, 2, 1, 0 }
	{ "*", { "-_id:10" }, 			 { "2", "3", "4", "5", "6", "7", "8", "9", "1", "10" } },
	// { "book", "computer", "cook", "cooking", "cooking", "hola", "hola", "mundo", "mundo", "\xff" }
	{ "*", { "name" }, 			     { "2", "9", "1", "3", "8", "4", "7", "5", "6", "10" } },
	// { "\xff", "mundo", "mundo", "hola mundo", "hola mundo", "hola", "hola", "cooked", "computer", "book store" }
	{ "*", { "-name" }, 			 { "10", "5", "6", "3", "8", "4", "7", "1", "9", "2" } },
	// { 0, 0, 3, 3, 5, 5, 6, 6, 7, MAX_DBL }
	{ "*", { "name:hola" }, 		 { "4", "7", "1", "2", "5", "6", "3", "8", "9", "10" } },
	{ "*", { "name:hola", "-_id" },  { "7", "4", "2", "1", "6", "5", "8", "3", "9", "10" } },
	// { MAX_DBL, 9, 7, 6, 6, 5, 5, 5, 0, 0 }
	{ "*", { "-name:hola" }, 		 { "10", "2", "9", "3", "8", "1", "5", "6", "4", "7" } },
	{ "*", { "-name:hola", "-_id" }, { "10", "2", "9", "8", "3", "6", "5", "1", "7", "4" } }
};


sort_t numerical_tests[] {
	/*
	 * Table reference data to verify the ordering
	 * dist(fieldname:value) -> abs(Xapian::sortable_unserialise(get_value(fieldname)) - value)
	 * value for sort -> It is the value's field that is selected for the ordering when in the slot
	 *                   there are several values (in array).
	 * In arrays, for ascending order we take the smallest value and for descending order we take the largest.
	 *
	 * "_id"	"year"			dist(year:1000)	dist(year:2000)	value for sort (ASC)	value for sort (DESC)
	 * "1"		[2010, 2015]	[1010, 1015]	[10, 15]		2010					2015
	 * "2"		[2000, 2001]	[1000, 1001]	[0, 1]			2000					2001
	 * "3"		[-10000, 0]   	[11000, 1000]	[12000, 2000]	-10000					0
	 * "4"		100				900				1900			100						100
	 * "5"		500				500				1500			500						500
	 * "6"		400				600				1600			400						400
	 * "7"		100				900				1900			100						100
	 * "8"		[-10000, 0]		[11000, 1000]	[12000, 2000]	-10000					0
	 * "9"		[2000, 2001]	[1000, 1001]	[0, 1]			2000					2001
	 * "10"		2020			1020			20				2020					2020
	 *
	 * The documents are indexed as the value of "_id" indicates.
	*/
	// { -10000, -10000, 100, 100, 400, 500, 2000, 2000, 2010, 2020 }
	{ "*", { "year" }, 				 { "3", "8", "4", "7", "6", "5", "2", "9", "1", "10" } },
	// { 2020, 2015, 2001, 2001, 500, 400, 100, 100, 0, 0 }
	{ "*", { "-year" },				 { "10", "1", "2", "9", "5", "6", "4", "7", "3", "8" } },
	// { 500, 600, 900, 900, 1000, 1000, 1000, 1000, 1010, 1020  }
	{ "*", { "year:1000" }, 		 { "5", "6", "4", "7", "2", "3", "8", "9", "1", "10" } },
	// { 11000, 11000, 1020, 1015, 1001, 1001, 900, 900, 600, 500 }
	{ "*", { "-year:1000" }, 		 { "3", "8", "10", "1", "2", "9", "4", "7", "6", "5" } },
	// { 0, 0, 10, 20, 1500, 1600, 1900, 1900, 2000, 2000 }
	{ "*", { "year:2000" }, 		 { "2", "9", "1", "10", "5", "6", "4", "7", "3", "8" } },
	{ "*", { "year:2000", "-_id" },  { "9", "2", "1", "10", "5", "6", "7", "4", "8", "3" } },
	// { 12000, 12000, 1900, 1900, 1600, 1500, 1100, 1100, 20, 10, 1, 1  }
	{ "*", { "-year:2000" },		 { "3", "8", "4", "7", "6", "5", "10", "1", "2", "9" } },
	{ "*", { "-year:2000", "-_id" }, { "8", "3", "7", "4", "6", "5", "10", "1", "9", "2" } }
};


sort_t date_tests[] {
	/*
	 * Table reference data to verify the ordering.
	 * dist(fieldname:value) -> abs(Xapian::sortable_unserialise(get_value(fieldname)) - Datetime::timestamp(value))
	 * value for sort -> It is the value's field that is selected for the ordering when in the slot
	 *                   there are several values (in array).
	 * In arrays, for ascending order we take the smallest value and for descending order we take the largest.
	 *
	 * "_id"	"date"								dist(date:2010-01-01)		dist(date:0001-01-01)
	 *												Epoch: 1262304000	    	Epoch: -62135596800
	 * "1"		["2010-10-21", "2011-01-01"],		[25315200, 31536000]		[63423216000, 63429436800]
	 *			Epoch: [1287619200, 1293840000]
	 * "2"		["1810-01-01", "1910-01-01"],		[6311433600, 3155760000]	[57086467200, 60242140800]
	 *			Epoch: [-5049129600, -1893456000]
	 * "3"		["0010-01-01", "0020-01-01"],		[63113904000, 62798371200]	[283996800, 599529600]
	 *			Epoch: [-61851600000, -61536067200]
	 * "4"		"0001-01-01",						63397900800					0
	 *			Epoch: -62135596800
	 * "5"		"2015-01-01",						157766400					63555667200
	 *			Epoch: 1420070400
	 * "6"		"2015-01-01",						157766400					63555667200
	 *			Epoch: 1420070400
	 * "7"		"0300-01-01",						53962416000					9435484800
	 *			Epoch: -52700112000
	 * "8"		["0010-01-01", "0020-01-01"],		[63113904000, 62798371200]	[283996800, 599529600]
	 *			Epoch: [-61851600000, -61536067200]
	 * "9"		["1810-01-01", "1910-01-01"],		[6311433600, 3155760000]	[57086467200, 60242140800]
	 *			Epoch: [-5049129600, -1893456000]
	 * "10"		["2010-10-21", "2011-01-01"],		[25315200, 31536000]		[63423216000, 63429436800]
	 *			Epoch: [1287619200, 1293840000]
	 *
	 * The documents are indexed as the value of "_id" indicates.
	*/
	// { "0001-01-01", "0010-01-01", "0010-01-01", "0300-01-01", "1810-01-01", "1810-01-01", "2010-10-21", "2010-10-21", "2015-01-01", "2015-01-01" }
	{ "*", { "date" }, 						{ "4", "3", "8", "7", "2", "9", "1", "10", "5", "6" } },
	// { "2015-01-01", "2015-01-01", "2011-01-01", "2011-01-01", "1910-01-01", "1910-01-01", "0300-01-01", "0020-01-01", "0020-01-01", "0001-01-01" }
	{ "*", { "-date" },						{ "5", "6", "1", "10", "2", "9", "7", "3", "8", "4" } },
	// { 25315200, 25315200, 157766400, 157766400, 3155760000, 3155760000, 53962416000, 62798371200, 62798371200, 63397900800}
	{ "*", { "date:2010-01-01" }, 			{ "1", "10", "5", "6", "2", "9", "7", "3", "8", "4" } },
	{ "*", { "date:20100101 00:00:00" }, 	{ "1", "10", "5", "6", "2", "9", "7", "3", "8", "4" } },
	{ "*", { "date:1262304000" }, 			{ "1", "10", "5", "6", "2", "9", "7", "3", "8", "4" } },
	// { 63397900800, 63113904000, 63113904000, 53962416000, 6311433600, 6311433600, 157766400, 157766400, 31536000, 31536000}
	{ "*", { "-date:2010-01-01" }, 			{ "4", "3", "8", "7", "2", "9", "5", "6", "1", "10" } },
	// { 0, 283996800, 283996800, 9435484800, 57086467200, 57086467200, 63423216000, 63423216000, 63555667200, 63555667200 }
	{ "*", { "date:0001-01-01" }, 			{ "4", "3", "8", "7", "2", "9", "1", "10", "5", "6" } },
	{ "*", { "date:00010101 00:00:00" }, 	{ "4", "3", "8", "7", "2", "9", "1", "10", "5", "6" } },
	{ "*", { "date:-62135596800" }, 		{ "4", "3", "8", "7", "2", "9", "1", "10", "5", "6" } },
	{ "*", { "date:0001-01-01", "-_id" },	{ "4", "8", "3", "7", "9", "2", "10", "1", "6", "5" } },
	// { 63555667200, 63555667200, 63429436800, 63429436800, 60242140800, 60242140800, 9435484800, 599529600, 599529600, 0 }
	{ "*", { "-date:0001-01-01" },			{ "5", "6", "1", "10", "2", "9", "7", "3", "8", "4" } },
	{ "*", { "-date:0001-01-01", "-_id" },	{ "6", "5", "10", "1", "9", "2", "7", "8", "3", "4" } }
};


sort_t boolean_tests[] {
	/*
	 * Table reference data to verify the ordering
	 * dist(fieldname:value) -> get_value(fieldname) == value ? 0 : 1
	 * value for sort -> It is the value's field that is selected for the ordering when in the slot
	 *                   there are several values (in arrays).
	 * In arrays, for ascending order we take the smallest value and for descending order we take the largest.
	 *
	 * "_id"	"there"				dist(there:false)	dist(there:true)	value for sort (ASC)	value for sort (DESC)
	 * "1"		[true, false], 		[1, 0]				[0, 1]				false					true
	 * "2"		[false, false], 	[0, 0]				[1, 1]				false					false
	 * "3"		[true, true], 		[1, 1]				[0, 0]				true					true
	 * "4"		true, 					1					0				true					true
	 * "5"		false, 					0					1				false					false
	 * "6"		false, 					0					1				false					false
	 * "7"		true, 					1					0				true					true
	 * "8"		[true, true], 		[1, 1]				[0, 0]				true					true
	 * "9"		[false, false] 		[0, 0]				[1, 1]				false					false
	 * "10"		[true, false], 		[1, 0]				[0, 1]				false					true
	 *
	 * The documents are indexed as the value of "_id" indicates.
	*/
	// { false, false, false, false, false, true, true, true, true, true }
	{ "*", { "there" }, 			 	{ "1", "2", "5", "6", "9", "10", "3", "4", "7", "8" } },
	// { true, true, true, true, true, true, false, false, false, false }
	{ "*", { "-there" }, 			 	{ "1", "3", "4", "7", "8", "10", "2", "5", "6", "9" } },
	// { 0, 0, 0, 0, 0, 0, 1, 1, 1, 1 }
	{ "*", { "there:true" }, 		 	{ "1", "3", "4", "7", "8", "10", "2", "5", "6", "9" } },
	// { 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 }
	{ "*", { "-there:true" }, 		 	{ "1", "2", "5", "6", "9", "10", "3", "4", "7", "8" } },
	// { 0, 0, 0, 0, 0, 0, 1, 1, 1, 1 }
	{ "*", { "there:false" }, 		 	{ "1", "2", "5", "6", "9", "10", "3", "4", "7", "8" } },
	// { 1, 1, 1, 1, 1, 1, 0, 0, 0, 0 }
	{ "*", { "-there:false" }, 			{ "1", "3", "4", "7", "8", "10", "2", "5", "6", "9" } },
	{ "*", { "-there:false", "-_id" },	{ "8", "7", "4", "3", "10", "1", "9", "6", "5", "2" } },
};


sort_t geo_tests[] {
	/*
	 * Table reference data to verify the ordering
	 * radius(fieldname:value) -> Angle between centroids of value and centroids saved in the slot.
	 * value for sort -> It is the value's field that is selected for the ordering when in the slot
	 *                   there are several values (in array).
	 * In arrays, for ascending order we take the smallest value and for descending order we take the largest.
	 *
	 * "_id"	"location"							radius(location:POINT(5 5))	radius(location:CIRCLE(10 10,200000))
	 * "1"		["POINT(10 21)", "POINT(10 20)"]	[0.290050, 0.273593]		[0.189099, 0.171909]
	 * "2"		["POINT(20 40)", "POINT(50 60)"]	[0.648657, 1.120883]		[0.533803, 0.999915]
	 * "3"		["POINT(0 0)", "POINT(0 70)"]		[0.122925, 1.136214]		[0.245395, 1.055833]
	 * "4"		"CIRCLE(2 2, 2000)"					0.073730					0.196201
	 * "5"		"CIRCLE(10 10, 2000)"				0.122473					0.000036
	 * "6"		"CIRCLE(10 10, 2000)"				0.122473					0.000036
	 * "7"		"CIRCLE(2 2, 2000)"					0.073730					0.196201
	 * "8"		"POINT(3.2 10.1)"					0.094108					0.117923
	 * "9"		["POINT(20 40)", "POINT(50 60)"]	[0.648657, 1.120883]		[0.533803, 0.999915]
	 * "10"		["POINT(10 21)", "POINT(10 20)"]	[0.290050, 0.273593]		[0.189099, 0.171909]
	 *
	 * The documents are indexed as the value of "_id" indicates.
	*/
	// It does not have effect in the results.
	{ "*", { "location" }, 			 	{ "1", "2", "3", "4", "5", "6", "7", "8", "9", "10" } },
	// It does not have effect in the results.
	{ "*", { "-location" },			 	{ "1", "2", "3", "4", "5", "6", "7", "8", "9", "10" } },
	// { 0.073730, 0.073730, 0.094108, 0.122473, 0.122473, 0.122925, 0.273593, 0.273593, 0.648657, 0.648657 }
	{ "*", { "location:POINT(5 5)" }, 	{ "4", "7", "8", "5", "6", "3", "1", "10", "2", "9" } },
	// { 1.136214, 1.120883, 1.120883, 0.290050, 0.290050, 0.122473, 0.122473, 0.094108, 0.073730, 0.073730 }
	{ "*", { "-location:POINT(5 5)" }, 	{ "3", "2", "9", "1", "10", "5", "6", "8", "4", "7" } },
	// { 0.000036, 0.000036, 0.117923, 0.171909, 0.171909, 0.196201, 0.196201, 0.245395, 0.533803, 0.533803 }
	{ "*", { "location:CIRCLE(10 10,200000)" },			 { "5", "6", "8", "1", "10", "4", "7", "3", "2", "9" } },
	{ "*", { "location:CIRCLE(10 10,200000)", "-_id" },	 { "6", "5", "8", "10", "1", "7", "4", "3", "9", "2" } },
	// { 1.055833, 0.999915, 0.999915, 0.196201, 0.196201, 0.189099, 0.189099,  0.117923, 0.000036, 0.000036 }
	{ "*", { "-location:CIRCLE(10 10,200000)" },		 { "3", "2", "9", "4", "7", "1", "10", "8", "5", "6" } },
	{ "*", { "-location:CIRCLE(10 10,200000)", "-_id" }, { "3", "9", "2", "7", "4", "10", "1", "8", "6", "5" } }
};


int create_test_db()
{
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
	queue = new DatabaseQueue();
	database = new Database(queue, endpoints, DB_WRITABLE | DB_SPAWN);

	std::vector<std::string> _docs({
		"examples/sort/doc1.txt",
		"examples/sort/doc2.txt",
		"examples/sort/doc3.txt",
		"examples/sort/doc4.txt",
		"examples/sort/doc5.txt",
		"examples/sort/doc6.txt",
		"examples/sort/doc7.txt",
		"examples/sort/doc8.txt",
		"examples/sort/doc9.txt",
		"examples/sort/doc10.txt"
	});

	// Index documents in the database.
	size_t i = 1;
	for (std::vector<std::string>::iterator it(_docs.begin()); it != _docs.end(); it++) {
		std::ifstream fstream(*it);
		std::stringstream buffer;
		buffer << fstream.rdbuf();
		unique_cJSON document(cJSON_Parse(buffer.str().c_str()), cJSON_Delete);
		if (not database->index(document.get(), std::to_string(i), true)) {
			cont++;
			LOG_ERR(NULL, "ERROR: File %s can not index\n", it->c_str());
		}
		fstream.close();
		++i;
	}

	return cont;
}


int make_search(const sort_t _tests[], int len)
{
	int cont = 0;
	query_t query;
	query.offset = 0;
	query.limit = 10;
	query.check_at_least = 0;
	query.spelling = false;
	query.synonyms = false;
	query.is_fuzzy = false;
	query.is_nearest = false;

	for (int i = 0; i < len; ++i) {
		sort_t p = _tests[i];
		query.query.clear();
		query.sort.clear();
		query.query.push_back(p.query);

		std::vector<std::string>::iterator it(p.sort.begin());
		for ( ; it != p.sort.end(); it++) {
			query.sort.push_back(*it);
		}

		Xapian::MSet mset;
		std::vector<std::string> suggestions;
		std::vector<std::pair<std::string, std::unique_ptr<MultiValueCountMatchSpy>>> spies;

		int rmset = database->get_mset(query, mset, spies, suggestions);
		if (rmset != 0) {
			cont++;
			LOG_ERR(NULL, "ERROR: Failed in get_mset\n");
		} else if (mset.size() != p.expect_result.size()) {
			cont++;
			LOG_ERR(NULL, "ERROR: Different number of documents obtained\n");
		} else {
			it = p.expect_result.begin();
			Xapian::MSetIterator m = mset.begin();
			for ( ; m != mset.end(); ++it, ++m) {
				std::string d_id(m.get_document().get_value(0));
				if (it->compare(d_id) != 0) {
					cont++;
					LOG_ERR(NULL, "ERROR: Result = %s:%s   Expected = %s:%s\n", RESERVED_ID, d_id.c_str(), RESERVED_ID, it->c_str());
				}
			}
		}
	}

	// Delete de database and release memory.
	delete_files(name_database);
	delete database;

	return cont;
}


int sort_test_string()
{
	try {
		int cont = create_test_db();
		if (cont == 0 && make_search(string_tests, sizeof(string_tests) / sizeof(string_tests[0])) == 0) {
			LOG(NULL, "Testing sort strings is correct!\n");
			return 0;
		} else {
			LOG_ERR(NULL, "ERROR: Testing sort strings has mistakes.\n");
			return 1;
		}
	} catch (const Xapian::Error &err) {
		LOG_ERR(NULL, "ERROR: %s\n", err.get_msg().c_str());
		return 1;
	} catch (const std::exception &err) {
		LOG_ERR(NULL, "ERROR: %s\n", err.what());
		return 1;
	}
}


int sort_test_numerical()
{
	try {
		int cont = create_test_db();
		if (cont == 0 && make_search(numerical_tests, sizeof(numerical_tests) / sizeof(numerical_tests[0])) == 0) {
			LOG(NULL, "Testing sort numbers is correct!\n");
			return 0;
		} else {
			LOG_ERR(NULL, "ERROR: Testing sort numbers has mistakes.\n");
			return 1;
		}
	} catch (const Xapian::Error &err) {
		LOG_ERR(NULL, "ERROR: %s\n", err.get_msg().c_str());
		return 1;
	} catch (const std::exception &err) {
		LOG_ERR(NULL, "ERROR: %s\n", err.what());
		return 1;
	}
}


int sort_test_date()
{
	try {
		int cont = create_test_db();
		if (cont == 0 && make_search(date_tests, sizeof(date_tests) / sizeof(date_tests[0])) == 0) {
			LOG(NULL, "Testing sort dates is correct!\n");
			return 0;
		} else {
			LOG_ERR(NULL, "ERROR: Testing sort dates has mistakes.\n");
			return 1;
		}
	} catch (const Xapian::Error &err) {
		LOG_ERR(NULL, "ERROR: %s\n", err.get_msg().c_str());
		return 1;
	} catch (const std::exception &err) {
		LOG_ERR(NULL, "ERROR: %s\n", err.what());
		return 1;
	}
}


int sort_test_boolean()
{
	try {
		int cont = create_test_db();
		if (cont == 0 && make_search(boolean_tests, sizeof(boolean_tests) / sizeof(boolean_tests[0])) == 0) {
			LOG(NULL, "Testing sort booleans is correct!\n");
			return 0;
		} else {
			LOG_ERR(NULL, "ERROR: Testing sort booleans has mistakes.\n");
			return 1;
		}
	} catch (const Xapian::Error &err) {
		LOG_ERR(NULL, "ERROR: %s\n", err.get_msg().c_str());
		return 1;
	} catch (const std::exception &err) {
		LOG_ERR(NULL, "ERROR: %s\n", err.what());
		return 1;
	}
}


int sort_test_geo()
{
	try {
		int cont = create_test_db();
		if (cont == 0 && make_search(geo_tests, sizeof(geo_tests) / sizeof(geo_tests[0])) == 0) {
			LOG(NULL, "Testing sort geospatials is correct!\n");
			return 0;
		} else {
			LOG_ERR(NULL, "ERROR: Testing sort geospatials has mistakes.\n");
			return 1;
		}
	} catch (const Xapian::Error &err) {
		LOG_ERR(NULL, "ERROR: %s\n", err.get_msg().c_str());
		return 1;
	} catch (const std::exception &err) {
		LOG_ERR(NULL, "ERROR: %s\n", err.what());
		return 1;
	}
}