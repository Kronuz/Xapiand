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

#include "test_threadpool.h"

#include <check.h>
#include <stdlib.h>


START_TEST(threadpool_test)
{
	ck_assert_int_eq(test_pool(), 0);
}
END_TEST


START_TEST(threadpool_limit)
{
	ck_assert_int_eq(test_pool_limit(), 0);
}
END_TEST


START_TEST(test_threadpool_function)
{
	ck_assert_int_eq(test_pool_func(), 0);
}
END_TEST


START_TEST(test_threadpool_function_shared)
{
	ck_assert_int_eq(test_pool_func_shared(), 0);
}
END_TEST



START_TEST(test_threadpool_function_unique)
{
	ck_assert_int_eq(test_pool_func_unique(), 0);
}
END_TEST


Suite* testTheadpool(void) {
	Suite *s = suite_create("Test class Threadpool");

	TCase *tc_pool = tcase_create("Test ThreadPool::enqueue");
	tcase_set_timeout(tc_pool, 10);
	tcase_add_test(tc_pool, threadpool_test);
	suite_add_tcase(s, tc_pool);

	TCase *tc_pool_limit = tcase_create("Test ThreadPool's limit");
	tcase_set_timeout(tc_pool_limit, 10);
	tcase_add_test(tc_pool_limit, threadpool_limit);
	suite_add_tcase(s, tc_pool_limit);

	TCase *tc_pool_func_int = tcase_create("ThreadPool::enqueue functions with int");
	tcase_set_timeout(tc_pool_func_int, 10);
	tcase_add_test(tc_pool_func_int, test_threadpool_function);
	suite_add_tcase(s, tc_pool_func_int);

	TCase *tc_pool_func_shared = tcase_create("ThreadPool::enqueue functions with std::shared_ptr");
	tcase_set_timeout(tc_pool_func_shared, 10);
	tcase_add_test(tc_pool_func_shared, test_threadpool_function_shared);
	suite_add_tcase(s, tc_pool_func_shared);

	TCase *tc_pool_func_unique = tcase_create("ThreadPool::enqueue functions with std::unique_ptr");
	tcase_set_timeout(tc_pool_func_unique, 10);
	tcase_add_test(tc_pool_func_unique, test_threadpool_function_unique);
	suite_add_tcase(s, tc_pool_func_unique);

	return s;
}


int main(void) {
	Suite *testing_threadpool = testTheadpool();
	SRunner *sr = srunner_create(testing_threadpool);
	srunner_run_all(sr, CK_NORMAL);
	int number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
