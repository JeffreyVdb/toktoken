/*
 * fast_hash.c -- Fast non-cryptographic hashing via XXHash3.
 *
 * XXH_INLINE_ALL inlines the entire XXHash implementation for maximum
 * performance (no function-call overhead). XXH_NO_STREAM disables the
 * streaming API we don't need, reducing code size.
 */

#define XXH_INLINE_ALL
#define XXH_NO_STREAM
#include "xxhash.h"

#include "fast_hash.h"
#include "platform.h"
#include "error.h"

#include <stdlib.h>
#include <stdio.h>

char *tt_fast_hash_hex(const char *data, size_t len)
{
    if (!data)
    {
        tt_error_set("tt_fast_hash_hex: NULL data");
        return NULL;
    }

    XXH64_hash_t h = XXH3_64bits(data, len);

    char *hex = malloc(17); /* 16 hex chars + null */
    if (!hex)
    {
        tt_error_set("tt_fast_hash_hex: malloc failed");
        return NULL;
    }

    snprintf(hex, 17, "%016llx", (unsigned long long)h);
    return hex;
}

char *tt_fast_hash_file(const char *path)
{
    if (!path)
    {
        tt_error_set("tt_fast_hash_file: NULL path");
        return NULL;
    }

    size_t len = 0;
    char *data = tt_read_file(path, &len);
    if (!data)
        return NULL;

    char *hex = tt_fast_hash_hex(data, len);
    free(data);
    return hex;
}
