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

#include "test_queue.h"

#include <check.h>
#include <stdlib.h>


START_TEST(queue_test)
{
	ck_assert_int_eq(test_queue(), 0);
}
END_TEST


START_TEST(queue_constructor_test)
{
	ck_assert_int_eq(test_queue_constructor(), 0);
}
END_TEST


START_TEST(queue_unique_test)
{
	ck_assert_int_eq(test_unique(), 0);
}
END_TEST


START_TEST(queue_shared_test)
{
	ck_assert_int_eq(test_shared(), 0);
}
END_TEST



START_TEST(queue_set_test)
{
	ck_assert_int_eq(test_queue_set(), 0);
}
END_TEST


START_TEST(queue_set_on_dup)
{
	ck_assert_int_eq(test_queue_set_on_dup(), 0);
}
END_TEST


Suite* testQueue(void) {
	Suite *s = suite_create("Test class Queue");

	TCase *tc_queue = tcase_create("Test Queue");
	tcase_add_test(tc_queue, queue_test);
	suite_add_tcase(s, tc_queue);

	TCase *tc_queue_const = tcase_create("Test Queue Constructor");
	tcase_add_test(tc_queue_const, queue_constructor_test);
	suite_add_tcase(s, tc_queue_const);

	TCase *tc_queue_unique = tcase_create("Test Queue with std::unique_ptr");
	tcase_add_test(tc_queue_unique, queue_unique_test);
	suite_add_tcase(s, tc_queue_unique);

	TCase *tc_queue_shared = tcase_create("Test Queue with std::shared_ptr");
	tcase_add_test(tc_queue_shared, queue_shared_test);
	suite_add_tcase(s, tc_queue_shared);

	return s;
}


Suite* testQueueSet(void) {
	Suite *s = suite_create("Test class QueueSet");

	TCase *tc_queue_set = tcase_create("Test QueueSet");
	tcase_add_test(tc_queue_set, queue_set_test);
	suite_add_tcase(s, tc_queue_set);

	TCase *tc_queue_set_on_dup = tcase_create("Test QueueSet on dup");
	tcase_add_test(tc_queue_set_on_dup, queue_set_on_dup);
	suite_add_tcase(s, tc_queue_set_on_dup);

	return s;
}


int main(void) {
	Suite *queue = testQueue();
	SRunner *sr = srunner_create(queue);
	srunner_run_all(sr, CK_NORMAL);
	int number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	Suite *queue_set = testQueueSet();
	sr = srunner_create(queue_set);
	srunner_run_all(sr, CK_NORMAL);
	number_failed += srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
