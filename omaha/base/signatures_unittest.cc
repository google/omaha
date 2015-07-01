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
#include "omaha/base/module_utils.h"
#include "omaha/base/path.h"
#include "omaha/base/signatures.h"
#include "omaha/base/utils.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace {

struct {
  char* binary;
  char* base64;
} test_data[] = {
  "",                                "",
  "what",                            "d2hhdA==",
  "what will print out",             "d2hhdCB3aWxsIHByaW50IG91dA==",
  "foobar",                          "Zm9vYmFy",
  "a man, a plan, a canal: panama!", "YSBtYW4sIGEgcGxhbiwgYSBjYW5hbDogcGFuYW1hIQ==",    // NOLINT
};

// This test data from http://en.wikipedia.org/wiki/SHA-1:
struct {
  char* binary;
  byte  hash[20];
} test_hash[] = {
  "The quick brown fox jumps over the lazy dog",
    0x2f, 0xd4, 0xe1, 0xc6, 0x7a, 0x2d, 0x28, 0xfc, 0xed, 0x84,
    0x9e, 0xe1, 0xbb, 0x76, 0xe7, 0x39, 0x1b, 0x93, 0xeb, 0x12,
  "The quick brown fox jumps over the lazy cog",
    0xde, 0x9f, 0x2c, 0x7f, 0xd2, 0x5e, 0x1b, 0x3a, 0xfa, 0xd3,
    0xe8, 0x5a, 0x0b, 0xd1, 0x7d, 0x9b, 0x10, 0x0d, 0xb4, 0xb3,
};

}  // namespace

TEST(SignaturesTest, Base64) {
  for (size_t i = 0; i != arraysize(test_data); i++) {
    std::vector<byte> buffer(strlen(test_data[i].binary));
    if (strlen(test_data[i].binary) != 0) {
      memcpy(&buffer.front(), test_data[i].binary, strlen(test_data[i].binary));
    }
    CStringA test_e;
    ASSERT_SUCCEEDED(Base64::Encode(buffer, &test_e));
    ASSERT_STREQ(test_e, test_data[i].base64);
    std::vector<byte> test_d;
    uint32 test_d_written = 0;
    ASSERT_SUCCEEDED(Base64::Decode(test_e, &test_d));
    ASSERT_EQ(test_d.size(), strlen(test_data[i].binary));
    if (strlen(test_data[i].binary) != 0) {
      ASSERT_EQ(0, memcmp(&test_d.front(),
                          test_data[i].binary,
                          strlen(test_data[i].binary)));
    }
  }
}

TEST(SignaturesTest, CryptoHash) {
  CryptoHash chash;
  for (size_t i = 0; i != arraysize(test_hash); i++) {
    std::vector<byte> buffer(strlen(test_hash[i].binary));
    memcpy(&buffer.front(), test_hash[i].binary, strlen(test_hash[i].binary));
    std::vector<byte> hash;
    ASSERT_SUCCEEDED(chash.Compute(buffer, &hash));
    ASSERT_EQ(hash.size(), CryptoHash::kHashSize);
    ASSERT_EQ(0, memcmp(&hash.front(),
                        test_hash[i].hash,
                        CryptoHash::kHashSize));
    ASSERT_SUCCEEDED(chash.Validate(buffer, hash));
  }
}

