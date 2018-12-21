// Copyright 2009 Google Inc.
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
// Unit tests for the Google file signature validation.

#include <windows.h>
#include <atlstr.h>
#include "omaha/base/app_util.h"
#include "omaha/base/const_code_signing.h"
#include "omaha/base/file.h"
#include "omaha/base/signaturevalidator.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace {

bool VerifySigneeIsGoogle(const wchar_t* signed_file) {
  std::vector<CString> subject;
  subject.push_back(kSha256CertificateSubjectName);
  subject.push_back(kCertificateSubjectName);
  return SUCCEEDED(
    VerifyCertificate(signed_file,
                      subject,
                      true,   // Allow test variant.
                      false,  // Check certificate is valid now.
                      NULL));
}

}  // namespace

// Checks Omaha Thawte certificate sha1 (11/28/2016 to 11/21/2019).
TEST(CertInfoTest, CertInfo) {
  const TCHAR kRelativePath[] =
      _T("unittest_support\\sha1_14F8FDD167F92402B1570B5DC495C815.sys");

  CString executable_full_path(app_util::GetCurrentModuleDirectory());
  ASSERT_TRUE(::PathAppend(CStrBuf(executable_full_path, MAX_PATH),
                           kRelativePath));
  ASSERT_TRUE(File::Exists(executable_full_path));
  EXPECT_TRUE(VerifySigneeIsGoogle(executable_full_path));

  CertList cert_list;
  ExtractAllCertificatesFromSignature(executable_full_path, &cert_list);

  // ExtractAllCertificatesFromSignature() gets the certificate chain for the
  // first signature and the certificate chain for the corresponding timestamp,
  // excluding the root certificates.
  // The following certificates are enumerated from SaveArguments.exe signed
  // Thursday, April 14, 2016 3:57:37 PM:
  // * "Google Inc" hash 1a6ac0549a4a44264deb6ff003391da2f285b19f.
  // * "Thawte Code Signing CA - G2" hash
  //   808d62642b7d1c4a9a83fd667f7a2a9d243fb1c7.
  // * "COMODO SHA-1 Time Stamping Signer" hash
  //   03a5b14663eb12023091b84a6d6a68bc871de66b.
  EXPECT_EQ(3, cert_list.size());

  const CertInfo* cert_info = NULL;
  std::vector<CString> subject;
  subject.push_back(kCertificateSubjectName);
  cert_list.FindFirstCert(&cert_info,
                          subject,
                          CString(),
                          CString(),
                          false,      // Do not allow test variant.
                          true);      // Check if the certificate is valid now.
  ASSERT_TRUE(cert_info);

  EXPECT_STREQ(kCertificateSubjectName, cert_info->issuing_company_name_);
  EXPECT_STREQ(kCertificateThumbprint, cert_info->thumbprint_);
  EXPECT_STREQ(kCertificatePublicKeyHash, cert_info->public_key_hash_);
}

// Checks Chrome certificate sha256 (11/06/2018 to 11/17/2021).
TEST(CertInfoTest, CertInfo_Sha256) {
  const TCHAR kRelativePath[] =
      _T("unittest_support\\chrome_setup.exe");

  CString executable_full_path(app_util::GetCurrentModuleDirectory());
  ASSERT_TRUE(::PathAppend(CStrBuf(executable_full_path, MAX_PATH),
                           kRelativePath));
  ASSERT_TRUE(File::Exists(executable_full_path));

  CertList cert_list;
  ExtractAllCertificatesFromSignature(executable_full_path, &cert_list);

  // For some reason, extracting all certificates from the file only yields
  // the code signing certificates but skips the time stamping certificates.
  // In the case of chrome_setup.exe the following certs are picked up:
  // * Dummy certificate.
  // * DigiCert SHA2 Assured ID Code Signing CA
  //   hash 92c1588e85af2201ce7915e8538b492f605b80c6
  // * Google LLC hash cb7e84887f3c6015fe7edfb4f8f36df7dc10590e
  EXPECT_EQ(3, cert_list.size());

  const CertInfo* cert_info = NULL;
  std::vector<CString> subject;
  subject.push_back(kSha256CertificateSubjectName);
  cert_list.FindFirstCert(&cert_info,
                          subject,
                          CString(),
                          CString(),
                          false,      // Do not allow test variant.
                          true);      // Check if the certificate is valid now.
  ASSERT_TRUE(cert_info);

  EXPECT_STREQ(kSha256CertificateSubjectName, cert_info->issuing_company_name_);
  EXPECT_STREQ(kSha256CertificateThumbprint, cert_info->thumbprint_);
  EXPECT_STREQ(kSha256CertificatePublicKeyHash, cert_info->public_key_hash_);
}

