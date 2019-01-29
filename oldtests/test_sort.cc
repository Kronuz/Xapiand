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

#include "test_sort.h"

#include "../src/datetime.h"
#include "../src/schema.h"
#include "../src/serialise.h"
#include "utils.h"


const std::string path_test_sort = std::string(FIXTURES_PATH) + "/examples/sort/";


const std::vector<sort_t> string_levens_tests({
	/*
	 * Table reference data to verify the ordering
	 * levens(fieldname:value) -> levenshtein_distance(get_value(fieldname), value)
	 * value for sort -> It is the value's field that is selected for the ordering when in the slot
	 *                   there are several values (in arrays).
	 * In arrays, for ascending order we take the smallest value and for descending order we take the largest.
	 *
	 * "id"     "name.en"                   levens(name.en:cook)    value for sort (ASC)    value for sort (DESC)
	 * "1"      ["cook", "cooked"]          [0.000000, 0.333333]    "cook"                  "cooked"
	 * "2"      ["bookstore", "book"]       [0.666667, 0.250000]    "book"                  "bookstore"
	 * "3"      ["cooking", "hello world"]  [0.428571, 0.818182]    "cooking"               "hello world"
	 * "4"      "hello"                     1.000000                "hello"                 "hello"
	 * "5"      "world"                     0.800000                "world"                 "world"
	 * "6"      "world"                     0.800000                "world"                 "world"
	 * "7"      "hello"                     1.000000                "hello"                 "hello"
	 * "8"      ["cooking", "hello world"]  [0.428571, 0.818182]    "cooking"               "hello world"
	 * "9"      "computer"                  0.750000                "computer"              "computer"
	 * "10"     Does not have               DBL_MAX/-DBL_MAX        "\xff"                  "\x00"
	 *
	 * The documents are indexed as the value of "id" indicates.
	*/
	// { "book", "computer", "cook", "cooking", "cooking", "hello", "hello", "world", "world", "\xff" }
	{ "*", { "name.en" },               { "2", "9", "1", "3", "8", "4", "7", "5", "6", "10" } },
	// { "world", "world", "hello world", "hello world", "hello", "hello", "cooked", "computer", "bookstore", "\x00" }
	{ "*", { "-name.en" },              { "5", "6", "3", "8", "4", "7", "1", "9", "2", "10" } },
	// { 0, 0.250000, 0.428571, 0.428571, 0.750000, 0.800000, 0.800000, 1, 1, DBL_MAX }
	{ "*", { "name.en:cook" },          { "1", "2", "3", "8", "9", "5", "6", "4", "7", "10" } },
	{ "*", { "name.en:cook", "-_id" },  { "1", "2", "8", "3", "9", "6", "5", "7", "4", "10" } },
	// { 1, 1, 0.818182, 0.818182, 0.800000, 0.800000, 0.750000, 0.666667, 0.333333, -DBL_MAX }
	{ "*", { "-name.en:cook" },         { "4", "7", "3", "8", "5", "6", "9", "2", "1", "10" } },
	{ "*", { "-name.en:cook", "-_id" }, { "7", "4", "8", "3", "6", "5", "9", "2", "1", "10" } },
});


const std::vector<sort_t> string_jaro_tests({
	/*
	 * Table reference data to verify the ordering
	 * jaro(fieldname:value) -> jaro(get_value(fieldname), value)
	 * value for sort -> It is the value's field that is selected for the ordering when in the slot
	 *                   there are several values (in arrays).
	 * In arrays, for ascending order we take the smallest value and for descending order we take the largest.
	 *
	 * "id"     "name.en"                   jaro(name.en:cook)      value for sort (ASC)    value for sort (DESC)
	 * "1"      ["cook", "cooked"]          [0.000000, 0.111111]    "cook"                  "cooked"
	 * "2"      ["bookstore", "book"]       [0.305556, 0.166667]    "book"                  "bookstore"
	 * "3"      ["cooking", "hello world"]  [0.142857, 0.553030]    "cooking"               "hello world"
	 * "4"      "hello"                     1.000000                "hello"                 "hello"
	 * "5"      "world"                     0.516667                "world"                 "world"
	 * "6"      "world"                     0.516667                "world"                 "world"
	 * "7"      "hello"                     1.000000                "hello"                 "hello"
	 * "8"      ["cooking", "hello world"]  [0.142857, 0.553030]    "cooking"               "hello world"
	 * "9"      "computer"                  0.416667                "computer"              "computer"
	 * "10"     Does not have               DBL_MAX/-DBL_MAX        "\xff"                  "\x00"
	 *
	 * The documents are indexed as the value of "_id" indicates.
	*/
	// { "book", "computer", "cook", "cooking", "cooking", "hello", "hello", "world", "world", "\xff" }
	{ "*", { "name.en" },               { "2", "9", "1", "3", "8", "4", "7", "5", "6", "10" } },
	// { "world", "world", "hello world", "hello world", "hello", "hello", "cooked", "computer", "bookstore", "\x00" }
	{ "*", { "-name.en" },              { "5", "6", "3", "8", "4", "7", "1", "9", "2", "10" } },
	// { 0, 0.142857, 0.142857, 0.166667, 0.416667, 0.500000, 0.500000, 1, 1, DBL_MAX }
	{ "*", { "name.en:cook" },          { "1", "3", "8", "2", "9", "5", "6", "4", "7", "10" } },
	{ "*", { "name.en:cook", "-_id" },  { "1", "8", "3", "2", "9", "6", "5", "7", "4", "10" } },
	// { 1, 1, 0.553030, 0.553030, 0.516667, 0.516667, 0.416667, 0.305556, 0.111111, -DBL_MAX }
	{ "*", { "-name.en:cook" },         { "4", "7", "3", "8", "5", "6", "9", "2", "1", "10" } },
	{ "*", { "-name.en:cook", "-_id" }, { "7", "4", "8", "3", "6", "5", "9", "2", "1", "10" } },
});


const std::vector<sort_t> string_jaro_w_tests({
	/*
	 * Table reference data to verify the ordering
	 * jaro_w(fieldname:value) -> jaro_winkler(get_value(fieldname), value)
	 * value for sort -> It is the value's field that is selected for the ordering when in the slot
	 *                   there are several values (in arrays).
	 * In arrays, for ascending order we take the smallest value and for descending order we take the largest.
	 *
	 * "_id"    "name.en"                   jaro_w(name.en:cook)    value for sort (ASC)    value for sort (DESC)
	 * "1"      ["cook", "cooked"]          [0.000000, 0.066667]    "cook"                  "cooked"
	 * "2"      ["bookstore", "book"]       [0.305556, 0.166667]    "book"                  "bookstore"
	 * "3"      ["cooking", "hello world"]  [0.085714, 0.553030]    "cooking"               "hello world"
	 * "4"      "hello"                     1.000000                "hello"                 "hello"
	 * "5"      "world"                     0.516667                "world"                 "world"
	 * "6"      "world"                     0.516667                "world"                 "world"
	 * "7"      "hello"                     1.000000                "hello"                 "hello"
	 * "8"      ["cooking", "hello world"]  [0.085714, 0.553030]    "cooking"               "hello world"
	 * "9"      "computer"                  0.416667                "computer"              "computer"
	 * "10"     Does not have               DBL_MAX/-DBL_MAX        "\xff"                  "\x00"
	 *
	 * The documents are indexed as the value of "_id" indicates.
	*/
	// { "book", "computer", "cook", "cooking", "cooking", "hello", "hello", "world", "world", "\xff" }
	{ "*", { "name.en" },               { "2", "9", "1", "3", "8", "4", "7", "5", "6", "10" } },
	// { "world", "world", "hello world", "hello world", "hello", "hello", "cooked", "computer", "bookstore", "\x00" }
	{ "*", { "-name.en" },              { "5", "6", "3", "8", "4", "7", "1", "9", "2", "10" } },
	// { 0, 0.085714, 0.085714, 0.166667, 0.416667, 0.516667, 0.516667, 1, 1, DBL_MAX }
	{ "*", { "name.en:cook" },          { "1", "3", "8", "2", "9", "5", "6", "4", "7", "10" } },
	{ "*", { "name.en:cook", "-_id" },  { "1", "8", "3", "2", "9", "6", "5", "7", "4", "10" } },
	// { 1, 1, 0.553030, 0.553030, 0.516667, 0.516667, 0.416667, 0.305556, 0.066667, -DBL_MAX }
	{ "*", { "-name.en:cook" },         { "4", "7", "3", "8", "5", "6", "9", "2", "1", "10" } },
	{ "*", { "-name.en:cook", "-_id" }, { "7", "4", "8", "3", "6", "5", "9", "2", "1", "10" } },
});


