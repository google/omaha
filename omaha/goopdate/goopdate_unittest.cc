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

#include "omaha/base/constants.h"
#include "omaha/base/reg_key.h"
#include "omaha/common/command_line.h"
#include "omaha/goopdate/goopdate_internal.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace {

const TCHAR* const kAppMachineClientStatePath =
    _T("HKLM\\Software\\") PATH_COMPANY_NAME _T("\\") PRODUCT_NAME
    _T("\\ClientState\\{19BE47E4-CF32-48c1-94C4-046507F6A8A6}\\");
const TCHAR* const kApp2MachineClientStatePath =
    _T("HKLM\\Software\\") PATH_COMPANY_NAME _T("\\") PRODUCT_NAME
    _T("\\ClientState\\{553B2D8C-E6A7-43ed-ACC9-A8BA5D34395F}\\");
const TCHAR* const kAppUserClientStatePath =
    _T("HKCU\\Software\\") PATH_COMPANY_NAME _T("\\") PRODUCT_NAME
    _T("\\ClientState\\{19BE47E4-CF32-48c1-94C4-046507F6A8A6}\\");
const TCHAR* const kApp2UserClientStatePath =
    _T("HKCU\\Software\\") PATH_COMPANY_NAME _T("\\") PRODUCT_NAME
    _T("\\ClientState\\{553B2D8C-E6A7-43ed-ACC9-A8BA5D34395F}\\");

const TCHAR* const kAppMachineClientStateMediumPath =
    _T("HKLM\\Software\\") PATH_COMPANY_NAME _T("\\") PRODUCT_NAME
    _T("\\ClientStateMedium\\{19BE47E4-CF32-48c1-94C4-046507F6A8A6}\\");

// Update this when new modes are added.
const int kLastMode = COMMANDLINE_MODE_HEALTH_CHECK;

}  // namespace

class GoopdateRegistryProtectedTest : public testing::Test {
 protected:
  GoopdateRegistryProtectedTest()
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

TEST_F(GoopdateRegistryProtectedTest,
       PromoteAppEulaAccepted_Machine_UpdateKeyDoesNotExist_NoAppKeys) {
  EXPECT_SUCCEEDED(internal::PromoteAppEulaAccepted(true));
  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(GoopdateRegistryProtectedTest,
       PromoteAppEulaAccepted_Machine_UpdateValueDoesNotExist_NoAppKeys) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(MACHINE_REG_UPDATE));
  EXPECT_SUCCEEDED(internal::PromoteAppEulaAccepted(true));
  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(GoopdateRegistryProtectedTest,
       PromoteAppEulaAccepted_Machine_UpdateValueDoesNotExist_AppKeyNoValue) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("pv"),
                                    _T("1.2.3.4")));
  EXPECT_SUCCEEDED(internal::PromoteAppEulaAccepted(true));
  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(GoopdateRegistryProtectedTest,
       PromoteAppEulaAccepted_Machine_UpdateValueDoesNotExist_AppValueZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(internal::PromoteAppEulaAccepted(true));
  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(GoopdateRegistryProtectedTest,
       PromoteAppEulaAccepted_Machine_UpdateZero_NoAppKeys) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));

  EXPECT_EQ(S_FALSE, internal::PromoteAppEulaAccepted(true));

  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(MACHINE_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
}

TEST_F(GoopdateRegistryProtectedTest,
       PromoteAppEulaAccepted_Machine_UpdateZero_AppKeyNoValue) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("pv"),
                                    _T("1.2.3.4")));

  EXPECT_SUCCEEDED(internal::PromoteAppEulaAccepted(true));

  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(MACHINE_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStatePath, _T("eulaaccepted")));
}

TEST_F(GoopdateRegistryProtectedTest,
       PromoteAppEulaAccepted_Machine_UpdateZero_AppValueZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));

  EXPECT_SUCCEEDED(internal::PromoteAppEulaAccepted(true));

  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(MACHINE_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
  value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(kAppMachineClientStatePath, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
}

TEST_F(GoopdateRegistryProtectedTest,
       PromoteAppEulaAccepted_Machine_UpdateZero_AppValueOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));

  EXPECT_SUCCEEDED(internal::PromoteAppEulaAccepted(true));

  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, _T("eulaaccepted")));
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(kAppMachineClientStatePath, _T("eulaaccepted"), &value));
  EXPECT_EQ(1, value);
}

