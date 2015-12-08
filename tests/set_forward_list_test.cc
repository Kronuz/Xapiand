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

#include "test_forward_list.h"

#include <check.h>
#include <stdlib.h>


START_TEST(test_push_front)
{
	ck_assert_int_eq(test_push_front(), 0);
}
END_TEST


START_TEST(test_insert_after)
{
	ck_assert_int_eq(test_insert_after(), 0);
}
END_TEST


START_TEST(test_emplace_front)
{
	ck_assert_int_eq(test_emplace_front(), 0);
}
END_TEST


START_TEST(test_emplace_after)
{
	ck_assert_int_eq(test_emplace_after(), 0);
}
END_TEST


START_TEST(test_pop_front)
{
	ck_assert_int_eq(test_pop_front(), 0);
}
END_TEST


START_TEST(test_erase_after)
{
	ck_assert_int_eq(test_erase_after(), 0);
}
END_TEST


START_TEST(test_erase)
{
	ck_assert_int_eq(test_erase(), 0);
}
END_TEST


START_TEST(test_remove)
{
	ck_assert_int_eq(test_remove(), 0);
}
END_TEST


START_TEST(test_find)
{
	ck_assert_int_eq(test_find(), 0);
}
END_TEST


START_TEST(test_single_producer_consumer)
{
	ck_assert_int_eq(test_single_producer_consumer(), 0);
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
	Suite *s = suite_create("Test of Lock Free Forward List with single thread");

	TCase *tc_push_front = tcase_create("ForwardList::push_front");
	tcase_add_test(tc_push_front, test_push_front);
	suite_add_tcase(s, tc_push_front);

	TCase *tc_insert_after = tcase_create("ForwardList::insert_after");
	tcase_add_test(tc_insert_after, test_insert_after);
	suite_add_tcase(s, tc_insert_after);

	TCase *tc_emplace_front = tcase_create("ForwardList::emplace_front");
	tcase_add_test(tc_emplace_front, test_emplace_front);
	suite_add_tcase(s, tc_emplace_front);

	TCase *tc_emplace_after = tcase_create("ForwardList::emplace_after");
	tcase_add_test(tc_emplace_after, test_emplace_after);
	suite_add_tcase(s, tc_emplace_after);

	TCase *tc_pop_front = tcase_create("ForwardList::pop_front");
	tcase_add_test(tc_pop_front, test_pop_front);
	suite_add_tcase(s, tc_pop_front);

	TCase *tc_erase_after = tcase_create("ForwardList::erase_after");
	tcase_add_test(tc_erase_after, test_erase_after);
	suite_add_tcase(s, tc_erase_after);

	TCase *tc_erase = tcase_create("ForwardList::erase");
	tcase_add_test(tc_erase, test_erase);
	suite_add_tcase(s, tc_erase);

	TCase *tc_remove = tcase_create("ForwardList::remove");
	tcase_add_test(tc_remove, test_remove);
	suite_add_tcase(s, tc_remove);

	TCase *tc_find = tcase_create("ForwardList::find");
	tcase_add_test(tc_find, test_find);
	suite_add_tcase(s, tc_find);

	TCase *tc_single_producer_consumer = tcase_create("Test single producer-consumer");
	tcase_set_timeout(tc_single_producer_consumer, 10);
	tcase_add_test(tc_single_producer_consumer, test_single_producer_consumer);
	suite_add_tcase(s, tc_single_producer_consumer);

	return s;
}


Suite* test_suite_multiple_threads(void) {
	Suite *s = suite_create("Test of Lock Free Forward List with multiple threads");

	TCase *tc_multiple_producers = tcase_create("Multiple producers");
	tcase_set_timeout(tc_multiple_producers, 10);
	tcase_add_test(tc_multiple_producers, test_multiple_producers);
	suite_add_tcase(s, tc_multiple_producers);

	TCase *tc_multiple_producers_consumers = tcase_create("Multiple producers and consumers test 1");
	tcase_set_timeout(tc_multiple_producers_consumers, 10);
	tcase_add_test(tc_multiple_producers_consumers, test_multiple_producers_consumers);
	suite_add_tcase(s, tc_multiple_producers_consumers);

	TCase *tc_multiple_producers_consumers2 = tcase_create("Multiple producers and consumers test 2");
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
