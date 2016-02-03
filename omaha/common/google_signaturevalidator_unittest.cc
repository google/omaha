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

TEST(GoogleSignatureValidatorTest, GoogleSignatureValidator) {
  // Check current Omaha certificate.
  CString filename(app_util::GetCurrentModuleDirectory());
  EXPECT_TRUE(::PathAppend(CStrBuf(filename, MAX_PATH),
                           _T("unittest_support\\SaveArguments.exe")));
  EXPECT_TRUE(File::Exists(filename));
  EXPECT_HRESULT_SUCCEEDED(VerifyGoogleAuthenticodeSignature(filename, true));

  // Check current Chrome certificate.
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

  // Check new Omaha/Chrome certificates.
  filename = app_util::GetCurrentModuleDirectory();
  EXPECT_TRUE(::PathAppend(CStrBuf(filename, MAX_PATH),
                           _T("unittest_support\\Sha1_")
                           _T("4c40dba5f988fae57a57d6457495f98b.exe")));
  EXPECT_TRUE(File::Exists(filename));
  EXPECT_HRESULT_SUCCEEDED(VerifyGoogleAuthenticodeSignature(filename, true));

  filename = app_util::GetCurrentModuleDirectory();
  EXPECT_TRUE(::PathAppend(CStrBuf(filename, MAX_PATH),
                           _T("unittest_support\\sha2_")
                           _T("2a9c21acaaa63a3c58a7b9322bee948d.exe")));
  EXPECT_TRUE(File::Exists(filename));
  EXPECT_HRESULT_SUCCEEDED(VerifyGoogleAuthenticodeSignature(filename, true));

  filename = app_util::GetCurrentModuleDirectory();
  EXPECT_TRUE(::PathAppend(CStrBuf(filename, MAX_PATH),
                           _T("unittest_support\\Sha1_")
                           _T("4c40dba5f988fae57a57d6457495f98b")
                           _T("_and_sha2_")
                           _T("2a9c21acaaa63a3c58a7b9322bee948d.exe")));
  EXPECT_TRUE(File::Exists(filename));
  EXPECT_HRESULT_SUCCEEDED(VerifyGoogleAuthenticodeSignature(filename, true));
}

}  // namespace omaha

