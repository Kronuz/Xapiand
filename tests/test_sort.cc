/*
 * Copyright (C) 2015,2016 deipi.com LLC and contributors. All rights reserved.
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

#include "utils.h"


const std::string path_test_sort = std::string(PACKAGE_PATH_TEST) + "/examples/sort/";


const sort_t string_levens_tests[] {
	/*
	 * Table reference data to verify the ordering
	 * levens(fieldname:value) -> levenshtein_distance(get_value(fieldname), value)
	 * value for sort -> It is the value's field that is selected for the ordering when in the slot
	 *                   there are several values (in arrays).
	 * In arrays, for ascending order we take the smallest value and for descending order we take the largest.
	 *
	 * "_id"	"name"						levens(name:cook)		value for sort (ASC)	value for sort (DESC)
	 * "1"		["cook", "cooked"]			[0.000000, 0.333333]	"cook"					"cooked"
	 * "2"		["book store", "book"]		[0.700000, 0.250000]	"book"					"book store"
	 * "3"		["cooking", "hola mundo"]   [0.428571, 0.900000]	"cooking"				"hola mundo"
	 * "4"		"hola"						0.750000				"hola"					"hola"
	 * "5"		"mundo"						1.000000				"mundo"					"mundo"
	 * "6"		"mundo"						1.000000				"mundo"					"mundo"
	 * "7"		"hola"						0.750000				"hola"					"hola"
	 * "8"		["cooking", "hola mundo"]	[0.428571, 0.900000]	"cooking"				"hola mundo"
	 * "9"		"computer"					0.750000				"computer"				"computer"
	 * "10"		Does not have				MAX_DBL					"\xff"					"\xff"
	 *
	 * The documents are indexed as the value of "_id" indicates.
	*/
	// { "book", "computer", "cook", "cooking", "cooking", "hola", "hola", "mundo", "mundo", "\xff" }
	{ "*", { "name" }, 			     { "2", "9", "1", "3", "8", "4", "7", "5", "6", "10" } },
	// { "\xff", "mundo", "mundo", "hola mundo", "hola mundo", "hola", "hola", "cooked", "computer", "book store" }
	{ "*", { "-name" }, 			 { "10", "5", "6", "3", "8", "4", "7", "1", "9", "2" } },
	// { 0, 0.250000, 0.428571, 0.428571, 0.750000, 0.750000, 0.750000, 1, 1, MAX_DBL }
	{ "*", { "name:cook" }, 		 { "1", "2", "3", "8", "4", "7", "9", "5", "6", "10" } },
	{ "*", { "name:cook", "-_id" },  { "1", "2", "8", "3", "9", "7", "4", "6", "5", "10" } },
	// { MAX_DBL, 1, 1, 0.900000, 0.900000, 0.750000, 0.750000, 0.750000, 0.700000, 0.333333 }
	{ "*", { "-name:cook" }, 		 { "10", "5", "6", "3", "8", "4", "7", "9", "2", "1" } },
	{ "*", { "-name:cook", "-_id" }, { "10", "6", "5", "8", "3", "9", "7", "4", "2", "1" } }
};


const sort_t string_jaro_tests[] {
	/*
	 * Table reference data to verify the ordering
	 * jaro(fieldname:value) -> jaro(get_value(fieldname), value)
	 * value for sort -> It is the value's field that is selected for the ordering when in the slot
	 *                   there are several values (in arrays).
	 * In arrays, for ascending order we take the smallest value and for descending order we take the largest.
	 *
	 * "_id"	"name"						jaro(name:cook)			value for sort (ASC)	value for sort (DESC)
	 * "1"		["cook", "cooked"]			[0.000000, 0.111111]	"cook"					"cooked"
	 * "2"		["book store", "book"]		[0.316667, 0.166667]	"book"					"book store"
	 * "3"		["cooking", "hola mundo"]   [0.142857, 0.550000]	"cooking"				"hola mundo"
	 * "4"		"hola"						0.500000				"hola"					"hola"
	 * "5"		"mundo"						1.000000				"mundo"					"mundo"
	 * "6"		"mundo"						1.000000				"mundo"					"mundo"
	 * "7"		"hola"						0.500000				"hola"					"hola"
	 * "8"		["cooking", "hola mundo"]	[0.142857, 0.550000]	"cooking"				"hola mundo"
	 * "9"		"computer"					0.416667				"computer"				"computer"
	 * "10"		Does not have				MAX_DBL					"\xff"					"\xff"
	 *
	 * The documents are indexed as the value of "_id" indicates.
	*/
	// { "book", "computer", "cook", "cooking", "cooking", "hola", "hola", "mundo", "mundo", "\xff" }
	{ "*", { "name" }, 			     { "2", "9", "1", "3", "8", "4", "7", "5", "6", "10" } },
	// { "\xff", "mundo", "mundo", "hola mundo", "hola mundo", "hola", "hola", "cooked", "computer", "book store" }
	{ "*", { "-name" }, 			 { "10", "5", "6", "3", "8", "4", "7", "1", "9", "2" } },
	// { 0, 0.142857, 0.142857, 0.166667, 0.416667, 0.500000, 0.500000, 1, 1, MAX_DBL }
	{ "*", { "name:cook" }, 		 { "1", "3", "8", "2", "9", "4", "7", "5", "6", "10" } },
	{ "*", { "name:cook", "-_id" },  { "1", "8", "3", "2", "9", "7", "4", "6", "5", "10" } },
	// { MAX_DBL, 1, 1, 0.550000, 0.550000, 0.500000, 0.500000, 0.416667, 0.316667, 0.111111 }
	{ "*", { "-name:cook" }, 		 { "10", "5", "6", "3", "8", "4", "7", "9", "2", "1" } },
	{ "*", { "-name:cook", "-_id" }, { "10", "6", "5", "8", "3", "7", "4", "9", "2", "1" } }
};


