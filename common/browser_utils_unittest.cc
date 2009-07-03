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

// GetDefaultBrowserName() uses ::RegOpenCurrentUser, which does not appear to
// be affected by registry hive overrides. Therefore, in order to test methods
// that rely on it, the actual value must be replaced. This class saves the
// value and restores it. If the test is interrupted before TearDown, the
// default browser may not be correctly registered.
class BrowserUtilsDefaultBrowserSavedTest : public testing::Test {
 protected:
  virtual void SetUp() {
    if (!RegKey::HasKey(kRegKeyUserDefaultBrowser)) {
      return;
    }

    EXPECT_SUCCEEDED(RegKey::GetValue(kRegKeyUserDefaultBrowser,
                                      NULL,
                                      &default_browser_name_));
  }

  virtual void TearDown() {
    if (default_browser_name_.IsEmpty()) {
      RegKey::DeleteKey(kRegKeyUserDefaultBrowser);
      return;
    }

    EXPECT_SUCCEEDED(RegKey::SetValue(kRegKeyUserDefaultBrowser,
                                      NULL,
                                      default_browser_name_));
  }

  CString default_browser_name_;
};

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

TEST_F(BrowserUtilsDefaultBrowserSavedTest, GetDefaultBrowserType_IE) {
  if (!ShouldRunLargeTest()) {
    return;
  }
  EXPECT_SUCCEEDED(
      RegKey::SetValue(kRegKeyUserDefaultBrowser, NULL, _T("IeXpLoRe.ExE")));
  BrowserType type = BROWSER_UNKNOWN;
  EXPECT_SUCCEEDED(GetDefaultBrowserType(&type));
  EXPECT_EQ(BROWSER_IE, type);
}

TEST_F(BrowserUtilsDefaultBrowserSavedTest, GetDefaultBrowserType_Firefox) {
  if (!ShouldRunLargeTest()) {
    return;
  }
  EXPECT_SUCCEEDED(
      RegKey::SetValue(kRegKeyUserDefaultBrowser, NULL, _T("FiReFoX.ExE")));
  BrowserType type = BROWSER_UNKNOWN;
  EXPECT_SUCCEEDED(GetDefaultBrowserType(&type));
  EXPECT_EQ(BROWSER_FIREFOX, type);
}

TEST_F(BrowserUtilsDefaultBrowserSavedTest, GetDefaultBrowserType_Chrome) {
  if (!ShouldRunLargeTest()) {
    return;
  }
  EXPECT_SUCCEEDED(
      RegKey::SetValue(kRegKeyUserDefaultBrowser, NULL, _T("ChRoMe.ExE")));
  BrowserType type = BROWSER_UNKNOWN;
  EXPECT_SUCCEEDED(GetDefaultBrowserType(&type));
  EXPECT_EQ(BROWSER_CHROME, type);
}

TEST_F(BrowserUtilsDefaultBrowserSavedTest, GetDefaultBrowserType_Unsupported) {
  if (!ShouldRunLargeTest()) {
    return;
  }
  EXPECT_SUCCEEDED(
      RegKey::SetValue(kRegKeyUserDefaultBrowser, NULL, _T("FoO.ExE")));
  BrowserType type = BROWSER_UNKNOWN;
  EXPECT_SUCCEEDED(GetDefaultBrowserType(&type));
  EXPECT_EQ(BROWSER_UNKNOWN, type);
}

TEST(BrowserUtilsTest, BrowserTypeToProcessName_Unknown) {
  CString exe_name;
  EXPECT_EQ(E_FAIL, BrowserTypeToProcessName(BROWSER_UNKNOWN, &exe_name));
  EXPECT_TRUE(exe_name.IsEmpty());
}

// Writes the default browser to ensure consistent results.
TEST_F(BrowserUtilsDefaultBrowserSavedTest, BrowserTypeToProcessName_Default) {
  if (!ShouldRunLargeTest()) {
    return;
  }
  EXPECT_SUCCEEDED(
      RegKey::SetValue(kRegKeyUserDefaultBrowser, NULL, _T("IeXpLoRe.ExE")));

  CString default_exe_name;
  EXPECT_SUCCEEDED(GetDefaultBrowserName(&default_exe_name));
  EXPECT_STREQ(_T("IeXpLoRe.ExE"), default_exe_name);

  CString exe_name;
  EXPECT_SUCCEEDED(BrowserTypeToProcessName(BROWSER_DEFAULT, &exe_name));
  EXPECT_STREQ(_T("IeXpLoRe.ExE"), exe_name);
}

