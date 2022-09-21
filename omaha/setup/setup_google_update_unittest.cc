// Copyright 2007-2009 Google Inc.
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
#include "omaha/base/app_util.h"
#include "omaha/base/atlregmapex.h"
#include "omaha/base/constants.h"
#include "omaha/base/omaha_version.h"
#include "omaha/base/file.h"
#include "omaha/base/path.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/scoped_impersonation.h"
#include "omaha/base/time.h"
#include "omaha/base/utils.h"
#include "omaha/base/vista_utils.h"
#include "omaha/base/vistautil.h"
#include "omaha/common/command_line_builder.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/const_cmd_line.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/common/scheduled_task_utils.h"
#include "omaha/setup/setup_google_update.h"
#include "omaha/testing/unit_test.h"
#include "omaha/third_party/smartany/scoped_any.h"

namespace omaha {

namespace {

const TCHAR kRunKey[] =
    _T("HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run");

const TCHAR* const kAppId1 = _T("{B7E61EF9-AAE5-4cdf-A2D3-E4C8DF975145}");
const TCHAR* const kAppId2 = _T("{35F1A986-417D-4039-8718-781DD418232A}");

// Copies the shell and DLLs that FinishInstall needs.
// Does not replace files if they already exist.
void CopyFilesRequiredByFinishInstall(bool is_machine, const CString& version) {
  const CString omaha_path = is_machine ?
      GetGoogleUpdateMachinePath() : GetGoogleUpdateUserPath();
  const CString expected_shell_path =
      ConcatenatePath(omaha_path, kOmahaShellFileName);
  const CString version_path = ConcatenatePath(omaha_path, version);

  ASSERT_SUCCEEDED(CreateDir(omaha_path, NULL));
  ASSERT_SUCCEEDED(File::Copy(
      ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                      kOmahaShellFileName),
      expected_shell_path,
      false));

  ASSERT_SUCCEEDED(CreateDir(version_path, NULL));

  const TCHAR* files[] = {kOmahaDllName,
                          kOmahaCOMRegisterShell64,
                          kPSFileNameMachine,
                          kPSFileNameMachine64,
                          kPSFileNameUser,
                          kPSFileNameUser64};
  for (size_t i = 0; i < arraysize(files); ++i) {
    ASSERT_SUCCEEDED(File::Copy(
        ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                        files[i]),
        ConcatenatePath(version_path, files[i]),
        false));
  }
}

// RegisterOrUnregisterCOMLocalServer() called from FinishInstall() runs in a
// separate process. When using registry redirection in the test, the new
// process writes to the real registry, so the unit test fails. This function
// creates test entries that VerifyCOMLocalServerRegistration() verifies, and is
// happy about.
void SetupCOMLocalServerRegistration(bool is_machine) {
  // Setup the following for the unit test:
  // * LocalServer32 under CLSID_OnDemandMachineAppsClass or
  //   CLSID_OnDemandUserAppsClass should be the current exe's module path.
  // * InProcServer32 under CLSID of IID_IGoogleUpdate should be the path of the
  //   current module.
  // * ProxyStubClsid32 under IGoogleUpdate interface should be the CLSID of the
  //   proxy, which is PROXY_CLSID_IS.

  CString base_clsid_key(is_machine ? _T("HKLM") : _T("HKCU"));
  base_clsid_key += _T("\\Software\\Classes\\CLSID\\");
  CString ondemand_clsid_key(base_clsid_key);
// TODO(omaha3): Implement this for Omaha 3.
#if 0
  ondemand_clsid_key += GuidToString(is_machine ?
                                     __uuidof(OnDemandMachineAppsClass) :
                                     __uuidof(OnDemandUserAppsClass));
  CString local_server_key(ondemand_clsid_key + _T("\\LocalServer32"));
  CString expected_server(app_util::GetModulePath(NULL));
  EnclosePath(&expected_server);
  ASSERT_FALSE(expected_server.IsEmpty());
  ASSERT_SUCCEEDED(RegKey::SetValue(local_server_key,
                                    NULL,
                                    expected_server));

  const GUID proxy_clsid = PROXY_CLSID_IS;
  CString ondemand_proxy_clsid_key(base_clsid_key);
  ondemand_proxy_clsid_key += GuidToString(proxy_clsid);
  CString inproc_server_key(ondemand_proxy_clsid_key + _T("\\InProcServer32"));
  expected_server = app_util::GetCurrentModulePath();
  ASSERT_FALSE(expected_server.IsEmpty());
  ASSERT_SUCCEEDED(RegKey::SetValue(inproc_server_key,
                                    NULL,
                                    expected_server));

  CString igoogleupdate_interface_key(is_machine ? _T("HKLM") : _T("HKCU"));
  igoogleupdate_interface_key += _T("\\Software\\Classes\\Interface\\");
  igoogleupdate_interface_key += GuidToString(__uuidof(IGoogleUpdate));
  igoogleupdate_interface_key += _T("\\ProxyStubClsid32");
  CString proxy_interface_value(GuidToString(proxy_clsid));
  ASSERT_FALSE(proxy_interface_value.IsEmpty());
  ASSERT_SUCCEEDED(RegKey::SetValue(igoogleupdate_interface_key,
                                    NULL,
                                    proxy_interface_value));
#endif
}

