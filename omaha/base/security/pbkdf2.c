// Copyright 2014 Google Inc.
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

#include <string.h>

#include "pbkdf2.h"
#include "sha256.h"
#include "hmac.h"

// Implemented as per RFC 2898

static void F(const HMAC_CTX* initialized_ctx,
              const uint8_t* salt, uint32_t salt_len,
              uint32_t iterations, uint32_t i,
              uint8_t digest_acc[SHA256_DIGEST_SIZE]  // output
             ) {
  HMAC_CTX ctx;
  uint8_t digest_tmp[SHA256_DIGEST_SIZE];
  uint8_t i_buf[4];
  uint32_t iter;
  uint32_t j;

  i_buf[0] = (uint8_t)((i >> 24) & 0xff);
  i_buf[1] = (uint8_t)((i >> 16) & 0xff);
  i_buf[2] = (uint8_t)((i >> 8) & 0xff);
  i_buf[3] = (uint8_t)(i & 0xff);
  ctx = *initialized_ctx;
  // First iteration uses password as key, salt || big_endian(i) as input
  HMAC_update (&ctx, salt, salt_len);
  HMAC_update (&ctx, i_buf, 4);
  memcpy (digest_tmp, HMAC_final(&ctx), SHA256_DIGEST_SIZE);
  memcpy (digest_acc, digest_tmp, SHA256_DIGEST_SIZE);
  // Next count - 1 iterations use previous digest value.
  // All successive digest values XORed together into digest_acc[umulator]
  for (iter = 1; iter < iterations; iter++) {
    ctx = *initialized_ctx;
    HMAC_update (&ctx, digest_tmp, SHA256_DIGEST_SIZE);
    memcpy (digest_tmp, HMAC_final(&ctx), SHA256_DIGEST_SIZE);
    for (j = 0; j < SHA256_DIGEST_SIZE; j++) {
      digest_acc[j] ^= digest_tmp[j];
    }
  }
}

void pbkdf2_hmac_sha256(const uint8_t* password, uint32_t password_len,
                        const uint8_t* salt, uint32_t salt_len,
                        uint32_t count, uint32_t dkLen, uint8_t* dk) {
  uint32_t i, l, r;
  uint8_t digest_tmp[SHA256_DIGEST_SIZE];  // used for final [partial] block
  HMAC_CTX ctx;

  if (dkLen < 1) {  // If they ask for 0 bytes, there is no work to do
    return;
  }

  // l = ceiling (dkLen / SHA256_DIGEST_SIZE)
  l = (dkLen + SHA256_DIGEST_SIZE - 1) / SHA256_DIGEST_SIZE;
  r = dkLen - (l - 1) * SHA256_DIGEST_SIZE;

  HMAC_SHA256_init (&ctx, password, password_len);

  // First l-1 blocks, indexed from 1
  for (i = 1; i < l; i++) {
    F (&ctx, salt, salt_len, count, i, &dk[(i-1)*SHA256_DIGEST_SIZE]);
  }
  // Final (possibly partial) block of r bytes
  F (&ctx, salt, salt_len, count, i, digest_tmp);
  memcpy (&dk[(l - 1)*SHA256_DIGEST_SIZE], digest_tmp, r);
}
