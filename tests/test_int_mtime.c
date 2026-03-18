/*
 * test_int_mtime.c -- Integration tests for mtime portability.
 *
 * Validates:
 *   - tt_file_mtime() correctness
 *   - TT_ST_MTIME_SEC / TT_ST_MTIME_NSEC macro correctness
 *   - Sub-second precision (when filesystem supports it)
 *   - Macro type safety (int64_t)
 *   - Consistency between tt_file_mtime() and TT_ST_MTIME_SEC()
 *   - Roundtrip: stat -> macro -> store -> readback -> compare
 *   - Edge cases: freshly created file, rapid rewrites
 */

#include "test_framework.h"
#include "test_helpers.h"
#include "platform.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#ifndef TT_PLATFORM_WINDOWS
#include <unistd.h>
#endif

/* ---- Helpers ---- */

static char *s_tmpdir;

static void mtime_setup(void)
{
    s_tmpdir = tt_test_tmpdir();
}

static void mtime_cleanup(void)
{
    if (s_tmpdir)
    {
        tt_test_rmdir(s_tmpdir);
        free(s_tmpdir);
        s_tmpdir = NULL;
    }
}

/* ---- Tests ---- */

/*
 * tt_file_mtime() returns a positive value for an existing file.
 */
TT_TEST(test_file_mtime_returns_positive)
{
    mtime_setup();

    tt_test_write_file(s_tmpdir, "a.txt", "hello\n");
    char *path = tt_path_join(s_tmpdir, "a.txt");

    time_t mt = tt_file_mtime(path);
    TT_ASSERT(mt > 0, "tt_file_mtime should return > 0 for existing file");

    free(path);
    mtime_cleanup();
}

/*
 * tt_file_mtime() returns 0 for a nonexistent file.
 */
TT_TEST(test_file_mtime_nonexistent)
{
    time_t mt = tt_file_mtime("/nonexistent_path_tt_xyz_987");
    TT_ASSERT(mt == 0, "tt_file_mtime should return 0 for missing file");
}

/*
 * tt_file_mtime(NULL) returns 0.
 */
TT_TEST(test_file_mtime_null)
{
    time_t mt = tt_file_mtime(NULL);
    TT_ASSERT(mt == 0, "tt_file_mtime(NULL) should return 0");
}

/*
 * TT_ST_MTIME_SEC produces a reasonable value (> 2020-01-01 epoch).
 */
TT_TEST(test_macro_sec_reasonable_value)
{
    mtime_setup();

    tt_test_write_file(s_tmpdir, "b.txt", "data\n");
    char *path = tt_path_join(s_tmpdir, "b.txt");

    struct stat st;
    int rc = stat(path, &st);
    TT_ASSERT_EQ_INT(rc, 0);

    int64_t sec = TT_ST_MTIME_SEC(st);
    /* 2020-01-01 00:00:00 UTC = 1577836800 */
    TT_ASSERT(sec > 1577836800LL, "TT_ST_MTIME_SEC should be after 2020-01-01");
    /* Sanity upper bound: before 2100-01-01 = 4102444800 */
    TT_ASSERT(sec < 4102444800LL, "TT_ST_MTIME_SEC should be before 2100-01-01");

    free(path);
    mtime_cleanup();
}

/*
 * TT_ST_MTIME_NSEC is in range [0, 999999999].
 */
TT_TEST(test_macro_nsec_in_range)
{
    mtime_setup();

    tt_test_write_file(s_tmpdir, "c.txt", "x\n");
    char *path = tt_path_join(s_tmpdir, "c.txt");

    struct stat st;
    int rc = stat(path, &st);
    TT_ASSERT_EQ_INT(rc, 0);

    int64_t nsec = TT_ST_MTIME_NSEC(st);
    TT_ASSERT(nsec >= 0, "TT_ST_MTIME_NSEC should be >= 0");
    TT_ASSERT(nsec < 1000000000LL, "TT_ST_MTIME_NSEC should be < 1e9");

    free(path);
    mtime_cleanup();
}

/*
 * TT_ST_MTIME_SEC is consistent with tt_file_mtime().
 * They should agree on seconds (tt_file_mtime uses the same macro internally).
 */
TT_TEST(test_macro_sec_matches_file_mtime)
{
    mtime_setup();

    tt_test_write_file(s_tmpdir, "d.txt", "test content\n");
    char *path = tt_path_join(s_tmpdir, "d.txt");

    struct stat st;
    int rc = stat(path, &st);
    TT_ASSERT_EQ_INT(rc, 0);

    int64_t macro_sec = TT_ST_MTIME_SEC(st);
    time_t api_mtime = tt_file_mtime(path);

    TT_ASSERT((int64_t)api_mtime == macro_sec,
              "tt_file_mtime and TT_ST_MTIME_SEC must agree");

    free(path);
    mtime_cleanup();
}

