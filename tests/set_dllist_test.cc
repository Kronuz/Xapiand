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

#include "test_dllist.h"

#include <check.h>
#include <stdlib.h>


START_TEST(test_iterators)
{
	ck_assert_int_eq(test_iterators(), 0);
}
END_TEST


START_TEST(test_push_front)
{
	ck_assert_int_eq(test_push_front(), 0);
}
END_TEST


START_TEST(test_emplace_front)
{
	ck_assert_int_eq(test_emplace_front(), 0);
}
END_TEST


START_TEST(test_push_back)
{
	ck_assert_int_eq(test_push_back(), 0);
}
END_TEST


START_TEST(test_emplace_back)
{
	ck_assert_int_eq(test_emplace_back(), 0);
}
END_TEST


START_TEST(test_insert)
{
	ck_assert_int_eq(test_insert(), 0);
}
END_TEST


START_TEST(test_pop_front)
{
	ck_assert_int_eq(test_pop_front(), 0);
}
END_TEST


START_TEST(test_pop_back)
{
	ck_assert_int_eq(test_pop_back(), 0);
}
END_TEST


START_TEST(test_erase)
{
	ck_assert_int_eq(test_erase(), 0);
}
END_TEST


START_TEST(test_single_producer_consumer)
{
	ck_assert_int_eq(test_single_producer_consumer(), 0);
}
END_TEST


START_TEST(test_multi_push_emplace_front)
{
	ck_assert_int_eq(test_multi_push_emplace_front(), 0);
}
END_TEST


START_TEST(test_multi_push_emplace_back)
{
	ck_assert_int_eq(test_multi_push_emplace_back(), 0);
}
END_TEST


START_TEST(test_multi_insert)
{
	ck_assert_int_eq(test_multi_insert(), 0);
}
END_TEST


START_TEST(test_multi_producers)
{
	ck_assert_int_eq(test_multi_producers(), 0);
}
END_TEST


START_TEST(test_multi_push_pop_front)
{
	ck_assert_int_eq(test_multi_push_pop_front(), 0);
}
END_TEST


START_TEST(test_multi_push_pop_back)
{
	ck_assert_int_eq(test_multi_push_pop_back(), 0);
}
END_TEST


START_TEST(test_multi_insert_erase)
{
	ck_assert_int_eq(test_multi_insert_erase(), 0);
}
END_TEST


START_TEST(test_multiple_producers_single_consumer)
{
	ck_assert_int_eq(test_multiple_producers_single_consumer(), 0);
}
END_TEST


START_TEST(test_single_producer_multiple_consumers)
{
	ck_assert_int_eq(test_single_producer_multiple_consumers(), 0);
}
END_TEST


Suite* test_suite_single_thread(void) {
	Suite *s = suite_create("Test of Non-Blocking Doubly-Linked Lists with single thread");

	TCase *tc_iterators = tcase_create("DLList::iterators");
	tcase_add_test(tc_iterators, test_iterators);
	suite_add_tcase(s, tc_iterators);

	TCase *tc_push_front = tcase_create("DLList::push_front");
	tcase_add_test(tc_push_front, test_push_front);
	suite_add_tcase(s, tc_push_front);

	TCase *tc_emplace_front = tcase_create("DLList::emplace_front");
	tcase_add_test(tc_emplace_front, test_emplace_front);
	suite_add_tcase(s, tc_emplace_front);

	TCase *tc_push_back = tcase_create("DLList::push_back");
	tcase_add_test(tc_push_back, test_push_back);
	suite_add_tcase(s, tc_push_back);

	TCase *tc_emplace_back = tcase_create("DLList::emplace_back");
	tcase_add_test(tc_emplace_back, test_emplace_back);
	suite_add_tcase(s, tc_emplace_back);

	TCase *tc_insert = tcase_create("DLList::insert");
	tcase_add_test(tc_insert, test_insert);
	suite_add_tcase(s, tc_insert);

	TCase *tc_pop_front = tcase_create("DLList::pop_front");
	tcase_add_test(tc_pop_front, test_pop_front);
	suite_add_tcase(s, tc_pop_front);

	TCase *tc_pop_back = tcase_create("DLList::pop_back");
	tcase_add_test(tc_pop_back, test_pop_back);
	suite_add_tcase(s, tc_pop_back);

	TCase *tc_erase = tcase_create("DLList::erase");
	tcase_add_test(tc_erase, test_erase);
	suite_add_tcase(s, tc_erase);

	TCase *tc_single_producer_consumer = tcase_create("Test single producer-consumer");
	tcase_set_timeout(tc_single_producer_consumer, 10);
	tcase_add_test(tc_single_producer_consumer, test_single_producer_consumer);
	suite_add_tcase(s, tc_single_producer_consumer);

	return s;
}