TEST(SignatureValidatorTest, VerifySigneeIsGoogle_OfficiallySigned) {
  const TCHAR kRelativePath[] = _T("unittest_support\\SaveArguments.exe");

  CString executable_full_path(app_util::GetCurrentModuleDirectory());
  ASSERT_TRUE(::PathAppend(CStrBuf(executable_full_path, MAX_PATH),
                           kRelativePath));
  ASSERT_TRUE(File::Exists(executable_full_path));
  EXPECT_TRUE(VerifySigneeIsGoogle(executable_full_path));
}

// Tests a certificate subject containing multiple CNs such as:
//    "CN = Google Inc (TEST), CN = Some Other CN, ...
// The code exactly matches on the first CN only.
TEST(SignatureValidatorTest, VerifySigneeIsGoogle_TestSigned_MultipleCN) {
  const TCHAR kRelativePath[] =
      _T("unittest_support\\SaveArguments_multiple_cn.exe");

  CString executable_full_path(app_util::GetCurrentModuleDirectory());
  ASSERT_TRUE(::PathAppend(CStrBuf(executable_full_path, MAX_PATH),
                           kRelativePath));
  ASSERT_TRUE(File::Exists(executable_full_path));
  EXPECT_TRUE(VerifySigneeIsGoogle(executable_full_path));
}

TEST(SignatureValidatorTest,
       VerifySigneeIsGoogle_OfficiallySigned_DifferentOU) {
  const TCHAR kRelativePath[] =
      _T("unittest_support\\SaveArguments_different_ou.exe");

  CString executable_full_path(app_util::GetCurrentModuleDirectory());
  ASSERT_TRUE(::PathAppend(CStrBuf(executable_full_path, MAX_PATH),
                           kRelativePath));
  ASSERT_TRUE(File::Exists(executable_full_path));
  EXPECT_TRUE(VerifySigneeIsGoogle(executable_full_path));
}

TEST(SignatureValidatorTest, VerifySigneeIsGoogle_OmahaTestSigned) {
  const TCHAR kRelativePath[] =
      _T("unittest_support\\SaveArguments_OmahaTestSigned.exe");

  CString executable_full_path(app_util::GetCurrentModuleDirectory());
  ASSERT_TRUE(::PathAppend(CStrBuf(executable_full_path, MAX_PATH),
                           kRelativePath));
  ASSERT_TRUE(File::Exists(executable_full_path));
  EXPECT_TRUE(VerifySigneeIsGoogle(executable_full_path));
}

TEST(SignatureValidatorTest, VerifySigneeIsGoogle_Sha256) {
  const TCHAR kRelativePath[] =
      _T("unittest_support\\sha2_2a9c21acaaa63a3c58a7b9322bee948d.exe");

  CString executable_full_path(app_util::GetCurrentModuleDirectory());
  ASSERT_TRUE(::PathAppend(CStrBuf(executable_full_path, MAX_PATH),
                           kRelativePath));
  ASSERT_TRUE(File::Exists(executable_full_path));
  EXPECT_TRUE(VerifySigneeIsGoogle(executable_full_path));

  executable_full_path = app_util::GetCurrentModuleDirectory();
  ASSERT_TRUE(::PathAppend(CStrBuf(executable_full_path, MAX_PATH),
                           _T("unittest_support\\chrome_setup.exe")));
  ASSERT_TRUE(File::Exists(executable_full_path));
  EXPECT_TRUE(VerifySigneeIsGoogle(executable_full_path));
}