// Unfortunately, the app's ClientStateMedium key is not checked if there is no
// corresponding ClientState key.
TEST_F(GoopdateRegistryProtectedTest,
       PromoteAppEulaAccepted_Machine_UpdateZero_OnlyMediumAppValueOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));

  EXPECT_SUCCEEDED(internal::PromoteAppEulaAccepted(true));

  // The ClientStateMedium was not checked, so eulaaccepted = 0 still exists.
  EXPECT_TRUE(RegKey::HasValue(MACHINE_REG_UPDATE, _T("eulaaccepted")));
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(RegKey::GetValue(MACHINE_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    &value));
  EXPECT_EQ(0, value);
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  value = UINT_MAX;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    &value));
  EXPECT_EQ(1, value);
}

TEST_F(GoopdateRegistryProtectedTest,
       PromoteAppEulaAccepted_Machine_UpdateZero_MediumAppValueOneAndStateKey) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(kAppMachineClientStatePath));
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));

  EXPECT_SUCCEEDED(internal::PromoteAppEulaAccepted(true));

  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, _T("eulaaccepted")));
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    &value));
  EXPECT_EQ(1, value);
  value = UINT_MAX;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    &value));
  EXPECT_EQ(1, value);
}

// The ClientStateMedium 1 is copied over the ClientState 0.
TEST_F(GoopdateRegistryProtectedTest,
       PromoteAppEulaAccepted_Machine_UpdateZero_AppZeroAppMediumOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));

  EXPECT_SUCCEEDED(internal::PromoteAppEulaAccepted(true));

  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, _T("eulaaccepted")));
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    &value));
  EXPECT_EQ(1, value);
  value = UINT_MAX;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    &value));
  EXPECT_EQ(1, value);
}

// The ClientStateMedium 0 is ignored and not copied.
TEST_F(GoopdateRegistryProtectedTest,
       PromoteAppEulaAccepted_Machine_UpdateZero_AppOneAppMediumZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));

  EXPECT_SUCCEEDED(internal::PromoteAppEulaAccepted(true));

  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, _T("eulaaccepted")));
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    &value));
  EXPECT_EQ(1, value);
  value = UINT_MAX;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    &value));
  EXPECT_EQ(0, value);
}

TEST_F(GoopdateRegistryProtectedTest,
       PromoteAppEulaAccepted_Machine_UpdateZero_FirstAppZeroSecondAppOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kApp2MachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));

  EXPECT_SUCCEEDED(internal::PromoteAppEulaAccepted(true));

  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, _T("eulaaccepted")));
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(kAppMachineClientStatePath, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
  value = UINT_MAX;
  EXPECT_SUCCEEDED(RegKey::GetValue(kApp2MachineClientStatePath,
                                    _T("eulaaccepted"),
                                    &value));
  EXPECT_EQ(1, value);
}

TEST_F(GoopdateRegistryProtectedTest,
       PromoteAppEulaAccepted_Machine_UpdateZero_FirstAppNoValueSecondAppOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("pv"),
                                    _T("1.2.3.4")));
  EXPECT_SUCCEEDED(RegKey::SetValue(kApp2MachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));

  EXPECT_SUCCEEDED(internal::PromoteAppEulaAccepted(true));

  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppMachineClientStatePath, _T("eulaaccepted")));
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(RegKey::GetValue(kApp2MachineClientStatePath,
                                    _T("eulaaccepted"),
                                    &value));
  EXPECT_EQ(1, value);
}

// Asserts because this is an unexpected case.
TEST_F(GoopdateRegistryProtectedTest,
       PromoteAppEulaAccepted_Machine_UpdateZero_UpdateClientStateOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_CLIENT_STATE_GOOPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));

  ExpectAsserts expect_asserts;
  EXPECT_SUCCEEDED(internal::PromoteAppEulaAccepted(true));

  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, _T("eulaaccepted")));
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(RegKey::GetValue(MACHINE_REG_CLIENT_STATE_GOOPDATE,
                                    _T("eulaaccepted"),
                                    &value));
  EXPECT_EQ(1, value);
}

TEST_F(GoopdateRegistryProtectedTest,
       PromoteAppEulaAccepted_Machine_UpdateZero_UserAppOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));

  EXPECT_SUCCEEDED(internal::PromoteAppEulaAccepted(true));

  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(MACHINE_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
  value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(kAppUserClientStatePath, _T("eulaaccepted"), &value));
  EXPECT_EQ(1, value);
}