/*
 * TT_ST_MTIME_SEC is consistent with raw st_mtime.
 * On all platforms, the seconds component must match st_mtime.
 */
TT_TEST(test_macro_sec_matches_raw_st_mtime)
{
    mtime_setup();

    tt_test_write_file(s_tmpdir, "e.txt", "raw mtime test\n");
    char *path = tt_path_join(s_tmpdir, "e.txt");

    struct stat st;
    int rc = stat(path, &st);
    TT_ASSERT_EQ_INT(rc, 0);

    int64_t macro_sec = TT_ST_MTIME_SEC(st);
    int64_t raw_sec = (int64_t)st.st_mtime;

    TT_ASSERT(macro_sec == raw_sec,
              "TT_ST_MTIME_SEC must agree with st_mtime seconds");

    free(path);
    mtime_cleanup();
}

/*
 * Macros return int64_t: verify via sizeof.
 * This is a compile-time property but we assert at runtime to get a test failure
 * rather than a compile error if someone changes the macro.
 */
TT_TEST(test_macro_returns_int64)
{
    mtime_setup();

    tt_test_write_file(s_tmpdir, "f.txt", "type\n");
    char *path = tt_path_join(s_tmpdir, "f.txt");

    struct stat st;
    stat(path, &st);

    /* _Generic would be ideal but C11 _Generic is not universally reliable
     * in all compilers. sizeof check ensures at least the width is correct. */
    TT_ASSERT(sizeof(TT_ST_MTIME_SEC(st)) == sizeof(int64_t),
              "TT_ST_MTIME_SEC must produce int64_t-sized result");
    TT_ASSERT(sizeof(TT_ST_MTIME_NSEC(st)) == sizeof(int64_t),
              "TT_ST_MTIME_NSEC must produce int64_t-sized result");

    free(path);
    mtime_cleanup();
}

/*
 * Rewriting a file after a delay produces a different mtime (seconds).
 * We sleep 1.1s to guarantee the second changes on any filesystem.
 */
TT_TEST(test_mtime_changes_on_rewrite)
{
    mtime_setup();

    tt_test_write_file(s_tmpdir, "g.txt", "version 1\n");
    char *path = tt_path_join(s_tmpdir, "g.txt");

    struct stat st1;
    stat(path, &st1);
    int64_t sec1 = TT_ST_MTIME_SEC(st1);

    /* Sleep > 1 second to guarantee second-level mtime difference */
    tt_sleep_ms(1100);

    tt_test_write_file(s_tmpdir, "g.txt", "version 2 -- different content\n");

    struct stat st2;
    stat(path, &st2);
    int64_t sec2 = TT_ST_MTIME_SEC(st2);

    TT_ASSERT(sec2 > sec1,
              "rewrite after 1.1s sleep must produce strictly greater mtime_sec");

    free(path);
    mtime_cleanup();
}

/*
 * Sub-second precision test: two rapid writes should produce the same
 * second but (on ext4/APFS/tmpfs with ns support) different nanoseconds.
 *
 * This test is best-effort -- on FAT32 or NFS with coarse timestamps
 * nsec might be 0 for both. We only assert the weak invariant: if nsec
 * differs, the values are both in range.
 *
 * On platforms without sub-second support (Windows fallback), both nsec
 * should be 0.
 */
TT_TEST(test_subsecond_precision)
{
    mtime_setup();

    tt_test_write_file(s_tmpdir, "h1.txt", "first\n");
    char *path1 = tt_path_join(s_tmpdir, "h1.txt");

    struct stat st1;
    stat(path1, &st1);
    int64_t nsec1 = TT_ST_MTIME_NSEC(st1);

    /* Tiny delay to nudge nanoseconds */
#ifndef TT_PLATFORM_WINDOWS
    usleep(1000); /* 1ms */
#endif

    tt_test_write_file(s_tmpdir, "h2.txt", "second\n");
    char *path2 = tt_path_join(s_tmpdir, "h2.txt");

    struct stat st2;
    stat(path2, &st2);
    int64_t nsec2 = TT_ST_MTIME_NSEC(st2);

    /* Both must be in valid range */
    TT_ASSERT(nsec1 >= 0 && nsec1 < 1000000000LL,
              "nsec1 must be in [0, 999999999]");
    TT_ASSERT(nsec2 >= 0 && nsec2 < 1000000000LL,
              "nsec2 must be in [0, 999999999]");

    /* If both nsec are 0, we're on a platform/fs without sub-second support.
     * If at least one is nonzero, the platform provides sub-second mtime. */
    int has_subsecond = (nsec1 != 0 || nsec2 != 0);
    if (has_subsecond)
    {
        fprintf(stderr, "(sub-second supported: nsec1=%lld, nsec2=%lld) ",
                (long long)nsec1, (long long)nsec2);
    }
    else
    {
        fprintf(stderr, "(no sub-second support detected) ");
    }
    /* Always passes -- informational */
    TT_ASSERT(1, "sub-second probe completed");

    free(path1);
    free(path2);
    mtime_cleanup();
}

