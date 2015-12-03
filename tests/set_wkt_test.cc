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

#include "test_wkt_parser.h"

#include <check.h>
#include <stdlib.h>


START_TEST(test_wkt_parser)
{
	ck_assert_int_eq(test_wkt_parser(), 0);
}
END_TEST


START_TEST(test_wkt_speed)
{
	ck_assert_int_eq(test_wkt_speed(), 0);
}
END_TEST


Suite* test_suite_wkt(void) {
	Suite *s = suite_create("Test of WKT");

	TCase *tc_wkt_parser = tcase_create("WKT parser");
	tcase_set_timeout(tc_wkt_parser, 10);
	tcase_add_test(tc_wkt_parser, test_wkt_parser);
	suite_add_tcase(s, tc_wkt_parser);

	TCase *tc_wkt_speed = tcase_create("WKT speed");
	tcase_set_timeout(tc_wkt_speed, 10);
	tcase_add_test(tc_wkt_speed, test_wkt_speed);
	suite_add_tcase(s, tc_wkt_speed);

	return s;
}


int main(void) {
	Suite *wkt = test_suite_wkt();
	SRunner *sr = srunner_create(wkt);
	srunner_run_all(sr, CK_NORMAL);
	int number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