TEST_F(GoopdateRegistryProtectedTest,
       PromoteAppEulaAccepted_Machine_UpdateOne_AppZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));

  EXPECT_SUCCEEDED(internal::PromoteAppEulaAccepted(true));

  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(MACHINE_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(1, value);
  value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(kAppMachineClientStatePath, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
}

// The fact that the value is deleted also tells us that the method continued
// processing because the value existed even though the value was not zero.
TEST_F(GoopdateRegistryProtectedTest,
       PromoteAppEulaAccepted_Machine_UpdateOne_AppOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));

  EXPECT_SUCCEEDED(internal::PromoteAppEulaAccepted(true));

  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, _T("eulaaccepted")));
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(kAppMachineClientStatePath, _T("eulaaccepted"), &value));
  EXPECT_EQ(1, value);
}

TEST_F(GoopdateRegistryProtectedTest,
       PromoteAppEulaAccepted_User_UpdateKeyDoesNotExist_NoAppKeys) {
  EXPECT_SUCCEEDED(internal::PromoteAppEulaAccepted(false));
  EXPECT_FALSE(RegKey::HasValue(USER_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(GoopdateRegistryProtectedTest,
       PromoteAppEulaAccepted_User_UpdateValueDoesNotExist_NoAppKeys) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(USER_REG_UPDATE));
  EXPECT_SUCCEEDED(internal::PromoteAppEulaAccepted(false));
  EXPECT_FALSE(RegKey::HasValue(USER_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(GoopdateRegistryProtectedTest,
       PromoteAppEulaAccepted_User_UpdateValueDoesNotExist_AppKeyNoValue) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("pv"),
                                    _T("1.2.3.4")));
  EXPECT_SUCCEEDED(internal::PromoteAppEulaAccepted(false));
  EXPECT_FALSE(RegKey::HasValue(USER_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(GoopdateRegistryProtectedTest,
       PromoteAppEulaAccepted_User_UpdateValueDoesNotExist_AppValueZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(internal::PromoteAppEulaAccepted(false));
  EXPECT_FALSE(RegKey::HasValue(USER_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(GoopdateRegistryProtectedTest,
       PromoteAppEulaAccepted_User_UpdateZero_NoAppKeys) {
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));

  EXPECT_EQ(S_FALSE, internal::PromoteAppEulaAccepted(false));

  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(USER_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
}

TEST_F(GoopdateRegistryProtectedTest,
       PromoteAppEulaAccepted_User_UpdateZero_AppKeyNoValue) {
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("pv"),
                                    _T("1.2.3.4")));

  EXPECT_SUCCEEDED(internal::PromoteAppEulaAccepted(false));

  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(USER_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStatePath, _T("eulaaccepted")));
}

TEST_F(GoopdateRegistryProtectedTest,
       PromoteAppEulaAccepted_User_UpdateZero_AppValueZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));

  EXPECT_SUCCEEDED(internal::PromoteAppEulaAccepted(false));

  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(USER_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
  value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(kAppUserClientStatePath, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
}

TEST_F(GoopdateRegistryProtectedTest,
       PromoteAppEulaAccepted_User_UpdateZero_FirstAppZeroSecondAppOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kApp2UserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));

  EXPECT_SUCCEEDED(internal::PromoteAppEulaAccepted(false));

  EXPECT_FALSE(RegKey::HasValue(USER_REG_UPDATE, _T("eulaaccepted")));
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(kAppUserClientStatePath, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
  value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(kApp2UserClientStatePath, _T("eulaaccepted"), &value));
  EXPECT_EQ(1, value);
}

TEST_F(GoopdateRegistryProtectedTest,
       PromoteAppEulaAccepted_User_UpdateZero_FirstAppNoValueSecondAppOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("pv"),
                                    _T("1.2.3.4")));
  EXPECT_SUCCEEDED(RegKey::SetValue(kApp2UserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));

  EXPECT_SUCCEEDED(internal::PromoteAppEulaAccepted(false));

  EXPECT_FALSE(RegKey::HasValue(USER_REG_UPDATE, _T("eulaaccepted")));
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStatePath, _T("eulaaccepted")));
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(kApp2UserClientStatePath, _T("eulaaccepted"), &value));
  EXPECT_EQ(1, value);
}

// Asserts because this is an unexpected case.
TEST_F(GoopdateRegistryProtectedTest,
       PromoteAppEulaAccepted_User_UpdateZero_UpdateClientStateOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));

  ExpectAsserts expect_asserts;
  EXPECT_SUCCEEDED(internal::PromoteAppEulaAccepted(false));

  EXPECT_FALSE(RegKey::HasValue(USER_REG_UPDATE, _T("eulaaccepted")));
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(RegKey::GetValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                    _T("eulaaccepted"),
                                    &value));
  EXPECT_EQ(1, value);
}

