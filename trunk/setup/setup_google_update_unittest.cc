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
#include <msi.h>
#include "base/scoped_ptr.h"
#include "goopdate/google_update_idl.h"
#include "omaha/common/app_util.h"
#include "omaha/common/atlregmapex.h"
#include "omaha/common/const_cmd_line.h"
#include "omaha/common/omaha_version.h"
#include "omaha/common/file.h"
#include "omaha/common/path.h"
#include "omaha/common/time.h"
#include "omaha/common/utils.h"
#include "omaha/common/vistautil.h"
#include "omaha/goopdate/command_line.h"
#include "omaha/goopdate/config_manager.h"
#include "omaha/goopdate/const_goopdate.h"
#include "omaha/goopdate/goopdate_utils.h"
#include "omaha/setup/msi_test_utils.h"
#include "omaha/setup/setup_google_update.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace {

const TCHAR kMsiInstallRegValue[] = _T("MsiStubRun");
const TCHAR kMsiUninstallKey[] =
    _T("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\")
    _T("Uninstall\\{A92DAB39-4E2C-4304-9AB6-BC44E68B55E2}");
const TCHAR kRunKey[] =
    _T("HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run");

const TCHAR* const kExpectedIid = _T("{A972BB39-CCA3-4F25-9737-3308F5FA19B5}");
const TCHAR* const kExpectedBrand = _T("GOOG");
const TCHAR* const kExpectedClientId = _T("some_partner");

const TCHAR* const kAppId1 = _T("{B7E61EF9-AAE5-4cdf-A2D3-E4C8DF975145}");
const TCHAR* const kAppId2 = _T("{35F1A986-417D-4039-8718-781DD418232A}");

const TCHAR kRegistryHiveOverrideRootInHklm[] =
    _T("HKLM\\Software\\Google\\Update\\UnitTest\\");

// Copies the shell and DLLs that FinishInstall needs.
// Does not replace files if they already exist.
void CopyFilesRequiredByFinishInstall(bool is_machine, const CString& version) {
  ASSERT_FALSE(is_machine) << _T("machine installs not currently supported");
  const CString omaha_path = is_machine ?
      GetGoogleUpdateMachinePath() : GetGoogleUpdateUserPath();
  const CString expected_shell_path =
      ConcatenatePath(omaha_path, _T("GoogleUpdate.exe"));
  const CString version_path = ConcatenatePath(omaha_path, version);

  ASSERT_SUCCEEDED(CreateDir(omaha_path, NULL));
  ASSERT_SUCCEEDED(File::Copy(
      ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                      _T("GoogleUpdate.exe")),
      expected_shell_path,
      false));

  ASSERT_SUCCEEDED(CreateDir(version_path, NULL));

  const TCHAR* files[] = {_T("goopdate.dll"),
                          _T("GoopdateBho.dll"),
                          ACTIVEX_FILENAME};
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
// creates dummy entries that VerifyCOMLocalServerRegistration() verifies, and
// is happy about.
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

  // These checks take a long time, so avoid some of them in the common case.
  if (!ShouldRunLargeTest()) {
    return;
  }

  const ACCESS_MASK kExpectedPowerUsersAccess = vista_util::IsVistaOrLater() ?
      0 : DELETE | READ_CONTROL | KEY_READ | KEY_WRITE;

  CDacl dacl;
  RegKey key;
  EXPECT_SUCCEEDED(key.Open(key_name));
  EXPECT_TRUE(::AtlGetDacl(key.Key(), SE_REGISTRY_KEY, &dacl));

  CString current_user_sid_string;
  EXPECT_SUCCEEDED(
      user_info::GetCurrentUser(NULL, NULL, &current_user_sid_string));
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
  administrators.ptstrName = _T("Administrators");
  VerifyAccessRightsForTrustee(key_name,
                               KEY_ALL_ACCESS,
                               &dacl,
                               &administrators);

  TRUSTEE users = {0};
  users.TrusteeForm = TRUSTEE_IS_NAME;
  users.TrusteeType = TRUSTEE_IS_GROUP;
  users.ptstrName = _T("Users");
  VerifyAccessRightsForTrustee(key_name, KEY_READ, &dacl, &users);

  TRUSTEE power_users = {0};
  power_users.TrusteeForm = TRUSTEE_IS_NAME;
  power_users.TrusteeType = TRUSTEE_IS_GROUP;
  power_users.ptstrName = _T("Power Users");
  VerifyAccessRightsForTrustee(key_name,
                               kExpectedPowerUsersAccess,
                               &dacl,
                               &power_users);

  TRUSTEE interactive = {0};
  interactive.TrusteeForm = TRUSTEE_IS_NAME;
  interactive.TrusteeType = TRUSTEE_IS_GROUP;
  interactive.ptstrName = _T("INTERACTIVE");
  VerifyAccessRightsForTrustee(key_name,
                               expected_non_admin_interactive_access,
                               &dacl,
                               &interactive);

  TRUSTEE everyone = {0};
  everyone.TrusteeForm = TRUSTEE_IS_NAME;
  everyone.TrusteeType = TRUSTEE_IS_GROUP;
  everyone.ptstrName = _T("Everyone");
  VerifyAccessRightsForTrustee(key_name, 0, &dacl, &everyone);

  TRUSTEE guest = {0};
  guest.TrusteeForm = TRUSTEE_IS_NAME;
  guest.TrusteeType = TRUSTEE_IS_USER;
  guest.ptstrName = _T("Guest");
  VerifyAccessRightsForTrustee(key_name, 0, &dacl, &guest);
}

}  // namespace

