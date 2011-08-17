// Copyright 2007-2010 Google Inc.
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
#include <atlpath.h>
#include <atlsecurity.h>
#include <atlstr.h>
#include <map>
#include <vector>
#include "omaha/base/app_util.h"
#include "omaha/base/browser_utils.h"
#include "omaha/base/constants.h"
#include "omaha/base/const_utils.h"
#include "omaha/base/error.h"
#include "omaha/base/file.h"
#include "omaha/base/omaha_version.h"
#include "omaha/base/path.h"
#include "omaha/base/reg_key.h"
#include "omaha/base/scope_guard.h"
#include "omaha/base/scoped_ptr_cotask.h"
#include "omaha/base/string.h"
#include "omaha/base/time.h"
#include "omaha/base/user_info.h"
#include "omaha/base/utils.h"
#include "omaha/base/vistautil.h"
#include "omaha/base/vista_utils.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/common/const_group_policy.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/common/oem_install_utils.h"
#include "omaha/testing/resource.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace {

#define DUMMY_CLSID  _T("{6FC94136-0D4C-450e-99C2-BCDA72A9C8F0}")
const TCHAR* hkcr_key_name = _T("HKCR\\CLSID\\") DUMMY_CLSID;
const TCHAR* hklm_key_name = _T("HKLM\\Software\\Classes\\CLSID\\") DUMMY_CLSID;
const TCHAR* hkcu_key_name = _T("HKCU\\Software\\Classes\\CLSID\\") DUMMY_CLSID;

const TCHAR* kAppId = _T("{3DAE8C13-C394-481E-8163-4E7A7699084F}");

}  // namespace

namespace goopdate_utils {

static void Cleanup() {
  ASSERT_SUCCEEDED(RemoveRedirectHKCR());

  RegKey::DeleteKey(hkcr_key_name, true);
  RegKey::DeleteKey(hklm_key_name, true);
  RegKey::DeleteKey(hkcu_key_name, true);
}

static void TestGetBrowserToRestart(BrowserType stamped,
                                    bool found1,
                                    bool killed1,
                                    BrowserType def_browser,
                                    bool found2,
                                    bool killed2,
                                    BrowserType expected) {
  TerminateBrowserResult res(found1, killed1);
  TerminateBrowserResult def(found2, killed2);

  BrowserType type = BROWSER_UNKNOWN;
  if (expected == BROWSER_UNKNOWN) {
    EXPECT_FALSE(GetBrowserToRestart(stamped,
                                     def_browser,
                                     res,
                                     def,
                                     &type))
        << _T("stamped: ") << stamped << _T(" ") << found1 << _T(" ") << killed1
        << _T("   default: ") << def_browser << _T(" ") << found2 << _T(" ")
        << killed2;
  } else {
    EXPECT_TRUE(GetBrowserToRestart(stamped,
                                    def_browser,
                                    res,
                                    def,
                                    &type))
        << _T("stamped: ") << stamped << _T(" ") << found1 << _T(" ") << killed1
        << _T("   default: ") << def_browser << _T(" ") << found2 << _T(" ")
        << killed2;
  }
  EXPECT_EQ(expected, type)
      << _T("stamped: ") << stamped << _T(" ") << found1 << _T(" ") << killed1
      << _T("   default: ") << def_browser << _T(" ") << found2 << _T(" ")
      << killed2;
}

// TerminateAllBrowsers is not tested with valid browser values because the
// tests would terminate developers' browsers.

TEST(GoopdateUtilsTest, GetBrowserToRestart_StampedUnknown) {
  ExpectAsserts expect_asserts;
  TestGetBrowserToRestart(BROWSER_UNKNOWN, false, false,
                          BROWSER_IE, false, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_UNKNOWN, true, false,
                          BROWSER_IE, false, false,
                          BROWSER_IE);
}

TEST(GoopdateUtilsTest, GetBrowserToRestart_DefaultUnknown) {
  TestGetBrowserToRestart(BROWSER_IE, false, false,
                          BROWSER_UNKNOWN, false, false,
                          BROWSER_UNKNOWN);
}

TEST(GoopdateUtilsTest, GetBrowserToRestart_StampedAndDefaultUnknown) {
  ExpectAsserts expect_asserts;
  TestGetBrowserToRestart(BROWSER_UNKNOWN, false, false,
                          BROWSER_UNKNOWN, false, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_UNKNOWN, true, false,
                          BROWSER_UNKNOWN, false, false,
                          BROWSER_UNKNOWN);
}

TEST(GoopdateUtilsTest, GetBrowserToRestart_StampedDefault) {
  ExpectAsserts expect_asserts;
  TestGetBrowserToRestart(BROWSER_DEFAULT, false, false,
                          BROWSER_IE, false, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_DEFAULT, true, false,
                          BROWSER_IE, false, false,
                          BROWSER_IE);
}

TEST(GoopdateUtilsTest, GetBrowserToRestart_DefaultDefault) {
  ExpectAsserts expect_asserts;
  TestGetBrowserToRestart(BROWSER_IE, false, false,
                          BROWSER_DEFAULT, false, false,
                          BROWSER_UNKNOWN);
}

TEST(GoopdateUtilsTest, GetBrowserToRestart_StampedAndDefaultDefault) {
  ExpectAsserts expect_asserts;
  TestGetBrowserToRestart(BROWSER_DEFAULT, false, false,
                          BROWSER_DEFAULT, false, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_DEFAULT, true, false,
                          BROWSER_DEFAULT, false, false,
                          BROWSER_UNKNOWN);
}

TEST(GoopdateUtilsTest, GetBrowserToRestart_StampedMax) {
  ExpectAsserts expect_asserts;
  TestGetBrowserToRestart(BROWSER_MAX, false, false,
                          BROWSER_IE, false, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_MAX, true, false,
                          BROWSER_IE, false, false,
                          BROWSER_IE);
}

TEST(GoopdateUtilsTest, GetBrowserToRestart_DefaultMax) {
  ExpectAsserts expect_asserts;
  TestGetBrowserToRestart(BROWSER_IE, false, false,
                          BROWSER_MAX, false, false,
                          BROWSER_UNKNOWN);
}

TEST(GoopdateUtilsTest, GetBrowserToRestart_StampedAndDefaultMax) {
  ExpectAsserts expect_asserts;
  TestGetBrowserToRestart(BROWSER_MAX, false, false,
                          BROWSER_MAX, false, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_MAX, true, false,
                          BROWSER_MAX, false, false,
                          BROWSER_UNKNOWN);
}

TEST(GoopdateUtilsTest, GetBrowserToRestart_TypeIE_DefaultIE) {
  TestGetBrowserToRestart(BROWSER_IE, false, false,
                          BROWSER_IE, false, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, false, false,
                          BROWSER_IE, false, true,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, false, false,
                          BROWSER_IE, true, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, false, false,
                          BROWSER_IE, true, true,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, false, true,
                          BROWSER_IE, false, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, false, true,
                          BROWSER_IE, false, true,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, false, true,
                          BROWSER_IE, true, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, false, true,
                          BROWSER_IE, true, true,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, true, false,
                          BROWSER_IE, false, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, true, false,
                          BROWSER_IE, false, true,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, true, false,
                          BROWSER_IE, true, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, true, false,
                          BROWSER_IE, true, true,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, true, true,
                          BROWSER_IE, false, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, true, true,
                          BROWSER_IE, false, true,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, true, true,
                          BROWSER_IE, true, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, true, true,
                          BROWSER_IE, true, true,
                          BROWSER_IE);
}

TEST(GoopdateUtilsTest, GetBrowserToRestart_TypeIE_DefaultFirefox) {
  TestGetBrowserToRestart(BROWSER_IE, false, false,
                          BROWSER_FIREFOX, false, false,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_IE, false, false,
                          BROWSER_FIREFOX, false, true,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_IE, false, false,
                          BROWSER_FIREFOX, true, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_IE, false, false,
                          BROWSER_FIREFOX, true, true,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_IE, false, true,
                          BROWSER_FIREFOX, false, false,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_IE, false, true,
                          BROWSER_FIREFOX, false, true,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_IE, false, true,
                          BROWSER_FIREFOX, true, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_IE, false, true,
                          BROWSER_FIREFOX, true, true,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_IE, true, false,
                          BROWSER_FIREFOX, false, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, true, false,
                          BROWSER_FIREFOX, false, true,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, true, false,
                          BROWSER_FIREFOX, true, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, true, false,
                          BROWSER_FIREFOX, true, true,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, true, true,
                          BROWSER_FIREFOX, false, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, true, true,
                          BROWSER_FIREFOX, false, true,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, true, true,
                          BROWSER_FIREFOX, true, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, true, true,
                          BROWSER_FIREFOX, true, true,
                          BROWSER_IE);
}

TEST(GoopdateUtilsTest, GetBrowserToRestart_TypeIE_DefaultChrome) {
  TestGetBrowserToRestart(BROWSER_IE, false, false,
                          BROWSER_CHROME, false, false,
                          BROWSER_CHROME);
  TestGetBrowserToRestart(BROWSER_IE, false, false,
                          BROWSER_CHROME, false, true,
                          BROWSER_CHROME);
  TestGetBrowserToRestart(BROWSER_IE, false, false,
                          BROWSER_CHROME, true, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_IE, false, false,
                          BROWSER_CHROME, true, true,
                          BROWSER_CHROME);
  TestGetBrowserToRestart(BROWSER_IE, false, true,
                          BROWSER_CHROME, false, false,
                          BROWSER_CHROME);
  TestGetBrowserToRestart(BROWSER_IE, false, true,
                          BROWSER_CHROME, false, true,
                          BROWSER_CHROME);
  TestGetBrowserToRestart(BROWSER_IE, false, true,
                          BROWSER_CHROME, true, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_IE, false, true,
                          BROWSER_CHROME, true, true,
                          BROWSER_CHROME);
  TestGetBrowserToRestart(BROWSER_IE, true, false,
                          BROWSER_CHROME, false, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, true, false,
                          BROWSER_CHROME, false, true,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, true, false,
                          BROWSER_CHROME, true, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, true, false,
                          BROWSER_CHROME, true, true,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, true, true,
                          BROWSER_CHROME, false, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, true, true,
                          BROWSER_CHROME, false, true,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, true, true,
                          BROWSER_CHROME, true, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, true, true,
                          BROWSER_CHROME, true, true,
                          BROWSER_IE);
}

TEST(GoopdateUtilsTest, GetBrowserToRestart_TypeIE_DefaultUnknown) {
  TestGetBrowserToRestart(BROWSER_IE, false, false,
                          BROWSER_UNKNOWN, false, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_IE, false, false,
                          BROWSER_UNKNOWN, false, true,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_IE, false, false,
                          BROWSER_UNKNOWN, true, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_IE, false, false,
                          BROWSER_UNKNOWN, true, true,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_IE, false, true,
                          BROWSER_UNKNOWN, false, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_IE, false, true,
                          BROWSER_UNKNOWN, false, true,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_IE, false, true,
                          BROWSER_UNKNOWN, true, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_IE, false, true,
                          BROWSER_UNKNOWN, true, true,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_IE, true, false,
                          BROWSER_UNKNOWN, false, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, true, false,
                          BROWSER_UNKNOWN, false, true,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, true, false,
                          BROWSER_UNKNOWN, true, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, true, false,
                          BROWSER_UNKNOWN, true, true,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, true, true,
                          BROWSER_UNKNOWN, false, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, true, true,
                          BROWSER_UNKNOWN, false, true,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, true, true,
                          BROWSER_UNKNOWN, true, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_IE, true, true,
                          BROWSER_UNKNOWN, true, true,
                          BROWSER_IE);
}

TEST(GoopdateUtilsTest, GetBrowserToRestart_TypeFirefox_DefaultIE) {
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, false,
                          BROWSER_IE, false, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, false,
                          BROWSER_IE, false, true,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, false,
                          BROWSER_IE, true, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, false,
                          BROWSER_IE, true, true,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, true,
                          BROWSER_IE, false, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, true,
                          BROWSER_IE, false, true,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, true,
                          BROWSER_IE, true, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, true,
                          BROWSER_IE, true, true,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, false,
                          BROWSER_IE, false, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, false,
                          BROWSER_IE, false, true,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, false,
                          BROWSER_IE, true, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, false,
                          BROWSER_IE, true, true,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, true,
                          BROWSER_IE, false, false,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, true,
                          BROWSER_IE, false, true,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, true,
                          BROWSER_IE, true, false,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, true,
                          BROWSER_IE, true, true,
                          BROWSER_FIREFOX);
}