void VerifyAccessRightsForTrustee(const CString& key_name,
                                  ACCESS_MASK expected_access_rights,
                                  CDacl* dacl,
                                  TRUSTEE* trustee) {
  CString fail_message;
  fail_message.Format(_T("Key: %s; Trustee: "), key_name);
  if (TRUSTEE_IS_NAME == trustee->TrusteeForm) {
    fail_message.Append(trustee->ptstrName);
  } else {
    EXPECT_TRUE(TRUSTEE_IS_SID == trustee->TrusteeForm);
    fail_message.Append(_T("is a SID"));
  }

  // Cast away the const so the pointer can be passed to the API. This should
  // be safe for these purposes and because this is a unit test.
  PACL pacl = const_cast<PACL>(dacl->GetPACL());
  ACCESS_MASK access_rights = 0;
  EXPECT_EQ(ERROR_SUCCESS, ::GetEffectiveRightsFromAcl(pacl,
                                                       trustee,
                                                       &access_rights)) <<
      fail_message.GetString();
  EXPECT_EQ(expected_access_rights, access_rights) << fail_message.GetString();
}

// TODO(omaha): It would be nice to test the access for a non-admin process.
// The test requires admin privileges to run, so this would likely require
// running a de-elevated process.
void VerifyHklmKeyHasIntegrity(
    const CString& key_name,
    ACCESS_MASK expected_non_admin_interactive_access) {

  const ACCESS_MASK kExpectedPowerUsersAccess = vista_util::IsVistaOrLater() ?
      0 : DELETE | READ_CONTROL | KEY_READ | KEY_WRITE;

  CDacl dacl;
  RegKey key;
  EXPECT_SUCCEEDED(key.Open(key_name));
  EXPECT_TRUE(::AtlGetDacl(key.Key(), SE_REGISTRY_KEY, &dacl));

  CString current_user_sid_string;
  EXPECT_SUCCEEDED(
      user_info::GetProcessUser(NULL, NULL, &current_user_sid_string));
  PSID current_user_sid = NULL;
  EXPECT_TRUE(ConvertStringSidToSid(current_user_sid_string,
                                    &current_user_sid));
  TRUSTEE current_user = {0};
  current_user.TrusteeForm = TRUSTEE_IS_SID;
  current_user.TrusteeType = TRUSTEE_IS_USER;
  current_user.ptstrName = static_cast<LPTSTR>(current_user_sid);
  VerifyAccessRightsForTrustee(key_name, KEY_ALL_ACCESS, &dacl, &current_user);
  EXPECT_FALSE(::LocalFree(current_user_sid));

  PSID local_system_sid = NULL;
  EXPECT_TRUE(ConvertStringSidToSid(kLocalSystemSid, &local_system_sid));
  TRUSTEE local_system = {0};
  local_system.TrusteeForm = TRUSTEE_IS_SID;
  local_system.TrusteeType = TRUSTEE_IS_USER;
  local_system.ptstrName = static_cast<LPTSTR>(local_system_sid);
  VerifyAccessRightsForTrustee(key_name, KEY_ALL_ACCESS, &dacl, &local_system);
  EXPECT_FALSE(::LocalFree(local_system_sid));

  TRUSTEE administrators = {0};
  administrators.TrusteeForm = TRUSTEE_IS_NAME;
  administrators.TrusteeType = TRUSTEE_IS_GROUP;
  administrators.ptstrName = const_cast<LPTSTR>(_T("Administrators"));
  VerifyAccessRightsForTrustee(key_name,
                               KEY_ALL_ACCESS,
                               &dacl,
                               &administrators);

  TRUSTEE users = {0};
  users.TrusteeForm = TRUSTEE_IS_NAME;
  users.TrusteeType = TRUSTEE_IS_GROUP;
  users.ptstrName = const_cast<LPTSTR>(_T("Users"));
  VerifyAccessRightsForTrustee(key_name, KEY_READ, &dacl, &users);

  TRUSTEE power_users = {0};
  power_users.TrusteeForm = TRUSTEE_IS_NAME;
  power_users.TrusteeType = TRUSTEE_IS_GROUP;
  power_users.ptstrName = const_cast<LPTSTR>(_T("Power Users"));
  VerifyAccessRightsForTrustee(key_name,
                               kExpectedPowerUsersAccess,
                               &dacl,
                               &power_users);

  TRUSTEE interactive = {0};
  interactive.TrusteeForm = TRUSTEE_IS_NAME;
  interactive.TrusteeType = TRUSTEE_IS_GROUP;
  interactive.ptstrName = const_cast<LPTSTR>(_T("INTERACTIVE"));
  VerifyAccessRightsForTrustee(key_name,
                               expected_non_admin_interactive_access,
                               &dacl,
                               &interactive);

  TRUSTEE everyone = {0};
  everyone.TrusteeForm = TRUSTEE_IS_NAME;
  everyone.TrusteeType = TRUSTEE_IS_GROUP;
  everyone.ptstrName = const_cast<LPTSTR>(_T("Everyone"));
  VerifyAccessRightsForTrustee(key_name, 0, &dacl, &everyone);

  TRUSTEE guest = {0};
  guest.TrusteeForm = TRUSTEE_IS_NAME;
  guest.TrusteeType = TRUSTEE_IS_USER;
  guest.ptstrName = const_cast<LPTSTR>(_T("Guest"));
  VerifyAccessRightsForTrustee(key_name, 0, &dacl, &guest);
}

}  // namespace