/*
 * Multiple stat() calls on the same untouched file produce identical
 * sec + nsec values (deterministic).
 */
TT_TEST(test_mtime_stable_without_modification)
{
    mtime_setup();

    tt_test_write_file(s_tmpdir, "stable.txt", "immutable\n");
    char *path = tt_path_join(s_tmpdir, "stable.txt");

    struct stat st1, st2;
    stat(path, &st1);
    stat(path, &st2);

    int64_t sec1 = TT_ST_MTIME_SEC(st1);
    int64_t sec2 = TT_ST_MTIME_SEC(st2);
    int64_t nsec1 = TT_ST_MTIME_NSEC(st1);
    int64_t nsec2 = TT_ST_MTIME_NSEC(st2);

    TT_ASSERT(sec1 == sec2, "seconds must be stable across stat() calls");
    TT_ASSERT(nsec1 == nsec2, "nanoseconds must be stable across stat() calls");

    free(path);
    mtime_cleanup();
}

/*
 * Roundtrip fidelity: extract sec/nsec, reconstruct, compare.
 * If we store sec+nsec and read them back, they must match the original stat.
 */
TT_TEST(test_roundtrip_sec_nsec)
{
    mtime_setup();

    tt_test_write_file(s_tmpdir, "rt.txt", "roundtrip\n");
    char *path = tt_path_join(s_tmpdir, "rt.txt");

    struct stat st;
    stat(path, &st);

    int64_t orig_sec = TT_ST_MTIME_SEC(st);
    int64_t orig_nsec = TT_ST_MTIME_NSEC(st);

    /* Simulate "store and readback" by going through intermediate variables */
    volatile int64_t stored_sec = orig_sec;
    volatile int64_t stored_nsec = orig_nsec;

    /* Re-stat the same unmodified file */
    struct stat st2;
    stat(path, &st2);

    int64_t check_sec = TT_ST_MTIME_SEC(st2);
    int64_t check_nsec = TT_ST_MTIME_NSEC(st2);

    TT_ASSERT((int64_t)stored_sec == check_sec,
              "roundtrip sec must match re-stat");
    TT_ASSERT((int64_t)stored_nsec == check_nsec,
              "roundtrip nsec must match re-stat");

    free(path);
    mtime_cleanup();
}

/*
 * The SEC macro applied to two different files created at different times
 * produces ordered results.
 */
TT_TEST(test_mtime_ordering)
{
    mtime_setup();

    tt_test_write_file(s_tmpdir, "old.txt", "old\n");
    char *path_old = tt_path_join(s_tmpdir, "old.txt");
    struct stat st_old;
    stat(path_old, &st_old);

    tt_sleep_ms(1100);

    tt_test_write_file(s_tmpdir, "new.txt", "new\n");
    char *path_new = tt_path_join(s_tmpdir, "new.txt");
    struct stat st_new;
    stat(path_new, &st_new);

    int64_t sec_old = TT_ST_MTIME_SEC(st_old);
    int64_t sec_new = TT_ST_MTIME_SEC(st_new);

    TT_ASSERT(sec_new > sec_old,
              "newer file must have strictly greater mtime_sec");

    free(path_old);
    free(path_new);
    mtime_cleanup();
}

/* ---- Runner ---- */

void run_int_mtime_tests(void)
{
    TT_RUN(test_file_mtime_returns_positive);
    TT_RUN(test_file_mtime_nonexistent);
    TT_RUN(test_file_mtime_null);
    TT_RUN(test_macro_sec_reasonable_value);
    TT_RUN(test_macro_nsec_in_range);
    TT_RUN(test_macro_sec_matches_file_mtime);
    TT_RUN(test_macro_sec_matches_raw_st_mtime);
    TT_RUN(test_macro_returns_int64);
    TT_RUN(test_mtime_changes_on_rewrite);
    TT_RUN(test_subsecond_precision);
    TT_RUN(test_mtime_stable_without_modification);
    TT_RUN(test_roundtrip_sec_nsec);
    TT_RUN(test_mtime_ordering);
}