// INTERACTIVE group inherits privileges from Users in the default case.
void VerifyHklmKeyHasDefaultIntegrity(const CString& key_full_name) {
  VerifyHklmKeyHasIntegrity(key_full_name, KEY_READ);
}

void VerifyHklmKeyHasMediumIntegrity(const CString& key_full_name) {
  VerifyHklmKeyHasIntegrity(key_full_name, KEY_READ | KEY_SET_VALUE);
}

class SetupGoogleUpdateTest : public testing::Test {
 protected:
  explicit SetupGoogleUpdateTest(bool is_machine)
      : is_machine_(is_machine) {
  }

  void SetUp() {
    GUID iid  = StringToGuid(kExpectedIid);
    CommandLineAppArgs extra;
    args_.extra.language = _T("en");
    args_.extra.installation_id = iid;
    args_.extra.brand_code = kExpectedBrand;
    args_.extra.client_id = kExpectedClientId;
    args_.extra.referral_id = _T("should not be used by setup");
    args_.extra.apps.push_back(extra);
    setup_google_update_.reset(new SetupGoogleUpdate(is_machine_, &args_));
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

  HRESULT InstallMsiHelper() {
    return setup_google_update_->InstallMsiHelper();
  }

  HRESULT UninstallMsiHelper() {
    return setup_google_update_->UninstallMsiHelper();
  }

  bool is_machine_;
  scoped_ptr<SetupGoogleUpdate> setup_google_update_;
  CommandLineArgs args_;
};

class SetupGoogleUpdateUserTest : public SetupGoogleUpdateTest {
 protected:
  SetupGoogleUpdateUserTest()
      : SetupGoogleUpdateTest(false) {
  }
};

class SetupGoogleUpdateMachineTest : public SetupGoogleUpdateTest {
 protected:
  SetupGoogleUpdateMachineTest()
      : SetupGoogleUpdateTest(true) {
  }
};

class SetupGoogleUpdateUserRegistryProtectedTest
    : public SetupGoogleUpdateUserTest {
 protected:
  SetupGoogleUpdateUserRegistryProtectedTest()
      : SetupGoogleUpdateUserTest(),
        hive_override_key_name_(kRegistryHiveOverrideRoot) {
    const CString expected_shell_path =
        ConcatenatePath(GetGoogleUpdateUserPath(), _T("GoogleUpdate.exe"));
    expected_run_key_value_.Format(_T("\"%s\" /c"), expected_shell_path);
  }

  void SetUp() {
    RegKey::DeleteKey(hive_override_key_name_, true);
    // Do not override HKLM because it contains the CSIDL_* definitions.
    OverrideSpecifiedRegistryHives(hive_override_key_name_, false, true);

    SetupGoogleUpdateUserTest::SetUp();
  }

  virtual void TearDown() {
    SetupGoogleUpdateUserTest::TearDown();

    RestoreRegistryHives();
    ASSERT_SUCCEEDED(RegKey::DeleteKey(hive_override_key_name_, true));
  }

  CString hive_override_key_name_;
  CString expected_run_key_value_;
};

class SetupGoogleUpdateMachineRegistryProtectedTest
    : public SetupGoogleUpdateMachineTest {
 protected:
  SetupGoogleUpdateMachineRegistryProtectedTest()
      : SetupGoogleUpdateMachineTest(),
        hive_override_key_name_(kRegistryHiveOverrideRoot) {
  }

  void SetUp() {
    RegKey::DeleteKey(hive_override_key_name_, true);
    OverrideRegistryHives(hive_override_key_name_);

    // Add CSIDL values needed by the tests.
    ASSERT_SUCCEEDED(RegKey::SetValue(kCsidlSystemIdsRegKey,
                                      kCsidlProgramFilesRegValue,
                                      _T("C:\\Program Files")));

    SetupGoogleUpdateMachineTest::SetUp();
  }

  virtual void TearDown() {
    SetupGoogleUpdateMachineTest::TearDown();

    RestoreRegistryHives();
    ASSERT_SUCCEEDED(RegKey::DeleteKey(hive_override_key_name_, true));
  }

  CString hive_override_key_name_;
};

// There are a few tests where keys need to inherit the HKLM privileges, so put
// the override root in HKLM. All tests using this framework must run as admin.
class SetupGoogleUpdateMachineRegistryProtectedInHklmTest
    : public SetupGoogleUpdateMachineRegistryProtectedTest {
 protected:
  SetupGoogleUpdateMachineRegistryProtectedInHklmTest()
      : SetupGoogleUpdateMachineRegistryProtectedTest() {
    hive_override_key_name_ = kRegistryHiveOverrideRootInHklm;
  }
};

// This test uninstalls all other versions of Omaha.
TEST_F(SetupGoogleUpdateUserRegistryProtectedTest,
       FinishInstall_RunKeyDoesNotExist) {
  if (!ShouldRunLargeTest()) {
    return;
  }

  // The version in the real registry must be set because it is used by
  // GoogleUpdate.exe during registrations.
  RestoreRegistryHives();
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                    kRegValueProductVersion,
                                    GetVersionString()));
  OverrideSpecifiedRegistryHives(hive_override_key_name_, false, true);

  CopyFilesRequiredByFinishInstall(is_machine_, GetVersionString());
  SetupCOMLocalServerRegistration(is_machine_);

  EXPECT_SUCCEEDED(RegKey::CreateKey(_T("HKCU\\Software\\Classes")));

  ASSERT_FALSE(RegKey::HasKey(kRunKey));

  EXPECT_SUCCEEDED(setup_google_update_->FinishInstall());

  // Check the system state.

  CPath expected_shell_path(GetGoogleUpdateUserPath());
  expected_shell_path.Append(_T("GoogleUpdate.exe"));
  CString shell_path;
  EXPECT_SUCCEEDED(RegKey::GetValue(USER_REG_UPDATE, _T("path"), &shell_path));
  EXPECT_STREQ(expected_shell_path, shell_path);

  CString value;
  EXPECT_SUCCEEDED(RegKey::GetValue(kRunKey, _T("Google Update"), &value));
  EXPECT_STREQ(expected_run_key_value_, value);
  EXPECT_TRUE(goopdate_utils::IsInstalledGoopdateTaskUA(false));
  EXPECT_FALSE(goopdate_utils::IsDisabledGoopdateTaskUA(false));

  EXPECT_SUCCEEDED(
      RegKey::GetValue(USER_REG_UPDATE, kRegValueInstalledVersion, &value));
  EXPECT_EQ(GetVersionString(), value);
  EXPECT_FALSE(RegKey::HasValue(USER_REG_UPDATE, kRegValueLastChecked));

  // TODO(omaha): Check for other state.

  // Clean up the launch mechanisms, at least one of which is not in the
  // overriding registry.
  UninstallLaunchMechanisms();
  EXPECT_FALSE(RegKey::HasValue(kRunKey, _T("Google Update")));
  EXPECT_FALSE(goopdate_utils::IsInstalledGoopdateTaskUA(false));
}

