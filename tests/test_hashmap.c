/*
 * test_hashmap.c -- Unit tests for the hashmap module.
 */

#include "test_framework.h"
#include "hashmap.h"

#include <stdint.h>

/* ---- basic operations ---- */

TT_TEST(test_hashmap_set_get_has_remove)
{
    tt_hashmap_t *m = tt_hashmap_new(8);
    TT_ASSERT_NOT_NULL(m);

    void *old = tt_hashmap_set(m, "key1", (void *)0x1);
    TT_ASSERT_NULL(old);
    TT_ASSERT_EQ_INT((int)tt_hashmap_count(m), 1);

    void *v = tt_hashmap_get(m, "key1");
    TT_ASSERT(v == (void *)0x1, "get value");

    TT_ASSERT_TRUE(tt_hashmap_has(m, "key1"));
    TT_ASSERT_FALSE(tt_hashmap_has(m, "key2"));

    old = tt_hashmap_set(m, "key1", (void *)0x2);
    TT_ASSERT(old == (void *)0x1, "update returns old value");
    v = tt_hashmap_get(m, "key1");
    TT_ASSERT(v == (void *)0x2, "get updated value");

    old = tt_hashmap_remove(m, "key1");
    TT_ASSERT(old == (void *)0x2, "remove returns value");
    TT_ASSERT_FALSE(tt_hashmap_has(m, "key1"));
    TT_ASSERT_EQ_INT((int)tt_hashmap_count(m), 0);

    tt_hashmap_free(m);
}

/* ---- iteration ---- */

static int iter_count_cb(const char *key, void *value, void *userdata)
{
    (void)key; (void)value;
    int *count = userdata;
    (*count)++;
    return 0;
}

TT_TEST(test_hashmap_iterate)
{
    tt_hashmap_t *m = tt_hashmap_new(8);

    tt_hashmap_set(m, "a", (void *)1);
    tt_hashmap_set(m, "b", (void *)2);
    tt_hashmap_set(m, "c", (void *)3);

    int count = 0;
    tt_hashmap_iter(m, iter_count_cb, &count);
    TT_ASSERT_EQ_INT(count, 3);

    tt_hashmap_free(m);
}

/* ---- stress test: 10k entries ---- */

TT_TEST(test_hashmap_stress_10k)
{
    tt_hashmap_t *m = tt_hashmap_new(16);

    char key[32];
    for (int i = 0; i < 10000; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        tt_hashmap_set(m, key, (void *)(uintptr_t)(i + 1));
    }
    TT_ASSERT_EQ_INT((int)tt_hashmap_count(m), 10000);

    for (int i = 0; i < 10000; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        void *v = tt_hashmap_get(m, key);
        TT_ASSERT(v == (void *)(uintptr_t)(i + 1), "10k: value mismatch");
    }

    for (int i = 0; i < 5000; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        tt_hashmap_remove(m, key);
    }
    TT_ASSERT_EQ_INT((int)tt_hashmap_count(m), 5000);

    for (int i = 5000; i < 10000; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        TT_ASSERT_TRUE(tt_hashmap_has(m, key));
    }

    tt_hashmap_free(m);
}

/* ---- collision handling ---- */

TT_TEST(test_hashmap_collisions)
{
    tt_hashmap_t *m = tt_hashmap_new(4); /* tiny capacity to force collisions */

    for (int i = 0; i < 100; i++) {
        char key[16];
        snprintf(key, sizeof(key), "k%d", i);
        tt_hashmap_set(m, key, (void *)(uintptr_t)(i + 1));
    }
    TT_ASSERT_EQ_INT((int)tt_hashmap_count(m), 100);

    for (int i = 0; i < 100; i++) {
        char key[16];
        snprintf(key, sizeof(key), "k%d", i);
        void *v = tt_hashmap_get(m, key);
        TT_ASSERT(v == (void *)(uintptr_t)(i + 1), "collision: value mismatch");
    }

    tt_hashmap_free(m);
}

void run_hashmap_tests(void)
{
    TT_RUN(test_hashmap_set_get_has_remove);
    TT_RUN(test_hashmap_iterate);
    TT_RUN(test_hashmap_stress_10k);
    TT_RUN(test_hashmap_collisions);
}
