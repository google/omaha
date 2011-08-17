// Copyright 2007-2009 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// ========================================================================
//
// Optimized for minimal code size.

#include "md5.h"

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#define rol(bits, value) (((value) << (bits)) | ((value) >> (32 - (bits))))

static const char Kr[64] =
{
  7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,
  5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,
  4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,
  6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21
};

static const int KK[64] =
{
  0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
  0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
  0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
  0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
  0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
  0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
  0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
  0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
  0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
  0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
  0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
  0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
  0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
  0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
  0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
  0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
};

static void MD5_Transform(MD5_CTX* ctx) {
  uint32_t W[64];
  uint32_t A, B, C, D;
  uint8_t* p = ctx->buf;
  int t;

  for(t = 0; t < 16; ++t) {
    uint32_t tmp =  *p++;
    tmp |= *p++ << 8;
    tmp |= *p++ << 16;
    tmp |= *p++ << 24;
    W[t] = tmp;
  }

  A = ctx->state[0];
  B = ctx->state[1];
  C = ctx->state[2];
  D = ctx->state[3];

  for(t = 0; t < 64; t++) {
    uint32_t f, tmp;
    int g;

    if (t < 16) {
      f = (D^(B&(C^D)));
      g = t;
    } else if ( t < 32) {
      f = (C^(D&(B^C)));
      g = (5*t + 1) & 15;
    } else if ( t < 48) {
      f = (B^C^D);
      g = (3*t + 5) & 15;
    } else {
      f = (C^(B|(~D)));
      g = (7*t) & 15;
    }

    tmp = D;
    D = C;
    C = B;
    B = B + rol(Kr[t], (A+f+KK[t]+W[g]));
    A = tmp;
  }

  ctx->state[0] += A;
  ctx->state[1] += B;
  ctx->state[2] += C;
  ctx->state[3] += D;
}

static const HASH_VTAB MD5_VTAB = {
  MD5_init,
  MD5_update,
  MD5_final,
  MD5,
  MD5_DIGEST_SIZE
};

void MD5_init(MD5_CTX* ctx) {
  ctx->f = &MD5_VTAB;
  ctx->state[0] = 0x67452301;
  ctx->state[1] = 0xEFCDAB89;
  ctx->state[2] = 0x98BADCFE;
  ctx->state[3] = 0x10325476;
  ctx->count = 0;
}


void MD5_update(MD5_CTX* ctx, const void* data, int len) {
  int i = ctx->count & 63;
  const uint8_t* p = (const uint8_t*)data;

  ctx->count += len;

  while (len--) {
    ctx->buf[i++] = *p++;
    if (i == 64) {
      MD5_Transform(ctx);
      i = 0;
    }
  }
}


const uint8_t* MD5_final(MD5_CTX* ctx) {
  uint8_t* p = ctx->buf;
  uint64_t cnt = ctx->count * 8;
  int i;

  MD5_update(ctx, (uint8_t*)"\x80", 1);
  while ((ctx->count & 63) != 56) {
    MD5_update(ctx, (uint8_t*)"\0", 1);
  }
  for (i = 0; i < 8; ++i) {
    uint8_t tmp = cnt >> (i * 8);
    MD5_update(ctx, &tmp, 1);
  }

  for (i = 0; i < 4; i++) {
    uint32_t tmp = ctx->state[i];
    *p++ = tmp;
    *p++ = tmp >> 8;
    *p++ = tmp >> 16;
    *p++ = tmp >> 24;
  }

  return ctx->buf;
}


/* Convenience function */
const uint8_t* MD5(const void* data, int len, uint8_t* digest) {
  MD5_CTX ctx;
  MD5_init(&ctx);
  MD5_update(&ctx, data, len);
  memcpy(digest, MD5_final(&ctx), MD5_DIGEST_SIZE);
  return digest;
}
