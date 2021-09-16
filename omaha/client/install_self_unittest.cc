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

#include "omaha/base/constants.h"
#include "omaha/base/reg_key.h"
#include "omaha/client/install_self.h"
#include "omaha/client/install_self_internal.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace install_self {

namespace {

const TCHAR* const kAppMachineClientStatePath =
    _T("HKLM\\Software\\") PATH_COMPANY_NAME _T("\\") PRODUCT_NAME
    _T("\\ClientState\\{50DA5C89-FF97-4536-BF3F-DF54C2F02EA8}\\");

const TCHAR* const kAppUserClientStatePath =
    _T("HKCU\\Software\\") PATH_COMPANY_NAME _T("\\") PRODUCT_NAME
    _T("\\ClientState\\{50DA5C89-FF97-4536-BF3F-DF54C2F02EA8}\\");

}  // namespace

class RegistryProtectedInstallSelfTest
    : public RegistryProtectedTest {
};

TEST(InstallTest, CheckSystemRequirements) {
  EXPECT_SUCCEEDED(internal::CheckSystemRequirements());
}

TEST(InstallTest, HasXmlParser_True) {
  EXPECT_TRUE(internal::HasXmlParser());
}

// A few tests for the public method. The bulk of the cases are covered by
// SetEulaRequiredState tests.
TEST_F(RegistryProtectedInstallSelfTest,
       SetEulaAccepted_Machine_KeyDoesNotExist) {
  EXPECT_EQ(S_OK, SetEulaAccepted(true));
  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(RegistryProtectedInstallSelfTest,
       SetEulaAccepted_Machine_ValueDoesNotExist) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(MACHINE_REG_UPDATE));
  EXPECT_EQ(S_FALSE, SetEulaAccepted(true));
  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(RegistryProtectedInstallSelfTest, SetEulaAccepted_Machine_ExistsZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_CLIENT_STATE_GOOPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));

  EXPECT_SUCCEEDED(SetEulaAccepted(true));
  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, _T("eulaaccepted")));

  // ClientState for Google Update (never used) and other apps is not affected.
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(RegKey::GetValue(MACHINE_REG_CLIENT_STATE_GOOPDATE,
                   _T("eulaaccepted"),
                   &value));
  EXPECT_EQ(0, value);
  value = UINT_MAX;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                   _T("eulaaccepted"),
                   &value));
  EXPECT_EQ(0, value);
}

TEST_F(RegistryProtectedInstallSelfTest, SetEulaAccepted_User_KeyDoesNotExist) {
  EXPECT_EQ(S_OK, SetEulaAccepted(false));
  EXPECT_FALSE(RegKey::HasValue(USER_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(RegistryProtectedInstallSelfTest,
       SetEulaAccepted_User_ValueDoesNotExist) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(USER_REG_UPDATE));
  EXPECT_EQ(S_FALSE, SetEulaAccepted(false));
  EXPECT_FALSE(RegKey::HasValue(USER_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(RegistryProtectedInstallSelfTest, SetEulaAccepted_User_ExistsZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));

  EXPECT_SUCCEEDED(SetEulaAccepted(false));
  EXPECT_FALSE(RegKey::HasValue(USER_REG_UPDATE, _T("eulaaccepted")));

  // ClientState for Google Update (never used) and other apps is not affected.
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(RegKey::GetValue(USER_REG_CLIENT_STATE_GOOPDATE,
                   _T("eulaaccepted"),
                   &value));
  EXPECT_EQ(0, value);
  value = UINT_MAX;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppUserClientStatePath,
                   _T("eulaaccepted"),
                   &value));
  EXPECT_EQ(0, value);
}