TEST(SignatureValidatorTest, VerifySigneeIsGoogle_DualSigned_Sha1AndSha256) {
  const TCHAR kRelativePath[] = _T("unittest_support\\Sha1_4c40dba5f988fae57a57d6457495f98b_and_sha2_2a9c21acaaa63a3c58a7b9322bee948d.exe");  // NOLINT

  CString executable_full_path(app_util::GetCurrentModuleDirectory());
  ASSERT_TRUE(::PathAppend(CStrBuf(executable_full_path, MAX_PATH),
                           kRelativePath));
  ASSERT_TRUE(File::Exists(executable_full_path));
  EXPECT_TRUE(VerifySigneeIsGoogle(executable_full_path));
}

// The certificate was valid when it was used to sign the executable, but it has
// since expired.
TEST(SignatureValidatorTest, VerifySigneeIsGoogle_SignedWithNowExpiredCert) {
  const TCHAR kRelativePath[] =
      _T("unittest_support\\GoogleUpdate_now_expired_cert.exe");

  CString executable_full_path(app_util::GetCurrentModuleDirectory());
  ASSERT_TRUE(::PathAppend(CStrBuf(executable_full_path, MAX_PATH),
                           kRelativePath));
  ASSERT_TRUE(File::Exists(executable_full_path));
  EXPECT_TRUE(VerifySigneeIsGoogle(executable_full_path));
}

TEST(SignatureValidatorTest, VerifySigneeIsGoogle_TestSigned_NoCN) {
  const TCHAR kRelativePath[] =
      _T("unittest_support\\SaveArguments_no_cn.exe");

  CString executable_full_path(app_util::GetCurrentModuleDirectory());
  ASSERT_TRUE(::PathAppend(CStrBuf(executable_full_path, MAX_PATH),
                           kRelativePath));
  ASSERT_TRUE(File::Exists(executable_full_path));
  EXPECT_FALSE(VerifySigneeIsGoogle(executable_full_path));
}

TEST(SignatureValidatorTest, VerifySigneeIsGoogle_TestSigned_WrongCN) {
  const TCHAR kRelativePath[] =
      _T("unittest_support\\SaveArguments_wrong_cn.exe");

  CString executable_full_path(app_util::GetCurrentModuleDirectory());
  ASSERT_TRUE(::PathAppend(CStrBuf(executable_full_path, MAX_PATH),
                           kRelativePath));
  ASSERT_TRUE(File::Exists(executable_full_path));
  EXPECT_FALSE(VerifySigneeIsGoogle(executable_full_path));
}

TEST(SignatureValidatorTest, VerifyAuthenticodeSignature) {
  const TCHAR* kFileNamesToVerify[] = {
    _T("GoogleUpdate_now_expired_cert.exe"),
    _T("GoogleUpdate_old_signature.exe"),
    _T("SaveArguments.exe"),
    _T("chrome_setup.exe"),
    _T("sha2_0c15be4a15bb0903c901b1d6c265302f.msi"),
    _T("SaveArguments_OmahaTestSigned.exe"),
    _T("Sha1_4c40dba5f988fae57a57d6457495f98b_and_sha2_2a9c21acaaa63a3c58a7b9322bee948d.exe"),  // NOLINT
    _T("SaveArguments_unsigned_wrong_markup_value.exe"),
    _T("SaveArguments_wrong_cn.exe"),
  };
  const HRESULT kExpectedResult[] = {
    S_OK,
    S_OK,
    S_OK,
    S_OK,
    S_OK,
    CERT_E_UNTRUSTEDROOT,
    S_OK,
    TRUST_E_NOSIGNATURE,
    CERT_E_UNTRUSTEDROOT,
  };

  EXPECT_EQ(arraysize(kFileNamesToVerify), arraysize(kExpectedResult));

  CString files_full_path(app_util::GetCurrentModuleDirectory());
  ASSERT_TRUE(::PathAppend(CStrBuf(files_full_path, MAX_PATH),
                           _T("unittest_support")));

  for (size_t i = 0; i < arraysize(kFileNamesToVerify); ++i) {
    CString file_to_verify(files_full_path);
    ASSERT_TRUE(::PathAppend(CStrBuf(file_to_verify, MAX_PATH),
                             kFileNamesToVerify[i]));
    EXPECT_EQ(kExpectedResult[i],
              VerifyAuthenticodeSignature(file_to_verify, true))
        << _T("Signature of file ") << kFileNamesToVerify[i]
        << _T(" doesn't match expectation.");
  }
}

}  // namespace omaha
