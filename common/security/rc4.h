// Copyright 2005-2009 Google Inc.
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

#ifndef OMAHA_COMMON_SECURITY_RC4_H__
#define OMAHA_COMMON_SECURITY_RC4_H__

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint8_t S[256];
  uint8_t i;
  uint8_t j;
} RC4_CTX;

void RC4_setKey(RC4_CTX* ctx, const uint8_t* data, int len);
void RC4_discard(RC4_CTX* ctx, int len);
void RC4_crypt(RC4_CTX* ctx, const uint8_t* in, uint8_t* out, int len);
void RC4_stream(RC4_CTX* ctx, uint8_t* out, int len);

#ifdef __cplusplus
}
#endif

#endif  // OMAHA_COMMON_SECURITY_RC4_H__
