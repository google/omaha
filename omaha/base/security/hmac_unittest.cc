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

#include <string>

#include "omaha/base/security/hmac.h"
#include "omaha/base/string.h"
#include "omaha/testing/unit_test.h"

using std::string;

namespace {

class HmacTest : public testing::Test {};

// Known answers test from RFC2202 for both md5 and sha1 based HMACs.
// And from RFC4231 for sha256 HMAC.
static const struct KAT {
  const char* key;
  int md5_keylength;  // md5 test keys are sometimes shorter.
  const char* data;
  const char* md5;
  const char* sha256;
} KATS[] = {
    {"x0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b", 16, "Hi There",
     "9294727a3638bb1c13f48ef8158bfc9d",
     "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7"},
    {"Jefe", 4, "what do ya want for nothing?",
     "750c783e6ab0b503eaa86e310a5db738",
     "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843"},
    {"xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 16,
     "xdddddddddddddddddddddddddddddddddddddddddddddddddd"
     "dddddddddddddddddddddddddddddddddddddddddddddddddd",
     "56be34521d144c88dbb8c733f0e8b3f6",
     "773ea91e36800e46854db8ebd09181a72959098b3ef8c122d9635514ced565fe"},
    {"x0102030405060708090a0b0c0d0e0f10111213141516171819", 25,
     "xcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcd"
     "cdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcd",
     "697eaf0aca3a3aea3a75164746ffaa79",
     "82558a389a443c0ea4cc819899f2083a85f0faa3e578f8077a2e3ff46729665b"},
    {"x0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c", 16, "Test With Truncation",
     "56461ef2342edc00f9bab995690efd4c",
     "a3b6167473100ee06e0c796c2955552bfa6f7c0a6a8aef8b93f860aab0cd20c5"},
    {"xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
     80, "Test Using Larger Than Block-Size Key - Hash Key First",
     "6b1ab7fe4bd7bf8f0b62e6ce61b9d0cd",
     nullptr},
    {"xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaa",
     8 * 16 + 3, "Test Using Larger Than Block-Size Key - Hash Key First",
     nullptr,
     "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54"},
    {"xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
     80,
     "Test Using Larger Than Block-Size Key and Larger Than One Block-Size "
     "Data",
     "6f630fad67cda0ee1fb1f562db3aa53e",
     nullptr},
    {"xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaa",
     8 * 16 + 3,
     "This is a test using a larger than block-size key and a larger t"
     "han block-size data. The key needs to be hashed before being use"
     "d by the HMAC algorithm.",
     nullptr,
     "9b09ffa71b942fcb27635fbcd5b0e944bfdc63644f0713938a7f51535c3a35e2"},
    {nullptr}};

TEST_F(HmacTest, RFC2202andRFC4131) {
  // This tests against the RFC2202 & RFC4131 test vectors
  LITE_HMAC_CTX hmac;

  for (const struct KAT* katp = KATS; katp->key; ++katp) {
    string key = katp->key[0] ==
        'x' ? omaha::a2b_hex(katp->key + 1) : katp->key;
    string data = katp->data[0] ==
        'x' ? omaha::a2b_hex(katp->data + 1) : katp->data;

    if (katp->sha256) {
      HMAC_SHA256_init(&hmac, key.data(), key.size());
      HMAC_update(&hmac, data.data(), data.size());

      EXPECT_EQ(omaha::b2a_hex(
          reinterpret_cast<const char*>(HMAC_final(&hmac)),
          HMAC_size(&hmac)),
          katp->sha256);
    }
  }
}

}  // namespace
