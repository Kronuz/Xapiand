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

#include "test_sort.h"

#include <check.h>


START_TEST(sort_test_string)
{
	ck_assert_int_eq(sort_test_string(), 0);
}
END_TEST


START_TEST(sort_test_numerical)
{
	ck_assert_int_eq(sort_test_numerical(), 0);
}
END_TEST


START_TEST(sort_test_date)
{
	ck_assert_int_eq(sort_test_date(), 0);
}
END_TEST


START_TEST(sort_test_boolean)
{
	ck_assert_int_eq(sort_test_boolean(), 0);
}
END_TEST


START_TEST(sort_test_geo)
{
	ck_assert_int_eq(sort_test_geo(), 0);
}
END_TEST


Suite* test_sort_string(void)
{
	Suite *s = suite_create("Test sort by field of type string");

	TCase *tc_sort_string = tcase_create("Field name with or without its value");
	tcase_add_test(tc_sort_string, sort_test_string);
	suite_add_tcase(s, tc_sort_string);

	return s;
}


Suite* test_sort_numerical(void)
{
	Suite *s = suite_create("Test sort by field of type numerical");

	TCase *tc_sort_numerical = tcase_create("Field name with or without its value");
	tcase_add_test(tc_sort_numerical, sort_test_numerical);
	suite_add_tcase(s, tc_sort_numerical);

	return s;
}


Suite* test_sort_date(void)
{
	Suite *s = suite_create("Test sort by field of type date");

	TCase *tc_sort_date = tcase_create("Field name with or without its value");
	tcase_add_test(tc_sort_date, sort_test_date);
	suite_add_tcase(s, tc_sort_date);

	return s;
}


Suite* test_sort_boolean(void)
{
	Suite *s = suite_create("Test sort by field of type boolean");

	TCase *tc_sort_boolean = tcase_create("Field name with or without its value");
	tcase_add_test(tc_sort_boolean, sort_test_boolean);
	suite_add_tcase(s, tc_sort_boolean);

	return s;
}


Suite* test_sort_geo(void)
{
	Suite *s = suite_create("Test sort by field of type geospatial");

	TCase *tc_sort_geo = tcase_create("Field name with or without its value");
	tcase_add_test(tc_sort_geo, sort_test_geo);
	suite_add_tcase(s, tc_sort_geo);

	return s;
}


int main(void)
{
	Suite *sort_string = test_sort_string();
	SRunner *sr = srunner_create(sort_string);
	srunner_run_all(sr, CK_NORMAL);
	int number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	Suite *sort_numerical = test_sort_numerical();
	sr = srunner_create(sort_numerical);
	srunner_run_all(sr, CK_NORMAL);
	number_failed += srunner_ntests_failed(sr);
	srunner_free(sr);

	Suite *sort_date = test_sort_date();
	sr = srunner_create(sort_date);
	srunner_run_all(sr, CK_NORMAL);
	number_failed += srunner_ntests_failed(sr);
	srunner_free(sr);

	Suite *sort_boolean = test_sort_boolean();
	sr = srunner_create(sort_boolean);
	srunner_run_all(sr, CK_NORMAL);
	number_failed += srunner_ntests_failed(sr);
	srunner_free(sr);

	Suite *sort_geo = test_sort_geo();
	sr = srunner_create(sort_geo);
	srunner_run_all(sr, CK_NORMAL);
	number_failed += srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}