const std::vector<sort_t> string_dice_tests({
	/*
	 * Table reference data to verify the ordering
	 * dice(fieldname:value) -> sorensen_dice(get_value(fieldname), value)
	 * value for sort -> It is the value's field that is selected for the ordering when in the slot
	 *                   there are several values (in arrays).
	 * In arrays, for ascending order we take the smallest value and for descending order we take the largest.
	 *
	 * "_id"    "name.en"                   dice(name.en:cook)      value for sort (ASC)    value for sort (DESC)
	 * "1"      ["cook", "cooked"]          [0.000000, 0.250000]    "cook"                  "cooked"
	 * "2"      ["bookstore", "book"]       [0.636364, 0.333333]    "book"                  "bookstore"
	 * "3"      ["cooking", "hello world"]  [0.333333, 1.000000]    "cooking"               "hello world"
	 * "4"      "hello"                     1.000000                "hello"                 "hello"
	 * "5"      "world"                     1.000000                "world"                 "world"
	 * "6"      "world"                     1.000000                "world"                 "world"
	 * "7"      "hello"                     1.000000                "hello"                 "hello"
	 * "8"      ["cooking", "hello world"]  [0.333333, 1.000000]    "cooking"               "hello world"
	 * "9"      "computer"                  0.800000                "computer"              "computer"
	 * "10"     Does not have               DBL_MAX/-DBL_MAX        "\xff"                  "\x00"
	 *
	 * The documents are indexed as the value of "_id" indicates.
	*/
	// { "book", "computer", "cook", "cooking", "cooking", "hello", "hello", "world", "world", "\xff" }
	{ "*", { "name.en" },               { "2", "9", "1", "3", "8", "4", "7", "5", "6", "10" } },
	// { "world", "world", "hello world", "hello world", "hello", "hello", "cooked", "computer", "bookstore", "\x00" }
	{ "*", { "-name.en" },              { "5", "6", "3", "8", "4", "7", "1", "9", "2", "10" } },
	// { 0, 0.333333, 0.333333, 0.333333, 0.800000, 1, 1, 1, 1, DBL_MAX }
	{ "*", { "name.en:cook" },          { "1", "2", "3", "8", "9", "4", "5", "6", "7", "10" } },
	{ "*", { "name.en:cook", "-_id" },  { "1", "8", "3", "2", "9", "7", "6", "5", "4", "10" } },
	// { 1, 1, 1, 1, 1, 1, 0.800000, 0.636364, 0.250000, -DBL_MAX }
	{ "*", { "-name.en:cook" },         { "3", "4", "5", "6", "7", "8", "9", "2", "1", "10" } },
	{ "*", { "-name.en:cook", "-_id" }, { "8", "7", "6", "5", "4", "3", "9", "2", "1", "10" } },
});


const std::vector<sort_t> string_jaccard_tests({
	/*
	 * Table reference data to verify the ordering
	 * jaccard(fieldname:value) -> jaccard(get_value(fieldname), value)
	 * value for sort -> It is the value's field that is selected for the ordering when in the slot
	 *                   there are several values (in arrays).
	 * In arrays, for ascending order we take the smallest value and for descending order we take the largest.
	 *
	 * "_id"    "name.en"                   jaccard(name.en:cook)   value for sort (ASC)    value for sort (DESC)
	 * "1"      ["cook", "cooked"]          [0.000000, 0.400000]    "cook"                  "cooked"
	 * "2"      ["bookstore", "book"]       [0.750000, 0.500000]    "book"                  "bookstore"
	 * "3"      ["cooking", "hello world"]  [0.500000, 0.900000]    "cooking"               "hello world"
	 * "4"      "hello"                     0.833333                "hello"                 "hello"
	 * "5"      "world"                     0.857143                "world"                 "world"
	 * "6"      "world"                     0.857143                "world"                 "world"
	 * "7"      "hello"                     0.833333                "hello"                 "hello"
	 * "8"      ["cooking", "hello world"]  [0.500000, 0.900000]    "cooking"               "hello world"
	 * "9"      "computer"                  0.777778                "computer"              "computer"
	 * "10"     Does not have               DBL_MAX/-DBL_MAX        "\xff"                  "\x00"
	 *
	 * The documents are indexed as the value of "_id" indicates.
	*/
	// { "book", "computer", "cook", "cooking", "cooking", "hello", "hello", "world", "world", "\xff" }
	{ "*", { "name.en" },               { "2", "9", "1", "3", "8", "4", "7", "5", "6", "10" } },
	// { "world", "world", "hello world", "hello world", "hello", "hello", "cooked", "computer", "bookstore", "\x00" }
	{ "*", { "-name.en" },              { "5", "6", "3", "8", "4", "7", "1", "9", "2", "10" } },
	// { 0, 0.500000, 0.500000, 0.500000, 0.777778, 0.833333, 0.833333, 0.857143, 0.857143, DBL_MAX }
	{ "*", { "name.en:cook" },          { "1", "2", "3", "8", "9", "4", "7", "5", "6", "10" } },
	{ "*", { "name.en:cook", "-_id" },  { "1", "8", "3", "2", "9", "7", "4", "6", "5", "10" } },
	// { 0.900000, 0.900000, 0.857143, 0.857143, 0.833333, 0.833333, 0.777778, 0.750000, 0.400000, -DBL_MAX }
	{ "*", { "-name.en:cook" },         { "3", "8", "5", "6", "4", "7", "9", "2", "1", "10" } },
	{ "*", { "-name.en:cook", "-_id" }, { "8", "3", "6", "5", "7", "4", "9", "2", "1", "10" } },
});


const std::vector<sort_t> string_lcs_tests({
	/*
	 * Table reference data to verify the ordering
	 * lcs(fieldname:value) -> lcs(get_value(fieldname), value)
	 * value for sort -> It is the value's field that is selected for the ordering when in the slot
	 *                   there are several values (in arrays).
	 * In arrays, for ascending order we take the smallest value and for descending order we take the largest.
	 *
	 * "_id"    "name.en"                   lcs(name.en:cook)       value for sort (ASC)    value for sort (DESC)
	 * "1"      ["cook", "cooked"]          [0.000000, 0.333333]    "cook"                  "cooked"
	 * "2"      ["bookstore", "book"]       [0.666667, 0.250000]    "book"                  "bookstore"
	 * "3"      ["cooking", "hello world"]  [0.428571, 0.909091]    "cooking"               "hello world"
	 * "4"      "hello"                     0.800000                "hello"                 "hello"
	 * "5"      "world"                     0.800000                "world"                 "world"
	 * "6"      "world"                     0.800000                "world"                 "world"
	 * "7"      "hello"                     0.800000                "hello"                 "hello"
	 * "8"      ["cooking", "hello world"]  [0.42857, 0.909091]     "cooking"               "hello world"
	 * "9"      "computer"                  0.750000                "computer"              "computer"
	 * "10"     Does not have               DBL_MAX/-DBL_MAX        "\xff"                  "\x00"
	 *
	 * The documents are indexed as the value of "_id" indicates.
	*/
	// { "book", "computer", "cook", "cooking", "cooking", "hello", "hello", "world", "world", "\xff" }
	{ "*", { "name.en" },               { "2", "9", "1", "3", "8", "4", "7", "5", "6", "10" } },
	// { "world", "world", "hello world", "hello world", "hello", "hello", "cooked", "computer", "bookstore", "\x00" }
	{ "*", { "-name.en" },              { "5", "6", "3", "8", "4", "7", "1", "9", "2", "10" } },
	// { 0, 0.250000, 0.428571, 0.428571, 0.750000, 0.800000, 0.800000, 0.800000, 0.800000, DBL_MAX }
	{ "*", { "name.en:cook" },          { "1", "2", "3", "8", "9", "4", "5", "6", "7", "10" } },
	{ "*", { "name.en:cook", "-_id" },  { "1", "2", "8", "3", "9", "7", "6", "5", "4", "10" } },
	// { 0.909091, 0.909091, 0.800000, 0.800000, 0.800000, 0.800000, 0.750000, 0.666667, 0.333333, -DBL_MAX }
	{ "*", { "-name.en:cook" },         { "3", "8", "4", "5", "6", "7", "9", "2", "1", "10" } },
	{ "*", { "-name.en:cook", "-_id" }, { "8", "3", "7", "6", "5", "4", "9", "2", "1", "10" } },
});


