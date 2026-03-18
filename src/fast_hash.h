/*
 * fast_hash.h -- Fast non-cryptographic hashing for change detection.
 *
 * Uses XXH3_64bits (xxHash). NOT cryptographic.
 * For file/symbol content change detection only.
 * SHA-256 is kept separately for binary integrity verification.
 */

#ifndef TT_FAST_HASH_H
#define TT_FAST_HASH_H

#include <stddef.h>

/*
 * tt_fast_hash_hex -- Hash buffer for change detection.
 *
 * Returns a 16-char hex string (XXH3_64bits).
 * [caller-frees]
 */
char *tt_fast_hash_hex(const char *data, size_t len);

/*
 * tt_fast_hash_file -- Hash file content for change detection.
 *
 * Reads entire file, hashes with XXH3_64bits.
 * Returns a 16-char hex string.
 * [caller-frees]
 */
char *tt_fast_hash_file(const char *path);

#endif /* TT_FAST_HASH_H */
