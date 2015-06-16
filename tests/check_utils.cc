#include "../src/config.h"
#include "../src/utils.h"

#include <config.h>
#include <stdlib.h>
#include <stdint.h>
#include <check.h>

START_TEST(test_StartsWith)
{
    ck_assert(StartsWith("Hola mundo","Hola"));
}
END_TEST

Suite * utils_suite(void)
{

	Suite *s;
    TCase *tc_core;

    s = suite_create("utils");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_StartsWith);
    suite_add_tcase(s, tc_core);

    return s;

}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = utils_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