TEST(GoopdateUtilsTest, GetBrowserToRestart_TypeFirefox_DefaultFirefox) {
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, false,
                          BROWSER_FIREFOX, false, false,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, false,
                          BROWSER_FIREFOX, false, true,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, false,
                          BROWSER_FIREFOX, true, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, false,
                          BROWSER_FIREFOX, true, true,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, true,
                          BROWSER_FIREFOX, false, false,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, true,
                          BROWSER_FIREFOX, false, true,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, true,
                          BROWSER_FIREFOX, true, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, true,
                          BROWSER_FIREFOX, true, true,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, false,
                          BROWSER_FIREFOX, false, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, false,
                          BROWSER_FIREFOX, false, true,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, false,
                          BROWSER_FIREFOX, true, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, false,
                          BROWSER_FIREFOX, true, true,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, true,
                          BROWSER_FIREFOX, false, false,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, true,
                          BROWSER_FIREFOX, false, true,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, true,
                          BROWSER_FIREFOX, true, false,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, true,
                          BROWSER_FIREFOX, true, true,
                          BROWSER_FIREFOX);
}

TEST(GoopdateUtilsTest, GetBrowserToRestart_TypeFirefox_DefaultChrome) {
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, false,
                          BROWSER_CHROME, false, false,
                          BROWSER_CHROME);
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, false,
                          BROWSER_CHROME, false, true,
                          BROWSER_CHROME);
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, false,
                          BROWSER_CHROME, true, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, false,
                          BROWSER_CHROME, true, true,
                          BROWSER_CHROME);
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, true,
                          BROWSER_CHROME, false, false,
                          BROWSER_CHROME);
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, true,
                          BROWSER_CHROME, false, true,
                          BROWSER_CHROME);
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, true,
                          BROWSER_CHROME, true, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, true,
                          BROWSER_CHROME, true, true,
                          BROWSER_CHROME);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, false,
                          BROWSER_CHROME, false, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, false,
                          BROWSER_CHROME, false, true,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, false,
                          BROWSER_CHROME, true, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, false,
                          BROWSER_CHROME, true, true,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, true,
                          BROWSER_CHROME, false, false,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, true,
                          BROWSER_CHROME, false, true,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, true,
                          BROWSER_CHROME, true, false,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, true,
                          BROWSER_CHROME, true, true,
                          BROWSER_FIREFOX);
}

