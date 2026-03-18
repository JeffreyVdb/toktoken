/*
 * test_normalizer.c -- Unit tests for normalizer module.
 */

#include "test_framework.h"
#include "normalizer.h"

#include <string.h>

TT_TEST(test_norm_language_sh)
{
    TT_ASSERT_EQ_STR("bash", tt_normalize_language("Sh"));
}

TT_TEST(test_norm_language_cpp)
{
    TT_ASSERT_EQ_STR("cpp", tt_normalize_language("C++"));
}

TT_TEST(test_norm_language_csharp)
{
    TT_ASSERT_EQ_STR("csharp", tt_normalize_language("C#"));
}

TT_TEST(test_norm_language_elisp)
{
    TT_ASSERT_EQ_STR("elisp", tt_normalize_language("EmacsLisp"));
}

TT_TEST(test_norm_language_verilog)
{
    TT_ASSERT_EQ_STR("verilog", tt_normalize_language("SystemVerilog"));
}

TT_TEST(test_norm_language_passthrough)
{
    TT_ASSERT_EQ_STR("php", tt_normalize_language("PHP"));
}

TT_TEST(test_norm_language_case_insensitive)
{
    TT_ASSERT_EQ_STR("bash", tt_normalize_language("SH"));
    TT_ASSERT_EQ_STR("bash", tt_normalize_language("sh"));
    TT_ASSERT_EQ_STR("cpp", tt_normalize_language("c++"));
}

TT_TEST(test_norm_language_empty_string)
{
    TT_ASSERT_EQ_STR("unknown", tt_normalize_language(""));
}

TT_TEST(test_norm_language_unknown_passthrough)
{
    TT_ASSERT_EQ_STR("fortran", tt_normalize_language("Fortran"));
}

TT_TEST(test_norm_kind_canonical)
{
    TT_ASSERT_EQ_STR("method",   tt_normalize_kind("method"));
    TT_ASSERT_EQ_STR("class",    tt_normalize_kind("class"));
    TT_ASSERT_EQ_STR("function", tt_normalize_kind("Function"));
}

TT_TEST(test_norm_kind_unknown_passthrough)
{
    TT_ASSERT_EQ_STR("func",         tt_normalize_kind("func"));
    TT_ASSERT_EQ_STR("unknown_kind", tt_normalize_kind("unknown_kind"));
}

TT_TEST(test_norm_kind_empty_string)
{
    /* Empty string returns the same empty pointer */
    const char *result = tt_normalize_kind("");
    TT_ASSERT_EQ_STR("", result);
}

void run_normalizer_tests(void)
{
    TT_RUN(test_norm_language_sh);
    TT_RUN(test_norm_language_cpp);
    TT_RUN(test_norm_language_csharp);
    TT_RUN(test_norm_language_elisp);
    TT_RUN(test_norm_language_verilog);
    TT_RUN(test_norm_language_passthrough);
    TT_RUN(test_norm_language_case_insensitive);
    TT_RUN(test_norm_language_empty_string);
    TT_RUN(test_norm_language_unknown_passthrough);
    TT_RUN(test_norm_kind_canonical);
    TT_RUN(test_norm_kind_unknown_passthrough);
    TT_RUN(test_norm_kind_empty_string);
}
