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

#include <windows.h>
#include <cstring>
#include <vector>
#include "omaha/common/security/sha.h"
#include "omaha/common/string.h"
#include "omaha/common/utils.h"
#include "omaha/net/cup_utils.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace cup_utils {

namespace {

const char kPlainText[] = "The quick brown fox jumps over the lazy dog";
const int kRsaKeySize = 128;         // 128 bytes.

// Test padding for a random message of num_bytes length.
void TestPadding(size_t num_bytes) {
  std::vector<uint8> data(num_bytes);
  EXPECT_TRUE(GenRandom(&data.front(), data.size()));
  std::vector<uint8> m = RsaPad(kRsaKeySize, &data.front(), data.size());
  EXPECT_EQ(m.size(), kRsaKeySize);

  uint8 hash[SHA_DIGEST_SIZE] = {0};
  SHA(&m.front(), m.size() - SHA_DIGEST_SIZE, hash);
  EXPECT_EQ(memcmp(hash, &m[kRsaKeySize - SHA_DIGEST_SIZE], sizeof(hash)), 0);

  EXPECT_FALSE(m[0] & 0x80);    // msb is always reset.
  EXPECT_TRUE(m[0] & 0x60);     // bit next to msb is always set.
}

}  // namespace


TEST(CupUtilsTest, RsaPad) {
  EXPECT_GE(kRsaKeySize, SHA_DIGEST_SIZE);

  TestPadding(1);                 // 1 byte.
  TestPadding(SHA_DIGEST_SIZE);   // 20 bytes.
  TestPadding(kRsaKeySize);       // 128 bytes.
  TestPadding(1000);              // 1000 bytes.
}

// The underlying B64_encode function in common/security does not pad
// with '='.
TEST(CupUtilsTest, B64Encode) {
  const char* bytes = "1";
  EXPECT_STREQ(B64Encode(bytes, strlen(bytes)), "MQ");
  bytes = "12";
  EXPECT_STREQ(B64Encode(bytes, strlen(bytes)), "MTI");
  bytes = "123";
  EXPECT_STREQ(B64Encode(bytes, strlen(bytes)), "MTIz");
  bytes = "Google Inc";
  EXPECT_STREQ(B64Encode(bytes, strlen(bytes)), "R29vZ2xlIEluYw");

  const uint8* first = reinterpret_cast<const uint8*>(bytes);
  const uint8* last = first + strlen(bytes);
  EXPECT_STREQ(B64Encode(std::vector<uint8>(first, last)), "R29vZ2xlIEluYw");
}

TEST(CupUtilsTest, Hash) {
  // Empty vector.
  std::vector<uint8> data;
  std::vector<uint8> hash = Hash(data);
  EXPECT_STREQ(BytesToHex(hash),
               _T("da39a3ee5e6b4b0d3255bfef95601890afd80709"));

  // Non-empty vector.
  const uint8* first = reinterpret_cast<const uint8*>(kPlainText);
  const uint8* last  = first + strlen(kPlainText);
  data.clear();
  data.insert(data.begin(), first, last);
  hash = Hash(data);
  EXPECT_STREQ(BytesToHex(hash),
               _T("2fd4e1c67a2d28fced849ee1bb76e7391b93eb12"));

  // Empty CString.
  hash = Hash(CStringA());
  EXPECT_STREQ(BytesToHex(hash),
               _T("da39a3ee5e6b4b0d3255bfef95601890afd80709"));

  // Non-empty CString.
  hash = Hash(CStringA(kPlainText));
  EXPECT_STREQ(BytesToHex(hash),
               _T("2fd4e1c67a2d28fced849ee1bb76e7391b93eb12"));

  // Hash of strings
  CStringA arg(kPlainText);
  hash = HashBuffers(arg.GetString(), arg.GetLength(), NULL, 0, NULL, 0);
  EXPECT_STREQ(BytesToHex(hash),
               _T("64eb91d899d68d2af394fbfe8f7c3b2884055ddb"));

  hash = HashBuffers(NULL, 0, arg.GetString(), arg.GetLength(), NULL, 0);
  EXPECT_STREQ(BytesToHex(hash),
               _T("6c9f398d556ffe64fe441ca5491e25fd67d2ab65"));

  hash = HashBuffers(NULL, 0, NULL, 0, arg.GetString(), arg.GetLength());
  EXPECT_STREQ(BytesToHex(hash),
               _T("9a7e591318dc169fc023414d8d25321bcb11121e"));

  hash = HashBuffers(NULL, 0, NULL, 0, NULL, 0);
  EXPECT_STREQ(BytesToHex(hash),
               _T("d0dc1cf9bf61884f8e7982e0b1b87954bd9ee9c7"));
}

TEST(CupUtilsTest, SymSign) {
  // Sign of vectors: ignore NULL vectors.
  std::vector<uint8> key;  // This is the signing key.
  key.push_back(7);

  CStringA arg(kPlainText);
  std::vector<uint8> hash = Hash(arg);
  std::vector<uint8> sym_sign = SymSign(key, 1, &hash, NULL, NULL);
  EXPECT_STREQ(BytesToHex(sym_sign),
               _T("1c6fa359c0a7b11421ae4d31a92f9de8a4ef8d2d"));

  sym_sign = SymSign(key, 1, NULL, &hash, NULL);
  EXPECT_STREQ(BytesToHex(sym_sign),
               _T("1c6fa359c0a7b11421ae4d31a92f9de8a4ef8d2d"));

  sym_sign = SymSign(key, 1, NULL, NULL, &hash);
  EXPECT_STREQ(BytesToHex(sym_sign),
               _T("1c6fa359c0a7b11421ae4d31a92f9de8a4ef8d2d"));

  sym_sign = SymSign(key, 1, &hash, &hash, &hash);
    EXPECT_STREQ(BytesToHex(sym_sign),
                 _T("fef3e343795946cd47ff4e07eca5f3f09b051d86"));
}

TEST(CupUtilsTest, ParseCupCookie) {
  EXPECT_STREQ(_T(""), ParseCupCookie(_T("")));
  EXPECT_STREQ(_T(""), ParseCupCookie(_T("foo; bar;")));
  EXPECT_STREQ(_T("c="), ParseCupCookie(_T("c=")));
  EXPECT_STREQ(_T("c="), ParseCupCookie(_T("c= ;")));
  EXPECT_STREQ(_T("c=foo"), ParseCupCookie(_T("c=foo")));
  EXPECT_STREQ(_T("c=foo"), ParseCupCookie(_T(";c=foo")));
  EXPECT_STREQ(_T("c=foo"), ParseCupCookie(_T("c=foo ; bar;")));
  EXPECT_STREQ(_T("c=foo"), ParseCupCookie(_T("b=bar; c=foo;")));
  EXPECT_STREQ(_T("c=foo"), ParseCupCookie(_T("b=bar; c=foo ; foobar")));
}

}  // namespace cup_utils

}  // namespace omaha
