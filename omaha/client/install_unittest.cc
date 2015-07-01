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

#include <mstask.h>
#include "omaha/base/app_util.h"
#include "omaha/base/constants.h"
#include "omaha/base/error.h"
#include "omaha/base/file.h"
#include "omaha/base/path.h"
#include "omaha/base/reg_key.h"
#include "omaha/base/shell.h"
#include "omaha/base/time.h"
#include "omaha/base/utils.h"
#include "omaha/base/vistautil.h"
#include "omaha/client/install.h"
#include "omaha/client/install_internal.h"
#include "omaha/client/install_self_internal.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/common/scheduled_task_utils.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace {

const TCHAR* const kExpectedIid = _T("{A972BB39-CCA3-4F25-9737-3308F5FA19B5}");
const TCHAR* const kExpectedBrand = _T("GOOG");
const TCHAR* const kExpectedClientId = _T("some_partner");

const TCHAR* const kXpSystemSetupKey = _T("HKLM\\System\\Setup");
const TCHAR* const kVistaSetupStateKey =
    _T("HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Setup\\State");

// Assumes the fixture is inherited from RegistryProtectedInstallTest,
// RegistryProtectedWithComInterfacesPrimedInstallTest if is_machine.
// If is_machine, the test must run
// scheduled_task_utils::UninstallGoopdateTasks(true)) upon completion.
// The method used to prevent installing Omaha still results in Setup attempting
// to start the Core. When is_machine is true, this includes starting the
// service or scheduled task. If neither are installed, this fails with an
// assert. To avoid the assert, install the scheduled tasks. This is simpler
// than installing the service and copying files such that it will succeed.
// TODO(omaha3): Use a different method of avoiding running Setup; maybe a mock.
void PreventSetupFromRunning(bool is_machine) {
  const TCHAR* const kFutureVersionString = _T("10.9.8.7");
  EXPECT_SUCCEEDED(RegKey::SetValue(is_machine ?
                                        MACHINE_REG_CLIENTS_GOOPDATE :
                                        USER_REG_CLIENTS_GOOPDATE,
                                    kRegValueProductVersion,
                                    kFutureVersionString));

  if (!is_machine) {
    return;
  }
  const CString task_path = ConcatenatePath(
                                app_util::GetCurrentModuleDirectory(),
                                _T("unittest_support\\SaveArguments.exe"));
  EXPECT_SUCCEEDED(scheduled_task_utils::InstallGoopdateTasks(task_path, true));
}

}  // namespace

class InstallHandoffTest : public testing::Test {
 protected:
  explicit InstallHandoffTest(bool is_machine)
      : omaha_path_(is_machine ? GetGoogleUpdateMachinePath() :
                                 GetGoogleUpdateUserPath()),
        path_(ConcatenatePath(omaha_path_, kVersionString)) {
  }

  virtual void SetUp() {
    // Save the existing version if present.
    RegKey::GetValue(USER_REG_CLIENTS_GOOPDATE,
                     kRegValueProductVersion,
                     &existing_version_);
    InstallFiles();
  }

  virtual void TearDown() {
    EXPECT_SUCCEEDED(DeleteDirectory(path_));
    if (existing_version_.IsEmpty()) {
      EXPECT_SUCCEEDED(RegKey::DeleteValue(USER_REG_CLIENTS_GOOPDATE,
                                           kRegValueProductVersion));
    } else {
      EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                        kRegValueProductVersion,
                                        existing_version_));
    }
  }

  void InstallFiles() {
    DeleteDirectory(path_);
    EXPECT_FALSE(File::IsDirectory(path_));

    EXPECT_SUCCEEDED(CreateDir(path_, NULL));

    EXPECT_SUCCEEDED(File::Copy(
        ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                        kOmahaShellFileName),
        omaha_path_ + kOmahaShellFileName,
        false));

    EXPECT_SUCCEEDED(File::Copy(
        ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                        kOmahaDllName),
        ConcatenatePath(path_, kOmahaDllName),
        false));

    EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                      kRegValueProductVersion,
                                      kVersionString));
  }

  CString existing_version_;  // Saves the existing version from the registry.
  const CString omaha_path_;
  const CString path_;
  static const TCHAR* const kVersionString;
  static const TCHAR* const kAppGuid_;
  static const TCHAR* const kSessionId_;
};

