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

#include "omaha/base/reg_key.h"
#include "omaha/base/system_info.h"
#include "omaha/common/app_registry_utils.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace {

#define APP_GUID _T("{B7BAF788-9D64-49c3-AFDC-B336AB12F332}")
const TCHAR* const kAppGuid = APP_GUID;
const TCHAR* const kAppMachineClientStatePath =
    _T("HKLM\\Software\\") PATH_COMPANY_NAME _T("\\") PRODUCT_NAME
    _T("\\ClientState\\") APP_GUID;
const TCHAR* const kAppUserClientStatePath =
    _T("HKCU\\Software\\") PATH_COMPANY_NAME _T("\\") PRODUCT_NAME
    _T("\\ClientState\\") APP_GUID;
const TCHAR* const kAppMachineClientStateMediumPath =
    _T("HKLM\\Software\\") PATH_COMPANY_NAME _T("\\") PRODUCT_NAME
    _T("\\ClientStateMedium\\") APP_GUID;

// This should never exist. This contant is only used to verify it is not used.
const TCHAR* const kAppUserClientStateMediumPath =
    _T("HKCU\\Software\\") PATH_COMPANY_NAME _T("\\") PRODUCT_NAME
    _T("\\ClientStateMedium\\") APP_GUID;

const TCHAR* const kOmahaMachineClientsPath =
    _T("HKLM\\Software\\") PATH_COMPANY_NAME _T("\\") PRODUCT_NAME
    _T("\\Clients\\") GOOPDATE_APP_ID;

const TCHAR* const kOmahaUserClientsPath =
    _T("HKCU\\Software\\") PATH_COMPANY_NAME _T("\\") PRODUCT_NAME
    _T("\\Clients\\") GOOPDATE_APP_ID;

const TCHAR* const kOmahaMachineClientStatePath =
    _T("HKLM\\Software\\") PATH_COMPANY_NAME _T("\\") PRODUCT_NAME
    _T("\\ClientState\\") GOOPDATE_APP_ID;

const TCHAR* const kOmahaUserClientStatePath =
    _T("HKCU\\Software\\") PATH_COMPANY_NAME _T("\\") PRODUCT_NAME
    _T("\\ClientState\\") GOOPDATE_APP_ID;

int GetFirstDayOfWeek(int day) {
  if (day < 0) {
    return day;
  }

  const int kDaysInWeek = 7;
  return day / kDaysInWeek * kDaysInWeek;
}

}  // namespace

namespace app_registry_utils {

TEST(AppRegistryUtilsTest, GetAppClientsKey) {
  const TCHAR kAppGuid1[] = _T("{F998D7E0-0CD3-434e-96B9-B8D3A295C3FB}");

  EXPECT_STREQ(_T("HKCU\\Software\\") PATH_COMPANY_NAME
               _T("\\") PRODUCT_NAME _T("\\Clients\\")
               _T("{F998D7E0-0CD3-434e-96B9-B8D3A295C3FB}"),
               GetAppClientsKey(false, kAppGuid1));
  EXPECT_STREQ(_T("HKLM\\Software\\") PATH_COMPANY_NAME
               _T("\\") PRODUCT_NAME _T("\\Clients\\")
               _T("{F998D7E0-0CD3-434e-96B9-B8D3A295C3FB}"),
               GetAppClientsKey(true, kAppGuid1));
}

TEST(AppRegistryUtilsTest, GetAppClientStateKey) {
  const TCHAR kAppGuid1[] = _T("{F998D7E0-0CD3-434e-96B9-B8D3A295C3FB}");

  EXPECT_STREQ(_T("HKCU\\Software\\") PATH_COMPANY_NAME
               _T("\\") PRODUCT_NAME _T("\\ClientState\\")
               _T("{F998D7E0-0CD3-434e-96B9-B8D3A295C3FB}"),
               GetAppClientStateKey(false, kAppGuid1));
  EXPECT_STREQ(_T("HKLM\\Software\\") PATH_COMPANY_NAME
               _T("\\") PRODUCT_NAME _T("\\ClientState\\")
               _T("{F998D7E0-0CD3-434e-96B9-B8D3A295C3FB}"),
               GetAppClientStateKey(true, kAppGuid1));
}

// This is an invalid case and causes an assert. Always returns HKLM path.
TEST(AppRegistryUtilsTest, GetAppClientStateMediumKey_User) {
  const TCHAR kAppGuid1[] = _T("{F998D7E0-0CD3-434e-96B9-B8D3A295C3FB}");
  ExpectAsserts expect_asserts;
  EXPECT_STREQ(_T("HKLM\\Software\\") PATH_COMPANY_NAME
               _T("\\") PRODUCT_NAME _T("\\ClientStateMedium\\")
               _T("{F998D7E0-0CD3-434e-96B9-B8D3A295C3FB}"),
               GetAppClientStateMediumKey(false, kAppGuid1));
}

TEST(AppRegistryUtilsTest, GetAppClientStateMediumKey_Machine) {
  const TCHAR kAppGuid1[] = _T("{F998D7E0-0CD3-434e-96B9-B8D3A295C3FB}");
  EXPECT_STREQ(_T("HKLM\\Software\\") PATH_COMPANY_NAME
               _T("\\") PRODUCT_NAME _T("\\ClientStateMedium\\")
               _T("{F998D7E0-0CD3-434e-96B9-B8D3A295C3FB}"),
               GetAppClientStateMediumKey(true, kAppGuid1));
}

// This is an invalid case and causes an assert.
TEST(AppRegistryUtilsTest, GetAppClientStateMediumKey_UserAndMachineAreSame) {
  const TCHAR kAppGuid1[] = _T("{F998D7E0-0CD3-434e-96B9-B8D3A295C3FB}");
  ExpectAsserts expect_asserts;
  EXPECT_STREQ(GetAppClientStateMediumKey(true, kAppGuid1),
               GetAppClientStateMediumKey(false, kAppGuid1));
}

class AppRegistryUtilsRegistryProtectedTest :
    public ::testing::TestWithParam<bool> {
 protected:
  AppRegistryUtilsRegistryProtectedTest()
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

  bool IsMachine() {
    return GetParam();
  }

  CString GetClientStatePath() {
    return IsMachine() ? kOmahaMachineClientStatePath :
                         kOmahaUserClientStatePath;
  }
};

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       IsAppEulaAccepted_Machine_NotExplicit_NoKey) {
  EXPECT_TRUE(IsAppEulaAccepted(true, kAppGuid, false));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       IsAppEulaAccepted_Machine_NotExplicit_ClientStateExists) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(kAppMachineClientStatePath));
  EXPECT_TRUE(IsAppEulaAccepted(true, kAppGuid, false));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       IsAppEulaAccepted_Machine_NotExplicit_ClientStateOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_TRUE(IsAppEulaAccepted(true, kAppGuid, false));
  EXPECT_EQ(1, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       IsAppEulaAccepted_Machine_NotExplicit_ClientStateNegativeOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(-1)));
  EXPECT_TRUE(IsAppEulaAccepted(true, kAppGuid, false));
  EXPECT_EQ(-1, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       IsAppEulaAccepted_Machine_NotExplicit_ClientStateZero_MediumNotExist) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_FALSE(IsAppEulaAccepted(true, kAppGuid, false));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       IsAppEulaAccepted_Machine_NotExplicit_ClientStateZero_MediumExists) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::CreateKey(kAppMachineClientStateMediumPath));
  EXPECT_FALSE(IsAppEulaAccepted(true, kAppGuid, false));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       IsAppEulaAccepted_Machine_NotExplicit_ClientStateZero_MediumOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_TRUE(IsAppEulaAccepted(true, kAppGuid, false));
  EXPECT_EQ(1, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(1, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
    IsAppEulaAccepted_Machine_NotExplicit_ClientStateZero_MediumNegativeOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(-1)));
  EXPECT_TRUE(IsAppEulaAccepted(true, kAppGuid, false));
  EXPECT_EQ(-1, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(-1, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("eulaaccepted")));
}

// Also tests that user values are not used.
TEST_F(AppRegistryUtilsRegistryProtectedTest,
       IsAppEulaAccepted_Machine_NotExplicit_ClientStateZero_MediumZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_FALSE(IsAppEulaAccepted(true, kAppGuid, false));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("eulaaccepted")));
  EXPECT_EQ(1, GetDwordValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(1, GetDwordValue(kAppUserClientStateMediumPath,
                             _T("eulaaccepted")));
}

// ClientStateMedium does not override ClientState.
TEST_F(AppRegistryUtilsRegistryProtectedTest,
       IsAppEulaAccepted_Machine_NotExplicit_ClientStateOne_MediumZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_TRUE(IsAppEulaAccepted(true, kAppGuid, false));
  EXPECT_EQ(1, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("eulaaccepted")));
}

// Implicitly accepted because of the absence of eualaccepted=0.
TEST_F(AppRegistryUtilsRegistryProtectedTest,
       IsAppEulaAccepted_Machine_NotExplicit_ClientStateNotExist_MediumOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_TRUE(IsAppEulaAccepted(true, kAppGuid, false));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(1, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("eulaaccepted")));
}