// TODO(omaha): Assumes GoogleUpdate.exe exists in the installed location, which
// is not always true when run independently.
TEST_F(SetupGoogleUpdateUserRegistryProtectedTest, InstallRegistryValues) {
  // For this test only, we must also override HKLM in order to check that
  // MACHINE_REG_CLIENT_STATE_MEDIUM was not created.
  // Get the correct value of and set the CSIDL value needed by the test.
  CString profile_path;
  EXPECT_SUCCEEDED(GetFolderPath(CSIDL_PROFILE, &profile_path));
  OverrideSpecifiedRegistryHives(hive_override_key_name_, true, true);
  CString user_sid;
  EXPECT_SUCCEEDED(user_info::GetCurrentUser(NULL, NULL, &user_sid));
  const TCHAR* const kProfileListKey =
      _T("HKLM\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\")
      _T("ProfileList\\");
  const CString profile_path_key = kProfileListKey + user_sid;
  EXPECT_SUCCEEDED(
    RegKey::SetValue(profile_path_key, _T("ProfileImagePath"), profile_path));

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
  RegKey google_key;
  EXPECT_SUCCEEDED(google_key.Open(USER_REG_GOOGLE));
  EXPECT_EQ(1, google_key.GetSubkeyCount());

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
  expected_shell_path.Append(_T("GoogleUpdate.exe"));
  CString shell_path;
  EXPECT_SUCCEEDED(RegKey::GetValue(USER_REG_UPDATE, _T("path"), &shell_path));
  EXPECT_STREQ(expected_shell_path, shell_path);

  CString iid;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(USER_REG_CLIENT_STATE_GOOPDATE, _T("iid"), &iid));
  EXPECT_STREQ(kExpectedIid, iid);

  CString brand;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(USER_REG_CLIENT_STATE_GOOPDATE, _T("brand"), &brand));
  EXPECT_STREQ(kExpectedBrand, brand);

  CString client_id;
  EXPECT_SUCCEEDED(RegKey::GetValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                    _T("client"),
                                    &client_id));
  EXPECT_STREQ(kExpectedClientId, client_id);

  EXPECT_FALSE(
      RegKey::HasValue(USER_REG_CLIENT_STATE_GOOPDATE, _T("referral")));

  DWORD install_time(0);
  EXPECT_SUCCEEDED(RegKey::GetValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                    _T("InstallTime"),
                                    &install_time));
  EXPECT_GE(now, install_time);
  EXPECT_GE(static_cast<uint32>(200), now - install_time);

  CString product_version;
  EXPECT_SUCCEEDED(RegKey::GetValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                    _T("pv"),
                                    &product_version));
  EXPECT_STREQ(GetVersionString(), product_version);
}

