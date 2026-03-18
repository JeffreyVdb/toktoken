/*
 * SHA-256 -- Standalone implementation for TokToken
 *
 * Original implementation for the TokToken project.
 * Based on the algorithm described in FIPS PUB 180-4.
 *
 * License: same as TokToken project (MIT)
 */

#include "sha256.h"
#include <string.h>

/* SHA-256 round constants */
static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define EP1(x) (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define SIG0(x) (ROTR(x, 7) ^ ROTR(x, 18) ^ ((x) >> 3))
#define SIG1(x) (ROTR(x, 17) ^ ROTR(x, 19) ^ ((x) >> 10))

typedef struct {
    uint8_t block[64];
    uint32_t h[8];
    uint64_t total_len;
    size_t block_len;
} sha256_ctx;

static void sha256_transform(sha256_ctx *ctx)
{
    uint32_t w[64];
    uint32_t a, b, c, d, e, f, g, h;
    uint32_t t1, t2;
    int i;

    /* Prepare message schedule */
    for (i = 0; i < 16; i++) {
        w[i] = ((uint32_t)ctx->block[i * 4 + 0] << 24)
             | ((uint32_t)ctx->block[i * 4 + 1] << 16)
             | ((uint32_t)ctx->block[i * 4 + 2] << 8)
             | ((uint32_t)ctx->block[i * 4 + 3]);
    }
    for (i = 16; i < 64; i++) {
        w[i] = SIG1(w[i - 2]) + w[i - 7] + SIG0(w[i - 15]) + w[i - 16];
    }

    /* Initialize working variables */
    a = ctx->h[0];
    b = ctx->h[1];
    c = ctx->h[2];
    d = ctx->h[3];
    e = ctx->h[4];
    f = ctx->h[5];
    g = ctx->h[6];
    h = ctx->h[7];

    /* Compression */
    for (i = 0; i < 64; i++) {
        t1 = h + EP1(e) + CH(e, f, g) + K[i] + w[i];
        t2 = EP0(a) + MAJ(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    ctx->h[0] += a;
    ctx->h[1] += b;
    ctx->h[2] += c;
    ctx->h[3] += d;
    ctx->h[4] += e;
    ctx->h[5] += f;
    ctx->h[6] += g;
    ctx->h[7] += h;
}

static void sha256_init(sha256_ctx *ctx)
{
    ctx->h[0] = 0x6a09e667;
    ctx->h[1] = 0xbb67ae85;
    ctx->h[2] = 0x3c6ef372;
    ctx->h[3] = 0xa54ff53a;
    ctx->h[4] = 0x510e527f;
    ctx->h[5] = 0x9b05688c;
    ctx->h[6] = 0x1f83d9ab;
    ctx->h[7] = 0x5be0cd19;
    ctx->total_len = 0;
    ctx->block_len = 0;
}

static void sha256_update(sha256_ctx *ctx, const uint8_t *data, size_t len)
{
    size_t i;

    for (i = 0; i < len; i++) {
        ctx->block[ctx->block_len++] = data[i];
        if (ctx->block_len == 64) {
            sha256_transform(ctx);
            ctx->total_len += 512;
            ctx->block_len = 0;
        }
    }
}

static void sha256_final(sha256_ctx *ctx, uint8_t out[32])
{
    size_t i;
    uint64_t total_bits;

    i = ctx->block_len;
    total_bits = ctx->total_len + ctx->block_len * 8;

    /* Padding */
    ctx->block[i++] = 0x80;
    if (i > 56) {
        while (i < 64) {
            ctx->block[i++] = 0x00;
        }
        sha256_transform(ctx);
        i = 0;
    }
    while (i < 56) {
        ctx->block[i++] = 0x00;
    }

    /* Append length in bits (big-endian) */
    ctx->block[56] = (uint8_t)(total_bits >> 56);
    ctx->block[57] = (uint8_t)(total_bits >> 48);
    ctx->block[58] = (uint8_t)(total_bits >> 40);
    ctx->block[59] = (uint8_t)(total_bits >> 32);
    ctx->block[60] = (uint8_t)(total_bits >> 24);
    ctx->block[61] = (uint8_t)(total_bits >> 16);
    ctx->block[62] = (uint8_t)(total_bits >> 8);
    ctx->block[63] = (uint8_t)(total_bits);
    sha256_transform(ctx);

    /* Produce output (big-endian) */
    for (i = 0; i < 8; i++) {
        out[i * 4 + 0] = (uint8_t)(ctx->h[i] >> 24);
        out[i * 4 + 1] = (uint8_t)(ctx->h[i] >> 16);
        out[i * 4 + 2] = (uint8_t)(ctx->h[i] >> 8);
        out[i * 4 + 3] = (uint8_t)(ctx->h[i]);
    }
}

void tt_sha256(const uint8_t *data, size_t len, uint8_t out[32])
{
    sha256_ctx ctx;
    sha256_init(&ctx);
    if (data != NULL && len > 0) {
        sha256_update(&ctx, data, len);
    }
    sha256_final(&ctx, out);
}

void tt_sha256_hex(const uint8_t *data, size_t len, char out[65])
{
    static const char hex[] = "0123456789abcdef";
    uint8_t hash[32];
    int i;

    tt_sha256(data, len, hash);
    for (i = 0; i < 32; i++) {
        out[i * 2]     = hex[(hash[i] >> 4) & 0x0f];
        out[i * 2 + 1] = hex[hash[i] & 0x0f];
    }
    out[64] = '\0';
}