// Implicitly accepted because of the absence of eualaccepted=0.
TEST_F(AppRegistryUtilsRegistryProtectedTest,
       IsAppEulaAccepted_Machine_NotExplicit_ClientStateNotExist_MediumZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_TRUE(IsAppEulaAccepted(true, kAppGuid, false));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       IsAppEulaAccepted_Machine_Explicit_NoKey) {
  EXPECT_FALSE(IsAppEulaAccepted(true, kAppGuid, true));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       IsAppEulaAccepted_Machine_Explicit_ClientStateExists) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(kAppMachineClientStatePath));
  EXPECT_FALSE(IsAppEulaAccepted(true, kAppGuid, true));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       IsAppEulaAccepted_Machine_Explicit_ClientStateOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_TRUE(IsAppEulaAccepted(true, kAppGuid, true));
  EXPECT_EQ(1, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       IsAppEulaAccepted_Machine_Explicit_ClientStateNegativeOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(-1)));
  EXPECT_TRUE(IsAppEulaAccepted(true, kAppGuid, true));
  EXPECT_EQ(-1, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       IsAppEulaAccepted_Machine_Explicit_ClientStateZero_MediumNotExist) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_FALSE(IsAppEulaAccepted(true, kAppGuid, true));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       IsAppEulaAccepted_Machine_Explicit_ClientStateZero_MediumExists) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::CreateKey(kAppMachineClientStateMediumPath));
  EXPECT_FALSE(IsAppEulaAccepted(true, kAppGuid, true));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       IsAppEulaAccepted_Machine_Explicit_ClientStateZero_MediumOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_TRUE(IsAppEulaAccepted(true, kAppGuid, true));
  EXPECT_EQ(1, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(1, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       IsAppEulaAccepted_Machine_Explicit_ClientStateZero_MediumNegativeOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(-1)));
  EXPECT_TRUE(IsAppEulaAccepted(true, kAppGuid, true));
  EXPECT_EQ(-1, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(-1, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("eulaaccepted")));
}

// Also tests that user values are not used.
TEST_F(AppRegistryUtilsRegistryProtectedTest,
       IsAppEulaAccepted_Machine_Explicit_ClientStateZero_MediumZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_FALSE(IsAppEulaAccepted(true, kAppGuid, true));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("eulaaccepted")));
  EXPECT_EQ(1, GetDwordValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(1, GetDwordValue(kAppUserClientStateMediumPath,
                             _T("eulaaccepted")));
}

// ClientStateMedium does not override ClientState.
TEST_F(AppRegistryUtilsRegistryProtectedTest,
       IsAppEulaAccepted_Machine_Explicit_ClientStateOne_MediumZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_TRUE(IsAppEulaAccepted(true, kAppGuid, true));
  EXPECT_EQ(1, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       IsAppEulaAccepted_Machine_Explicit_ClientStateNotExist_MediumOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_TRUE(IsAppEulaAccepted(true, kAppGuid, true));
  EXPECT_EQ(1, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(1, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       IsAppEulaAccepted_Machine_Explicit_ClientStateNotExist_MediumZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_FALSE(IsAppEulaAccepted(true, kAppGuid, true));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("eulaaccepted")));
}

// ClientStateMedium is not supported for user apps.

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       IsAppEulaAccepted_User_NotExplicit_NoKey) {
  EXPECT_TRUE(IsAppEulaAccepted(false, kAppGuid, false));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       IsAppEulaAccepted_User_NotExplicit_ClientStateExists) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(kAppUserClientStatePath));
  EXPECT_TRUE(IsAppEulaAccepted(false, kAppGuid, false));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       IsAppEulaAccepted_User_NotExplicit_ClientStateOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_TRUE(IsAppEulaAccepted(false, kAppGuid, false));
  EXPECT_EQ(1, GetDwordValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       IsAppEulaAccepted_User_NotExplicit_ClientStateNegativeOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(-1)));
  EXPECT_TRUE(IsAppEulaAccepted(false, kAppGuid, false));
  EXPECT_EQ(-1, GetDwordValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       IsAppEulaAccepted_User_NotExplicit_ClientStateZero_MediumNotExist) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_FALSE(IsAppEulaAccepted(false, kAppGuid, false));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       IsAppEulaAccepted_User_NotExplicit_ClientStateZero_MediumExists) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::CreateKey(kAppUserClientStateMediumPath));
  EXPECT_FALSE(IsAppEulaAccepted(false, kAppGuid, false));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       IsAppEulaAccepted_User_NotExplicit_ClientStateZero_MediumOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_FALSE(IsAppEulaAccepted(false, kAppGuid, false));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(1, GetDwordValue(kAppUserClientStateMediumPath,
                             _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       IsAppEulaAccepted_User_NotExplicit_ClientStateZero_MediumNegativeOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(-1)));
  EXPECT_FALSE(IsAppEulaAccepted(false, kAppGuid, false));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(-1, GetDwordValue(kAppUserClientStateMediumPath,
                             _T("eulaaccepted")));
}

// Also tests that machine values are not used.
TEST_F(AppRegistryUtilsRegistryProtectedTest,
       IsAppEulaAccepted_User_NotExplicit_ClientStateZero_MediumZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_FALSE(IsAppEulaAccepted(false, kAppGuid, false));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStateMediumPath,
                             _T("eulaaccepted")));
  EXPECT_EQ(1, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(1, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("eulaaccepted")));
}

// ClientStateMedium is not used.
TEST_F(AppRegistryUtilsRegistryProtectedTest,
       IsAppEulaAccepted_User_NotExplicit_ClientStateOne_MediumZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_TRUE(IsAppEulaAccepted(false, kAppGuid, false));
  EXPECT_EQ(1, GetDwordValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStateMediumPath,
                             _T("eulaaccepted")));
}

// Implicitly accepted because of the absence of eualaccepted=0.
TEST_F(AppRegistryUtilsRegistryProtectedTest,
       IsAppEulaAccepted_User_NotExplicit_ClientStateNotExist_MediumOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_TRUE(IsAppEulaAccepted(false, kAppGuid, false));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(1, GetDwordValue(kAppUserClientStateMediumPath,
                             _T("eulaaccepted")));
}

// Implicitly accepted because of the absence of eualaccepted=0.
TEST_F(AppRegistryUtilsRegistryProtectedTest,
       IsAppEulaAccepted_User_NotExplicit_ClientStateNotExist_MediumZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_TRUE(IsAppEulaAccepted(false, kAppGuid, false));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStateMediumPath,
                             _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       IsAppEulaAccepted_User_Explicit_NoKey) {
  EXPECT_FALSE(IsAppEulaAccepted(false, kAppGuid, true));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       IsAppEulaAccepted_User_Explicit_ClientStateExists) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(kAppUserClientStatePath));
  EXPECT_FALSE(IsAppEulaAccepted(false, kAppGuid, true));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       IsAppEulaAccepted_User_Explicit_ClientStateOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_TRUE(IsAppEulaAccepted(false, kAppGuid, true));
  EXPECT_EQ(1, GetDwordValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       IsAppEulaAccepted_User_Explicit_ClientStateNotExist_MediumOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_FALSE(IsAppEulaAccepted(false, kAppGuid, true));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(1, GetDwordValue(kAppUserClientStateMediumPath,
                             _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       IsAppEulaAccepted_User_Explicit_ClientStateNotExist_MediumZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_FALSE(IsAppEulaAccepted(false, kAppGuid, true));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStateMediumPath,
                             _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       SetAppEulaNotAccepted_Machine_NoKey) {
  EXPECT_SUCCEEDED(SetAppEulaNotAccepted(true, kAppGuid));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       SetAppEulaNotAccepted_Machine_ClientStateExists) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(kAppMachineClientStatePath));
  EXPECT_SUCCEEDED(SetAppEulaNotAccepted(true, kAppGuid));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       SetAppEulaNotAccepted_Machine_ClientStateOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(SetAppEulaNotAccepted(true, kAppGuid));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       SetAppEulaNotAccepted_Machine_ClientStateZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(SetAppEulaNotAccepted(true, kAppGuid));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       SetAppEulaNotAccepted_Machine_ClientStateZero_ClientStateMediumOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(SetAppEulaNotAccepted(true, kAppGuid));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(1, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("eulaaccepted")));
}

// Also tests that user values are not affected.
TEST_F(AppRegistryUtilsRegistryProtectedTest,
       SetAppEulaNotAccepted_Machine_ClientStateZero_ClientStateMediumZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(SetAppEulaNotAccepted(true, kAppGuid));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("eulaaccepted")));
  EXPECT_EQ(1, GetDwordValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(1, GetDwordValue(kAppUserClientStateMediumPath,
                             _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       SetAppEulaNotAccepted_User_NoKey) {
  EXPECT_SUCCEEDED(SetAppEulaNotAccepted(false, kAppGuid));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       SetAppEulaNotAccepted_User_ClientStateExists) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(kAppUserClientStatePath));
  EXPECT_SUCCEEDED(SetAppEulaNotAccepted(false, kAppGuid));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       SetAppEulaNotAccepted_User_ClientStateOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(SetAppEulaNotAccepted(false, kAppGuid));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       SetAppEulaNotAccepted_User_ClientStateZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(SetAppEulaNotAccepted(false, kAppGuid));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       SetAppEulaNotAccepted_User_ClientStateZero_ClientStateMediumOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(SetAppEulaNotAccepted(false, kAppGuid));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(1, GetDwordValue(kAppUserClientStateMediumPath,
                             _T("eulaaccepted")));
}

