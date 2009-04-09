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

#include "hmac.h"

#include <memory.h>
#include "sha.h"
#include "md5.h"

static void HMAC_init(HMAC_CTX* ctx, const void* key, int len) {
  int i;
  memset(&ctx->opad[0], 0, sizeof(ctx->opad));

  if (len > sizeof(ctx->opad)) {
    HASH_init(&ctx->hash);
    HASH_update(&ctx->hash, key, len);
    memcpy(&ctx->opad[0], HASH_final(&ctx->hash), HASH_size(&ctx->hash));
  } else {
    memcpy(&ctx->opad[0], key, len);
  }

  for (i = 0; i < sizeof(ctx->opad); ++i) {
    ctx->opad[i] ^= 0x36;
  }

  HASH_init(&ctx->hash);
  HASH_update(&ctx->hash, ctx->opad, sizeof(ctx->opad));  // hash ipad

  for (i = 0; i < sizeof(ctx->opad); ++i) {
    ctx->opad[i] ^= (0x36 ^ 0x5c);
  }
}

void HMAC_MD5_init(HMAC_CTX* ctx, const void* key, int len) {
  MD5_init(&ctx->hash);
  HMAC_init(ctx, key, len);
}

void HMAC_SHA_init(HMAC_CTX* ctx, const void* key, int len) {
  SHA_init(&ctx->hash);
  HMAC_init(ctx, key, len);
}

const uint8_t* HMAC_final(HMAC_CTX* ctx) {
  uint8_t digest[32];  // upto SHA2
  memcpy(digest, HASH_final(&ctx->hash),
         (HASH_size(&ctx->hash) <= sizeof(digest) ?
             HASH_size(&ctx->hash) : sizeof(digest)));
  HASH_init(&ctx->hash);
  HASH_update(&ctx->hash, ctx->opad, sizeof(ctx->opad));
  HASH_update(&ctx->hash, digest, HASH_size(&ctx->hash));
  memset(&ctx->opad[0], 0, sizeof(ctx->opad));  // wipe key
  return HASH_final(&ctx->hash);
}
