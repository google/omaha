// Copyright 2008-2009 Google Inc.
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


#ifndef OMAHA_NET_CUP_UTILS_H__
#define OMAHA_NET_CUP_UTILS_H__

#include <atlstr.h>
#include <vector>
#include "base/basictypes.h"

namespace omaha {

namespace cup_utils {

// Takes up to (rsa_key_size - SHA_DIGEST_SIZE) from the caller provided
// input, and creates a padded result (data | SHA1(data)), where the msb
// is keybits-160 and the lsb is 160 SHA1 bits. If less input is provided
// the function pads the input with zeros.
std::vector<uint8> RsaPad(size_t rsa_key_size,
                          const void* data, size_t data_length);

// Encodes a buffer of bytes as B64. This encoder has a few differences from
// the standard: it uses a different char set that is http friendly and it does
// not pad the output with '='.
CStringA B64Encode(const void* data, size_t data_length);

// Encodes a vector of bytes as web safe B64.
CStringA B64Encode(const std::vector<uint8>& data);

// Computes the SHA hash of a vector of bytes: HASH(data).
std::vector<uint8> Hash(const std::vector<uint8>& data);

// Computes the SHA hash of a CStringA: HASH(data).
std::vector<uint8> Hash(const CStringA& data);

// Computes the SHA hash of buffers: HASH(HASH(buf1)|HASH(buf2)|HASH(buf3)).
std::vector<uint8> HashBuffers(const void* buf1, size_t len1,
                               const void* buf2, size_t len2,
                               const void* buf3, size_t len3);

// Computes the HMAC of hashes: SYMSign[key](id|h1|h2|h3)
// NULL arguments are not included in the signature computation.
std::vector<uint8> SymSign(const std::vector<uint8>& key,
                           uint8 id,
                           const std::vector<uint8>* h1,
                           const std::vector<uint8>* h2,
                           const std::vector<uint8>* h3);

// Parses out the c=xxx; from the cookie header.
CString ParseCupCookie(const CString& cookie_header);

}  // namespace cup_utils

}  // namespace omaha

#endif  // OMAHA_NET_CUP_UTILS_H__