// Also tests that machine values are not affected.
TEST_F(AppRegistryUtilsRegistryProtectedTest,
       SetAppEulaNotAccepted_User_ClientStateZero_ClientStateMediumZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(SetAppEulaNotAccepted(false, kAppGuid));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStateMediumPath,
                             _T("eulaaccepted")));
  EXPECT_EQ(1, GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(1, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       ClearAppEulaNotAccepted_Machine_NoKey) {
  EXPECT_SUCCEEDED(ClearAppEulaNotAccepted(true, kAppGuid));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       ClearAppEulaNotAccepted_Machine_ClientStateExists) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(kAppMachineClientStatePath));
  EXPECT_SUCCEEDED(ClearAppEulaNotAccepted(true, kAppGuid));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       ClearAppEulaNotAccepted_Machine_ClientStateOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(ClearAppEulaNotAccepted(true, kAppGuid));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       ClearAppEulaNotAccepted_Machine_ClientStateZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(ClearAppEulaNotAccepted(true, kAppGuid));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       ClearAppEulaNotAccepted_Machine_ClientStateZero_ClientStateMediumZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(ClearAppEulaNotAccepted(true, kAppGuid));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("eulaaccepted")));
}

// Also tests that user values are not affected.
TEST_F(AppRegistryUtilsRegistryProtectedTest,
       ClearAppEulaNotAccepted_Machine_ClientStateNone_ClientStateMediumZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(ClearAppEulaNotAccepted(true, kAppGuid));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("eulaaccepted")));
  EXPECT_EQ(0,
            GetDwordValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(0,
            GetDwordValue(kAppUserClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       ClearAppEulaNotAccepted_User_NoKey) {
  EXPECT_SUCCEEDED(ClearAppEulaNotAccepted(false, kAppGuid));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       ClearAppEulaNotAccepted_User_ClientStateExists) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(kAppUserClientStatePath));
  EXPECT_SUCCEEDED(ClearAppEulaNotAccepted(false, kAppGuid));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       ClearAppEulaNotAccepted_User_ClientStateOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(ClearAppEulaNotAccepted(false, kAppGuid));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       ClearAppEulaNotAccepted_User_ClientStateZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(ClearAppEulaNotAccepted(false, kAppGuid));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       ClearAppEulaNotAccepted_User_ClientStateZero_ClientStateMediumZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(ClearAppEulaNotAccepted(false, kAppGuid));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(0,
            GetDwordValue(kAppUserClientStateMediumPath, _T("eulaaccepted")));
}

// Also tests that machine values are not affected.
TEST_F(AppRegistryUtilsRegistryProtectedTest,
       ClearAppEulaNotAccepted_User_ClientStateNone_ClientStateMediumZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(ClearAppEulaNotAccepted(false, kAppGuid));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(0,
            GetDwordValue(kAppUserClientStateMediumPath, _T("eulaaccepted")));
  EXPECT_EQ(0,
            GetDwordValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("eulaaccepted")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_Machine_NoKey) {
  EXPECT_FALSE(AreAppUsageStatsEnabled(true, kAppGuid));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStatePath, _T("usagestats")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("usagestats")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_Machine_ClientStateExists) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(kAppMachineClientStatePath));
  EXPECT_FALSE(AreAppUsageStatsEnabled(true, kAppGuid));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStatePath, _T("usagestats")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("usagestats")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_Machine_ClientStateOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(1)));
  EXPECT_TRUE(AreAppUsageStatsEnabled(true, kAppGuid));
  EXPECT_EQ(1, GetDwordValue(kAppMachineClientStatePath, _T("usagestats")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("usagestats")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_Machine_ClientStateNegativeOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(-1)));
  EXPECT_FALSE(AreAppUsageStatsEnabled(true, kAppGuid));
  EXPECT_EQ(-1, GetDwordValue(kAppMachineClientStatePath, _T("usagestats")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("usagestats")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_Machine_ClientStateZero_MediumNotExist) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(0)));
  EXPECT_FALSE(AreAppUsageStatsEnabled(true, kAppGuid));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStatePath, _T("usagestats")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("usagestats")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_Machine_ClientStateZero_MediumExists) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::CreateKey(kAppMachineClientStateMediumPath));
  EXPECT_FALSE(AreAppUsageStatsEnabled(true, kAppGuid));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStatePath, _T("usagestats")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("usagestats")));
}

// ClientStateMedium overrides ClientState.
TEST_F(AppRegistryUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_Machine_ClientStateZero_MediumOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(1)));
  EXPECT_TRUE(AreAppUsageStatsEnabled(true, kAppGuid));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStatePath, _T("usagestats")));
  EXPECT_EQ(1, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("usagestats")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
    AreAppUsageStatsEnabled_Machine_ClientStateZero_MediumNegativeOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(-1)));
  EXPECT_FALSE(AreAppUsageStatsEnabled(true, kAppGuid));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStatePath, _T("usagestats")));
  EXPECT_EQ(-1, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("usagestats")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_Machine_ClientStateZero_MediumZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(0)));
  EXPECT_FALSE(AreAppUsageStatsEnabled(true, kAppGuid));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStatePath, _T("usagestats")));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("usagestats")));
}

// ClientStateMedium overrides ClientState.
TEST_F(AppRegistryUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_Machine_ClientStateOne_MediumZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(0)));
  EXPECT_FALSE(AreAppUsageStatsEnabled(true, kAppGuid));
  EXPECT_EQ(1, GetDwordValue(kAppMachineClientStatePath, _T("usagestats")));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("usagestats")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_Machine_ClientStateNotExist_MediumOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(1)));
  EXPECT_TRUE(AreAppUsageStatsEnabled(true, kAppGuid));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStatePath, _T("usagestats")));
  EXPECT_EQ(1, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("usagestats")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_Machine_ClientStateNotExist_MediumZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(0)));
  EXPECT_FALSE(AreAppUsageStatsEnabled(true, kAppGuid));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStatePath, _T("usagestats")));
  EXPECT_EQ(0, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("usagestats")));
}

// User does not affect machine.
TEST_F(AppRegistryUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_Machine_UserOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(1)));
  EXPECT_FALSE(AreAppUsageStatsEnabled(true, kAppGuid));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStatePath, _T("usagestats")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("usagestats")));
  EXPECT_EQ(1, GetDwordValue(kAppUserClientStatePath, _T("usagestats")));
  EXPECT_EQ(1, GetDwordValue(kAppUserClientStateMediumPath,
                             _T("usagestats")));
}

// ClientStateMedium is not supported for user apps.

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_User_NoKey) {
  EXPECT_FALSE(AreAppUsageStatsEnabled(false, kAppGuid));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStatePath, _T("usagestats")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("usagestats")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_User_ClientStateExists) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(kAppUserClientStatePath));
  EXPECT_FALSE(AreAppUsageStatsEnabled(false, kAppGuid));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStatePath, _T("usagestats")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("usagestats")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_User_ClientStateOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(1)));
  EXPECT_TRUE(AreAppUsageStatsEnabled(false, kAppGuid));
  EXPECT_EQ(1, GetDwordValue(kAppUserClientStatePath, _T("usagestats")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("usagestats")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_User_ClientStateNegativeOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(-1)));
  EXPECT_FALSE(AreAppUsageStatsEnabled(false, kAppGuid));
  EXPECT_EQ(-1, GetDwordValue(kAppUserClientStatePath, _T("usagestats")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("usagestats")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_User_ClientStateZero_MediumNotExist) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(0)));
  EXPECT_FALSE(AreAppUsageStatsEnabled(false, kAppGuid));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStatePath, _T("usagestats")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("usagestats")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_User_ClientStateZero_MediumExists) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::CreateKey(kAppUserClientStateMediumPath));
  EXPECT_FALSE(AreAppUsageStatsEnabled(false, kAppGuid));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStatePath, _T("usagestats")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("usagestats")));
}

// ClientStateMedium is not used.
TEST_F(AppRegistryUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_User_ClientStateZero_MediumOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(1)));
  EXPECT_FALSE(AreAppUsageStatsEnabled(false, kAppGuid));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStatePath, _T("usagestats")));
  EXPECT_EQ(1, GetDwordValue(kAppUserClientStateMediumPath,
                             _T("usagestats")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_User_ClientStateZero_MediumNegativeOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(-1)));
  EXPECT_FALSE(AreAppUsageStatsEnabled(false, kAppGuid));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStatePath, _T("usagestats")));
  EXPECT_EQ(-1, GetDwordValue(kAppUserClientStateMediumPath,
                             _T("usagestats")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_User_ClientStateZero_MediumZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(0)));
  EXPECT_FALSE(AreAppUsageStatsEnabled(false, kAppGuid));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStatePath, _T("usagestats")));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStateMediumPath,
                             _T("usagestats")));
}