const std::vector<sort_t> string_lcsq_tests({
	/*
	 * Table reference data to verify the ordering
	 * lcsq(fieldname:value) -> lcsq(get_value(fieldname), value)
	 * value for sort -> It is the value's field that is selected for the ordering when in the slot
	 *                   there are several values (in arrays).
	 * In arrays, for ascending order we take the smallest value and for descending order we take the largest.
	 *
	 * "_id"    "name.en"                   lcsq(name.en:cook)      value for sort (ASC)    value for sort (DESC)
	 * "1"      ["cook", "cooked"]          [0.000000, 0.333333]    "cook"                  "cooked"
	 * "2"      ["bookstore", "book"]       [0.666667, 0.250000]    "book"                  "bookstore"
	 * "3"      ["cooking", "hello world"]  [0.428571, 0.818182]    "cooking"               "hello world"
	 * "4"      "hello"                     0.800000                "hello"                 "hello"
	 * "5"      "world"                     0.800000                "world"                 "world"
	 * "6"      "world"                     0.800000                "world"                 "world"
	 * "7"      "hello"                     0.800000                "hello"                 "hello"
	 * "8"      ["cooking", "hello world"]  [0.42857, 0.818182]     "cooking"               "hello world"
	 * "9"      "computer"                  0.750000                "computer"              "computer"
	 * "10"     Does not have               DBL_MAX/-DBL_MAX        "\xff"                  "\x00"
	 *
	 * The documents are indexed as the value of "_id" indicates.
	*/
	// { "book", "computer", "cook", "cooking", "cooking", "hello", "hello", "world", "world", "\xff" }
	{ "*", { "name.en" },               { "2", "9", "1", "3", "8", "4", "7", "5", "6", "10" } },
	// { "world", "world", "hello world", "hello world", "hello", "hello", "cooked", "computer", "bookstore", "\x00" }
	{ "*", { "-name.en" },              { "5", "6", "3", "8", "4", "7", "1", "9", "2", "10" } },
	// { 0, 0.250000, 0.428571, 0.428571, 0.750000, 0.800000, 0.800000, 0.800000, 0.800000, DBL_MAX }
	{ "*", { "name.en:cook" },          { "1", "2", "3", "8", "9", "4", "5", "6", "7", "10" } },
	{ "*", { "name.en:cook", "-_id" },  { "1", "2", "8", "3", "9", "7", "6", "5", "4", "10" } },
	// { 0.818182, 0.818182, 0.800000, 0.800000, 0.800000, 0.800000, 0.750000, 0.666667, 0.333333, -DBL_MAX }
	{ "*", { "-name.en:cook" },         { "3", "8", "4", "5", "6", "7", "9", "2", "1", "10" } },
	{ "*", { "-name.en:cook", "-_id" }, { "8", "3", "7", "6", "5", "4", "9", "2", "1", "10" } },
});


const std::vector<sort_t> string_soundex_en_tests({
	/*
	 * Table reference data to verify the ordering
	 * sound_en(fieldname:value) -> soundex_en(get_value(fieldname), value)
	 * value for sort -> It is the value's field that is selected for the ordering when in the slot
	 *                   there are several values (in arrays).
	 * In arrays, for ascending order we take the smallest value and for descending order we take the largest.
	 *
	 * "_id"    "name.en"                   sound_en(name.en:cok)    value for sort (ASC)    value for sort (DESC)
	 * "1"      ["cook", "cooked"]          [0.000000, 0.333333]     "cook"                  "cooked"
	 * "2"      ["bookstore", "book"]       [0.750000, 0.500000]     "bookstore"             "book"
	 * "3"      ["cooking", "hello world"]  [0.428571, 0.857143]     "cooking"               "hello world"
	 * "4"      "hello"                     0.750000                 "hello"                 "hello"
	 * "5"      "world"                     0.800000                 "world"                 "world"
	 * "6"      "world"                     0.800000                 "world"                 "world"
	 * "7"      "hello"                     0.750000                 "hello"                 "hello"
	 * "8"      ["cooking", "hello world"]  [0.428571, 0.857143]     "cooking"               "hello world"
	 * "9"      "computer"                  0.666667                 "computer"              "computer"
	 * "10"     Does not have               DBL_MAX/-DBL_MAX         "\xff"                  "\x00"
	 *
	 * The documents are indexed as the value of "_id" indicates.
	*/
	// { "book", "computer", "cook", "cooking", "cooking", "hello", "hello", "world", "world", "\xff" }
	{ "*", { "name.en" },              { "2", "9", "1", "3", "8", "4", "7", "5", "6", "10" } },
	// { "world", "world", "hello world", "hello world", "hello", "hello", "cooked", "computer", "bookstore", "\x00" }
	{ "*", { "-name.en" },             { "5", "6", "3", "8", "4", "7", "1", "9", "2", "10" } },
	// { 0, 0.428571, 0.428571, 0.500000, 0.666667, 0.750000, 0.750000, 0.800000, 0.800000, DBL_MAX }
	{ "*", { "name.en:cok" },          { "1", "3", "8", "2", "9", "4", "7", "5", "6", "10" } },
	{ "*", { "name.en:cok", "-_id" },  { "1", "8", "3", "2", "9", "7", "4", "6", "5", "10" } },
	// { 0.857143, 0.857143, 0.800000, 0.800000, 0.750000, 0.750000, 0.750000, 0.666667, 0.333333, -DBL_MAX }
	{ "*", { "-name.en:cok" },         { "3", "8", "5", "6", "2", "4", "7", "9", "1", "10" } },
	{ "*", { "-name.en:cok", "-_id" }, { "8", "3", "6", "5", "7", "4", "2", "9", "1", "10" } },
});


const std::vector<sort_t> string_soundex_fr_tests({
	/*
	 * Table reference data to verify the ordering
	 * sound_fr(fieldname:value) -> soundex_fr(get_value(fieldname), value)
	 * value for sort -> It is the value's field that is selected for the ordering when in the slot
	 *                   there are several values (in arrays).
	 * In arrays, for ascending order we take the smallest value and for descending order we take the largest.
	 *
	 * "_id"    "name.fr"                        sound_fr(name.fr:bônjûr)     value for sort (ASC)    value for sort (DESC)
	 * "1"      ["cuire", "cuit"]                [0.666667, 0.833333]         "cuire"                 "cuit"
	 * "2"      ["librairie", "livre"]           [0.571429, 0.666667]         "librairie"             "livre"
	 * "3"      ["cuisine", "bonjour le monde"]  [0.666667, 0.500000]         "cuisine"               "bonjour le monde"
	 * "4"      "bonjour"                        0.000000                     "bonjour"               "bonjour"
	 * "5"      "monde"                          0.666667                     "monde"                 "monde"
	 * "6"      "monde"                          0.666667                     "monde"                 "monde"
	 * "7"      "bonjour"                        0.000000                     "bonjour"               "bonjour"
	 * "8"      ["cuisine", "bonjour le monde"]  [0.666667, 0.500000]         "cuisine"               "bonjour le monde"
	 * "9"      "ordinateur"                     0.555556                     "ordinateur"            "ordinateur"
	 * "10"     Does not have                    DBL_MAX/-DBL_MAX             "\xff"                  "\x00"
	 *
	 * The documents are indexed as the value of "_id" indicates.
	*/
	// { "bonjour", "bonjour", "bonjour le monde", "bonjour le monde", "cuire", "librairie", "monde", "monde", "ordinateur", "\xff" }
	{ "*", { "name.fr" },                 { "4", "7", "3", "8", "1", "2", "5", "6", "9", "10" } },
	// { "ordinateur", "monde", "monde", "librairie", "cuire", "bonjour le monde", "bonjour le monde", "bonjour", "bonjour", "\x00" }
	{ "*", { "-name.fr" },                { "9", "5", "6", "2", "1", "3", "8", "4", "7", "10" } },
	// { 0., 0., 0.500000, 0.500000, 0.555556, 0.571429, 0.666667, 0.666667, 0.666667, DBL_MAX }
	{ "*", { "name.fr:bônjûr" },          { "4", "7", "3", "8", "9", "2", "1", "5", "6", "10" } },
	{ "*", { "name.fr:bônjûr", "-_id" },  { "7", "4", "8", "3", "9", "2", "6", "5", "1", "10" } },
	// { 0.833333, 0.666667, 0.666667, 0.666667, 0.666667, 0.666667, 0.555556, 0.000000, 0.000000, -DBL_MAX }
	{ "*", { "-name.fr:bônjûr" },         { "1", "2", "3", "5", "6", "8", "9", "4", "7", "10" } },
	{ "*", { "-name.fr:bônjûr", "-_id" }, { "1", "8", "6", "5", "3", "2", "9", "7", "4", "10" } },
});


