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

#ifndef INCLUDED_TEST_SORT_H
#define INCLUDED_TEST_SORT_H

#include <sstream>
#include <fstream>

#include "../src/cJSON.h"
#include "../src/utils.h"
#include "../src/database.h"
#include "../src/endpoint.h"


typedef struct sort_s {
	std::string query;
	std::vector<std::string> sort;
	std::vector<std::string> expect_result;
} sort_t;


int create_test_db();
int make_search(const sort_t _tests[], int len);
int sort_test_string();
int sort_test_numerical();
int sort_test_date();
int sort_test_boolean();
int sort_test_geo();


#endif /* INCLUDED_TEST_SORT_H */