// TODO(omaha): Assumes GoogleUpdate.exe exists in the installed location, which
// is not always true when run independently.
// TODO(omaha): Fails when run by itself on Windows Vista.
TEST_F(SetupGoogleUpdateMachineRegistryProtectedInHklmTest,
       InstallRegistryValues) {
// TODO(omaha): Remove the ifdef when signing occurs after instrumentation or
// the TODO above is addressed.
#ifdef COVERAGE_ENABLED
  std::wcout << _T("\tTest does not run in coverage builds.") << std::endl;
#else
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
  RegKey google_key;
  EXPECT_SUCCEEDED(google_key.Open(MACHINE_REG_GOOGLE));
  EXPECT_EQ(1, google_key.GetSubkeyCount());

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
  expected_shell_path.Append(_T("\\Google\\Update\\GoogleUpdate.exe"));
  CString shell_path;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(MACHINE_REG_UPDATE, _T("path"), &shell_path));
  EXPECT_STREQ(expected_shell_path, shell_path);

  CString iid;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(MACHINE_REG_CLIENT_STATE_GOOPDATE, _T("iid"), &iid));
  EXPECT_STREQ(kExpectedIid, iid);

  CString brand;
  EXPECT_SUCCEEDED(
      RegKey::GetValue(MACHINE_REG_CLIENT_STATE_GOOPDATE, _T("brand"), &brand));
  EXPECT_STREQ(kExpectedBrand, brand);

  CString client_id;
  EXPECT_SUCCEEDED(RegKey::GetValue(MACHINE_REG_CLIENT_STATE_GOOPDATE,
                                    _T("client"),
                                    &client_id));
  EXPECT_STREQ(kExpectedClientId, client_id);

  EXPECT_FALSE(
      RegKey::HasValue(MACHINE_REG_CLIENT_STATE_GOOPDATE, _T("referral")));

  DWORD install_time(0);
  EXPECT_SUCCEEDED(RegKey::GetValue(MACHINE_REG_CLIENT_STATE_GOOPDATE,
                                    _T("InstallTime"),
                                    &install_time));
  EXPECT_GE(now, install_time);
  EXPECT_GE(static_cast<uint32>(200), now - install_time);

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
#endif
}

