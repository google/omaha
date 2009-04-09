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

#include "omaha/common/browser_utils.h"
#include "omaha/common/const_utils.h"
#include "omaha/common/path.h"
#include "omaha/common/reg_key.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

const CString kRegistryHiveOverrideClasses =
    CString(kRegistryHiveOverrideRoot) + _T("HKCR");

class GetLegacyDefaultBrowserInfoTest : public testing::Test {
 protected:
  virtual void SetUp() {
    RegKey::DeleteKey(kRegistryHiveOverrideClasses, true);
    RegKey classes_key;
    ASSERT_HRESULT_SUCCEEDED(classes_key.Create(kRegistryHiveOverrideClasses));
    ASSERT_HRESULT_SUCCEEDED(::RegOverridePredefKey(HKEY_CLASSES_ROOT,
                                                    classes_key.Key()));
  }

  virtual void TearDown() {
    ASSERT_HRESULT_SUCCEEDED(::RegOverridePredefKey(HKEY_CLASSES_ROOT, NULL));
    ASSERT_HRESULT_SUCCEEDED(RegKey::DeleteKey(kRegistryHiveOverrideClasses,
                             true));
  }
};

TEST(BrowserUtilsTest, GetBrowserImagePath) {
  CString browser;
  ASSERT_SUCCEEDED(GetDefaultBrowserName(&browser));

  BrowserType default_type = BROWSER_UNKNOWN;
  if (browser.CompareNoCase(kIeExeName) == 0) {
    default_type = BROWSER_IE;
  } else if (browser.CompareNoCase(kFirefoxExeName) == 0) {
    default_type = BROWSER_FIREFOX;
  }

  CString exp_browser_path;
  ASSERT_SUCCEEDED(GetDefaultBrowserPath(&exp_browser_path));

  if (default_type == BROWSER_IE) {
    CString path;
    ASSERT_SUCCEEDED(GetBrowserImagePath(BROWSER_IE, &path));

    CString long_name;
    ASSERT_SUCCEEDED(ShortPathToLongPath(path, &long_name));
    CString exp_long_name;
    ASSERT_SUCCEEDED(ShortPathToLongPath(exp_browser_path, &exp_long_name));
    ASSERT_STREQ(exp_long_name.MakeLower(), long_name.MakeLower());
  }

  if (default_type == BROWSER_FIREFOX) {
    CString path;
    ASSERT_SUCCEEDED(GetBrowserImagePath(BROWSER_FIREFOX, &path));

    CString long_name;
    ASSERT_SUCCEEDED(ShortPathToLongPath(path, &long_name));
    CString exp_long_name;
    ASSERT_SUCCEEDED(ShortPathToLongPath(exp_browser_path, &exp_long_name));
    ASSERT_STREQ(exp_long_name.MakeLower(), long_name.MakeLower());
  }

  CString path;
  ASSERT_SUCCEEDED(GetBrowserImagePath(BROWSER_DEFAULT, &path));
  CString long_name;
  ASSERT_SUCCEEDED(ShortPathToLongPath(path, &long_name));
  CString exp_long_name;
  ASSERT_SUCCEEDED(ShortPathToLongPath(exp_browser_path, &exp_long_name));
  ASSERT_STREQ(exp_long_name.MakeLower(), long_name.MakeLower());

  ASSERT_FAILED(GetBrowserImagePath(BROWSER_UNKNOWN, &path));
}

TEST(BrowserUtilsTest, GetLegacyDefaultBrowserInfo) {
  CString name;
  CString browser_path;
  EXPECT_SUCCEEDED(GetLegacyDefaultBrowserInfo(&name, &browser_path));
  EXPECT_FALSE(browser_path.IsEmpty());
  EXPECT_FALSE(name.IsEmpty());
}

TEST(BrowserUtilsTest, GetIeFontSize) {
  // The build server might not have IE configured.
  if (!IsBuildSystem()) {
    uint32 font_size = 0;
    const uint32 kMaxFontSize = 4;
    EXPECT_SUCCEEDED(GetIeFontSize(&font_size));
    EXPECT_LE(font_size, kMaxFontSize);
  }
}

