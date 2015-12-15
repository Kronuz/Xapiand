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

#include "test_lru.h"

#include <check.h>
#include <stdlib.h>


START_TEST(test_lru)
{
	ck_assert_int_eq(test_lru(), 0);
}
END_TEST


START_TEST(test_lru_emplace)
{
	ck_assert_int_eq(test_lru_emplace(), 0);
}
END_TEST


START_TEST(test_lru_actions)
{
	ck_assert_int_eq(test_lru_actions(), 0);
}
END_TEST


START_TEST(test_lru_mutate)
{
	ck_assert_int_eq(test_lru_mutate(), 0);
}
END_TEST


Suite* test_LRU(void) {
	Suite *s = suite_create("LRU Test");

	TCase *tc_lru = tcase_create("Test LRU");
	tcase_add_test(tc_lru, test_lru);
	suite_add_tcase(s, tc_lru);

	TCase *tc_lru_emplace = tcase_create("Test LRU emplace");
	tcase_add_test(tc_lru_emplace, test_lru_emplace);
	suite_add_tcase(s, tc_lru_emplace);

	TCase *tc_lru_actions = tcase_create("Test LRU actions");
	tcase_add_test(tc_lru_actions, test_lru_actions);
	suite_add_tcase(s, tc_lru_actions);

	TCase *tc_lru_mutate = tcase_create("Test LRU mutate");
	tcase_add_test(tc_lru_mutate, test_lru_mutate);
	suite_add_tcase(s, tc_lru_mutate);

	return s;
}


int main(void) {
	Suite *LRU = test_LRU();
	SRunner *sr = srunner_create(LRU);
	srunner_run_all(sr, CK_NORMAL);
	int number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