const sort_t string_jaro_w_tests[] {
	/*
	 * Table reference data to verify the ordering
	 * jaro_winkler(fieldname:value) -> jaro_winkler(get_value(fieldname), value)
	 * value for sort -> It is the value's field that is selected for the ordering when in the slot
	 *                   there are several values (in arrays).
	 * In arrays, for ascending order we take the smallest value and for descending order we take the largest.
	 *
	 * "_id"	"name"						jaro_winkler(name:cook)	value for sort (ASC)	value for sort (DESC)
	 * "1"		["cook", "cooked"]			[0.000000, 0.066667]	"cook"					"cooked"
	 * "2"		["book store", "book"]		[0.316667, 0.166667]	"book"					"book store"
	 * "3"		["cooking", "hola mundo"]   [0.085714, 0.550000]	"cooking"				"hola mundo"
	 * "4"		"hola"						0.500000				"hola"					"hola"
	 * "5"		"mundo"						1.000000				"mundo"					"mundo"
	 * "6"		"mundo"						1.000000				"mundo"					"mundo"
	 * "7"		"hola"						0.500000				"hola"					"hola"
	 * "8"		["cooking", "hola mundo"]	[0.085714, 0.550000]	"cooking"				"hola mundo"
	 * "9"		"computer"					0.416667				"computer"				"computer"
	 * "10"		Does not have				MAX_DBL					"\xff"					"\xff"
	 *
	 * The documents are indexed as the value of "_id" indicates.
	*/
	// { "book", "computer", "cook", "cooking", "cooking", "hola", "hola", "mundo", "mundo", "\xff" }
	{ "*", { "name" }, 			     { "2", "9", "1", "3", "8", "4", "7", "5", "6", "10" } },
	// { "\xff", "mundo", "mundo", "hola mundo", "hola mundo", "hola", "hola", "cooked", "computer", "book store" }
	{ "*", { "-name" }, 			 { "10", "5", "6", "3", "8", "4", "7", "1", "9", "2" } },
	// { 0, 0.085714, 0.085714, 0.166667, 0.416667, 0.500000, 0.500000, 1, 1, MAX_DBL }
	{ "*", { "name:cook" }, 		 { "1", "3", "8", "2", "9", "4", "7", "5", "6", "10" } },
	{ "*", { "name:cook", "-_id" },  { "1", "8", "3", "2", "9", "7", "4", "6", "5", "10" } },
	// { MAX_DBL, 1, 1, 0.550000, 0.550000, 0.500000, 0.500000, 0.416667, 0.316667, 0.066667 }
	{ "*", { "-name:cook" }, 		 { "10", "5", "6", "3", "8", "4", "7", "9", "2", "1" } },
	{ "*", { "-name:cook", "-_id" }, { "10", "6", "5", "8", "3", "7", "4", "9", "2", "1" } }
};


const sort_t string_dice_tests[] {
	/*
	 * Table reference data to verify the ordering
	 * dice(fieldname:value) -> sorensen_dice(get_value(fieldname), value)
	 * value for sort -> It is the value's field that is selected for the ordering when in the slot
	 *                   there are several values (in arrays).
	 * In arrays, for ascending order we take the smallest value and for descending order we take the largest.
	 *
	 * "_id"	"name"						dice(name:cook)			value for sort (ASC)	value for sort (DESC)
	 * "1"		["cook", "cooked"]			[0.000000, 0.250000]	"cook"					"cooked"
	 * "2"		["book store", "book"]		[0.666667, 0.333333]	"book"					"book store"
	 * "3"		["cooking", "hola mundo"]   [0.333333, 1.000000]	"cooking"				"hola mundo"
	 * "4"		"hola"						1.000000				"hola"					"hola"
	 * "5"		"mundo"						1.000000				"mundo"					"mundo"
	 * "6"		"mundo"						1.000000				"mundo"					"mundo"
	 * "7"		"hola"						1.000000				"hola"					"hola"
	 * "8"		["cooking", "hola mundo"]	[0.333333, 1.000000]	"cooking"				"hola mundo"
	 * "9"		"computer"					0.800000				"computer"				"computer"
	 * "10"		Does not have				MAX_DBL					"\xff"					"\xff"
	 *
	 * The documents are indexed as the value of "_id" indicates.
	*/
	// { "book", "computer", "cook", "cooking", "cooking", "hola", "hola", "mundo", "mundo", "\xff" }
	{ "*", { "name" }, 			     { "2", "9", "1", "3", "8", "4", "7", "5", "6", "10" } },
	// { "\xff", "mundo", "mundo", "hola mundo", "hola mundo", "hola", "hola", "cooked", "computer", "book store" }
	{ "*", { "-name" }, 			 { "10", "5", "6", "3", "8", "4", "7", "1", "9", "2" } },
	// { 0, 0.333333, 0.333333, 0.333333, 0.800000, 1, 1, 1, 1, MAX_DBL }
	{ "*", { "name:cook" }, 		 { "1", "2", "3", "8", "9", "4", "5", "6", "7", "10" } },
	{ "*", { "name:cook", "-_id" },  { "1", "8", "3", "2", "9", "7", "6", "5", "4", "10" } },
	// { MAX_DBL, 1, 1, 1, 1, 1, 1, 0.800000, 0.666667, 0.250000 }
	{ "*", { "-name:cook" }, 		 { "10", "3", "4", "5", "6", "7", "8", "9", "2", "1" } },
	{ "*", { "-name:cook", "-_id" }, { "10", "8", "7", "6", "5", "4", "3", "9", "2", "1" } }
};