const TCHAR* const InstallHandoffTest::kVersionString = _T("9.8.7.6");
const TCHAR* const InstallHandoffTest::kAppGuid_ =
    _T("{01D33078-BA95-4da6-A3FC-F31593FD4AA2}");
const TCHAR* const InstallHandoffTest::kSessionId_ =
    _T("{6cb069db-b073-4a40-9983-846a3819876a}");

class InstallHandoffUserTest : public InstallHandoffTest {
 protected:
  InstallHandoffUserTest()
      : InstallHandoffTest(false) {
  }
};

class RegistryProtectedInstallTest
    : public RegistryProtectedTest {
 protected:
  virtual void SetUp() {
    RegistryProtectedTest::SetUp();

    args_.extra.bundle_name = _T("bundle");  // Avoids assert in error cases.
    args_.install_source    = _T("unittest");

    // mpr.dll requires that the HwOrder key is present to be able to
    // initialize (http://support.microsoft.com/kb/329316). On Windows Vista and
    // later, its absence causes IPersistFile.Save() in
    // scheduled_task_utils::CreateScheduledTask() to fail with
    // ERROR_DLL_INIT_FAILED.
    EXPECT_SUCCEEDED(RegKey::CreateKey(
        _T("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\")
        _T("NetworkProvider\\HwOrder")));
  }

  CommandLineArgs args_;
};

// Primes the Task Scheduler and XML interfaces before overriding the registry
// so the interfaces are available after overriding HKLM. This is necessary to
// allow tests to be run individually.
class RegistryProtectedWithComInterfacesPrimedInstallTest
    : public RegistryProtectedInstallTest {
 protected:
  virtual void SetUp() {
    CComPtr<ITaskScheduler> scheduler;
    EXPECT_SUCCEEDED(scheduler.CoCreateInstance(CLSID_CTaskScheduler,
                                                NULL,
                                                CLSCTX_INPROC_SERVER));

    EXPECT_TRUE(install_self::internal::HasXmlParser());

    // Prime the special folder mapping. For some reason this works on
    // Windows XP but calling functions like
    // ConfigManager::GetMachineGoopdateInstallDir does not.
    typedef std::map<CString, CString> mapping;
    mapping folder_map;
    ASSERT_SUCCEEDED(Shell::GetSpecialFolderKeywordsMapping(&folder_map));

    RegistryProtectedInstallTest::SetUp();
  }
};

class InAuditModeTest
    : public RegistryProtectedWithComInterfacesPrimedInstallTest {
 protected:
  virtual void SetUp() {
    RegistryProtectedWithComInterfacesPrimedInstallTest::SetUp();

    if (vista_util::IsVistaOrLater()) {
      EXPECT_SUCCEEDED(RegKey::SetValue(kVistaSetupStateKey,
                                        _T("ImageState"),
                                        _T("IMAGE_STATE_UNDEPLOYABLE")));
    } else {
      EXPECT_SUCCEEDED(RegKey::SetValue(kXpSystemSetupKey,
                                        _T("AuditInProgress"),
                                        static_cast<DWORD>(1)));
    }

    EXPECT_TRUE(ConfigManager::Instance()->IsWindowsInstalling());
  }
};

TEST_F(InAuditModeTest, OemInstall_NotOffline) {
  if (!vista_util::IsUserAdmin()) {
    std::wcout << _T("\tTest did not run because the user is not an admin.")
               << std::endl;
    return;
  }

  PreventSetupFromRunning(true);

  bool is_machine = true;
  bool has_ui_been_displayed = false;
  EXPECT_EQ(GOOPDATE_E_OEM_WITH_ONLINE_INSTALLER,
            OemInstall(false,          // is_interactive
                       true,           // is_app_install
                       false,          // is_eula_required
                       false,          // is_install_elevated_instance
                       _T("unused"),   // install_cmd_line
                       args_,
                       &is_machine,
                       &has_ui_been_displayed));
  EXPECT_FALSE(has_ui_been_displayed);

  EXPECT_SUCCEEDED(scheduled_task_utils::UninstallGoopdateTasks(true));
}

TEST_F(RegistryProtectedWithComInterfacesPrimedInstallTest,
       Install_NotAppInstall_User_NoBrandSpecified_NoExistingBrand) {
  if (vista_util::IsElevatedWithEnableLUAOn()) {
    std::wcout << _T("\tSkipping test because user is elevated with UAC on.")
               << std::endl;
    return;
  }

  PreventSetupFromRunning(false);

  bool is_machine = false;
  bool has_ui_been_displayed = false;
  EXPECT_EQ(S_OK, Install(false,         // is_interactive
                          false,         // is_app_install
                          false,         // is_eula_required
                          false,         // is_oem_install
                          false,         // is_enterprise_install
                          false,         // is_install_elevated_instance
                          _T("foo"),     // install_cmd_line
                          args_,
                          &is_machine,
                          &has_ui_been_displayed));
  EXPECT_FALSE(has_ui_been_displayed);

  const uint32 now = Time64ToInt32(GetCurrent100NSTime());

  EXPECT_FALSE(RegKey::HasValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                kRegValueInstallationId));
  EXPECT_STREQ(_T("GGLS"), GetSzValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                      kRegValueBrandCode));
  EXPECT_FALSE(RegKey::HasValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                kRegValueClientId));
  EXPECT_FALSE(RegKey::HasValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                kRegValueReferralId));

  const DWORD install_time = GetDwordValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                           kRegValueInstallTimeSec);
  EXPECT_GE(now, install_time);
  EXPECT_GE(static_cast<uint32>(200), now - install_time);

  DWORD day_of_install = GetDwordValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                       kRegValueDayOfInstall);
  EXPECT_EQ(static_cast<DWORD>(-1), day_of_install);
}

