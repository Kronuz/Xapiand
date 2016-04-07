/*
 * Copyright (C) 2016 deipi.com LLC and contributors. All rights reserved.
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

#include "test_serialise_stl.h"

#include <check.h>
#include <stdlib.h>


START_TEST(StringList_test)
{
	ck_assert_int_eq(test_StringList(), 0);
}
END_TEST


START_TEST(StringSet_test)
{
	ck_assert_int_eq(test_StringSet(), 0);
}
END_TEST


START_TEST(CartesianUSet_test)
{
	ck_assert_int_eq(test_CartesianUSet(), 0);
}
END_TEST


START_TEST(RangeList_test)
{
	ck_assert_int_eq(test_RangeList(), 0);
}
END_TEST


Suite* SerialiseUnserialiseSTL(void) {
	Suite *s = suite_create("Testing Serialise and Unserialise of STL");

	TCase *sl = tcase_create("Testing Serialise and Unserialise of StringList");
	tcase_add_test(sl, StringList_test);
	suite_add_tcase(s, sl);

	TCase *ss = tcase_create("Testing Serialise and Unserialise of StringSet");
	tcase_add_test(ss, StringSet_test);
	suite_add_tcase(s, ss);

	TCase *c_uset = tcase_create("Testing Serialise and Unserialise of CartesianUSet");
	tcase_add_test(c_uset, CartesianUSet_test);
	suite_add_tcase(s, c_uset);

	TCase *rl = tcase_create("Testing Serialise and Unserialise of RangeList");
	tcase_add_test(rl, RangeList_test);
	suite_add_tcase(s, rl);

	return s;
}


int main(void) {
	Suite *st = SerialiseUnserialiseSTL();
	SRunner *sr = srunner_create(st);
	srunner_run_all(sr, CK_NORMAL);
	int number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
