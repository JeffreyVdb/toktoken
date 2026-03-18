/*
 * test_int_wildmatch.c -- Integration tests for wildmatch and tt_fnmatch wrappers.
 */

#include "test_framework.h"
#include "wildmatch.h"
#include "platform.h"

TT_TEST(test_wildmatch_basic_star)
{
    TT_ASSERT_EQ_INT(WM_MATCH, wildmatch("*.c", "foo.c", 0));
    TT_ASSERT_EQ_INT(WM_NOMATCH, wildmatch("*.c", "foo.h", 0));
}

TT_TEST(test_wildmatch_star_no_dir)
{
    TT_ASSERT_EQ_INT(WM_NOMATCH, wildmatch("*.c", "src/foo.c", WM_PATHNAME));
    TT_ASSERT_EQ_INT(WM_MATCH, wildmatch("*.c", "foo.c", WM_PATHNAME));
}

TT_TEST(test_wildmatch_globstar)
{
    TT_ASSERT_EQ_INT(WM_MATCH,
        wildmatch("**/*.c", "src/foo/bar.c", WM_WILDSTAR | WM_PATHNAME));
    TT_ASSERT_EQ_INT(WM_MATCH,
        wildmatch("**/*.c", "bar.c", WM_WILDSTAR | WM_PATHNAME));
    TT_ASSERT_EQ_INT(WM_NOMATCH,
        wildmatch("**/*.c", "bar.h", WM_WILDSTAR | WM_PATHNAME));
}

TT_TEST(test_wildmatch_question_mark)
{
    TT_ASSERT_EQ_INT(WM_MATCH, wildmatch("?.c", "a.c", 0));
    TT_ASSERT_EQ_INT(WM_NOMATCH, wildmatch("?.c", "ab.c", 0));
}

TT_TEST(test_wildmatch_casefold)
{
    TT_ASSERT_EQ_INT(WM_MATCH, wildmatch("*.C", "foo.c", WM_CASEFOLD));
    TT_ASSERT_EQ_INT(WM_MATCH, wildmatch("*.c", "FOO.C", WM_CASEFOLD));
}

TT_TEST(test_wildmatch_char_class)
{
    TT_ASSERT_EQ_INT(WM_MATCH, wildmatch("[[:alpha:]]*.c", "foo.c", 0));
    TT_ASSERT_EQ_INT(WM_NOMATCH, wildmatch("[[:alpha:]]*.c", "1foo.c", 0));
}

TT_TEST(test_wildmatch_backslash_escape)
{
    TT_ASSERT_EQ_INT(WM_MATCH, wildmatch("\\*.c", "*.c", 0));
    TT_ASSERT_EQ_INT(WM_NOMATCH, wildmatch("\\*.c", "foo.c", 0));
}

TT_TEST(test_wildmatch_negation)
{
    TT_ASSERT_EQ_INT(WM_MATCH, wildmatch("[!a]*.c", "b.c", 0));
    TT_ASSERT_EQ_INT(WM_NOMATCH, wildmatch("[!a]*.c", "a.c", 0));
}

TT_TEST(test_wildmatch_range_reversal)
{
    TT_ASSERT_EQ_INT(WM_MATCH, wildmatch("[z-a].c", "m.c", 0));
}

TT_TEST(test_wildmatch_empty)
{
    TT_ASSERT_EQ_INT(WM_MATCH, wildmatch("", "", 0));
    TT_ASSERT_EQ_INT(WM_NOMATCH, wildmatch("", "a", 0));
}

TT_TEST(test_wildmatch_leading_dir)
{
    TT_ASSERT_EQ_INT(WM_MATCH,
        wildmatch("src", "src/foo", WM_LEADING_DIR));
    TT_ASSERT_EQ_INT(WM_NOMATCH,
        wildmatch("src", "lib/foo", WM_LEADING_DIR));
}

TT_TEST(test_tt_fnmatch_wrapper)
{
    TT_ASSERT_TRUE(tt_fnmatch("*.c", "foo.c", false));
    TT_ASSERT_FALSE(tt_fnmatch("*.c", "foo.h", false));
    TT_ASSERT_TRUE(tt_fnmatch("*.C", "foo.c", true));
    TT_ASSERT_FALSE(tt_fnmatch(NULL, "foo.c", false));
    TT_ASSERT_FALSE(tt_fnmatch("*.c", NULL, false));
}

TT_TEST(test_tt_fnmatch_ex_wrapper)
{
    TT_ASSERT_TRUE(tt_fnmatch_ex("**/*.c", "src/foo.c", TT_FNM_PATHNAME));
    TT_ASSERT_TRUE(tt_fnmatch_ex("**/*.c", "foo.c", TT_FNM_PATHNAME));
    TT_ASSERT_FALSE(tt_fnmatch_ex("*.c", "src/foo.c", TT_FNM_PATHNAME));
    TT_ASSERT_TRUE(tt_fnmatch_ex("*.C", "foo.c", TT_FNM_CASEFOLD));
    TT_ASSERT_FALSE(tt_fnmatch_ex(NULL, "foo.c", 0));
    TT_ASSERT_FALSE(tt_fnmatch_ex("*.c", NULL, 0));
}

void run_int_wildmatch_tests(void)
{
    TT_RUN(test_wildmatch_basic_star);
    TT_RUN(test_wildmatch_star_no_dir);
    TT_RUN(test_wildmatch_globstar);
    TT_RUN(test_wildmatch_question_mark);
    TT_RUN(test_wildmatch_casefold);
    TT_RUN(test_wildmatch_char_class);
    TT_RUN(test_wildmatch_backslash_escape);
    TT_RUN(test_wildmatch_negation);
    TT_RUN(test_wildmatch_range_reversal);
    TT_RUN(test_wildmatch_empty);
    TT_RUN(test_wildmatch_leading_dir);
    TT_RUN(test_tt_fnmatch_wrapper);
    TT_RUN(test_tt_fnmatch_ex_wrapper);
}
