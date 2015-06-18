#include "../src/config.h"
#include "../src/utils.h"
#include "../src/tests.h"


#include <config.h>
#include <stdlib.h>
#include <stdint.h>
#include <check.h>

START_TEST(test_StartsWith)
{
    ck_assert(StartsWith("Hola mundo","Hola"));
}
END_TEST

START_TEST(test_cartesian_transforms)
{
    ck_assert_int_eq(test_cartesian_transforms(),0);
}
END_TEST


Suite * utils_suite(void)
{

	Suite *s;
    TCase *tc_StartsWith;
    TCase *tc_cartesian;

    s = suite_create("utils");

    tc_StartsWith = tcase_create("StartsWith");
    tcase_add_test(tc_StartsWith, test_StartsWith);
    suite_add_tcase(s, tc_StartsWith);


    tc_cartesian = tcase_create("Cartesian");
    tcase_add_test(tc_cartesian, test_cartesian_transforms);
    suite_add_tcase(s, tc_cartesian);


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