TEST_F(RegistryProtectedWithComInterfacesPrimedInstallTest,
       Install_NotAppInstall_User_BrandSpecified_NoExistingBrand) {
  if (vista_util::IsElevatedWithEnableLUAOn()) {
    std::wcout << _T("\tSkipping test because user is elevated with UAC on.")
               << std::endl;
    return;
  }

  PreventSetupFromRunning(false);

  args_.extra.installation_id = StringToGuid(kExpectedIid);
  args_.extra.brand_code = kExpectedBrand;
  args_.extra.client_id = kExpectedClientId;

  bool is_machine = false;
  bool has_ui_been_displayed = false;
  EXPECT_EQ(S_OK, Install(false,         // is_interactive
                          false,         // is_app_install
                          false,         // is_eula_required
                          false,         // is_oem_install
                          false,         // is_enterprise_install
                          false,         // is_install_elevated_instance
                          _T("foo"),     // install_cmd_line
                          args_,
                          &is_machine,
                          &has_ui_been_displayed));
  EXPECT_FALSE(has_ui_been_displayed);

  const uint32 now = Time64ToInt32(GetCurrent100NSTime());

  EXPECT_STREQ(kExpectedIid, GetSzValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                        kRegValueInstallationId));
  EXPECT_STREQ(kExpectedBrand, GetSzValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                          kRegValueBrandCode));
  EXPECT_STREQ(kExpectedClientId, GetSzValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                             kRegValueClientId));
  EXPECT_FALSE(RegKey::HasValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                kRegValueReferralId));

  const DWORD install_time = GetDwordValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                           kRegValueInstallTimeSec);
  EXPECT_GE(now, install_time);
  EXPECT_GE(static_cast<uint32>(200), now - install_time);

  DWORD day_of_install = GetDwordValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                       kRegValueDayOfInstall);
  EXPECT_EQ(static_cast<DWORD>(-1), day_of_install);
}

