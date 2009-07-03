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

#include "omaha/net/cup_utils.h"

#include <algorithm>
#include <vector>
#include "omaha/common/debug.h"
#include "omaha/common/security/b64.h"
#include "omaha/common/security/hmac.h"
#include "omaha/common/security/sha.h"

namespace omaha {

namespace cup_utils {

std::vector<uint8> RsaPad(size_t rsa_key_size,
                          const void* data, size_t data_length) {
  ASSERT1(rsa_key_size >= SHA_DIGEST_SIZE);
  ASSERT1(data);

  // The result gets padded with zeros if the result size is greater than
  // the size of the buffer provided by the caller.
  const uint8* start = static_cast<const uint8*>(data);
  std::vector<uint8> result(start, start + data_length);
  result.resize(rsa_key_size - SHA_DIGEST_SIZE);

  // The input needs to be smaller than the RSA modulus, which has always the
  // msb set.
  result[0] &= 127;  // Reset msb
  result[0] |= 64;   // Set second highest bit.

  uint8 digest[SHA_DIGEST_SIZE] = {0};
  SHA(&result.front(), result.size(), digest);

  result.insert(result.end(), digest, digest + SHA_DIGEST_SIZE);
  ASSERT1(result.size() == rsa_key_size);
  return result;
}

CStringA B64Encode(const void* data, size_t data_length) {
  ASSERT1(data);

  // After encoding the size grows a little bit, about 133%. We assume 150%
  // bigger size for simplicity. Also, the output length is a multiple of
  // 4 bytes, so the minimum buffer should be at least 5 chars to include
  // space for the string terminator.
  CStringA result;
  const size_t kMinBuffer = 5;
  int result_max_size = std::max(data_length * 3 / 2, kMinBuffer);
  int result_size = B64_encode(static_cast<const uint8*>(data),
                               data_length,
                               CStrBufA(result, result_max_size),
                               result_max_size);
  ASSERT1(result_size == result.GetLength());
  return result;
}

CStringA B64Encode(const std::vector<uint8>& data) {
  return B64Encode(&data.front(), data.size());
}

std::vector<uint8> Hash(const std::vector<uint8>& data) {
  std::vector<uint8>result(SHA_DIGEST_SIZE);
  SHA(data.empty() ? NULL : &data.front(), data.size(), &result.front());
  return result;
}

std::vector<uint8> Hash(const CStringA& data) {
  std::vector<uint8>result(SHA_DIGEST_SIZE);
  SHA(data.GetString(), data.GetLength(), &result.front());
  return result;
}

std::vector<uint8> HashBuffers(const void* buf1, size_t len1,
                               const void* buf2, size_t len2,
                               const void* buf3, size_t len3) {
  SHA_CTX sha_ctx = {0};
  SHA_init(&sha_ctx);
  const void* buffers[] = {buf1, buf2, buf3};
  const size_t lengths[] = {len1, len2, len3};
  for (size_t i = 0; i != arraysize(buffers); ++i) {
    uint8 hash[SHA_DIGEST_SIZE] = {0};
    SHA(buffers[i], lengths[i], hash);
    SHA_update(&sha_ctx, hash, sizeof(hash));
  }
  std::vector<uint8> result(SHA_DIGEST_SIZE);
  memcpy(&result.front(), SHA_final(&sha_ctx), result.size());
  return result;
}

std::vector<uint8> SymSign(const std::vector<uint8>& key,
                           uint8 id,
                           const std::vector<uint8>* h1,
                           const std::vector<uint8>* h2,
                           const std::vector<uint8>* h3) {
  HMAC_CTX hmac_ctx = {0};
  HMAC_SHA_init(&hmac_ctx, &key.front(), key.size());
  HMAC_update(&hmac_ctx, &id, sizeof(id));
  const std::vector<uint8>* args[] = {h1, h2, h3};
  for (size_t i = 0; i != arraysize(args); ++i) {
    if (args[i]) {
      ASSERT1(args[i]->size() == SHA_DIGEST_SIZE);
      HMAC_update(&hmac_ctx, &args[i]->front(), args[i]->size());
    }
  }
  std::vector<uint8> result(SHA_DIGEST_SIZE);
  memcpy(&result.front(), HMAC_final(&hmac_ctx), result.size());
  return result;
}

// Looks for "c=", extracts a substring up to ';', which is the attribute
// delimiter, and trims the white spaces at the end. If found, it returns
// the string corresponding to "c=xxx", otherwise it returns an empty string.
CString ParseCupCookie(const CString& cookie_header) {
  CString cookie;
  int start = cookie_header.Find(_T("c="));
  if (start != -1) {
    int end = cookie_header.Find(_T(';'), start);
    if (end != -1) {
      cookie = cookie_header.Mid(start, end - start);
    } else {
      cookie = cookie_header.Mid(start);
    }
    cookie.TrimRight(_T(' '));
  }
  return cookie;
}

}  // namespace cup_utils

}  // namespace omaha

