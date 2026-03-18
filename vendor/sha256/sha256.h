/*
 * SHA-256 -- Standalone implementation for TokToken
 *
 * Original implementation for the TokToken project.
 * Based on the algorithm described in FIPS PUB 180-4.
 *
 * License: same as TokToken project (MIT)
 */

#ifndef TT_SHA256_H
#define TT_SHA256_H

#include <stddef.h>
#include <stdint.h>

/*
 * tt_sha256 -- Compute SHA-256 hash of a data buffer.
 *
 * Parameters:
 *   data  [borrows] Input data buffer
 *   len   Length of input data in bytes
 *   out   [caller-owns] Output buffer, must be at least 32 bytes
 */
void tt_sha256(const uint8_t *data, size_t len, uint8_t out[32]);

/*
 * tt_sha256_hex -- Compute SHA-256 hash and return as hex string.
 *
 * Parameters:
 *   data  [borrows] Input data buffer
 *   len   Length of input data in bytes
 *   out   [caller-owns] Output buffer, must be at least 65 bytes (64 hex + null)
 */
void tt_sha256_hex(const uint8_t *data, size_t len, char out[65]);

#endif /* TT_SHA256_H */