// INTERACTIVE group inherits privileges from Users in the default case.
void VerifyHklmKeyHasDefaultIntegrity(const CString& key_full_name) {
  VerifyHklmKeyHasIntegrity(key_full_name, KEY_READ);
}

void VerifyHklmKeyHasMediumIntegrity(const CString& key_full_name) {
  VerifyHklmKeyHasIntegrity(key_full_name,
                            KEY_READ | KEY_SET_VALUE | KEY_CREATE_SUB_KEY);
}

class SetupGoogleUpdateTest : public testing::Test {
 protected:
  explicit SetupGoogleUpdateTest(bool is_machine) : is_machine_(is_machine) {}
  virtual ~SetupGoogleUpdateTest() {}

  void SetUp() override {
    setup_google_update_.reset(new SetupGoogleUpdate(is_machine_, false));
  }

  HRESULT InstallRegistryValues() {
    return setup_google_update_->InstallRegistryValues();
  }

  HRESULT InstallLaunchMechanisms() {
    return setup_google_update_->InstallLaunchMechanisms();
  }

  void UninstallLaunchMechanisms() {
    setup_google_update_->UninstallLaunchMechanisms();
  }

  HRESULT CreateClientStateMedium() {
    return setup_google_update_->CreateClientStateMedium();
  }

  bool is_machine_;
  std::unique_ptr<SetupGoogleUpdate> setup_google_update_;
};

class SetupGoogleUpdateUserTest : public SetupGoogleUpdateTest {
 protected:
  SetupGoogleUpdateUserTest() : SetupGoogleUpdateTest(false) {
    CString expected_core_command_line = ConcatenatePath(
        ConcatenatePath(GetGoogleUpdateUserPath(), GetVersionString()),
        kOmahaCoreFileName);
    EnclosePath(&expected_core_command_line);
    expected_run_key_value_ = expected_core_command_line;
  }