TEST_F(RegistryProtectedWithComInterfacesPrimedInstallTest,
       Install_NotAppInstall_User_BrandSpecified_ExistingBrandAndInstallTime) {
  if (vista_util::IsElevatedWithEnableLUAOn()) {
    std::wcout << _T("\tSkipping test because user is elevated with UAC on.")
               << std::endl;
    return;
  }

  PreventSetupFromRunning(false);

  const TCHAR* const kExistingBrand = _T("GOOG");
  const DWORD kExistingInstallTime = 1234567;
  const DWORD kExistingDayOfInstall = 6666;

  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                    kRegValueBrandCode,
                                    kExistingBrand));
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                    kRegValueInstallTimeSec,
                                    kExistingInstallTime));
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                    kRegValueDayOfInstall,
                                    kExistingDayOfInstall));

  args_.extra.installation_id = StringToGuid(kExpectedIid);
  args_.extra.brand_code = kExpectedBrand;
  args_.extra.client_id = kExpectedClientId;

  bool is_machine = false;
  bool has_ui_been_displayed = false;
  EXPECT_EQ(S_OK, Install(false,         // is_interactive
                          false,         // is_app_install
                          false,         // is_eula_required
                          false,         // is_oem_install
                          false,         // is_enterprise_install
                          false,         // is_install_elevated_instance
                          _T("foo"),     // install_cmd_line
                          args_,
                          &is_machine,
                          &has_ui_been_displayed));
  EXPECT_FALSE(has_ui_been_displayed);

  const uint32 now = Time64ToInt32(GetCurrent100NSTime());

  EXPECT_STREQ(kExpectedIid, GetSzValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                        kRegValueInstallationId));
  EXPECT_STREQ(kExistingBrand, GetSzValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                          kRegValueBrandCode));
  EXPECT_FALSE(RegKey::HasValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                kRegValueClientId));
  EXPECT_FALSE(RegKey::HasValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                kRegValueReferralId));

  EXPECT_EQ(kExistingInstallTime, GetDwordValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                                kRegValueInstallTimeSec));
  EXPECT_EQ(kExistingDayOfInstall, GetDwordValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                                 kRegValueDayOfInstall));
}

TEST_F(RegistryProtectedInstallTest,
       Install_NotAppInstall_User_BrandSpecified_ExistingLegacyInstallTime) {
  if (!vista_util::IsVistaOrLater()) {
    std::wcout << _T("\tSkipping test on OS before Vista.") << std::endl;
    return;
  }
  if (vista_util::IsElevatedWithEnableLUAOn()) {
    std::wcout << _T("\tSkipping test because user is elevated with UAC on.")
               << std::endl;
    return;
  }

  const TCHAR* const kExistingBrand = _T("GOOG");
  const DWORD kExistingInstallTime = 1234567;

  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                    kRegValueBrandCode,
                                    kExistingBrand));
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                    kRegValueInstallTimeSec,
                                    kExistingInstallTime));

  args_.extra.installation_id = StringToGuid(kExpectedIid);
  args_.extra.brand_code = kExpectedBrand;
  args_.extra.client_id = kExpectedClientId;

  bool is_machine = false;
  bool has_ui_been_displayed = false;
  EXPECT_EQ(S_OK, Install(false,         // is_interactive
                          false,         // is_app_install
                          false,         // is_eula_required
                          false,         // is_oem_install
                          false,         // is_enterprise_install
                          false,         // is_install_elevated_instance
                          _T("foo"),     // install_cmd_line
                          args_,
                          &is_machine,
                          &has_ui_been_displayed));
  EXPECT_FALSE(has_ui_been_displayed);

  const uint32 now = Time64ToInt32(GetCurrent100NSTime());

  EXPECT_STREQ(kExpectedIid, GetSzValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                        kRegValueInstallationId));
  EXPECT_STREQ(kExistingBrand, GetSzValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                          kRegValueBrandCode));
  EXPECT_FALSE(RegKey::HasValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                kRegValueClientId));
  EXPECT_FALSE(RegKey::HasValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                kRegValueReferralId));

  EXPECT_EQ(kExistingInstallTime, GetDwordValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                                kRegValueInstallTimeSec));
  EXPECT_FALSE(RegKey::HasValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                kRegValueDayOfInstall));
}