TEST_F(GoopdateRegistryProtectedTest,
       PromoteAppEulaAccepted_User_UpdateOne_AppZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));

  EXPECT_SUCCEEDED(internal::PromoteAppEulaAccepted(false));

  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(USER_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(1, value);
  value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(kAppUserClientStatePath, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
}

// The fact that the value is deleted also tells us that the method continued
// processing because the value existed even though the value was not zero.
TEST_F(GoopdateRegistryProtectedTest,
       PromoteAppEulaAccepted_User_UpdateOne_AppOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));

  EXPECT_SUCCEEDED(internal::PromoteAppEulaAccepted(false));

  EXPECT_FALSE(RegKey::HasValue(USER_REG_UPDATE, _T("eulaaccepted")));
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(kAppUserClientStatePath, _T("eulaaccepted"), &value));
  EXPECT_EQ(1, value);
}

TEST_F(GoopdateRegistryProtectedTest,
       PromoteAppEulaAccepted_User_UpdateZero_MachineAppOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));

  EXPECT_SUCCEEDED(internal::PromoteAppEulaAccepted(false));

  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(USER_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
  value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(kAppMachineClientStatePath, _T("eulaaccepted"), &value));
  EXPECT_EQ(1, value);
}

// ClientStateMedium is not used.
TEST_F(GoopdateRegistryProtectedTest,
       PromoteAppEulaAccepted_User_UpdateZero_MediumAppValueOneAndStateKey) {
  const TCHAR* const kAppUserClientStateMediumPath =
      _T("HKCU\\Software\\") PATH_COMPANY_NAME _T("\\") PRODUCT_NAME
      _T("\\ClientStateMedium\\{19BE47E4-CF32-48c1-94C4-046507F6A8A6}\\");

  EXPECT_SUCCEEDED(RegKey::CreateKey(kAppUserClientStatePath));
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));

  EXPECT_SUCCEEDED(internal::PromoteAppEulaAccepted(false));

  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(RegKey::GetValue(USER_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    &value));
  EXPECT_EQ(0, value);
  EXPECT_FALSE(
      RegKey::HasValue(kAppUserClientStatePath, _T("eulaaccepted")));
  value = UINT_MAX;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppUserClientStateMediumPath,
                                    _T("eulaaccepted"),
                                    &value));
  EXPECT_EQ(1, value);
}

//
// IsMachineProcess tests.
//

class GoopdateIsMachineProcessTest : public testing::Test {
 protected:
  bool FromMachineDirHelper(CommandLineMode mode) {
    return internal::IsMachineProcess(mode,
                                      true,
                                      false,
                                      false,
                                      TRISTATE_NONE);
  }

  bool IsLocalSystemHelper(CommandLineMode mode) {
    return internal::IsMachineProcess(mode,
                                      false,
                                      true,
                                      false,
                                      TRISTATE_NONE);
  }

  bool MachineOverrideHelper(CommandLineMode mode) {
    return internal::IsMachineProcess(mode,
                                      false,
                                      false,
                                      true,
                                      TRISTATE_NONE);
  }

  bool NeedsAdminFalseHelper(CommandLineMode mode) {
    return internal::IsMachineProcess(mode,
                                      false,
                                      false,
                                      false,
                                      TRISTATE_FALSE);
  }

  bool NeedsAdminTrueHelper(CommandLineMode mode) {
    return internal::IsMachineProcess(mode,
                                      false,
                                      false,
                                      false,
                                      TRISTATE_TRUE);
  }
};