TEST(GoopdateUtilsTest, GetBrowserToRestart_TypeFirefox_DefaultUnknown) {
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, false,
                          BROWSER_UNKNOWN, false, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, false,
                          BROWSER_UNKNOWN, false, true,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, false,
                          BROWSER_UNKNOWN, true, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, false,
                          BROWSER_UNKNOWN, true, true,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, true,
                          BROWSER_UNKNOWN, false, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, true,
                          BROWSER_UNKNOWN, false, true,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, true,
                          BROWSER_UNKNOWN, true, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_FIREFOX, false, true,
                          BROWSER_UNKNOWN, true, true,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, false,
                          BROWSER_UNKNOWN, false, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, false,
                          BROWSER_UNKNOWN, false, true,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, false,
                          BROWSER_UNKNOWN, true, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, false,
                          BROWSER_UNKNOWN, true, true,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, true,
                          BROWSER_UNKNOWN, false, false,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, true,
                          BROWSER_UNKNOWN, false, true,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, true,
                          BROWSER_UNKNOWN, true, false,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_FIREFOX, true, true,
                          BROWSER_UNKNOWN, true, true,
                          BROWSER_FIREFOX);
}

TEST(GoopdateUtilsTest, GetBrowserToRestart_TypeChrome_DefaultIE) {
  TestGetBrowserToRestart(BROWSER_CHROME, false, false,
                          BROWSER_IE, false, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_CHROME, false, false,
                          BROWSER_IE, false, true,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_CHROME, false, false,
                          BROWSER_IE, true, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_CHROME, false, false,
                          BROWSER_IE, true, true,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_CHROME, false, true,
                          BROWSER_IE, false, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_CHROME, false, true,
                          BROWSER_IE, false, true,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_CHROME, false, true,
                          BROWSER_IE, true, false,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_CHROME, false, true,
                          BROWSER_IE, true, true,
                          BROWSER_IE);
  TestGetBrowserToRestart(BROWSER_CHROME, true, false,
                          BROWSER_IE, false, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_CHROME, true, false,
                          BROWSER_IE, false, true,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_CHROME, true, false,
                          BROWSER_IE, true, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_CHROME, true, false,
                          BROWSER_IE, true, true,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_CHROME, true, true,
                          BROWSER_IE, false, false,
                          BROWSER_CHROME);
  TestGetBrowserToRestart(BROWSER_CHROME, true, true,
                          BROWSER_IE, false, true,
                          BROWSER_CHROME);
  TestGetBrowserToRestart(BROWSER_CHROME, true, true,
                          BROWSER_IE, true, false,
                          BROWSER_CHROME);
  TestGetBrowserToRestart(BROWSER_CHROME, true, true,
                          BROWSER_IE, true, true,
                          BROWSER_CHROME);
}

TEST(GoopdateUtilsTest, GetBrowserToRestart_TypeChrome_DefaultFirefox) {
  TestGetBrowserToRestart(BROWSER_CHROME, false, false,
                          BROWSER_FIREFOX, false, false,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_CHROME, false, false,
                          BROWSER_FIREFOX, false, true,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_CHROME, false, false,
                          BROWSER_FIREFOX, true, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_CHROME, false, false,
                          BROWSER_FIREFOX, true, true,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_CHROME, false, true,
                          BROWSER_FIREFOX, false, false,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_CHROME, false, true,
                          BROWSER_FIREFOX, false, true,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_CHROME, false, true,
                          BROWSER_FIREFOX, true, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_CHROME, false, true,
                          BROWSER_FIREFOX, true, true,
                          BROWSER_FIREFOX);
  TestGetBrowserToRestart(BROWSER_CHROME, true, false,
                          BROWSER_FIREFOX, false, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_CHROME, true, false,
                          BROWSER_FIREFOX, false, true,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_CHROME, true, false,
                          BROWSER_FIREFOX, true, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_CHROME, true, false,
                          BROWSER_FIREFOX, true, true,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_CHROME, true, true,
                          BROWSER_FIREFOX, false, false,
                          BROWSER_CHROME);
  TestGetBrowserToRestart(BROWSER_CHROME, true, true,
                          BROWSER_FIREFOX, false, true,
                          BROWSER_CHROME);
  TestGetBrowserToRestart(BROWSER_CHROME, true, true,
                          BROWSER_FIREFOX, true, false,
                          BROWSER_CHROME);
  TestGetBrowserToRestart(BROWSER_CHROME, true, true,
                          BROWSER_FIREFOX, true, true,
                          BROWSER_CHROME);
}

TEST(GoopdateUtilsTest, GetBrowserToRestart_TypeChrome_DefaultChrome) {
  TestGetBrowserToRestart(BROWSER_CHROME, false, false,
                          BROWSER_CHROME, false, false,
                          BROWSER_CHROME);
  TestGetBrowserToRestart(BROWSER_CHROME, false, false,
                          BROWSER_CHROME, false, true,
                          BROWSER_CHROME);
  TestGetBrowserToRestart(BROWSER_CHROME, false, false,
                          BROWSER_CHROME, true, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_CHROME, false, false,
                          BROWSER_CHROME, true, true,
                          BROWSER_CHROME);
  TestGetBrowserToRestart(BROWSER_CHROME, false, true,
                          BROWSER_CHROME, false, false,
                          BROWSER_CHROME);
  TestGetBrowserToRestart(BROWSER_CHROME, false, true,
                          BROWSER_CHROME, false, true,
                          BROWSER_CHROME);
  TestGetBrowserToRestart(BROWSER_CHROME, false, true,
                          BROWSER_CHROME, true, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_CHROME, false, true,
                          BROWSER_CHROME, true, true,
                          BROWSER_CHROME);
  TestGetBrowserToRestart(BROWSER_CHROME, true, false,
                          BROWSER_CHROME, false, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_CHROME, true, false,
                          BROWSER_CHROME, false, true,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_CHROME, true, false,
                          BROWSER_CHROME, true, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_CHROME, true, false,
                          BROWSER_CHROME, true, true,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_CHROME, true, true,
                          BROWSER_CHROME, false, false,
                          BROWSER_CHROME);
  TestGetBrowserToRestart(BROWSER_CHROME, true, true,
                          BROWSER_CHROME, false, true,
                          BROWSER_CHROME);
  TestGetBrowserToRestart(BROWSER_CHROME, true, true,
                          BROWSER_CHROME, true, false,
                          BROWSER_CHROME);
  TestGetBrowserToRestart(BROWSER_CHROME, true, true,
                          BROWSER_CHROME, true, true,
                          BROWSER_CHROME);
}

TEST(GoopdateUtilsTest, GetBrowserToRestart_TypeChrome_DefaultUnknown) {
  TestGetBrowserToRestart(BROWSER_CHROME, false, false,
                          BROWSER_UNKNOWN, false, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_CHROME, false, false,
                          BROWSER_UNKNOWN, false, true,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_CHROME, false, false,
                          BROWSER_UNKNOWN, true, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_CHROME, false, false,
                          BROWSER_UNKNOWN, true, true,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_CHROME, false, true,
                          BROWSER_UNKNOWN, false, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_CHROME, false, true,
                          BROWSER_UNKNOWN, false, true,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_CHROME, false, true,
                          BROWSER_UNKNOWN, true, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_CHROME, false, true,
                          BROWSER_UNKNOWN, true, true,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_CHROME, true, false,
                          BROWSER_UNKNOWN, false, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_CHROME, true, false,
                          BROWSER_UNKNOWN, false, true,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_CHROME, true, false,
                          BROWSER_UNKNOWN, true, false,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_CHROME, true, false,
                          BROWSER_UNKNOWN, true, true,
                          BROWSER_UNKNOWN);
  TestGetBrowserToRestart(BROWSER_CHROME, true, true,
                          BROWSER_UNKNOWN, false, false,
                          BROWSER_CHROME);
  TestGetBrowserToRestart(BROWSER_CHROME, true, true,
                          BROWSER_UNKNOWN, false, true,
                          BROWSER_CHROME);
  TestGetBrowserToRestart(BROWSER_CHROME, true, true,
                          BROWSER_UNKNOWN, true, false,
                          BROWSER_CHROME);
  TestGetBrowserToRestart(BROWSER_CHROME, true, true,
                          BROWSER_UNKNOWN, true, true,
                          BROWSER_CHROME);
}

