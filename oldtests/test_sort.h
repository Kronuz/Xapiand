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

#pragma once

#include <string>
#include <vector>


struct sort_t {
	std::string query;
	std::vector<std::string> sort;
	std::vector<std::string> expect_result;
};


// String Metrics.
int sort_test_string_levens();
int sort_test_string_jaro();
int sort_test_string_jaro_w();
int sort_test_string_dice();
int sort_test_string_jaccard();
int sort_test_string_lcs();
int sort_test_string_lcsq();

// Soundex Metrics.
int sort_test_string_soundex_en();
int sort_test_string_soundex_fr();
int sort_test_string_soundex_de();
int sort_test_string_soundex_es();

// Numericals
int sort_test_floats();
int sort_test_integers();
int sort_test_positives();

int sort_test_date();
int sort_test_boolean();
int sort_test_geo();