const sort_t string_jaccard_tests[] {
	/*
	 * Table reference data to verify the ordering
	 * jaccard(fieldname:value) -> jaccard(get_value(fieldname), value)
	 * value for sort -> It is the value's field that is selected for the ordering when in the slot
	 *                   there are several values (in arrays).
	 * In arrays, for ascending order we take the smallest value and for descending order we take the largest.
	 *
	 * "_id"	"name"						jaccard(name:cook)			value for sort (ASC)	value for sort (DESC)
	 * "1"		["cook", "cooked"]			[0.000000, 0.400000]	"cook"					"cooked"
	 * "2"		["book store", "book"]		[0.777778, 0.500000]	"book"					"book store"
	 * "3"		["cooking", "hola mundo"]   [0.500000, 0.909091]	"cooking"				"hola mundo"
	 * "4"		"hola"						0.833333				"hola"					"hola"
	 * "5"		"mundo"						0.857143				"mundo"					"mundo"
	 * "6"		"mundo"						0.857143				"mundo"					"mundo"
	 * "7"		"hola"						0.833333				"hola"					"hola"
	 * "8"		["cooking", "hola mundo"]	[0.500000, 0.909091]	"cooking"				"hola mundo"
	 * "9"		"computer"					0.777778				"computer"				"computer"
	 * "10"		Does not have				MAX_DBL					"\xff"					"\xff"
	 *
	 * The documents are indexed as the value of "_id" indicates.
	*/
	// { "book", "computer", "cook", "cooking", "cooking", "hola", "hola", "mundo", "mundo", "\xff" }
	{ "*", { "name" }, 			     { "2", "9", "1", "3", "8", "4", "7", "5", "6", "10" } },
	// { "\xff", "mundo", "mundo", "hola mundo", "hola mundo", "hola", "hola", "cooked", "computer", "book store" }
	{ "*", { "-name" }, 			 { "10", "5", "6", "3", "8", "4", "7", "1", "9", "2" } },
	// { 0, 0.500000, 0.500000, 0.500000, 0.777778, 0.833333, 0.833333, 0.857143, 0.857143, MAX_DBL }
	{ "*", { "name:cook" }, 		 { "1", "2", "3", "8", "9", "4", "7", "5", "6", "10" } },
	{ "*", { "name:cook", "-_id" },  { "1", "8", "3", "2", "9", "7", "4", "6", "5", "10" } },
	// { MAX_DBL, 0.909091, 0.909091, 0.857143, 0.857143, 0.833333, 0.833333, 0.777778, 0.777778, 0.400000 }
	{ "*", { "-name:cook" }, 		 { "10", "3", "8", "5", "6", "4", "7", "2", "9", "1" } },
	{ "*", { "-name:cook", "-_id" }, { "10", "8", "3", "6", "5", "7", "4", "9", "2", "1" } }
};


const sort_t string_lcs_tests[] {
	/*
	 * Table reference data to verify the ordering
	 * lcs(fieldname:value) -> lcs(get_value(fieldname), value)
	 * value for sort -> It is the value's field that is selected for the ordering when in the slot
	 *                   there are several values (in arrays).
	 * In arrays, for ascending order we take the smallest value and for descending order we take the largest.
	 *
	 * "_id"	"name"						lcs(name:cook)			value for sort (ASC)	value for sort (DESC)
	 * "1"		["cook", "cooked"]			[0.000000, 0.333333]	"cook"					"cooked"
	 * "2"		["book store", "book"]		[0.700000, 0.250000]	"book"					"book store"
	 * "3"		["cooking", "hola mundo"]   [0.428571, 0.900000]	"cooking"				"hola mundo"
	 * "4"		"hola"						0.750000				"hola"					"hola"
	 * "5"		"mundo"						0.800000				"mundo"					"mundo"
	 * "6"		"mundo"						0.800000				"mundo"					"mundo"
	 * "7"		"hola"						0.750000				"hola"					"hola"
	 * "8"		["cooking", "hola mundo"]	[0.42857, 0.900000]	    "cooking"				"hola mundo"
	 * "9"		"computer"					0.750000				"computer"				"computer"
	 * "10"		Does not have				MAX_DBL					"\xff"					"\xff"
	 *
	 * The documents are indexed as the value of "_id" indicates.
	*/
	// { "book", "computer", "cook", "cooking", "cooking", "hola", "hola", "mundo", "mundo", "\xff" }
	{ "*", { "name" }, 			     { "2", "9", "1", "3", "8", "4", "7", "5", "6", "10" } },
	// { "\xff", "mundo", "mundo", "hola mundo", "hola mundo", "hola", "hola", "cooked", "computer", "book store" }
	{ "*", { "-name" }, 			 { "10", "5", "6", "3", "8", "4", "7", "1", "9", "2" } },
	// { 0, 0.250000, 0.428571, 0.428571, 0.750000, 0.750000, 0.750000, 0.800000, 0.800000, MAX_DBL }
	{ "*", { "name:cook" }, 		 { "1", "2", "3", "8", "4", "7", "9", "5", "6", "10" } },
	{ "*", { "name:cook", "-_id" },  { "1", "2", "8", "3", "9", "7", "4", "6", "5", "10" } },
	// { MAX_DBL, 0.900000, 0.900000, 0.800000, 0.800000, 0.750000, 0.750000, 0.750000, 0.700000, 0.333333 }
	{ "*", { "-name:cook" }, 		 { "10", "3", "8", "5", "6", "4", "7", "9", "2", "1" } },
	{ "*", { "-name:cook", "-_id" }, { "10", "8", "3", "6", "5", "9", "7", "4", "2", "1" } }
};