TEST(GoopdateUtilsTest, ConvertStringToBrowserType) {
  BrowserType type = BROWSER_UNKNOWN;
  ASSERT_SUCCEEDED(ConvertStringToBrowserType(_T("0"), &type));
  ASSERT_EQ(BROWSER_UNKNOWN, type);

  ASSERT_SUCCEEDED(ConvertStringToBrowserType(_T("1"), &type));
  ASSERT_EQ(BROWSER_DEFAULT, type);

  ASSERT_SUCCEEDED(ConvertStringToBrowserType(_T("2"), &type));
  ASSERT_EQ(BROWSER_IE, type);

  ASSERT_SUCCEEDED(ConvertStringToBrowserType(_T("3"), &type));
  ASSERT_EQ(BROWSER_FIREFOX, type);

  ASSERT_SUCCEEDED(ConvertStringToBrowserType(_T("4"), &type));
  ASSERT_EQ(BROWSER_CHROME, type);

  ASSERT_FAILED(ConvertStringToBrowserType(_T("5"), &type));
  ASSERT_FAILED(ConvertStringToBrowserType(_T("asdf"), &type));
  ASSERT_FAILED(ConvertStringToBrowserType(_T("234"), &type));
  ASSERT_FAILED(ConvertStringToBrowserType(_T("-1"), &type));
}

TEST(GoopdateUtilsTest, RedirectHKCRTest) {
  RegKey key;
  Cleanup();

  if (vista_util::IsUserAdmin()) {
    // Only run this part of the test for Admins, because non-admins cannot
    // write to HKLM.

    // Without redirection, a HKCR write should write HKLM\Software\Classes,
    // assuming that the key does not already exist in HKCU.
    ASSERT_SUCCEEDED(key.Create(hkcr_key_name));
    ASSERT_TRUE(RegKey::HasKey(hklm_key_name));
    ASSERT_FALSE(RegKey::HasKey(hkcu_key_name));

    Cleanup();

    ASSERT_SUCCEEDED(RedirectHKCR(true));

    // With HKLM redirection, a HKCR write should write HKLM\Software\Classes.
    ASSERT_SUCCEEDED(key.Create(hkcr_key_name));
    ASSERT_TRUE(RegKey::HasKey(hklm_key_name));
    ASSERT_FALSE(RegKey::HasKey(hkcu_key_name));

    Cleanup();
  } else {
    std::wcout << _T("\tPart of this test did not run because the user ")
                  _T("is not an admin.") << std::endl;
  }

  ASSERT_SUCCEEDED(RedirectHKCR(false));

  // With HKCU redirection, a HKCR write should write HKCU\Software\Classes.
  ASSERT_SUCCEEDED(key.Create(hkcr_key_name));
  ASSERT_FALSE(RegKey::HasKey(hklm_key_name));
  ASSERT_TRUE(RegKey::HasKey(hkcu_key_name));

  ASSERT_SUCCEEDED(RemoveRedirectHKCR());

  if (vista_util::IsUserAdmin()) {
    // Without redirection, the following HKCR writes should write
    // HKCU\Software\Classes.
    // This is because the key already exists in HKCU from the writes above.
    ASSERT_SUCCEEDED(key.Create(hkcr_key_name));
    ASSERT_EQ(user_info::IsRunningAsSystem(), RegKey::HasKey(hklm_key_name));
    ASSERT_TRUE(RegKey::HasKey(hkcu_key_name));
  } else {
    std::wcout << _T("\tPart of this test did not run because the user ")
                  _T("is not an admin.") << std::endl;
  }

  Cleanup();
}

TEST(GoopdateUtilsTest, GetOSInfo) {
  CString os_version;
  CString service_pack;
  EXPECT_SUCCEEDED(GetOSInfo(&os_version, &service_pack));
  EXPECT_TRUE(!os_version.IsEmpty());
}

class GoopdateUtilsRegistryProtectedTest : public testing::Test {
 protected:
  GoopdateUtilsRegistryProtectedTest()
      : hive_override_key_name_(kRegistryHiveOverrideRoot) {
  }

  CString hive_override_key_name_;

  virtual void SetUp() {
    RegKey::DeleteKey(hive_override_key_name_, true);
    OverrideRegistryHives(hive_override_key_name_);
  }

  virtual void TearDown() {
    RestoreRegistryHives();
    ASSERT_SUCCEEDED(RegKey::DeleteKey(hive_override_key_name_, true));
  }
};

class GoopdateUtilsRegistryProtectedBooleanTest
    : public ::testing::TestWithParam<bool> {
 protected:
  GoopdateUtilsRegistryProtectedBooleanTest()
      : hive_override_key_name_(kRegistryHiveOverrideRoot) {
  }

  CString hive_override_key_name_;

  virtual void SetUp() {
    RegKey::DeleteKey(hive_override_key_name_, true);
    OverrideRegistryHives(hive_override_key_name_);
  }

  virtual void TearDown() {
    RestoreRegistryHives();
    ASSERT_SUCCEEDED(RegKey::DeleteKey(hive_override_key_name_, true));
  }
};

// Some methods used by goopdate_utils rely on registry entries that are
// overridden in the registry, so we need to write it.
class GoopdateUtilsRegistryProtectedWithMachineFolderPathsTest
    : public GoopdateUtilsRegistryProtectedTest {
 protected:
  virtual void SetUp() {
    // The tests start GoogleUpdate processes running as user and these
    // processes need the following registry value.
    ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                      kRegValueInstalledVersion,
                                      GetVersionString()));

    GoopdateUtilsRegistryProtectedTest::SetUp();

    // Creates a registry value for the Windows shell functions to work when
    // the registry hives are redirected.
    const TCHAR kWindowsCurrentVersionKeyPath[] =
        _T("HKLM\\Software\\Microsoft\\Windows\\CurrentVersion");
    const TCHAR kProgramFilesDirValueName[] = _T("ProgramFilesDir");
    const TCHAR kProgramFilesPath[] = _T("C:\\Program Files");
    ASSERT_SUCCEEDED(RegKey::SetValue(kWindowsCurrentVersionKeyPath,
                                      kProgramFilesDirValueName,
                                      kProgramFilesPath));
  }
};

