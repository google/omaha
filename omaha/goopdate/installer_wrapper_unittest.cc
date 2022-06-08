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

#include "omaha/goopdate/installer_wrapper.h"

#include <atlpath.h>
#include <atlstr.h>
#include <memory>

#include "omaha/base/app_util.h"
#include "omaha/base/error.h"
#include "omaha/base/file.h"
#include "omaha/base/path.h"
#include "omaha/base/process.h"
#include "omaha/base/reg_key.h"
#include "omaha/base/shell.h"
#include "omaha/base/system.h"
#include "omaha/base/timer.h"
#include "omaha/base/utils.h"
#include "omaha/base/vistautil.h"
#include "omaha/common/app_registry_utils.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/common/install_manifest.h"
#include "omaha/goopdate/app_manager.h"
#include "omaha/goopdate/model.h"
#include "omaha/goopdate/resource_manager.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace {

const TCHAR kAppId[] = _T("{B18BC01B-E0BD-4BF0-A33E-1133055E5FDE}");
const GUID kAppGuid = {0xB18BC01B, 0xE0BD, 0x4BF0,
                       {0xA3, 0x3E, 0x11, 0x33, 0x05, 0x5E, 0x5F, 0xDE}};

const TCHAR kFullAppClientsKeyPath[] =
    _T("HKCU\\Software\\") PATH_COMPANY_NAME _T("\\")
    PRODUCT_NAME _T("\\Clients\\{B18BC01B-E0BD-4BF0-A33E-1133055E5FDE}");
const TCHAR kFullAppClientStateKeyPath[] =
    _T("HKCU\\Software\\") PATH_COMPANY_NAME _T("\\")
    PRODUCT_NAME _T("\\ClientState\\{B18BC01B-E0BD-4BF0-A33E-1133055E5FDE}");
const TCHAR kFullFooAppClientKeyPath[] =
    _T("HKLM\\Software\\") PATH_COMPANY_NAME _T("\\")
    PRODUCT_NAME _T("\\Clients\\{D6B08267-B440-4C85-9F79-E195E80D9937}");
const TCHAR kFullFooAppClientStateKeyPath[] =
    _T("HKLM\\Software\\") PATH_COMPANY_NAME _T("\\")
    PRODUCT_NAME _T("\\ClientState\\{D6B08267-B440-4C85-9F79-E195E80D9937}");

const TCHAR kSetupFooV1RelativeLocation[] =
  _T("unittest_support\\test_foo_v1.0.101.0.msi");
const TCHAR kFooGuid[] = _T("{D6B08267-B440-4C85-9F79-E195E80D9937}");
const TCHAR kFooInstallerBarPropertyArg[] = _T("PROPBAR=7");
const TCHAR kFooInstallerBarValueName[] = _T("propbar");

// Values related to using cmd.exe as an "installer".
const TCHAR kCmdExecutable[] = _T("cmd.exe");
const TCHAR kExecuteCommandAndTerminateSwitch[] = _T("/c %s");
const TCHAR kExecuteTwoCommandsFormat[] = _T("\"%s & %s\"");

const TCHAR kMsiLogFormat[] = _T("%s.log");

const TCHAR kMsiUninstallArguments[] = _T("/quiet /uninstall \"%s\"");
const TCHAR kMsiCommand[] = _T("msiexec");

const DWORD kInitialErrorValue = 5;
const TCHAR kMeaninglessErrorString[] = _T("This is an error string.");

// Some error strings are slightly different on Vista than XP because spaces
// were removed. Therefore, the comparison must ignore that space.

// The US English error string for ERROR_INSTALL_PACKAGE_OPEN_FAILED.
const TCHAR kMsiPackageOpenFailedStringPartA[] =
    _T("This installation package could not be opened. ");
const TCHAR kMsiPackageOpenFailedStringPartB[] =
    _T("Verify that the package exists and that you can access it, ")
    _T("or contact the application vendor to verify that this is a ")
    _T("valid Windows Installer package. ");

// The US English error string for ERROR_INSTALL_ALREADY_RUNNING.
const TCHAR kMsiBusyStringPartA[] =
    _T("Another installation is already in progress. ");
const TCHAR kMsiBusyStringPartB[] =
    _T("Complete that installation before proceeding with this install. ");

const TCHAR* const kError1603Text =
    _T("The installer encountered error 1603: Fatal error during ")
    _T("installation. ");

const TCHAR kError1618MessagePrefix[] =
    _T("The installer encountered error 1618: ");
const int kError1618MessagePrefixLength =
    arraysize(kError1618MessagePrefix) - 1;

const TCHAR* const kError0x800B010FText =
    _T("The installer encountered error 0x800b010f: ")
    _T("The certificate's CN name does not match the passed value. ");

const TCHAR* const kLaunchCmdLine =
      _T("\"C:\\Local\\Google\\Chrome\\Application\\chrome.exe\" -home");

const TCHAR* const kLanguageEnglish = _T("en");

const int kMsiAlreadyRunningRetryDelayBaseMs = 5000;
const int kNumMsiTriesDefault = 4;  // Up to 35 seconds.
const int kNumMsiTriesOnBuildSystem = 7;  // Up to 6.25 minutes.

int GetNumMsiTries() {
  return IsBuildSystem() ? kNumMsiTriesOnBuildSystem : kNumMsiTriesDefault;
}

}  // namespace

extern const TCHAR kRegExecutable[] = _T("reg.exe");
extern const TCHAR kSetInstallerResultTypeMsiErrorRegCmdArgs[] =
    _T("add \"HKCU\\Software\\") PATH_COMPANY_NAME _T("\\") PRODUCT_NAME
    _T("\\ClientState\\{B18BC01B-E0BD-4BF0-A33E-1133055E5FDE}\" ")
    _T("/v InstallerResult /t REG_DWORD /d 2 /f");
extern const TCHAR kMsiInstallerBusyExitCodeCmd[] = _T("exit 1618");

extern const TCHAR kError1619MessagePrefix[] =
    _T("The installer encountered error 1619: ");
extern const int kError1619MessagePrefixLength =
    arraysize(kError1619MessagePrefix) - 1;

void VerifyStringIsMsiPackageOpenFailedString(const CString& str) {
  EXPECT_STREQ(kMsiPackageOpenFailedStringPartA,
               str.Left(arraysize(kMsiPackageOpenFailedStringPartA) - 1));
  EXPECT_STREQ(kMsiPackageOpenFailedStringPartB,
               str.Right(arraysize(kMsiPackageOpenFailedStringPartB) - 1));
}

void VerifyStringIsMsiBusyString(const CString& str) {
  EXPECT_STREQ(kMsiBusyStringPartA,
               str.Left(arraysize(kMsiBusyStringPartA) - 1));
  EXPECT_STREQ(kMsiBusyStringPartB,
               str.Right(arraysize(kMsiBusyStringPartB) - 1));
}

// Unit tests may run while other updaters are running on the build system.
// Give the tests lots of time to run to avoid false negatives.
void AdjustMsiTries(InstallerWrapper* installer_wrapper) {
  ASSERT1(installer_wrapper);
  installer_wrapper->set_num_tries_when_msi_busy(GetNumMsiTries());
}

// Waits for the uninstall to complete to avoid race conditions with other tests
// that install the same MSI. It appears msiexec causes an an asynchronous
// request that may be processed out of order.
// Retries when ERROR_INSTALL_ALREADY_RUNNING is encountered.
void UninstallTestMsi(const CString& installer_path) {
  CString uninstall_arguments;
  uninstall_arguments.Format(kMsiUninstallArguments, installer_path);

  const int max_tries = GetNumMsiTries();
  int retry_delay = kMsiAlreadyRunningRetryDelayBaseMs;
  int num_tries(0);
  uint32 exit_code = ERROR_INSTALL_ALREADY_RUNNING;
  for (num_tries = 0;
       exit_code == ERROR_INSTALL_ALREADY_RUNNING && num_tries < max_tries;
       ++num_tries) {
    if (0 < num_tries) {
      // Retrying - wait between attempts.
      ::Sleep(retry_delay);
      retry_delay *= 2;  // Double the retry delay next time.
    }

    Process p(kMsiCommand, NULL);
    EXPECT_HRESULT_SUCCEEDED(p.Start(uninstall_arguments, NULL));
    EXPECT_TRUE(p.WaitUntilDead(10000));

    EXPECT_TRUE(p.GetExitCode(&exit_code));
  }

  EXPECT_EQ(0, exit_code);
}

class InstallerWrapperTest : public testing::Test {
 protected:
  explicit InstallerWrapperTest(bool is_machine)
      : is_machine_(is_machine) {
  }

  static void SetUpTestCase() {
    CString system_path;
    EXPECT_SUCCEEDED(Shell::GetSpecialFolder(CSIDL_SYSTEM,
                                             false,
                                             &system_path));
    EXPECT_FALSE(system_path.IsEmpty());
    cmd_exe_path_.Combine(system_path, kCmdExecutable);
    EXPECT_TRUE(File::Exists(cmd_exe_path_));

    CPath reg_path;
    reg_path.Combine(system_path, kRegExecutable);
    set_installer_result_type_msi_error_cmd_.Format(
        _T("%s %s"),
        reg_path, kSetInstallerResultTypeMsiErrorRegCmdArgs);
  }

  virtual void SetUp() {
    RegKey::DeleteKey(kFullAppClientsKeyPath);
    RegKey::DeleteKey(kFullAppClientStateKeyPath);

    EXPECT_SUCCEEDED(AppManager::CreateInstance(is_machine_));

    iw_.reset(new InstallerWrapper(is_machine_));
    EXPECT_SUCCEEDED(iw_->Initialize());

    EXPECT_SUCCEEDED(ResourceManager::Create(
          is_machine_, app_util::GetCurrentModuleDirectory(), _T("en")));
  }

  virtual void TearDown() {
    AppManager::DeleteInstance();

    ResourceManager::Delete();
  }

