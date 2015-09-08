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

#include <stdlib.h>
#include <stdint.h>

#include "test_geo.h"

#include <check.h>


START_TEST(geo_range_test)
{
	ck_assert_int_eq(geo_range_test(), 0);
}
END_TEST


START_TEST(geo_terms_test)
{
	ck_assert_int_eq(geo_terms_test(), 0);
}
END_TEST


Suite* range_search_geo(void)
{
	Suite *s = suite_create("Search by range in geospatials");
	TCase *t = tcase_create("Search by range");
	tcase_add_test(t, geo_range_test);
	suite_add_tcase(s, t);

	return s;
}


Suite* terms_search_geo(void)
{
	Suite *s = suite_create("Search by geospatials terms");
	TCase *t = tcase_create("Search by terms");
	tcase_add_test(t, geo_terms_test);
	suite_add_tcase(s, t);

	return s;
}


int main(void)
{
	Suite *range = range_search_geo();
	SRunner *sr = srunner_create(range);
	srunner_run_all(sr, CK_NORMAL);
	int number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	Suite *terms = terms_search_geo();
	sr = srunner_create(terms);
	srunner_run_all(sr, CK_NORMAL);
	number_failed += srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}