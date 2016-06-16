/*
 * Copyright (C) 2016 deipi.com LLC and contributors. All rights reserved.
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

#pragma once

#include "../src/dllist.h"


std::string repr_results(const DLList<std::pair<int, char>>& l, bool sort);

// Single thread tests
int test_iterators();
int test_push_front();
int test_emplace_front();
int test_push_back();
int test_emplace_back();
int test_insert();

int test_pop_front();
int test_pop_back();
int test_erase();

int test_single_producer_consumer();

// Multi thread tests
int test_multi_push_front();
int test_multi_push_back();
int test_multi_insert();
int test_multi_producers();

int test_multi_push_pop_front();
int test_multi_push_pop_back();
int test_multi_insert_erase();

int test_multiple_producers_single_consumer();
int test_single_producer_multiple_consumers();