const sort_t string_lcsq_tests[] {
	/*
	 * Table reference data to verify the ordering
	 * lcsq(fieldname:value) -> lcsq(get_value(fieldname), value)
	 * value for sort -> It is the value's field that is selected for the ordering when in the slot
	 *                   there are several values (in arrays).
	 * In arrays, for ascending order we take the smallest value and for descending order we take the largest.
	 *
	 * "_id"	"name"						lcsq(name:cook)			value for sort (ASC)	value for sort (DESC)
	 * "1"		["cook", "cooked"]			[0.000000, 0.333333]	"cook"					"cooked"
	 * "2"		["book store", "book"]		[0.700000, 0.250000]	"book"					"book store"
	 * "3"		["cooking", "hola mundo"]   [0.428571, 0.800000]	"cooking"				"hola mundo"
	 * "4"		"hola"						0.750000				"hola"					"hola"
	 * "5"		"mundo"						0.800000				"mundo"					"mundo"
	 * "6"		"mundo"						0.800000				"mundo"					"mundo"
	 * "7"		"hola"						0.750000				"hola"					"hola"
	 * "8"		["cooking", "hola mundo"]	[0.42857, 0.800000]	    "cooking"				"hola mundo"
	 * "9"		"computer"					0.750000				"computer"				"computer"
	 * "10"		Does not have				MAX_DBL					"\xff"					"\xff"
	 *
	 * The documents are indexed as the value of "_id" indicates.
	*/
	// { "book", "computer", "cook", "cooking", "cooking", "hola", "hola", "mundo", "mundo", "\xff" }
	{ "*", { "name" }, 			     { "2", "9", "1", "3", "8", "4", "7", "5", "6", "10" } },
	// { "\xff", "mundo", "mundo", "hola mundo", "hola mundo", "hola", "hola", "cooked", "computer", "book store" }
	{ "*", { "-name" }, 			 { "10", "5", "6", "3", "8", "4", "7", "1", "9", "2" } },
	// { 0, 0.250000, 0.428571, 0.428571, 0.750000, 0.750000, 0.750000, 0.800000, 0.800000, MAX_DBL }
	{ "*", { "name:cook" }, 		 { "1", "2", "3", "8", "4", "7", "9", "5", "6", "10" } },
	{ "*", { "name:cook", "-_id" },  { "1", "2", "8", "3", "9", "7", "4", "6", "5", "10" } },
	// { MAX_DBL, 0.800000, 0.800000, 0.800000, 0.800000, 0.750000, 0.750000, 0.750000, 0.700000, 0.333333 }
	{ "*", { "-name:cook" }, 		 { "10", "3", "5", "6", "8", "4", "7", "9", "2", "1" } },
	{ "*", { "-name:cook", "-_id" }, { "10", "8", "6", "5", "3", "9", "7", "4", "2", "1" } }
};


const sort_t string_soundex_en_tests[] {
	/*
	 * Table reference data to verify the ordering
	 * sound_en(fieldname:value) -> soundex_en(get_value(fieldname), value)
	 * value for sort -> It is the value's field that is selected for the ordering when in the slot
	 *                   there are several values (in arrays).
	 * In arrays, for ascending order we take the smallest value and for descending order we take the largest.
	 *
	 * "_id"	"name"						sound_en(name:coquing)	value for sort (ASC)	value for sort (DESC)
	 * "1"		["cook", "cooked"]			[0.571429, 0.428571]	"cook"					"cooked"
	 * "2"		["book store", "book"]		[0.625000, 0.857143]	"book store"			"book"
	 * "3"		["cooking", "hola mundo"]   [0.142857, 0.666667]	"cooking"				"hola mundo"
	 * "4"		"hola"						0.714286				"hola"					"hola"
	 * "5"		"mundo"						0.714286				"mundo"					"mundo"
	 * "6"		"mundo"						0.714286				"mundo"					"mundo"
	 * "7"		"hola"						0.714286				"hola"					"hola"
	 * "8"		["cooking", "hola mundo"]	[0.142857, 0.666667]	"cooking"				"hola mundo"
	 * "9"		"computer"					0.555556				"computer"				"computer"
	 * "10"		Does not have				MAX_DBL					"\xff"					"\xff"
	 *
	 * The documents are indexed as the value of "_id" indicates.
	*/
	// { "book", "computer", "cook", "cooking", "cooking", "hola", "hola", "mundo", "mundo", "\xff" }
	{ "*", { "name" }, 			        { "2", "9", "1", "3", "8", "4", "7", "5", "6", "10" } },
	// { "\xff", "mundo", "mundo", "hola mundo", "hola mundo", "hola", "hola", "cooked", "computer", "book store" }
	{ "*", { "-name" }, 			    { "10", "5", "6", "3", "8", "4", "7", "1", "9", "2" } },
	// { 0.142857, 0.142857, 0.428571, 0.555556, 0.625000, 0.714286, 0.714286, 0.714286, 0.714286, MAX_DBL }
	{ "*", { "name:coquing" }, 		    { "3", "8", "1", "9", "2", "4", "5", "6", "7", "10" } },
	{ "*", { "name:coquing", "-_id" },  { "8", "3", "1", "9", "2", "7", "6", "5", "4", "10" } },
	// { MAX_DBL, 0.857143, 0.714286, 0.714286, 0.714286, 0.714286, 0.666667, 0.666667, 0.571429, 0.555556 }
	{ "*", { "-name:coquing" }, 	    { "10", "2", "4", "5", "6", "7", "3", "8", "1", "9" } },
	{ "*", { "-name:coquing", "-_id" }, { "10", "2", "7", "6", "5", "4", "8", "3", "1", "9" } }
};


