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

#include "test_hash.h"

#include <check.h>
#include <stdlib.h>


START_TEST(test_md5)
{
	ck_assert_int_eq(test_md5(), 0);
}
END_TEST


START_TEST(test_sha256)
{
	ck_assert_int_eq(test_sha256(), 0);
}
END_TEST


Suite* test_suite_single_thread(void) {
	Suite *s = suite_create("Test of hash MD5 and SHA256");

	TCase *tc_md5 = tcase_create("Test MD5");
	tcase_add_test(tc_md5, test_md5);
	suite_add_tcase(s, tc_md5);

	TCase *tc_sha256 = tcase_create("Test SHA256");
	tcase_add_test(tc_sha256, test_sha256);
	suite_add_tcase(s, tc_sha256);

	return s;
}


int main(void) {
	Suite *single_thread = test_suite_single_thread();
	SRunner *sr = srunner_create(single_thread);
	srunner_run_all(sr, CK_NORMAL);
	int number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
