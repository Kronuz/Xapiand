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

#include "test_query.h"

#include <check.h>


START_TEST(test_query_search)
{
	ck_assert_int_eq(test_query_search(), 0);
}
END_TEST


START_TEST(test_terms_search)
{
	ck_assert_int_eq(test_terms_search(), 0);
}
END_TEST


START_TEST(test_partials_search)
{
	ck_assert_int_eq(test_partials_search(), 0);
}
END_TEST


START_TEST(test_facets_search)
{
	ck_assert_int_eq(test_facets_search(), 0);
}
END_TEST


Suite* test_query(void)
{
	Suite *s = suite_create("Search Test");

	TCase *q = tcase_create("Test for query");
	tcase_add_test(q, test_query_search);
	suite_add_tcase(s, q);

	TCase *t = tcase_create("Test for terms");
	tcase_add_test(t, test_terms_search);
	suite_add_tcase(s, t);

	TCase *p = tcase_create("Test for partials");
	tcase_add_test(p, test_partials_search);
	suite_add_tcase(s, p);

	TCase *f = tcase_create("Test for facets");
	tcase_add_test(f, test_facets_search);
	suite_add_tcase(s, f);

	return s;
}


int main(void)
{
	Suite *query = test_query();
	SRunner *sr = srunner_create(query);
	srunner_run_all(sr, CK_NORMAL);
	int number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}