/*
 * test_symbol.c -- Unit tests for symbol module (ID generation, free).
 */

#include "test_framework.h"
#include "symbol.h"

#include <stdlib.h>
#include <string.h>

TT_TEST(test_symbol_make_id_basic)
{
    char *id = tt_symbol_make_id("src/Auth.php", "Auth.login", "method", 0);
    TT_ASSERT_NOT_NULL(id);
    TT_ASSERT_EQ_STR("src/Auth.php::Auth.login#method", id);
    free(id);
}

TT_TEST(test_symbol_make_id_with_overload)
{
    char *id = tt_symbol_make_id("src/Auth.php", "Auth.login", "method", 2);
    TT_ASSERT_NOT_NULL(id);
    TT_ASSERT_EQ_STR("src/Auth.php::Auth.login#method~2", id);
    free(id);
}

TT_TEST(test_symbol_make_id_zero_overload_no_suffix)
{
    char *id = tt_symbol_make_id("src/Auth.php", "Auth.login", "method", 0);
    TT_ASSERT_STR_NOT_CONTAINS(id, "~");
    free(id);
}

TT_TEST(test_symbol_make_id_negative_overload_ignored)
{
    char *id = tt_symbol_make_id("f.php", "Foo", "class", -1);
    TT_ASSERT_NOT_NULL(id);
    TT_ASSERT_EQ_STR("f.php::Foo#class", id);
    free(id);
}

TT_TEST(test_symbol_make_id_high_overload)
{
    char *id = tt_symbol_make_id("f.php", "overloaded", "function", 99);
    TT_ASSERT_NOT_NULL(id);
    TT_ASSERT_EQ_STR("f.php::overloaded#function~99", id);
    free(id);
}

TT_TEST(test_symbol_make_id_special_chars_in_path)
{
    char *id = tt_symbol_make_id("src/My File (1).php", "Foo.bar", "method", 0);
    TT_ASSERT_NOT_NULL(id);
    TT_ASSERT_EQ_STR("src/My File (1).php::Foo.bar#method", id);
    free(id);
}

TT_TEST(test_symbol_make_id_namespace_backslash)
{
    char *id = tt_symbol_make_id("src/Auth.php", "App\\Auth.login", "method", 0);
    TT_ASSERT_NOT_NULL(id);
    TT_ASSERT_EQ_STR("src/Auth.php::App\\Auth.login#method", id);
    free(id);
}

TT_TEST(test_symbol_free_handles_nulls)
{
    tt_symbol_t sym;
    memset(&sym, 0, sizeof(sym));
    tt_symbol_free(&sym); /* should not crash */
    TT_ASSERT(1, "symbol_free with NULL fields should not crash");
}

void run_symbol_tests(void)
{
    TT_RUN(test_symbol_make_id_basic);
    TT_RUN(test_symbol_make_id_with_overload);
    TT_RUN(test_symbol_make_id_zero_overload_no_suffix);
    TT_RUN(test_symbol_make_id_negative_overload_ignored);
    TT_RUN(test_symbol_make_id_high_overload);
    TT_RUN(test_symbol_make_id_special_chars_in_path);
    TT_RUN(test_symbol_make_id_namespace_backslash);
    TT_RUN(test_symbol_free_handles_nulls);
}