const sort_t string_soundex_fr_tests[] {
	/*
	 * Table reference data to verify the ordering
	 * sound_fr(fieldname:value) -> soundex_fr(get_value(fieldname), value)
	 * value for sort -> It is the value's field that is selected for the ordering when in the slot
	 *                   there are several values (in arrays).
	 * In arrays, for ascending order we take the smallest value and for descending order we take the largest.
	 *
	 * "_id"	"name"						sound_fr(name:cõœkîng)	value for sort (ASC)	value for sort (DESC)
	 * "1"		["cook", "cooked"]			[0.500000, 0.333333]	"cooked"				"cook"
	 * "2"		["book store", "book"]		[0.571429, 0.666667]	"book store"			"book"
	 * "3"		["cooking", "hola mundo"]   [0.000000, 0.571429]	"cooking"				"hola mundo"
	 * "4"		"hola"						0.833333				"hola"					"hola"
	 * "5"		"mundo"						0.666667				"mundo"					"mundo"
	 * "6"		"mundo"						0.666667				"mundo"					"mundo"
	 * "7"		"hola"						0.833333				"hola"					"hola"
	 * "8"		["cooking", "hola mundo"]	[0.000000, 0.571429]	"cooking"				"hola mundo"
	 * "9"		"computer"					0.625000				"computer"				"computer"
	 * "10"		Does not have				MAX_DBL					"\xff"					"\xff"
	 *
	 * The documents are indexed as the value of "_id" indicates.
	*/
	// { "book", "computer", "cook", "cooking", "cooking", "hola", "hola", "mundo", "mundo", "\xff" }
	{ "*", { "name" }, 			        { "2", "9", "1", "3", "8", "4", "7", "5", "6", "10" } },
	// { "\xff", "mundo", "mundo", "hola mundo", "hola mundo", "hola", "hola", "cooked", "computer", "book store" }
	{ "*", { "-name" }, 			    { "10", "5", "6", "3", "8", "4", "7", "1", "9", "2" } },
	// { 0., 0., 0.333333, 0.571429, 0.625000, 0.666667, 0.666667, 0.833333, 0.833333, MAX_DBL }
	{ "*", { "name:cõœkîng" }, 		    { "3", "8", "1", "2", "9", "5", "6", "4", "7", "10" } },
	{ "*", { "name:cõœkîng", "-_id" },  { "8", "3", "1", "2", "9", "6", "5", "7", "4", "10" } },
	// { MAX_DBL, 0.833333, 0.833333, 0.666667, 0.666667, 0.666667, 0.625000, 0.571429, 0.571429, 0.500000 }
	{ "*", { "-name:cõœkîng" }, 	    { "10", "4", "7", "2", "5", "6", "9", "3", "8", "1" } },
	{ "*", { "-name:cõœkîng", "-_id" }, { "10", "7", "4", "6", "5", "2", "9", "8", "3", "1" } }
};


const sort_t string_soundex_de_tests[] {
	/*
	 * Table reference data to verify the ordering
	 * sound_de(fieldname:value) -> soundex_de(get_value(fieldname), value)
	 * value for sort -> It is the value's field that is selected for the ordering when in the slot
	 *                   there are several values (in arrays).
	 * In arrays, for ascending order we take the smallest value and for descending order we take the largest.
	 *
	 * "_id"	"name"						sound_de(name:cöök)		value for sort (ASC)	value for sort (DESC)
	 * "1"		["cook", "cooked"]			[0.000000, 0.400000]	"cook"					"cooked"
	 * "2"		["book store", "book"]		[0.750000, 0.333333]	"book"					"book store"
	 * "3"		["cooking", "hola mundo"]   [0.500000, 0.875000]	"cooking"				"hola mundo"
	 * "4"		"hola"						0.666667				"hola"					"hola"
	 * "5"		"mundo"						0.800000				"mundo"					"mundo"
	 * "6"		"mundo"						0.800000				"mundo"					"mundo"
	 * "7"		"hola"						0.666667				"hola"					"hola"
	 * "8"		["cooking", "hola mundo"]	[0.500000, 0.875000]	"cooking"				"hola mundo"
	 * "9"		"computer"					0.714286				"computer"				"computer"
	 * "10"		Does not have				MAX_DBL					"\xff"					"\xff"
	 *
	 * The documents are indexed as the value of "_id" indicates.
	*/
	// { "book", "computer", "cook", "cooking", "cooking", "hola", "hola", "mundo", "mundo", "\xff" }
	{ "*", { "name" }, 			     { "2", "9", "1", "3", "8", "4", "7", "5", "6", "10" } },
	// { "\xff", "mundo", "mundo", "hola mundo", "hola mundo", "hola", "hola", "cooked", "computer", "book store" }
	{ "*", { "-name" }, 			 { "10", "5", "6", "3", "8", "4", "7", "1", "9", "2" } },
	// { 0., 0.333333, 0.500000, 0.500000, 0.666667, 0.666667, 0.714286, 0.800000, 0.800000, MAX_DBL }
	{ "*", { "name:cöök" }, 		 { "1", "2", "3", "8", "4", "7", "9", "5", "6", "10" } },
	{ "*", { "name:cöök", "-_id" },  { "1", "2", "8", "3", "7", "4", "9", "6", "5", "10" } },
	// { MAX_DBL, 0.875000, 0.875000, 0.800000, 0.800000, 0.750000, 0.714286, 0.666667, 0.666667, 0.400000 }
	{ "*", { "-name:cöök" }, 	     { "10", "3", "8", "5", "6", "2", "9", "4", "7", "1" } },
	{ "*", { "-name:cöök", "-_id" }, { "10", "8", "3", "6", "5", "2", "9", "7", "4", "1" } }
};


