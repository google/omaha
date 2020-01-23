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

#ifndef OMAHA_BASE_SECURITY_HMAC_H_
#define OMAHA_BASE_SECURITY_HMAC_H_

#include <stdint.h>
#include "hash-internal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LITE_HMAC_CTX {
  HASH_CTX hash;
  uint8_t opad[64];
} LITE_HMAC_CTX;

void HMAC_SHA256_init(LITE_HMAC_CTX* ctx, const void* key, unsigned int len);
const uint8_t* HMAC_final(LITE_HMAC_CTX* ctx);

#define HMAC_update(ctx, data, len) HASH_update(&(ctx)->hash, data, len)
#define HMAC_size(ctx) HASH_size(&(ctx)->hash)

#ifdef __cplusplus
}
#endif

#endif  // OMAHA_BASE_SECURITY_HMAC_H_