// Unused function. Its sole purpose is to make sure that the unit tests below
// were correctly updated when new modes were added.
#pragma warning(push)
// enumerator 'identifier' in switch of enum 'enumeration' is not explicitly
// handled by a case label.
// enumerator 'identifier' in switch of enum 'enumeration' is not handled.
#pragma warning(1: 4061 4062)
static void EnsureUnitTestUpdatedWithNewModes() {
  CommandLineMode unused_mode(COMMANDLINE_MODE_UNKNOWN);
  switch (unused_mode) {
    case COMMANDLINE_MODE_UNKNOWN:
    case COMMANDLINE_MODE_NOARGS:
    case COMMANDLINE_MODE_CORE:
    case COMMANDLINE_MODE_SERVICE:
    case COMMANDLINE_MODE_REGSERVER:
    case COMMANDLINE_MODE_UNREGSERVER:
    case COMMANDLINE_MODE_CRASH:
    case COMMANDLINE_MODE_REPORTCRASH:
    case COMMANDLINE_MODE_INSTALL:
    case COMMANDLINE_MODE_UPDATE:
    case COMMANDLINE_MODE_HANDOFF_INSTALL:
    case COMMANDLINE_MODE_UA:
    case COMMANDLINE_MODE_RECOVER:
    case COMMANDLINE_MODE_CODE_RED_CHECK:
    case COMMANDLINE_MODE_COMSERVER:
    case COMMANDLINE_MODE_REGISTER_PRODUCT:
    case COMMANDLINE_MODE_UNREGISTER_PRODUCT:
    case COMMANDLINE_MODE_SERVICE_REGISTER:
    case COMMANDLINE_MODE_SERVICE_UNREGISTER:
    case COMMANDLINE_MODE_CRASH_HANDLER:
    case COMMANDLINE_MODE_COMBROKER:
    case COMMANDLINE_MODE_ONDEMAND:
    case COMMANDLINE_MODE_MEDIUM_SERVICE:
    case COMMANDLINE_MODE_UNINSTALL:
    case COMMANDLINE_MODE_PING:
    case COMMANDLINE_MODE_HEALTH_CHECK:
    //
    // When adding a new mode, be sure to update kLastMode too.
    //
      break;
  }
}
#pragma warning(pop)

TEST_F(GoopdateIsMachineProcessTest, IsMachineProcess_MachineDirOnly) {
  EXPECT_TRUE(FromMachineDirHelper(COMMANDLINE_MODE_UNKNOWN));
  EXPECT_TRUE(FromMachineDirHelper(COMMANDLINE_MODE_NOARGS));
  EXPECT_FALSE(FromMachineDirHelper(COMMANDLINE_MODE_CORE));
  {
    ExpectAsserts expect_asserts;
    EXPECT_FALSE(FromMachineDirHelper(COMMANDLINE_MODE_SERVICE));
  }
  EXPECT_TRUE(FromMachineDirHelper(COMMANDLINE_MODE_REGSERVER));
  EXPECT_TRUE(FromMachineDirHelper(COMMANDLINE_MODE_UNREGSERVER));
  EXPECT_TRUE(FromMachineDirHelper(COMMANDLINE_MODE_CRASH));
  // TODO(omaha): Change to machine.
  EXPECT_FALSE(FromMachineDirHelper(COMMANDLINE_MODE_REPORTCRASH));
  {
    ExpectAsserts expect_asserts;
    EXPECT_FALSE(FromMachineDirHelper(COMMANDLINE_MODE_INSTALL));
  }
  EXPECT_FALSE(FromMachineDirHelper(COMMANDLINE_MODE_UPDATE));
  {
    ExpectAsserts expect_asserts;
    EXPECT_FALSE(FromMachineDirHelper(COMMANDLINE_MODE_HANDOFF_INSTALL));
  }
  EXPECT_FALSE(FromMachineDirHelper(COMMANDLINE_MODE_UA));
  EXPECT_FALSE(FromMachineDirHelper(COMMANDLINE_MODE_RECOVER));
  EXPECT_FALSE(FromMachineDirHelper(COMMANDLINE_MODE_CODE_RED_CHECK));
  EXPECT_TRUE(FromMachineDirHelper(COMMANDLINE_MODE_COMSERVER));
  {
    ExpectAsserts expect_asserts;
    EXPECT_FALSE(FromMachineDirHelper(COMMANDLINE_MODE_REGISTER_PRODUCT));
  }
  {
    ExpectAsserts expect_asserts;
    EXPECT_FALSE(FromMachineDirHelper(COMMANDLINE_MODE_UNREGISTER_PRODUCT));
  }
  EXPECT_TRUE(FromMachineDirHelper(COMMANDLINE_MODE_SERVICE_REGISTER));
  EXPECT_TRUE(FromMachineDirHelper(COMMANDLINE_MODE_SERVICE_UNREGISTER));
  EXPECT_FALSE(FromMachineDirHelper(COMMANDLINE_MODE_CRASH_HANDLER));
  EXPECT_TRUE(FromMachineDirHelper(COMMANDLINE_MODE_COMBROKER));
  EXPECT_TRUE(FromMachineDirHelper(COMMANDLINE_MODE_ONDEMAND));
  {
    ExpectAsserts expect_asserts;
    EXPECT_FALSE(FromMachineDirHelper(COMMANDLINE_MODE_MEDIUM_SERVICE));
  }
  EXPECT_TRUE(FromMachineDirHelper(COMMANDLINE_MODE_UNINSTALL));
  EXPECT_TRUE(FromMachineDirHelper(COMMANDLINE_MODE_PING));
  EXPECT_TRUE(FromMachineDirHelper(COMMANDLINE_MODE_HEALTH_CHECK));
  EXPECT_TRUE(FromMachineDirHelper(
      static_cast<CommandLineMode>(kLastMode + 1)));
}