  void SetupInstallerResultRegistry(const CString& app_guid,
                                    bool set_installer_result,
                                    DWORD installer_result,
                                    bool set_installer_error,
                                    DWORD installer_error,
                                    bool set_installer_extra_code1,
                                    DWORD installer_extra_code1,
                                    bool set_installer_result_uistring,
                                    const CString& installer_result_uistring,
                                    bool set_installer_launch_cmd_line,
                                    const CString& installer_launch_cmd_line) {
    CString app_client_state_key =
        app_registry_utils::GetAppClientStateKey(is_machine_, app_guid);
    RegKey::CreateKey(app_client_state_key);
    if (set_installer_result) {
      RegKey::SetValue(app_client_state_key,
                       kRegValueInstallerResult,
                       installer_result);
    }

    if (set_installer_error) {
      RegKey::SetValue(app_client_state_key,
                       kRegValueInstallerError,
                       installer_error);
    }

    if (set_installer_extra_code1) {
      RegKey::SetValue(app_client_state_key,
                       kRegValueInstallerExtraCode1,
                       installer_extra_code1);
    }

    if (set_installer_result_uistring) {
      RegKey::SetValue(app_client_state_key,
                       kRegValueInstallerResultUIString,
                       installer_result_uistring);
    }

    if (set_installer_launch_cmd_line) {
      RegKey::SetValue(app_client_state_key,
                       kRegValueInstallerSuccessLaunchCmdLine,
                       installer_launch_cmd_line);
    }
  }

  void VerifyLastRegistryValues(const CString& app_guid,
                                bool expect_installer_result,
                                DWORD expected_installer_result,
                                bool expect_installer_error,
                                DWORD expected_installer_error,
                                bool expect_installer_extra_code1,
                                DWORD expected_installer_extra_code1,
                                bool expect_installer_result_uistring,
                                const CString& expected_result_uistring,
                                bool expect_installer_launch_cmd_line,
                                const CString& expected_launch_cmd_line) {
    ASSERT_TRUE(expect_installer_result || !expected_installer_result);
    ASSERT_TRUE(expect_installer_error || !expected_installer_error);
    ASSERT_TRUE(expect_installer_extra_code1 ||
                !expected_installer_extra_code1);
    ASSERT_TRUE(expect_installer_result_uistring ||
                expected_result_uistring.IsEmpty());
    ASSERT_TRUE(expect_installer_launch_cmd_line ||
                expected_launch_cmd_line.IsEmpty());

    CString app_client_state_key =
        app_registry_utils::GetAppClientStateKey(is_machine_, app_guid);
    EXPECT_FALSE(RegKey::HasValue(app_client_state_key,
                                  kRegValueInstallerResult));
    EXPECT_FALSE(RegKey::HasValue(app_client_state_key,
                                  kRegValueInstallerError));
    EXPECT_FALSE(RegKey::HasValue(app_client_state_key,
                                  kRegValueInstallerExtraCode1));
    EXPECT_FALSE(RegKey::HasValue(app_client_state_key,
                                  kRegValueInstallerResultUIString));

    if (expect_installer_result) {
      EXPECT_TRUE(RegKey::HasValue(app_client_state_key,
                                   kRegValueLastInstallerResult));
      DWORD last_installer_result = 0;
      EXPECT_SUCCEEDED(RegKey::GetValue(app_client_state_key,
                                        kRegValueLastInstallerResult,
                                        &last_installer_result));
      EXPECT_EQ(expected_installer_result, last_installer_result);
    } else {
      EXPECT_FALSE(RegKey::HasValue(app_client_state_key,
                                    kRegValueLastInstallerResult));
    }

    if (expect_installer_error) {
      EXPECT_TRUE(RegKey::HasValue(app_client_state_key,
                                   kRegValueLastInstallerError));
      DWORD last_installer_error = 0;
      EXPECT_SUCCEEDED(RegKey::GetValue(app_client_state_key,
                                        kRegValueLastInstallerError,
                                        &last_installer_error));
      EXPECT_EQ(expected_installer_error, last_installer_error);
    } else {
      EXPECT_FALSE(RegKey::HasValue(app_client_state_key,
                                    kRegValueLastInstallerError));
    }

    if (expect_installer_extra_code1) {
      EXPECT_TRUE(RegKey::HasValue(app_client_state_key,
                                   kRegValueLastInstallerExtraCode1));
      DWORD last_installer_extracode1 = 0;
      EXPECT_SUCCEEDED(RegKey::GetValue(app_client_state_key,
                                        kRegValueLastInstallerExtraCode1,
                                        &last_installer_extracode1));
      EXPECT_EQ(expected_installer_extra_code1, last_installer_extracode1);
    } else {
      EXPECT_FALSE(RegKey::HasValue(app_client_state_key,
                                    kRegValueLastInstallerExtraCode1));
    }

    if (expect_installer_result_uistring) {
      EXPECT_TRUE(RegKey::HasValue(app_client_state_key,
                                   kRegValueLastInstallerResultUIString));
      CString last_installer_result_uistring;
      EXPECT_SUCCEEDED(RegKey::GetValue(app_client_state_key,
                                        kRegValueLastInstallerResultUIString,
                                        &last_installer_result_uistring));
      EXPECT_STREQ(expected_result_uistring,
                   last_installer_result_uistring);
    } else {
      EXPECT_FALSE(RegKey::HasValue(app_client_state_key,
                                    kRegValueLastInstallerResultUIString));
    }

    if (expect_installer_launch_cmd_line) {
      EXPECT_TRUE(RegKey::HasValue(app_client_state_key,
                                   kRegValueLastInstallerSuccessLaunchCmdLine));
      CString last_installer_launch_cmd_line;
      EXPECT_SUCCEEDED(
          RegKey::GetValue(app_client_state_key,
                           kRegValueLastInstallerSuccessLaunchCmdLine,
                           &last_installer_launch_cmd_line));
      EXPECT_STREQ(expected_launch_cmd_line,
                   last_installer_launch_cmd_line);
    } else {
      EXPECT_FALSE(RegKey::HasValue(
          app_client_state_key,
          kRegValueLastInstallerSuccessLaunchCmdLine));
    }
  }

  void VerifyNoLastRegistryValues(const CString& app_guid) {
    VerifyLastRegistryValues(app_guid,
                             false, 0,
                             false, 0,
                             false, 0,
                             false, _T(""),
                             false, _T(""));
  }

  void CallGetInstallerResultHelper(const GUID& app_guid,
                                    int installer_type,
                                    uint32 exit_code,
                                    InstallerResultInfo* result_info) {
    ASSERT1(result_info);

    iw_->GetInstallerResultHelper(
        app_guid,
        static_cast<InstallerWrapper::InstallerType>(installer_type),
        exit_code,
        kLanguageEnglish,
        result_info);
  }

  static const int kResultSuccess = AppManager::INSTALLER_RESULT_SUCCESS;
  static const int kResultFailedCustomError =
      AppManager::INSTALLER_RESULT_FAILED_CUSTOM_ERROR;
  static const int kResultFailedMsiError =
      AppManager::INSTALLER_RESULT_FAILED_MSI_ERROR;
  static const int kResultFailedSystemError =
      AppManager::INSTALLER_RESULT_FAILED_SYSTEM_ERROR;
  static const int kResultExitCode = AppManager::INSTALLER_RESULT_EXIT_CODE;

  static const int kMsiInstaller = InstallerWrapper::MSI_INSTALLER;
  static const int kOtherInstaller = InstallerWrapper::CUSTOM_INSTALLER;

  bool is_machine_;
  std::unique_ptr<InstallerWrapper> iw_;

  // Used as an argument to various functions.
  InstallerResultInfo result_info_;

  static CPath cmd_exe_path_;
  static CString set_installer_result_type_msi_error_cmd_;
};

CPath InstallerWrapperTest::cmd_exe_path_;
CString InstallerWrapperTest::set_installer_result_type_msi_error_cmd_;

class InstallerWrapperMachineTest : public InstallerWrapperTest {
 protected:
  InstallerWrapperMachineTest()
    : InstallerWrapperTest(true) {
  }
};

class InstallerWrapperUserTest : public InstallerWrapperTest {
 protected:
  InstallerWrapperUserTest()
    : InstallerWrapperTest(false) {
  }
};

class InstallerWrapperUserGetInstallerResultHelperTest
    : public InstallerWrapperUserTest {
 protected:
  InstallerWrapperUserGetInstallerResultHelperTest()
      : InstallerWrapperUserTest() {
    result_info_.text = kMeaninglessErrorString;
  }

  virtual void SetUp() {
    InstallerWrapperUserTest::SetUp();
    AppManager::Instance()->GetRegistryStableStateLock().Lock();
  }

  virtual void TearDown() {
    AppManager::Instance()->GetRegistryStableStateLock().Unlock();
    InstallerWrapperUserTest::TearDown();
  }

  // Shorter names used to make test calls fit on one line.
  static const int kMsi = kMsiInstaller;
  static const int kOther = kOtherInstaller;
};

//
// Helper method tests
//
TEST(InstallerWrapperTest, GetMessageForSystemErrorCode) {
  VerifyStringIsMsiPackageOpenFailedString(
      GetMessageForSystemErrorCode(ERROR_INSTALL_PACKAGE_OPEN_FAILED));
}

// CheckApplicationRegistration does not read the registry. This is verified by
// not setting any registry values before calling it.

TEST_F(InstallerWrapperUserTest,
       CheckApplicationRegistration_EmptyRegisteredVersion) {
  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_DID_NOT_WRITE_CLIENTS_KEY,
            iw_->CheckApplicationRegistration(
                kAppGuid, _T(""), _T(""), _T(""), false));
  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_DID_NOT_WRITE_CLIENTS_KEY,
            iw_->CheckApplicationRegistration(
                kAppGuid, _T(""), _T(""), _T(""), true));
  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_DID_NOT_WRITE_CLIENTS_KEY,
            iw_->CheckApplicationRegistration(
                kAppGuid, _T(""), _T("1.2.3.4"), _T(""), false));
  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_DID_NOT_WRITE_CLIENTS_KEY,
            iw_->CheckApplicationRegistration(
                kAppGuid, _T(""), _T("1.2.3.4"), _T(""), true));
  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_DID_NOT_WRITE_CLIENTS_KEY,
            iw_->CheckApplicationRegistration(
                kAppGuid, _T(""), _T("1.2.3.4"), _T("1.2.3.3"), false));
  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_DID_NOT_WRITE_CLIENTS_KEY,
            iw_->CheckApplicationRegistration(
                kAppGuid, _T(""), _T("1.2.3.4"), _T("1.2.3.3"), true));
}