// ClientStateMedium is not used.
TEST_F(AppRegistryUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_User_ClientStateOne_MediumZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(0)));
  EXPECT_TRUE(AreAppUsageStatsEnabled(false, kAppGuid));
  EXPECT_EQ(1, GetDwordValue(kAppUserClientStatePath, _T("usagestats")));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStateMediumPath,
                             _T("usagestats")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_User_ClientStateNotExist_MediumOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(1)));
  EXPECT_FALSE(AreAppUsageStatsEnabled(false, kAppGuid));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStatePath, _T("usagestats")));
  EXPECT_EQ(1, GetDwordValue(kAppUserClientStateMediumPath,
                             _T("usagestats")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_User_ClientStateNotExist_MediumZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(0)));
  EXPECT_FALSE(AreAppUsageStatsEnabled(false, kAppGuid));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStatePath, _T("usagestats")));
  EXPECT_EQ(0, GetDwordValue(kAppUserClientStateMediumPath,
                             _T("usagestats")));
}

// Machine does not affect user.
TEST_F(AppRegistryUtilsRegistryProtectedTest,
       AreAppUsageStatsEnabled_User_MachineOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(1)));
  EXPECT_FALSE(AreAppUsageStatsEnabled(false, kAppGuid));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStatePath, _T("usagestats")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStateMediumPath, _T("usagestats")));
  EXPECT_EQ(1, GetDwordValue(kAppMachineClientStatePath, _T("usagestats")));
  EXPECT_EQ(1, GetDwordValue(kAppMachineClientStateMediumPath,
                             _T("usagestats")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest, SetUsageStatsEnable_Machine_Off) {
  EXPECT_SUCCEEDED(SetUsageStatsEnable(true, kAppGuid, TRISTATE_FALSE));

  ASSERT_TRUE(RegKey::HasKey(kAppMachineClientStatePath));
  ASSERT_TRUE(RegKey::HasValue(kAppMachineClientStatePath,
                               _T("usagestats")));
  ASSERT_FALSE(RegKey::HasKey(kAppUserClientStatePath));

  DWORD enable_value = 1;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    _T("usagestats"),
                                    &enable_value));
  EXPECT_EQ(0, enable_value);
}

TEST_F(AppRegistryUtilsRegistryProtectedTest, SetUsageStatsEnable_User_Off) {
  EXPECT_SUCCEEDED(SetUsageStatsEnable(false, kAppGuid, TRISTATE_FALSE));

  ASSERT_TRUE(RegKey::HasKey(kAppUserClientStatePath));
  ASSERT_TRUE(RegKey::HasValue(kAppUserClientStatePath,
                               _T("usagestats")));
  ASSERT_FALSE(RegKey::HasKey(kAppMachineClientStatePath));

  DWORD enable_value = 1;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppUserClientStatePath,
                                    _T("usagestats"),
                                    &enable_value));
  EXPECT_EQ(0, enable_value);
}

TEST_F(AppRegistryUtilsRegistryProtectedTest, SetUsageStatsEnable_Machine_On) {
  EXPECT_SUCCEEDED(SetUsageStatsEnable(true, kAppGuid, TRISTATE_TRUE));

  ASSERT_TRUE(RegKey::HasKey(kAppMachineClientStatePath));
  ASSERT_TRUE(RegKey::HasValue(kAppMachineClientStatePath,
                               _T("usagestats")));
  ASSERT_FALSE(RegKey::HasKey(kAppUserClientStatePath));

  DWORD enable_value = 0;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    _T("usagestats"),
                                    &enable_value));
  EXPECT_EQ(1, enable_value);
}

TEST_F(AppRegistryUtilsRegistryProtectedTest, SetUsageStatsEnable_User_On) {
  EXPECT_SUCCEEDED(SetUsageStatsEnable(false, kAppGuid, TRISTATE_TRUE));

  ASSERT_TRUE(RegKey::HasKey(kAppUserClientStatePath));
  ASSERT_TRUE(RegKey::HasValue(kAppUserClientStatePath,
                               _T("usagestats")));
  ASSERT_FALSE(RegKey::HasKey(kAppMachineClientStatePath));

  DWORD enable_value = 0;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppUserClientStatePath,
                                    _T("usagestats"),
                                    &enable_value));
  EXPECT_EQ(1, enable_value);
}


TEST_F(AppRegistryUtilsRegistryProtectedTest,
       SetUsageStatsEnable_Machine_None) {
  EXPECT_SUCCEEDED(SetUsageStatsEnable(true, kAppGuid, TRISTATE_NONE));
  ASSERT_FALSE(RegKey::HasKey(MACHINE_REG_UPDATE));
  ASSERT_FALSE(RegKey::HasKey(USER_REG_UPDATE));

  EXPECT_SUCCEEDED(SetUsageStatsEnable(false, kAppGuid, TRISTATE_NONE));
  ASSERT_FALSE(RegKey::HasKey(USER_REG_UPDATE));
  ASSERT_FALSE(RegKey::HasKey(MACHINE_REG_UPDATE));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       SetUsageStatsEnable_Machine_Overwrite) {
  EXPECT_SUCCEEDED(SetUsageStatsEnable(true, kAppGuid, TRISTATE_FALSE));

  ASSERT_TRUE(RegKey::HasKey(kAppMachineClientStatePath));
  ASSERT_TRUE(RegKey::HasValue(kAppMachineClientStatePath,
                               _T("usagestats")));
  ASSERT_FALSE(RegKey::HasKey(kAppUserClientStatePath));

  DWORD enable_value = 1;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    _T("usagestats"),
                                    &enable_value));
  EXPECT_EQ(0, enable_value);

  EXPECT_SUCCEEDED(SetUsageStatsEnable(true, kAppGuid, TRISTATE_TRUE));

  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    _T("usagestats"),
                                    &enable_value));
  EXPECT_EQ(1, enable_value);

  EXPECT_SUCCEEDED(SetUsageStatsEnable(true, kAppGuid, TRISTATE_FALSE));

  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    _T("usagestats"),
                                    &enable_value));
  EXPECT_EQ(0, enable_value);
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       SetUsageStatsEnable_Machine_NoneDoesNotOverwrite) {
  EXPECT_SUCCEEDED(SetUsageStatsEnable(true, kAppGuid, TRISTATE_FALSE));

  ASSERT_TRUE(RegKey::HasKey(kAppMachineClientStatePath));
  ASSERT_TRUE(RegKey::HasValue(kAppMachineClientStatePath,
                               _T("usagestats")));
  ASSERT_FALSE(RegKey::HasKey(kAppUserClientStatePath));

  DWORD enable_value = 1;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    _T("usagestats"),
                                    &enable_value));
  EXPECT_EQ(0, enable_value);

  EXPECT_SUCCEEDED(SetUsageStatsEnable(true, kAppGuid, TRISTATE_NONE));

  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    _T("usagestats"),
                                    &enable_value));
  EXPECT_EQ(0, enable_value);

  EXPECT_SUCCEEDED(SetUsageStatsEnable(true, kAppGuid, TRISTATE_TRUE));

  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    _T("usagestats"),
                                    &enable_value));
  EXPECT_EQ(1, enable_value);

  EXPECT_SUCCEEDED(SetUsageStatsEnable(true, kAppGuid, TRISTATE_NONE));

  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    _T("usagestats"),
                                    &enable_value));
  EXPECT_EQ(1, enable_value);
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       SetUsageStatsEnable_Machine_ClientStateMediumCleared) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(0)));

  EXPECT_SUCCEEDED(SetUsageStatsEnable(true, kAppGuid, TRISTATE_TRUE));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("usagestats")));

  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(SetUsageStatsEnable(true, kAppGuid, TRISTATE_FALSE));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStateMediumPath, _T("usagestats")));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       SetUsageStatsEnable_Machine_NoneDoesNotClearClientStateMedium) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(1)));

  EXPECT_SUCCEEDED(SetUsageStatsEnable(true, kAppGuid, TRISTATE_NONE));

  DWORD enable_value = 0;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStateMediumPath,
                                    _T("usagestats"),
                                    &enable_value));
  EXPECT_EQ(1, enable_value);
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       SetUsageStatsEnable_User_ClientStateMediumNotCleared) {
  // User and machine values should not be cleared.
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(0)));

  // True does not clear them.
  EXPECT_SUCCEEDED(SetUsageStatsEnable(false, kAppGuid, TRISTATE_TRUE));
  DWORD enable_value = 1;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppUserClientStateMediumPath,
                                    _T("usagestats"),
                                    &enable_value));
  EXPECT_EQ(0, enable_value);
  enable_value = 1;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStateMediumPath,
                                    _T("usagestats"),
                                    &enable_value));
  EXPECT_EQ(0, enable_value);

  // False does not clear them.
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("usagestats"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(SetUsageStatsEnable(false, kAppGuid, TRISTATE_FALSE));
  enable_value = 0;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppUserClientStateMediumPath,
                                    _T("usagestats"),
                                    &enable_value));
  EXPECT_EQ(1, enable_value);
  enable_value = 0;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStateMediumPath,
                                    _T("usagestats"),
                                    &enable_value));
  EXPECT_EQ(1, enable_value);
}