TEST_F(SetupGoogleUpdateMachineRegistryProtectedInHklmTest,
       CreateClientStateMedium_KeyAlreadyExistsWithSamePermissions) {
  EXPECT_SUCCEEDED(RegKey::CreateKey(MACHINE_REG_UPDATE));
  EXPECT_SUCCEEDED(CreateClientStateMedium());
  VerifyHklmKeyHasDefaultIntegrity(MACHINE_REG_CLIENT_STATE_MEDIUM);

  EXPECT_SUCCEEDED(CreateClientStateMedium());
  VerifyHklmKeyHasDefaultIntegrity(MACHINE_REG_CLIENT_STATE_MEDIUM);
}

// CreateClientStateMedium does not replace permissions on existing keys.
TEST_F(SetupGoogleUpdateMachineRegistryProtectedInHklmTest,
       CreateClientStateMedium_KeysAlreadyExistWithDifferentPermissions) {
  if (!ShouldRunLargeTest()) {
    return;
  }

  const CString app1_client_state_medium_key_name = AppendRegKeyPath(
      MACHINE_REG_CLIENT_STATE_MEDIUM,
      kAppId1);
  const CString app2_client_state_medium_key_name = AppendRegKeyPath(
      MACHINE_REG_CLIENT_STATE_MEDIUM,
      kAppId2);

  TRUSTEE users = {0};
  users.TrusteeForm = TRUSTEE_IS_NAME;
  users.TrusteeType = TRUSTEE_IS_GROUP;
  users.ptstrName = _T("Users");

  TRUSTEE interactive = {0};
  interactive.TrusteeForm = TRUSTEE_IS_NAME;
  interactive.TrusteeType = TRUSTEE_IS_GROUP;
  interactive.ptstrName = _T("INTERACTIVE");

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

TEST_F(SetupGoogleUpdateUserRegistryProtectedTest,
       InstallLaunchMechanisms_RunKeyValueExists) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kRunKey, _T("Google Update"), _T("fo /b")));

  EXPECT_SUCCEEDED(InstallLaunchMechanisms());

  CString value;
  EXPECT_SUCCEEDED(RegKey::GetValue(kRunKey, _T("Google Update"), &value));
  EXPECT_STREQ(expected_run_key_value_, value);
  EXPECT_TRUE(goopdate_utils::IsInstalledGoopdateTaskUA(false));
  EXPECT_FALSE(goopdate_utils::IsDisabledGoopdateTaskUA(false));

  UninstallLaunchMechanisms();
  EXPECT_FALSE(RegKey::HasValue(kRunKey, _T("Google Update")));
  EXPECT_FALSE(goopdate_utils::IsInstalledGoopdateTaskUA(false));
}

