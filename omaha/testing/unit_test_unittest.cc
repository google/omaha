// Copyright 2009-2010 Google Inc.
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

#include "omaha/base/utils.h"
#include "omaha/base/user_info.h"
#include "omaha/base/vistautil.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/const_group_policy.h"
#include "testing/unit_test.h"

namespace omaha {

TEST(UnitTestHelpersTest, GetLocalAppDataPath) {
  if (IsTestRunByLocalSystem()) {
    return;
  }

  const TCHAR kUserXpLocalAppDataPathFormat[] =
      _T("C:\\Documents and Settings\\%s\\Local Settings\\Application Data\\");
  const TCHAR kUserVistaLocalAppDataPathFormat[] =
      _T("C:\\Users\\%s\\AppData\\Local\\");

  TCHAR username[MAX_PATH] = {0};
  EXPECT_TRUE(::GetEnvironmentVariable(_T("USERNAME"),
                                       username,
                                       arraysize(username)));
  CString expected_path;
  expected_path.Format(vista_util::IsVistaOrLater() ?
                           kUserVistaLocalAppDataPathFormat :
                           kUserXpLocalAppDataPathFormat,
                       username);
  EXPECT_STREQ(expected_path, GetLocalAppDataPath());
}

// GUIDs cannot be compared in GTest because there is no << operator. Therefore,
// we must treat them as strings. All these tests rely on GuidToString working.
#define EXPECT_GUID_EQ(expected, actual) \
    EXPECT_STREQ(GuidToString(expected), GuidToString(actual))

TEST(UnitTestHelpersTest, StringToGuid_InvalidString) {
  ExpectAsserts expect_asserts;  // Invalid strings cause an assert.

  EXPECT_GUID_EQ(GUID_NULL, StringToGuid(_T("")));
  EXPECT_GUID_EQ(GUID_NULL, StringToGuid(_T("{}")));
  EXPECT_GUID_EQ(GUID_NULL, StringToGuid(_T("a")));
  EXPECT_GUID_EQ(GUID_NULL,
                 StringToGuid(_T("CA3045BFA6B14fb8A0EFA615CEFE452C")));

  // Missing {}
  EXPECT_GUID_EQ(GUID_NULL,
                 StringToGuid(_T("CA3045BF-A6B1-4fb8-A0EF-A615CEFE452C")));

  // Invalid char X
  EXPECT_GUID_EQ(GUID_NULL,
                 StringToGuid(_T("{XA3045BF-A6B1-4fb8-A0EF-A615CEFE452C}")));

  // Invalid binary char 0x200
  EXPECT_GUID_EQ(
      GUID_NULL,
      StringToGuid(_T("{\0x200a3045bf-a6b1-4fb8-a0ef-a615cefe452c}")));

  // Missing -
  EXPECT_GUID_EQ(GUID_NULL,
                 StringToGuid(_T("{CA3045BFA6B14fb8A0EFA615CEFE452C}")));

  // Double quotes
  EXPECT_GUID_EQ(
      GUID_NULL,
      StringToGuid(_T("\"{ca3045bf-a6b1-4fb8-a0ef-a615cefe452c}\"")));
}

TEST(UnitTestHelpersTest, StringToGuid_ValidString) {
  const GUID kExpectedGuid = {0xCA3045BF, 0xA6B1, 0x4FB8,
                              {0xA0, 0xEF, 0xA6, 0x15, 0xCE, 0xFE, 0x45, 0x2C}};

  // Converted successfully, but indistinguishable from failures.
  EXPECT_GUID_EQ(GUID_NULL,
                 StringToGuid(_T("{00000000-0000-0000-0000-000000000000}")));

  EXPECT_GUID_EQ(kExpectedGuid,
                 StringToGuid(_T("{CA3045BF-A6B1-4fb8-A0EF-A615CEFE452C}")));

  EXPECT_GUID_EQ(kExpectedGuid,
                 StringToGuid(_T("{ca3045bf-a6b1-4fb8-a0ef-a615cefe452c}")));
}

TEST(UnitTestHelpersTest, ClearGroupPolicies) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE_DEV,
                                    kRegValueIsEnrolledToDomain,
                                    1UL));
  EXPECT_SUCCEEDED(SetPolicyString(kRegValueDownloadPreference,
                                   kDownloadPreferenceCacheable));
  ConfigManager* cm = ConfigManager::Instance();
  cm->LoadPolicies(true);
  EXPECT_STREQ(cm->GetDownloadPreferenceGroupPolicy(nullptr), _T("cacheable"));
  ClearGroupPolicies();
  EXPECT_STREQ(cm->GetDownloadPreferenceGroupPolicy(nullptr), _T(""));
  RegKey::DeleteKey(kRegKeyGoopdateGroupPolicy);
  EXPECT_SUCCEEDED(RegKey::DeleteValue(MACHINE_REG_UPDATE_DEV,
                                       kRegValueIsEnrolledToDomain));
}

}  // namespace omaha
