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

#ifndef OMAHA_BASE_SECURITY_PBKDF2_H_
#define OMAHA_BASE_SECURITY_PBKDF2_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Implements PBKDF2-HMAC-SHA256 per RFC 2898. dk is assumed to point to at
// least dkLen bytes of space.
void pbkdf2_hmac_sha256(const uint8_t* password, uint32_t password_len,
                        const uint8_t* salt, uint32_t salt_len,
                        uint32_t count, uint32_t dkLen, uint8_t* dk);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // OMAHA_BASE_SECURITY_PBKDF2_H_