const sort_t string_soundex_es_tests[] {
	/*
	 * Table reference data to verify the ordering
	 * sound_es(fieldname:value) -> soundex_es(get_value(fieldname), value)
	 * value for sort -> It is the value's field that is selected for the ordering when in the slot
	 *                   there are several values (in arrays).
	 * In arrays, for ascending order we take the smallest value and for descending order we take the largest.
	 *
	 * "_id"	"name"						sound_es(name:coquing)	value for sort (ASC)	value for sort (DESC)
	 * "1"		["cook", "cooked"]			[0.428571, 0.285714]	"cooked"				"cook"
	 * "2"		["book store", "book"]		[0.625000, 0.714286]	"book store"			"book"
	 * "3"		["cooking", "hola mundo"]   [0.000000, 0.666667]	"cooking"				"hola mundo"
	 * "4"		"hola"						0.714286				"hola"					"hola"
	 * "5"		"mundo"						0.714286				"mundo"					"mundo"
	 * "6"		"mundo"						0.714286				"mundo"					"mundo"
	 * "7"		"hola"						0.714286				"hola"					"hola"
	 * "8"		["cooking", "hola mundo"]	[0.000000, 0.666667]	"cooking"				"hola mundo"
	 * "9"		"computer"					0.555556				"computer"				"computer"
	 * "10"		Does not have				MAX_DBL					"\xff"					"\xff"
	 *
	 * The documents are indexed as the value of "_id" indicates.
	*/
	// { "book", "computer", "cook", "cooking", "cooking", "hola", "hola", "mundo", "mundo", "\xff" }
	{ "*", { "name" }, 			        { "2", "9", "1", "3", "8", "4", "7", "5", "6", "10" } },
	// { "\xff", "mundo", "mundo", "hola mundo", "hola mundo", "hola", "hola", "cooked", "computer", "book store" }
	{ "*", { "-name" }, 			    { "10", "5", "6", "3", "8", "4", "7", "1", "9", "2" } },
	// { 0, 0, 0.285714, 0.555556, 0.625000, 0.714286, 0.714286, 0.714286, 0.714286, MAX_DBL }
	{ "*", { "name:coquing" }, 		    { "3", "8", "1", "9", "2", "4", "5", "6", "7", "10" } },
	{ "*", { "name:coquing", "-_id" },  { "8", "3", "1", "9", "2", "7", "6", "5", "4", "10" } },
	// { MAX_DBL, 0.714286, 0.714286, 0.714286, 0.714286, 0.714286, 0.666667, 0.666667, 0.555556, 0.428571 }
	{ "*", { "-name:coquing" }, 	    { "10", "2", "4", "5", "6", "7", "3", "8", "9", "1" } },
	{ "*", { "-name:coquing", "-_id" }, { "10", "7", "6", "5", "4", "2", "8", "3", "9", "1" } }
};