// TODO(omaha3): Enable once the EULA support is finalized. The tests were
// significantly changed in the mainline.
#if 0
TEST_F(SetupRegistryProtectedMachineTest,
       SetEulaRequiredState_NotRequired_KeyDoesNotExist) {
  SetModeInstall();
  args_.is_eula_required_set = false;
  EXPECT_EQ(S_OK, SetEulaRequiredState());
  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(SetupRegistryProtectedMachineTest,
       SetEulaRequiredState_NotRequired_ValueDoesNotExist) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(MACHINE_REG_UPDATE));
  SetModeInstall();
  args_.is_eula_required_set = false;
  EXPECT_SUCCEEDED(SetEulaRequiredState());
  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(SetupRegistryProtectedMachineTest,
       SetEulaRequiredState_NotRequired_ExistsZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_CLIENT_STATE_GOOPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));

  SetModeInstall();
  args_.is_eula_required_set = false;
  EXPECT_SUCCEEDED(SetEulaRequiredState());
  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, _T("eulaaccepted")));

  // ClientState for Google Update (never used) and other apps is not affected.
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(RegKey::GetValue(MACHINE_REG_CLIENT_STATE_GOOPDATE,
                   _T("eulaaccepted"),
                   &value));
  EXPECT_EQ(0, value);
  value = UINT_MAX;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                   _T("eulaaccepted"),
                   &value));
  EXPECT_EQ(0, value);
}

TEST_F(SetupRegistryProtectedMachineTest,
       SetEulaRequiredState_NotRequired_ExistsOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  SetModeInstall();
  args_.is_eula_required_set = false;
  EXPECT_SUCCEEDED(SetEulaRequiredState());
  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(SetupRegistryProtectedMachineTest,
       SetEulaRequiredState_NotRequired_ExistsOther) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(8000)));
  SetModeInstall();
  args_.is_eula_required_set = false;
  EXPECT_SUCCEEDED(SetEulaRequiredState());
  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(SetupRegistryProtectedMachineTest,
       SetEulaRequiredState_NotRequired_ExistsString) {
  EXPECT_SUCCEEDED(
      RegKey::SetValue(MACHINE_REG_UPDATE, _T("eulaaccepted"), _T("0")));
  SetModeInstall();
  args_.is_eula_required_set = false;
  EXPECT_SUCCEEDED(SetEulaRequiredState());
  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(SetupRegistryProtectedMachineTest,
       SetEulaRequiredState_NotRequired_ValueDoesNotExistAlreadyInstalled) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("pv"),
                                    _T("1.2.3.4")));

  SetModeInstall();
  args_.is_eula_required_set = false;
  EXPECT_SUCCEEDED(SetEulaRequiredState());
  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(SetupRegistryProtectedMachineTest,
       SetEulaRequiredState_NotRequired_ExistsZeroAlreadyInstalled) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("pv"),
                                    _T("1.2.3.4")));

  SetModeInstall();
  args_.is_eula_required_set = false;
  EXPECT_SUCCEEDED(SetEulaRequiredState());
  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(SetupRegistryProtectedMachineTest,
       SetEulaRequiredState_Required_DoesNotExist) {
  SetModeInstall();
  args_.is_eula_required_set = true;
  EXPECT_SUCCEEDED(SetEulaRequiredState());
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(MACHINE_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
}

TEST_F(SetupRegistryProtectedMachineTest,
       SetEulaRequiredState_Required_ExistsZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_CLIENT_STATE_GOOPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));

  SetModeInstall();
  args_.is_eula_required_set = true;
  EXPECT_SUCCEEDED(SetEulaRequiredState());
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(MACHINE_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);

  // ClientState for Google Update (never used) and other apps is not affected.
  value = UINT_MAX;
  EXPECT_SUCCEEDED(RegKey::GetValue(MACHINE_REG_CLIENT_STATE_GOOPDATE,
                   _T("eulaaccepted"),
                   &value));
  EXPECT_EQ(0, value);
  value = UINT_MAX;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppMachineClientStatePath,
                   _T("eulaaccepted"),
                   &value));
  EXPECT_EQ(0, value);
}