  void SetUp() override {
    SetupGoogleUpdateTest::SetUp();
    RegKey::DeleteKey(USER_REG_UPDATE);
  }

  void TearDown() override {
    RegKey::DeleteKey(USER_REG_UPDATE);
    SetupGoogleUpdateTest::TearDown();
  }

  CString expected_run_key_value_;
};

class SetupGoogleUpdateMachineTest : public SetupGoogleUpdateTest {
 protected:
  SetupGoogleUpdateMachineTest() : SetupGoogleUpdateTest(true) {}

  void SetUp() override {
    RegKey::DeleteKey(MACHINE_REG_UPDATE);
    SetupGoogleUpdateTest::SetUp();
  }

  void TearDown() override {
    RegKey::DeleteKey(MACHINE_REG_UPDATE);
    SetupGoogleUpdateTest::TearDown();
  }
};

// This test uninstalls all other versions of Omaha.
TEST_F(SetupGoogleUpdateUserTest, FinishInstall_RunKeyDoesNotExist) {
  RegKey::DeleteValue(kRunKey, _T(OMAHA_APP_NAME_ANSI));
  ASSERT_FALSE(RegKey::HasValue(kRunKey, _T(OMAHA_APP_NAME_ANSI)));

  const bool had_pv = RegKey::HasValue(USER_REG_CLIENTS_GOOPDATE,
                                       kRegValueProductVersion);
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                    kRegValueProductVersion,
                                    GetVersionString()));

  CopyFilesRequiredByFinishInstall(is_machine_, GetVersionString());
  SetupCOMLocalServerRegistration(is_machine_);

  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    _T("mi"),
                                    _T("39980C99-CDD5-43A0-93C7-69D90C14729")));
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("mi"),
                                    _T("39980C99-CDD5-43A0-93C7-69D90C14729")));
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_UPDATE,
                                    _T("ui"),
                                    _T("49980C99-CDD5-43A0-93C7-69D90C14729")));

  EXPECT_SUCCEEDED(setup_google_update_->FinishInstall());

  // Check the system state.

  CPath expected_shell_path(GetGoogleUpdateUserPath());
  expected_shell_path.Append(kOmahaShellFileName);
  CString shell_path;
  EXPECT_SUCCEEDED(RegKey::GetValue(USER_REG_UPDATE, _T("path"), &shell_path));
  EXPECT_STREQ(expected_shell_path, shell_path);

  CString value;
  EXPECT_SUCCEEDED(RegKey::GetValue(kRunKey,
                                    _T(OMAHA_APP_NAME_ANSI),
                                    &value));
  EXPECT_STREQ(expected_run_key_value_, value);
  EXPECT_TRUE(scheduled_task_utils::IsInstalledGoopdateTaskUA(false));
  EXPECT_FALSE(scheduled_task_utils::IsDisabledGoopdateTaskUA(false));

  EXPECT_SUCCEEDED(
      RegKey::GetValue(USER_REG_UPDATE, kRegValueInstalledVersion, &value));
  EXPECT_EQ(GetVersionString(), value);
  EXPECT_FALSE(RegKey::HasValue(USER_REG_UPDATE, kRegValueLastChecked));

  EXPECT_TRUE(RegKey::HasValue(USER_REG_UPDATE, _T("mi")));
  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_UPDATE, _T("mi")));
  EXPECT_FALSE(RegKey::HasValue(USER_REG_UPDATE, _T("ui")));

  // TODO(omaha): Check for other state.

  // Clean up the launch mechanisms, at least one of which is not in the
  // overriding registry.
  UninstallLaunchMechanisms();
  EXPECT_FALSE(RegKey::HasValue(kRunKey, _T(OMAHA_APP_NAME_ANSI)));
  EXPECT_FALSE(scheduled_task_utils::IsInstalledGoopdateTaskUA(false));

  if (!had_pv) {
    // Delete the pv value. Some tests or shell .exe instances may see this
    // value and assume Omaha is correctly installed.
    EXPECT_SUCCEEDED(RegKey::DeleteValue(USER_REG_CLIENTS_GOOPDATE,
                                         kRegValueProductVersion));
  }
}