TEST_F(InstallerWrapperUserTest,
       CheckApplicationRegistration_NoExpectedOrPreviousVersion) {
  EXPECT_SUCCEEDED(iw_->CheckApplicationRegistration(
                       kAppGuid, _T("0.9.6.4"), _T(""), _T(""), false));
  EXPECT_SUCCEEDED(iw_->CheckApplicationRegistration(
                       kAppGuid, _T("0.9.6.4"), _T(""), _T(""), true));
}

TEST_F(InstallerWrapperUserTest,
       CheckApplicationRegistration_ExpectedMatch_NoPreviousVersion) {
  EXPECT_SUCCEEDED(iw_->CheckApplicationRegistration(
                       kAppGuid, _T("0.9.6.4"), _T("0.9.6.4"), _T(""), false));
  EXPECT_SUCCEEDED(iw_->CheckApplicationRegistration(
                       kAppGuid, _T("0.9.6.4"), _T("0.9.6.4"), _T(""), true));
}

TEST_F(InstallerWrapperUserTest,
       CheckApplicationRegistration_NoExpectedVersion_PreviousVersionOlder) {
  EXPECT_SUCCEEDED(iw_->CheckApplicationRegistration(
                       kAppGuid, _T("0.9.6.4"), _T(""), _T("0.9.6.3"), false));
  EXPECT_SUCCEEDED(iw_->CheckApplicationRegistration(
                       kAppGuid, _T("0.9.6.4"), _T(""), _T("0.9.6.3"), true));
}

TEST_F(InstallerWrapperUserTest,
       CheckApplicationRegistration_ExpectedMatch_PreviousVersionSame_UnchangedAllowed) {  // NOLINT
  EXPECT_SUCCEEDED(
      iw_->CheckApplicationRegistration(
          kAppGuid, _T("0.9.6.4"), _T(""), _T("0.9.6.4"), false));
  EXPECT_SUCCEEDED(
      iw_->CheckApplicationRegistration(
          kAppGuid, _T("0.9.6.4"), _T("0.9.6.4"), _T("0.9.6.4"), false));
}

TEST_F(InstallerWrapperUserTest,
       CheckApplicationRegistration_ExpectedMatch_PreviousVersionSame_UnchangedNotAllowed) {  // NOLINT
  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_DID_NOT_CHANGE_VERSION,
            iw_->CheckApplicationRegistration(
                kAppGuid, _T("0.9.6.4"), _T(""), _T("0.9.6.4"), true));

  ExpectAsserts expect_asserts;  // expected_version is expected to be greater.
  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_DID_NOT_CHANGE_VERSION,
            iw_->CheckApplicationRegistration(
                kAppGuid, _T("0.9.6.4"), _T("0.9.6.4"), _T("0.9.6.4"), true));
}

TEST_F(InstallerWrapperUserTest,
       CheckApplicationRegistration_ExpectedMatch_PreviousVersionOlder) {
  EXPECT_SUCCEEDED(
      iw_->CheckApplicationRegistration(
          kAppGuid, _T("0.9.6.4"), _T("0.9.6.4"), _T("0.9.6.3"), false));
  EXPECT_SUCCEEDED(
      iw_->CheckApplicationRegistration(
          kAppGuid, _T("0.9.6.4"), _T("0.9.6.4"), _T("0.9.6.3"), true));
}

TEST_F(InstallerWrapperUserTest,
       CheckApplicationRegistration_ExpectedMatch_PreviousVersionNewer) {
  EXPECT_SUCCEEDED(
      iw_->CheckApplicationRegistration(
          kAppGuid, _T("0.9.6.4"), _T("0.9.6.4"), _T("0.9.6.5"), false));
  EXPECT_SUCCEEDED(
      iw_->CheckApplicationRegistration(
          kAppGuid, _T("0.9.6.4"), _T("0.9.6.4"), _T("0.9.6.5"), true));
}

// This is the over-install when a newer version is present case. This might
// happen if a dev track version is installed and a stable track version is
// installed. For installs, this is okay.
TEST_F(InstallerWrapperUserTest,
       CheckApplicationRegistration_ExpectedMatch_RegisteredNewer_NoPreviousVersion_NewInstall) {  // NOLINT
  EXPECT_SUCCEEDED(
      iw_->CheckApplicationRegistration(
          kAppGuid, _T("0.9.6.4"), _T("0.9.6.3"), _T(""), false));
}

TEST_F(InstallerWrapperUserTest,
       CheckApplicationRegistration_ExpectedMismatch_RegisteredNewer_NoPreviousVersion_Update) {  // NOLINT
  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_VERSION_MISMATCH,
            iw_->CheckApplicationRegistration(
                kAppGuid, _T("0.9.6.4"), _T("0.9.6.3"), _T(""), true));
}

// This is the over-install when a newer version is present case. This might
// happen if a dev track version is installed and a stable track version is
// installed. For installs, this is okay.
TEST_F(InstallerWrapperUserTest,
       CheckApplicationRegistration_ExpectedMismatch_RegisteredNewer_PreviousVersionSame) {  // NOLINT
  EXPECT_SUCCEEDED(
            iw_->CheckApplicationRegistration(
                kAppGuid, _T("0.9.6.4"), _T("0.9.6.3"), _T("0.9.6.4"), false));
  ExpectAsserts expect_asserts;  // expected_version is expected to be greater.
  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_DID_NOT_CHANGE_VERSION,
            iw_->CheckApplicationRegistration(
                kAppGuid, _T("0.9.6.4"), _T("0.9.6.3"), _T("0.9.6.4"), true));
}

TEST_F(InstallerWrapperUserTest,
       CheckApplicationRegistration_ExpectedMismatch_RegisteredNewer_PreviousVersionDifferent) {  // NOLINT
  EXPECT_SUCCEEDED(
            iw_->CheckApplicationRegistration(
                kAppGuid, _T("0.9.6.4"), _T("0.9.6.3"), _T("0.9.6.3"), false));
  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_VERSION_MISMATCH,
            iw_->CheckApplicationRegistration(
                kAppGuid, _T("0.9.6.4"), _T("0.9.6.3"), _T("0.9.6.3"), true));
}

TEST_F(InstallerWrapperUserTest,
       CheckApplicationRegistration_ExpectedMismatch_RegisteredOlder_NoPreviousVersion) {  // NOLINT
  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_VERSION_MISMATCH,
            iw_->CheckApplicationRegistration(
                kAppGuid, _T("0.9.6.2"), _T("0.9.6.3"), _T(""), false));
  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_VERSION_MISMATCH,
            iw_->CheckApplicationRegistration(
                kAppGuid, _T("0.9.6.2"), _T("0.9.6.3"), _T(""), true));
}

TEST_F(InstallerWrapperUserTest,
       CheckApplicationRegistration_ExpectedMismatch_RegisteredOlder_PreviousVersionSame) {  // NOLINT
  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_VERSION_MISMATCH,
            iw_->CheckApplicationRegistration(
                kAppGuid, _T("0.9.6.2"), _T("0.9.6.3"), _T("0.9.6.2"), false));
  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_VERSION_MISMATCH,
            iw_->CheckApplicationRegistration(
                kAppGuid, _T("0.9.6.2"), _T("0.9.6.3"), _T("0.9.6.2"), true));
}

TEST_F(InstallerWrapperUserTest,
       CheckApplicationRegistration_ExpectedMismatch_RegisteredOlder_PreviousVersionDifferent) {  // NOLINT
  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_VERSION_MISMATCH,
            iw_->CheckApplicationRegistration(
                kAppGuid, _T("0.9.6.2"), _T("0.9.6.3"), _T("0.9.6.1"), false));
  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_VERSION_MISMATCH,
            iw_->CheckApplicationRegistration(
                kAppGuid, _T("0.9.6.2"), _T("0.9.6.3"), _T("0.9.6.1"), true));
}

TEST_F(InstallerWrapperUserTest,
       CheckApplicationRegistration_ExpectedMismatch_RegisteredNewer_PreviousVersionSame_UnrecognizedVersions) {  // NOLINT
  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_VERSION_MISMATCH,
            iw_->CheckApplicationRegistration(
                kAppGuid, _T("0.9.6.4.0"), _T("0.9.6.3"), _T("0.9.6.4.0"),
                false));
  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_VERSION_MISMATCH,
            iw_->CheckApplicationRegistration(
                kAppGuid, _T("0.9.6.4.0"), _T("0.9.6.3"), _T("0.9.6.4.0"),
                true));

  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_VERSION_MISMATCH,
            iw_->CheckApplicationRegistration(
                kAppGuid, _T("0.9.6.4"), _T("0.9.6.3.0"), _T("0.9.6.4"),
                false));
  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_VERSION_MISMATCH,
            iw_->CheckApplicationRegistration(
                kAppGuid, _T("0.9.6.4"), _T("0.9.6.3.0"), _T("0.9.6.4"),
                true));

  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_VERSION_MISMATCH,
            iw_->CheckApplicationRegistration(
                kAppGuid, _T("0.9.6.4.0"), _T("0.9.6.3.0"), _T("0.9.6.4.0"),
                false));
  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_VERSION_MISMATCH,
            iw_->CheckApplicationRegistration(
                kAppGuid, _T("0.9.6.4.0"), _T("0.9.6.3.0"), _T("0.9.6.4.0"),
                true));

  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_VERSION_MISMATCH,
            iw_->CheckApplicationRegistration(
                kAppGuid, _T("0.9.6"), _T("0.9.5"), _T("0.9.6"), false));
  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_VERSION_MISMATCH,
            iw_->CheckApplicationRegistration(
                kAppGuid, _T("0.9.6"), _T("0.9.5"), _T("0.9.6"), true));
}

//
// Negative Tests
//

TEST_F(InstallerWrapperUserTest, InstallApp_InstallerWithoutFilenameExtension) {
  EXPECT_EQ(GOOPDATEINSTALL_E_FILENAME_INVALID,
            iw_->InstallApp(NULL,
                            kAppGuid,
                            _T("c:\\temp\\foo"),
                            _T(""),  // Arguments.
                            _T(""),  // Installer data.
                            kLanguageEnglish,
                            _T(""),  // Untrusted data.
                            0,
                            &result_info_));

  EXPECT_EQ(INSTALLER_RESULT_UNKNOWN, result_info_.type);
  EXPECT_EQ(S_OK, result_info_.code);
  EXPECT_TRUE(result_info_.text.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_launch_command_line.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_url.IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, result_info_.post_install_action);
}