TEST_F(RegistryProtectedWithComInterfacesPrimedInstallTest,
       Install_NotAppInstall_Machine_BrandSpecified_NoExistingBrand) {
  PreventSetupFromRunning(true);

  args_.extra.installation_id = StringToGuid(kExpectedIid);
  args_.extra.brand_code = kExpectedBrand;
  args_.extra.client_id = kExpectedClientId;

  bool is_machine = true;
  bool has_ui_been_displayed = false;
  EXPECT_SUCCEEDED(Install(false,         // is_interactive
                           false,         // is_app_install
                           false,         // is_eula_required
                           false,         // is_oem_install
                           false,         // is_enterprise_install
                           false,         // is_install_elevated_instance
                           _T("foo"),     // install_cmd_line
                           args_,
                           &is_machine,
                           &has_ui_been_displayed));
  EXPECT_FALSE(has_ui_been_displayed);

  const uint32 now = Time64ToInt32(GetCurrent100NSTime());

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
                                    kRegValueInstallTimeSec,
                                    &install_time));
  EXPECT_GE(now, install_time);
  EXPECT_GE(static_cast<uint32>(200), now - install_time);

  DWORD day_of_install = GetDwordValue(MACHINE_REG_CLIENT_STATE_GOOPDATE,
                                       kRegValueDayOfInstall);
  EXPECT_EQ(static_cast<DWORD>(-1), day_of_install);

  EXPECT_SUCCEEDED(scheduled_task_utils::UninstallGoopdateTasks(true));
}


TEST_F(RegistryProtectedWithComInterfacesPrimedInstallTest,
       Install_EulaRequiredNotOffline_User) {
  if (vista_util::IsElevatedWithEnableLUAOn()) {
    std::wcout << _T("\tSkipping test because user is elevated with UAC on.")
               << std::endl;
    return;
  }

  PreventSetupFromRunning(false);

  bool is_machine = false;
  bool has_ui_been_displayed = false;
  EXPECT_EQ(GOOPDATE_E_EULA_REQURED_WITH_ONLINE_INSTALLER,
            Install(false,          // is_interactive
                    true,           // is_app_install
                    true,           // is_eula_required
                    false,          // is_oem_install
                    false,          // is_enterprise_install
                    false,          // is_install_elevated_instance
                    _T("unused"),   // install_cmd_line
                    args_,
                    &is_machine,
                    &has_ui_been_displayed));
  EXPECT_FALSE(has_ui_been_displayed);
}

TEST_F(RegistryProtectedWithComInterfacesPrimedInstallTest,
       Install_EulaRequiredNotOffline_Machine) {
  if (!vista_util::IsUserAdmin()) {
    std::wcout << _T("\tTest did not run because the user is not an admin.")
               << std::endl;
    return;
  }

  PreventSetupFromRunning(true);

  bool is_machine = true;
  bool has_ui_been_displayed = false;
  EXPECT_EQ(GOOPDATE_E_EULA_REQURED_WITH_ONLINE_INSTALLER,
            Install(false,          // is_interactive
                    true,           // is_app_install
                    true,           // is_eula_required
                    false,          // is_oem_install
                    false,          // is_enterprise_install
                    false,          // is_install_elevated_instance
                    _T("unused"),   // install_cmd_line
                    args_,
                    &is_machine,
                    &has_ui_been_displayed));
  EXPECT_FALSE(has_ui_been_displayed);

  EXPECT_SUCCEEDED(scheduled_task_utils::UninstallGoopdateTasks(true));
}

