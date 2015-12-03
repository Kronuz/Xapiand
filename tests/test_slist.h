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

#include "../src/slist.h"


std::string repr_results(slist<std::string>& l, bool sort);
void push_front(slist<std::string>& l, char type, unsigned num);
void insert(slist<std::string>& l, char type, unsigned num);
void consumer(slist<std::string>& l, char type, unsigned num, std::atomic_size_t& deletes);
void consumer_v2(slist<std::string>& l, char type, unsigned num, std::atomic_size_t& deletes);

int test_push_front();
int test_insert();
int test_correct_order();
int test_remove();
int test_erase();
int test_pop_front();
int test_find();
int test_multiple_producers();
int test_multiple_producers_consumers();
int test_multiple_producers_consumers_v2();
