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

#include "omaha/common/constants.h"
#include "omaha/common/reg_key.h"
#include "omaha/goopdate/goopdate-internal.h"
#include "omaha/testing/unit_test.h"

namespace {

const TCHAR* const kAppMachineClientStatePath =
    _T("HKLM\\Software\\Google\\Update\\ClientState\\")
    _T("{19BE47E4-CF32-48c1-94C4-046507F6A8A6}\\");
const TCHAR* const kApp2MachineClientStatePath =
    _T("HKLM\\Software\\Google\\Update\\ClientState\\")
    _T("{553B2D8C-E6A7-43ed-ACC9-A8BA5D34395F}\\");
const TCHAR* const kAppUserClientStatePath =
    _T("HKCU\\Software\\Google\\Update\\ClientState\\")
    _T("{19BE47E4-CF32-48c1-94C4-046507F6A8A6}\\");
const TCHAR* const kApp2UserClientStatePath =
    _T("HKCU\\Software\\Google\\Update\\ClientState\\")
    _T("{553B2D8C-E6A7-43ed-ACC9-A8BA5D34395F}\\");

const TCHAR* const kAppMachineClientStateMediumPath =
    _T("HKLM\\Software\\Google\\Update\\ClientStateMedium\\")
    _T("{19BE47E4-CF32-48c1-94C4-046507F6A8A6}\\");

}  // namespace

namespace omaha {

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
      _T("HKCU\\Software\\Google\\Update\\ClientStateMedium\\")
      _T("{19BE47E4-CF32-48c1-94C4-046507F6A8A6}\\");

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

}  // namespace omaha