// TODO(omaha): Assumes GoogleUpdate.exe exists in the installed location, which
// is not always true when run independently.
TEST_F(SetupGoogleUpdateUserTest, InstallRegistryValues) {
  if (IsTestRunByLocalSystem()) {
    return;
  }

  RegKey::DeleteKey(USER_REG_UPDATE);
  RegKey::DeleteKey(MACHINE_REG_CLIENT_STATE_MEDIUM);

  EXPECT_SUCCEEDED(InstallRegistryValues());
  const uint32 now = Time64ToInt32(GetCurrent100NSTime());

  EXPECT_TRUE(RegKey::HasKey(USER_REG_GOOGLE));
  EXPECT_TRUE(RegKey::HasKey(USER_REG_UPDATE));
  EXPECT_TRUE(RegKey::HasKey(USER_REG_CLIENTS));
  EXPECT_TRUE(RegKey::HasKey(USER_REG_CLIENT_STATE));
  EXPECT_TRUE(RegKey::HasKey(USER_REG_CLIENTS_GOOPDATE));
  EXPECT_TRUE(RegKey::HasKey(USER_REG_CLIENT_STATE_GOOPDATE));
  EXPECT_FALSE(RegKey::HasKey(MACHINE_REG_CLIENT_STATE_MEDIUM));

  // Ensure no unexpected keys were created.
  RegKey update_key;
  EXPECT_SUCCEEDED(update_key.Open(USER_REG_UPDATE));
  EXPECT_EQ(2, update_key.GetSubkeyCount());

  RegKey clients_key;
  EXPECT_SUCCEEDED(clients_key.Open(USER_REG_CLIENTS));
  EXPECT_EQ(1, clients_key.GetSubkeyCount());

  RegKey client_state_key;
  EXPECT_SUCCEEDED(client_state_key.Open(USER_REG_CLIENT_STATE));
  EXPECT_EQ(1, client_state_key.GetSubkeyCount());

  CPath expected_shell_path(GetGoogleUpdateUserPath());
  expected_shell_path.Append(kOmahaShellFileName);
  CString shell_path;
  EXPECT_SUCCEEDED(RegKey::GetValue(USER_REG_UPDATE, _T("path"), &shell_path));
  EXPECT_STREQ(expected_shell_path, shell_path);

  CommandLineBuilder builder(COMMANDLINE_MODE_UNINSTALL);
  CString expected_uninstall_cmd_line =
      builder.GetCommandLine(expected_shell_path);
  CString uninstall_cmd_line;
  EXPECT_SUCCEEDED(RegKey::GetValue(USER_REG_UPDATE,
                                    kRegValueUninstallCmdLine,
                                    &uninstall_cmd_line));
  EXPECT_STREQ(expected_uninstall_cmd_line, uninstall_cmd_line);

  CString product_version;
  EXPECT_SUCCEEDED(RegKey::GetValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                    _T("pv"),
                                    &product_version));
  EXPECT_STREQ(GetVersionString(), product_version);
}

