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

#include "test_htm.h"

#include <check.h>


START_TEST(test_cartesian_transforms)
{
	ck_assert_int_eq(test_cartesian_transforms(), 0);
}
END_TEST


START_TEST(test_hullConvex)
{
	ck_assert_int_eq(test_hullConvex(), 0);
}
END_TEST


START_TEST(test_HTM_chull)
{
	ck_assert_int_eq(test_HTM_chull(), 0);
}
END_TEST


START_TEST(test_HTM_circle)
{
	ck_assert_int_eq(test_HTM_circle(), 0);
}
END_TEST


Suite* test_suite_cartesian(void) {
	Suite *s = suite_create("Test of transformation of coordinates between CRS");

	TCase *tc_cartesian_transforms = tcase_create("Transformation of coordinates between CRS");
	tcase_add_test(tc_cartesian_transforms, test_cartesian_transforms);
	suite_add_tcase(s, tc_cartesian_transforms);

	return s;
}


Suite* test_suite_convex_hull(void) {
	Suite *s = suite_create("Test of Geometry Hull Convex");

	TCase *tc_hull_convex = tcase_create("Convex hull from a set point");
	tcase_add_test(tc_hull_convex, test_hullConvex);
	suite_add_tcase(s, tc_hull_convex);

	return s;
}


Suite* test_suite_htm(void) {
	Suite *s = suite_create("Test of HTM");

	TCase *tc_htm_chull = tcase_create("HTM for Polygons");
	tcase_set_timeout(tc_htm_chull, 10);
	tcase_add_test(tc_htm_chull, test_HTM_chull);
	suite_add_tcase(s, tc_htm_chull);

	TCase *tc_htm_circle = tcase_create("HTM for Bounding Circles");
	tcase_set_timeout(tc_htm_circle, 10);
	tcase_add_test(tc_htm_circle, test_HTM_circle);
	suite_add_tcase(s, tc_htm_circle);

	return s;
}


int main(void) {
	Suite *cartesian = test_suite_cartesian();
	SRunner *sr = srunner_create(cartesian);
	srunner_run_all(sr, CK_NORMAL);
	int number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	Suite *convex_hull = test_suite_convex_hull();
	sr = srunner_create(convex_hull);
	srunner_run_all(sr, CK_NORMAL);
	number_failed += srunner_ntests_failed(sr);
	srunner_free(sr);

	Suite *htm = test_suite_htm();
	sr = srunner_create(htm);
	srunner_run_all(sr, CK_NORMAL);
	number_failed += srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
