/*
 * sha256_util.c -- Convenience wrappers around vendor SHA-256.
 */

#include "sha256_util.h"
#include "platform.h"
#include "error.h"
#include "sha256.h"

#include <stdlib.h>
#include <string.h>

char *tt_sha256_file(const char *path)
{
    if (!path) {
        tt_error_set("tt_sha256_file: NULL path");
        return NULL;
    }

    size_t len = 0;
    char *data = tt_read_file(path, &len);
    if (!data) return NULL; /* tt_read_file already set the error */

    char *hex = malloc(65);
    if (!hex) {
        free(data);
        tt_error_set("tt_sha256_file: malloc failed");
        return NULL;
    }

    tt_sha256_hex((const uint8_t *)data, len, hex);
    free(data);
    return hex;
}

char *tt_sha256_buf(const char *data, size_t len)
{
    if (!data) {
        tt_error_set("tt_sha256_buf: NULL data");
        return NULL;
    }

    char *hex = malloc(65);
    if (!hex) {
        tt_error_set("tt_sha256_buf: malloc failed");
        return NULL;
    }

    tt_sha256_hex((const uint8_t *)data, len, hex);
    return hex;
}