// TODO(omaha): Assumes GoogleUpdate.exe exists in the installed location, which
// is not always true when run independently.
// TODO(omaha): Fails when run by itself on Windows Vista.
TEST_F(SetupGoogleUpdateMachineTest, InstallRegistryValues) {
  EXPECT_SUCCEEDED(InstallRegistryValues());
  const uint32 now = Time64ToInt32(GetCurrent100NSTime());

  EXPECT_TRUE(RegKey::HasKey(MACHINE_REG_GOOGLE));
  EXPECT_TRUE(RegKey::HasKey(MACHINE_REG_UPDATE));
  EXPECT_TRUE(RegKey::HasKey(MACHINE_REG_CLIENTS));
  EXPECT_TRUE(RegKey::HasKey(MACHINE_REG_CLIENT_STATE));
  EXPECT_TRUE(RegKey::HasKey(MACHINE_REG_CLIENTS_GOOPDATE));
  EXPECT_TRUE(RegKey::HasKey(MACHINE_REG_CLIENT_STATE_GOOPDATE));
  EXPECT_TRUE(RegKey::HasKey(MACHINE_REG_CLIENT_STATE_MEDIUM));

  // Ensure no unexpected keys were created.
  RegKey update_key;
  EXPECT_SUCCEEDED(update_key.Open(MACHINE_REG_UPDATE));
  EXPECT_EQ(3, update_key.GetSubkeyCount());

  RegKey clients_key;
  EXPECT_SUCCEEDED(clients_key.Open(MACHINE_REG_CLIENTS));
  EXPECT_EQ(1, clients_key.GetSubkeyCount());

  RegKey client_state_key;
  EXPECT_SUCCEEDED(client_state_key.Open(MACHINE_REG_CLIENT_STATE));
  EXPECT_EQ(1, client_state_key.GetSubkeyCount());

  RegKey client_state_medium_key;
  EXPECT_SUCCEEDED(
      client_state_medium_key.Open(MACHINE_REG_CLIENT_STATE_MEDIUM));
  EXPECT_EQ(0, client_state_medium_key.GetSubkeyCount());
  VerifyHklmKeyHasDefaultIntegrity(MACHINE_REG_CLIENT_STATE_MEDIUM);

  CString expected_shell_path;
  EXPECT_SUCCEEDED(GetFolderPath(CSIDL_PROGRAM_FILES, &expected_shell_path));
  expected_shell_path.Append(_T("\\") PATH_COMPANY_NAME
                             _T("\\") PRODUCT_NAME
                             _T("\\") MAIN_EXE_BASE_NAME _T(".exe"));
  CString shell_path;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(MACHINE_REG_UPDATE, _T("path"), &shell_path));
  EXPECT_STREQ(expected_shell_path, shell_path);

  CommandLineBuilder builder(COMMANDLINE_MODE_UNINSTALL);
  CString expected_uninstall_cmd_line =
      builder.GetCommandLine(expected_shell_path);
  CString uninstall_cmd_line;
  EXPECT_SUCCEEDED(RegKey::GetValue(MACHINE_REG_UPDATE,
                                    kRegValueUninstallCmdLine,
                                    &uninstall_cmd_line));
  EXPECT_STREQ(expected_uninstall_cmd_line, uninstall_cmd_line);

  CString product_version;
  EXPECT_SUCCEEDED(RegKey::GetValue(MACHINE_REG_CLIENT_STATE_GOOPDATE,
                                    _T("pv"),
                                    &product_version));
  EXPECT_STREQ(GetVersionString(), product_version);

  // Test permission of Update and ClientState. ClientStateMedium checked above.
  VerifyHklmKeyHasDefaultIntegrity(MACHINE_REG_UPDATE);
  VerifyHklmKeyHasDefaultIntegrity(MACHINE_REG_CLIENT_STATE);

  // Test the permission inheritance for ClientStateMedium.
  const CString app_client_state_medium_key_name = AppendRegKeyPath(
      MACHINE_REG_CLIENT_STATE_MEDIUM,
      kAppId1);
  EXPECT_SUCCEEDED(RegKey::CreateKey(app_client_state_medium_key_name));

  VerifyHklmKeyHasMediumIntegrity(app_client_state_medium_key_name);
}

TEST_F(SetupGoogleUpdateMachineTest,
       CreateClientStateMedium_KeyAlreadyExistsWithSamePermissions) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(MACHINE_REG_UPDATE));
  EXPECT_SUCCEEDED(CreateClientStateMedium());
  VerifyHklmKeyHasDefaultIntegrity(MACHINE_REG_CLIENT_STATE_MEDIUM);

  EXPECT_SUCCEEDED(CreateClientStateMedium());
  VerifyHklmKeyHasDefaultIntegrity(MACHINE_REG_CLIENT_STATE_MEDIUM);
}

