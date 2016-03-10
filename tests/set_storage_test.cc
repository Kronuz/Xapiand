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

#include "test_storage.h"

#include "../src/storage.h"

#include <check.h>
#include <stdlib.h>


START_TEST(test_storage_datas)
{
	ck_assert_int_eq(test_storage_data(), 0);
}
END_TEST


START_TEST(test_storage_datas_cmp)
{
	ck_assert_int_eq(test_storage_data(STORAGE_COMPRESS), 0);
}
END_TEST


START_TEST(test_storage_files)
{
	ck_assert_int_eq(test_storage_file(), 0);
}
END_TEST


START_TEST(test_storage_files_cmp)
{
	ck_assert_int_eq(test_storage_file(STORAGE_COMPRESS), 0);
}
END_TEST


Suite* storage_datas(void) {
	Suite *s = suite_create("Testing Storage for datas");

	TCase *datas = tcase_create("Storage::write(const std::string& data, void* param=nullptr) without compress");
	tcase_add_test(datas, test_storage_datas);
	suite_add_tcase(s, datas);

	TCase *cmp_datas = tcase_create("Storage::write(const std::string& data, void* param=nullptr) with compress");
	tcase_add_test(cmp_datas, test_storage_datas_cmp);
	suite_add_tcase(s, cmp_datas);

	return s;
}


Suite* storage_files(void) {
	Suite *s = suite_create("Testing Storage for files");

	TCase *files = tcase_create("Storage::write_file(const std::string& filename, void* param=nullptr) without compress");
	tcase_add_test(files, test_storage_files);
	suite_add_tcase(s, files);

	TCase *cmp_files = tcase_create("Storage::write_file(const std::string& filename, void* param=nullptr) with compress");
	tcase_add_test(cmp_files, test_storage_files_cmp);
	suite_add_tcase(s, cmp_files);

	return s;
}


int main(void) {
	Suite *sto_data = storage_datas();
	SRunner *sr = srunner_create(sto_data);
	srunner_run_all(sr, CK_NORMAL);
	int number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