// The existing value is ignored if there are not two registered apps. This is
// an artifact of the implementation and not a requirement.
TEST_F(SetupRegistryProtectedMachineTest,
       SetEulaRequiredState_Required_ExistsOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  SetModeInstall();
  args_.is_eula_required_set = true;
  EXPECT_SUCCEEDED(SetEulaRequiredState());
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(MACHINE_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
}

TEST_F(SetupRegistryProtectedMachineTest,
       SetEulaRequiredState_Required_ExistsOther) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(8000)));
  SetModeInstall();
  args_.is_eula_required_set = true;
  EXPECT_SUCCEEDED(SetEulaRequiredState());
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(MACHINE_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
}

TEST_F(SetupRegistryProtectedMachineTest,
       SetEulaRequiredState_Required_ExistsString) {
  EXPECT_SUCCEEDED(
      RegKey::SetValue(MACHINE_REG_UPDATE, _T("eulaaccepted"), _T("0")));
  SetModeInstall();
  args_.is_eula_required_set = true;
  EXPECT_SUCCEEDED(SetEulaRequiredState());
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(MACHINE_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
}

// One app is not sufficient for detecting that Google Update is already
// installed.
TEST_F(SetupRegistryProtectedMachineTest,
       SetEulaRequiredState_Required_ValueDoesNotExistOneAppRegistered) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientsPath,
                                    _T("pv"),
                                    _T("1.2.3.4")));

  SetModeInstall();
  args_.is_eula_required_set = true;
  EXPECT_SUCCEEDED(SetEulaRequiredState());
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(MACHINE_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
}

// Even Google Update registered is not sufficient for detecting that Google
// Update is already installed.
TEST_F(SetupRegistryProtectedMachineTest,
       SetEulaRequiredState_Required_ValueDoesNotExistGoogleUpdateRegistered) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_CLIENT_STATE,
                                    _T("pv"),
                                    _T("1.2.3.4")));

  SetModeInstall();
  args_.is_eula_required_set = true;
  EXPECT_SUCCEEDED(SetEulaRequiredState());
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(MACHINE_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
}

// Important: The existing state is not changed because two apps are registered.
TEST_F(SetupRegistryProtectedMachineTest,
       SetEulaRequiredState_Required_ValueDoesNotExistTwoAppsRegistered) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientsPath,
                                    _T("pv"),
                                    _T("1.2.3.4")));
  EXPECT_SUCCEEDED(RegKey::SetValue(kApp2MachineClientsPath,
                                    _T("pv"),
                                    _T("1.2.3.4")));

  SetModeInstall();
  args_.is_eula_required_set = true;
  EXPECT_SUCCEEDED(SetEulaRequiredState());
  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, _T("eulaaccepted")));
}

// The existing state is not changed because Google Update is already
// installed, but there is no way to differentiate this from writing 0.
TEST_F(SetupRegistryProtectedMachineTest,
       SetEulaRequiredState_Required_ExistsZeroTwoAppsRegistered) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientsPath,
                                    _T("pv"),
                                    _T("1.2.3.4")));
  EXPECT_SUCCEEDED(RegKey::SetValue(kApp2MachineClientsPath,
                                    _T("pv"),
                                    _T("1.2.3.4")));

  SetModeInstall();
  args_.is_eula_required_set = true;
  EXPECT_SUCCEEDED(SetEulaRequiredState());
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(MACHINE_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
}

// The existing state is not changed because Google Update is already installed.
TEST_F(SetupRegistryProtectedMachineTest,
       SetEulaRequiredState_Required_ExistsOneTwoAppsRegistered) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppMachineClientsPath,
                                    _T("pv"),
                                    _T("1.2.3.4")));
  EXPECT_SUCCEEDED(RegKey::SetValue(kApp2MachineClientsPath,
                                    _T("pv"),
                                    _T("1.2.3.4")));

  SetModeInstall();
  args_.is_eula_required_set = true;
  EXPECT_SUCCEEDED(SetEulaRequiredState());
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(MACHINE_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(1, value);
}

