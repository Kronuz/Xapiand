/*
 * Copyright (C) 2015, 2016 deipi.com LLC and contributors. All rights reserved.
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

#include "test_generate_terms.h"

#include <check.h>
#include <stdlib.h>


START_TEST(numeric_test)
{
	ck_assert_int_eq(numeric_test(), 0);
}
END_TEST


START_TEST(date_test)
{
	ck_assert_int_eq(date_test(), 0);
}
END_TEST


START_TEST(geo_test)
{
	ck_assert_int_eq(geo_test(), 0);
}
END_TEST


Suite* Generate_Numerical_Terms(void) {
	Suite *s = suite_create("Testing Generation of numerical terms");

	TCase *n = tcase_create("Generation numerical terms");
	tcase_add_test(n, numeric_test);
	suite_add_tcase(s, n);

	return s;
}


Suite* Generate_Date_Terms(void) {
	Suite *s = suite_create("Testing Generation of terms for dates");

	TCase *d = tcase_create("Generation of terms for dates");
	tcase_add_test(d, date_test);
	suite_add_tcase(s, d);

	return s;
}


Suite* Generate_Geo_Terms(void) {
	Suite *s = suite_create("Testing Generation of terms for geospatials");

	TCase *g = tcase_create("Generation of terms for geospatials");
	tcase_add_test(g, geo_test);
	suite_add_tcase(s, g);

	return s;
}


int main(void) {
	Suite *s_nt = Generate_Numerical_Terms();
	SRunner *sr = srunner_create(s_nt);
	srunner_run_all(sr, CK_NORMAL);
	int number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	Suite *s_dt = Generate_Date_Terms();
	sr = srunner_create(s_dt);
	srunner_run_all(sr, CK_NORMAL);
	number_failed += srunner_ntests_failed(sr);
	srunner_free(sr);

	Suite *s_gt = Generate_Geo_Terms();
	sr = srunner_create(s_gt);
	srunner_run_all(sr, CK_NORMAL);
	number_failed += srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