const sort_t numerical_tests[] {
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
	{ "*", { "_id" }, 				 { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10" } },
	{ "*", { "-_id" }, 				 { "10", "9", "8", "7", "6", "5", "4", "3", "2", "1" } },
	// { 0, 1, 2, 2, 2, 2, 2, 2, 2, 2 }
	{ "*", { "_id:10" }, 			 { "10", "9", "8", "7", "6", "5", "4", "3", "2", "1" } },
	// { 2, 2, 2, 2, 2, 2, 2, 2, 1, 0 }
	{ "*", { "-_id:10" }, 			 { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10" } },
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


const sort_t date_tests[] {
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


const sort_t boolean_tests[] {
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
	{ "*", { "-there:false", "-_id" },	{ "10", "8", "7", "4", "3", "1", "9", "6", "5", "2" } },
};


const sort_t geo_tests[] {
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


static DB_Test db_sort(".db_sort.db", std::vector<std::string>({
		// Examples used in test geo.
		path_test_sort + "doc1.txt",
		path_test_sort + "doc2.txt",
		path_test_sort + "doc3.txt",
		path_test_sort + "doc4.txt",
		path_test_sort + "doc5.txt",
		path_test_sort + "doc6.txt",
		path_test_sort + "doc7.txt",
		path_test_sort + "doc8.txt",
		path_test_sort + "doc9.txt",
		path_test_sort + "doc10.txt"
	}), DB_WRITABLE | DB_SPAWN | DB_NOWAL);


static int make_search(const sort_t _tests[], int len, const std::string& metric=std::string(), const std::string& lan="en") {
	int cont = 0;
	query_field_t query;
	query.offset = 0;
	query.limit = 10;
	query.check_at_least = 0;
	query.spelling = false;
	query.synonyms = false;
	query.is_fuzzy = false;
	query.is_nearest = false;
	query.metric = metric;
	query.language.push_back(lan);

	for (int i = 0; i < len; ++i) {
		sort_t p = _tests[i];
		query.query.clear();
		query.sort.clear();
		query.query.push_back(p.query);

		for (const auto& _sort : p.sort) {
			query.sort.push_back(_sort);
		}

		Xapian::MSet mset;
		std::vector<std::string> suggestions;
		std::vector<std::pair<std::string, std::unique_ptr<MultiValueCountMatchSpy>>> spies;
		try {
			db_sort.db_handler.get_mset(query, mset, spies, suggestions);
			if (mset.size() != p.expect_result.size()) {
				++cont;
				L_ERR(nullptr, "ERROR: Different number of documents. Obtained %u. Expected: %zu.", mset.size(), p.expect_result.size());
			} else {
				Xapian::MSetIterator m = mset.begin();
				for (auto it = p.expect_result.begin(); m != mset.end(); ++it, ++m) {
					auto val = Unserialise::unserialise(INTEGER_TYPE, m.get_document().get_value(0));
					if (it->compare(val) != 0) {
						++cont;
						L_ERR(nullptr, "ERROR: Result = %s:%s   Expected = %s:%s", RESERVED_ID, val.c_str(), RESERVED_ID, it->c_str());
					}
				}
			}
		} catch (const std::exception& exc) {
			L_EXC(nullptr, "ERROR: %s\n", exc.what());
			++cont;
		}
	}

	return cont;
}


int sort_test_string_levens() {
	try {
		int cont = make_search(string_levens_tests, arraySize(string_levens_tests), "leven");
		if (cont == 0) {
			L_DEBUG(nullptr, "Testing sort strings is correct!");
		} else {
			L_ERR(nullptr, "ERROR: Testing sort strings has mistakes.");
		}
		RETURN(cont);
	} catch (const Xapian::Error &err) {
		L_ERR(nullptr, "ERROR: %s", err.get_msg().c_str());
		RETURN(1);
	} catch (const std::exception &err) {
		L_ERR(nullptr, "ERROR: %s", err.what());
		RETURN(1);
	}
}


int sort_test_string_jaro() {
	try {
		int cont = make_search(string_jaro_tests, arraySize(string_jaro_tests), "jaro");
		if (cont == 0) {
			L_DEBUG(nullptr, "Testing sort strings is correct!");
		} else {
			L_ERR(nullptr, "ERROR: Testing sort strings has mistakes.");
		}
		RETURN(cont);
	} catch (const Xapian::Error &err) {
		L_ERR(nullptr, "ERROR: %s", err.get_msg().c_str());
		RETURN(1);
	} catch (const std::exception &err) {
		L_ERR(nullptr, "ERROR: %s", err.what());
		RETURN(1);
	}
}


int sort_test_string_jaro_w() {
	try {
		int cont = make_search(string_jaro_w_tests, arraySize(string_jaro_w_tests), "jarow");
		if (cont == 0) {
			L_DEBUG(nullptr, "Testing sort strings is correct!");
		} else {
			L_ERR(nullptr, "ERROR: Testing sort strings has mistakes.");
		}
		RETURN(cont);
	} catch (const Xapian::Error &err) {
		L_ERR(nullptr, "ERROR: %s", err.get_msg().c_str());
		RETURN(1);
	} catch (const std::exception &err) {
		L_ERR(nullptr, "ERROR: %s", err.what());
		RETURN(1);
	}
}


int sort_test_string_dice() {
	try {
		int cont = make_search(string_dice_tests, arraySize(string_dice_tests), "dice");
		if (cont == 0) {
			L_DEBUG(nullptr, "Testing sort strings is correct!");
		} else {
			L_ERR(nullptr, "ERROR: Testing sort strings has mistakes.");
		}
		RETURN(cont);
	} catch (const Xapian::Error &err) {
		L_ERR(nullptr, "ERROR: %s", err.get_msg().c_str());
		RETURN(1);
	} catch (const std::exception &err) {
		L_ERR(nullptr, "ERROR: %s", err.what());
		RETURN(1);
	}
}


int sort_test_string_jaccard() {
	try {
		int cont = make_search(string_jaccard_tests, arraySize(string_jaccard_tests), "jaccard");
		if (cont == 0) {
			L_DEBUG(nullptr, "Testing sort strings is correct!");
		} else {
			L_ERR(nullptr, "ERROR: Testing sort strings has mistakes.");
		}
		RETURN(cont);
	} catch (const Xapian::Error &err) {
		L_ERR(nullptr, "ERROR: %s", err.get_msg().c_str());
		RETURN(1);
	} catch (const std::exception &err) {
		L_ERR(nullptr, "ERROR: %s", err.what());
		RETURN(1);
	}
}


int sort_test_string_lcs() {
	try {
		int cont = make_search(string_lcs_tests, arraySize(string_lcs_tests), "lcs");
		if (cont == 0) {
			L_DEBUG(nullptr, "Testing sort strings is correct!");
		} else {
			L_ERR(nullptr, "ERROR: Testing sort strings has mistakes.");
		}
		RETURN(cont);
	} catch (const Xapian::Error &err) {
		L_ERR(nullptr, "ERROR: %s", err.get_msg().c_str());
		RETURN(1);
	} catch (const std::exception &err) {
		L_ERR(nullptr, "ERROR: %s", err.what());
		RETURN(1);
	}
}


int sort_test_string_lcsq() {
	try {
		int cont = make_search(string_lcsq_tests, arraySize(string_lcsq_tests), "lcsq");
		if (cont == 0) {
			L_DEBUG(nullptr, "Testing sort strings is correct!");
		} else {
			L_ERR(nullptr, "ERROR: Testing sort strings has mistakes.");
		}
		RETURN(cont);
	} catch (const Xapian::Error &err) {
		L_ERR(nullptr, "ERROR: %s", err.get_msg().c_str());
		RETURN(1);
	} catch (const std::exception &err) {
		L_ERR(nullptr, "ERROR: %s", err.what());
		RETURN(1);
	}
}


int sort_test_string_soundex_en() {
	try {
		int cont = make_search(string_soundex_en_tests, arraySize(string_soundex_en_tests), "soundex", "en");
		if (cont == 0) {
			L_DEBUG(nullptr, "Testing sort strings is correct!");
		} else {
			L_ERR(nullptr, "ERROR: Testing sort strings has mistakes.");
		}
		RETURN(cont);
	} catch (const Xapian::Error &err) {
		L_ERR(nullptr, "ERROR: %s", err.get_msg().c_str());
		RETURN(1);
	} catch (const std::exception &err) {
		L_ERR(nullptr, "ERROR: %s", err.what());
		RETURN(1);
	}
}


int sort_test_string_soundex_fr() {
	try {
		int cont = make_search(string_soundex_fr_tests, arraySize(string_soundex_fr_tests), "soundex", "fr");
		if (cont == 0) {
			L_DEBUG(nullptr, "Testing sort strings is correct!");
		} else {
			L_ERR(nullptr, "ERROR: Testing sort strings has mistakes.");
		}
		RETURN(cont);
	} catch (const Xapian::Error &err) {
		L_ERR(nullptr, "ERROR: %s", err.get_msg().c_str());
		RETURN(1);
	} catch (const std::exception &err) {
		L_ERR(nullptr, "ERROR: %s", err.what());
		RETURN(1);
	}
}


int sort_test_string_soundex_de() {
	try {
		int cont = make_search(string_soundex_de_tests, arraySize(string_soundex_de_tests), "soundex", "german");
		if (cont == 0) {
			L_DEBUG(nullptr, "Testing sort strings is correct!");
		} else {
			L_ERR(nullptr, "ERROR: Testing sort strings has mistakes.");
		}
		RETURN(cont);
	} catch (const Xapian::Error &err) {
		L_ERR(nullptr, "ERROR: %s", err.get_msg().c_str());
		RETURN(1);
	} catch (const std::exception &err) {
		L_ERR(nullptr, "ERROR: %s", err.what());
		RETURN(1);
	}
}


int sort_test_string_soundex_es() {
	try {
		int cont = make_search(string_soundex_es_tests, arraySize(string_soundex_es_tests), "soundex", "es");
		if (cont == 0) {
			L_DEBUG(nullptr, "Testing sort strings is correct!");
		} else {
			L_ERR(nullptr, "ERROR: Testing sort strings has mistakes.");
		}
		RETURN(cont);
	} catch (const Xapian::Error &err) {
		L_ERR(nullptr, "ERROR: %s", err.get_msg().c_str());
		RETURN(1);
	} catch (const std::exception &err) {
		L_ERR(nullptr, "ERROR: %s", err.what());
		RETURN(1);
	}
}


int sort_test_numerical() {
	try {
		int cont = make_search(numerical_tests, arraySize(numerical_tests));
		if (cont == 0) {
			L_DEBUG(nullptr, "Testing sort numbers is correct!");
		} else {
			L_ERR(nullptr, "ERROR: Testing sort numbers has mistakes.");
		}
		RETURN(cont);
	} catch (const Xapian::Error &err) {
		L_ERR(nullptr, "ERROR: %s", err.get_msg().c_str());
		RETURN(1);
	} catch (const std::exception &err) {
		L_ERR(nullptr, "ERROR: %s", err.what());
		RETURN(1);
	}
}


int sort_test_date() {
	try {
		int cont = make_search(date_tests, arraySize(date_tests));
		if (cont == 0) {
			L_DEBUG(nullptr, "Testing sort dates is correct!");
		} else {
			L_ERR(nullptr, "ERROR: Testing sort dates has mistakes.");
		}
		RETURN(cont);
	} catch (const Xapian::Error &err) {
		L_ERR(nullptr, "ERROR: %s", err.get_msg().c_str());
		RETURN (1);
	} catch (const std::exception &err) {
		L_ERR(nullptr, "ERROR: %s", err.what());
		RETURN (1);
	}
}


int sort_test_boolean() {
	try {
		int cont = make_search(boolean_tests, arraySize(boolean_tests));
		if (cont == 0) {
			L_DEBUG(nullptr, "Testing sort booleans is correct!");
		} else {
			L_ERR(nullptr, "ERROR: Testing sort booleans has mistakes.");
		}
		RETURN(cont);
	} catch (const Xapian::Error &err) {
		L_ERR(nullptr, "ERROR: %s", err.get_msg().c_str());
		RETURN(1);
	} catch (const std::exception &err) {
		L_ERR(nullptr, "ERROR: %s", err.what());
		RETURN(1);
	}
}


int sort_test_geo() {
	try {
		int cont = make_search(geo_tests, arraySize(geo_tests));
		if (cont == 0) {
			L_DEBUG(nullptr, "Testing sort geospatials is correct!");
		} else {
			L_ERR(nullptr, "ERROR: Testing sort geospatials has mistakes.");
		}
		RETURN(cont);
	} catch (const Xapian::Error &err) {
		L_ERR(nullptr, "ERROR: %s", err.get_msg().c_str());
		RETURN(1);
	} catch (const std::exception &err) {
		L_ERR(nullptr, "ERROR: %s", err.what());
		RETURN(1);
	}
}