// Some methods used by goopdate_utils rely on registry entries that are
// overridden in the registry, so we need to write it.
class GoopdateUtilsRegistryProtectedWithUserFolderPathsTest
    : public GoopdateUtilsRegistryProtectedTest {
 protected:
  virtual void SetUp() {
  const TCHAR kUserShellKeyPath[] =
        _T("HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\")
        _T("User Shell Folders");
    const TCHAR kLocalAppDataValueDirName[] = _T("Local AppData");
    const TCHAR kLocalAppDataPath[] =
        _T("%USERPROFILE%\\Local Settings\\Application Data");

    GoopdateUtilsRegistryProtectedTest::SetUp();
    ASSERT_SUCCEEDED(RegKey::SetValueExpandSZ(kUserShellKeyPath,
                                              kLocalAppDataValueDirName,
                                              kLocalAppDataPath));
  }
};

class VersionProtectedTest : public RegistryProtectedTest {
 protected:
  VersionProtectedTest()
      : RegistryProtectedTest(),
        module_version_(GetVersion()) {
  }

  virtual void SetUp() {
    RegistryProtectedTest::SetUp();
    InitializeVersion(kFakeVersion);
  }

  virtual void TearDown() {
    InitializeVersion(module_version_);
    RegistryProtectedTest::TearDown();
  }

  const ULONGLONG module_version_;
  static const ULONGLONG kFakeVersion = 0x0005000600070008;
};

// pv should be ignored.
TEST_F(GoopdateUtilsRegistryProtectedWithMachineFolderPathsTest,
       BuildGoogleUpdateExePath_MachineVersionFound) {
  ASSERT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_CLIENTS_GOOPDATE,
                                    _T("pv"),
                                    _T("1.2.3.4")));

  CString path = BuildGoogleUpdateExePath(true);
  CString program_files_path;
  EXPECT_SUCCEEDED(GetFolderPath(CSIDL_PROGRAM_FILES, &program_files_path));
  EXPECT_STREQ(program_files_path + _T("\\") + SHORT_COMPANY_NAME +
               _T("\\") + PRODUCT_NAME + _T("\\GoogleUpdate.exe"),
               path);
}

TEST_F(GoopdateUtilsRegistryProtectedWithMachineFolderPathsTest,
       BuildGoogleUpdateExePath_MachineVersionNotFound) {
  // Test when the key doesn't exist.
  CString path = BuildGoogleUpdateExePath(true);
  CString program_files_path;
  EXPECT_SUCCEEDED(GetFolderPath(CSIDL_PROGRAM_FILES, &program_files_path));
  EXPECT_STREQ(program_files_path + _T("\\") + SHORT_COMPANY_NAME +
               _T("\\") + PRODUCT_NAME + _T("\\GoogleUpdate.exe"),
               path);

  // Test when the key exists but the value doesn't.
  ASSERT_SUCCEEDED(RegKey::CreateKey(MACHINE_REG_CLIENTS_GOOPDATE));
  path = BuildGoogleUpdateExePath(true);
  EXPECT_STREQ(program_files_path + _T("\\") + SHORT_COMPANY_NAME +
               _T("\\") + PRODUCT_NAME + _T("\\GoogleUpdate.exe"),
               path);
}

// pv should be ignored.
TEST_F(GoopdateUtilsRegistryProtectedWithUserFolderPathsTest,
       BuildGoogleUpdateExePath_UserVersionFound) {
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                    _T("pv"),
                                    _T("1.2.3.4")));

  CString path = BuildGoogleUpdateExePath(false);

  CString user_appdata;
  EXPECT_SUCCEEDED(GetFolderPath(CSIDL_LOCAL_APPDATA, &user_appdata));
  CString expected_path;
  expected_path.Format(_T("%s\\") SHORT_COMPANY_NAME _T("\\")
                       PRODUCT_NAME _T("\\GoogleUpdate.exe"),
                       user_appdata);
  EXPECT_STREQ(expected_path, path);
}

TEST_F(GoopdateUtilsRegistryProtectedWithUserFolderPathsTest,
       BuildGoogleUpdateExePath_UserVersionNotFound) {
  CString user_appdata;
  EXPECT_SUCCEEDED(GetFolderPath(CSIDL_LOCAL_APPDATA, &user_appdata));
  CString expected_path;
  expected_path.Format(_T("%s\\") SHORT_COMPANY_NAME _T("\\")
                       PRODUCT_NAME _T("\\GoogleUpdate.exe"),
                       user_appdata);

  // Test when the key doesn't exist.
  CString path = BuildGoogleUpdateExePath(false);
  EXPECT_STREQ(expected_path, path);

  // Test when the key exists but the value doesn't.
  ASSERT_SUCCEEDED(RegKey::CreateKey(USER_REG_CLIENTS_GOOPDATE));
  path = BuildGoogleUpdateExePath(false);
  EXPECT_STREQ(expected_path, path);
}

// The version is no longer used by StartGoogleUpdateWithArgs, so the return
// value depends on whether program_files\Google\Update\GoogleUpdate.exe exists.
// The arguments must be valid to avoid displaying invalid command line error.
TEST_F(GoopdateUtilsRegistryProtectedWithMachineFolderPathsTest,
       StartGoogleUpdateWithArgs_MachineVersionVersionDoesNotExist) {
  ASSERT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_CLIENTS_GOOPDATE,
                                    _T("pv"),
                                    _T("1.2.3.4")));
  const TCHAR* kArgs = _T("/cr");
  HRESULT hr = StartGoogleUpdateWithArgs(true, kArgs, NULL);
  EXPECT_TRUE(S_OK == hr || HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) == hr);
}

// The version is no longer used by StartGoogleUpdateWithArgs, so the return
// value depends on whether <user_folder>\Google\Update\GoogleUpdate.exe exists.
// The arguments must be valid to avoid displaying invalid command line error.
TEST_F(GoopdateUtilsRegistryProtectedWithUserFolderPathsTest,
       StartGoogleUpdateWithArgs_UserVersionVersionDoesNotExist) {
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                    _T("pv"),
                                    _T("1.2.3.4")));
  const TCHAR* kArgs = _T("/cr");
  HRESULT hr = StartGoogleUpdateWithArgs(false, kArgs, NULL);
  EXPECT_TRUE(S_OK == hr || HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) == hr);
}

TEST(GoopdateUtilsTest, BuildInstallDirectory_Machine) {
  const CPath dir = BuildInstallDirectory(true, _T("1.2.3.0"));
  CString program_files_path;
  EXPECT_SUCCEEDED(GetFolderPath(CSIDL_PROGRAM_FILES, &program_files_path));
  EXPECT_STREQ(program_files_path + _T("\\") + SHORT_COMPANY_NAME +
               _T("\\") + PRODUCT_NAME + _T("\\1.2.3.0"), dir);
}

TEST(GoopdateUtilsTest, BuildInstallDirectory_User) {
  CPath expected_path(GetGoogleUpdateUserPath());
  expected_path.Append(_T("4.5.6.7"));
  EXPECT_STREQ(expected_path,
               BuildInstallDirectory(false, _T("4.5.6.7")));
}

TEST(GoopdateUtilsTest, ConvertBrowserTypeToString) {
  for (int i = 0; i < BROWSER_MAX; ++i) {
    CString str_type = ConvertBrowserTypeToString(
        static_cast<BrowserType>(i));
    BrowserType type = BROWSER_UNKNOWN;
    ASSERT_HRESULT_SUCCEEDED(
        ConvertStringToBrowserType(str_type, &type));
    ASSERT_EQ(static_cast<int>(type), i);
  }
}

TEST(GoopdateUtilsTest, UniqueEventInEnvironment_User) {
  const TCHAR* kEnvVarName = _T("SOME_ENV_VAR_FOR_TEST");
  scoped_event created_event;
  scoped_event opened_event;

  ASSERT_HRESULT_SUCCEEDED(CreateUniqueEventInEnvironment(
      kEnvVarName,
      false,
      address(created_event)));
  ASSERT_TRUE(created_event);
  EXPECT_EQ(WAIT_TIMEOUT, ::WaitForSingleObject(get(created_event), 0));

  TCHAR event_name[MAX_PATH] = {0};
  EXPECT_TRUE(
      ::GetEnvironmentVariable(kEnvVarName, event_name, arraysize(event_name)));

  ASSERT_HRESULT_SUCCEEDED(OpenUniqueEventFromEnvironment(
      kEnvVarName,
      false,
      address(opened_event)));
  ASSERT_TRUE(opened_event);

  EXPECT_TRUE(::SetEvent(get(opened_event)));
  EXPECT_EQ(WAIT_OBJECT_0, ::WaitForSingleObject(get(created_event), 0));

  EXPECT_TRUE(::SetEnvironmentVariable(kEnvVarName, NULL));
}

