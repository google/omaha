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
#include "omaha/common/app_util.h"
#include "omaha/common/file.h"
#include "omaha/common/signaturevalidator.h"
#include "omaha/third_party/gtest/include/gtest/gtest.h"

namespace omaha {

class SignatureValidatorTest : public testing::Test {
  virtual void SetUp() {
  }

  virtual void TearDown() {
  }
};


TEST_F(SignatureValidatorTest, VerifySigneeIsGoogle_OfficiallySigned) {
  const TCHAR kUnsignedExecutable[] =
      _T("unittest_support\\SaveArguments.exe");

  CString executable_full_path(app_util::GetCurrentModuleDirectory());
  ASSERT_TRUE(::PathAppend(CStrBuf(executable_full_path, MAX_PATH),
                           kUnsignedExecutable));
  ASSERT_TRUE(File::Exists(executable_full_path));
  EXPECT_TRUE(VerifySigneeIsGoogleNoTimestampCheck(executable_full_path));
}

TEST_F(SignatureValidatorTest, VerifySigneeIsGoogle_OmahaTestSigned) {
  const TCHAR kUnsignedExecutable[] =
      _T("unittest_support\\SaveArguments_OmahaTestSigned.exe");

  CString executable_full_path(app_util::GetCurrentModuleDirectory());
  ASSERT_TRUE(::PathAppend(CStrBuf(executable_full_path, MAX_PATH),
                           kUnsignedExecutable));
  ASSERT_TRUE(File::Exists(executable_full_path));
  EXPECT_TRUE(VerifySigneeIsGoogleNoTimestampCheck(executable_full_path));
}

}  // namespace omaha
