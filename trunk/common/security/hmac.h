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

#ifndef OMAHA_COMMON_SECURITY_HMAC_H__
#define OMAHA_COMMON_SECURITY_HMAC_H__

#include <inttypes.h>
#include "hash-internal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct HMAC_CTX {
  HASH_CTX hash;
  uint8_t opad[64];
} HMAC_CTX;

void HMAC_MD5_init(HMAC_CTX* ctx, const void* key, int len);
void HMAC_SHA_init(HMAC_CTX* ctx, const void* key, int len);
const uint8_t* HMAC_final(HMAC_CTX* ctx);

#define HMAC_update(ctx, data, len) HASH_update(&(ctx)->hash, data, len)
#define HMAC_size(ctx) HASH_size(&(ctx)->hash)

#ifdef __cplusplus
}
#endif

#endif  // OMAHA_COMMON_SECURITY_HMAC_H__