TEST_F(InstallerWrapperUserTest,
       InstallApp_UnsupportedInstallerFilenameExtension) {
  EXPECT_EQ(GOOPDATEINSTALL_E_FILENAME_INVALID,
            iw_->InstallApp(NULL,
                            kAppGuid,
                            _T("c:\\temp\\foo.bar"),
                            _T(""),  // Arguments.
                            _T(""),  // Installer data.
                            kLanguageEnglish,
                            _T(""),  // Untrusted data.
                            0,
                            &result_info_));

  EXPECT_EQ(INSTALLER_RESULT_UNKNOWN, result_info_.type);
  EXPECT_EQ(S_OK, result_info_.code);
  EXPECT_TRUE(result_info_.text.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_launch_command_line.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_url.IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, result_info_.post_install_action);
}

TEST_F(InstallerWrapperUserTest, InstallApp_InstallerEmptyFilename) {
  EXPECT_EQ(GOOPDATEINSTALL_E_FILENAME_INVALID,
            iw_->InstallApp(NULL,
                            kAppGuid,
                            _T(""),
                            _T(""),  // Arguments.
                            _T(""),  // Installer data.
                            kLanguageEnglish,
                            _T(""),  // Untrusted data.
                            0,
                            &result_info_));

  EXPECT_EQ(INSTALLER_RESULT_UNKNOWN, result_info_.type);
  EXPECT_EQ(S_OK, result_info_.code);
  EXPECT_TRUE(result_info_.text.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_launch_command_line.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_url.IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, result_info_.post_install_action);
}

TEST_F(InstallerWrapperUserTest, InstallApp_ExeFileDoesNotExist) {
  __mutexScope(AppManager::Instance()->GetRegistryStableStateLock());
  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_FAILED_START,
            iw_->InstallApp(NULL,
                            kAppGuid,
                            _T("c:\\temp\\foo.exe"),
                            _T(""),  // Arguments.
                            _T(""),  // Installer data.
                            kLanguageEnglish,
                            _T(""),  // Untrusted data.
                            0,
                            &result_info_));

  EXPECT_EQ(INSTALLER_RESULT_UNKNOWN, result_info_.type);
  EXPECT_EQ(S_OK, result_info_.code);
  EXPECT_TRUE(result_info_.text.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_launch_command_line.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_url.IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, result_info_.post_install_action);

  EXPECT_FALSE(RegKey::HasKey(kFullAppClientsKeyPath));
  EXPECT_FALSE(RegKey::HasKey(kFullAppClientStateKeyPath));
}

//
// EXE Installer Tests
//

// This test uses cmd.exe as an installer that leaves the payload
// kPayloadFileName.
TEST_F(InstallerWrapperUserTest, InstallApp_ExeInstallerWithArgumentsSucceeds) {
  const TCHAR kPayloadFileName[] = _T("exe_payload.txt");
  const TCHAR kCommandToExecute[] = _T("echo \"hi\" > %s");

  CString full_command_to_execute;
  full_command_to_execute.Format(kCommandToExecute, kPayloadFileName);
  CString arguments;
  arguments.Format(kExecuteCommandAndTerminateSwitch, full_command_to_execute);

  EXPECT_SUCCEEDED(File::Remove(kPayloadFileName));
  EXPECT_FALSE(File::Exists(kPayloadFileName));

  // Create the Clients key since this isn't an actual installer.
  EXPECT_SUCCEEDED(RegKey::CreateKey(kFullAppClientsKeyPath));
  EXPECT_TRUE(RegKey::HasKey(kFullAppClientsKeyPath));
  EXPECT_SUCCEEDED(RegKey::SetValue(kFullAppClientsKeyPath,
                                    kRegValueProductVersion,
                                    _T("0.10.69.5")));

  __mutexScope(AppManager::Instance()->GetRegistryStableStateLock());
  EXPECT_SUCCEEDED(iw_->InstallApp(NULL,
                                   kAppGuid,
                                   cmd_exe_path_,
                                   arguments,
                                   _T(""),  // Installer data.
                                   kLanguageEnglish,
                                   _T(""),  // Untrusted data.
                                   0,
                                   &result_info_));

  EXPECT_EQ(INSTALLER_RESULT_SUCCESS, result_info_.type);
  EXPECT_EQ(S_OK, result_info_.code);
  EXPECT_TRUE(result_info_.text.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_launch_command_line.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_url.IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, result_info_.post_install_action);

  // EXPECT_SUCCEEDED(
  //     iw_->CheckApplicationRegistration(kAppGuid, _T("0.9.70.1"), false));

  EXPECT_TRUE(File::Exists(kPayloadFileName));
  EXPECT_SUCCEEDED(File::Remove(kPayloadFileName));
}

TEST_F(InstallerWrapperUserTest,
       InstallApp_ExeInstallerReturnsNonZeroExitCode) {
  const TCHAR kCommandToExecute[] = _T("exit 1");

  CString arguments;
  arguments.Format(kExecuteCommandAndTerminateSwitch, kCommandToExecute);

  // Create the Clients key since this isn't an actual installer.
  ASSERT_SUCCEEDED(RegKey::CreateKey(kFullAppClientsKeyPath));
  ASSERT_TRUE(RegKey::HasKey(kFullAppClientsKeyPath));
  ASSERT_SUCCEEDED(RegKey::SetValue(kFullAppClientsKeyPath,
                                    kRegValueProductVersion,
                                    _T("0.10.69.5")));

  __mutexScope(AppManager::Instance()->GetRegistryStableStateLock());
  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_FAILED,
            iw_->InstallApp(NULL,
                            kAppGuid,
                            cmd_exe_path_,
                            arguments,
                            _T(""),  // Installer data.
                            kLanguageEnglish,
                            _T(""),  // Untrusted data.
                            0,
                            &result_info_));

  EXPECT_EQ(INSTALLER_RESULT_ERROR_OTHER, result_info_.type);
  EXPECT_EQ(1, result_info_.code);
  EXPECT_STREQ(_T("The installer encountered error 1."), result_info_.text);
  EXPECT_TRUE(result_info_.post_install_launch_command_line.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_url.IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, result_info_.post_install_action);
}

/* TODO(omaha): Figure out a way to perform this test.
   ClearInstallerResultApiValues clears the result values it sets.
   TODO(omaha): Add another test that reports an error in using registry API.
// Also tests that the launch cmd is set.
TEST_F(InstallerWrapperUserTest,
       InstallApp_ExeInstallerReturnsNonZeroExitCode_InstallerResultSuccess) {
  const TCHAR kCommandToExecute[] = _T("exit 1");

  CString arguments;
  arguments.Format(kExecuteCommandAndTerminateSwitch, kCommandToExecute);

  // Create the Clients key since this isn't an actual installer.
  ASSERT_SUCCEEDED(RegKey::CreateKey(kFullAppClientsKeyPath));
  ASSERT_TRUE(RegKey::HasKey(kFullAppClientsKeyPath));
  ASSERT_SUCCEEDED(RegKey::SetValue(kFullAppClientsKeyPath,
                                    kRegValueProductVersion,
                                    _T("0.10.69.5")));
  SetupInstallerResultRegistry(kAppId,
                               true, kResultSuccess,
                               false, 0,
                               false, _T(""),
                               true, kLaunchCmdLine);

  __mutexScope(AppManager::Instance()->GetRegistryStableStateLock());
  EXPECT_SUCCEEDED(iw_->InstallApp(kAppGuid,
                                   cmd_exe_path_,
                                   arguments,
                                   _T(""),  // Installer data.
                                   kLanguageEnglish,
                                   _T(""),  // Untrusted data.
                                   &result_info_));

  EXPECT_EQ(INSTALLER_RESULT_SUCCESS, result_info_.type);
  EXPECT_EQ(1, result_info_.code);
  EXPECT_TRUE(result_info_.text.IsEmpty());
  EXPECT_STREQ(kLaunchCmdLine, result_info_.post_install_launch_command_line);
  EXPECT_TRUE(result_info_.post_install_url.IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_LAUNCH_COMMAND,
            result_info_.post_install_action);

  EXPECT_SUCCEEDED(
      iw_->CheckApplicationRegistration(kAppGuid, _T("0.9.70.1"), false));

  VerifyLastRegistryValues(kAppId,
                           true, kResultSuccess,
                           false, 0,
                           false, _T(""),
                           true, kLaunchCmdLine);
}
*/

TEST_F(InstallerWrapperMachineTest, InstallApp_MsiInstallerSucceeds) {
  if (!vista_util::IsUserAdmin()) {
    std::wcout << _T("\tTest did not run because the user is not an admin.")
               << std::endl;
    return;
  }
  // We can't fake the registry keys because we are interacting with a real
  // installer.
  RestoreRegistryHives();

  AdjustMsiTries(iw_.get());

  CString installer_full_path(
      ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                      kSetupFooV1RelativeLocation));
  ASSERT_TRUE(File::Exists(installer_full_path));

  CString installer_log_full_path;
  installer_log_full_path.Format(kMsiLogFormat, installer_full_path);

  ASSERT_SUCCEEDED(File::Remove(installer_log_full_path));
  ASSERT_FALSE(File::Exists(installer_log_full_path));

  RegKey::DeleteKey(kFullFooAppClientKeyPath);
  ASSERT_FALSE(RegKey::HasKey(kFullFooAppClientKeyPath));
  RegKey::DeleteKey(kFullFooAppClientStateKeyPath);
  ASSERT_FALSE(RegKey::HasKey(kFullFooAppClientStateKeyPath));

  __mutexScope(AppManager::Instance()->GetRegistryStableStateLock());
  EXPECT_SUCCEEDED(iw_->InstallApp(NULL,
                                   StringToGuid(kFooGuid),
                                   installer_full_path,
                                   _T(""),  // Arguments.
                                   _T(""),  // Installer data.
                                   kLanguageEnglish,
                                   _T(""),  // Untrusted data.
                                   0,
                                   &result_info_));

  EXPECT_EQ(INSTALLER_RESULT_SUCCESS, result_info_.type);
  EXPECT_EQ(1234, result_info_.code);
  EXPECT_TRUE(result_info_.text.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_launch_command_line.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_url.IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, result_info_.post_install_action);

  EXPECT_TRUE(File::Exists(installer_log_full_path));

  EXPECT_TRUE(RegKey::HasKey(kFullFooAppClientKeyPath));
  EXPECT_TRUE(RegKey::HasKey(kFullFooAppClientStateKeyPath));
  // Verify the installer did not write a value that is to be written only in
  // the presence of an MSI property that was not specified.
  EXPECT_FALSE(RegKey::HasValue(kFullFooAppClientKeyPath,
                                kFooInstallerBarValueName));
  // EXPECT_SUCCEEDED(iw_->CheckApplicationRegistration(
  //                      StringToGuid(kFooGuid), _T("0.9.70.1"), false));

  UninstallTestMsi(installer_full_path);

  EXPECT_FALSE(RegKey::HasKey(kFullFooAppClientKeyPath));
  EXPECT_SUCCEEDED(RegKey::DeleteKey(kFullFooAppClientKeyPath));
}