TEST_F(AppRegistryUtilsRegistryProtectedTest, GetNumClients) {
  size_t num_clients(0);

  // Fails when no "Clients" key.
  EXPECT_HRESULT_FAILED(GetNumClients(true, &num_clients));
  EXPECT_HRESULT_FAILED(GetNumClients(false, &num_clients));

  // Tests no subkeys.
  const TCHAR* keys_to_create[] = { MACHINE_REG_CLIENTS, USER_REG_CLIENTS };
  EXPECT_HRESULT_SUCCEEDED(RegKey::CreateKeys(keys_to_create,
                                              arraysize(keys_to_create)));
  EXPECT_HRESULT_SUCCEEDED(GetNumClients(true, &num_clients));
  EXPECT_EQ(0, num_clients);
  EXPECT_HRESULT_SUCCEEDED(GetNumClients(false, &num_clients));
  EXPECT_EQ(0, num_clients);

  // Subkeys should be counted. Values should not be counted.
  RegKey machine_key;
  EXPECT_HRESULT_SUCCEEDED(machine_key.Open(HKEY_LOCAL_MACHINE,
                                            GOOPDATE_REG_RELATIVE_CLIENTS));
  EXPECT_HRESULT_SUCCEEDED(machine_key.SetValue(_T("name"), _T("value")));
  EXPECT_HRESULT_SUCCEEDED(GetNumClients(true, &num_clients));
  EXPECT_EQ(0, num_clients);

  const TCHAR* app_id = _T("{AA5523E3-40C0-4b85-B074-4BBA09559CCD}");
  EXPECT_HRESULT_SUCCEEDED(machine_key.Create(machine_key.Key(), app_id));
  EXPECT_HRESULT_SUCCEEDED(GetNumClients(true, &num_clients));
  EXPECT_EQ(1, num_clients);

  // Tests user scenario.
  RegKey user_key;
  EXPECT_HRESULT_SUCCEEDED(user_key.Open(HKEY_CURRENT_USER,
                                         GOOPDATE_REG_RELATIVE_CLIENTS));
  EXPECT_HRESULT_SUCCEEDED(user_key.SetValue(_T("name"), _T("value")));
  EXPECT_HRESULT_SUCCEEDED(GetNumClients(false, &num_clients));
  EXPECT_EQ(0, num_clients);

  EXPECT_HRESULT_SUCCEEDED(user_key.Create(user_key.Key(), app_id));
  EXPECT_HRESULT_SUCCEEDED(GetNumClients(false, &num_clients));
  EXPECT_EQ(1, num_clients);
}

// This test verifies that InstallTime is created afresh for Omaha if it does
// not exist, and even if the brand code is already set.
TEST_F(AppRegistryUtilsRegistryProtectedTest,
       SetGoogleUpdateBranding_BrandAlreadyExistsAllEmpty) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    _T("EFGH")));

  EXPECT_SUCCEEDED(SetGoogleUpdateBranding(kAppMachineClientStatePath,
                                           _T(""),
                                           _T("")));

  const uint32 now = Time64ToInt32(GetCurrent100NSTime());

  CString value;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    &value));
  EXPECT_STREQ(_T("EFGH"), value);
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueClientId,
                             &value));
  DWORD install_time = 0;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueInstallTimeSec,
                                    &install_time));
  EXPECT_GE(now, install_time);
  EXPECT_GE(static_cast<uint32>(200), now - install_time);

  DWORD day_of_install(0);
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueDayOfInstall,
                             &day_of_install));
}

// This test verifies that InstallTime remains unchanged for Omaha if it already
// exists and the brand code is already set.
TEST_F(AppRegistryUtilsRegistryProtectedTest,
       SetGoogleUpdateBranding_AllAlreadyExistAllEmpty) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    _T("EFGH")));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueClientId,
                                    _T("existing_partner")));
  const DWORD kInstallTime = 1234567890;
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueInstallTimeSec,
                                    kInstallTime));

  const DWORD kDayOfInstall = 3344;
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueDayOfInstall,
                                    kDayOfInstall));

  EXPECT_SUCCEEDED(SetGoogleUpdateBranding(kAppMachineClientStatePath,
                                           _T(""),
                                           _T("")));

  CString value;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    &value));
  EXPECT_STREQ(_T("EFGH"), value);
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueClientId,
                                    &value));
  EXPECT_STREQ(_T("existing_partner"), value);
  EXPECT_EQ(kInstallTime,
            GetDwordValue(kAppMachineClientStatePath, kRegValueInstallTimeSec));
  EXPECT_EQ(kDayOfInstall,
            GetDwordValue(kAppMachineClientStatePath, kRegValueDayOfInstall));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest, SetAppBranding_KeyDoesNotExist) {
  EXPECT_SUCCEEDED(SetAppBranding(kAppMachineClientStatePath,
                                  _T("ABCD"),
                                  _T("some_partner"),
                                  _T("referrer"),
                                  -1));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest, SetAppBranding_AllEmpty) {
  ASSERT_SUCCEEDED(RegKey::CreateKey(kAppMachineClientStatePath));

  EXPECT_SUCCEEDED(SetAppBranding(kAppMachineClientStatePath,
                                  _T(""),
                                  _T(""),
                                  _T(""),
                                  -1));
  const uint32 now = Time64ToInt32(GetCurrent100NSTime());

  CString value;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    &value));
  EXPECT_STREQ(_T("GGLS"), value);
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueClientId,
                             &value));
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueReferralId,
                             &value));
  DWORD install_time = 0;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueInstallTimeSec,
                                    &install_time));
  EXPECT_GE(now, install_time);
  EXPECT_GE(static_cast<uint32>(200), now - install_time);

  DWORD day_of_install(0);
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueDayOfInstall,
                                    &day_of_install));
  EXPECT_EQ(static_cast<DWORD>(-1), day_of_install);
}

TEST_F(AppRegistryUtilsRegistryProtectedTest, SetAppBranding_BrandCodeOnly) {
  ASSERT_SUCCEEDED(RegKey::CreateKey(kAppMachineClientStatePath));

  EXPECT_SUCCEEDED(SetAppBranding(kAppMachineClientStatePath,
                                  _T("ABCD"),
                                  _T(""),
                                  _T(""),
                                  -1));
  const uint32 now = Time64ToInt32(GetCurrent100NSTime());

  CString value;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    &value));
  EXPECT_STREQ(_T("ABCD"), value);
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueClientId,
                             &value));
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueReferralId,
                             &value));
  DWORD install_time = 0;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueInstallTimeSec,
                                    &install_time));
  EXPECT_GE(now, install_time);
  EXPECT_GE(static_cast<uint32>(200), now - install_time);

  DWORD day_of_install(0);
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueDayOfInstall,
                                    &day_of_install));
  EXPECT_EQ(static_cast<DWORD>(-1), day_of_install);
}

TEST_F(AppRegistryUtilsRegistryProtectedTest, SetAppBranding_BrandCodeTooLong) {
  EXPECT_EQ(E_INVALIDARG, SetAppBranding(kAppMachineClientStatePath,
                                         _T("CHMGon.href)}"),
                                         _T(""),
                                         _T(""),
                                         -1));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest, SetAppBranding_ClientIdOnly) {
  ASSERT_SUCCEEDED(RegKey::CreateKey(kAppMachineClientStatePath));

  EXPECT_SUCCEEDED(SetAppBranding(kAppMachineClientStatePath,
                                  _T(""),
                                  _T("some_partner"),
                                  _T(""),
                                  -1));
  const uint32 now = Time64ToInt32(GetCurrent100NSTime());

  CString value;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    &value));
  EXPECT_STREQ(_T("GGLS"), value);
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueClientId,
                                    &value));
  EXPECT_STREQ(_T("some_partner"), value);
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueReferralId,
                             &value));
  DWORD install_time = 0;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueInstallTimeSec,
                                    &install_time));
  EXPECT_GE(now, install_time);
  EXPECT_GE(static_cast<uint32>(200), now - install_time);

  DWORD day_of_install(0);
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueDayOfInstall,
                                    &day_of_install));
  EXPECT_EQ(static_cast<DWORD>(-1), day_of_install);
}

