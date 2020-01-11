// Copyright 2003-2009 Google Inc.
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
//
// signatures_unittest.cpp
//
// Unittests for classes and functions related to crypto-hashes of buffers and
// digital signatures of buffers.
// TODO(omaha): There are a number of places inside the signatures code, where
// empty vector iterators were being dereferenced. Ensure that all these are
// being tested.

#include <cstring>
#include <vector>
#include "omaha/base/app_util.h"
#include "omaha/base/error.h"
#include "omaha/base/path.h"
#include "omaha/base/signatures.h"
#include "omaha/base/string.h"
#include "omaha/base/utils.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace {

// This test data from http://en.wikipedia.org/wiki/SHA-2:
struct {
  const char* binary;
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


}  // namespace

TEST(SignaturesTest, CryptoHashSha256) {
  CryptoHash chash;

  for (size_t i = 0; i != arraysize(test_hash256); i++) {
    std::vector<byte> buffer(strlen(test_hash256[i].binary));
    if (buffer.size()) {
      memcpy(&buffer.front(),
             test_hash256[i].binary,
             strlen(test_hash256[i].binary));
    }
    std::vector<byte> hash;
    ASSERT_SUCCEEDED(chash.Compute(buffer, &hash));
    ASSERT_EQ(hash.size(), chash.hash_size());
    ASSERT_EQ(0, memcmp(&hash.front(),
                        test_hash256[i].hash,
                        hash.size()));
    ASSERT_SUCCEEDED(chash.Validate(buffer, hash));
  }
}

TEST(SignaturesTest, VerifyFileHashSha256) {
  const CString executable_path(app_util::GetCurrentModuleDirectory());

  const CString source_file1 = ConcatenatePath(
      executable_path,
      _T("unittest_support\\download_cache_test\\")
      _T("{89640431-FE64-4da8-9860-1A1085A60E13}\\gears-win32-opt.msi"));

  const CString hash_file1 =
      _T("49b45f78865621b154fa65089f955182345a67f9746841e43e2d6daa288988d0");

  const CString source_file2 = ConcatenatePath(
       executable_path,
       _T("unittest_support\\download_cache_test\\")
       _T("{7101D597-3481-4971-AD23-455542964072}\\livelysetup.exe"));

  const CString hash_files =
      _T("d5e06b4436c5e33f2de88298b890f47815fc657b63b3050d2217c55a5d0730b0");

  const CString bad_hash = _T("00bad000");

  std::vector<CString> files;

  // Authenticate one file.
  files.push_back(source_file1);
  EXPECT_HRESULT_SUCCEEDED(VerifyFileHashSha256(files, hash_file1));

  // Incorrect hash.
  EXPECT_EQ(SIGS_E_INVALID_SIGNATURE, VerifyFileHashSha256(files, hash_files));

  // Bad hash.
  EXPECT_EQ(E_INVALIDARG, VerifyFileHashSha256(files, bad_hash));
  EXPECT_EQ(E_INVALIDARG, VerifyFileHashSha256(files, _T("")));

  // Authenticate two files.
  files.push_back(source_file2);
  EXPECT_HRESULT_SUCCEEDED(VerifyFileHashSha256(files, hash_files));

  // Round trip through CryptoHash::Compute to verify the hash of two files.
  CryptoHash crypto;
  std::vector<byte> hash_out;
  EXPECT_HRESULT_SUCCEEDED(crypto.Compute(files, 0, &hash_out));

  std::string actual_hash_files;
  b2a_hex(&hash_out[0], &actual_hash_files, hash_out.size());
  EXPECT_STREQ(hash_files, CString(actual_hash_files.c_str()));
}

}  // namespace omaha