TEST_F(RegistryProtectedInstallTest, Install_NeedsElevation_Silent) {
  if (vista_util::IsUserAdmin()) {
    std::wcout << _T("\tTest did not run because the user IS an admin.")
               << std::endl;
    return;
  }

  bool is_machine = true;
  bool has_ui_been_displayed = false;
  EXPECT_EQ(GOOPDATE_E_SILENT_INSTALL_NEEDS_ELEVATION,
            Install(false,          // is_interactive
                    true,           // is_app_install
                    false,          // is_eula_required
                    false,          // is_oem_install
                    false,          // is_enterprise_install
                    false,          // is_install_elevated_instance
                    _T("unused"),   // install_cmd_line
                    args_,
                    &is_machine,
                    &has_ui_been_displayed));
  EXPECT_FALSE(has_ui_been_displayed);
}

// Tests that non-app installs can request elevation.
// TODO(omaha3): Once the elevation code is finalized, figure out a way to cause
// it to fail without a UAC prompt. Then change the expected error code to a
// list of possible values.
TEST_F(RegistryProtectedInstallTest, Install_NeedsElevation_NotAppInstall) {
  if (vista_util::IsUserAdmin()) {
    std::wcout << _T("\tTest did not run because the user IS an admin.")
               << std::endl;
    return;
  }

  bool is_machine = true;
  bool has_ui_been_displayed = false;
  EXPECT_NE(GOOPDATE_E_SILENT_INSTALL_NEEDS_ELEVATION,
            Install(true,           // is_interactive
                    false,          // is_app_install
                    false,          // is_eula_required
                    false,          // is_oem_install
                    false,          // is_enterprise_install
                    false,          // is_install_elevated_instance
                    _T("unused"),   // install_cmd_line
                    args_,
                    &is_machine,
                    &has_ui_been_displayed));
  EXPECT_FALSE(has_ui_been_displayed);
}

TEST_F(RegistryProtectedInstallTest, Install_NeedsElevation_ElevatedInstance) {
  if (vista_util::IsUserAdmin()) {
    std::wcout << _T("\tTest did not run because the user IS an admin.")
               << std::endl;
    return;
  }

  bool is_machine = true;

  // is_interactive is true to get past that check.
  bool has_ui_been_displayed = false;
  EXPECT_EQ(GOOPDATE_E_INSTALL_ELEVATED_PROCESS_NEEDS_ELEVATION,
            Install(true,           // is_interactive
                    true,           // is_app_install
                    false,          // is_eula_required
                    false,          // is_oem_install
                    false,          // is_enterprise_install
                    true,           // is_install_elevated_instance
                    _T("unused"),   // install_cmd_line
                    args_,
                    &is_machine,
                    &has_ui_been_displayed));
  EXPECT_FALSE(has_ui_been_displayed);
}

// This test will never run because developers do not run XP as non-admin.
// TODO(omaha3): Implement some way to fake/mock IsUserAdmin(), etc.
TEST_F(RegistryProtectedInstallTest, Install_NeedsElevation_XpNonAdmin) {
  if (vista_util::IsVistaOrLater()) {
    std::wcout << _T("\tTest did not run because OS is Vista or later.")
               << std::endl;
    return;
  }
  if (vista_util::IsUserAdmin()) {
    std::wcout << _T("\tTest did not run because the user IS an admin.")
               << std::endl;
    return;
  }

  bool is_machine = true;

  // is_interactive is true to get past that check.
  bool has_ui_been_displayed = false;
  EXPECT_EQ(GOOPDATE_E_NONADMIN_INSTALL_ADMIN_APP,
            Install(true,           // is_interactive
                    true,           // is_app_install
                    false,          // is_eula_required
                    false,          // is_oem_install
                    false,          // is_enterprise_install
                    false,          // is_install_elevated_instance
                    _T("unused"),   // install_cmd_line
                    args_,
                    &is_machine,
                    &has_ui_been_displayed));
  EXPECT_FALSE(has_ui_been_displayed);
}


