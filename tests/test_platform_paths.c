/*
 * test_platform_paths.c -- Unit tests for platform path functions,
 * fnmatch, time, and string case functions.
 *
 * Only pure functions that don't require filesystem or process I/O.
 */

#include "test_framework.h"
#include "platform.h"
#include "error.h"

/* ---- path_join ---- */

TT_TEST(test_path_join_basic)
{
    char *p = tt_path_join("/home/user", "file.txt");
    TT_ASSERT_NOT_NULL(p);
    TT_ASSERT_EQ_STR(p, "/home/user/file.txt");
    free(p);
}

TT_TEST(test_path_join_trailing_slash)
{
    char *p = tt_path_join("/home/user/", "file.txt");
    TT_ASSERT_EQ_STR(p, "/home/user/file.txt");
    free(p);
}

TT_TEST(test_path_join_leading_slash)
{
    char *p = tt_path_join("/home", "/sub/file.txt");
    TT_ASSERT_EQ_STR(p, "/home/sub/file.txt");
    free(p);
}

/* ---- path_basename ---- */

TT_TEST(test_path_basename)
{
    TT_ASSERT_EQ_STR(tt_path_basename("/home/user/file.txt"), "file.txt");
    TT_ASSERT_EQ_STR(tt_path_basename("file.txt"), "file.txt");
    TT_ASSERT_EQ_STR(tt_path_basename("/"), "");
}

/* ---- path_extension ---- */

TT_TEST(test_path_extension)
{
    TT_ASSERT_EQ_STR(tt_path_extension("file.php"), ".php");
    TT_ASSERT_EQ_STR(tt_path_extension("file.blade.php"), ".blade.php");
    TT_ASSERT_EQ_STR(tt_path_extension("file.vue"), ".vue");
    TT_ASSERT_EQ_STR(tt_path_extension("file.js"), ".js");
    TT_ASSERT_EQ_STR(tt_path_extension("Makefile"), "");
    TT_ASSERT_EQ_STR(tt_path_extension(".gitignore"), "");
}

/* ---- path_is_absolute ---- */

TT_TEST(test_path_is_absolute)
{
    TT_ASSERT_TRUE(tt_path_is_absolute("/home"));
    TT_ASSERT_FALSE(tt_path_is_absolute("relative"));
    TT_ASSERT_FALSE(tt_path_is_absolute("./relative"));
}

/* ---- path_relative ---- */

TT_TEST(test_path_relative)
{
    char *r = tt_path_relative("/home/user/project", "/home/user/project/src/main.c");
    TT_ASSERT_NOT_NULL(r);
    TT_ASSERT_EQ_STR(r, "src/main.c");
    free(r);

    r = tt_path_relative("/home/user/project", "/other/path");
    TT_ASSERT_NULL(r);

    r = tt_path_relative("/home/user/project", "/home/user/project");
    TT_ASSERT_NOT_NULL(r);
    TT_ASSERT_EQ_STR(r, "");
    free(r);
}

/* ---- path_normalize_sep ---- */

TT_TEST(test_path_normalize_sep)
{
    char path[] = "src\\main\\file.c";
    tt_path_normalize_sep(path);
    TT_ASSERT_EQ_STR(path, "src/main/file.c");
}

/* ---- fnmatch ---- */

TT_TEST(test_fnmatch)
{
    TT_ASSERT_TRUE(tt_fnmatch("*.php", "test.php", false));
    TT_ASSERT_FALSE(tt_fnmatch("*.php", "test.js", false));
    TT_ASSERT_TRUE(tt_fnmatch("test.*", "test.php", false));
    TT_ASSERT_TRUE(tt_fnmatch("?est.php", "test.php", false));
    TT_ASSERT_FALSE(tt_fnmatch("*.PHP", "test.php", false));
}

/* ---- time ---- */

TT_TEST(test_now_rfc3339_format)
{
    const char *ts = tt_now_rfc3339();
    TT_ASSERT_NOT_NULL(ts);
    size_t len = strlen(ts);
    TT_ASSERT(len >= 25, "rfc3339 should be at least 25 chars");
    TT_ASSERT(ts[4] == '-', "dash at pos 4");
    TT_ASSERT(ts[7] == '-', "dash at pos 7");
    TT_ASSERT(ts[10] == 'T', "T separator");
    TT_ASSERT(ts[13] == ':', "colon at pos 13");
    TT_ASSERT(ts[16] == ':', "colon at pos 16");
    TT_ASSERT(ts[len - 3] == ':', "timezone colon");
}

TT_TEST(test_monotonic_ms)
{
    uint64_t t1 = tt_monotonic_ms();
    TT_ASSERT(t1 > 0, "nonzero");
    uint64_t t2 = tt_monotonic_ms();
    TT_ASSERT(t2 >= t1, "monotonically increasing");
}

/* ---- strcasecmp / strcasestr ---- */

TT_TEST(test_strcasecmp)
{
    TT_ASSERT_TRUE(tt_strcasecmp("Hello", "hello") == 0);
    TT_ASSERT_TRUE(tt_strcasecmp("abc", "abd") < 0);

    const char *found = tt_strcasestr("Hello World", "WORLD");
    TT_ASSERT_NOT_NULL(found);
    TT_ASSERT_EQ_STR(found, "World");

    TT_ASSERT_NULL(tt_strcasestr("hello", "xyz"));
}

void run_platform_paths_tests(void)
{
    TT_RUN(test_path_join_basic);
    TT_RUN(test_path_join_trailing_slash);
    TT_RUN(test_path_join_leading_slash);
    TT_RUN(test_path_basename);
    TT_RUN(test_path_extension);
    TT_RUN(test_path_is_absolute);
    TT_RUN(test_path_relative);
    TT_RUN(test_path_normalize_sep);
    TT_RUN(test_fnmatch);
    TT_RUN(test_now_rfc3339_format);
    TT_RUN(test_monotonic_ms);
    TT_RUN(test_strcasecmp);
}
