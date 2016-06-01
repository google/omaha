// Copyright 2013 Google Inc.
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

#include "omaha/base/security/sha256.h"
#include <cstring>
#include <vector>
#include "omaha/testing/unit_test.h"

namespace omaha {

// This test data from http://en.wikipedia.org/wiki/SHA-2:
struct {
  char* binary;
  byte  hash[32];
} test_hash256[] = {
    "",
    0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14, 0x9a, 0xfb,
    0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24, 0x27, 0xae, 0x41, 0xe4,
    0x64, 0x9b, 0x93, 0x4c, 0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52,
    0xb8, 0x55,
    "The quick brown fox jumps over the lazy dog",
    0xd7, 0xa8, 0xfb, 0xb3, 0x07, 0xd7, 0x80, 0x94, 0x69, 0xca,
    0x9a, 0xbc, 0xb0, 0x08, 0x2e, 0x4f, 0x8d, 0x56, 0x51, 0xe4,
    0x6d, 0x3c, 0xdb, 0x76, 0x2d, 0x02, 0xd0, 0xbf, 0x37, 0xc9,
    0xe5, 0x92,
    "The quick brown fox jumps over the lazy dog.",
    0xef, 0x53, 0x7f, 0x25, 0xc8, 0x95, 0xbf, 0xa7, 0x82, 0x52,
    0x65, 0x29, 0xa9, 0xb6, 0x3d, 0x97, 0xaa, 0x63, 0x15, 0x64,
    0xd5, 0xd7, 0x89, 0xc2, 0xb7, 0x65, 0x44, 0x8c, 0x86, 0x35,
    0xfb, 0x6c
};

TEST(Security, Sha256) {
  const size_t kDigestSize = SHA256_DIGEST_SIZE;

  for (size_t i = 0; i != arraysize(test_hash256); ++i) {
    uint8_t hash[kDigestSize] = {0};
    const int len = static_cast<int>(strlen(test_hash256[i].binary));
    const uint8_t* result = SHA256_hash(test_hash256[i].binary, len, hash);
    EXPECT_EQ(result, hash);
    EXPECT_EQ(0, memcmp(hash, test_hash256[i].hash, kDigestSize));
  }


  for (size_t i = 0; i != arraysize(test_hash256); ++i) {
    const int len = static_cast<int>(strlen(test_hash256[i].binary));

    LITE_SHA256_CTX context = {0};
    SHA256_init(&context);
    SHA256_update(&context, test_hash256[i].binary, len);
    const uint8_t* result = SHA256_final(&context);

    EXPECT_EQ(0, memcmp(result, test_hash256[i].hash, kDigestSize));
  }
}

}  // namespace omaha

