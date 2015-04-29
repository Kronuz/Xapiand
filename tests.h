/*
 * Copyright deipi.com LLC and contributors. All rights reserved.
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

#ifndef XAPIAND_INCLUDED_TESTS_H
#define XAPIAND_INCLUDED_TESTS_H

#include <stdio.h>
#include <string.h>
#include <algorithm>
#include "utils.h"
#include <unistd.h>

typedef struct test {
	const char *str;
	const char *expect;
} test;

typedef struct test_str_double {
	const char *str;
	const double val;
} test_str_double;

bool test_datetotimestamp();
bool test_distanceLatLong();
bool test_unserialise_date();
bool test_unserialise_geo();
void test_position_time();
void print_stats_sec();
void print_stats_min(int start = 0, int end = SLOT_TIME_MINUTE);

#endif /* XAPIAND_INCLUDED_TESTS_H */