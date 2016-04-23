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

#include "test_msgpack.h"

#include <check.h>
#include <stdlib.h>


START_TEST(test_MsgPack_correct_cpp)
{
	ck_assert_int_eq(test_correct_cpp(), 0);
}
END_TEST


START_TEST(test_MsgPack_constructors)
{
	ck_assert_int_eq(test_constructors(), 0);
}
END_TEST


START_TEST(test_MsgPack_assigment)
{
	ck_assert_int_eq(test_assigment(), 0);
}
END_TEST


START_TEST(test_MsgPack_iterator)
{
	ck_assert_int_eq(test_iterator(), 0);
}
END_TEST


START_TEST(test_MsgPack_serialise)
{
	ck_assert_int_eq(test_serialise(), 0);
}
END_TEST


START_TEST(test_MsgPack_unserialise)
{
	ck_assert_int_eq(test_unserialise(), 0);
}
END_TEST


START_TEST(test_MsgPack_explore)
{
	ck_assert_int_eq(test_explore(), 0);
}
END_TEST


START_TEST(test_MsgPack_copy)
{
	ck_assert_int_eq(test_copy(), 0);
}
END_TEST


START_TEST(test_MsgPack_reference)
{
	ck_assert_int_eq(test_reference(), 0);
}
END_TEST


START_TEST(test_MsgPack_path)
{
	ck_assert_int_eq(test_path(), 0);
}
END_TEST


START_TEST(test_MsgPack_erase)
{
	ck_assert_int_eq(test_erase(), 0);
}
END_TEST


START_TEST(test_MsgPack_reserve)
{
	ck_assert_int_eq(test_reserve(), 0);
}
END_TEST


START_TEST(test_MsgPack_keys)
{
	ck_assert_int_eq(test_keys(), 0);
}
END_TEST


START_TEST(test_MsgPack_change_keys)
{
	ck_assert_int_eq(test_change_keys(), 0);
}
END_TEST


Suite* testMsgPack(void) {
	Suite *s = suite_create("Test MsgPack");

	TCase *tc_cpp = tcase_create("Test version of cpp");
	tcase_add_test(tc_cpp, test_MsgPack_correct_cpp);
	suite_add_tcase(s, tc_cpp);

	TCase *tc_explicit_constructors = tcase_create("Test Constructors");
	tcase_add_test(tc_explicit_constructors, test_MsgPack_constructors);
	suite_add_tcase(s, tc_explicit_constructors);

	TCase *tc_assigment = tcase_create("Test assigment");
	tcase_add_test(tc_assigment, test_MsgPack_assigment);
	suite_add_tcase(s, tc_assigment);

	TCase *tc_iterator = tcase_create("Test iterator");
	tcase_add_test(tc_iterator, test_MsgPack_iterator);
	suite_add_tcase(s, tc_iterator);

	TCase *tc_serialise = tcase_create("Test serialise");
	tcase_add_test(tc_serialise, test_MsgPack_serialise);
	suite_add_tcase(s, tc_serialise);

	TCase *tc_unserialise = tcase_create("Test unserialise");
	tcase_add_test(tc_unserialise, test_MsgPack_unserialise);
	suite_add_tcase(s, tc_unserialise);

	TCase *tc_explore = tcase_create("Test explore MAP");
	tcase_add_test(tc_explore, test_MsgPack_explore);
	suite_add_tcase(s, tc_explore);

	TCase *tc_copy = tcase_create("Test Copy");
	tcase_add_test(tc_copy, test_MsgPack_copy);
	suite_add_tcase(s, tc_copy);

	TCase *tc_reference = tcase_create("Test Reference");
	tcase_add_test(tc_reference, test_MsgPack_reference);
	suite_add_tcase(s, tc_reference);

	TCase *tc_path = tcase_create("Test path");
	tcase_add_test(tc_path, test_MsgPack_path);
	suite_add_tcase(s, tc_path);

	TCase *tc_erase = tcase_create("Test Erase");
	tcase_add_test(tc_erase, test_MsgPack_erase);
	suite_add_tcase(s, tc_erase);

	TCase *tc_reserve = tcase_create("Test Reserve");
	tcase_add_test(tc_reserve, test_MsgPack_reserve);
	suite_add_tcase(s, tc_reserve);

	TCase *tc_keys = tcase_create("Test Keys");
	tcase_add_test(tc_keys, test_MsgPack_keys);
	suite_add_tcase(s, tc_keys);

	TCase *tc_change_keys = tcase_create("Test Change values of keys");
	tcase_add_test(tc_change_keys, test_MsgPack_change_keys);
	suite_add_tcase(s, tc_change_keys);

	return s;
}


int main(void) {
	Suite *test_MsgPack = testMsgPack();
	SRunner *sr = srunner_create(test_MsgPack);
	srunner_run_all(sr, CK_NORMAL);
	int number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