TEST(GoopdateUtilsTest, UniqueEventInEnvironment_Machine) {
  const TCHAR* kEnvVarName = _T("OTHER_ENV_VAR_FOR_TEST");
  scoped_event created_event;
  scoped_event opened_event;
  TCHAR event_name[MAX_PATH] = {0};

  if (!vista_util::IsUserAdmin()) {
    EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_INVALID_OWNER),
              CreateUniqueEventInEnvironment(kEnvVarName,
                                             true,
                                             address(created_event)));
    EXPECT_FALSE(created_event);

    EXPECT_FALSE(::GetEnvironmentVariable(kEnvVarName,
                                          event_name,
                                          arraysize(event_name)));
    return;
  }

  ASSERT_HRESULT_SUCCEEDED(CreateUniqueEventInEnvironment(
      kEnvVarName,
      true,
      address(created_event)));
  ASSERT_TRUE(created_event);
  EXPECT_EQ(WAIT_TIMEOUT, ::WaitForSingleObject(get(created_event), 0));

  EXPECT_TRUE(
      ::GetEnvironmentVariable(kEnvVarName, event_name, arraysize(event_name)));

  ASSERT_HRESULT_SUCCEEDED(OpenUniqueEventFromEnvironment(
      kEnvVarName,
      true,
      address(opened_event)));
  ASSERT_TRUE(opened_event);

  EXPECT_TRUE(::SetEvent(get(opened_event)));
  EXPECT_EQ(WAIT_OBJECT_0, ::WaitForSingleObject(get(created_event), 0));

  EXPECT_TRUE(::SetEnvironmentVariable(kEnvVarName, NULL));
}

TEST(GoopdateUtilsTest, UniqueEventInEnvironment_UserMachineMismatch) {
  const TCHAR* kEnvVarName = _T("ENV_VAR_FOR_MIXED_TEST");
  scoped_event created_event;
  scoped_event opened_event;

  ASSERT_HRESULT_SUCCEEDED(CreateUniqueEventInEnvironment(
      kEnvVarName,
      false,
      address(created_event)));
  ASSERT_TRUE(created_event);
  EXPECT_EQ(WAIT_TIMEOUT, ::WaitForSingleObject(get(created_event), 0));

  TCHAR event_name[MAX_PATH] = {0};
  EXPECT_TRUE(
      ::GetEnvironmentVariable(kEnvVarName, event_name, arraysize(event_name)));

  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            OpenUniqueEventFromEnvironment(kEnvVarName,
                                           true,
                                           address(opened_event)));

  EXPECT_TRUE(::SetEnvironmentVariable(kEnvVarName, NULL));
}

TEST(GoopdateUtilsTest, OpenUniqueEventFromEnvironment_EnvVarDoesNotExist) {
  const TCHAR* kEnvVarName = _T("ANOTHER_ENV_VAR_FOR_TEST");
  scoped_event opened_event;
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_ENVVAR_NOT_FOUND),
            OpenUniqueEventFromEnvironment(kEnvVarName,
                                           false,
                                           address(opened_event)));
}

TEST(GoopdateUtilsTest, OpenUniqueEventFromEnvironment_EventDoesNotExist) {
  const TCHAR* kEnvVarName = _T("YET_ANOTHER_ENV_VAR_FOR_TEST");

  EXPECT_TRUE(::SetEnvironmentVariable(kEnvVarName, _T("foo")));

  scoped_event opened_event;
    EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
              OpenUniqueEventFromEnvironment(kEnvVarName,
                                             false,
                                             address(opened_event)));

  EXPECT_TRUE(::SetEnvironmentVariable(kEnvVarName, NULL));
}


CString GetTempFile() {
  TCHAR temp_path[MAX_PATH] = {0};
  TCHAR temp_file[MAX_PATH] = {0};

  EXPECT_LT(::GetTempPath(arraysize(temp_path), temp_path),
            arraysize(temp_path));
  EXPECT_NE(0, ::GetTempFileName(temp_path, _T("ut_"), 0, temp_file));
  return CString(temp_file);
}

typedef std::map<CString, CString> StringMap;
typedef StringMap::const_iterator StringMapIter;

TEST(GoopdateUtilsTest, ReadNameValuePairsFromFileTest_MissingFile) {
  CString temp_file = GetTempFile();
  ::DeleteFile(temp_file);

  ASSERT_FALSE(File::Exists(temp_file));

  StringMap pairs_read;
  ASSERT_FAILED(ReadNameValuePairsFromFile(temp_file,
                                           _T("my_group"),
                                           &pairs_read));
  ASSERT_EQ(0, pairs_read.size());
}

TEST(GoopdateUtilsTest, ReadNameValuePairsFromFileTest_ReadEmpty) {
  CString temp_file = GetTempFile();
  ON_SCOPE_EXIT(::DeleteFile, temp_file.GetString());
  File file_write;
  EXPECT_SUCCEEDED(file_write.Open(temp_file, true, false));
  file_write.Close();

  StringMap pairs_read;
  ASSERT_SUCCEEDED(ReadNameValuePairsFromFile(temp_file,
                                              _T("my_group"),
                                              &pairs_read));
  ASSERT_EQ(0, pairs_read.size());
}

void ValidateStringMapEquality(const StringMap& expected,
                               const StringMap& actual) {
  ASSERT_EQ(expected.size(), actual.size());

  StringMapIter it_expected = expected.begin();
  for (; it_expected != expected.end(); ++it_expected) {
    StringMapIter it_actual = actual.find(it_expected->first);
    ASSERT_TRUE(it_actual != actual.end());
    ASSERT_STREQ(it_expected->second, it_actual->second);
  }
}

TEST(GoopdateUtilsTest, ReadNameValuePairsFromFileTest_ReadOnePair) {
  CString group = _T("my_group");

  StringMap pairs_write;
  pairs_write[_T("some_name")] = _T("some_value");

  CString temp_file = GetTempFile();
  ON_SCOPE_EXIT(::DeleteFile, temp_file.GetString());
  ASSERT_SUCCEEDED(WriteNameValuePairsToFile(temp_file, group, pairs_write));
  ASSERT_TRUE(File::Exists(temp_file));

  StringMap pairs_read;
  ASSERT_SUCCEEDED(ReadNameValuePairsFromFile(temp_file, group, &pairs_read));

  ValidateStringMapEquality(pairs_write, pairs_read);
}

TEST(GoopdateUtilsTest, ReadNameValuePairsFromFileTest_ReadManyPairs) {
  CString group = _T("my_group");

  StringMap pairs_write;
  const int kCountPairs = 10;
  for (int i = 1; i <= kCountPairs; ++i) {
    CString name;
    name.Format(_T("name%d"), i);
    CString value;
    value.Format(_T("value%d"), i);
    pairs_write[name] = value;
  }

  CString temp_file = GetTempFile();
  ON_SCOPE_EXIT(::DeleteFile, temp_file.GetString());
  ASSERT_SUCCEEDED(WriteNameValuePairsToFile(temp_file, group, pairs_write));
  ASSERT_TRUE(File::Exists(temp_file));

  StringMap pairs_read;
  ASSERT_SUCCEEDED(ReadNameValuePairsFromFile(temp_file, group, &pairs_read));

  ValidateStringMapEquality(pairs_write, pairs_read);
}

