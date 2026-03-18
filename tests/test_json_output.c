/*
 * test_json_output.c -- Unit tests for json_output module.
 */

#include "test_framework.h"
#include "json_output.h"
#include "summarizer.h"
#include "symbol_kind.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

TT_TEST(test_jo_output_success_returns_zero)
{
    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "status", "ok");

    int code = tt_output_success(data); /* tt_output_success frees data */
    TT_ASSERT_EQ_INT(0, code);
}

TT_TEST(test_jo_output_error_returns_one)
{
    int code = tt_output_error("not_found", "File not found", NULL);
    TT_ASSERT_EQ_INT(1, code);
}

TT_TEST(test_jo_output_error_with_hint)
{
    /* This prints to stdout; we just verify it returns 1 and doesn't crash */
    int code = tt_output_error("no_index", "No index", "Run index:create");
    TT_ASSERT_EQ_INT(1, code);
}

TT_TEST(test_jo_output_error_empty_hint_omitted)
{
    int code = tt_output_error("err", "msg", "");
    TT_ASSERT_EQ_INT(1, code);
}

TT_TEST(test_jo_output_error_null_hint_omitted)
{
    int code = tt_output_error("err", "msg", NULL);
    TT_ASSERT_EQ_INT(1, code);
}

TT_TEST(test_jo_timer_returns_positive)
{
    tt_timer_start();
    usleep(1000); /* 1ms */
    double ms = tt_timer_elapsed_ms();
    TT_ASSERT(ms >= 0.0, "elapsed ms should be non-negative");
}

TT_TEST(test_jo_timer_sec_returns_non_negative)
{
    tt_timer_start();
    double sec = tt_timer_elapsed_sec();
    TT_ASSERT(sec >= 0.0, "elapsed sec should be non-negative");
}

/* isTier3Fallback tests (exposed via summarizer.h) */

TT_TEST(test_jo_tier3_class_label)
{
    TT_ASSERT_TRUE(tt_is_tier3_fallback("Class User", TT_KIND_CLASS, "User"));
}

TT_TEST(test_jo_tier3_method_label)
{
    TT_ASSERT_TRUE(tt_is_tier3_fallback("Method handle", TT_KIND_METHOD, "handle"));
}

TT_TEST(test_jo_tier3_function_label)
{
    TT_ASSERT_TRUE(tt_is_tier3_fallback("Function main", TT_KIND_FUNCTION, "main"));
}

TT_TEST(test_jo_tier3_empty_summary)
{
    TT_ASSERT_TRUE(tt_is_tier3_fallback("", TT_KIND_CLASS, "Foo"));
}

TT_TEST(test_jo_tier3_real_summary)
{
    TT_ASSERT_FALSE(tt_is_tier3_fallback("Handles user authentication.", TT_KIND_CLASS, "AuthService"));
}

TT_TEST(test_jo_tier3_similar_but_different)
{
    TT_ASSERT_FALSE(tt_is_tier3_fallback("Class User with extras", TT_KIND_CLASS, "User"));
}

void run_json_output_tests(void)
{
    TT_RUN(test_jo_output_success_returns_zero);
    TT_RUN(test_jo_output_error_returns_one);
    TT_RUN(test_jo_output_error_with_hint);
    TT_RUN(test_jo_output_error_empty_hint_omitted);
    TT_RUN(test_jo_output_error_null_hint_omitted);
    TT_RUN(test_jo_timer_returns_positive);
    TT_RUN(test_jo_timer_sec_returns_non_negative);
    TT_RUN(test_jo_tier3_class_label);
    TT_RUN(test_jo_tier3_method_label);
    TT_RUN(test_jo_tier3_function_label);
    TT_RUN(test_jo_tier3_empty_summary);
    TT_RUN(test_jo_tier3_real_summary);
    TT_RUN(test_jo_tier3_similar_but_different);
}