TEST(SignaturesTest, CreationVerification) {
  TCHAR module_directory[MAX_PATH] = {0};
  ASSERT_TRUE(GetModuleDirectory(NULL, module_directory));
  CString directory;
  directory.Format(_T("%s\\unittest_support"), module_directory);

  CString encoded_cert_with_private_key_path;
  encoded_cert_with_private_key_path.AppendFormat(
      _T("%s\\certificate-with-private-key.pfx"), directory);
  CString encoded_cert_without_private_key_path;
  encoded_cert_without_private_key_path.AppendFormat(
      _T("%s\\certificate-without-private-key.cer"), directory);
  CString raw_test_data_path;
  raw_test_data_path.AppendFormat(_T("%s\\declaration.txt"), directory);

  // Get cert with private key and cert without private key.
  std::vector<byte> encoded_cert_with_private_key;
  std::vector<byte> encoded_cert_without_private_key;
  ASSERT_SUCCEEDED(ReadEntireFile(encoded_cert_with_private_key_path,
                                  0,
                                  &encoded_cert_with_private_key));
  ASSERT_SUCCEEDED(ReadEntireFile(encoded_cert_without_private_key_path,
                                  0,
                                  &encoded_cert_without_private_key));
  CString cert_password = _T("f00bar");
  CString cert_subject_name = _T("Unofficial Google Test");

  // Get testdata.
  std::vector<byte> raw_testdata;
  ASSERT_SUCCEEDED(ReadEntireFile(raw_test_data_path, 0, &raw_testdata));

  // Create a signing certificate.
  CryptoSigningCertificate signing_certificate;
  ASSERT_SUCCEEDED(signing_certificate.ImportCertificate(
      encoded_cert_with_private_key, cert_password, cert_subject_name));

  // Create a signature object and sign the test data.
  std::vector<byte> signature;
  CryptoComputeSignature signer(&signing_certificate);
  ASSERT_SUCCEEDED(signer.Sign(raw_testdata, &signature));

  // Create a validating certificate.
  CryptoSignatureVerificationCertificate verification_certificate;
  ASSERT_SUCCEEDED(verification_certificate.ImportCertificate(
      encoded_cert_without_private_key, cert_subject_name));

  // Create a signature object and verify the test data's signature.
  CryptoVerifySignature verifier(verification_certificate);
  ASSERT_SUCCEEDED(verifier.Validate(raw_testdata, signature));

  // Mess up the signature and show it doesn't verify.
  size_t mid = signature.size() / 2;
  byte mid_byte = signature[mid];
  signature[mid] = ~mid_byte;
  ASSERT_FAILED(verifier.Validate(raw_testdata, signature));

  // Restore the signature, mess up the test data, and show it doesn't verify.
  signature[mid] = mid_byte;
  mid = raw_testdata.size() / 2;
  mid_byte = raw_testdata[mid];
  raw_testdata[mid] = ~mid_byte;
  ASSERT_FAILED(verifier.Validate(raw_testdata, signature));
}

TEST(SignaturesTest, AuthenticateFiles) {
  const CString executable_path(app_util::GetCurrentModuleDirectory());

  const CString source_file1 = ConcatenatePath(
      executable_path,
      _T("unittest_support\\download_cache_test\\")
      _T("{89640431-FE64-4da8-9860-1A1085A60E13}\\gears-win32-opt.msi"));

  const CString hash_file1 = _T("ImV9skETZqGFMjs32vbZTvzAYJU=");

  const CString source_file2 = ConcatenatePath(
       executable_path,
       _T("unittest_support\\download_cache_test\\")
       _T("{7101D597-3481-4971-AD23-455542964072}\\livelysetup.exe"));

  const CString hash_file2 = _T("Igq6bYaeXFJCjH770knXyJ6V53s=");

  const CString hash_files = _T("e2uzy96jlusKbADl87zie6F5iwE=");

  const CString bad_hash = _T("sFzmoHgCbowEnioqVb8WanTYbhIabcde=");

  std::vector<CString> files;

  // Authenticate one file.
  files.push_back(source_file1);
  EXPECT_HRESULT_SUCCEEDED(AuthenticateFiles(files, hash_file1));

  // Incorrect hash.
  EXPECT_EQ(SIGS_E_INVALID_SIGNATURE, AuthenticateFiles(files, hash_file2));

  // Bad hash.
  EXPECT_EQ(E_INVALIDARG, AuthenticateFiles(files, bad_hash));
  EXPECT_EQ(E_INVALIDARG, AuthenticateFiles(files, _T("")));

  // Authenticate two files.
  files.push_back(source_file2);
  EXPECT_HRESULT_SUCCEEDED(AuthenticateFiles(files, hash_files));

  // Round trip through CryptoHash::Compute to verify the hash of two files.
  CryptoHash crypto;
  std::vector<byte> hash_out;
  EXPECT_HRESULT_SUCCEEDED(crypto.Compute(files, 0, &hash_out));

  CStringA actual_hash_files;
  EXPECT_HRESULT_SUCCEEDED(Base64::Encode(hash_out, &actual_hash_files));
  EXPECT_STREQ(hash_files, CString(actual_hash_files));
}

}  // namespace omaha