TEST(BrowserUtilsTest, BrowserTypeToProcessName_Browsers) {
  CString exe_name;
  EXPECT_SUCCEEDED(BrowserTypeToProcessName(BROWSER_IE, &exe_name));
  EXPECT_STREQ(_T("IEXPLORE.EXE"), exe_name);
  EXPECT_SUCCEEDED(BrowserTypeToProcessName(BROWSER_FIREFOX, &exe_name));
  EXPECT_STREQ(_T("FIREFOX.EXE"), exe_name);
  EXPECT_SUCCEEDED(BrowserTypeToProcessName(BROWSER_CHROME, &exe_name));
  EXPECT_STREQ(_T("CHROME.EXE"), exe_name);
}

TEST(BrowserUtilsTest, BrowserTypeToProcessName_Invalid) {
  CString exe_name;
  ExpectAsserts expect_asserts;
  EXPECT_EQ(E_FAIL, BrowserTypeToProcessName(BROWSER_MAX, &exe_name));
  EXPECT_TRUE(exe_name.IsEmpty());
  EXPECT_EQ(E_FAIL,
            BrowserTypeToProcessName(static_cast<BrowserType>(9), &exe_name));
  EXPECT_TRUE(exe_name.IsEmpty());
  EXPECT_EQ(E_FAIL,
            BrowserTypeToProcessName(static_cast<BrowserType>(-1), &exe_name));
  EXPECT_TRUE(exe_name.IsEmpty());
}

TEST(BrowserUtilsTest, GetBrowserImagePath_DefaultBrowser) {
  CString browser;
  ASSERT_SUCCEEDED(GetDefaultBrowserName(&browser));

  BrowserType default_type = BROWSER_UNKNOWN;
  if (browser.CompareNoCase(kIeExeName) == 0) {
    default_type = BROWSER_IE;
  } else if (browser.CompareNoCase(kFirefoxExeName) == 0) {
    default_type = BROWSER_FIREFOX;
  } else if (browser.CompareNoCase(kChromeExeName) == 0) {
    default_type = BROWSER_CHROME;
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
  } else if (default_type == BROWSER_FIREFOX) {
    CString path;
    ASSERT_SUCCEEDED(GetBrowserImagePath(BROWSER_FIREFOX, &path));

    CString long_name;
    ASSERT_SUCCEEDED(ShortPathToLongPath(path, &long_name));
    CString exp_long_name;
    ASSERT_SUCCEEDED(ShortPathToLongPath(exp_browser_path, &exp_long_name));
    ASSERT_STREQ(exp_long_name.MakeLower(), long_name.MakeLower());
  } else if (default_type == BROWSER_CHROME) {
    CString path;
    ASSERT_SUCCEEDED(GetBrowserImagePath(BROWSER_CHROME, &path));

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

// Try all browsers to get more test coverage.
TEST(BrowserUtilsTest, GetBrowserImagePath_AllSupportedBrowsers) {
  CString path;

  HRESULT hr = GetBrowserImagePath(BROWSER_IE, &path);
  if (SUCCEEDED(hr)) {
    EXPECT_EQ(0, path.CompareNoCase(
                     _T("C:\\Program Files\\Internet Explorer\\iexplore.exe")))
        << _T("Actual path: ") << path.GetString();
  } else {
    EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), hr);
  }

  hr = GetBrowserImagePath(BROWSER_FIREFOX, &path);
  if (SUCCEEDED(hr)) {
    EXPECT_TRUE(
        0 == path.CompareNoCase(
            _T("C:\\Program Files\\Mozilla Firefox\\firefox.exe")) ||
        0 == path.CompareNoCase(
            _T("C:\\PROGRA~1\\MOZILL~1\\FIREFOX.EXE")))
        << _T("Actual path: ") << path.GetString();
  } else {
    EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), hr);
  }

  hr = GetBrowserImagePath(BROWSER_CHROME, &path);
  if (SUCCEEDED(hr)) {
    EXPECT_TRUE(
        0 == path.CompareNoCase(
            _T("C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe")) ||
        0 == path.CompareNoCase(
            GetLocalAppDataPath() +
            _T("Google\\Chrome\\Application\\chrome.exe")))
        << _T("Actual path: ") << path.GetString();
  } else {
    EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), hr);
  }
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

