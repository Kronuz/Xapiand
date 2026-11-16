/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
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
	std::string serialized;
};


struct test_cartesian_t {
	Cartesian cartesian;
	std::string serialized;
};


struct test_range_t {
	range_t range;
	std::string serialized;
};


struct test_uuid_t {
	std::string uuid;
	std::string serialized;
	std::string unserialized;
};


// Testing the transformation between datetime string and timestamp.
int test_datetotimestamp();
// Testing unserialize datetime.
int test_unserialize_date();
// Testing serialize Cartesian.
int test_serialize_cartesian();
// Testing unserialize Cartesian.
int test_unserialize_cartesian();
// Testing serialize range_t.
int test_serialize_range();
// Testing unserialize range_t.
int test_unserialize_range();
// Testing serialize uuid.
int test_serialize_uuid();
// Testing unserialize uuid.
int test_unserialize_uuid();