TEST_F(InstallerWrapperMachineTest,
       InstallApp_MsiInstallerWithArgumentSucceeds) {
  if (!vista_util::IsUserAdmin()) {
    std::wcout << _T("\tTest did not run because the user is not an admin.")
               << std::endl;
    return;
  }

  // We can't fake the registry keys because we are interacting with a real
  // installer.
  RestoreRegistryHives();

  AdjustMsiTries(iw_.get());

  CString installer_full_path(
      ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                      kSetupFooV1RelativeLocation));
  ASSERT_TRUE(File::Exists(installer_full_path));

  CString installer_log_full_path;
  installer_log_full_path.Format(kMsiLogFormat, installer_full_path);

  ASSERT_SUCCEEDED(File::Remove(installer_log_full_path));
  ASSERT_FALSE(File::Exists(installer_log_full_path));

  RegKey::DeleteKey(kFullFooAppClientKeyPath);
  ASSERT_FALSE(RegKey::HasKey(kFullFooAppClientKeyPath));
  RegKey::DeleteKey(kFullFooAppClientStateKeyPath);
  ASSERT_FALSE(RegKey::HasKey(kFullFooAppClientStateKeyPath));

  __mutexScope(AppManager::Instance()->GetRegistryStableStateLock());
  EXPECT_SUCCEEDED(iw_->InstallApp(NULL,
                                   StringToGuid(kFooGuid),
                                   installer_full_path,
                                   kFooInstallerBarPropertyArg,
                                   _T(""),  // Installer data.
                                   kLanguageEnglish,
                                   _T(""),  // Untrusted data.
                                   0,
                                   &result_info_));

  EXPECT_EQ(INSTALLER_RESULT_SUCCESS, result_info_.type);
  EXPECT_EQ(1234, result_info_.code);
  EXPECT_TRUE(result_info_.text.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_launch_command_line.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_url.IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, result_info_.post_install_action);

  EXPECT_TRUE(File::Exists(installer_log_full_path));

  EXPECT_TRUE(RegKey::HasKey(kFullFooAppClientKeyPath));
  EXPECT_TRUE(RegKey::HasKey(kFullFooAppClientStateKeyPath));
  EXPECT_TRUE(RegKey::HasValue(kFullFooAppClientStateKeyPath,
                               kFooInstallerBarValueName));
  DWORD barprop_value = 0;
  EXPECT_SUCCEEDED(RegKey::GetValue(kFullFooAppClientStateKeyPath,
                                    kFooInstallerBarValueName,
                                    &barprop_value));
  EXPECT_EQ(7, barprop_value);

  // EXPECT_SUCCEEDED(iw_->CheckApplicationRegistration(
  //                      StringToGuid(kFooGuid), _T("0.9.70.1"), false));

  UninstallTestMsi(installer_full_path);

  EXPECT_FALSE(RegKey::HasKey(kFullFooAppClientKeyPath));
  EXPECT_SUCCEEDED(RegKey::DeleteKey(kFullFooAppClientKeyPath));
}

// The use of kGoogleUpdateAppId is the key to this test.
// Note that the version is not changed - this is the normal self-update case.
TEST_F(InstallerWrapperUserTest, InstallApp_UpdateOmahaSucceeds) {
  CString arguments;
  arguments.Format(kExecuteCommandAndTerminateSwitch, _T(""));

  const CString kExistingVersion(_T("0.9.69.5"));

  // Because we don't actually run the Omaha installer, we need to make sure
  // its Clients key and pv value exist to avoid an error.
  ASSERT_SUCCEEDED(RegKey::CreateKey(USER_REG_CLIENTS_GOOPDATE));
  ASSERT_TRUE(RegKey::HasKey(USER_REG_CLIENTS_GOOPDATE));
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                    kRegValueProductVersion,
                                    kExistingVersion));

  __mutexScope(AppManager::Instance()->GetRegistryStableStateLock());
  EXPECT_SUCCEEDED(iw_->InstallApp(NULL,
                                   kGoopdateGuid,
                                   cmd_exe_path_,
                                   arguments,
                                   _T(""),  // Installer data.
                                   kLanguageEnglish,
                                   _T(""),  // Untrusted data.
                                   0,
                                   &result_info_));

  EXPECT_EQ(INSTALLER_RESULT_SUCCESS, result_info_.type);
  EXPECT_EQ(S_OK, result_info_.code);
  EXPECT_TRUE(result_info_.text.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_launch_command_line.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_url.IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, result_info_.post_install_action);

  EXPECT_TRUE(RegKey::HasKey(USER_REG_CLIENTS_GOOPDATE));
  CString version;
  EXPECT_SUCCEEDED(RegKey::GetValue(USER_REG_CLIENTS_GOOPDATE,
                                    kRegValueProductVersion,
                                    &version));
  EXPECT_STREQ(kExistingVersion, version);

  // Do not call CheckApplicationRegistration for Omaha.
}

// The main purpose of this test is to ensure that self-updates don't fail if
// Omaha's Clients key doesn't exist for some reason.
TEST_F(InstallerWrapperUserTest,
       InstallApp_UpdateOmahaSucceedsWhenClientsKeyAbsent) {
  CString arguments;
  arguments.Format(kExecuteCommandAndTerminateSwitch, _T(""));

  const CString kExistingVersion(_T("0.9.69.5"));

  RegKey::DeleteKey(USER_REG_CLIENTS_GOOPDATE);

  __mutexScope(AppManager::Instance()->GetRegistryStableStateLock());
  EXPECT_SUCCEEDED(iw_->InstallApp(NULL,
                                   kGoopdateGuid,
                                   cmd_exe_path_,
                                   arguments,
                                   _T(""),  // Installer data.
                                   kLanguageEnglish,
                                   _T(""),  // Untrusted data.
                                   0,
                                   &result_info_));

  EXPECT_EQ(INSTALLER_RESULT_SUCCESS, result_info_.type);
  EXPECT_EQ(S_OK, result_info_.code);
  EXPECT_TRUE(result_info_.text.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_launch_command_line.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_url.IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, result_info_.post_install_action);

  EXPECT_FALSE(RegKey::HasKey(USER_REG_CLIENTS_GOOPDATE));

  // Do not call CheckApplicationRegistration for Omaha.
}

TEST_F(InstallerWrapperUserTest, InstallApp_InstallerDoesNotWriteClientsKey) {
  CString arguments;
  arguments.Format(kExecuteCommandAndTerminateSwitch, _T(""));

  __mutexScope(AppManager::Instance()->GetRegistryStableStateLock());
  EXPECT_SUCCEEDED(iw_->InstallApp(NULL,
                                   kAppGuid,
                                   cmd_exe_path_,
                                   arguments,
                                   _T(""),  // Installer data.
                                   kLanguageEnglish,
                                   _T(""),  // Untrusted data.
                                   0,
                                   &result_info_));

  EXPECT_FALSE(RegKey::HasKey(kFullAppClientsKeyPath));
  EXPECT_FALSE(RegKey::HasKey(kFullAppClientStateKeyPath));

  EXPECT_EQ(INSTALLER_RESULT_SUCCESS, result_info_.type);
  EXPECT_EQ(S_OK, result_info_.code);
  EXPECT_TRUE(result_info_.text.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_launch_command_line.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_url.IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, result_info_.post_install_action);

  // EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_DID_NOT_WRITE_CLIENTS_KEY,
  //           iw_->CheckApplicationRegistration(kAppGuid, _T(""), true));
}

TEST_F(InstallerWrapperUserTest,
       InstallApp_InstallerFailureMsiFileDoesNotExist) {
  CPath msi_path(app_util::GetTempDir());
  msi_path.Append(_T("foo.msi"));
  const CString log_path = msi_path + _T(".log");

  AdjustMsiTries(iw_.get());

  ASSERT_SUCCEEDED(File::Remove(log_path));
  ASSERT_FALSE(File::Exists(log_path));

  __mutexScope(AppManager::Instance()->GetRegistryStableStateLock());
  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_FAILED,
            iw_->InstallApp(NULL,
                            kAppGuid,
                            msi_path,
                            _T(""),  // Arguments.
                            _T(""),  // Installer data.
                            kLanguageEnglish,
                            _T(""),  // Untrusted data.
                            0,
                            &result_info_));

  EXPECT_EQ(INSTALLER_RESULT_ERROR_MSI, result_info_.type);
  EXPECT_EQ(ERROR_INSTALL_PACKAGE_OPEN_FAILED, result_info_.code);
  EXPECT_STREQ(kError1619MessagePrefix,
               result_info_.text.Left(kError1619MessagePrefixLength));
  VerifyStringIsMsiPackageOpenFailedString(
      result_info_.text.Mid(kError1619MessagePrefixLength));
  EXPECT_TRUE(result_info_.post_install_launch_command_line.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_url.IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, result_info_.post_install_action);

  // msiexec creates an empty log file.
  EXPECT_TRUE(File::Exists(log_path));
  EXPECT_SUCCEEDED(File::Remove(log_path));

  EXPECT_FALSE(RegKey::HasKey(kFullAppClientsKeyPath));
  EXPECT_FALSE(RegKey::HasKey(kFullAppClientStateKeyPath));
}