// TODO(omaha3): Once the elevation code is finalized, figure out a way to cause
// it to fail before the UAC prompt and test GOOPDATE_E_ELEVATION_FAILED_ADMIN
// and GOOPDATE_E_ELEVATION_FAILED_NON_ADMIN. Check the command line used for
// elevation if possible.

// TODO(omaha3): If UI ends up not being handled in this method, test
// is_interactive variations too.

// TODO(omaha3): Test more success cases, including Setup and handoff.

// TODO(omaha3): Enable when support for offline builds is finalized.
#if 0
class SetupOfflineInstallerTest : public testing::Test {
 protected:
  static bool CallCopyOfflineFiles(const CommandLineArgs& args,
                                   const CString& target_location) {
    omaha::Setup setup(false, &args);
    return setup.CopyOfflineFiles(target_location);
  }

  static HRESULT CallCopyOfflineFilesForGuid(const CString& app_guid,
                                             const CString& target_location) {
    return omaha::Setup::CopyOfflineFilesForGuid(app_guid, target_location);
  }
};

TEST_F(SetupOfflineInstallerTest, ValidOfflineInstaller) {
  CString guid_string = _T("{CDABE316-39CD-43BA-8440-6D1E0547AEE6}");

  CString offline_manifest_path(guid_string);
  offline_manifest_path += _T(".gup");
  offline_manifest_path = ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                                          offline_manifest_path);
  EXPECT_SUCCEEDED(File::Copy(
      ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                      _T("server_manifest_one_app.xml")),
      offline_manifest_path,
      false));

  CString installer_exe = _T("foo_installer.exe");
  CString tarred_installer_path;
  tarred_installer_path.Format(_T("%s.%s"), installer_exe, guid_string);
  tarred_installer_path = ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                                          tarred_installer_path);

  EXPECT_SUCCEEDED(File::Copy(
      ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                      kOmahaShellFileName),
      tarred_installer_path,
      false));

  CommandLineArgs args;
  CommandLineAppArgs app1;
  app1.app_guid = StringToGuid(guid_string);
  args.extra.apps.push_back(app1);
  CString target_location = ConcatenatePath(
                                app_util::GetCurrentModuleDirectory(),
                                _T("offline_test"));

  EXPECT_TRUE(CallCopyOfflineFiles(args, target_location));

  CString target_manifest = ConcatenatePath(target_location,
                                            guid_string + _T(".gup"));
  EXPECT_TRUE(File::Exists(target_manifest));
  CString target_file = ConcatenatePath(
      ConcatenatePath(target_location, guid_string), installer_exe);
  EXPECT_TRUE(File::Exists(target_file));

  EXPECT_SUCCEEDED(DeleteDirectory(target_location));
  EXPECT_SUCCEEDED(File::Remove(tarred_installer_path));
  EXPECT_SUCCEEDED(File::Remove(offline_manifest_path));
}

TEST_F(SetupOfflineInstallerTest, NoOfflineInstaller) {
  CString guid_string = _T("{CDABE316-39CD-43BA-8440-6D1E0547AEE6}");
  CommandLineArgs args;
  CommandLineAppArgs app1;
  app1.app_guid = StringToGuid(guid_string);
  args.extra.apps.push_back(app1);
  CString target_location = ConcatenatePath(
                                app_util::GetCurrentModuleDirectory(),
                                _T("offline_test"));

  EXPECT_FALSE(CallCopyOfflineFiles(args, target_location));
}