TEST_F(SetupRegistryProtectedUserTest,
       SetEulaRequiredState_NotRequired_KeyDoesNotExist) {
  SetModeInstall();
  args_.is_eula_required_set = false;
  EXPECT_EQ(S_OK, SetEulaRequiredState());
  EXPECT_FALSE(RegKey::HasValue(USER_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(SetupRegistryProtectedUserTest,
       SetEulaRequiredState_NotRequired_ValueDoesNotExist) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(USER_REG_UPDATE));
  SetModeInstall();
  args_.is_eula_required_set = false;
  EXPECT_SUCCEEDED(SetEulaRequiredState());
  EXPECT_FALSE(RegKey::HasValue(USER_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(SetupRegistryProtectedUserTest,
       SetEulaRequiredState_NotRequired_ExistsZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));

  SetModeInstall();
  args_.is_eula_required_set = false;
  EXPECT_SUCCEEDED(SetEulaRequiredState());
  EXPECT_FALSE(RegKey::HasValue(USER_REG_UPDATE, _T("eulaaccepted")));

  // ClientState for Google Update (never used) and other apps is not affected.
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(RegKey::GetValue(USER_REG_CLIENT_STATE_GOOPDATE,
                   _T("eulaaccepted"),
                   &value));
  EXPECT_EQ(0, value);
  value = UINT_MAX;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppUserClientStatePath,
                   _T("eulaaccepted"),
                   &value));
  EXPECT_EQ(0, value);
}

