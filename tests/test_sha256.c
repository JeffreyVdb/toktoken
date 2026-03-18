/*
 * test_sha256.c -- Unit tests for SHA-256 (vendor) and sha256_util (buffer hash).
 */

#include "test_framework.h"
#include "sha256.h"
#include "sha256_util.h"

#include <string.h>

/* ---- SHA-256 vendor: known vectors (FIPS PUB 180-4) ---- */

TT_TEST(test_sha256_empty_string)
{
    char hex[65];
    tt_sha256_hex((const uint8_t *)"", 0, hex);
    TT_ASSERT_EQ_STR(hex, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TT_TEST(test_sha256_abc)
{
    char hex[65];
    tt_sha256_hex((const uint8_t *)"abc", 3, hex);
    TT_ASSERT_EQ_STR(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TT_TEST(test_sha256_448bit)
{
    char hex[65];
    tt_sha256_hex((const uint8_t *)"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56, hex);
    TT_ASSERT_EQ_STR(hex, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

TT_TEST(test_sha256_896bit)
{
    char hex[65];
    tt_sha256_hex((const uint8_t *)"abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
                  112, hex);
    TT_ASSERT_EQ_STR(hex, "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1");
}

TT_TEST(test_sha256_null_data)
{
    char hex[65];
    tt_sha256_hex(NULL, 0, hex);
    TT_ASSERT_EQ_STR(hex, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

/* ---- sha256_util: buffer hash ---- */

TT_TEST(test_sha256_util_buf)
{
    char *hex = tt_sha256_buf("abc", 3);
    TT_ASSERT_NOT_NULL(hex);
    TT_ASSERT_EQ_STR(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    free(hex);
}

void run_sha256_tests(void)
{
    TT_RUN(test_sha256_empty_string);
    TT_RUN(test_sha256_abc);
    TT_RUN(test_sha256_448bit);
    TT_RUN(test_sha256_896bit);
    TT_RUN(test_sha256_null_data);
    TT_RUN(test_sha256_util_buf);
}
