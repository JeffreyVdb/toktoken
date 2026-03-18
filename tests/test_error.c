/*
 * test_error.c -- Unit tests for the error module.
 */

#include "test_framework.h"
#include "error.h"

TT_TEST(test_error_set_and_get)
{
    tt_error_clear();
    TT_ASSERT_EQ_STR(tt_error_get(), "");

    tt_error_set("file not found: %s", "/tmp/test");
    TT_ASSERT_EQ_STR(tt_error_get(), "file not found: /tmp/test");

    tt_error_clear();
    TT_ASSERT_EQ_STR(tt_error_get(), "");
}

TT_TEST(test_error_overwrite)
{
    tt_error_set("first error");
    tt_error_set("second error %d", 42);
    TT_ASSERT_EQ_STR(tt_error_get(), "second error 42");
    tt_error_clear();
}

void run_error_tests(void)
{
    TT_RUN(test_error_set_and_get);
    TT_RUN(test_error_overwrite);
}