TEST_F(SetupGoogleUpdateUserRegistryProtectedTest,
       InstallLaunchMechanisms_RunKeyDoesNotExist) {
  ASSERT_FALSE(RegKey::HasKey(kRunKey));

  EXPECT_SUCCEEDED(InstallLaunchMechanisms());

  CString value;
  EXPECT_SUCCEEDED(RegKey::GetValue(kRunKey, _T("Google Update"), &value));
  EXPECT_STREQ(expected_run_key_value_, value);
  EXPECT_TRUE(goopdate_utils::IsInstalledGoopdateTaskUA(false));
  EXPECT_FALSE(goopdate_utils::IsDisabledGoopdateTaskUA(false));

  UninstallLaunchMechanisms();
  EXPECT_FALSE(RegKey::HasValue(kRunKey, _T("Google Update")));
  EXPECT_FALSE(goopdate_utils::IsInstalledGoopdateTaskUA(false));
}

// The helper can be installed when the test begins.
// It will not be installed when the test successfully completes.
TEST_F(SetupGoogleUpdateMachineTest, InstallAndUninstallMsiHelper) {
  if (!ShouldRunLargeTest()) {
    return;
  }
  const TCHAR* MsiInstallRegValueKey =
      ConfigManager::Instance()->machine_registry_update();

  if (vista_util::IsUserAdmin()) {
    // Prepare for the test - make sure the helper isn't installed.
    EXPECT_HRESULT_SUCCEEDED(UninstallMsiHelper());
    EXPECT_FALSE(RegKey::HasValue(MsiInstallRegValueKey, kMsiInstallRegValue));
    EXPECT_FALSE(RegKey::HasKey(kMsiUninstallKey));

    // Verify installation.
    DWORD reg_value = 0xffffffff;
    EXPECT_HRESULT_SUCCEEDED(InstallMsiHelper());
    EXPECT_TRUE(RegKey::HasValue(MsiInstallRegValueKey, kMsiInstallRegValue));
    EXPECT_HRESULT_SUCCEEDED(RegKey::GetValue(MsiInstallRegValueKey,
                                              kMsiInstallRegValue,
                                              &reg_value));
    EXPECT_EQ(0, reg_value);
    EXPECT_TRUE(RegKey::HasKey(kMsiUninstallKey));

    // Verify over-install.
    EXPECT_HRESULT_SUCCEEDED(InstallMsiHelper());
    EXPECT_TRUE(RegKey::HasValue(MsiInstallRegValueKey, kMsiInstallRegValue));
    EXPECT_HRESULT_SUCCEEDED(RegKey::GetValue(MsiInstallRegValueKey,
                                              kMsiInstallRegValue,
                                              &reg_value));
    EXPECT_EQ(0, reg_value);
    EXPECT_TRUE(RegKey::HasKey(kMsiUninstallKey));

    // Verify uninstall.
    EXPECT_HRESULT_SUCCEEDED(UninstallMsiHelper());
    EXPECT_FALSE(RegKey::HasValue(MsiInstallRegValueKey, kMsiInstallRegValue));
    EXPECT_FALSE(RegKey::HasKey(kMsiUninstallKey));

    // Verify uninstall when not currently installed.
    EXPECT_HRESULT_SUCCEEDED(UninstallMsiHelper());
  } else {
    {
      // This method expects to be called elevated and makes an assumption
      // about a return value.
      ExpectAsserts expect_asserts;
      EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_INSTALL_PACKAGE_REJECTED),
                InstallMsiHelper());
    }
    if (IsMsiHelperInstalled()) {
      // If the MSI is installed UninstallMsiHelper returns
      // ERROR_INSTALL_FAILURE.
      EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_INSTALL_FAILURE),
                UninstallMsiHelper());
    } else {
      // If the MSI is not installed UninstallMsiHelper returns S_OK.
      EXPECT_HRESULT_SUCCEEDED(UninstallMsiHelper());
    }
  }
}

