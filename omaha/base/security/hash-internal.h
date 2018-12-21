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

#ifndef OMAHA_BASE_SECURITY_HASH_INTERNAL_H_
#define OMAHA_BASE_SECURITY_HASH_INTERNAL_H_

#include <stddef.h>
#include <stdint.h>

#ifdef LITE_EMULATED_64BIT_OPS
#define LITE_LShiftU64(a, b) LShiftU64((a), (b))
#define LITE_RShiftU64(a, b) RShiftU64((a), (b))
#else
#define LITE_LShiftU64(a, b) ((a) << (b))
#define LITE_RShiftU64(a, b) ((a) >> (b))
#endif  // LITE_EMULATED_64BIT_OPS

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

struct HASH_CTX;  // forward decl

typedef struct HASH_VTAB {
  void (* const init)(struct HASH_CTX*);
  void (* const update)(struct HASH_CTX*, const void*, size_t);
  const uint8_t* (* const final)(struct HASH_CTX*);
  const uint8_t* (* const hash)(const void*, size_t, uint8_t*);
  unsigned int size;
} HASH_VTAB;

typedef struct HASH_CTX {
  const HASH_VTAB * f;
  uint64_t count;
#ifndef SHA512_SUPPORT
  uint8_t buf[64];
  uint32_t state[8];  // upto SHA2-256
#else
  uint8_t buf[128];
  uint64_t state[8];  // upto SHA2-512
#endif
} HASH_CTX;

#define HASH_init(ctx) (ctx)->f->init(ctx)
#define HASH_update(ctx, data, len) (ctx)->f->update(ctx, data, len)
#define HASH_final(ctx) (ctx)->f->final(ctx)
#define HASH_hash(data, len, digest) (ctx)->f->hash(data, len, digest)
#define HASH_size(ctx) (ctx)->f->size

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // OMAHA_BASE_SECURITY_HASH_INTERNAL_H_
