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

#include "test_slist.h"

#include <check.h>
#include <stdlib.h>


START_TEST(test_push_front)
{
	ck_assert_int_eq(test_push_front(), 0);
}
END_TEST


START_TEST(test_insert)
{
	ck_assert_int_eq(test_insert(), 0);
}
END_TEST


START_TEST(test_correct_order)
{
	ck_assert_int_eq(test_correct_order(), 0);
}
END_TEST


START_TEST(test_remove)
{
	ck_assert_int_eq(test_remove(), 0);
}
END_TEST


START_TEST(test_erase)
{
	ck_assert_int_eq(test_erase(), 0);
}
END_TEST


START_TEST(test_pop_front)
{
	ck_assert_int_eq(test_pop_front(), 0);
}
END_TEST


START_TEST(test_find)
{
	ck_assert_int_eq(test_find(), 0);
}
END_TEST


START_TEST(test_multiple_producers)
{
	ck_assert_int_eq(test_multiple_producers(), 0);
}
END_TEST


START_TEST(test_multiple_producers_consumers)
{
	ck_assert_int_eq(test_multiple_producers_consumers(), 0);
}
END_TEST


START_TEST(test_multiple_producers_consumers_v2)
{
	ck_assert_int_eq(test_multiple_producers_consumers_v2(), 0);
}
END_TEST


Suite* test_suite_single_thread(void) {
	Suite *s = suite_create("Test of Lock Free List with single thread");

	TCase *tc_push_front = tcase_create("slist::push_front");
	tcase_set_timeout(tc_push_front, 5);
	tcase_add_test(tc_push_front, test_push_front);
	suite_add_tcase(s, tc_push_front);

	TCase *tc_push_insert = tcase_create("slist::insert");
	tcase_set_timeout(tc_push_insert, 5);
	tcase_add_test(tc_push_insert, test_insert);
	suite_add_tcase(s, tc_push_insert);

	TCase *tc_correct_order = tcase_create("Correct order of slist::insert and slist::push_front");
	tcase_set_timeout(tc_correct_order, 5);
	tcase_add_test(tc_correct_order, test_correct_order);
	suite_add_tcase(s, tc_correct_order);

	TCase *tc_remove = tcase_create("slist::remove");
	tcase_set_timeout(tc_remove, 5);
	tcase_add_test(tc_remove, test_remove);
	suite_add_tcase(s, tc_remove);

	TCase *tc_erase = tcase_create("slist::erase");
	tcase_set_timeout(tc_erase, 5);
	tcase_add_test(tc_erase, test_erase);
	suite_add_tcase(s, tc_erase);

	TCase *tc_pop_front = tcase_create("slist::pop_front");
	tcase_set_timeout(tc_pop_front, 5);
	tcase_add_test(tc_pop_front, test_pop_front);
	suite_add_tcase(s, tc_pop_front);

	TCase *tc_find = tcase_create("slist::find");
	tcase_set_timeout(tc_find, 5);
	tcase_add_test(tc_find, test_find);
	suite_add_tcase(s, tc_find);

	return s;
}


Suite* test_suite_multiple_threads(void) {
	Suite *s = suite_create("Test of Lock Free List with multiple threads");

	TCase *tc_multiple_producers = tcase_create("Multiple producers");
	tcase_set_timeout(tc_multiple_producers, 5);
	tcase_add_test(tc_multiple_producers, test_multiple_producers);
	suite_add_tcase(s, tc_multiple_producers);

	TCase *tc_multiple_producers_consumers = tcase_create("Multiple producers and consumers v1");
	tcase_set_timeout(tc_multiple_producers_consumers, 10);
	tcase_add_test(tc_multiple_producers_consumers, test_multiple_producers_consumers);
	suite_add_tcase(s, tc_multiple_producers_consumers);

	TCase *tc_multiple_producers_consumers2 = tcase_create("Multiple producers and consumers v2");
	tcase_set_timeout(tc_multiple_producers_consumers2, 10);
	tcase_add_test(tc_multiple_producers_consumers2, test_multiple_producers_consumers_v2);
	suite_add_tcase(s, tc_multiple_producers_consumers2);

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