TEST_F(AppRegistryUtilsRegistryProtectedTest, SetAppBranding_AllValid) {
  const int kExpectedDayOfInstall = 2555;

  ASSERT_SUCCEEDED(RegKey::CreateKey(kAppMachineClientStatePath));
  EXPECT_SUCCEEDED(SetAppBranding(kAppMachineClientStatePath,
                                  _T("ABCD"),
                                  _T("some_partner"),
                                  _T("referrer"),
                                  kExpectedDayOfInstall));
  const uint32 now = Time64ToInt32(GetCurrent100NSTime());

  CString value;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    &value));
  EXPECT_STREQ(_T("ABCD"), value);
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueClientId,
                                    &value));
  EXPECT_STREQ(_T("some_partner"), value);
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueReferralId,
                                    &value));
  EXPECT_STREQ(_T("referrer"), value);
  DWORD install_time(0);
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueInstallTimeSec,
                                    &install_time));
  EXPECT_GE(now, install_time);
  EXPECT_GE(static_cast<uint32>(200), now - install_time);

  DWORD day_of_install(0);
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueDayOfInstall,
                                    &day_of_install));
  EXPECT_EQ(kExpectedDayOfInstall, day_of_install);
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       SetAppBranding_BrandAlreadyExistsAllEmpty) {
  ASSERT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    _T("EFGH")));

  EXPECT_SUCCEEDED(SetAppBranding(kAppMachineClientStatePath,
                                  _T(""),
                                  _T(""),
                                  _T(""),
                                  -1));

  CString value;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    &value));
  EXPECT_STREQ(_T("EFGH"), value);
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueClientId,
                             &value));
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueReferralId,
                             &value));
  DWORD dword_value(0);
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueInstallTimeSec,
                             &dword_value));

  DWORD day_of_install(0);
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueDayOfInstall,
                             &day_of_install));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       SetAppBranding_BrandAlreadyExistsBrandCodeOnly) {
  ASSERT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    _T("EFGH")));

  EXPECT_SUCCEEDED(SetAppBranding(kAppMachineClientStatePath,
                                  _T("ABCD"),
                                  _T(""),
                                  _T(""),
                                  -1));

  CString value;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    &value));
  EXPECT_STREQ(_T("EFGH"), value);
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueClientId,
                             &value));
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueReferralId,
                             &value));
  DWORD dword_value(0);
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueInstallTimeSec,
                             &dword_value));

  DWORD day_of_install(0);
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueDayOfInstall,
                             &day_of_install));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       SetAppBranding_ExistingBrandTooLong) {
  ASSERT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    _T("CHMG4CUTNt")));

  EXPECT_SUCCEEDED(SetAppBranding(kAppMachineClientStatePath,
                                  _T("ABCD"),
                                  _T(""),
                                  _T(""),
                                  -1));

  CString value;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    &value));
  EXPECT_STREQ(_T("CHMG"), value);
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueClientId,
                             &value));
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueReferralId,
                             &value));
  DWORD dword_value(0);
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueInstallTimeSec,
                             &dword_value));

  DWORD day_of_install(0);
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueDayOfInstall,
                             &day_of_install));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       SetAppBranding_BrandAlreadyExistsCliendIdOnly) {
  ASSERT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    _T("EFGH")));

  EXPECT_SUCCEEDED(SetAppBranding(kAppMachineClientStatePath,
                                  _T(""),
                                  _T("some_partner"),
                                  _T(""),
                                  -1));

  CString value;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    &value));
  EXPECT_STREQ(_T("EFGH"), value);
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueClientId,
                             &value));
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueReferralId,
                             &value));
  DWORD dword_value(0);
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueInstallTimeSec,
                             &dword_value));

  DWORD day_of_install(0);
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueDayOfInstall,
                             &day_of_install));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       SetAppBranding_BrandAlreadyExistsBothValid) {
  ASSERT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    _T("EFGH")));

  EXPECT_SUCCEEDED(SetAppBranding(kAppMachineClientStatePath,
                                  _T("ABCD"),
                                  _T("some_partner"),
                                  _T(""),
                                  -1));

  CString value;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    &value));
  EXPECT_STREQ(_T("EFGH"), value);
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueClientId,
                             &value));
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueReferralId,
                             &value));
  DWORD dword_value(0);
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueInstallTimeSec,
                             &dword_value));

  DWORD day_of_install(0);
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueDayOfInstall,
                             &day_of_install));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       SetAppBranding_ClientIdAlreadyExistsAllEmtpy) {
  ASSERT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueClientId,
                                    _T("existing_partner")));

  EXPECT_SUCCEEDED(SetAppBranding(kAppMachineClientStatePath,
                                  _T(""),
                                  _T(""),
                                  _T(""),
                                  -1));
  const uint32 now = Time64ToInt32(GetCurrent100NSTime());

  CString value;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    &value));
  EXPECT_STREQ(_T("GGLS"), value);
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueClientId,
                                    &value));
  EXPECT_STREQ(_T("existing_partner"), value);
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueReferralId,
                             &value));
  DWORD install_time = 0;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueInstallTimeSec,
                                    &install_time));
  EXPECT_GE(now, install_time);
  EXPECT_GE(static_cast<uint32>(200), now - install_time);

  DWORD day_of_install(0);
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueDayOfInstall,
                                    &day_of_install));
  EXPECT_EQ(static_cast<DWORD>(-1), day_of_install);
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       SetAppBranding_ClientIdAlreadyExistsBrandCodeOnly) {
  ASSERT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueClientId,
                                    _T("existing_partner")));

  EXPECT_SUCCEEDED(SetAppBranding(kAppMachineClientStatePath,
                                  _T("ABCE"),
                                  _T(""),
                                  _T(""),
                                  -1));
  const uint32 now = Time64ToInt32(GetCurrent100NSTime());

  CString value;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    &value));
  EXPECT_STREQ(_T("ABCE"), value);
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueClientId,
                                    &value));
  EXPECT_STREQ(_T("existing_partner"), value);
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueReferralId,
                             &value));
  DWORD install_time = 0;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueInstallTimeSec,
                                    &install_time));
  EXPECT_GE(now, install_time);
  EXPECT_GE(static_cast<uint32>(200), now - install_time);

  DWORD day_of_install(0);
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueDayOfInstall,
                                    &day_of_install));
  EXPECT_EQ(static_cast<DWORD>(-1), day_of_install);
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       SetAppBranding_ClientIdAlreadyExistsCliendIdOnly) {
  ASSERT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueClientId,
                                    _T("existing_partner")));

  EXPECT_SUCCEEDED(SetAppBranding(kAppMachineClientStatePath,
                                  _T(""),
                                  _T("some_partner"),
                                  _T(""),
                                  -1));
  const uint32 now = Time64ToInt32(GetCurrent100NSTime());

  CString value;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    &value));
  EXPECT_STREQ(_T("GGLS"), value);
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueClientId,
                                    &value));
  EXPECT_STREQ(_T("some_partner"), value);
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueReferralId,
                             &value));
  DWORD install_time = 0;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueInstallTimeSec,
                                    &install_time));
  EXPECT_GE(now, install_time);
  EXPECT_GE(static_cast<uint32>(200), now - install_time);

  DWORD day_of_install(0);
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueDayOfInstall,
                                    &day_of_install));
  EXPECT_EQ(static_cast<DWORD>(-1), day_of_install);
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       SetAppBranding_ClientIdAlreadyExistsBothValid) {
  ASSERT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueClientId,
                                    _T("existing_partner")));

  EXPECT_SUCCEEDED(SetAppBranding(kAppMachineClientStatePath,
                                  _T("ABCD"),
                                  _T("some_partner"),
                                  _T(""),
                                  -1));
  const uint32 now = Time64ToInt32(GetCurrent100NSTime());

  CString value;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    &value));
  EXPECT_STREQ(_T("ABCD"), value);
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueClientId,
                                    &value));
  EXPECT_STREQ(_T("some_partner"), value);
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueReferralId,
                             &value));
  DWORD install_time = 0;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueInstallTimeSec,
                                    &install_time));
  EXPECT_GE(now, install_time);
  EXPECT_GE(static_cast<uint32>(200), now - install_time);

  DWORD day_of_install(0);
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueDayOfInstall,
                                    &day_of_install));
  EXPECT_EQ(static_cast<DWORD>(-1), day_of_install);
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       SetAppBranding_AllAlreadyExistAllEmpty) {
  ASSERT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    _T("EFGH")));
  ASSERT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueClientId,
                                    _T("existing_partner")));
  ASSERT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueReferralId,
                                    __T("existingreferrerid")));
  const DWORD kInstallTime = 1234567890;
  ASSERT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueInstallTimeSec,
                                    kInstallTime));
  const DWORD kDayOfInstall = 3344;
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueDayOfInstall,
                                    kDayOfInstall));

  EXPECT_SUCCEEDED(SetAppBranding(kAppMachineClientStatePath,
                                  _T(""),
                                  _T(""),
                                  _T(""),
                                  -1));

  CString value;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    &value));
  EXPECT_STREQ(_T("EFGH"), value);
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueClientId,
                                    &value));
  EXPECT_STREQ(_T("existing_partner"), value);
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueReferralId,
                                    &value));
  EXPECT_STREQ(__T("existingreferrerid"), value);
  EXPECT_EQ(kInstallTime,
            GetDwordValue(kAppMachineClientStatePath, kRegValueInstallTimeSec));
  EXPECT_EQ(kDayOfInstall,
            GetDwordValue(kAppMachineClientStatePath, kRegValueDayOfInstall));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       SetAppBranding_AllAlreadyExistBrandCodeOnly) {
  ASSERT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    _T("EFGH")));
  ASSERT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueClientId,
                                    _T("existing_partner")));
  ASSERT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueReferralId,
                                    __T("existingreferrerid")));
  const DWORD kInstallTime = 1234567890;
  ASSERT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueInstallTimeSec,
                                    kInstallTime));
  const DWORD kDayOfInstall = 3344;
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueDayOfInstall,
                                    kDayOfInstall));

  EXPECT_SUCCEEDED(SetAppBranding(kAppMachineClientStatePath,
                                  _T("ABCD"),
                                  _T(""),
                                  _T(""),
                                  -1));

  CString value;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    &value));
  EXPECT_STREQ(_T("EFGH"), value);
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueClientId,
                                    &value));
  EXPECT_STREQ(_T("existing_partner"), value);
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueReferralId,
                                    &value));
  EXPECT_STREQ(__T("existingreferrerid"), value);
  EXPECT_EQ(kInstallTime,
            GetDwordValue(kAppMachineClientStatePath, kRegValueInstallTimeSec));
  EXPECT_EQ(kDayOfInstall,
            GetDwordValue(kAppMachineClientStatePath, kRegValueDayOfInstall));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       SetAppBranding_BothAlreadyExistCliendIdOnly) {
  ASSERT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    _T("EFGH")));
  ASSERT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueClientId,
                                    _T("existing_partner")));
  ASSERT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueReferralId,
                                    __T("existingreferrerid")));

  EXPECT_SUCCEEDED(SetAppBranding(kAppMachineClientStatePath,
                                  _T(""),
                                  _T("some_partner"),
                                  _T(""),
                                  -1));

  CString value;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    &value));
  EXPECT_STREQ(_T("EFGH"), value);
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueClientId,
                                    &value));
  EXPECT_STREQ(_T("existing_partner"), value);
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueReferralId,
                                    &value));
  EXPECT_STREQ(__T("existingreferrerid"), value);
  DWORD dword_value(0);
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueInstallTimeSec,
                             &dword_value));

  DWORD day_of_install(0);
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueDayOfInstall,
                             &day_of_install));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       SetAppBranding_BothAlreadyExistBothValid) {
  ASSERT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    _T("EFGH")));
  ASSERT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueClientId,
                                    _T("existing_partner")));
  ASSERT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueReferralId,
                                    _T("existingreferrerid")));

  EXPECT_SUCCEEDED(SetAppBranding(kAppMachineClientStatePath,
                                  _T("ABCD"),
                                  _T("some_partner"),
                                  _T(""),
                                  -1));

  CString value;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    &value));
  EXPECT_STREQ(_T("EFGH"), value);
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueClientId,
                                    &value));
  EXPECT_STREQ(_T("existing_partner"), value);
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueReferralId,
                                    &value));
  EXPECT_STREQ(_T("existingreferrerid"), value);
  DWORD dword_value(0);
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueInstallTimeSec,
                             &dword_value));

  DWORD day_of_install(0);
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueDayOfInstall,
                             &day_of_install));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest,
       SetAppBranding_InstallTimeAlreadyExistsBrandCodeOnly) {
  const DWORD kExistingInstallTime = 1234567890;
  ASSERT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueInstallTimeSec,
                                    kExistingInstallTime));
  const DWORD kDayOfInstall = 3344;
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    kRegValueDayOfInstall,
                                    kDayOfInstall));

  EXPECT_SUCCEEDED(SetAppBranding(kAppMachineClientStatePath,
                                  _T("ABCE"),
                                  _T(""),
                                  _T(""),
                                  -1));
  const uint32 now = Time64ToInt32(GetCurrent100NSTime());

  CString value;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueBrandCode,
                                    &value));
  EXPECT_STREQ(_T("ABCE"), value);
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueClientId,
                             &value));
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            RegKey::GetValue(kAppMachineClientStatePath,
                             kRegValueReferralId,
                             &value));
  DWORD install_time = 0;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueInstallTimeSec,
                                    &install_time));
  EXPECT_NE(kExistingInstallTime, install_time);
  EXPECT_GE(now, install_time);
  EXPECT_GE(static_cast<uint32>(200), now - install_time);

  DWORD day_of_install(0);
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    kRegValueDayOfInstall,
                                    &day_of_install));
  EXPECT_EQ(static_cast<DWORD>(-1), day_of_install);
}