TEST(GoopdateUtilsTest, IsGoogleUpdate2OrLater_LegacyVersions) {
  EXPECT_FALSE(IsGoogleUpdate2OrLater(_T("1.0.0.0")));
  EXPECT_FALSE(IsGoogleUpdate2OrLater(_T("1.1.103.9")));
  EXPECT_FALSE(IsGoogleUpdate2OrLater(_T("1.1.65535.65535")));
}

TEST(GoopdateUtilsTest, IsGoogleUpdate2OrLater_Omaha2AndLater) {
  EXPECT_TRUE(IsGoogleUpdate2OrLater(_T("1.2.0.0")));
  EXPECT_TRUE(IsGoogleUpdate2OrLater(_T("1.2.0111.2222")));
  EXPECT_TRUE(IsGoogleUpdate2OrLater(_T("1.3.456.7890")));
  EXPECT_TRUE(IsGoogleUpdate2OrLater(_T("2.0.0.0")));
}

TEST(GoopdateUtilsTest, IsGoogleUpdate2OrLater_VersionZero) {
  ExpectAsserts expect_asserts;
  EXPECT_FALSE(IsGoogleUpdate2OrLater(_T("0.0.0.0")));
}

TEST(GoopdateUtilsTest, IsGoogleUpdate2OrLater_VersionUpperLimits) {
  EXPECT_TRUE(IsGoogleUpdate2OrLater(_T("65535.65535.65535.65535")));

  ExpectAsserts expect_asserts;
  EXPECT_FALSE(IsGoogleUpdate2OrLater(_T("65536.65536.65536.65536")));
  EXPECT_FALSE(IsGoogleUpdate2OrLater(_T("1.2.65536.65536")));
  EXPECT_FALSE(IsGoogleUpdate2OrLater(_T("1.1.65536.65536")));
}

TEST(GoopdateUtilsTest, IsGoogleUpdate2OrLater_TooFewElements) {
  ExpectAsserts expect_asserts;
  EXPECT_FALSE(IsGoogleUpdate2OrLater(_T("1.1.1")));
}

TEST(GoopdateUtilsTest, IsGoogleUpdate2OrLater_ExtraPeriod) {
  ExpectAsserts expect_asserts;
  EXPECT_FALSE(IsGoogleUpdate2OrLater(_T("1.1.2.3.")));
}

TEST(GoopdateUtilsTest, IsGoogleUpdate2OrLater_TooManyElements) {
  ExpectAsserts expect_asserts;
  EXPECT_FALSE(IsGoogleUpdate2OrLater(_T("1.1.2.3.4")));
}

TEST(GoopdateUtilsTest, IsGoogleUpdate2OrLater_Char) {
  ExpectAsserts expect_asserts;
  EXPECT_FALSE(IsGoogleUpdate2OrLater(_T("1.B.3.4")));
  EXPECT_FALSE(IsGoogleUpdate2OrLater(_T("1.2.3.B")));
  EXPECT_FALSE(IsGoogleUpdate2OrLater(_T("1.2.3.9B")));
}

TEST(GoopdateUtilsTest, WriteInstallerDataToTempFile) {
  CStringA utf8_bom;
  utf8_bom.Format("%c%c%c", 0xEF, 0xBB, 0xBF);

  std::vector<CString> list_installer_data;
  list_installer_data.push_back(_T(""));
  list_installer_data.push_back(_T("hello\n"));
  list_installer_data.push_back(_T("good bye"));
  list_installer_data.push_back(_T("  there  you\n     go "));
  list_installer_data.push_back(_T("\"http://foo.bar.org/?q=stuff&h=other\""));
  list_installer_data.push_back(_T("foo\r\nbar\n"));
  list_installer_data.push_back(_T("foo\n\rbar"));    // LFCR is not recognized.
  list_installer_data.push_back(_T("this is a string over 1024 characters.  ------------------------------01------------------------------02------------------------------03------------------------------04------------------------------05------------------------------06------------------------------07------------------------------08------------------------------09------------------------------10------------------------------11------------------------------12------------------------------13------------------------------14------------------------------15------------------------------16------------------------------17------------------------------18------------------------------19------------------------------20------------------------------21------------------------------22------------------------------23------------------------------24------------------------------25------------------------------26------------------------------27------------------------------28------------------------------29------------------------------30------------------------------31------------------------------32------------------------------33------------------------------34------------------------------35------------------------------36------------------------------37------------------------------38------------------------------39------------------------------40 end.")); //NOLINT

  std::vector<CStringA> expected_installer_data;
  expected_installer_data.push_back("");
  expected_installer_data.push_back("hello\n");
  expected_installer_data.push_back("good bye");
  expected_installer_data.push_back("  there  you\n     go ");
  expected_installer_data.push_back("\"http://foo.bar.org/?q=stuff&h=other\"");
  expected_installer_data.push_back("foo\r\nbar\n");
  expected_installer_data.push_back("foo\n\rbar");
  expected_installer_data.push_back("this is a string over 1024 characters.  ------------------------------01------------------------------02------------------------------03------------------------------04------------------------------05------------------------------06------------------------------07------------------------------08------------------------------09------------------------------10------------------------------11------------------------------12------------------------------13------------------------------14------------------------------15------------------------------16------------------------------17------------------------------18------------------------------19------------------------------20------------------------------21------------------------------22------------------------------23------------------------------24------------------------------25------------------------------26------------------------------27------------------------------28------------------------------29------------------------------30------------------------------31------------------------------32------------------------------33------------------------------34------------------------------35------------------------------36------------------------------37------------------------------38------------------------------39------------------------------40 end."); //NOLINT

  ASSERT_EQ(expected_installer_data.size(), list_installer_data.size());

  for (size_t i = 0; i < list_installer_data.size(); ++i) {
    CString installer_data = list_installer_data[i];
    SCOPED_TRACE(installer_data);

    CString file_path;
    HRESULT hr = WriteInstallerDataToTempFile(installer_data, &file_path);
    ON_SCOPE_EXIT(::DeleteFile, file_path.GetString());
    EXPECT_SUCCEEDED(hr);

    // TODO(omaha): consider eliminating the special case.
    // WriteInstallerDataToTempFile() will return S_FALSE with "" data.
    if (S_OK == hr) {
      File file;
      const int kBufferLen = 4096;
      std::vector<byte> data_line(kBufferLen);
      EXPECT_SUCCEEDED(file.Open(file_path, false, false));
      uint32 bytes_read(0);
      EXPECT_SUCCEEDED(file.Read(data_line.size(),
                                 &data_line.front(),
                                 &bytes_read));
      data_line.resize(bytes_read);
      data_line.push_back(0);
      EXPECT_STREQ(utf8_bom + expected_installer_data[i],
                   reinterpret_cast<const char*>(&data_line.front()));
      EXPECT_SUCCEEDED(file.Close());
    } else {
      EXPECT_TRUE(installer_data.IsEmpty());
    }
  }
}