// CreateClientStateMedium does not replace permissions on existing keys.
TEST_F(SetupGoogleUpdateMachineTest,
       CreateClientStateMedium_KeysAlreadyExistWithDifferentPermissions) {
  const CString app1_client_state_medium_key_name = AppendRegKeyPath(
      MACHINE_REG_CLIENT_STATE_MEDIUM,
      kAppId1);
  const CString app2_client_state_medium_key_name = AppendRegKeyPath(
      MACHINE_REG_CLIENT_STATE_MEDIUM,
      kAppId2);

  TRUSTEE users = {0};
  users.TrusteeForm = TRUSTEE_IS_NAME;
  users.TrusteeType = TRUSTEE_IS_GROUP;
  users.ptstrName = const_cast<LPTSTR>(_T("Users"));

  TRUSTEE interactive = {0};
  interactive.TrusteeForm = TRUSTEE_IS_NAME;
  interactive.TrusteeType = TRUSTEE_IS_GROUP;
  interactive.ptstrName = const_cast<LPTSTR>(_T("INTERACTIVE"));

  EXPECT_SUCCEEDED(RegKey::CreateKey(MACHINE_REG_UPDATE));

  CDacl dacl;
  dacl.AddAllowedAce(Sids::Admins(), GENERIC_ALL);
  // Interactive is not explicitly set.
  dacl.AddAllowedAce(Sids::Users(), KEY_WRITE);

  CSecurityDesc security_descriptor;
  security_descriptor.SetDacl(dacl);
  security_descriptor.MakeAbsolute();

  CSecurityAttributes sa;
  sa.Set(security_descriptor);

  EXPECT_SUCCEEDED(RegKey::CreateKey(MACHINE_REG_CLIENT_STATE_MEDIUM,
                                     REG_NONE,
                                     REG_OPTION_NON_VOLATILE,
                                     &sa));

  EXPECT_SUCCEEDED(RegKey::CreateKey(app1_client_state_medium_key_name));


  EXPECT_SUCCEEDED(CreateClientStateMedium());

  // Verify the ACLs for the existing keys were not changed.
  // INTERACTIVE appears to inherit the privileges of Users.
  VerifyAccessRightsForTrustee(
      MACHINE_REG_CLIENT_STATE_MEDIUM, KEY_WRITE, &dacl, &users);
  VerifyAccessRightsForTrustee(
      MACHINE_REG_CLIENT_STATE_MEDIUM, KEY_WRITE, &dacl, &interactive);
  VerifyAccessRightsForTrustee(
      app1_client_state_medium_key_name, KEY_WRITE, &dacl, &users);
  VerifyAccessRightsForTrustee(
      app1_client_state_medium_key_name, KEY_WRITE, &dacl, &interactive);

  // Verify the ACLs of newly created subkeys.
  EXPECT_SUCCEEDED(RegKey::CreateKey(app2_client_state_medium_key_name));
  VerifyAccessRightsForTrustee(
      app2_client_state_medium_key_name, KEY_WRITE, &dacl, &users);
  VerifyAccessRightsForTrustee(
      app2_client_state_medium_key_name, KEY_WRITE, &dacl, &interactive);
}

TEST_F(SetupGoogleUpdateUserTest, InstallLaunchMechanisms_RunKeyValueExists) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kRunKey,
                                    _T(OMAHA_APP_NAME_ANSI),
                                    _T("fo /b")));

  EXPECT_SUCCEEDED(InstallLaunchMechanisms());

  CString value;
  EXPECT_SUCCEEDED(RegKey::GetValue(kRunKey,
                                    _T(OMAHA_APP_NAME_ANSI),
                                    &value));
  EXPECT_STREQ(expected_run_key_value_, value);
  EXPECT_TRUE(scheduled_task_utils::IsInstalledGoopdateTaskUA(false));
  EXPECT_FALSE(scheduled_task_utils::IsDisabledGoopdateTaskUA(false));

  UninstallLaunchMechanisms();
  EXPECT_FALSE(RegKey::HasValue(kRunKey, _T(OMAHA_APP_NAME_ANSI)));
  EXPECT_FALSE(scheduled_task_utils::IsInstalledGoopdateTaskUA(false));
}