const std::vector<sort_t> string_soundex_de_tests({
	/*
	 * Table reference data to verify the ordering
	 * sound_de(fieldname:value) -> soundex_de(get_value(fieldname), value)
	 * value for sort -> It is the value's field that is selected for the ordering when in the slot
	 *                   there are several values (in arrays).
	 * In arrays, for ascending order we take the smallest value and for descending order we take the largest.
	 *
	 * "_id"    "name.de"                  sound_de(name.de:häälöö)    value for sort (ASC)   value for sort (DESC)
	 * "1"      ["coch", "gecocht"]        [1.000000, 0.800000]        "coch"                 "gecocht"
	 * "2"      ["buchladen", "buch"]      [0.625000, 0.666667]        "buch"                 "buchladen"
	 * "3"      ["kochen", "hallo welt"]   [0.600000, 0.571429]        "hallo welt"           "kochen"
	 * "4"      "hallo"                    0.000000                    "hallo"                "hallo"
	 * "5"      "welt"                     0.500000                    "welt"                 "welt"
	 * "6"      "welt"                     0.500000                    "welt"                 "welt"
	 * "7"      "hallo"                    0.000000                    "hallo"                "hallo"
	 * "8"      ["kochen", "hallo welt"]   [0.600000, 0.571429]        "hallo welt"           "kochen"
	 * "9"      "computer"                 0.714286                    "computer"             "computer"
	 * "10"     Does not have              DBL_MAX/-DBL_MAX           "\xff"                  "\x00"
	 *
	 * The documents are indexed as the value of "_id" indicates.
	*/
	// { "buch", "coch", "computer", "hallo", "hallo", "hallo welt", "hallo welt", "welt", "welt", "\xff" }
	{ "*", { "name.de" },                 { "2", "1", "9", "4", "7", "3", "8", "5", "6", "10" } },
	// { "welt", "welt", "kochen", "kochen", "hallo", "hallo", "gecocht", "computer", "buchladen", "\x00" }
	{ "*", { "-name.de" },                { "5", "6", "3", "8", "4", "7", "1", "9", "2", "10" } },
	// { 0., 0., 0.500000, 0.500000, 0.571429, 0.571429, 0.625000, 0.714286, 0.800000, DBL_MAX }
	{ "*", { "name.de:häälöö" },          { "4", "7", "5", "6", "3", "8", "2", "9", "1", "10" } },
	{ "*", { "name.de:häälöö", "-_id" },  { "7", "4", "6", "5", "8", "3", "2", "9", "1", "10" } },
	// { 1.000000, 0.714286, 0.666667, 0.600000, 0.600000, 0.500000, 0.500000, 0., 0., -DBL_MAX }
	{ "*", { "-name.de:häälöö" },         { "1", "9", "2", "3", "8", "5", "6", "4", "7", "10" } },
	{ "*", { "-name.de:häälöö", "-_id" }, { "1", "9", "2", "8", "3", "6", "5", "7", "4", "10" } },
});


const std::vector<sort_t> string_soundex_es_tests({
	/*
	 * Table reference data to verify the ordering
	 * sound_es(fieldname:value) -> soundex_es(get_value(fieldname), value)
	 * value for sort -> It is the value's field that is selected for the ordering when in the slot
	 *                   there are several values (in arrays).
	 * In arrays, for ascending order we take the smallest value and for descending order we take the largest.
	 *
	 * "_id"    "name.es"                  sound_es(name.es:kocinor)   value for sort (ASC)  value for sort (DESC)
	 * "1"      ["cocinar", "coc_ido"]     [0.000000, 0.285714]        "cocinar"             "coc_ido"
	 * "2"      ["librería", "libro"]      [0.625000, 0.714286]        "librería"            "libro"
	 * "3"      ["cocina", "hola mundo"]   [0.142857, 0.666667]        "cocina"              "hola mundo"
	 * "4"      "hola"                     0.714286                    "hola"                "hola"
	 * "5"      "mundo"                    0.571429                    "mundo"               "mundo"
	 * "6"      "mundo"                    0.571429                    "mundo"               "mundo"
	 * "7"      "hola"                     0.714286                    "hola"                "hola"
	 * "8"      ["cocina", "hola mundo"]   [0.142857, 0.666667]        "cocina"              "hola mundo"
	 * "9"      "computadora"              0.500000                    "computadora"         "computadora"
	 * "10"     Does not have              DBL_MAX/-DBL_MAX            "\xff"                "\x00"
	 *
	 * The documents are indexed as the value of "_id" indicates.
	*/
	// { "coc_ido", "cocina", "cocina", "computadora", "hola", "hola", "librería", "mundo", "mundo", "\xff" }
	{ "*", { "name.es" },                  { "1", "3", "8", "9", "4", "7", "2", "5", "6", "10" } },
	// { "mundo", "mundo", "libro", "hola mundo", "hola mundo", "hola", "hola", "computadora", "cocinar", "\x00" }
	{ "*", { "-name.es" },                 { "5", "6", "2", "3", "8", "4", "7", "9", "1", "10" } },
	// { 0., 0.142857, 0.142857, 0.500000, 0.571429, 0.571429, 0.625000, 0.714286, 0.714286, DBL_MAX }
	{ "*", { "name.es:kocinor" },          { "1", "3", "8", "9", "5", "6", "2", "4", "7", "10" } },
	{ "*", { "name.es:kocinor", "-_id" },  { "1", "8", "3", "9", "6", "5", "2", "7", "4", "10" } },
	// { 0.714286, 0.714286, 0.714286, 0.666667, 0.666667, 0.571429, 0.571429, 0.500000, 0.285714, -DBL_MAX }
	{ "*", { "-name.es:kocinor" },         { "2", "4", "7", "3", "8", "5", "6", "9", "1", "10" } },
	{ "*", { "-name.es:kocinor", "-_id" }, { "7", "4", "2", "8", "3", "6", "5", "9", "1", "10" } },
});