// Simulates the MSI busy error by having an exe installer return the error
// as its exit code and specifying MSI error using Installer Result API.
// Assumes reg.exe is in the path.
TEST_F(InstallerWrapperUserTest, InstallApp_MsiIsBusy_NoRetries) {
  CString commands;
  commands.Format(kExecuteTwoCommandsFormat,
                  set_installer_result_type_msi_error_cmd_,
                  kMsiInstallerBusyExitCodeCmd);

  CString arguments;
  arguments.Format(kExecuteCommandAndTerminateSwitch, commands);

  LowResTimer install_timer(true);

  __mutexScope(AppManager::Instance()->GetRegistryStableStateLock());
  EXPECT_EQ(GOOPDATEINSTALL_E_MSI_INSTALL_ALREADY_RUNNING,
            iw_->InstallApp(NULL,
                            kAppGuid,
                            cmd_exe_path_,
                            arguments,
                            _T(""),  // Installer data.
                            kLanguageEnglish,
                            _T(""),  // Untrusted data.
                            0,
                            &result_info_));

  EXPECT_GT(2, install_timer.GetSeconds());  // Check Omaha did not retry.

  EXPECT_EQ(INSTALLER_RESULT_ERROR_MSI, result_info_.type);
  EXPECT_EQ(ERROR_INSTALL_ALREADY_RUNNING, result_info_.code);
  EXPECT_STREQ(kError1618MessagePrefix,
               result_info_.text.Left(kError1618MessagePrefixLength));
  VerifyStringIsMsiBusyString(
      result_info_.text.Mid(kError1618MessagePrefixLength));
  EXPECT_TRUE(result_info_.post_install_launch_command_line.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_url.IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, result_info_.post_install_action);
}

TEST_F(InstallerWrapperUserTest, InstallApp_MsiIsBusy_TwoTries) {
  CString commands;
  commands.Format(kExecuteTwoCommandsFormat,
                  set_installer_result_type_msi_error_cmd_,
                  kMsiInstallerBusyExitCodeCmd);

  CString arguments;
  arguments.Format(kExecuteCommandAndTerminateSwitch, commands);

  iw_->set_num_tries_when_msi_busy(2);

  LowResTimer install_timer(true);

  __mutexScope(AppManager::Instance()->GetRegistryStableStateLock());
  EXPECT_EQ(GOOPDATEINSTALL_E_MSI_INSTALL_ALREADY_RUNNING,
            iw_->InstallApp(NULL,
                            kAppGuid,
                            cmd_exe_path_,
                            arguments,
                            _T(""),  // Installer data.
                            kLanguageEnglish,
                            _T(""),  // Untrusted data.
                            0,
                            &result_info_));

  EXPECT_LE(5, install_timer.GetSeconds());  // Check Omaha did retry.
  EXPECT_GT(10, install_timer.GetSeconds());

  EXPECT_EQ(INSTALLER_RESULT_ERROR_MSI, result_info_.type);
  EXPECT_EQ(ERROR_INSTALL_ALREADY_RUNNING, result_info_.code);
  EXPECT_STREQ(kError1618MessagePrefix,
               result_info_.text.Left(kError1618MessagePrefixLength));
  VerifyStringIsMsiBusyString(
      result_info_.text.Mid(kError1618MessagePrefixLength));
  EXPECT_TRUE(result_info_.post_install_launch_command_line.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_url.IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, result_info_.post_install_action);
}

// This test uses cmd.exe as an installer that leaves the payload files.
TEST_F(InstallerWrapperUserTest, InstallApp_InstallMultipleApps) {
  const TCHAR kPayloadFileName1[] = _T("exe_payload1.txt");
  const TCHAR kPayloadFileName2[] = _T("exe_payload2.txt");
  const TCHAR kCommandToExecute[] = _T("echo \"hi\" > %s");

  CString full_command_to_execute;
  full_command_to_execute.Format(kCommandToExecute, kPayloadFileName1);
  CString arguments1;
  arguments1.Format(kExecuteCommandAndTerminateSwitch, full_command_to_execute);

  full_command_to_execute.Format(kCommandToExecute, kPayloadFileName2);
  CString arguments2;
  arguments2.Format(kExecuteCommandAndTerminateSwitch, full_command_to_execute);

  EXPECT_SUCCEEDED(File::Remove(kPayloadFileName1));
  EXPECT_FALSE(File::Exists(kPayloadFileName1));
  EXPECT_SUCCEEDED(File::Remove(kPayloadFileName2));
  EXPECT_FALSE(File::Exists(kPayloadFileName2));

  // Create the Clients key since this isn't an actual installer.
  EXPECT_SUCCEEDED(RegKey::CreateKey(kFullAppClientsKeyPath));
  EXPECT_TRUE(RegKey::HasKey(kFullAppClientsKeyPath));
  EXPECT_SUCCEEDED(RegKey::SetValue(kFullAppClientsKeyPath,
                                    kRegValueProductVersion,
                                    _T("0.10.69.5")));

  __mutexScope(AppManager::Instance()->GetRegistryStableStateLock());

  EXPECT_SUCCEEDED(iw_->InstallApp(NULL,
                                   kAppGuid,
                                   cmd_exe_path_,
                                   arguments1,
                                   _T(""),  // Installer data.
                                   kLanguageEnglish,
                                   _T(""),  // Untrusted data.
                                   0,
                                   &result_info_));

  EXPECT_EQ(INSTALLER_RESULT_SUCCESS, result_info_.type);
  EXPECT_EQ(S_OK, result_info_.code);
  EXPECT_TRUE(result_info_.text.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_launch_command_line.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_url.IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, result_info_.post_install_action);

  // EXPECT_SUCCEEDED(
  //     iw_->CheckApplicationRegistration(kAppGuid, _T("0.9.70.1"), false));

  EXPECT_TRUE(File::Exists(kPayloadFileName1));
  EXPECT_SUCCEEDED(File::Remove(kPayloadFileName1));

  // Run the second installer.

  result_info_.type = INSTALLER_RESULT_UNKNOWN;
  result_info_.code = kInitialErrorValue;

  EXPECT_SUCCEEDED(iw_->InstallApp(NULL,
                                   kAppGuid,
                                   cmd_exe_path_,
                                   arguments2,
                                   _T(""),  // Installer data.
                                   kLanguageEnglish,
                                   _T(""),  // Untrusted data.
                                   0,
                                   &result_info_));

  EXPECT_EQ(INSTALLER_RESULT_SUCCESS, result_info_.type);
  EXPECT_EQ(S_OK, result_info_.code);
  EXPECT_TRUE(result_info_.text.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_launch_command_line.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_url.IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, result_info_.post_install_action);

  // EXPECT_SUCCEEDED(
  //     iw_->CheckApplicationRegistration(kAppGuid, _T("0.9.70.1"), false));

  EXPECT_TRUE(File::Exists(kPayloadFileName2));
  EXPECT_SUCCEEDED(File::Remove(kPayloadFileName2));
}

//
// GetInstallerResultHelper tests
//

TEST_F(InstallerWrapperUserGetInstallerResultHelperTest,
       GetInstallerResultHelper_NoRegistry_MSI_ZeroExitCode) {
  CallGetInstallerResultHelper(kAppGuid, kMsi, 0, &result_info_);

  EXPECT_EQ(INSTALLER_RESULT_SUCCESS, result_info_.type);
  EXPECT_EQ(0, result_info_.code);
  EXPECT_TRUE(result_info_.text.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_launch_command_line.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_url.IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, result_info_.post_install_action);

  VerifyNoLastRegistryValues(kAppId);
}

TEST_F(InstallerWrapperUserGetInstallerResultHelperTest,
       GetInstallerResultHelper_NoRegistry_MSI_NonZeroExitCode) {
  CallGetInstallerResultHelper(kAppGuid, kMsi, 1603, &result_info_);

  EXPECT_EQ(INSTALLER_RESULT_ERROR_MSI, result_info_.type);
  EXPECT_EQ(1603, result_info_.code);
  EXPECT_STREQ(kError1603Text, result_info_.text);
  EXPECT_TRUE(result_info_.post_install_launch_command_line.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_url.IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, result_info_.post_install_action);

  VerifyNoLastRegistryValues(kAppId);
}

TEST_F(InstallerWrapperUserGetInstallerResultHelperTest,
       GetInstallerResultHelper_NoRegistry_EXE_ZeroExitCode) {
  CallGetInstallerResultHelper(kAppGuid, kOther, 0, &result_info_);

  EXPECT_EQ(INSTALLER_RESULT_SUCCESS, result_info_.type);
  EXPECT_EQ(0, result_info_.code);
  EXPECT_TRUE(result_info_.text.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_launch_command_line.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_url.IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, result_info_.post_install_action);

  VerifyNoLastRegistryValues(kAppId);
}

TEST_F(InstallerWrapperUserGetInstallerResultHelperTest,
       GetInstallerResultHelper_NoRegistry_EXE_NonZeroExitCode_SmallNumber) {
  CallGetInstallerResultHelper(kAppGuid, kOther, 8, &result_info_);

  EXPECT_EQ(INSTALLER_RESULT_ERROR_OTHER, result_info_.type);
  EXPECT_EQ(8, result_info_.code);
  EXPECT_STREQ(_T("The installer encountered error 8."), result_info_.text);
  EXPECT_TRUE(result_info_.post_install_launch_command_line.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_url.IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, result_info_.post_install_action);

  VerifyNoLastRegistryValues(kAppId);
}

TEST_F(InstallerWrapperUserGetInstallerResultHelperTest,
       GetInstallerResultHelper_NoRegistry_EXE_NonZeroExitCode_HRESULTFailure) {
  CallGetInstallerResultHelper(kAppGuid, kOther, 0x80004005, &result_info_);

  EXPECT_EQ(INSTALLER_RESULT_ERROR_OTHER, result_info_.type);
  EXPECT_EQ(0x80004005, result_info_.code);
  EXPECT_STREQ(_T("The installer encountered error 0x80004005."),
               result_info_.text);
  EXPECT_TRUE(result_info_.post_install_launch_command_line.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_url.IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, result_info_.post_install_action);

  VerifyNoLastRegistryValues(kAppId);
}