TEST_F(GoopdateIsMachineProcessTest, IsMachineProcess_IsLocalSystemOnly) {
  EXPECT_FALSE(IsLocalSystemHelper(COMMANDLINE_MODE_UNKNOWN));
  EXPECT_FALSE(IsLocalSystemHelper(COMMANDLINE_MODE_NOARGS));
  EXPECT_TRUE(IsLocalSystemHelper(COMMANDLINE_MODE_CORE));
  EXPECT_TRUE(IsLocalSystemHelper(COMMANDLINE_MODE_SERVICE));
  EXPECT_FALSE(IsLocalSystemHelper(COMMANDLINE_MODE_REGSERVER));
  EXPECT_FALSE(IsLocalSystemHelper(COMMANDLINE_MODE_UNREGSERVER));
  EXPECT_FALSE(IsLocalSystemHelper(COMMANDLINE_MODE_CRASH));
  EXPECT_FALSE(IsLocalSystemHelper(COMMANDLINE_MODE_REPORTCRASH));
  {
    ExpectAsserts expect_asserts;
    EXPECT_FALSE(IsLocalSystemHelper(COMMANDLINE_MODE_INSTALL));
  }
  EXPECT_TRUE(IsLocalSystemHelper(COMMANDLINE_MODE_UPDATE));
  {
    ExpectAsserts expect_asserts;
    EXPECT_FALSE(IsLocalSystemHelper(COMMANDLINE_MODE_HANDOFF_INSTALL));
  }
  EXPECT_TRUE(IsLocalSystemHelper(COMMANDLINE_MODE_UA));
  EXPECT_FALSE(IsLocalSystemHelper(COMMANDLINE_MODE_RECOVER));
  EXPECT_TRUE(IsLocalSystemHelper(COMMANDLINE_MODE_CODE_RED_CHECK));
  EXPECT_FALSE(IsLocalSystemHelper(COMMANDLINE_MODE_COMSERVER));
  {
    ExpectAsserts expect_asserts;
    EXPECT_FALSE(IsLocalSystemHelper(COMMANDLINE_MODE_REGISTER_PRODUCT));
  }
  {
    ExpectAsserts expect_asserts;
    EXPECT_FALSE(IsLocalSystemHelper(COMMANDLINE_MODE_UNREGISTER_PRODUCT));
  }
  EXPECT_TRUE(IsLocalSystemHelper(COMMANDLINE_MODE_SERVICE_REGISTER));
  EXPECT_TRUE(IsLocalSystemHelper(COMMANDLINE_MODE_SERVICE_UNREGISTER));
  EXPECT_TRUE(IsLocalSystemHelper(COMMANDLINE_MODE_CRASH_HANDLER));
  EXPECT_FALSE(IsLocalSystemHelper(COMMANDLINE_MODE_COMBROKER));
  EXPECT_FALSE(IsLocalSystemHelper(COMMANDLINE_MODE_ONDEMAND));
  EXPECT_TRUE(IsLocalSystemHelper(COMMANDLINE_MODE_MEDIUM_SERVICE));
  EXPECT_FALSE(IsLocalSystemHelper(COMMANDLINE_MODE_UNINSTALL));
  EXPECT_FALSE(IsLocalSystemHelper(COMMANDLINE_MODE_PING));
  EXPECT_FALSE(IsLocalSystemHelper(COMMANDLINE_MODE_HEALTH_CHECK));
  EXPECT_FALSE(IsLocalSystemHelper(
      static_cast<CommandLineMode>(kLastMode + 1)));
}