TEST_F(SetupOfflineInstallerTest, ValidCopyOfflineFilesForGuid) {
  CString guid_string = _T("{CDABE316-39CD-43BA-8440-6D1E0547AEE6}");

  CString offline_manifest_path(guid_string);
  offline_manifest_path += _T(".gup");
  offline_manifest_path = ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                                          offline_manifest_path);
  EXPECT_SUCCEEDED(File::Copy(
      ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                      _T("server_manifest_one_app.xml")),
      offline_manifest_path,
      false));

  CString installer_exe = _T("foo_installer.exe");
  CString tarred_installer_path;
  tarred_installer_path.Format(_T("%s.%s"), installer_exe, guid_string);
  tarred_installer_path = ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                                          tarred_installer_path);

  EXPECT_SUCCEEDED(File::Copy(
      ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                      kOmahaShellFileName),
      tarred_installer_path,
      false));

  CString target_location = ConcatenatePath(
                                app_util::GetCurrentModuleDirectory(),
                                _T("offline_test"));

  EXPECT_SUCCEEDED(CallCopyOfflineFilesForGuid(guid_string, target_location));

  CString target_manifest = ConcatenatePath(target_location,
                                            guid_string + _T(".gup"));
  EXPECT_TRUE(File::Exists(target_manifest));
  CString target_file = ConcatenatePath(
      ConcatenatePath(target_location, guid_string), installer_exe);
  EXPECT_TRUE(File::Exists(target_file));

  EXPECT_SUCCEEDED(DeleteDirectory(target_location));
  EXPECT_SUCCEEDED(File::Remove(tarred_installer_path));
  EXPECT_SUCCEEDED(File::Remove(offline_manifest_path));
}

TEST_F(SetupOfflineInstallerTest, NoCopyOfflineFilesForGuid) {
  CString guid_string = _T("{CDABE316-39CD-43BA-8440-6D1E0547AEE6}");
  CString target_location = ConcatenatePath(
                                app_util::GetCurrentModuleDirectory(),
                                _T("offline_test"));

  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            CallCopyOfflineFilesForGuid(guid_string, target_location));
}
#endif

// TODO(omaha): Duplicate these tests for machine.
TEST_F(InstallHandoffUserTest, InstallApplications_HandoffWithShellMissing) {
  CString shell_path = ConcatenatePath(omaha_path_, kOmahaShellFileName);
  EXPECT_TRUE(SUCCEEDED(File::DeleteAfterReboot(shell_path)) ||
              !vista_util::IsUserAdmin());
  EXPECT_FALSE(File::Exists(shell_path));

  CommandLineArgs args;
  args.extra_args_str = _T("appguid={BF85992F-2E0F-4700-9A6C-FEC9126CEE4B}&")
                        _T("appname=Foo&needsadmin=False&");
  args.install_source = _T("unittest");
  bool ui_displayed = false;
  bool has_launched_handoff = false;
  EXPECT_EQ(GOOPDATE_E_HANDOFF_FAILED,
            internal::InstallApplications(false,
                                          false,
                                          args,
                                          kSessionId_,
                                          NULL,
                                          &ui_displayed,
                                          &has_launched_handoff));
// TODO(omaha3): Verify the actual error when this is implemented.
#if 0
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), extra_code1);
#endif
  EXPECT_FALSE(ui_displayed);
}

TEST_F(InstallHandoffUserTest,
       InstallApplications_HandoffWithGoopdateDllMissing) {
  CString dll_path = ConcatenatePath(path_, kOmahaDllName);
  EXPECT_SUCCEEDED(File::Remove(dll_path));
  EXPECT_FALSE(File::Exists(dll_path));

  CommandLineArgs args;
  args.extra_args_str = _T("appguid={BF85992F-2E0F-4700-9A6C-FEC9126CEE4B}&")
                        _T("appname=Foo&needsadmin=False&");
  args.install_source = _T("unittest");
  bool ui_displayed = false;
  bool has_launched_handoff = false;
  EXPECT_EQ(GOOGLEUPDATE_E_DLL_NOT_FOUND,
            internal::InstallApplications(false,
                                          false,
                                          args,
                                          kSessionId_,
                                          NULL,
                                          &ui_displayed,
                                          &has_launched_handoff));
// TODO(omaha3): Verify the actual error when this is implemented.
#if 0
  EXPECT_EQ(0, setup_->extra_code1());
#endif
}

}  // namespace omaha