TEST_F(SetupRegistryProtectedUserTest,
       SetEulaRequiredState_NotRequired_ExistsOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  SetModeInstall();
  args_.is_eula_required_set = false;
  EXPECT_SUCCEEDED(SetEulaRequiredState());
  EXPECT_FALSE(RegKey::HasValue(USER_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(SetupRegistryProtectedUserTest,
       SetEulaRequiredState_NotRequired_ExistsOther) {
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(8000)));
  SetModeInstall();
  args_.is_eula_required_set = false;
  EXPECT_SUCCEEDED(SetEulaRequiredState());
  EXPECT_FALSE(RegKey::HasValue(USER_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(SetupRegistryProtectedUserTest,
       SetEulaRequiredState_NotRequired_ExistsString) {
  EXPECT_SUCCEEDED(
      RegKey::SetValue(USER_REG_UPDATE, _T("eulaaccepted"), _T("0")));
  SetModeInstall();
  args_.is_eula_required_set = false;
  EXPECT_SUCCEEDED(SetEulaRequiredState());
  EXPECT_FALSE(RegKey::HasValue(USER_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(SetupRegistryProtectedUserTest,
       SetEulaRequiredState_NotRequired_ValueDoesNotExistAlreadyInstalled) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("pv"),
                                    _T("1.2.3.4")));

  SetModeInstall();
  args_.is_eula_required_set = false;
  EXPECT_SUCCEEDED(SetEulaRequiredState());
  EXPECT_FALSE(RegKey::HasValue(USER_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(SetupRegistryProtectedUserTest,
       SetEulaRequiredState_NotRequired_ExistsZeroAlreadyInstalled) {
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("pv"),
                                    _T("1.2.3.4")));

  SetModeInstall();
  args_.is_eula_required_set = false;
  EXPECT_SUCCEEDED(SetEulaRequiredState());
  EXPECT_FALSE(RegKey::HasValue(USER_REG_UPDATE, _T("eulaaccepted")));
}

TEST_F(SetupRegistryProtectedUserTest,
       SetEulaRequiredState_Required_DoesNotExist) {
  SetModeInstall();
  args_.is_eula_required_set = true;
  EXPECT_SUCCEEDED(SetEulaRequiredState());
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(USER_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
}

TEST_F(SetupRegistryProtectedUserTest,
       SetEulaRequiredState_Required_ExistsZero) {
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientStatePath,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));

  SetModeInstall();
  args_.is_eula_required_set = true;
  EXPECT_SUCCEEDED(SetEulaRequiredState());
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(USER_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);

  // ClientState for Google Update (never used) and other apps is not affected.
  value = UINT_MAX;
  EXPECT_SUCCEEDED(RegKey::GetValue(USER_REG_CLIENT_STATE_GOOPDATE,
                   _T("eulaaccepted"),
                   &value));
  EXPECT_EQ(0, value);
  value = UINT_MAX;
  EXPECT_SUCCEEDED(RegKey::GetValue(kAppUserClientStatePath,
                   _T("eulaaccepted"),
                   &value));
  EXPECT_EQ(0, value);
}

// The existing value is ignored if there are not two registered apps. This is
// an artifact of the implementation and not a requirement.
TEST_F(SetupRegistryProtectedUserTest,
       SetEulaRequiredState_Required_ExistsOne) {
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  SetModeInstall();
  args_.is_eula_required_set = true;
  EXPECT_SUCCEEDED(SetEulaRequiredState());
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(USER_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
}

TEST_F(SetupRegistryProtectedUserTest,
       SetEulaRequiredState_Required_ExistsOther) {
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(8000)));
  SetModeInstall();
  args_.is_eula_required_set = true;
  EXPECT_SUCCEEDED(SetEulaRequiredState());
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(USER_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
}

TEST_F(SetupRegistryProtectedUserTest,
       SetEulaRequiredState_Required_ExistsString) {
  EXPECT_SUCCEEDED(
      RegKey::SetValue(USER_REG_UPDATE, _T("eulaaccepted"), _T("0")));
  SetModeInstall();
  args_.is_eula_required_set = true;
  EXPECT_SUCCEEDED(SetEulaRequiredState());
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(USER_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
}

// One app is not sufficient for detecting that Google Update is already
// installed.
TEST_F(SetupRegistryProtectedUserTest,
       SetEulaRequiredState_Required_ValueDoesNotExistOneAppRegistered) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientsPath,
                                    _T("pv"),
                                    _T("1.2.3.4")));

  SetModeInstall();
  args_.is_eula_required_set = true;
  EXPECT_SUCCEEDED(SetEulaRequiredState());
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(USER_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
}

// Even Google Update registered is not sufficient for detecting that Google
// Update is already installed.
TEST_F(SetupRegistryProtectedUserTest,
       SetEulaRequiredState_Required_ValueDoesNotExistGoogleUpdateRegistered) {
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENT_STATE,
                                    _T("pv"),
                                    _T("1.2.3.4")));

  SetModeInstall();
  args_.is_eula_required_set = true;
  EXPECT_SUCCEEDED(SetEulaRequiredState());
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(USER_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
}

// Important: The existing state is not changed because two apps are registered.
TEST_F(SetupRegistryProtectedUserTest,
       SetEulaRequiredState_Required_ValueDoesNotExistTwoAppsRegistered) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientsPath,
                                    _T("pv"),
                                    _T("1.2.3.4")));
  EXPECT_SUCCEEDED(RegKey::SetValue(kApp2UserClientsPath,
                                    _T("pv"),
                                    _T("1.2.3.4")));

  SetModeInstall();
  args_.is_eula_required_set = true;
  EXPECT_SUCCEEDED(SetEulaRequiredState());
  EXPECT_FALSE(RegKey::HasValue(USER_REG_UPDATE, _T("eulaaccepted")));
}

// The existing state is not changed because Google Update is already
// installed, but there is no way to differentiate this from writing 0.
TEST_F(SetupRegistryProtectedUserTest,
       SetEulaRequiredState_Required_ExistsZeroTwoAppsRegistered) {
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientsPath,
                                    _T("pv"),
                                    _T("1.2.3.4")));
  EXPECT_SUCCEEDED(RegKey::SetValue(kApp2UserClientsPath,
                                    _T("pv"),
                                    _T("1.2.3.4")));

  SetModeInstall();
  args_.is_eula_required_set = true;
  EXPECT_SUCCEEDED(SetEulaRequiredState());
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(USER_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(0, value);
}

