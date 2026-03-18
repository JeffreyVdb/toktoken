/*
 * test_e2e_main.c -- Main runner for Phase 12 E2E tests.
 */

#include "test_framework.h"

TT_TEST_MAIN_VARS;

extern void run_e2e_index_tests(void);
extern void run_e2e_search_tests(void);
extern void run_e2e_inspect_tests(void);
extern void run_e2e_manage_tests(void);
extern void run_e2e_errors_tests(void);

int main(void)
{
    TT_SUITE("E2E Index");
    run_e2e_index_tests();

    TT_SUITE("E2E Search");
    run_e2e_search_tests();

    TT_SUITE("E2E Inspect");
    run_e2e_inspect_tests();

    TT_SUITE("E2E Manage");
    run_e2e_manage_tests();

    TT_SUITE("E2E Errors");
    run_e2e_errors_tests();

    TT_SUMMARY();
}
