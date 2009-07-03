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

#include "rc4.h"

#include <inttypes.h>

void RC4_setKey(RC4_CTX* ctx, const uint8_t* key, int len) {
  uint8_t* S = ctx->S;
  int i, j;

  for (i = 0; i < 256; ++i) {
    S[i] = i;
  }

  j = 0;
  for (i = 0; i < 256; ++i) {
    uint8_t tmp;

    j = (j + S[i] + key[i % len]) & 255;

    tmp = S[i];
    S[i] = S[j];
    S[j] = tmp;
  }

  ctx->i = 0;
  ctx->j = 0;
}

void RC4_crypt(RC4_CTX* ctx,
               const uint8_t *in,
               uint8_t* out,
               int len) {
  uint8_t i = ctx->i;
  uint8_t j = ctx->j;
  uint8_t* S = ctx->S;

  int n;

  for (n = 0; n < len; ++n) {
    uint8_t tmp;

    i = (i + 1) & 255;
    j = (j + S[i]) & 255;

    tmp = S[i];
    S[i] = S[j];
    S[j] = tmp;

    if (in) {
      if (out) {
        out[n] = in[n] ^ S[(S[i] + S[j]) & 255];
      }
    } else {
      if (out) {
        out[n] = S[(S[i] + S[j]) & 255];
      }
    }
  }

  ctx->i = i;
  ctx->j = j;
}

void RC4_discard(RC4_CTX* ctx, int len) {
  RC4_crypt(ctx, 0, 0, len);
}

void RC4_stream(RC4_CTX* ctx, uint8_t* out, int len) {
  RC4_crypt(ctx, 0, out, len);
}