Suite* test_suite_multiple_threads(void) {
	Suite *s = suite_create("Test of Non-Blocking Doubly-Linked Lists with multiple threads");

	TCase *tc_multi_push_emplace_front = tcase_create("Multiple threads are doing push_front and emplace_front");
	tcase_set_timeout(tc_multi_push_emplace_front, 10);
	tcase_add_test(tc_multi_push_emplace_front, test_multi_push_emplace_front);
	suite_add_tcase(s, tc_multi_push_emplace_front);

	TCase *tc_multi_push_emplace_back = tcase_create("Multiple threads are doing push_back and emplace_back");
	tcase_set_timeout(tc_multi_push_emplace_back, 10);
	tcase_add_test(tc_multi_push_emplace_back, test_multi_push_emplace_back);
	suite_add_tcase(s, tc_multi_push_emplace_back);

	TCase *tc_multi_insert = tcase_create("Multiple threads are doing insert");
	tcase_set_timeout(tc_multi_insert, 10);
	tcase_add_test(tc_multi_insert, test_multi_insert);
	suite_add_tcase(s, tc_multi_insert);

	TCase *tc_multi_producers = tcase_create("Multiple threads are adding elements");
	tcase_set_timeout(tc_multi_producers, 10);
	tcase_add_test(tc_multi_producers, test_multi_producers);
	suite_add_tcase(s, tc_multi_producers);

	TCase *tc_multi_push_pop_front = tcase_create("Multiple threads are doing push_front and pop_front");
	tcase_set_timeout(tc_multi_push_pop_front, 10);
	tcase_add_test(tc_multi_push_pop_front, test_multi_push_pop_front);
	suite_add_tcase(s, tc_multi_push_pop_front);

	TCase *tc_multi_push_pop_back = tcase_create("Multiple threads are doing push_back and pop_back");
	tcase_set_timeout(tc_multi_push_pop_back, 10);
	tcase_add_test(tc_multi_push_pop_back, test_multi_push_pop_back);
	suite_add_tcase(s, tc_multi_push_pop_back);

	TCase *tc_multi_insert_erase = tcase_create("Multiple threads are doing insert and erase");
	tcase_set_timeout(tc_multi_insert_erase, 10);
	tcase_add_test(tc_multi_insert_erase, test_multi_insert_erase);
	suite_add_tcase(s, tc_multi_insert_erase);

	TCase *tc_multiple_producers_single_consumer = tcase_create("Multiple producers and single consumer");
	tcase_set_timeout(tc_multiple_producers_single_consumer, 10);
	tcase_add_test(tc_multiple_producers_single_consumer, test_multiple_producers_single_consumer);
	suite_add_tcase(s, tc_multiple_producers_single_consumer);

	TCase *tc_single_producer_multiple_consumers = tcase_create("Single producer and multiple consumers");
	tcase_set_timeout(tc_single_producer_multiple_consumers, 10);
	tcase_add_test(tc_single_producer_multiple_consumers, test_single_producer_multiple_consumers);
	suite_add_tcase(s, tc_single_producer_multiple_consumers);

	return s;
}


int main(void) {
	Suite *single_thread = test_suite_single_thread();
	SRunner *sr = srunner_create(single_thread);
	srunner_run_all(sr, CK_NORMAL);
	int number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	Suite *multiple_threads = test_suite_multiple_threads();
	sr = srunner_create(multiple_threads);
	srunner_run_all(sr, CK_NORMAL);
	number_failed += srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
