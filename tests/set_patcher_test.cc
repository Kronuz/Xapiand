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

#include "test_patcher.h"

#include <check.h>
#include <stdlib.h>


START_TEST(test_patcher_mix)
{
	ck_assert_int_eq(test_mix(), 0);
}
END_TEST


START_TEST(test_patcher_add)
{
	ck_assert_int_eq(test_add(), 0);
}
END_TEST


START_TEST(test_patcher_remove)
{
	ck_assert_int_eq(test_remove(), 0);
}
END_TEST


START_TEST(test_patcher_replace)
{
	ck_assert_int_eq(test_remove(), 0);
}
END_TEST


START_TEST(test_patcher_move)
{
	ck_assert_int_eq(test_move(), 0);
}
END_TEST


START_TEST(test_patcher_copy)
{
	ck_assert_int_eq(test_copy(), 0);
}
END_TEST


START_TEST(test_patcher_test)
{
	ck_assert_int_eq(test_test(), 0);
}
END_TEST


START_TEST(test_patcher_incr)
{
	ck_assert_int_eq(test_incr(), 0);
}
END_TEST


START_TEST(test_patcher_decr)
{
	ck_assert_int_eq(test_decr(), 0);
}
END_TEST


Suite* testPatcher(void) {
	Suite *s = suite_create("Test patcher");

	TCase *t_mix = tcase_create("Test patcher miscellaneous");
	tcase_add_test(t_mix, test_patcher_mix);
	suite_add_tcase(s, t_mix);

	TCase *t_add = tcase_create("Test patcher add");
	tcase_add_test(t_add, test_patcher_add);
	suite_add_tcase(s, t_add);

	TCase *t_remove = tcase_create("Test patcher remove");
	tcase_add_test(t_remove, test_patcher_remove);
	suite_add_tcase(s, t_remove);

	TCase *t_replace = tcase_create("Test patcher replace");
	tcase_add_test(t_replace, test_patcher_replace);
	suite_add_tcase(s, t_replace);

	TCase *t_move = tcase_create("Test patcher move");
	tcase_add_test(t_move, test_patcher_move);
	suite_add_tcase(s, t_move);

	TCase *t_copy = tcase_create("Test patcher copy");
	tcase_add_test(t_copy, test_patcher_copy);
	suite_add_tcase(s, t_copy);

	TCase *t_test = tcase_create("Test patcher test");
	tcase_add_test(t_test, test_patcher_test);
	suite_add_tcase(s, t_test);

	TCase *t_incr = tcase_create("Test patcher increment");
	tcase_add_test(t_incr, test_patcher_incr);
	suite_add_tcase(s, t_incr);

	TCase *t_decr = tcase_create("Test patcher decrement");
	tcase_add_test(t_incr, test_patcher_decr);
	suite_add_tcase(s, t_decr);

	return s;
}


int main(void) {
	Suite *test_patcher = testPatcher();
	SRunner *sr = srunner_create(test_patcher);
	srunner_run_all(sr, CK_NORMAL);
	int number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
