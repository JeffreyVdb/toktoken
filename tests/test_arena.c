/*
 * test_arena.c -- Unit tests for the arena allocator module.
 */

#include "test_framework.h"
#include "arena.h"

TT_TEST(test_arena_alloc_and_strdup)
{
    tt_arena_t *a = tt_arena_new(0);
    TT_ASSERT_NOT_NULL(a);

    void *p1 = tt_arena_alloc(a, 100);
    TT_ASSERT_NOT_NULL(p1);
    memset(p1, 0xAA, 100);

    char *s1 = tt_arena_strdup(a, "hello");
    TT_ASSERT_NOT_NULL(s1);
    TT_ASSERT_EQ_STR(s1, "hello");

    char *s2 = tt_arena_strndup(a, "hello world", 5);
    TT_ASSERT_NOT_NULL(s2);
    TT_ASSERT_EQ_STR(s2, "hello");

    TT_ASSERT(tt_arena_used(a) > 0, "used > 0");

    tt_arena_free(a);
}

TT_TEST(test_arena_oversized_alloc)
{
    tt_arena_t *a = tt_arena_new(256); /* small blocks */

    void *big = tt_arena_alloc(a, 1024);
    TT_ASSERT_NOT_NULL(big);
    memset(big, 0xBB, 1024);

    void *small = tt_arena_alloc(a, 32);
    TT_ASSERT_NOT_NULL(small);

    tt_arena_free(a);
}

TT_TEST(test_arena_10k_allocs)
{
    tt_arena_t *a = tt_arena_new(0);

    for (int i = 0; i < 10000; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "item_%d", i);
        char *s = tt_arena_strdup(a, buf);
        TT_ASSERT_NOT_NULL(s);
    }

    TT_ASSERT(tt_arena_used(a) > 0, "used bytes > 0");
    tt_arena_free(a);
}

void run_arena_tests(void)
{
    TT_RUN(test_arena_alloc_and_strdup);
    TT_RUN(test_arena_oversized_alloc);
    TT_RUN(test_arena_10k_allocs);
}