const std::vector<sort_t> float_tests({
	/*
	 * Table reference data to verify the ordering
	 * dist(fieldname:value) -> fabs(Unserialise::_float(get_value(fieldname)) - value)
	 * value for sort -> It is the value's field that is selected for the ordering when in the slot
	 *                   there are several values (in array).
	 * In arrays, for ascending order we take the smallest value and for descending order we take the largest.
	 *
	 * "_id"    "price"                      dist(price:1000.05)          dist(price:2000.10)       value for sort (ASC)   value for sort (DESC)
	 * "1"      [500.10, 2010.20, 2015.30]   [499.95, 1010.15, 1015.25]   [1500.00, 10.10, 15.20]   500.10                 2015.30
	 * "2"      [2000.10, 2001.30]           [1000.05, 1001.25]           [0.00, 1.20]              2000.10                2001.30
	 * "3"      [1000.15, 2000.30]           [0.10, 1000.25]              [999.95, 0.20]            1000.15                2000.30
	 * "4"      100.90                       899.15                       1899.20                   100.90                 100.90
	 * "5"      500.50                       499.55                       1499.60                   500.50                 500.50
	 * "6"      400.40                       599.65                       1599.70                   400.40                 400.40
	 * "7"      100.10                       899.95                       1900.00                   100.10                 100.10
	 * "8"      [-10000.60, 0.00]            [11000.65, 1000.05]          [12000.70, 2000.10]       -10000.60              0.00
	 * "9"      [2000.15, 2001.09]           [1000.10, 1001.04]           [0.05, 0.99]              2000.15                2001.09
	 * "10"     Does not have                DBL_MAX/-DBL_MAX              DBL_MAX/-DBL_MAX           DBL_MAX                -DBL_MAX
	 *
	 * The documents are indexed as the value of "_id" indicates.
	*/
	// { -10000.60, 100.10, 100.90, 400.40, 500.10, 500.50, 1000.15, 2000.10, 2000.15, DBL_MAX }
	{ "*", { "price" },                  { "8", "7", "4", "6", "1", "5", "3", "2", "9", "10" } },
	// { 2015.30, 2001.30, 2001.09, 2000.30, 500.50, 400.40, 100.90, 100.10, 0.00, -DBL_MAX }
	{ "*", { "-price" },                 { "1", "2", "9", "3", "5", "6", "4", "7", "8", "10" } },
	// { 0.10, 499.55, 499.95, 599.65, 899.15, 899.95, 1000.05, 1000.05, 1000.10, DBL_MAX  }
	{ "*", { "price:1000.05" },          { "3", "5", "1", "6", "4", "7", "2", "8", "9", "10" } },
	{ "*", { "price:1000.05", "-_id" },  { "3", "5", "1", "6", "4", "7", "8", "2", "9", "10" } },
	// { 11000.65, 1015.25, 1001.25, 1001.04, 1000.25, 899.95, 899.15, 599.65, 499.55, -DBL_MAX }
	{ "*", { "-price:1000.05" },         { "8", "1", "2", "9", "3", "7", "4", "6", "5", "10" } },
	// { 0.00, 0.05, 0.20, 10.10, 1499.60, 1599.70, 1899.20, 1900.00, 2000.10, DBL_MAX }
	{ "*", { "price:2000.10" },          { "2", "9", "3", "1", "5", "6", "4", "7", "8", "10" } },
	// { 12000.70, 1900.00, 1899.20, 1599.70, 1500.00, 1499.60, 1000.20, 1.20, 0.99, -DBL_MAX  }
	{ "*", { "-price:2000.10" },         { "8", "7", "4", "6", "1", "5", "3", "2", "9", "10" } },
});


