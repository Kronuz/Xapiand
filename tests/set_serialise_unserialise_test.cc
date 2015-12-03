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

#include "test_serialise.h"

#include <check.h>
#include <stdlib.h>


START_TEST(test_datetotimestamp)
{
	ck_assert_int_eq(test_datetotimestamp(), 0);
}
END_TEST


START_TEST(test_unserialise_date)
{
	ck_assert_int_eq(test_unserialise_date(), 0);
}
END_TEST


START_TEST(test_serialise_cartesian)
{
	ck_assert_int_eq(test_serialise_cartesian(), 0);
}
END_TEST


START_TEST(test_unserialise_cartesian)
{
	ck_assert_int_eq(test_unserialise_cartesian(), 0);
}
END_TEST


START_TEST(test_serialise_trixel_id)
{
	ck_assert_int_eq(test_serialise_trixel_id(), 0);
}
END_TEST


START_TEST(test_unserialise_trixel_id)
{
	ck_assert_int_eq(test_unserialise_trixel_id(), 0);
}
END_TEST


Suite* test_suite_serialise(void) {
	Suite *s = suite_create("Tests of serialise");

	TCase *tc_datetotimestamp = tcase_create("Serialise Date");
	tcase_add_test(tc_datetotimestamp, test_datetotimestamp);
	suite_add_tcase(s, tc_datetotimestamp);

	TCase *tc_serialise_cartesian = tcase_create("Serialise Cartesian");
	tcase_add_test(tc_serialise_cartesian, test_serialise_cartesian);
	suite_add_tcase(s, tc_serialise_cartesian);

	TCase *tc_serialise_trixel_id = tcase_create("Serialise HTM trixel's id");
	tcase_add_test(tc_serialise_trixel_id, test_serialise_trixel_id);
	suite_add_tcase(s, tc_serialise_trixel_id);

	return s;
}


Suite* test_suite_unserialise(void) {
	Suite *s = suite_create("Tests of unserialise");

	TCase *tc_unserialise_date = tcase_create("Unserialise date");
	tcase_add_test(tc_unserialise_date, test_unserialise_date);
	suite_add_tcase(s, tc_unserialise_date);

	TCase *tc_unserialise_cartesian = tcase_create("Unserialise cartesian");
	tcase_add_test(tc_unserialise_cartesian, test_unserialise_cartesian);
	suite_add_tcase(s, tc_unserialise_cartesian);

	TCase *tc_unserialise_trixel_id = tcase_create("Unserialise HTM trixel's id");
	tcase_add_test(tc_unserialise_trixel_id, test_unserialise_trixel_id);
	suite_add_tcase(s, tc_unserialise_trixel_id);

	return s;
}


int main(void) {
	Suite *serialise = test_suite_serialise();
	SRunner *sr = srunner_create(serialise);
	srunner_run_all(sr, CK_NORMAL);
	int number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	Suite *unserialise = test_suite_unserialise();
	sr = srunner_create(unserialise);
	srunner_run_all(sr, CK_NORMAL);
	number_failed += srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
