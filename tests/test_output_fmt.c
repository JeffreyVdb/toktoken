/*
 * test_output_fmt.c -- Unit tests for output_fmt module.
 */

#include "test_framework.h"
#include "output_fmt.h"

#include <stdlib.h>
#include <string.h>

/* ---- matchesPathFilters ---- */

TT_TEST(test_ofmt_no_filters_matches_everything)
{
    TT_ASSERT_TRUE(tt_matches_path_filters("src/Auth/Controller.php", NULL, NULL));
}

TT_TEST(test_ofmt_filter_includes_matching)
{
    TT_ASSERT_TRUE(tt_matches_path_filters("src/Auth/Controller.php", "Auth", NULL));
}

TT_TEST(test_ofmt_filter_excludes_non_matching)
{
    TT_ASSERT_FALSE(tt_matches_path_filters("src/Cache/Store.php", "Auth", NULL));
}

TT_TEST(test_ofmt_filter_case_insensitive)
{
    TT_ASSERT_TRUE(tt_matches_path_filters("src/AUTH/Controller.php", "auth", NULL));
}

TT_TEST(test_ofmt_filter_pipe_separated_or)
{
    TT_ASSERT_TRUE(tt_matches_path_filters("src/Auth/Controller.php", "Auth|Cache", NULL));
    TT_ASSERT_TRUE(tt_matches_path_filters("src/Cache/Store.php", "Auth|Cache", NULL));
    TT_ASSERT_FALSE(tt_matches_path_filters("src/Models/User.php", "Auth|Cache", NULL));
}

TT_TEST(test_ofmt_exclude_removes_matching)
{
    TT_ASSERT_FALSE(tt_matches_path_filters("vendor/autoload.php", NULL, "vendor"));
    TT_ASSERT_TRUE(tt_matches_path_filters("src/App.php", NULL, "vendor"));
}

TT_TEST(test_ofmt_exclude_pipe_separated_or)
{
    TT_ASSERT_FALSE(tt_matches_path_filters("vendor/package.php", NULL, "vendor|test"));
    TT_ASSERT_FALSE(tt_matches_path_filters("tests/Unit/FooTest.php", NULL, "vendor|test"));
    TT_ASSERT_TRUE(tt_matches_path_filters("src/App.php", NULL, "vendor|test"));
}

TT_TEST(test_ofmt_filter_and_exclude_combined)
{
    TT_ASSERT_TRUE(tt_matches_path_filters("src/App.php", "src", "test"));
    TT_ASSERT_FALSE(tt_matches_path_filters("src/tests/FooTest.php", "src", "test"));
    TT_ASSERT_FALSE(tt_matches_path_filters("lib/App.php", "src", "test"));
}

TT_TEST(test_ofmt_filter_empty_pipe_segments)
{
    TT_ASSERT_TRUE(tt_matches_path_filters("src/Auth/X.php", "Auth||Cache", NULL));
    TT_ASSERT_TRUE(tt_matches_path_filters("src/Cache/X.php", "Auth||Cache", NULL));
}

TT_TEST(test_ofmt_exclude_case_insensitive)
{
    TT_ASSERT_FALSE(tt_matches_path_filters("vendor/foo.php", NULL, "VENDOR"));
}

TT_TEST(test_ofmt_null_path_returns_false)
{
    TT_ASSERT_FALSE(tt_matches_path_filters(NULL, NULL, NULL));
}

/* ---- applyLimit ---- */

TT_TEST(test_ofmt_apply_limit_caps)
{
    int count = 10;
    tt_apply_limit(5, &count);
    TT_ASSERT_EQ_INT(5, count);
}

TT_TEST(test_ofmt_apply_limit_no_op_when_zero)
{
    int count = 10;
    tt_apply_limit(0, &count);
    TT_ASSERT_EQ_INT(10, count);
}

TT_TEST(test_ofmt_apply_limit_no_op_when_under)
{
    int count = 3;
    tt_apply_limit(10, &count);
    TT_ASSERT_EQ_INT(3, count);
}

TT_TEST(test_ofmt_apply_limit_null_count)
{
    /* Should not crash */
    tt_apply_limit(5, NULL);
}

void run_output_fmt_tests(void)
{
    TT_RUN(test_ofmt_no_filters_matches_everything);
    TT_RUN(test_ofmt_filter_includes_matching);
    TT_RUN(test_ofmt_filter_excludes_non_matching);
    TT_RUN(test_ofmt_filter_case_insensitive);
    TT_RUN(test_ofmt_filter_pipe_separated_or);
    TT_RUN(test_ofmt_exclude_removes_matching);
    TT_RUN(test_ofmt_exclude_pipe_separated_or);
    TT_RUN(test_ofmt_filter_and_exclude_combined);
    TT_RUN(test_ofmt_filter_empty_pipe_segments);
    TT_RUN(test_ofmt_exclude_case_insensitive);
    TT_RUN(test_ofmt_null_path_returns_false);
    TT_RUN(test_ofmt_apply_limit_caps);
    TT_RUN(test_ofmt_apply_limit_no_op_when_zero);
    TT_RUN(test_ofmt_apply_limit_no_op_when_under);
    TT_RUN(test_ofmt_apply_limit_null_count);
}