TEST_F(AppRegistryUtilsRegistryProtectedTest, GetAppVersion_User) {
  const CString expected_pv = _T("1.0");

  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(kOmahaUserClientsPath,
                                            kRegValueProductVersion,
                                            expected_pv));

  CString actual_pv;
  GetAppVersion(false, kGoogleUpdateAppId, &actual_pv);
  EXPECT_STREQ(expected_pv, actual_pv);
}

TEST_F(AppRegistryUtilsRegistryProtectedTest, GetAppVersion_Machine) {
  const CString expected_pv = _T("1.0");

  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(kOmahaMachineClientsPath,
                                            kRegValueProductVersion,
                                            expected_pv));

  CString actual_pv;
  GetAppVersion(true, kGoogleUpdateAppId, &actual_pv);
  EXPECT_STREQ(expected_pv, actual_pv);
}

TEST_F(AppRegistryUtilsRegistryProtectedTest, GetAppName_User) {
  const CString expected_name = _T("User App");

  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(kOmahaUserClientsPath,
                                            kRegValueAppName,
                                            expected_name));

  CString actual_name;
  GetAppName(false, kGoogleUpdateAppId, &actual_name);
  EXPECT_STREQ(expected_name, actual_name);
}

TEST_F(AppRegistryUtilsRegistryProtectedTest, GetAppName_Machine) {
  const CString expected_name = _T("Machine App");

  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(kOmahaMachineClientsPath,
                                            kRegValueAppName,
                                            expected_name));

  CString actual_name;
  GetAppName(true, kGoogleUpdateAppId, &actual_name);
  EXPECT_STREQ(expected_name, actual_name);
}

TEST_F(AppRegistryUtilsRegistryProtectedTest, GetAppLang_User) {
  const CString expected_lang = _T("ar");

  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(kOmahaUserClientStatePath,
                                            kRegValueLanguage,
                                            expected_lang));

  CString actual_lang;
  GetAppLang(false, kGoogleUpdateAppId, &actual_lang);
  EXPECT_STREQ(expected_lang, actual_lang);
}

TEST_F(AppRegistryUtilsRegistryProtectedTest, GetAppLang_Machine) {
  const CString expected_lang = _T("es");

  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(kOmahaMachineClientStatePath,
                                            kRegValueLanguage,
                                            expected_lang));

  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(kOmahaMachineClientsPath,
                                            kRegValueLanguage,
                                            _T("ar")));

  CString actual_lang;
  GetAppLang(true, kGoogleUpdateAppId, &actual_lang);
  EXPECT_STREQ(expected_lang, actual_lang);
}

INSTANTIATE_TEST_CASE_P(IsMachine,
                        AppRegistryUtilsRegistryProtectedTest,
                        ::testing::Bool());

TEST_P(AppRegistryUtilsRegistryProtectedTest, GetClientStateData) {
  const CString expected_pv           = _T("1.0");
  const CString expected_ap           = _T("additional parameters");
  const CString expected_lang         = _T("some lang");
  const CString expected_brand_code   = _T("some brand");
  const CString expected_client_id    = _T("some client id");
  const CString expected_iid          =
      _T("{7C0B6E56-B24B-436b-A960-A6EA201E886F}");

  // Sets a valid and an expired experiment label. Expects that the expired
  // experiment label is filtered out and the valid experiment label includes
  // a time stamp.
  const CString experiment_label =
      _T("a=a|Wed, 14 Mar 2029 23:36:18 GMT;b=a|Fri, 14 Aug 2015 16:13:03 GMT");
  const CString expected_experiment_label =
      _T("a=a|Wed, 14 Mar 2029 23:36:18 GMT");

  const Cohort expected_cohort = {
    _T("Cohort1"),
    _T("CohortHint1"),
    _T("CohortName1")
  };
  const int day_of_install   = 2677;

  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(GetClientStatePath(),
                                            kRegValueProductVersion,
                                            expected_pv));
  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(GetClientStatePath(),
                                            kRegValueAdditionalParams,
                                            expected_ap));
  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(GetClientStatePath(),
                                            kRegValueLanguage,
                                            expected_lang));
  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(GetClientStatePath(),
                                            kRegValueBrandCode,
                                            expected_brand_code));
  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(GetClientStatePath(),
                                            kRegValueClientId,
                                            expected_client_id));
  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(GetClientStatePath(),
                                            kRegValueInstallationId,
                                            expected_iid));
  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(GetClientStatePath(),
                                            kRegValueExperimentLabels,
                                            experiment_label));
  EXPECT_HRESULT_SUCCEEDED(WriteCohort(IsMachine(),
                                       kGoogleUpdateAppId,
                                       expected_cohort));
  EXPECT_HRESULT_SUCCEEDED(
      RegKey::SetValue(GetClientStatePath(),
                       kRegValueDayOfInstall,
                       static_cast<DWORD>(day_of_install)));


  const DWORD now = Time64ToInt32(GetCurrent100NSTime());
  const DWORD one_day_back = now - kSecondsPerDay;
  ASSERT_SUCCEEDED(RegKey::SetValue(GetClientStatePath(),
                                    kRegValueInstallTimeSec,
                                    one_day_back));
  CString actual_pv, actual_ap, actual_lang, actual_brand_code,
      actual_client_id, actual_experiment_label, actual_iid;
  Cohort actual_cohort;
  int actual_day_of_install(0);
  int actual_install_time_diff_sec(0);

  GetClientStateData(IsMachine(),
                     kGoogleUpdateAppId,
                     &actual_pv,
                     &actual_ap,
                     &actual_lang,
                     &actual_brand_code,
                     &actual_client_id,
                     &actual_iid,
                     &actual_experiment_label,
                     &actual_cohort,
                     &actual_install_time_diff_sec,
                     &actual_day_of_install);

  EXPECT_STREQ(expected_pv, actual_pv);
  EXPECT_STREQ(expected_ap, actual_ap);
  EXPECT_STREQ(expected_lang, actual_lang);
  EXPECT_STREQ(expected_brand_code, actual_brand_code);
  EXPECT_STREQ(expected_client_id, actual_client_id);
  EXPECT_STREQ(expected_iid, actual_iid);
  EXPECT_STREQ(expected_experiment_label, actual_experiment_label);
  EXPECT_STREQ(expected_cohort.cohort, actual_cohort.cohort);
  EXPECT_STREQ(expected_cohort.hint, actual_cohort.hint);
  EXPECT_STREQ(expected_cohort.name, actual_cohort.name);
  EXPECT_EQ(GetFirstDayOfWeek(day_of_install), actual_day_of_install);
  EXPECT_GE(actual_install_time_diff_sec, kSecondsPerDay);
}