TEST_F(GetLegacyDefaultBrowserInfoTest, ValidQuotedIE) {
  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(
      kRegKeyLegacyDefaultBrowserCommand,
      NULL,
      _T("\"C:\\Program Files\\Internet Explorer\\iexplore.exe\" -nohome")));

  CString name;
  CString browser_path;
  EXPECT_SUCCEEDED(GetLegacyDefaultBrowserInfo(&name, &browser_path));
  EXPECT_STREQ(_T("C:\\Program Files\\Internet Explorer\\iexplore.exe"),
               browser_path);
  EXPECT_STREQ(_T("iexplore.exe"), name);
}

TEST_F(GetLegacyDefaultBrowserInfoTest, ValidUnquotedPath) {
  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(
      kRegKeyLegacyDefaultBrowserCommand,
      NULL,
      _T("C:\\Program Files\\Internet Explorer\\iexplore.exe")));

  CString name;
  CString browser_path;
  EXPECT_SUCCEEDED(GetLegacyDefaultBrowserInfo(&name, &browser_path));
  EXPECT_STREQ(_T("C:\\Program Files\\Internet Explorer\\iexplore.exe"),
               browser_path);
  EXPECT_STREQ(_T("iexplore.exe"), name);
}

TEST_F(GetLegacyDefaultBrowserInfoTest, InvalidNoKey) {
  CString name;
  CString browser_path;
  EXPECT_FAILED(GetLegacyDefaultBrowserInfo(&name, &browser_path));
  EXPECT_TRUE(browser_path.IsEmpty());
  EXPECT_TRUE(name.IsEmpty());
}

TEST_F(GetLegacyDefaultBrowserInfoTest, InvalidNoValue) {
  EXPECT_HRESULT_SUCCEEDED(RegKey::CreateKey(
      kRegKeyLegacyDefaultBrowserCommand));

  CString name;
  CString browser_path;
  EXPECT_FAILED(GetLegacyDefaultBrowserInfo(&name, &browser_path));
  EXPECT_TRUE(browser_path.IsEmpty());
  EXPECT_TRUE(name.IsEmpty());
}

TEST_F(GetLegacyDefaultBrowserInfoTest, InvalidPath) {
  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(
      kRegKeyLegacyDefaultBrowserCommand,
      NULL,
      _T("\"C:\\Program File\\iexplore.exe\" -nohome")));

  CString name;
  CString browser_path;
  EXPECT_FAILED(GetLegacyDefaultBrowserInfo(&name, &browser_path));
  EXPECT_TRUE(browser_path.IsEmpty());
  EXPECT_TRUE(name.IsEmpty());
}

TEST_F(GetLegacyDefaultBrowserInfoTest, InvalidUnquotedPath) {
  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(
      kRegKeyLegacyDefaultBrowserCommand,
      NULL,
      _T("C:\\Program Files\\Internet Explorer\\iexplore.exe -nohome")));

  CString name;
  CString browser_path;
  EXPECT_FAILED(GetLegacyDefaultBrowserInfo(&name, &browser_path));
  EXPECT_TRUE(browser_path.IsEmpty());
  EXPECT_TRUE(name.IsEmpty());
}

TEST_F(GetLegacyDefaultBrowserInfoTest, InvalidUnquotedDirectory) {
  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(
      kRegKeyLegacyDefaultBrowserCommand,
      NULL,
      _T("C:\\Program Files\\Internet Explorer\\")));

  CString name;
  CString browser_path;
  EXPECT_FAILED(GetLegacyDefaultBrowserInfo(&name, &browser_path));
  EXPECT_TRUE(browser_path.IsEmpty());
  EXPECT_TRUE(name.IsEmpty());
}

TEST_F(GetLegacyDefaultBrowserInfoTest, InvalidQuotedDirectory) {
  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(
      kRegKeyLegacyDefaultBrowserCommand,
      NULL,
      _T("\"C:\\Program Files\\Internet Explorer\\\" -nohome")));

  CString name;
  CString browser_path;
  EXPECT_FAILED(GetLegacyDefaultBrowserInfo(&name, &browser_path));
  EXPECT_TRUE(browser_path.IsEmpty());
  EXPECT_TRUE(name.IsEmpty());
}

}  // namespace omaha