const std::vector<sort_t> integer_tests({
	/*
	 * Table reference data to verify the ordering
	 * dist(fieldname:value) -> llabs(Unserialise::integer(get_value(fieldname)) - value)
	 * value for sort -> It is the value's field that is selected for the ordering when in the slot
	 *                   there are several values (in array).
	 * In arrays, for ascending order we take the smallest value and for descending order we take the largest.
	 *
	 * "_id"    "year"              dist(year:1000)     dist(year:2000)   value for sort (ASC)   value for sort (DESC)
	 * "1"      [500, 2010, 2015]   [500, 1010, 1015]   [1500, 10, 15]    500                    2015
	 * "2"      [2000, 2001]        [1000, 1001]        [0, 1]            2000                   2001
	 * "3"      [1000, 2000]        [0, 1000]           [1000, 0]         1000                   2000
	 * "4"      100                 900                 1900              100                    100
	 * "5"      500                 500                 1500              500                    500
	 * "6"      400                 600                 1600              400                    400
	 * "7"      100                 900                 1900              100                    100
	 * "8"      [-10000, 0]         [11000, 1000]       [12000, 2000]     -10000                 0
	 * "9"      [2000, 2001]        [1000, 1001]        [0, 1]            2000                   2001
	 * "10"     Does not have       DBL_MAX/-DBL_MAX     DBL_MAX/-DBL_MAX   DBL_MAX                -DBL_MAX
	 *
	 * The documents are indexed as the value of "_id" indicates.
	*/
	{ "*", { "_id" },                 { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10" } },
	{ "*", { "-_id" },                { "10", "9", "8", "7", "6", "5", "4", "3", "2", "1" } },
	// { 0, 1, 1, 2, 2, 3, 3, 4, 4, 5 }
	{ "*", { "_id:5" },               { "5", "4", "6", "3", "7", "2", "8", "1", "9", "10" } },
	// { 5, 4, 4, 3, 3, 2, 2, 1, 1, 0 }
	{ "*", { "-_id:5" },              { "10", "1", "9", "2", "8", "3", "7", "4", "6", "5" } },
	// { -10000, 100, 100, 400, 500, 500, 1000, 2000, 2000, DBL_MAX }
	{ "*", { "year" },                { "8", "4", "7", "6", "1", "5", "3", "2", "9", "10" } },
	// { 2015, 2000, 2001, 2001, 500, 400, 100, 100, 0, -DBL_MAX }
	{ "*", { "-year" },               { "1", "2", "9", "3", "5", "6", "4", "7", "8", "10" } },
	// { 0, 500, 500, 600, 900, 900, 1000, 1000, 1000, DBL_MAX  }
	{ "*", { "year:1000" },           { "3", "1", "5", "6", "4", "7", "2", "8", "9", "10" } },
	// { 11000, 1015, 1001, 1001, 1000, 900, 900, 600, 500, -DBL_MAX }
	{ "*", { "-year:1000" },          { "8", "1", "2", "9", "3", "4", "7", "6", "5", "10" } },
	// { 0, 0, 0, 10, 1500, 1600, 1900, 1900, 2000, DBL_MAX }
	{ "*", { "year:2000" },           { "2", "3", "9", "1", "5", "6", "4", "7", "8", "10" } },
	{ "*", { "year:2000", "-_id" },   { "9", "3", "2", "1", "5", "6", "7", "4", "8", "10" } },
	// { 12000, 1900, 1900, 1600, 1500, 1500, 1000, 1, 1, -DBL_MAX }
	{ "*", { "-year:2000" },          { "8", "4", "7", "6", "1", "5", "3", "2", "9", "10" } },
	{ "*", { "-year:2000", "-_id" },  { "8", "7", "4", "6", "5", "1", "3", "9", "2", "10" } },
});


const std::vector<sort_t> positive_tests({
	/*
	 * Table reference data to verify the ordering
	 * dist(fieldname:value) -> llabs(Unserialise::positive(get_value(fieldname)) - value)
	 * value for sort -> It is the value's field that is selected for the ordering when in the slot
	 *                   there are several values (in array).
	 * In arrays, for ascending order we take the smallest value and for descending order we take the largest.
	 *
	 * "_id"    "members"            dist(members:1500)   dist(members:2500)   value for sort (ASC)   value for sort (DESC)
	 * "1"      [1000, 2000, 3000]   [500, 500, 1500]     [1500, 500, 500]     1000                   3000
	 * "2"      [1500, 3200]         [0, 1700]            [1000, 700]          1500                   3200
	 * "3"      [800, 3000]          [700, 1500]          [1700, 500]          800                    3000
	 * "4"      400                  1100                 2100                 400                    400
	 * "5"      500                  1000                 2000                 500                    500
	 * "6"      600                  900                  1900                 600                    600
	 * "7"      380                  1120                 2120                 380                    380
	 * "8"      [0, 10000]           [1500, 8500]         [2500, 7500]         0                      10000
	 * "9"      [2100, 2500]         [600, 1000]          [400, 0]             2100                   2500
	 * "10"     Does not have        DBL_MAX/-DBL_MAX      DBL_MAX/-DBL_MAX      DBL_MAX                -DBL_MAX
	 *
	 * The documents are indexed as the value of "_id" indicates.
	*/
	// { 0, 380, 400, 500, 600, 800, 1000, 1500, 2100, DBL_MAX }
	{ "*", { "members" },                { "8", "7", "4", "5", "6", "3", "1", "2", "9", "10" } },
	// { 10000, 3200, 3000, 3000, 2500, 600, 500, 400, 380, -DBL_MAX }
	{ "*", { "-members" },               { "8", "2", "1", "3", "9", "6", "5", "4", "7", "10" } },
	// { 0, 500, 600, 700, 900, 1000, 1100, 1120, 1500, DBL_MAX  }
	{ "*", { "members:1500" },           { "2", "1", "9", "3", "6", "5", "4", "7", "8", "10" } },
	// { 8500, 1700, 1500, 1500, 1120, 1100, 1000, 1000, 900, -DBL_MAX }
	{ "*", { "-members:1500" },          { "8", "2", "1", "3", "7", "4", "5", "9", "6", "10" } },
	{ "*", { "-members:1500", "-_id" },  { "8", "2", "3", "1", "7", "4", "9", "5", "6", "10" } },
	// { 0, 500, 500, 700, 1900, 2000, 2100, 2120, 2500, DBL_MAX }
	{ "*", { "members:2500" },           { "9", "1", "3", "2", "6", "5", "4", "7", "8", "10" } },
	{ "*", { "members:2500", "-_id" },   { "9", "3", "1", "2", "6", "5", "4", "7", "8", "10" } },
	// { 7500, 2120, 2100, 2000, 1900, 1700, 1500, 1000, 400, -DBL_MAX }
	{ "*", { "-members:2500" },          { "8", "7", "4", "5", "6", "3", "1", "2", "9", "10" } },
});


const std::vector<sort_t> date_tests({
	/*
	 * Table reference data to verify the ordering.
	 * dist(fieldname:value) -> fabs(Unserialise::timestamp(get_value(fieldname)) - Datetime::timestamp(value))
	 * value for sort -> It is the value's field that is selected for the ordering when in the slot
	 *                   there are several values (in array).
	 * In arrays, for ascending order we take the smallest value and for descending order we take the largest.
	 *
	 * "_id"     "date"                               dist(date:2010-01-01)        dist(date:0001-01-01)         value for sort (ASC)   value for sort (DESC)
	 *                                                Epoch: 1262304000            Epoch: -62135596800
	 * "1"      ["2010-10-21", "2011-01-01"],         [25315200, 31536000]         [63423216000, 63429436800]    1287619200             1293840000
	 *          Epoch: [1287619200, 1293840000]
	 * "2"      ["1810-01-01", "1910-01-01"],         [6311433600, 3155760000]     [57086467200, 60242140800]    -5049129600            -1893456000
	 *          Epoch: [-5049129600, -1893456000]
	 * "3"      ["0010-01-01", "0020-01-01"],         [63113904000, 62798371200]   [283996800, 599529600]        -61851600000           -61536067200
	 *          Epoch: [-61851600000, -61536067200]
	 * "4"      "0001-01-01",                         63397900800                   0                            -62135596800           -62135596800
	 *          Epoch: -62135596800
	 * "5"      "2015-01-01",                         157766400                     63555667200                  1420070400             1420070400
	 *          Epoch: 1420070400
	 * "6"      "2015-01-01",                         157766400                     63555667200                  1420070400             1420070400
	 *          Epoch: 1420070400
	 * "7"      "0300-01-01",                         53962416000                   9435484800                   -52700112000           -52700112000
	 *          Epoch: -52700112000
	 * "8"      ["0010-01-01", "0020-01-01"],         [63113904000, 62798371200]    [283996800, 599529600]       -61851600000           -61536067200
	 *          Epoch: [-61851600000, -61536067200]
	 * "9"      ["1810-01-01", "1910-01-01"],         [6311433600, 3155760000]      [57086467200, 60242140800]   -5049129600            -1893456000
	 *          Epoch: [-5049129600, -1893456000]
	 * "10"     Does not have value                   DBL_MAX/-DBL_MAX               DBL_MAX/-DBL_MAX              DBL_MAX                -DBL_MAX
	 *
	 * The documents are indexed as the value of "_id" indicates.
	*/
	// { "0001-01-01", "0010-01-01", "0010-01-01", "0300-01-01", "1810-01-01", "1810-01-01", "2010-10-21", "2015-01-01", "2015-01-01", DBL_MAX }
	{ "*", { "date" },                      { "4", "3", "8", "7", "2", "9", "1", "5", "6", "10" } },
	// { "2015-01-01", "2015-01-01", "2011-01-01", "1910-01-01", "1910-01-01", "0300-01-01", "0020-01-01", "0020-01-01", "0001-01-01", -DBL_MAX }
	{ "*", { "-date" },                     { "5", "6", "1", "2", "9", "7", "3", "8", "4", "10" } },
	// { 25315200, 157766400, 157766400, 3155760000, 3155760000, 53962416000, 62798371200, 62798371200, 63397900800, DBL_MAX }
	{ "*", { "date:2010-01-01" },           { "1", "5", "6", "2", "9", "7", "3", "8", "4", "10" } },
	{ "*", { "date:20100101 00:00:00" },    { "1", "5", "6", "2", "9", "7", "3", "8", "4", "10" } },
	{ "*", { "date:2010-01-01T00:00:00" },  { "1", "5", "6", "2", "9", "7", "3", "8", "4", "10" } },
	// { 63397900800, 63113904000, 63113904000, 53962416000, 6311433600, 6311433600, 157766400, 157766400, 31536000, -DBL_MAX }
	{ "*", { "-date:2010-01-01" },          { "4", "3", "8", "7", "2", "9", "5", "6", "1", "10" } },
	// { 0, 283996800, 283996800, 9435484800, 57086467200, 57086467200, 63423216000, 63555667200, 63555667200, DBL_MAX }
	{ "*", { "date:0001-01-01" },           { "4", "3", "8", "7", "2", "9", "1", "5", "6", "10" } },
	{ "*", { "date:00010101 00:00:00" },    { "4", "3", "8", "7", "2", "9", "1", "5", "6", "10" } },
	{ "*", { "date:0001-01-01T00:00:00" },  { "4", "3", "8", "7", "2", "9", "1", "5", "6", "10" } },
	{ "*", { "date:0001-01-01", "-_id" },   { "4", "8", "3", "7", "9", "2", "1", "6", "5", "10" } },
	// { 63555667200, 63555667200, 63429436800, 60242140800, 60242140800, 9435484800, 599529600, 599529600, 0, -DBL_MAX }
	{ "*", { "-date:0001-01-01" },          { "5", "6", "1", "2", "9", "7", "3", "8", "4", "10" } },
	{ "*", { "-date:0001-01-01", "-_id" },  { "6", "5", "1", "9", "2", "7", "8", "3", "4", "10" } },
});


const std::vector<sort_t> boolean_tests({
	/*
	 * Table reference data to verify the ordering
	 * dist(fieldname:value) -> get_value(fieldname) == value ? 0 : 1
	 * value for sort -> It is the value's field that is selected for the ordering when in the slot
	 *                   there are several values (in arrays).
	 * In arrays, for ascending order we take the smallest value and for descending order we take the largest.
	 *
	 * "_id"    "there"             dist(there:false)    dist(there:true)    value for sort (ASC)    value for sort (DESC)
	 * "1"      [true, false],        [1, 0]             [0, 1]              false                   true
	 * "2"      [false, false],       [0, 0]             [1, 1]              false                   false
	 * "3"      [true, true],         [1, 1]             [0, 0]              true                    true
	 * "4"      true,                     1                  0               true                    true
	 * "5"      false,                    0                  1               false                   false
	 * "6"      false,                    0                  1               false                   false
	 * "7"      true,                     1                  0               true                    true
	 * "8"      [true, true],         [1, 1]             [0, 0]              true                    true
	 * "9"      [false, false]        [0, 0]             [1, 1]              false                   false
	 * "10"     Does not have value   DBL_MAX/-DBL_MAX   DBL_MAX/-DBL_MAX    DBL_MAX                 -DBL_MAX
	 *
	 * The documents are indexed as the value of "_id" indicates.
	*/
	// { false, false, false, false, true, true, true, true, true, DBL_MAX }
	{ "*", { "there" },                 { "1", "2", "5", "6", "9", "3", "4", "7", "8", "10" } },
	// { true, true, true, true, true, false, false, false, false, -DBL_MAX }
	{ "*", { "-there" },                { "1", "3", "4", "7", "8", "2", "5", "6", "9", "10" } },
	// { 0, 0, 0, 0, 0, 1, 1, 1, 1, DBL_MAX }
	{ "*", { "there:true" },            { "1", "3", "4", "7", "8", "2", "5", "6", "9", "10" } },
	// { 1, 1, 1, 1, 1, 0, 0, 0, 0, -DBL_MAX }
	{ "*", { "-there:true" },           { "1", "2", "5", "6", "9", "3", "4", "7", "8", "10" } },
	// { 0, 0, 0, 0, 0, 1, 1, 1, 1, DBL_MAX }
	{ "*", { "there:false" },           { "1", "2", "5", "6", "9", "3", "4", "7", "8", "10" } },
	// { 1, 1, 1, 1, 1, 0, 0, 0, 0, -DBL_MAX }
	{ "*", { "-there:false" },          { "1", "3", "4", "7", "8", "2", "5", "6", "9", "10" } },
	{ "*", { "-there:false", "-_id" },  { "8", "7", "4", "3", "1", "9", "6", "5", "2", "10" } },
});


const std::vector<sort_t> geo_tests({
	/*
	 * Table reference data to verify the ordering
	 * radius(fieldname:value) -> Angle between centroids of value and centroids saved in the slot.
	 * value for sort -> It is the value's field that is selected for the ordering when in the slot
	 *                   there are several values (in array).
	 * In arrays, for ascending order we take the smallest value and for descending order we take the largest.
	 *
	 * "_id"    "location"                          radius(location:POINT(5 5))   radius(location:CIRCLE(10 10,200000))
	 * "1"      ["POINT(10 21)", "POINT(10 20)"]    [0.290050, 0.273593]          [0.189099, 0.171909]
	 * "2"      ["POINT(20 40)", "POINT(50 60)"]    [0.648657, 1.120883]          [0.533803, 0.999915]
	 * "3"      ["POINT(0 0)", "POINT(0 70)"]       [0.122925, 1.136214]          [0.245395, 1.055833]
	 * "4"      "CIRCLE(2 2, 2000)"                 0.073730                      0.196201
	 * "5"      "CIRCLE(10 10, 2000)"               0.122473                      0.000036
	 * "6"      "CIRCLE(10 10, 2000)"               0.122473                      0.000036
	 * "7"      "CIRCLE(2 2, 2000)"                 0.073730                      0.196201
	 * "8"      "POINT(3.2 10.1)"                   0.094108                      0.117923
	 * "9"      ["POINT(20 40)", "POINT(50 60)"]    [0.648657, 1.120883]          [0.533803, 0.999915]
	 * "10"     Does not have value                 DBL_MAX/-DBL_MAX              DBL_MAX/-DBL_MAX
	 *
	 * The documents are indexed as the value of "_id" indicates.
	*/
	// It does not have effect in the results.
	{ "*", { "location" },                                { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10" } },
	// It does not have effect in the results.
	{ "*", { "-location" },                               { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10" } },
	// { 0.073730, 0.073730, 0.094108, 0.122473, 0.122473, 0.122925, 0.273593, 0.648657, 0.648657, DBL_MAX }
	{ "*", { "location:POINT(5 5)" },                     { "4", "7", "8", "5", "6", "3", "1", "2", "9", "10" } },
	// { 1.136214, 1.120883, 1.120883, 0.290050, 0.122473, 0.122473, 0.094108, 0.073730, 0.073730, -DBL_MAX }
	{ "*", { "-location:POINT(5 5)" },                    { "3", "2", "9", "1", "5", "6", "8", "4", "7", "10" } },
	// { 0.000036, 0.000036, 0.117923, 0.171909, 0.196201, 0.196201, 0.245395, 0.533803, 0.533803, DBL_MAX }
	{ "*", { "location:CIRCLE(10 10,200000)" },           { "5", "6", "8", "1", "4", "7", "3", "2", "9", "10" } },
	{ "*", { "location:CIRCLE(10 10,200000)", "-_id" },   { "6", "5", "8", "1", "7", "4", "3", "9", "2", "10" } },
	// { 1.055833, 0.999915, 0.999915, 0.196201, 0.196201, 0.189099,  0.117923, 0.000036, 0.000036, -DBL_MAX }
	{ "*", { "-location:CIRCLE(10 10,200000)" },          { "3", "2", "9", "4", "7", "1", "8", "5", "6", "10" } },
	{ "*", { "-location:CIRCLE(10 10,200000)", "-_id" },  { "3", "9", "2", "7", "4", "1", "8", "6", "5", "10" } },
});


static int make_search(const std::vector<sort_t> _tests, const std::string& metric=std::string()) {
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
		}), DB_WRITABLE | DB_CREATE_OR_OPEN | DB_NO_WAL);

	int cont = 0;
	query_field_t query;
	query.metric = metric;

	auto schema = db_sort.db_handler.get_schema();
	auto spc_id = schema->get_data_id();
	auto id_type = spc_id.get_type();

	for (const auto& test : _tests) {
		query.query.clear();
		query.query.push_back(test.query);
		query.sort = test.sort;

		MSet mset;
		std::vector<std::string> suggestions;

		try {
			mset = db_sort.db_handler.get_mset(query, nullptr, nullptr, suggestions);
			if (mset.size() != test.expect_result.size()) {
				++cont;
				L_ERR("ERROR: Different number of documents. Obtained %u. Expected: %zu.", mset.size(), test.expect_result.size());
			} else {
				auto m = mset.begin();
				for (auto it = test.expect_result.begin(); m != mset.end(); ++it, ++m) {
					auto document = db_sort.db_handler.get_document(*m);
					auto val = Unserialise::MsgPack(id_type, document.get_value(0)).to_string();
					if (it->compare(val) != 0) {
						++cont;
						L_ERR("ERROR: Result = %s:%s   Expected = %s:%s", ID_FIELD_NAME, val, ID_FIELD_NAME, *it);
					}
				}
			}
		} catch (const std::exception& exc) {
			L_EXC("ERROR: %s", exc.what());
			++cont;
		}
	}

	return cont;
}


int sort_test_string_levens() {
	INIT_LOG
	try {
		int cont = make_search(string_levens_tests, "leven");
		if (cont == 0) {
			L_DEBUG("Testing sort strings (levens) is correct!");
		} else {
			L_ERR("ERROR: Testing sort strings (levens) has mistakes.");
		}
		RETURN(cont);
	} catch (const Xapian::Error &exc) {
		L_EXC("ERROR: %s", exc.get_description());
		RETURN(1);
	} catch (const std::exception &exc) {
		L_EXC("ERROR: %s", exc.what());
		RETURN(1);
	}
}


int sort_test_string_jaro() {
	INIT_LOG
	try {
		int cont = make_search(string_jaro_tests, "jaro");
		if (cont == 0) {
			L_DEBUG("Testing sort strings (jaro) is correct!");
		} else {
			L_ERR("ERROR: Testing sort strings (jaro) has mistakes.");
		}
		RETURN(cont);
	} catch (const Xapian::Error &exc) {
		L_EXC("ERROR: %s", exc.get_description());
		RETURN(1);
	} catch (const std::exception &exc) {
		L_EXC("ERROR: %s", exc.what());
		RETURN(1);
	}
}


int sort_test_string_jaro_w() {
	INIT_LOG
	try {
		int cont = make_search(string_jaro_w_tests, "jarow");
		if (cont == 0) {
			L_DEBUG("Testing sort strings  (jaro-winkler) is correct!");
		} else {
			L_ERR("ERROR: Testing sort strings (jaro-winkler)  has mistakes.");
		}
		RETURN(cont);
	} catch (const Xapian::Error &exc) {
		L_EXC("ERROR: %s", exc.get_description());
		RETURN(1);
	} catch (const std::exception &exc) {
		L_EXC("ERROR: %s", exc.what());
		RETURN(1);
	}
}


int sort_test_string_dice() {
	INIT_LOG
	try {
		int cont = make_search(string_dice_tests, "dice");
		if (cont == 0) {
			L_DEBUG("Testing sort strings (sorensen-dice) is correct!");
		} else {
			L_ERR("ERROR: Testing sort strings (sorensen-dice) has mistakes.");
		}
		RETURN(cont);
	} catch (const Xapian::Error &exc) {
		L_EXC("ERROR: %s", exc.get_description());
		RETURN(1);
	} catch (const std::exception &exc) {
		L_EXC("ERROR: %s", exc.what());
		RETURN(1);
	}
}


int sort_test_string_jaccard() {
	INIT_LOG
	try {
		int cont = make_search(string_jaccard_tests, "jaccard");
		if (cont == 0) {
			L_DEBUG("Testing sort strings (jaccard) is correct!");
		} else {
			L_ERR("ERROR: Testing sort strings (jaccard) has mistakes.");
		}
		RETURN(cont);
	} catch (const Xapian::Error &exc) {
		L_EXC("ERROR: %s", exc.get_description());
		RETURN(1);
	} catch (const std::exception &exc) {
		L_EXC("ERROR: %s", exc.what());
		RETURN(1);
	}
}


int sort_test_string_lcs() {
	INIT_LOG
	try {
		int cont = make_search(string_lcs_tests, "lcs");
		if (cont == 0) {
			L_DEBUG("Testing sort strings (lcs) is correct!");
		} else {
			L_ERR("ERROR: Testing sort strings (lcs) has mistakes.");
		}
		RETURN(cont);
	} catch (const Xapian::Error &exc) {
		L_EXC("ERROR: %s", exc.get_description());
		RETURN(1);
	} catch (const std::exception &exc) {
		L_EXC("ERROR: %s", exc.what());
		RETURN(1);
	}
}


int sort_test_string_lcsq() {
	INIT_LOG
	try {
		int cont = make_search(string_lcsq_tests, "lcsq");
		if (cont == 0) {
			L_DEBUG("Testing sort strings (lcsq) is correct!");
		} else {
			L_ERR("ERROR: Testing sort strings (lcsq) has mistakes.");
		}
		RETURN(cont);
	} catch (const Xapian::Error &exc) {
		L_EXC("ERROR: %s", exc.get_description());
		RETURN(1);
	} catch (const std::exception &exc) {
		L_EXC("ERROR: %s", exc.what());
		RETURN(1);
	}
}


int sort_test_string_soundex_en() {
	INIT_LOG
	try {
		int cont = make_search(string_soundex_en_tests, "soundex");
		if (cont == 0) {
			L_DEBUG("Testing sort strings (soundex-en) is correct!");
		} else {
			L_ERR("ERROR: Testing sort strings (soundex-en) has mistakes.");
		}
		RETURN(cont);
	} catch (const Xapian::Error &exc) {
		L_EXC("ERROR: %s", exc.get_description());
		RETURN(1);
	} catch (const std::exception &exc) {
		L_EXC("ERROR: %s", exc.what());
		RETURN(1);
	}
}


int sort_test_string_soundex_fr() {
	INIT_LOG
	try {
		int cont = make_search(string_soundex_fr_tests, "soundex");
		if (cont == 0) {
			L_DEBUG("Testing sort strings (soundex-fr) is correct!");
		} else {
			L_ERR("ERROR: Testing sort strings (soundex-fr) has mistakes.");
		}
		RETURN(cont);
	} catch (const Xapian::Error &exc) {
		L_EXC("ERROR: %s", exc.get_description());
		RETURN(1);
	} catch (const std::exception &exc) {
		L_EXC("ERROR: %s", exc.what());
		RETURN(1);
	}
}


int sort_test_string_soundex_de() {
	INIT_LOG
	try {
		int cont = make_search(string_soundex_de_tests, "soundex");
		if (cont == 0) {
			L_DEBUG("Testing sort strings (soundex-de) is correct!");
		} else {
			L_ERR("ERROR: Testing sort strings (soundex-de) has mistakes.");
		}
		RETURN(cont);
	} catch (const Xapian::Error &exc) {
		L_EXC("ERROR: %s", exc.get_description());
		RETURN(1);
	} catch (const std::exception &exc) {
		L_EXC("ERROR: %s", exc.what());
		RETURN(1);
	}
}


int sort_test_string_soundex_es() {
	INIT_LOG
	try {
		int cont = make_search(string_soundex_es_tests, "soundex");
		if (cont == 0) {
			L_DEBUG("Testing sort strings (soundex-es) is correct!");
		} else {
			L_ERR("ERROR: Testing sort strings (soundex-es) has mistakes.");
		}
		RETURN(cont);
	} catch (const Xapian::Error &exc) {
		L_EXC("ERROR: %s", exc.get_description());
		RETURN(1);
	} catch (const std::exception &exc) {
		L_EXC("ERROR: %s", exc.what());
		RETURN(1);
	}
}


int sort_test_floats() {
	INIT_LOG
	try {
		int cont = make_search(float_tests);
		if (cont == 0) {
			L_DEBUG("Testing sort floats is correct!");
		} else {
			L_ERR("ERROR: Testing sort floats has mistakes.");
		}
		RETURN(cont);
	} catch (const Xapian::Error &exc) {
		L_EXC("ERROR: %s", exc.get_description());
		RETURN(1);
	} catch (const std::exception &exc) {
		L_EXC("ERROR: %s", exc.what());
		RETURN(1);
	}
}


int sort_test_integers() {
	INIT_LOG
	try {
		int cont = make_search(integer_tests);
		if (cont == 0) {
			L_DEBUG("Testing sort integers is correct!");
		} else {
			L_ERR("ERROR: Testing sort integers has mistakes.");
		}
		RETURN(cont);
	} catch (const Xapian::Error &exc) {
		L_EXC("ERROR: %s", exc.get_description());
		RETURN(1);
	} catch (const std::exception &exc) {
		L_EXC("ERROR: %s", exc.what());
		RETURN(1);
	}
}


int sort_test_positives() {
	INIT_LOG
	try {
		int cont = make_search(positive_tests);
		if (cont == 0) {
			L_DEBUG("Testing sort positives is correct!");
		} else {
			L_ERR("ERROR: Testing sort positives has mistakes.");
		}
		RETURN(cont);
	} catch (const Xapian::Error &exc) {
		L_EXC("ERROR: %s", exc.get_description());
		RETURN(1);
	} catch (const std::exception &exc) {
		L_EXC("ERROR: %s", exc.what());
		RETURN(1);
	}
}


int sort_test_date() {
	INIT_LOG
	try {
		int cont = make_search(date_tests);
		if (cont == 0) {
			L_DEBUG("Testing sort dates is correct!");
		} else {
			L_ERR("ERROR: Testing sort dates has mistakes.");
		}
		RETURN(cont);
	} catch (const Xapian::Error &exc) {
		L_EXC("ERROR: %s", exc.get_description());
		RETURN (1);
	} catch (const std::exception &exc) {
		L_EXC("ERROR: %s", exc.what());
		RETURN (1);
	}
}


int sort_test_boolean() {
	INIT_LOG
	try {
		int cont = make_search(boolean_tests);
		if (cont == 0) {
			L_DEBUG("Testing sort booleans is correct!");
		} else {
			L_ERR("ERROR: Testing sort booleans has mistakes.");
		}
		RETURN(cont);
	} catch (const Xapian::Error &exc) {
		L_EXC("ERROR: %s", exc.get_description());
		RETURN(1);
	} catch (const std::exception &exc) {
		L_EXC("ERROR: %s", exc.what());
		RETURN(1);
	}
}


int sort_test_geo() {
	INIT_LOG
	try {
		int cont = make_search(geo_tests);
		if (cont == 0) {
			L_DEBUG("Testing sort geospatials is correct!");
		} else {
			L_ERR("ERROR: Testing sort geospatials has mistakes.");
		}
		RETURN(cont);
	} catch (const Xapian::Error &exc) {
		L_EXC("ERROR: %s", exc.get_description());
		RETURN(1);
	} catch (const std::exception &exc) {
		L_EXC("ERROR: %s", exc.what());
		RETURN(1);
	}
}
