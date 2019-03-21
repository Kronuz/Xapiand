/*
 * Copyright (c) 2015-2019 Dubalu LLC
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

#include "../src/geospatial/htm.h"


struct test_date_t {
	std::string datetime;
	std::string serialised;
};


struct test_cartesian_t {
	Cartesian cartesian;
	std::string serialised;
};


struct test_range_t {
	range_t range;
	std::string serialised;
};


struct test_uuid_t {
	std::string uuid;
	std::string serialised;
	std::string unserialised;
};


// Testing the transformation between datetime string and timestamp.
int test_datetotimestamp();
// Testing unserialise datetime.
int test_unserialise_date();
// Testing serialise Cartesian.
int test_serialise_cartesian();
// Testing unserialise Cartesian.
int test_unserialise_cartesian();
// Testing serialise range_t.
int test_serialise_range();
// Testing unserialise range_t.
int test_unserialise_range();
// Testing serialise uuid.
int test_serialise_uuid();
// Testing unserialise uuid.
int test_unserialise_uuid();