TEST_F(AppRegistryUtilsRegistryProtectedTest, RemoveClientState_Machine) {
  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                            kRegValueProductVersion,
                                            _T("1.1")));
  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                            kRegValueProductVersion,
                                            _T("1.1")));
  RemoveClientState(true, GOOPDATE_APP_ID);
  EXPECT_TRUE(RegKey::HasValue(kAppMachineClientStatePath,
                               kRegValueProductVersion));
  EXPECT_TRUE(RegKey::HasValue(kAppMachineClientStateMediumPath,
                               kRegValueProductVersion));
  EXPECT_HRESULT_SUCCEEDED(RemoveClientState(true, kAppGuid));
  EXPECT_FALSE(RegKey::HasValue(kAppMachineClientStatePath,
                                kRegValueProductVersion));
  EXPECT_FALSE(RegKey::HasValue(kAppMachineClientStateMediumPath,
                                kRegValueProductVersion));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest, RemoveClientState_User) {
  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                            kRegValueProductVersion,
                                            _T("1.1")));
  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                            kRegValueProductVersion,
                                            _T("1.1")));
  RemoveClientState(false, GOOPDATE_APP_ID);
  EXPECT_TRUE(RegKey::HasValue(kAppUserClientStatePath,
                               kRegValueProductVersion));
  EXPECT_HRESULT_SUCCEEDED(RemoveClientState(false, kAppGuid));
  EXPECT_FALSE(RegKey::HasValue(kAppUserClientStatePath,
                                kRegValueProductVersion));

  // For user case, StateMedium key is not deleted.
  EXPECT_TRUE(RegKey::HasValue(kAppUserClientStateMediumPath,
                               kRegValueProductVersion));
}

TEST_F(AppRegistryUtilsRegistryProtectedTest, GetUninstalledApps_Machine) {
  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(kOmahaMachineClientsPath,
                                            kRegValueProductVersion,
                                            _T("1.0")));
  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(kOmahaMachineClientStatePath,
                                            kRegValueProductVersion,
                                            _T("1.0")));
  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                            kRegValueProductVersion,
                                            _T("1.1")));

  std::vector<CString> uninstalled_apps;
  GetUninstalledApps(true, &uninstalled_apps);
  EXPECT_EQ(1, uninstalled_apps.size());
  EXPECT_STREQ(kAppGuid, uninstalled_apps[0]);

  RemoveClientStateForApps(true, uninstalled_apps);
  uninstalled_apps.clear();
  GetUninstalledApps(true, &uninstalled_apps);
  EXPECT_EQ(0, uninstalled_apps.size());
}

TEST_F(AppRegistryUtilsRegistryProtectedTest, GetUninstalledApps_User) {
  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(kOmahaUserClientsPath,
                                            kRegValueProductVersion,
                                            _T("1.0")));
  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(kOmahaUserClientStatePath,
                                            kRegValueProductVersion,
                                            _T("1.0")));
  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                            kRegValueProductVersion,
                                            _T("1.1")));

  std::vector<CString> uninstalled_apps;
  GetUninstalledApps(false, &uninstalled_apps);
  EXPECT_EQ(1, uninstalled_apps.size());
  EXPECT_STREQ(kAppGuid, uninstalled_apps[0]);

  RemoveClientStateForApps(false, uninstalled_apps);
  uninstalled_apps.clear();
  GetUninstalledApps(false, &uninstalled_apps);
  EXPECT_EQ(0, uninstalled_apps.size());
}

TEST_F(AppRegistryUtilsRegistryProtectedTest, LastOSVersion_User) {
  const bool is_machine = false;

  OSVERSIONINFOEX this_os = {};
  OSVERSIONINFOEX reg_version = {};
  ASSERT_SUCCEEDED(SystemInfo::GetOSVersion(&this_os));
  ASSERT_EQ(sizeof(this_os), this_os.dwOSVersionInfoSize);

  // Double-check that GetLastOSVersion() returns nothing if there's no
  // registry key.
  ::ZeroMemory(&reg_version, sizeof(reg_version));
  EXPECT_HRESULT_FAILED(GetLastOSVersion(is_machine, &reg_version));
  EXPECT_EQ(0, reg_version.dwOSVersionInfoSize);

  // Call SetLastOSVersion(NULL) to write the current OS version.
  EXPECT_HRESULT_SUCCEEDED(SetLastOSVersion(is_machine, NULL));

  ::ZeroMemory(&reg_version, sizeof(reg_version));
  ASSERT_HRESULT_SUCCEEDED(GetLastOSVersion(is_machine, &reg_version));
  ASSERT_EQ(sizeof(reg_version), reg_version.dwOSVersionInfoSize);
  EXPECT_EQ(0, ::memcmp(&this_os, &reg_version, sizeof(this_os)));

  // Then, test calling SetLastOSVersion() with an arbitrary OS version.
  --this_os.dwMajorVersion;
  --this_os.wServicePackMinor;
  EXPECT_HRESULT_SUCCEEDED(SetLastOSVersion(is_machine, &this_os));

  ::ZeroMemory(&reg_version, sizeof(reg_version));
  ASSERT_HRESULT_SUCCEEDED(GetLastOSVersion(is_machine, &reg_version));
  ASSERT_EQ(sizeof(reg_version), reg_version.dwOSVersionInfoSize);
  EXPECT_EQ(0, ::memcmp(&this_os, &reg_version, sizeof(this_os)));
}


TEST_F(AppRegistryUtilsRegistryProtectedTest, LastOSVersion_Machine) {
  const bool is_machine = true;

  OSVERSIONINFOEX this_os = {};
  OSVERSIONINFOEX reg_version = {};
  ASSERT_SUCCEEDED(SystemInfo::GetOSVersion(&this_os));
  ASSERT_EQ(sizeof(this_os), this_os.dwOSVersionInfoSize);

  // Double-check that GetLastOSVersion() returns nothing if there's no
  // registry key.
  ::ZeroMemory(&reg_version, sizeof(reg_version));
  EXPECT_HRESULT_FAILED(GetLastOSVersion(is_machine, &reg_version));
  EXPECT_EQ(0, reg_version.dwOSVersionInfoSize);

  // Call SetLastOSVersion(NULL) to write the current OS version.
  EXPECT_HRESULT_SUCCEEDED(SetLastOSVersion(is_machine, NULL));

  ::ZeroMemory(&reg_version, sizeof(reg_version));
  ASSERT_HRESULT_SUCCEEDED(GetLastOSVersion(is_machine, &reg_version));
  ASSERT_EQ(sizeof(reg_version), reg_version.dwOSVersionInfoSize);
  EXPECT_EQ(0, ::memcmp(&this_os, &reg_version, sizeof(this_os)));

  // Then, test calling SetLastOSVersion() with an arbitrary OS version.
  --this_os.dwMajorVersion;
  --this_os.wServicePackMinor;
  EXPECT_HRESULT_SUCCEEDED(SetLastOSVersion(is_machine, &this_os));

  ::ZeroMemory(&reg_version, sizeof(reg_version));
  ASSERT_HRESULT_SUCCEEDED(GetLastOSVersion(is_machine, &reg_version));
  ASSERT_EQ(sizeof(reg_version), reg_version.dwOSVersionInfoSize);
  EXPECT_EQ(0, ::memcmp(&this_os, &reg_version, sizeof(this_os)));
}

}  // namespace app_registry_utils

}  // namespace omaha