TEST_F(InstallerWrapperUserGetInstallerResultHelperTest,
       GetInstallerResultHelper_ExitCode_MSI) {
  SetupInstallerResultRegistry(kAppId,
                               true, kResultExitCode,
                               false, 0,
                               false, 0,
                               false, _T(""),
                               false, _T(""));

  CallGetInstallerResultHelper(kAppGuid, kMsi, 1603, &result_info_);

  EXPECT_EQ(INSTALLER_RESULT_ERROR_MSI, result_info_.type);
  EXPECT_EQ(1603, result_info_.code);
  EXPECT_STREQ(kError1603Text, result_info_.text);
  EXPECT_TRUE(result_info_.post_install_launch_command_line.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_url.IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, result_info_.post_install_action);

  VerifyLastRegistryValues(kAppId,
                           true, kResultExitCode,
                           false, 0,
                           false, 0,
                           false, _T(""),
                           false, _T(""));
}

TEST_F(InstallerWrapperUserGetInstallerResultHelperTest,
       GetInstallerResultHelper_NoRegistry_MSI_RebootRequired) {
  CallGetInstallerResultHelper(kAppGuid, kMsi, ERROR_SUCCESS_REBOOT_REQUIRED,
                               &result_info_);

  EXPECT_EQ(INSTALLER_RESULT_SUCCESS, result_info_.type);
  EXPECT_EQ(0, result_info_.code);
  EXPECT_TRUE(result_info_.text.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_launch_command_line.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_url.IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_REBOOT, result_info_.post_install_action);

  VerifyNoLastRegistryValues(kAppId);
}

TEST_F(InstallerWrapperUserGetInstallerResultHelperTest,
       GetInstallerResultHelper_SystemError_EXE_RebootRequired) {
  SetupInstallerResultRegistry(kAppId,
                               true, kResultFailedSystemError,
                               true, ERROR_SUCCESS_REBOOT_REQUIRED,
                               false, 0,
                               false, _T(""),
                               true, kLaunchCmdLine);

  CallGetInstallerResultHelper(kAppGuid, kOther, 1, &result_info_);

  EXPECT_EQ(INSTALLER_RESULT_SUCCESS, result_info_.type);
  EXPECT_EQ(0, result_info_.code);
  EXPECT_TRUE(result_info_.text.IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_REBOOT, result_info_.post_install_action);

  VerifyLastRegistryValues(kAppId,
                           true, kResultFailedSystemError,
                           true, ERROR_SUCCESS_REBOOT_REQUIRED,
                           false, 0,
                           false, _T(""),
                           true, kLaunchCmdLine);
}

TEST_F(InstallerWrapperUserGetInstallerResultHelperTest,
       GetInstallerResultHelper_CustomError_ShouldNotReboot) {
  SetupInstallerResultRegistry(kAppId,
                               true, kResultFailedCustomError,
                               true, ERROR_SUCCESS_REBOOT_REQUIRED,
                               false, 0,
                               false, _T(""),
                               false, _T(""));

  CallGetInstallerResultHelper(kAppGuid, kMsi, 0, &result_info_);

  EXPECT_EQ(INSTALLER_RESULT_ERROR_OTHER, result_info_.type);
  EXPECT_EQ(ERROR_SUCCESS_REBOOT_REQUIRED, result_info_.code);
  EXPECT_STREQ(_T("The installer encountered error 3010."), result_info_.text);
  EXPECT_TRUE(result_info_.post_install_launch_command_line.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_url.IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, result_info_.post_install_action);
}

TEST_F(InstallerWrapperUserGetInstallerResultHelperTest,
       GetInstallerResultHelper_Success_NoErrorCode) {
  SetupInstallerResultRegistry(kAppId,
                               true, kResultSuccess,
                               false, 0,
                               false, 0,
                               false, _T(""),
                               false, _T(""));

  CallGetInstallerResultHelper(kAppGuid, kOther, 99, &result_info_);

  EXPECT_EQ(INSTALLER_RESULT_SUCCESS, result_info_.type);
  EXPECT_EQ(99, result_info_.code);
  EXPECT_TRUE(result_info_.text.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_launch_command_line.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_url.IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, result_info_.post_install_action);

  VerifyLastRegistryValues(kAppId,
                           true, kResultSuccess,
                           false, 0,
                           false, 0,
                           false, _T(""),
                           false, _T(""));
}

TEST_F(InstallerWrapperUserGetInstallerResultHelperTest,
       GetInstallerResultHelper_Success_AllValues) {
  SetupInstallerResultRegistry(kAppId,
                               true, kResultSuccess,
                               true, 555,
                               false, 0,
                               true, _T("an ignored error"),
                               true, kLaunchCmdLine);

  CallGetInstallerResultHelper(kAppGuid, kOther, 99, &result_info_);

  EXPECT_EQ(INSTALLER_RESULT_SUCCESS, result_info_.type);
  EXPECT_EQ(555, result_info_.code) <<
      _T("InstallerError overwrites exit code.");
  EXPECT_FALSE(result_info_.text.IsEmpty()) <<
      _T("UIString is ignored for Success.");
  EXPECT_STREQ(kLaunchCmdLine, result_info_.post_install_launch_command_line);
  EXPECT_TRUE(result_info_.post_install_url.IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_LAUNCH_COMMAND,
            result_info_.post_install_action);

  VerifyLastRegistryValues(kAppId,
                           true, kResultSuccess,
                           true, 555,
                           false, 0,
                           true, _T("an ignored error"),
                           true, kLaunchCmdLine);
}

TEST_F(InstallerWrapperUserGetInstallerResultHelperTest,
       GetInstallerResultHelper_LaunchCmdOnly_MSI_ZeroExitCode) {
  SetupInstallerResultRegistry(kAppId,
                               false, 0,
                               false, 0,
                               false, 0,
                               false, _T(""),
                               true, kLaunchCmdLine);

  CallGetInstallerResultHelper(kAppGuid, kMsi, 0, &result_info_);

  EXPECT_EQ(INSTALLER_RESULT_SUCCESS, result_info_.type);
  EXPECT_EQ(0, result_info_.code);
  EXPECT_TRUE(result_info_.text.IsEmpty());
  EXPECT_STREQ(kLaunchCmdLine, result_info_.post_install_launch_command_line);
  EXPECT_TRUE(result_info_.post_install_url.IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_LAUNCH_COMMAND,
            result_info_.post_install_action);

  VerifyLastRegistryValues(kAppId,
                           false, 0,
                           false, 0,
                           false, 0,
                           false, _T(""),
                           true, kLaunchCmdLine);
}

// Exit code is used when no error code is present. It's interpreted as a system
// error even though the installer is not an MSI.
TEST_F(InstallerWrapperUserGetInstallerResultHelperTest,
       GetInstallerResultHelper_Failed_NoErrorCodeOrUiString) {
  SetupInstallerResultRegistry(kAppId,
                               true, kResultFailedCustomError,
                               false, 0,
                               false, 0,
                               false, _T(""),
                               false, _T(""));

  CallGetInstallerResultHelper(kAppGuid, kOther, 8, &result_info_);

  EXPECT_EQ(INSTALLER_RESULT_ERROR_OTHER, result_info_.type);
  EXPECT_EQ(8, result_info_.code);
  EXPECT_STREQ(_T("The installer encountered error 8."), result_info_.text);
  EXPECT_TRUE(result_info_.post_install_launch_command_line.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_url.IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, result_info_.post_install_action);

  VerifyLastRegistryValues(kAppId,
                           true, kResultFailedCustomError,
                           false, 0,
                           false, 0,
                           false, _T(""),
                           false, _T(""));
}

TEST_F(InstallerWrapperUserGetInstallerResultHelperTest,
       GetInstallerResultHelper_Failed_WithErrorCode) {
  SetupInstallerResultRegistry(kAppId,
                               true, kResultFailedCustomError,
                               true, 8,
                               false, 0,
                               false, _T(""),
                               false, _T(""));

  CallGetInstallerResultHelper(kAppGuid, kOther, 1618, &result_info_);

  EXPECT_EQ(INSTALLER_RESULT_ERROR_OTHER, result_info_.type);
  EXPECT_EQ(8, result_info_.code);
  EXPECT_STREQ(_T("The installer encountered error 8."), result_info_.text);
  EXPECT_TRUE(result_info_.post_install_launch_command_line.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_url.IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, result_info_.post_install_action);

  VerifyLastRegistryValues(kAppId,
                           true, kResultFailedCustomError,
                           true, 8,
                           false, 0,
                           false, _T(""),
                           false, _T(""));
}

// This test shows that command line is read and
// POST_INSTALL_ACTION_LAUNCH_COMMAND is set.
TEST_F(InstallerWrapperUserGetInstallerResultHelperTest,
       GetInstallerResultHelper_Failed_AllValues) {
  const DWORD kInstallerErrorValue = 8;
  const TCHAR* const kUiString = _T("a message from the installer");

  SetupInstallerResultRegistry(kAppId,
                               true, kResultFailedCustomError,
                               true, kInstallerErrorValue,
                               false, 0,
                               true, kUiString,
                               true, kLaunchCmdLine);

  CallGetInstallerResultHelper(kAppGuid, kOther, 1618, &result_info_);

  EXPECT_EQ(INSTALLER_RESULT_ERROR_OTHER, result_info_.type);
  EXPECT_EQ(kInstallerErrorValue, result_info_.code);
  EXPECT_STREQ(kUiString, result_info_.text);
  EXPECT_STREQ(kLaunchCmdLine, result_info_.post_install_launch_command_line);
  EXPECT_TRUE(result_info_.post_install_url.IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_LAUNCH_COMMAND,
            result_info_.post_install_action);

  VerifyLastRegistryValues(kAppId,
                           true, kResultFailedCustomError,
                           true, kInstallerErrorValue,
                           false, 0,
                           true, kUiString,
                           true, kLaunchCmdLine);
}