// This test installs a different build of the helper installer then calls
// InstallMsiHelper to install the one that has been built.
// If run without the REINSTALL property, ERROR_PRODUCT_VERSION would occur.
// This test verifies that such overinstalls are correctly handled and that the
// registry value is correctly changed.
// Note: The name of the installer cannot be different. MSI tries to find the
// original filename in the new directory.
// The helper can be installed when the test begins.
// It will not be installed when the test successfully completes.
TEST_F(SetupGoogleUpdateMachineTest,
       InstallMsiHelper_OverinstallDifferentMsiBuild) {
  if (!ShouldRunLargeTest()) {
    return;
  }
  if (!vista_util::IsUserAdmin()) {
    std::wcout << _T("\tThis test did not run because it must be run as admin.")
               << std::endl;
    return;
  }

  const TCHAR kDifferentMsi[] =
      _T("unittest_support\\GoogleUpdateHelper.msi");
  const TCHAR* MsiInstallRegValueKey =
      ConfigManager::Instance()->machine_registry_update();

  CString different_msi_path(app_util::GetCurrentModuleDirectory());
  ASSERT_TRUE(::PathAppend(CStrBuf(different_msi_path, MAX_PATH),
                           kDifferentMsi));

  // Prepare for the test - make sure the helper is not installed.
  EXPECT_HRESULT_SUCCEEDED(UninstallMsiHelper());
  EXPECT_FALSE(RegKey::HasValue(MsiInstallRegValueKey, kMsiInstallRegValue));
  EXPECT_FALSE(RegKey::HasKey(kMsiUninstallKey));

  // Install an older version of the MSI.
  DWORD reg_value = 0xffffffff;
  ::MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);
  EXPECT_EQ(ERROR_SUCCESS, ::MsiInstallProduct(different_msi_path, _T("")));
  EXPECT_TRUE(RegKey::HasValue(MsiInstallRegValueKey, kMsiInstallRegValue));
  EXPECT_HRESULT_SUCCEEDED(
      RegKey::GetValue(MsiInstallRegValueKey, kMsiInstallRegValue, &reg_value));
  EXPECT_EQ(9, reg_value);
  EXPECT_TRUE(RegKey::HasKey(kMsiUninstallKey));

  // Over-install.
  EXPECT_HRESULT_SUCCEEDED(InstallMsiHelper());
  EXPECT_TRUE(RegKey::HasValue(MsiInstallRegValueKey, kMsiInstallRegValue));
  EXPECT_HRESULT_SUCCEEDED(
      RegKey::GetValue(MsiInstallRegValueKey, kMsiInstallRegValue, &reg_value));
  EXPECT_EQ(0, reg_value);
  EXPECT_TRUE(RegKey::HasKey(kMsiUninstallKey));

  // Clean up.
  EXPECT_HRESULT_SUCCEEDED(UninstallMsiHelper());
  EXPECT_FALSE(RegKey::HasValue(MsiInstallRegValueKey, kMsiInstallRegValue));
  EXPECT_FALSE(RegKey::HasKey(kMsiUninstallKey));
}

}  // namespace omaha
