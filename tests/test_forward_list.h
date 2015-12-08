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

#include "../src/forward_list.h"


template<typename Compare>
std::string repr_results(const ForwardList<std::pair<int, char>, Compare>& l, bool sort);

int test_push_front();
int test_insert_after();

int test_emplace_front();
int test_emplace_after();

int test_pop_front();
int test_erase_after();
int test_erase();

int test_remove();
int test_find();
int test_single_producer_consumer();

void task_producer(ForwardList<int>& mylist, std::atomic_size_t& elements);
int test_multiple_producers();

void task_producer_consumer(ForwardList<std::pair<int, char>>& mylist, std::atomic_size_t& elements);
int test_multiple_producers_consumers();

void task_producer_allconsumer(ForwardList<std::pair<int, char>>& mylist);
int test_multiple_producers_consumers_v2();
