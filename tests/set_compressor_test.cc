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

#include "test_compressor.h"

#include <check.h>
#include <stdlib.h>


START_TEST(test_small_datas)
{
	ck_assert_int_eq(test_small_datas(), 0);
}
END_TEST


START_TEST(test_big_datas)
{
	ck_assert_int_eq(test_big_datas(), 0);
}
END_TEST


START_TEST(test_small_files)
{
	ck_assert_int_eq(test_small_files(), 0);
}
END_TEST


START_TEST(test_big_files)
{
	ck_assert_int_eq(test_big_files(), 0);
}
END_TEST


Suite* compress_data(void) {
	Suite *s = suite_create("Testing LZ4CompressData");

	TCase *small = tcase_create("Small datas");
	tcase_add_test(small, test_small_datas);
	suite_add_tcase(s, small);

	TCase *big = tcase_create("Big datas");
	tcase_add_test(big, test_big_datas);
	suite_add_tcase(s, big);

	return s;
}


Suite* compress_file(void) {
	Suite *s = suite_create("Testing LZ4CompressFile");

	TCase *small = tcase_create("Small datas");
	tcase_add_test(small, test_small_files);
	suite_add_tcase(s, small);

	TCase *big = tcase_create("Big datas");
	tcase_add_test(big, test_big_files);
	suite_add_tcase(s, big);

	return s;
}


int main(void) {
	Suite *cmp_data = compress_data();
	SRunner *sr = srunner_create(cmp_data);
	srunner_run_all(sr, CK_NORMAL);
	int number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	Suite *cmp_file = compress_file();
	sr = srunner_create(cmp_file);
	srunner_run_all(sr, CK_NORMAL);
	number_failed += srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