TEST_F(GoopdateUtilsRegistryProtectedTest, UpdateLastChecked) {
  EXPECT_SUCCEEDED(UpdateLastChecked(false));
  EXPECT_FALSE(ShouldCheckForUpdates(false));

  ConfigManager::Instance()->SetLastCheckedTime(false, 0);
  EXPECT_TRUE(ShouldCheckForUpdates(false));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       ShouldCheckForUpdates_NoLastCheckedPresent) {
  EXPECT_TRUE(ShouldCheckForUpdates(false));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       ShouldCheckForUpdates_LastCheckedPresent) {
  const uint32 now = Time64ToInt32(GetCurrent100NSTime());

  ConfigManager::Instance()->SetLastCheckedTime(false, now - 10);
  EXPECT_FALSE(ShouldCheckForUpdates(false));

  ConfigManager::Instance()->SetLastCheckedTime(false,
                                                now - kLastCheckPeriodSec - 1);
  EXPECT_TRUE(ShouldCheckForUpdates(false));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       ShouldCheckForUpdates_LastCheckedInFuture) {
  const uint32 now = Time64ToInt32(GetCurrent100NSTime());

  // The absolute difference is within the check period.
  ConfigManager::Instance()->SetLastCheckedTime(false, now + 600);
  EXPECT_FALSE(ShouldCheckForUpdates(false));

  // The absolute difference is greater than the check period.
  ConfigManager::Instance()->SetLastCheckedTime(false,
                                                now + kLastCheckPeriodSec + 1);
  EXPECT_TRUE(ShouldCheckForUpdates(false));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       ShouldCheckForUpdates_PeriodZero) {
  EXPECT_SUCCEEDED(
      RegKey::SetValue(kRegKeyGoopdateGroupPolicy,
                       kRegValueAutoUpdateCheckPeriodOverrideMinutes,
                       static_cast<DWORD>(0)));

  EXPECT_FALSE(ShouldCheckForUpdates(false));
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       ShouldCheckForUpdates_PeriodOverride) {
  const DWORD kOverrideMinutes = 10;
  const DWORD kOverrideSeconds = kOverrideMinutes * 60;
  const uint32 now = Time64ToInt32(GetCurrent100NSTime());

  EXPECT_SUCCEEDED(
      RegKey::SetValue(kRegKeyGoopdateGroupPolicy,
                       kRegValueAutoUpdateCheckPeriodOverrideMinutes,
                       kOverrideMinutes));

  ConfigManager::Instance()->SetLastCheckedTime(false, now - 10);
  EXPECT_FALSE(ShouldCheckForUpdates(false));

  ConfigManager::Instance()->SetLastCheckedTime(false,
                                                now - kOverrideSeconds - 1);
  EXPECT_TRUE(ShouldCheckForUpdates(false));
}

TEST_P(GoopdateUtilsRegistryProtectedBooleanTest, CreateUserId) {
  const bool is_machine = GetParam();
  CString user_id1, user_id2;

  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE_DEV,
                                    kRegValueForceUsageStats,
                                    _T("1")));

  EXPECT_SUCCEEDED(goopdate_utils::CreateUserId(is_machine));
  user_id1 = goopdate_utils::GetUserIdLazyInit(is_machine);

  // If user id exists, CreateUserId() should not create a new one.
  EXPECT_SUCCEEDED(goopdate_utils::CreateUserId(is_machine));
  user_id2 = goopdate_utils::GetUserIdLazyInit(is_machine);
  EXPECT_STREQ(user_id1, user_id2);

  goopdate_utils::DeleteUserId(is_machine);

  // Recreate user id should result in a different id.
  user_id2 = goopdate_utils::GetUserIdLazyInit(is_machine);
  EXPECT_STRNE(user_id1, user_id2);

  // Id generation should fail if machine is in OEM install state.
  const DWORD now = Time64ToInt32(GetCurrent100NSTime());
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("OemInstallTime"),
                                    now));
  if (is_machine) {
    ASSERT_TRUE(oem_install_utils::IsOemInstalling(is_machine));
    EXPECT_FAILED(goopdate_utils::CreateUserId(is_machine));
  } else {
    ASSERT_FALSE(oem_install_utils::IsOemInstalling(is_machine));
    EXPECT_SUCCEEDED(goopdate_utils::CreateUserId(is_machine));
  }
}

TEST_F(GoopdateUtilsRegistryProtectedTest,
       GenerateUserId_EachUserShouldHaveHisOwnHive) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE_DEV,
                                    kRegValueForceUsageStats,
                                    _T("1")));

  CString machine_user_id = goopdate_utils::GetUserIdLazyInit(true);
  CString user_id = goopdate_utils::GetUserIdLazyInit(false);
  EXPECT_STRNE(machine_user_id, user_id);
}

TEST_P(GoopdateUtilsRegistryProtectedBooleanTest, GetOptInUserId_UpdateDev) {
  const bool is_machine = GetParam();

  CString user_id = goopdate_utils::GetUserIdLazyInit(is_machine);
  EXPECT_TRUE(user_id.IsEmpty());

  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE_DEV,
                                    kRegValueForceUsageStats,
                                    _T("1")));

  user_id = goopdate_utils::GetUserIdLazyInit(is_machine);
  EXPECT_FALSE(user_id.IsEmpty());

  EXPECT_STREQ(user_id, goopdate_utils::GetUserIdLazyInit(is_machine));

  GUID guid = GUID_NULL;
  EXPECT_SUCCEEDED(StringToGuidSafe(user_id, &guid));
}

TEST_P(GoopdateUtilsRegistryProtectedBooleanTest, GetOptInUserId_AppOptIn) {
  const bool is_machine = GetParam();
  CString user_id = goopdate_utils::GetUserIdLazyInit(is_machine);
  EXPECT_TRUE(user_id.IsEmpty());

  CString key_path =
      ConfigManager::Instance()->registry_client_state(is_machine);
  key_path = AppendRegKeyPath(key_path, kAppId);
  EXPECT_SUCCEEDED(RegKey::SetValue(key_path,
                                    kRegValueForceUsageStats,
                                    static_cast<DWORD>(1)));

  user_id = goopdate_utils::GetUserIdLazyInit(is_machine);
  EXPECT_FALSE(user_id.IsEmpty());

  GUID guid = GUID_NULL;
  EXPECT_SUCCEEDED(StringToGuidSafe(user_id, &guid));
}

TEST_F(GoopdateUtilsRegistryProtectedTest, GetOptInUserId_UserNotOptIn) {
  const bool is_machine = false;

  EXPECT_TRUE(goopdate_utils::GetUserIdLazyInit(is_machine).IsEmpty());

  EXPECT_SUCCEEDED(goopdate_utils::CreateUserId(is_machine));
  EXPECT_TRUE(
      RegKey::HasValue(ConfigManager::Instance()->registry_update(is_machine),
                       kRegValueUserId));
  EXPECT_TRUE(goopdate_utils::GetUserIdLazyInit(is_machine).IsEmpty());

  // ID should be removed.
  EXPECT_FALSE(
      RegKey::HasValue(ConfigManager::Instance()->registry_update(is_machine),
                       kRegValueUserId));
}

TEST_P(GoopdateUtilsRegistryProtectedBooleanTest,
       GetOptInUserId_OemInstalling) {
  const bool is_machine = GetParam();
  EXPECT_SUCCEEDED(goopdate_utils::CreateUserId(is_machine));

  const DWORD now = Time64ToInt32(GetCurrent100NSTime());
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("OemInstallTime"),
                                    now));
  EXPECT_EQ(is_machine, oem_install_utils::IsOemInstalling(is_machine));

  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE_DEV,
                                    kRegValueForceUsageStats,
                                    _T("1")));

  if (is_machine) {
    EXPECT_TRUE(goopdate_utils::GetUserIdLazyInit(is_machine).IsEmpty());

    EXPECT_SUCCEEDED(RegKey::DeleteValue(MACHINE_REG_UPDATE,
                                         _T("OemInstallTime")));
    EXPECT_FALSE(goopdate_utils::GetUserIdLazyInit(is_machine).IsEmpty());
  } else {
    EXPECT_FALSE(goopdate_utils::GetUserIdLazyInit(is_machine).IsEmpty());
  }
}

INSTANTIATE_TEST_CASE_P(MachineOrUser,
                        GoopdateUtilsRegistryProtectedBooleanTest,
                        ::testing::Bool());

}  // namespace goopdate_utils

}  // namespace omaha