// The existing state is not changed because Google Update is already installed.
TEST_F(SetupRegistryProtectedUserTest,
       SetEulaRequiredState_Required_ExistsOneTwoAppsRegistered) {
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppUserClientsPath,
                                    _T("pv"),
                                    _T("1.2.3.4")));
  EXPECT_SUCCEEDED(RegKey::SetValue(kApp2UserClientsPath,
                                    _T("pv"),
                                    _T("1.2.3.4")));

  SetModeInstall();
  args_.is_eula_required_set = true;
  EXPECT_SUCCEEDED(SetEulaRequiredState());
  DWORD value = UINT_MAX;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(USER_REG_UPDATE, _T("eulaaccepted"), &value));
  EXPECT_EQ(1, value);
}
#endif

TEST_F(RegistryProtectedInstallSelfTest, PersistUpdateErrorInfo_User) {
  internal::PersistUpdateErrorInfo(false, 0x98765432, 77, _T("1.2.3.4"));

  DWORD value(0);
  EXPECT_SUCCEEDED(RegKey::GetValue(USER_REG_UPDATE,
                                    kRegValueSelfUpdateErrorCode,
                                    &value));
  EXPECT_EQ(0x98765432, value);

  EXPECT_SUCCEEDED(RegKey::GetValue(USER_REG_UPDATE,
                                    kRegValueSelfUpdateExtraCode1,
                                    &value));
  EXPECT_EQ(77, value);

  CString version;
  EXPECT_SUCCEEDED(RegKey::GetValue(USER_REG_UPDATE,
                                    kRegValueSelfUpdateVersion,
                                    &version));
  EXPECT_FALSE(version.IsEmpty());
  EXPECT_STREQ(_T("1.2.3.4"), version);
}

TEST_F(RegistryProtectedInstallSelfTest, PersistUpdateErrorInfo_Machine) {
  internal::PersistUpdateErrorInfo(true, 0x98765430, 0x12345678, _T("2.3.4.5"));

  DWORD value(0);
  EXPECT_SUCCEEDED(RegKey::GetValue(MACHINE_REG_UPDATE,
                                    kRegValueSelfUpdateErrorCode,
                                    &value));
  EXPECT_EQ(0x98765430, value);

  EXPECT_SUCCEEDED(RegKey::GetValue(MACHINE_REG_UPDATE,
                                    kRegValueSelfUpdateExtraCode1,
                                    &value));
  EXPECT_EQ(0x12345678, value);

  CString version;
  EXPECT_SUCCEEDED(RegKey::GetValue(MACHINE_REG_UPDATE,
                                    kRegValueSelfUpdateVersion,
                                    &version));
  EXPECT_FALSE(version.IsEmpty());
  EXPECT_STREQ(_T("2.3.4.5"), version);
}

TEST_F(RegistryProtectedInstallSelfTest,
       ReadAndClearUpdateErrorInfo_User_KeyDoesNotExist) {
  DWORD self_update_error_code(0);
  DWORD self_update_extra_code1(0);
  CString self_update_version;
  ExpectAsserts expect_asserts;
  EXPECT_FALSE(ReadAndClearUpdateErrorInfo(false,
                                           &self_update_error_code,
                                           &self_update_extra_code1,
                                           &self_update_version));
}

TEST_F(RegistryProtectedInstallSelfTest,
       ReadAndClearUpdateErrorInfo_Machine_KeyDoesNotExist) {
  DWORD self_update_error_code(0);
  DWORD self_update_extra_code1(0);
  CString self_update_version;
  ExpectAsserts expect_asserts;
  EXPECT_FALSE(ReadAndClearUpdateErrorInfo(true,
                                           &self_update_error_code,
                                           &self_update_extra_code1,
                                           &self_update_version));
}

