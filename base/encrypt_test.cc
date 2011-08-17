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

#include <omaha/base/encrypt.h>
#include <cstring>
#include <vector>
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace encrypt {

TEST(EncryptTest, Test) {
  const char* plaintext = "the quick brown fox jumps over the lazy dog";
  std::vector<uint8> ciphertext;
  EXPECT_HRESULT_SUCCEEDED(EncryptData(NULL, 0,
                                       plaintext, strlen(plaintext),
                                       &ciphertext));
  std::vector<uint8> decrypted_text;
  EXPECT_HRESULT_SUCCEEDED(DecryptData(NULL, 0,
                                       &ciphertext.front(), ciphertext.size(),
                                       &decrypted_text));
  EXPECT_EQ(decrypted_text.size(), strlen(plaintext));
  decrypted_text.push_back(0);
  EXPECT_STREQ(reinterpret_cast<char*>(&decrypted_text.front()), plaintext);


  const char* key = "foobar";
  ciphertext.clear();
  EXPECT_HRESULT_SUCCEEDED(EncryptData(key, strlen(key),
                                       plaintext, strlen(plaintext),
                                       &ciphertext));
  decrypted_text.clear();
  EXPECT_HRESULT_SUCCEEDED(DecryptData(key, strlen(key),
                                       &ciphertext.front(), ciphertext.size(),
                                       &decrypted_text));
  EXPECT_EQ(decrypted_text.size(), strlen(plaintext));
  decrypted_text.push_back(0);
  EXPECT_STREQ(reinterpret_cast<char*>(&decrypted_text.front()), plaintext);
}

}  // namespace encrypt

}  // namespace omaha

