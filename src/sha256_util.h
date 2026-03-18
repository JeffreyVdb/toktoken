/*
 * sha256_util.h -- Convenience wrappers around vendor SHA-256.
 */

#ifndef TT_SHA256_UTIL_H
#define TT_SHA256_UTIL_H

#include <stddef.h>

/*
 * tt_sha256_file -- Hash file content and return hex string.
 *
 * [caller-frees] Returns NULL on error (file not found, read error).
 */
char *tt_sha256_file(const char *path);

/*
 * tt_sha256_buf -- Hash buffer and return hex string.
 *
 * [caller-frees]
 */
char *tt_sha256_buf(const char *data, size_t len);

#endif /* TT_SHA256_UTIL_H */