TEST_F(RegistryProtectedInstallSelfTest,
       ReadAndClearUpdateErrorInfo_User_UpdateErrorCodeDoesNotExist) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(USER_REG_UPDATE));
  DWORD self_update_error_code(0);
  DWORD self_update_extra_code1(0);
  CString self_update_version;
  EXPECT_FALSE(ReadAndClearUpdateErrorInfo(false,
                                           &self_update_error_code,
                                           &self_update_extra_code1,
                                           &self_update_version));
}

TEST_F(RegistryProtectedInstallSelfTest,
       ReadAndClearUpdateErrorInfo_Machine_UpdateErrorCodeDoesNotExist) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(MACHINE_REG_UPDATE));
  DWORD self_update_error_code(0);
  DWORD self_update_extra_code1(0);
  CString self_update_version;
  EXPECT_FALSE(ReadAndClearUpdateErrorInfo(true,
                                           &self_update_error_code,
                                           &self_update_extra_code1,
                                           &self_update_version));
}

TEST_F(RegistryProtectedInstallSelfTest,
       ReadAndClearUpdateErrorInfo_User_AllValuesPresent) {
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    kRegValueSelfUpdateErrorCode,
                                    static_cast<DWORD>(0x87654321)));
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    kRegValueSelfUpdateExtraCode1,
                                    static_cast<DWORD>(55)));
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    kRegValueSelfUpdateVersion,
                                    _T("0.2.4.8")));

  DWORD self_update_error_code(0);
  DWORD self_update_extra_code1(0);
  CString self_update_version;
  EXPECT_TRUE(ReadAndClearUpdateErrorInfo(false,
                                          &self_update_error_code,
                                          &self_update_extra_code1,
                                          &self_update_version));

  EXPECT_EQ(0x87654321, self_update_error_code);
  EXPECT_EQ(55, self_update_extra_code1);
  EXPECT_STREQ(_T("0.2.4.8"), self_update_version);
}

TEST_F(RegistryProtectedInstallSelfTest,
       ReadAndClearUpdateErrorInfo_Machine_AllValuesPresent) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    kRegValueSelfUpdateErrorCode,
                                    static_cast<DWORD>(0x87654321)));
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    kRegValueSelfUpdateExtraCode1,
                                    static_cast<DWORD>(55)));
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    kRegValueSelfUpdateVersion,
                                    _T("0.2.4.8")));

  DWORD self_update_error_code(0);
  DWORD self_update_extra_code1(0);
  CString self_update_version;
  EXPECT_TRUE(ReadAndClearUpdateErrorInfo(true,
                                          &self_update_error_code,
                                          &self_update_extra_code1,
                                          &self_update_version));

  EXPECT_EQ(0x87654321, self_update_error_code);
  EXPECT_EQ(55, self_update_extra_code1);
  EXPECT_STREQ(_T("0.2.4.8"), self_update_version);
}

TEST_F(RegistryProtectedInstallSelfTest,
       ReadAndClearUpdateErrorInfo_User_ValuesPresentInMachineOnly) {
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    kRegValueSelfUpdateErrorCode,
                                    static_cast<DWORD>(0x87654321)));
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    kRegValueSelfUpdateExtraCode1,
                                    static_cast<DWORD>(55)));
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    kRegValueSelfUpdateVersion,
                                    _T("0.2.4.8")));

  DWORD self_update_error_code(0);
  DWORD self_update_extra_code1(0);
  CString self_update_version;
  ExpectAsserts expect_asserts;
  EXPECT_FALSE(ReadAndClearUpdateErrorInfo(false,
                                           &self_update_error_code,
                                           &self_update_extra_code1,
                                           &self_update_version));
}

}  // namespace install_self

}  // namespace omaha
