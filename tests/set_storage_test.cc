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


START_TEST(test_StorageBadHeader)
{
	ck_assert_int_eq(test_storage_bad_headers(), 0);
}
END_TEST


START_TEST(test_StorageExceptionWrite)
{
	ck_assert_int_eq(test_storage_exception_write(), 0);
}
END_TEST


START_TEST(test_StorageExceptionWrite_cmp)
{
	ck_assert_int_eq(test_storage_exception_write(STORAGE_COMPRESS), 0);
}
END_TEST


START_TEST(test_StorageExceptionWriteFile)
{
	ck_assert_int_eq(test_storage_exception_write_file(), 0);
}
END_TEST


START_TEST(test_StorageExceptionWriteFile_cmp)
{
	ck_assert_int_eq(test_storage_exception_write_file(STORAGE_COMPRESS), 0);
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


Suite* storage_bad_header(void) {
	Suite *s = suite_create("Testing Storage for bad headers");

	TCase *bad_header = tcase_create("Storage bad headers");
	tcase_add_test(bad_header, test_StorageBadHeader);
	suite_add_tcase(s, bad_header);

	return s;
}


Suite* storage_exception_write(void) {
	Suite *s = suite_create("Testing Storage::write with exceptions");

	TCase *datas = tcase_create("Exceptions when Storage::write(const std::string& data, void* param=nullptr) is running");
	tcase_add_test(datas, test_StorageExceptionWrite);
	suite_add_tcase(s, datas);

	TCase *cmp_datas = tcase_create("Exceptions when Storage::write(const std::string& data, void* param=nullptr) with compress is running");
	tcase_add_test(cmp_datas, test_StorageExceptionWrite_cmp);
	suite_add_tcase(s, cmp_datas);

	return s;
}


Suite* storage_exception_write_file(void) {
	Suite *s = suite_create("Testing Storage::write_file with exceptions");

	TCase *files = tcase_create("Exceptions when Storage::write_file(const std::string& filename, void* param=nullptr) is running");
	tcase_add_test(files, test_StorageExceptionWriteFile);
	suite_add_tcase(s, files);

	TCase *cmp_files = tcase_create("Exceptions when Storage::write(const std::string& filename, void* param=nullptr) with compress is running");
	tcase_add_test(cmp_files, test_StorageExceptionWriteFile_cmp);
	suite_add_tcase(s, cmp_files);

	return s;
}


int main(void) {
	Suite *sto_data = storage_datas();
	SRunner *sr = srunner_create(sto_data);
	srunner_run_all(sr, CK_NORMAL);
	int number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	Suite *sto_file = storage_files();
	sr = srunner_create(sto_file);
	srunner_run_all(sr, CK_NORMAL);
	number_failed += srunner_ntests_failed(sr);
	srunner_free(sr);

	Suite *sto_bad_header = storage_bad_header();
	sr = srunner_create(sto_bad_header);
	srunner_run_all(sr, CK_NORMAL);
	number_failed += srunner_ntests_failed(sr);
	srunner_free(sr);

	Suite *sto_exceptions = storage_exception_write();
	sr = srunner_create(sto_exceptions);
	srunner_run_all(sr, CK_NORMAL);
	number_failed += srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
