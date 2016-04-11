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


START_TEST(test_msgpack_cpp)
{
	ck_assert_int_eq(test_correct_cpp(), 0);
}
END_TEST


START_TEST(test_msgpack_pack)
{
	ck_assert_int_eq(test_pack(), 0);
}
END_TEST


START_TEST(test_msgpack_unpack)
{
	ck_assert_int_eq(test_unpack(), 0);
}
END_TEST


START_TEST(test_msgpack_explore_json)
{
	ck_assert_int_eq(test_explore_json(), 0);
}
END_TEST


START_TEST(test_msgpack_add_items)
{
	ck_assert_int_eq(test_add_items(), 0);
}
END_TEST


START_TEST(test_msgpack_assigment)
{
	ck_assert_int_eq(test_assigment(), 0);
}
END_TEST


START_TEST(test_msgpack_path)
{
	ck_assert_int_eq(test_path(), 0);
}
END_TEST


START_TEST(test_msgpack_clone)
{
	ck_assert_int_eq(test_clone(), 0);
}
END_TEST


START_TEST(test_msgpack_erase)
{
	ck_assert_int_eq(test_erase(), 0);
}
END_TEST


START_TEST(test_msgpack_reserve)
{
	ck_assert_int_eq(test_reserve(), 0);
}
END_TEST


START_TEST(test_msgpack_reset)
{
	ck_assert_int_eq(test_reset(), 0);
}
END_TEST


START_TEST(test_msgpack_explicit_constructors)
{
	ck_assert_int_eq(test_explicit_constructors(), 0);
}
END_TEST


Suite* testMsgPack(void) {
	Suite *s = suite_create("Test MsgPack");

	TCase *tc_cpp = tcase_create("Test version of cpp");
	tcase_add_test(tc_cpp, test_msgpack_cpp);
	suite_add_tcase(s, tc_cpp);

	TCase *tc_pack = tcase_create("Test pack");
	tcase_add_test(tc_pack, test_msgpack_pack);
	suite_add_tcase(s, tc_pack);

	TCase *tc_unpack = tcase_create("Test unpack");
	tcase_add_test(tc_unpack, test_msgpack_unpack);
	suite_add_tcase(s, tc_unpack);

	TCase *tc_explore_json = tcase_create("Test explore json");
	tcase_add_test(tc_explore_json, test_msgpack_explore_json);
	suite_add_tcase(s, tc_explore_json);

	TCase *tc_add_items = tcase_create("Test add items");
	tcase_add_test(tc_add_items, test_msgpack_add_items);
	suite_add_tcase(s, tc_add_items);

	TCase *tc_assigment = tcase_create("Test assigment");
	tcase_add_test(tc_assigment, test_msgpack_assigment);
	suite_add_tcase(s, tc_assigment);

	TCase *tc_path = tcase_create("Test path");
	tcase_add_test(tc_path, test_msgpack_path);
	suite_add_tcase(s, tc_path);

	TCase *tc_clone = tcase_create("Test Clone");
	tcase_add_test(tc_clone, test_msgpack_clone);
	suite_add_tcase(s, tc_clone);

	TCase *tc_erase = tcase_create("Test Erase");
	tcase_add_test(tc_erase, test_msgpack_erase);
	suite_add_tcase(s, tc_erase);

	TCase *tc_reserve = tcase_create("Test Reserve");
	tcase_add_test(tc_reserve, test_msgpack_reserve);
	suite_add_tcase(s, tc_reserve);

	TCase *tc_reset = tcase_create("Test Reset");
	tcase_add_test(tc_reset, test_msgpack_reset);
	suite_add_tcase(s, tc_reset);

	TCase *tc_explicit_constructors = tcase_create("Test Explicit Constructors");
	tcase_add_test(tc_explicit_constructors, test_msgpack_explicit_constructors);
	suite_add_tcase(s, tc_explicit_constructors);

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
