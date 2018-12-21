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

#include <windows.h>
#include <atlstr.h>
#include "omaha/base/app_util.h"
#include "omaha/base/file.h"
#include "omaha/common/google_signaturevalidator.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

// This test uses certificate serial numbers to encode the names of the test
// files used below. This is slightly confusing since the signing tools and
// build scrips use the certificate thumbprint (the sha1 hash of
// the certificate) to choose what certificate to sign with.
TEST(GoogleSignatureValidatorTest, GoogleSignatureValidator) {
  // Check current Omaha certificate.
  CString filename(app_util::GetCurrentModuleDirectory());
  EXPECT_TRUE(::PathAppend(CStrBuf(filename, MAX_PATH),
                           _T("unittest_support\\SaveArguments.exe")));
  EXPECT_TRUE(File::Exists(filename));
  EXPECT_HRESULT_SUCCEEDED(VerifyGoogleAuthenticodeSignature(filename, true));

  // Check current Chrome setup file.
  filename = app_util::GetCurrentModuleDirectory();
  EXPECT_TRUE(::PathAppend(CStrBuf(filename, MAX_PATH),
                           _T("unittest_support\\chrome_setup.exe")));
  EXPECT_TRUE(File::Exists(filename));
  EXPECT_HRESULT_SUCCEEDED(VerifyGoogleAuthenticodeSignature(filename, true));

  // Check 2912C70C9A2B8A3EF6F6074662D68B8D Chrome certificate.
  filename = app_util::GetCurrentModuleDirectory();
  EXPECT_TRUE(::PathAppend(CStrBuf(filename, MAX_PATH),
                           _T("unittest_support\\chrome_certificate_")
                           _T("2912C70C9A2B8A3EF6F6074662D68B8D.dll")));
  EXPECT_TRUE(File::Exists(filename));
  EXPECT_HRESULT_SUCCEEDED(VerifyGoogleAuthenticodeSignature(filename, true));

  // Check the past and revoked Chrome certificate is still accepted.
  filename = app_util::GetCurrentModuleDirectory();
  EXPECT_TRUE(::PathAppend(CStrBuf(filename, MAX_PATH),
                           _T("unittest_support\\chrome_certificate_")
                           _T("09E28B26DB593EC4E73286B66499C370.dll")));
  EXPECT_TRUE(File::Exists(filename));
  EXPECT_HRESULT_SUCCEEDED(VerifyGoogleAuthenticodeSignature(filename, true));

  // Check an old Google certificate which is not in the pin list.
  filename = app_util::GetCurrentModuleDirectory();
  EXPECT_TRUE(::PathAppend(CStrBuf(filename, MAX_PATH),
                           _T("unittest_support\\old_google_certificate.dll")));
  EXPECT_TRUE(File::Exists(filename));
  EXPECT_HRESULT_FAILED(VerifyGoogleAuthenticodeSignature(filename, true));

  // Check Omaha/Chrome Thawte certificate sha1 (11/28/2016 to 11/21/2019).
  filename = app_util::GetCurrentModuleDirectory();
  EXPECT_TRUE(::PathAppend(CStrBuf(filename, MAX_PATH),
                           _T("unittest_support\\sha1_")
                           _T("14F8FDD167F92402B1570B5DC495C815.sys")));
  EXPECT_TRUE(File::Exists(filename));
  EXPECT_HRESULT_SUCCEEDED(VerifyGoogleAuthenticodeSignature(filename, true));

  // Check Omaha/Chrome certificate sha256 (12/15/2015 to 12/16/2018).
  filename = app_util::GetCurrentModuleDirectory();
  EXPECT_TRUE(::PathAppend(CStrBuf(filename, MAX_PATH),
                           _T("unittest_support\\sha2_")
                           _T("2a9c21acaaa63a3c58a7b9322bee948d.exe")));
  EXPECT_TRUE(File::Exists(filename));
  EXPECT_HRESULT_SUCCEEDED(VerifyGoogleAuthenticodeSignature(filename, true));

  // Check Omaha/Chrome dual signed certificates sha1 and sha2
  // (11/28/2016 to 11/21/2019) and (12/15/2015 to 12/16/2018), respectively.
  filename = app_util::GetCurrentModuleDirectory();
  EXPECT_TRUE(::PathAppend(CStrBuf(filename, MAX_PATH),
                           _T("unittest_support\\Sha1_")
                           _T("4c40dba5f988fae57a57d6457495f98b")
                           _T("_and_sha2_")
                           _T("2a9c21acaaa63a3c58a7b9322bee948d.exe")));
  EXPECT_TRUE(File::Exists(filename));
  EXPECT_HRESULT_SUCCEEDED(VerifyGoogleAuthenticodeSignature(filename, true));

  // Check Omaha/Chrome certificate sha256 (11/06/2018 to 11/17/2021).
  filename = app_util::GetCurrentModuleDirectory();
  EXPECT_TRUE(::PathAppend(CStrBuf(filename, MAX_PATH),
                           _T("unittest_support\\sha2_")
                           _T("0c15be4a15bb0903c901b1d6c265302f.msi")));
  EXPECT_TRUE(File::Exists(filename));
  EXPECT_HRESULT_SUCCEEDED(VerifyGoogleAuthenticodeSignature(filename, true));
}

}  // namespace omaha

