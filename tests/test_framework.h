/*
 * test_framework.h -- Minimal test framework for TokToken (no external deps).
 *
 * Provides assertion macros and a test runner.
 * Counters are extern -- define them in the *_main.c file with
 * TT_TEST_MAIN_VARS.
 *
 * Inspired by minunit.h.
 */

#ifndef TT_TEST_FRAMEWORK_H
#define TT_TEST_FRAMEWORK_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

/* Counter variables (defined once per executable via TT_TEST_MAIN_VARS) */
extern int tt_tests_run;
extern int tt_tests_failed;

/* Put this in exactly ONE .c file per test executable (the main file). */
#define TT_TEST_MAIN_VARS \
    int tt_tests_run = 0;  \
    int tt_tests_failed = 0

/* ---- Assertion macros ---- */

#define TT_ASSERT(test, msg) do { \
    tt_tests_run++; \
    if (!(test)) { \
        tt_tests_failed++; \
        fprintf(stderr, "  FAIL: %s:%d: %s\n", __FILE__, __LINE__, msg); \
    } \
} while(0)

#define TT_ASSERT_EQ_INT(a, b) do { \
    int _a = (a), _b = (b); \
    tt_tests_run++; \
    if (_a != _b) { \
        tt_tests_failed++; \
        fprintf(stderr, "  FAIL: %s:%d: %d != %d\n", __FILE__, __LINE__, _a, _b); \
    } \
} while(0)

#define TT_ASSERT_EQ_STR(a, b) do { \
    const char *_a = (a), *_b = (b); \
    tt_tests_run++; \
    if (_a == NULL || _b == NULL || strcmp(_a, _b) != 0) { \
        tt_tests_failed++; \
        fprintf(stderr, "  FAIL: %s:%d: \"%s\" != \"%s\"\n", \
                __FILE__, __LINE__, _a ? _a : "(null)", _b ? _b : "(null)"); \
    } \
} while(0)

#define TT_ASSERT_TRUE(test) TT_ASSERT(test, #test)
#define TT_ASSERT_FALSE(test) TT_ASSERT(!(test), "!(" #test ")")
#define TT_ASSERT_NULL(p) TT_ASSERT((p) == NULL, #p " should be NULL")
#define TT_ASSERT_NOT_NULL(p) TT_ASSERT((p) != NULL, #p " should not be NULL")

#define TT_ASSERT_GE_INT(a, b) do { \
    int _a = (a), _b = (b); \
    tt_tests_run++; \
    if (_a < _b) { \
        tt_tests_failed++; \
        fprintf(stderr, "  FAIL: %s:%d: %d < %d (expected >=)\n", __FILE__, __LINE__, _a, _b); \
    } \
} while(0)

#define TT_ASSERT_GT_INT(a, b) do { \
    int _a = (a), _b = (b); \
    tt_tests_run++; \
    if (_a <= _b) { \
        tt_tests_failed++; \
        fprintf(stderr, "  FAIL: %s:%d: %d <= %d (expected >)\n", __FILE__, __LINE__, _a, _b); \
    } \
} while(0)

#define TT_ASSERT_LE_INT(a, b) do { \
    int _a = (a), _b = (b); \
    tt_tests_run++; \
    if (_a > _b) { \
        tt_tests_failed++; \
        fprintf(stderr, "  FAIL: %s:%d: %d > %d (expected <=)\n", __FILE__, __LINE__, _a, _b); \
    } \
} while(0)

#define TT_ASSERT_STR_CONTAINS(haystack, needle) do { \
    const char *_h = (haystack), *_n = (needle); \
    tt_tests_run++; \
    if (_h == NULL || _n == NULL || strstr(_h, _n) == NULL) { \
        tt_tests_failed++; \
        fprintf(stderr, "  FAIL: %s:%d: \"%s\" does not contain \"%s\"\n", \
                __FILE__, __LINE__, _h ? _h : "(null)", _n ? _n : "(null)"); \
    } \
} while(0)

#define TT_ASSERT_STR_NOT_CONTAINS(haystack, needle) do { \
    const char *_h = (haystack), *_n = (needle); \
    tt_tests_run++; \
    if (_h != NULL && _n != NULL && strstr(_h, _n) != NULL) { \
        tt_tests_failed++; \
        fprintf(stderr, "  FAIL: %s:%d: \"%s\" should not contain \"%s\"\n", \
                __FILE__, __LINE__, _h, _n); \
    } \
} while(0)

#define TT_ASSERT_STR_ENDS_WITH(str, suffix) do { \
    const char *_s = (str), *_sf = (suffix); \
    tt_tests_run++; \
    size_t _sl = strlen(_s), _sfl = strlen(_sf); \
    if (_sl < _sfl || strcmp(_s + _sl - _sfl, _sf) != 0) { \
        tt_tests_failed++; \
        fprintf(stderr, "  FAIL: %s:%d: \"%s\" does not end with \"%s\"\n", \
                __FILE__, __LINE__, _s, _sf); \
    } \
} while(0)

/* ---- Test runner macros ---- */

#define TT_TEST(name) static void name(void)

#define TT_RUN(name) do { \
    fprintf(stderr, "  %s ... ", #name); \
    name(); \
    fprintf(stderr, "ok\n"); \
} while(0)

/* Skip a test with a reason (counts as passed, not failed). */
#define TT_SKIP(name, reason) do { \
    fprintf(stderr, "  %s ... SKIP (%s)\n", #name, reason); \
} while(0)

/* Print a suite header label. */
#define TT_SUITE(label) do { \
    fprintf(stderr, "\n=== %s ===\n", label); \
} while(0)

/* Print summary and return appropriate exit code. */
#define TT_SUMMARY() do { \
    fprintf(stderr, "\n%d tests, %d failed\n", tt_tests_run, tt_tests_failed); \
    return tt_tests_failed > 0 ? 1 : 0; \
} while(0)

#endif /* TT_TEST_FRAMEWORK_H */