TEST_F(GoopdateIsMachineProcessTest, IsMachineProcess_MachineOverrideOnly) {
  EXPECT_FALSE(MachineOverrideHelper(COMMANDLINE_MODE_UNKNOWN));
  EXPECT_FALSE(MachineOverrideHelper(COMMANDLINE_MODE_NOARGS));
  EXPECT_FALSE(MachineOverrideHelper(COMMANDLINE_MODE_CORE));
  {
    ExpectAsserts expect_asserts;
    EXPECT_FALSE(MachineOverrideHelper(COMMANDLINE_MODE_SERVICE));
  }
  EXPECT_FALSE(MachineOverrideHelper(COMMANDLINE_MODE_REGSERVER));
  EXPECT_FALSE(MachineOverrideHelper(COMMANDLINE_MODE_UNREGSERVER));
  EXPECT_FALSE(MachineOverrideHelper(COMMANDLINE_MODE_CRASH));
  EXPECT_TRUE(MachineOverrideHelper(COMMANDLINE_MODE_REPORTCRASH));
  {
    ExpectAsserts expect_asserts;
    EXPECT_FALSE(MachineOverrideHelper(COMMANDLINE_MODE_INSTALL));
  }
  EXPECT_FALSE(MachineOverrideHelper(COMMANDLINE_MODE_UPDATE));
  {
    ExpectAsserts expect_asserts;
    EXPECT_FALSE(MachineOverrideHelper(COMMANDLINE_MODE_HANDOFF_INSTALL));
  }
  EXPECT_TRUE(MachineOverrideHelper(COMMANDLINE_MODE_UA));
  EXPECT_TRUE(MachineOverrideHelper(COMMANDLINE_MODE_RECOVER));
  EXPECT_FALSE(MachineOverrideHelper(COMMANDLINE_MODE_CODE_RED_CHECK));
  EXPECT_FALSE(MachineOverrideHelper(COMMANDLINE_MODE_COMSERVER));
  {
    ExpectAsserts expect_asserts;
    EXPECT_FALSE(MachineOverrideHelper(COMMANDLINE_MODE_REGISTER_PRODUCT));
  }
  {
    ExpectAsserts expect_asserts;
    EXPECT_FALSE(MachineOverrideHelper(COMMANDLINE_MODE_UNREGISTER_PRODUCT));
  }
  EXPECT_TRUE(MachineOverrideHelper(COMMANDLINE_MODE_SERVICE_REGISTER));
  EXPECT_TRUE(MachineOverrideHelper(COMMANDLINE_MODE_SERVICE_UNREGISTER));
  EXPECT_FALSE(MachineOverrideHelper(COMMANDLINE_MODE_CRASH_HANDLER));
  EXPECT_FALSE(MachineOverrideHelper(COMMANDLINE_MODE_COMBROKER));
  EXPECT_FALSE(MachineOverrideHelper(COMMANDLINE_MODE_ONDEMAND));
  {
    ExpectAsserts expect_asserts;
    EXPECT_FALSE(MachineOverrideHelper(COMMANDLINE_MODE_MEDIUM_SERVICE));
  }
  EXPECT_FALSE(MachineOverrideHelper(COMMANDLINE_MODE_UNINSTALL));
  EXPECT_FALSE(MachineOverrideHelper(COMMANDLINE_MODE_PING));
  EXPECT_FALSE(MachineOverrideHelper(COMMANDLINE_MODE_HEALTH_CHECK));
  EXPECT_FALSE(MachineOverrideHelper(
      static_cast<CommandLineMode>(kLastMode + 1)));
}

TEST_F(GoopdateIsMachineProcessTest, IsMachineProcess_NeedsAdminFalseOnly) {
  EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_UNKNOWN));
  EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_NOARGS));
  EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_CORE));
  {
    ExpectAsserts expect_asserts;
    EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_SERVICE));
  }
  EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_REGSERVER));
  EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_UNREGSERVER));
  EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_CRASH));
  EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_REPORTCRASH));
  EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_INSTALL));
  EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_UPDATE));
  EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_HANDOFF_INSTALL));
  EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_UA));
  EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_RECOVER));
  EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_CODE_RED_CHECK));
  EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_COMSERVER));
  EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_REGISTER_PRODUCT));
  EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_UNREGISTER_PRODUCT));
  EXPECT_TRUE(NeedsAdminFalseHelper(COMMANDLINE_MODE_SERVICE_REGISTER));
  EXPECT_TRUE(NeedsAdminFalseHelper(COMMANDLINE_MODE_SERVICE_UNREGISTER));
  EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_CRASH_HANDLER));
  EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_COMBROKER));
  EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_ONDEMAND));
  {
    ExpectAsserts expect_asserts;
    EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_MEDIUM_SERVICE));
  }
  EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_UNINSTALL));
  EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_PING));
  EXPECT_FALSE(NeedsAdminFalseHelper(COMMANDLINE_MODE_HEALTH_CHECK));
  EXPECT_FALSE(NeedsAdminFalseHelper(
      static_cast<CommandLineMode>(kLastMode + 1)));
}