// Exit code is used and interpreted as MSI error when no error code present.
TEST_F(InstallerWrapperUserGetInstallerResultHelperTest,
       GetInstallerResultHelper_FailedMsiError_NoErrorCode) {
  SetupInstallerResultRegistry(kAppId,
                               true, kResultFailedMsiError,
                               false, 0,
                               false, 0,
                               false, _T(""),
                               false, _T(""));

  CallGetInstallerResultHelper(kAppGuid, kOther, 1603, &result_info_);

  EXPECT_EQ(INSTALLER_RESULT_ERROR_MSI, result_info_.type);
  EXPECT_EQ(1603, result_info_.code);
  EXPECT_STREQ(kError1603Text, result_info_.text);
  EXPECT_TRUE(result_info_.post_install_launch_command_line.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_url.IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, result_info_.post_install_action);

  VerifyLastRegistryValues(kAppId,
                           true, kResultFailedMsiError,
                           false, 0,
                           false, 0,
                           false, _T(""),
                           false, _T(""));
}

TEST_F(InstallerWrapperUserGetInstallerResultHelperTest,
       GetInstallerResultHelper_FailedMsiError_WithErrorCode) {
  const DWORD kInstallerErrorValue = 1603;

  SetupInstallerResultRegistry(kAppId,
                               true, kResultFailedMsiError,
                               true, kInstallerErrorValue,
                               false, 0,
                               false, _T(""),
                               false, _T(""));

  CallGetInstallerResultHelper(kAppGuid, kOther, 1618, &result_info_);

  EXPECT_EQ(INSTALLER_RESULT_ERROR_MSI, result_info_.type);
  EXPECT_EQ(kInstallerErrorValue, result_info_.code);
  EXPECT_STREQ(kError1603Text, result_info_.text);
  EXPECT_TRUE(result_info_.post_install_launch_command_line.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_url.IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, result_info_.post_install_action);

  VerifyLastRegistryValues(kAppId,
                           true, kResultFailedMsiError,
                           true, kInstallerErrorValue,
                           false, 0,
                           false, _T(""),
                           false, _T(""));
}

// This test shows that command line is read and
// POST_INSTALL_ACTION_LAUNCH_COMMAND is set.
TEST_F(InstallerWrapperUserGetInstallerResultHelperTest,
       GetInstallerResultHelper_FailedMsiError_AllValues) {
  const DWORD kInstallerErrorValue = 1603;

  SetupInstallerResultRegistry(kAppId,
                               true, kResultFailedMsiError,
                               true, kInstallerErrorValue,
                               false, 0,
                               true, _T("an ignored error"),
                               true, kLaunchCmdLine);

  CallGetInstallerResultHelper(kAppGuid, kOther, 1618, &result_info_);

  EXPECT_EQ(INSTALLER_RESULT_ERROR_MSI, result_info_.type);
  EXPECT_EQ(kInstallerErrorValue, result_info_.code);
  EXPECT_STREQ(kError1603Text, result_info_.text) << _T("UIString is ignored.");
  EXPECT_STREQ(kLaunchCmdLine, result_info_.post_install_launch_command_line);
  EXPECT_TRUE(result_info_.post_install_url.IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_LAUNCH_COMMAND,
            result_info_.post_install_action);

  VerifyLastRegistryValues(kAppId,
                           true, kResultFailedMsiError,
                           true, kInstallerErrorValue,
                           false, 0,
                           true, _T("an ignored error"),
                           true, kLaunchCmdLine);
}

// Exit code is used and interpreted as system error when no error code present.
TEST_F(InstallerWrapperUserGetInstallerResultHelperTest,
       GetInstallerResultHelper_FailedSystemError_NoErrorCode) {
  SetupInstallerResultRegistry(kAppId,
                               true, kResultFailedSystemError,
                               false, 0,
                               false, 0,
                               false, _T(""),
                               false, _T(""));

  CallGetInstallerResultHelper(kAppGuid, kOther, 1603, &result_info_);

  EXPECT_EQ(INSTALLER_RESULT_ERROR_SYSTEM, result_info_.type);
  EXPECT_EQ(1603, result_info_.code);
  EXPECT_STREQ(kError1603Text, result_info_.text);
  EXPECT_TRUE(result_info_.post_install_launch_command_line.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_url.IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, result_info_.post_install_action);

  VerifyLastRegistryValues(kAppId,
                           true, kResultFailedSystemError,
                           false, 0,
                           false, 0,
                           false, _T(""),
                           false, _T(""));
}

TEST_F(InstallerWrapperUserGetInstallerResultHelperTest,
       GetInstallerResultHelper_FailedSystemError_WithErrorCode) {
  const DWORD kInstallerErrorValue = 1603;

  SetupInstallerResultRegistry(kAppId,
                               true, kResultFailedSystemError,
                               true, kInstallerErrorValue,
                               false, 0,
                               false, _T(""),
                               false, _T(""));

  CallGetInstallerResultHelper(kAppGuid, kOther, 1618, &result_info_);

  EXPECT_EQ(INSTALLER_RESULT_ERROR_SYSTEM, result_info_.type);
  EXPECT_EQ(kInstallerErrorValue, result_info_.code);
  EXPECT_STREQ(kError1603Text, result_info_.text);
  EXPECT_TRUE(result_info_.post_install_launch_command_line.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_url.IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, result_info_.post_install_action);

  VerifyLastRegistryValues(kAppId,
                           true, kResultFailedSystemError,
                           true, kInstallerErrorValue,
                           false, 0,
                           false, _T(""),
                           false, _T(""));
}

// INSTALLER_RESULT_FAILED_SYSTEM_ERROR supports values beyond the basic
// "System Error Codes" and their HRESULT equivalents.
TEST_F(InstallerWrapperUserGetInstallerResultHelperTest,
       GetInstallerResultHelper_FailedSystemError_WithHRESULTSystemError) {
  const DWORD kInstallerErrorValue = 0x800B010F;

  SetupInstallerResultRegistry(kAppId,
                               true, kResultFailedSystemError,
                               true, kInstallerErrorValue,
                               false, 0,
                               false, _T(""),
                               false, _T(""));

  CallGetInstallerResultHelper(kAppGuid, kOther, 1618, &result_info_);

  EXPECT_EQ(INSTALLER_RESULT_ERROR_SYSTEM, result_info_.type);
  EXPECT_EQ(kInstallerErrorValue, result_info_.code);
  EXPECT_STREQ(kError0x800B010FText, result_info_.text);
  EXPECT_TRUE(result_info_.post_install_launch_command_line.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_url.IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, result_info_.post_install_action);

  VerifyLastRegistryValues(kAppId,
                           true, kResultFailedSystemError,
                           true, kInstallerErrorValue,
                           false, 0,
                           false, _T(""),
                           false, _T(""));
}

TEST_F(InstallerWrapperUserGetInstallerResultHelperTest,
       GetInstallerResultHelper_FailedSystemError_WithUnrecognizedError) {
  const DWORD kInstallerErrorValue = 0x80040200;

  SetupInstallerResultRegistry(kAppId,
                               true, kResultFailedSystemError,
                               true, kInstallerErrorValue,
                               false, 0,
                               false, _T(""),
                               false, _T(""));

  CallGetInstallerResultHelper(kAppGuid, kOther, 1618, &result_info_);

  EXPECT_EQ(INSTALLER_RESULT_ERROR_SYSTEM, result_info_.type);
  EXPECT_EQ(kInstallerErrorValue, result_info_.code);
  EXPECT_STREQ(_T("The installer encountered error 0x80040200."),
               result_info_.text);
  EXPECT_TRUE(result_info_.post_install_launch_command_line.IsEmpty());
  EXPECT_TRUE(result_info_.post_install_url.IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, result_info_.post_install_action);

  VerifyLastRegistryValues(kAppId,
                           true, kResultFailedSystemError,
                           true, kInstallerErrorValue,
                           false, 0,
                           false, _T(""),
                           false, _T(""));
}

// This test shows that command line is read and
// POST_INSTALL_ACTION_LAUNCH_COMMAND is set.
TEST_F(InstallerWrapperUserGetInstallerResultHelperTest,
       GetInstallerResultHelper_FailedSystemError_AllValues) {
  const DWORD kInstallerErrorValue = 1603;

  SetupInstallerResultRegistry(kAppId,
                               true, kResultFailedSystemError,
                               true, kInstallerErrorValue,
                               false, 0,
                               true, _T("an ignored error"),
                               true, kLaunchCmdLine);

  CallGetInstallerResultHelper(kAppGuid, kOther, 1618, &result_info_);

  EXPECT_EQ(INSTALLER_RESULT_ERROR_SYSTEM, result_info_.type);
  EXPECT_EQ(kInstallerErrorValue, result_info_.code);
  EXPECT_STREQ(kError1603Text, result_info_.text) << _T("UIString is ignored.");
  EXPECT_STREQ(kLaunchCmdLine, result_info_.post_install_launch_command_line);
  EXPECT_TRUE(result_info_.post_install_url.IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_LAUNCH_COMMAND,
            result_info_.post_install_action);

  VerifyLastRegistryValues(kAppId,
                           true, kResultFailedSystemError,
                           true, kInstallerErrorValue,
                           false, 0,
                           true, _T("an ignored error"),
                           true, kLaunchCmdLine);
}

TEST_F(InstallerWrapperUserGetInstallerResultHelperTest,
       GetInstallerResultHelper_ExtraCode1) {
  const DWORD kInstallerErrorValue = 10;
  const DWORD kInstallerExtraCode1 = 0xabcd;

  SetupInstallerResultRegistry(kAppId,
                               true, kResultFailedCustomError,
                               true, kInstallerErrorValue,
                               true, kInstallerExtraCode1,
                               false, _T(""),
                               false, _T(""));

  CallGetInstallerResultHelper(kAppGuid,
                               kOther,
                               kInstallerErrorValue,
                               &result_info_);

  EXPECT_EQ(INSTALLER_RESULT_ERROR_OTHER, result_info_.type);
  EXPECT_EQ(kInstallerErrorValue, result_info_.code);
  EXPECT_EQ(kInstallerExtraCode1, result_info_.extra_code1);

  VerifyLastRegistryValues(kAppId,
                           true, kResultFailedCustomError,
                           true, kInstallerErrorValue,
                           true, kInstallerExtraCode1,
                           false, _T(""),
                           false, _T(""));
}

// TODO(omaha): Add a machine test.

}  // namespace omaha