TEST_F(SetupGoogleUpdateUserTest, InstallLaunchMechanisms_RunKeyDoesNotExist) {
  RegKey::DeleteValue(kRunKey, _T(OMAHA_APP_NAME_ANSI));
  ASSERT_FALSE(RegKey::HasValue(kRunKey, _T(OMAHA_APP_NAME_ANSI)));

  EXPECT_SUCCEEDED(InstallLaunchMechanisms());

  CString value;
  EXPECT_SUCCEEDED(RegKey::GetValue(kRunKey,
                                    _T(OMAHA_APP_NAME_ANSI),
                                    &value));
  EXPECT_STREQ(expected_run_key_value_, value);
  EXPECT_TRUE(scheduled_task_utils::IsInstalledGoopdateTaskUA(false));
  EXPECT_FALSE(scheduled_task_utils::IsDisabledGoopdateTaskUA(false));

  UninstallLaunchMechanisms();
  EXPECT_FALSE(RegKey::HasValue(kRunKey, _T(OMAHA_APP_NAME_ANSI)));
  EXPECT_FALSE(scheduled_task_utils::IsInstalledGoopdateTaskUA(false));
}

TEST_F(SetupGoogleUpdateMachineTest, WriteClientStateMedium) {
  bool is_uac_on(false);
  EXPECT_SUCCEEDED(vista_util::IsUACOn(&is_uac_on));

  if (!is_uac_on || !vista_util::IsUserAdmin()) {
    std::wcout << _T("\tThis test did not run. Run as admin with UAC on.")
               << std::endl;
    return;
  }

  EXPECT_SUCCEEDED(CreateClientStateMedium());

  const CString app_client_state_medium_key_name(AppendRegKeyPath(
      MACHINE_REG_CLIENT_STATE_MEDIUM,
      kAppId1));
  EXPECT_SUCCEEDED(RegKey::CreateKey(app_client_state_medium_key_name));

  scoped_handle token;
  ASSERT_SUCCEEDED(vista::GetLoggedOnUserToken(address(token)));
  scoped_impersonation impersonate_user(get(token));
  EXPECT_EQ(impersonate_user.result(), S_OK);

  // Verify that we can write values and create subkeys with values when running
  // with the medium integrity token (assuming UAC is turned on).
  EXPECT_TRUE(!vista_util::IsUserAdmin());

  const CString key_name(app_client_state_medium_key_name);
  RegKey key;
  EXPECT_SUCCEEDED(key.Open(key_name,
                            KEY_READ | KEY_SET_VALUE | KEY_CREATE_SUB_KEY));

  EXPECT_SUCCEEDED(key.SetValue(_T("Value1"), _T("11")));

  const CString subkey_name(AppendRegKeyPath(key_name, _T("Subkey1")));
  RegKey subkey;
  EXPECT_SUCCEEDED(subkey.Create(subkey_name));
  EXPECT_SUCCEEDED(subkey.SetValue(_T("Subkey1Value1"), _T("21")));
}

// Creates the ClientStateMedium key with the appropriate permissions then
// verifies that the created app subkey inherits those.
// The Update key must be created first to avoid applying ClientStateMedium's
// permissions to all its parent keys.
TEST_F(SetupGoogleUpdateMachineTest,
       WritePreInstallData_CheckClientStateMediumPermissions) {
  // Start with a clean state without a ClientStateMedium key.
  const CString client_state_medium_key_name(
      ConfigManager::Instance()->machine_registry_client_state_medium());
  RegKey::DeleteKey(client_state_medium_key_name);

  // Create the ClientStateMedium key and one subkey.
  EXPECT_SUCCEEDED(RegKey::CreateKey(
      ConfigManager::Instance()->machine_registry_update()));
  CreateClientStateMedium();
  CString app_client_state_medium_key_name;
  SafeCStringFormat(&app_client_state_medium_key_name, _T("%s\\%s"),
      client_state_medium_key_name, "{21CD0965-0B0E-47cf-B421-2D191C16C0E2}");
  EXPECT_SUCCEEDED(RegKey::CreateKey(app_client_state_medium_key_name));

  // Verify the security ACLs for the key and its subkey.
  VerifyHklmKeyHasMediumIntegrity(app_client_state_medium_key_name);
  VerifyHklmKeyHasDefaultIntegrity(client_state_medium_key_name);
}

}  // namespace omaha