TEST_F(GoopdateIsMachineProcessTest, IsMachineProcess_NeedsAdminTrueOnly) {
  EXPECT_FALSE(NeedsAdminTrueHelper(COMMANDLINE_MODE_UNKNOWN));
  EXPECT_FALSE(NeedsAdminTrueHelper(COMMANDLINE_MODE_NOARGS));
  EXPECT_FALSE(NeedsAdminTrueHelper(COMMANDLINE_MODE_CORE));
  {
    ExpectAsserts expect_asserts;
    EXPECT_FALSE(NeedsAdminTrueHelper(COMMANDLINE_MODE_SERVICE));
  }
  EXPECT_FALSE(NeedsAdminTrueHelper(COMMANDLINE_MODE_REGSERVER));
  EXPECT_FALSE(NeedsAdminTrueHelper(COMMANDLINE_MODE_UNREGSERVER));
  EXPECT_FALSE(NeedsAdminTrueHelper(COMMANDLINE_MODE_CRASH));
  EXPECT_FALSE(NeedsAdminTrueHelper(COMMANDLINE_MODE_REPORTCRASH));
  EXPECT_TRUE(NeedsAdminTrueHelper(COMMANDLINE_MODE_INSTALL));
  EXPECT_FALSE(NeedsAdminTrueHelper(COMMANDLINE_MODE_UPDATE));
  EXPECT_TRUE(NeedsAdminTrueHelper(COMMANDLINE_MODE_HANDOFF_INSTALL));
  EXPECT_FALSE(NeedsAdminTrueHelper(COMMANDLINE_MODE_UA));
  EXPECT_FALSE(NeedsAdminTrueHelper(COMMANDLINE_MODE_RECOVER));
  EXPECT_FALSE(NeedsAdminTrueHelper(COMMANDLINE_MODE_CODE_RED_CHECK));
  EXPECT_FALSE(NeedsAdminTrueHelper(COMMANDLINE_MODE_COMSERVER));
  EXPECT_TRUE(NeedsAdminTrueHelper(COMMANDLINE_MODE_REGISTER_PRODUCT));
  EXPECT_TRUE(NeedsAdminTrueHelper(COMMANDLINE_MODE_UNREGISTER_PRODUCT));
  EXPECT_TRUE(NeedsAdminTrueHelper(COMMANDLINE_MODE_SERVICE_REGISTER));
  EXPECT_TRUE(NeedsAdminTrueHelper(COMMANDLINE_MODE_SERVICE_UNREGISTER));
  EXPECT_FALSE(NeedsAdminTrueHelper(COMMANDLINE_MODE_CRASH_HANDLER));
  EXPECT_FALSE(NeedsAdminTrueHelper(COMMANDLINE_MODE_COMBROKER));
  EXPECT_FALSE(NeedsAdminTrueHelper(COMMANDLINE_MODE_ONDEMAND));
  {
    ExpectAsserts expect_asserts;
    EXPECT_FALSE(NeedsAdminTrueHelper(COMMANDLINE_MODE_MEDIUM_SERVICE));
  }
  EXPECT_FALSE(NeedsAdminTrueHelper(COMMANDLINE_MODE_UNINSTALL));
  EXPECT_FALSE(NeedsAdminTrueHelper(COMMANDLINE_MODE_PING));
  EXPECT_FALSE(NeedsAdminTrueHelper(COMMANDLINE_MODE_HEALTH_CHECK));
  EXPECT_FALSE(NeedsAdminTrueHelper(
      static_cast<CommandLineMode>(kLastMode + 1)));
}

// Tests all modes plus an undefined one.
TEST(GoopdateTest, CanDisplayUi_NotSilent) {
  for (int mode = 0; mode <= kLastMode + 1; ++mode) {
    const bool kExpected = mode == COMMANDLINE_MODE_UNKNOWN ||
                           mode == COMMANDLINE_MODE_INSTALL ||
                           mode == COMMANDLINE_MODE_HANDOFF_INSTALL ||
                           mode == COMMANDLINE_MODE_UA;

    EXPECT_EQ(kExpected,
              internal::CanDisplayUi(static_cast<CommandLineMode>(mode),
                                     false));
  }
}

TEST(GoopdateTest, CanDisplayUi_Silent) {
  int mode = 0;
  // These two modes always return true.
  for (; mode <= COMMANDLINE_MODE_UNKNOWN; ++mode) {
    EXPECT_TRUE(internal::CanDisplayUi(static_cast<CommandLineMode>(mode),
                                       true));
  }

  // Tests the remaining modes plus an undefined one.
  for (; mode <= kLastMode + 1; ++mode) {
    EXPECT_FALSE(internal::CanDisplayUi(static_cast<CommandLineMode>(mode),
                                        true));
  }
}

}  // namespace omaha
