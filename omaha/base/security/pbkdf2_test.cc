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

#include "security/util/lite/pbkdf2.h"
#include "strings/escaping.h"
#include "testing/base/public/gunit.h"

namespace {

class Pbkdf2Test : public ::testing::Test {
 protected:
};

struct TestVector {
  const char* password;
  const uint32_t password_len;
  const char* salt;
  const uint32_t salt_len;
  const uint32_t count;
  const uint32_t dkLen;
  const char* expected_dk;
};

static const struct TestVector vectors[] = {
  { "password", 8, "salt", 4, 1, 32,
    "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b" },
  { "password", 8, "salt", 4, 2, 32,
    "ae4d0c95af6b46d32d0adff928f06dd02a303f8ef3c251dfd6e2d85a95474c43" },
  { "password", 8, "salt", 4, 4096, 32,
    "c5e478d59288c841aa530db6845c4c8d962893a001ce4e11a4963873aa98134a" },
  { "passwordPASSWORDpassword", 24, "saltSALTsaltSALTsaltSALTsaltSALTsalt", 36,
     4096, 40,
    "348c89dbcbd32b2f32d814b8116e84cf2b17347e"
    "bc1800181c4e2a1fb8dd53e1c635518c7dac47e9" },
  { "pass\0word", 9, "sa\0lt", 5, 4096, 16, "89b69d0516f829893c696226650a8687"},
  { "password", 8, "salt", 4, 1, 20,
    "120fb6cffcf8b32c43e7225256c4f837a86548c9" },
  { "password", 8, "salt", 4, 2, 20,
    "ae4d0c95af6b46d32d0adff928f06dd02a303f8e" },
  { "password", 8, "salt", 4, 4096, 20,
    "c5e478d59288c841aa530db6845c4c8d962893a0" },
  { "passwordPASSWORDpassword", 24, "saltSALTsaltSALTsaltSALTsaltSALTsalt", 36,
    4096, 25,
    "348c89dbcbd32b2f32d814b8116e84cf2b17347ebc1800181c" },
  { "pass\0word", 9, "sa\0lt", 5, 4096, 16,
    "89b69d0516f829893c696226650a8687" },
};

TEST_F(Pbkdf2Test, KnownTestVectors) {
  uint8_t derived_key[1024];

  for (uint32_t i = 0; i < sizeof(vectors)/sizeof(*vectors); i++) {
    const struct TestVector* v = &vectors[i];

    pbkdf2_hmac_sha256((const uint8_t*)v->password, v->password_len,
                       (const uint8_t*)v->salt, v->salt_len,
                       v->count,
                       v->dkLen, derived_key);

    LOG(INFO) << "derived_key for vector " << i << ": "
              << strings::b2a_hex(string(
                  reinterpret_cast<const char*>(derived_key), v->dkLen));

    EXPECT_EQ(string(v->expected_dk),
              strings::b2a_hex(string(
                  reinterpret_cast<const char*>(derived_key), v->dkLen)));
  }
}

static const struct TestVector slow_vectors[] = {
  { "password", 8, "salt", 4, 16777216, 32,
    "cf81c66fe8cfc04d1f31ecb65dab4089f7f179e89b3b0bcb17ad10e3ac6eba46" },
  { "password", 8, "salt", 4, 16777216, 20,
    "cf81c66fe8cfc04d1f31ecb65dab4089f7f179e8" },
};

// These test vectors take a few minutes each and thus it may not be desired
// to run them every time.
TEST_F(Pbkdf2Test, KnownSlowTestVectors) {
  uint8_t derived_key[1024];

  for (uint32_t i = 0; i < sizeof(slow_vectors)/sizeof(*slow_vectors); i++) {
    const struct TestVector* v = &slow_vectors[i];

    pbkdf2_hmac_sha256((const uint8_t*)v->password, v->password_len,
                       (const uint8_t*)v->salt, v->salt_len,
                       v->count,
                       v->dkLen, derived_key);

    LOG(INFO) << "derived_key for slow_vector " << i << ": "
              << strings::b2a_hex(string(
                  reinterpret_cast<const char*>(derived_key), v->dkLen));

    EXPECT_EQ(string(v->expected_dk),
              strings::b2a_hex(string(
                  reinterpret_cast<const char*>(derived_key), v->dkLen)));
  }
}

}  // namespace
