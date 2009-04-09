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

#ifndef OMAHA_COMMON_SECURITY_AES_H__
#define OMAHA_COMMON_SECURITY_AES_H__

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

// Encrypt a block.
// Both key and block size are 128 bits.
void AES_encrypt_block(const uint8_t* key,
                       const uint8_t* in,
                       uint8_t* out );

#define AES_BLOCK_SIZE 16

#ifdef __cplusplus
}
#endif

#endif  // OMAHA_COMMON_SECURITY_AES_H__
