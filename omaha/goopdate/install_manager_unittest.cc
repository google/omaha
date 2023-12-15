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

#include "omaha/goopdate/install_manager.h"

#include <atlpath.h>
#include <atlstr.h>

#include "omaha/base/app_util.h"
#include "omaha/base/error.h"
#include "omaha/base/file.h"
#include "omaha/base/path.h"
#include "omaha/base/reg_key.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/shell.h"
#include "omaha/base/synchronized.h"
#include "omaha/base/system.h"
#include "omaha/base/timer.h"
#include "omaha/base/utils.h"
#include "omaha/base/vistautil.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/common/install_manifest.h"
#include "omaha/common/ping_event.h"
#include "omaha/goopdate/app_bundle_state_initialized.h"
#include "omaha/goopdate/app_state_waiting_to_install.h"
#include "omaha/goopdate/app_unittest_base.h"
#include "omaha/goopdate/installer_wrapper.h"
#include "omaha/testing/unit_test.h"

using ::testing::_;
using ::testing::Return;

namespace omaha {

// TODO(omaha): there is a problem with this unit test. The model is built
// bottom up. This makes it impossible to set the references to parents. Will
// have to fix the code, eventually using Builder DP to create a bunch of
// models containing bundles, apps, and such.

namespace {

const TCHAR kAppId[] = _T("{B18BC01B-E0BD-4BF0-A33E-1133055E5FDE}");
const TCHAR kApp2Id[] = _T("{85794B39-42E5-457c-B567-4A0F2A0FB272}");

const TCHAR kFullAppClientsKeyPath[] =
    _T("HKCU\\Software\\") PATH_COMPANY_NAME _T("\\") PRODUCT_NAME
    _T("\\Clients\\{B18BC01B-E0BD-4BF0-A33E-1133055E5FDE}");
const TCHAR kFullAppClientStateKeyPath[] =
    _T("HKCU\\Software\\") PATH_COMPANY_NAME _T("\\") PRODUCT_NAME
    _T("\\ClientState\\{B18BC01B-E0BD-4BF0-A33E-1133055E5FDE}");
const TCHAR kFullFooAppClientKeyPath[] =
    _T("HKLM\\Software\\") PATH_COMPANY_NAME _T("\\") PRODUCT_NAME
    _T("\\Clients\\{D6B08267-B440-4C85-9F79-E195E80D9937}");
const TCHAR kFullFooAppClientStateKeyPath[] =
    _T("HKLM\\Software\\") PATH_COMPANY_NAME _T("\\") PRODUCT_NAME
    _T("\\ClientState\\{D6B08267-B440-4C85-9F79-E195E80D9937}");

const TCHAR kFullApp2ClientsKeyPath[] =
    _T("HKCU\\Software\\") PATH_COMPANY_NAME _T("\\") PRODUCT_NAME
    _T("\\Clients\\{85794B39-42E5-457c-B567-4A0F2A0FB272}");

const TCHAR kSetupFooV1RelativeLocation[] =
  _T("unittest_support\\test_foo_v1.0.101.0.msi");
const TCHAR kFooId[] = _T("{D6B08267-B440-4C85-9F79-E195E80D9937}");
const TCHAR kFooVersion[] = _T("1.0.101.0");
const TCHAR kFooInstallerBarPropertyArg[] = _T("PROPBAR=7");
const TCHAR kFooInstallerBarValueName[] = _T("propbar");

// Values related to using cmd.exe as an "installer".
const TCHAR kCmdExecutable[] = _T("cmd.exe");
const TCHAR kExecuteCommandAndTerminateSwitch[] = _T("/c %s");
const TCHAR kExecuteTwoCommandsFormat[] = _T("\"%s & %s\"");

const TCHAR kMsiInstallerBusyErrorMessage[] =
    _T("Installation failed. Please wait for other Windows installers to ")
    _T("finish and try installing again.");

const TCHAR kMsiLogFormat[] = _T("%s.log");

// brand, InstallTime, DayOfInstall, DayOfLastActivity, DayOfLastRollCall, and
// LastCheckSuccess are automatically populated.
const int kNumAutoPopulatedValues = 7;

}  // namespace

// Values and functions in installer_wrapper_unittest.cc.
extern const TCHAR kRegExecutable[];
extern const TCHAR kSetInstallerResultTypeMsiErrorRegCmdArgs[];
extern const TCHAR kMsiInstallerBusyExitCodeCmd[];
extern const TCHAR kError1619MessagePrefix[];
extern const int kError1619MessagePrefixLength;
void VerifyStringIsMsiPackageOpenFailedString(const CString& str);
void UninstallTestMsi(const CString& installer_path);
void AdjustMsiTries(InstallerWrapper* installer_wrapper);

// TODO(omaha3): Test the rest of InstallManager.
class InstallManagerTest : public testing::TestWithParam<bool> {
 protected:
  virtual void SetUp() {}
  virtual void TearDown() {}

  bool IsMachine() const {
    return GetParam();
  }

  CString GetInstallWorkingDir() const {
    return IsMachine() ?
        ConfigManager::Instance()->GetMachineInstallWorkingDir() :
        ConfigManager::Instance()->GetUserInstallWorkingDir();
  }
};

class InstallManagerInstallAppTest : public AppTestBase {
 protected:
  explicit InstallManagerInstallAppTest(bool is_machine)
      : AppTestBase(is_machine, false) {}

  static void SetUpTestCase() {
    CString system_path;
    EXPECT_SUCCEEDED(Shell::GetSpecialFolder(CSIDL_SYSTEM,
                                             false,
                                             &system_path));
    EXPECT_FALSE(system_path.IsEmpty());
    cmd_exe_dir_ += system_path;

    CPath cmd_exe_path;
    cmd_exe_path.Combine(system_path, kCmdExecutable);
    EXPECT_TRUE(File::Exists(cmd_exe_path));

    CPath reg_path;
    reg_path.Combine(system_path, kRegExecutable);
    set_installer_result_type_msi_error_cmd_.Format(
        _T("%s %s"),
        reg_path, kSetInstallerResultTypeMsiErrorRegCmdArgs);

    unittest_support_dir_ = ConcatenatePath(
        app_util::GetCurrentModuleDirectory(),
        _T("unittest_support"));
  }

  static void TearDownTestCase() {
  }

  virtual void SetUp() {
    AppTestBase::SetUp();

    RegKey::DeleteKey(kFullAppClientsKeyPath);
    RegKey::DeleteKey(kFullAppClientStateKeyPath);
    RegKey::DeleteKey(kFullFooAppClientKeyPath);
    RegKey::DeleteKey(kFullFooAppClientStateKeyPath);
    RegKey::DeleteKey(kFullApp2ClientsKeyPath);

    installer_wrapper_.reset(new InstallerWrapper(is_machine_));
    EXPECT_SUCCEEDED(installer_wrapper_->Initialize());

    ASSERT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kAppId), &app_));

    SetAppStateWaitingToInstall(app_);
  }

  HRESULT InstallApp(const CString& existing_version,
                     App* app,
                     const CString& dir) {
    ASSERT1(app);
    return InstallManager::InstallApp(is_machine_,
                                      NULL,
                                      existing_version,
                                      app->model()->lock(),
                                      installer_wrapper_.get(),
                                      app,
                                      dir);
  }

  void SetArgumentsInManifest(const CString& arguments,
                              const CString& expected_version,
                              App* app) {
    ASSERT1(app);

    // TODO(omaha3): Consider using a mock object.
    xml::InstallManifest* install_manifest = new xml::InstallManifest;
    install_manifest->version = expected_version;

    xml::InstallAction install_event_action;
    install_event_action.install_event = xml::InstallAction::kInstall;
    install_event_action.needs_admin = is_machine_ ? NEEDS_ADMIN_YES :
                                                     NEEDS_ADMIN_NO;
    // TODO(omaha3): Set install_event_action.program_to_run?
    install_event_action.program_arguments = arguments;

    install_manifest->install_actions.push_back(install_event_action);
    app->next_version()->set_install_manifest(install_manifest);
  }

  void SetPostInstallActionInManifest(SuccessfulInstallAction success_action,
                                      const CString& success_url,
                                      bool terminate_all_browsers,
                                      App* app) {
    xml::InstallManifest* install_manifest = new xml::InstallManifest;
    install_manifest->version = _T("1.2.3.4");

    xml::InstallAction post_install_event_action;
    post_install_event_action.install_event = xml::InstallAction::kPostInstall;
    post_install_event_action.needs_admin = is_machine_ ? NEEDS_ADMIN_YES :
                                                          NEEDS_ADMIN_NO;
    post_install_event_action.success_url = success_url;
    post_install_event_action.terminate_all_browsers = terminate_all_browsers;
    post_install_event_action.success_action = success_action;
    install_manifest->install_actions.push_back(post_install_event_action);
    app->next_version()->set_install_manifest(install_manifest);
  }

  static void SetAppStateWaitingToInstall(App* app) {
    SetAppStateForUnitTest(app, new fsm::AppStateWaitingToInstall);
  }

  static HRESULT GetResultCode(const App* app) {
    return app->error_context_.error_code;
  }

  static CString GetCompletionMessage(const App* app) {
    return app->completion_message_;
  }

  static PingEvent::Results GetCompletionResult(const App* app) {
    return app->completion_result_;
  }

  static CString GetPostInstallLaunchCommandLine(const App* app) {
    return app->post_install_launch_command_line_;
  }

  static CString GetPostInstallUrl(const App* app) {
    return app->post_install_url_;
  }

  static PostInstallAction GetPostInstallAction(const App* app) {
    return app->post_install_action_;
  }

  static void PopulateSuccessfulInstallResultInfo(
      const App* app, InstallerResultInfo* result_info) {
    return InstallManager::PopulateSuccessfulInstallResultInfo(
        app, result_info);
  }

  std::unique_ptr<InstallerWrapper> installer_wrapper_;

  App* app_;

  static CPath cmd_exe_dir_;
  static CString set_installer_result_type_msi_error_cmd_;
  static CString unittest_support_dir_;
};

CPath InstallManagerInstallAppTest::cmd_exe_dir_;
CString InstallManagerInstallAppTest::set_installer_result_type_msi_error_cmd_;
CString InstallManagerInstallAppTest::unittest_support_dir_;

class InstallManagerInstallAppMachineTest
    : public InstallManagerInstallAppTest {
 protected:
  InstallManagerInstallAppMachineTest()
    : InstallManagerInstallAppTest(true) {
  }
};

class InstallManagerInstallAppUserTest : public InstallManagerInstallAppTest {
 protected:
  InstallManagerInstallAppUserTest()
    : InstallManagerInstallAppTest(false) {
  }
};

//
// Helper method tests
//

// TODO(omaha3): We may replace these with the tests from
// installer_wrapper_unittest.cc if CheckApplicationRegistration() is moved to
// InstallManager.
#if 0
TEST_F(InstallManagerInstallAppUserTest,
       CheckApplicationRegistration_FailsWhenClientsKeyAbsent) {
  AppData app_data(kAppGuid, is_machine_);
  Job job(false, &ping_);
  job.set_app_data(app_data);

  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_DID_NOT_WRITE_CLIENTS_KEY,
            CheckApplicationRegistration(CString(), &job));

  EXPECT_FALSE(RegKey::HasKey(kFullAppClientsKeyPath));
  EXPECT_FALSE(RegKey::HasKey(kFullAppClientStateKeyPath));
}

TEST_F(InstallManagerInstallAppUserTest,
       CheckApplicationRegistration_FailsWhenVersionValueAbsent) {
  ASSERT_SUCCEEDED(RegKey::CreateKey(kFullAppClientsKeyPath));

  AppData app_data(kAppGuid, is_machine_);
  Job job(false, &ping_);
  job.set_app_data(app_data);

  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_DID_NOT_WRITE_CLIENTS_KEY,
            CheckApplicationRegistration(CString(), &job));

  EXPECT_TRUE(RegKey::HasKey(kFullAppClientsKeyPath));
  EXPECT_FALSE(RegKey::HasKey(kFullAppClientStateKeyPath));
}

TEST_F(InstallManagerInstallAppUserTest,
       CheckApplicationRegistration_SucceedsWhenStateKeyAbsent) {
  ASSERT_SUCCEEDED(RegKey::SetValue(kFullAppClientsKeyPath,
                                    kRegValueProductVersion,
                                    _T("0.9.68.4")));

  AppData app_data(kAppGuid, is_machine_);
  Job job(false, &ping_);
  job.set_app_data(app_data);

  EXPECT_SUCCEEDED(CheckApplicationRegistration(CString(), &job));
}

TEST_F(InstallManagerInstallAppUserTest,
       CheckApplicationRegistration_SucceedsWhenStateKeyPresent) {
  const TCHAR* keys_to_create[] = {kFullAppClientsKeyPath,
                                   kFullAppClientStateKeyPath};
  ASSERT_SUCCEEDED(RegKey::CreateKeys(keys_to_create, 2));
  ASSERT_TRUE(RegKey::HasKey(kFullAppClientsKeyPath));
  ASSERT_TRUE(RegKey::HasKey(kFullAppClientStateKeyPath));
  ASSERT_SUCCEEDED(RegKey::SetValue(kFullAppClientsKeyPath,
                                    kRegValueProductVersion,
                                    _T("0.9.70.0")));

  AppData app_data(kAppGuid, is_machine_);
  Job job(false, &ping_);
  job.set_app_data(app_data);

  // The install should succeed even if the version is the same.
  EXPECT_SUCCEEDED(CheckApplicationRegistration(_T("0.9.70.0"), &job));
}

TEST_F(InstallManagerInstallAppUserTest,
       CheckApplicationRegistration_UpdateSucceeds) {
  const TCHAR* keys_to_create[] = {kFullAppClientsKeyPath,
                                   kFullAppClientStateKeyPath};
  ASSERT_SUCCEEDED(RegKey::CreateKeys(keys_to_create, 2));
  ASSERT_TRUE(RegKey::HasKey(kFullAppClientsKeyPath));
  ASSERT_TRUE(RegKey::HasKey(kFullAppClientStateKeyPath));
  ASSERT_SUCCEEDED(RegKey::SetValue(kFullAppClientsKeyPath,
                                    kRegValueProductVersion,
                                    _T("0.9.70.0")));

  AppData app_data(kAppGuid, is_machine_);
  Job job(true, &ping_);
  job.set_app_data(app_data);

  EXPECT_SUCCEEDED(CheckApplicationRegistration(_T("0.9.70.1"), &job));
}

TEST_F(InstallManagerInstallAppUserTest,
       CheckApplicationRegistration_UpdateFailsWhenVersionDoesNotChange) {
  const TCHAR* keys_to_create[] = {kFullAppClientsKeyPath,
                                   kFullAppClientStateKeyPath};
  ASSERT_SUCCEEDED(RegKey::CreateKeys(keys_to_create,
                                      arraysize(keys_to_create)));
  ASSERT_TRUE(RegKey::HasKey(kFullAppClientsKeyPath));
  ASSERT_TRUE(RegKey::HasKey(kFullAppClientStateKeyPath));
  ASSERT_SUCCEEDED(RegKey::SetValue(kFullAppClientsKeyPath,
                                    kRegValueProductVersion,
                                    _T("0.9.70.0")));

  AppData app_data(kAppGuid, is_machine_);
  Job job(true, &ping_);
  job.set_app_data(app_data);

  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_DID_NOT_CHANGE_VERSION,
            CheckApplicationRegistration(_T("0.9.70.0"), &job));
}
#endif

//
// Negative Tests
//

TEST_F(InstallManagerInstallAppUserTest,
       InstallApp_InstallerWithoutFilenameExtension) {
  app_->next_version()->AddPackage(_T("foo"), 100, _T("sha256hash"));

  // TODO(omaha): We should be able to eliminate this.
  SetArgumentsInManifest(CString(), _T("1.2.3.4"), app_);

  EXPECT_EQ(GOOPDATEINSTALL_E_FILENAME_INVALID,
            InstallApp(_T(""), app_, _T("c:\\temp")));

  EXPECT_EQ(STATE_ERROR, app_->state());
  EXPECT_EQ(PingEvent::EVENT_RESULT_ERROR, GetCompletionResult(app_));
  EXPECT_EQ(GOOPDATEINSTALL_E_FILENAME_INVALID, GetResultCode(app_));
  EXPECT_STREQ(
      _T("The installer filename c:\\temp\\foo is invalid or unsupported."),
      GetCompletionMessage(app_));
  EXPECT_TRUE(GetPostInstallLaunchCommandLine(app_).IsEmpty());
  EXPECT_TRUE(GetPostInstallUrl(app_).IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, GetPostInstallAction(app_));
}

TEST_F(InstallManagerInstallAppUserTest,
       InstallApp_UnsupportedInstallerFilenameExtension) {
  app_->next_version()->AddPackage(_T("foo.bar"), 100, _T("sha256hash"));

  // TODO(omaha): We should be able to eliminate this.
  SetArgumentsInManifest(CString(), _T("1.2.3.4"), app_);

  EXPECT_EQ(GOOPDATEINSTALL_E_FILENAME_INVALID,
            InstallApp(_T(""), app_, _T("c:\\temp")));

  EXPECT_EQ(STATE_ERROR, app_->state());
  EXPECT_EQ(PingEvent::EVENT_RESULT_ERROR, GetCompletionResult(app_));
  EXPECT_EQ(GOOPDATEINSTALL_E_FILENAME_INVALID, GetResultCode(app_));
  EXPECT_STREQ(
      _T("The installer filename c:\\temp\\foo.bar is invalid or unsupported."),
      GetCompletionMessage(app_));
  EXPECT_TRUE(GetPostInstallLaunchCommandLine(app_).IsEmpty());
  EXPECT_TRUE(GetPostInstallUrl(app_).IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, GetPostInstallAction(app_));
}

TEST_F(InstallManagerInstallAppUserTest, InstallApp_InstallerEmptyFilename) {
  // Package asserts that the filename and file path are not NULL.
  ExpectAsserts expect_asserts;

  app_->next_version()->AddPackage(_T(""), 100, _T("sha256hash"));
  // This test does not call
  // app_->next_version()->GetPackage(0)->set_local_file_path().

  // TODO(omaha): We should be able to eliminate this.
  SetArgumentsInManifest(CString(), _T("1.2.3.4"), app_);

  EXPECT_EQ(GOOPDATEINSTALL_E_FILENAME_INVALID,
            InstallApp(_T(""), app_, NULL));

  EXPECT_EQ(STATE_ERROR, app_->state());
  EXPECT_EQ(PingEvent::EVENT_RESULT_ERROR, GetCompletionResult(app_));
  EXPECT_EQ(GOOPDATEINSTALL_E_FILENAME_INVALID, GetResultCode(app_));
  EXPECT_STREQ(_T("The installer filename \\ is invalid or unsupported."),
               GetCompletionMessage(app_));
  EXPECT_TRUE(GetPostInstallLaunchCommandLine(app_).IsEmpty());
  EXPECT_TRUE(GetPostInstallUrl(app_).IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, GetPostInstallAction(app_));
}

TEST_F(InstallManagerInstallAppUserTest, InstallApp_NoPackage) {
  // InstallApp asserts that there is at least one package.
  ExpectAsserts expect_asserts;

  EXPECT_EQ(E_FAIL, InstallApp(_T(""), app_, NULL));

  EXPECT_EQ(STATE_ERROR, app_->state());
  EXPECT_EQ(PingEvent::EVENT_RESULT_ERROR, GetCompletionResult(app_));
  EXPECT_EQ(E_FAIL, GetResultCode(app_));
  EXPECT_STREQ(
      _T("Installation failed. Please try again."),
      GetCompletionMessage(app_));
  EXPECT_TRUE(GetPostInstallLaunchCommandLine(app_).IsEmpty());
  EXPECT_TRUE(GetPostInstallUrl(app_).IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, GetPostInstallAction(app_));
}

TEST_F(InstallManagerInstallAppUserTest, InstallApp_ExeFileDoesNotExist) {
  app_->next_version()->AddPackage(_T("foo.exe"), 100, _T("sha256hash"));

  // TODO(omaha): We should be able to eliminate this.
  SetArgumentsInManifest(CString(), _T("1.2.3.4"), app_);

  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_FAILED_START,
            InstallApp(_T(""), app_, _T("c:\\temp")));

  EXPECT_EQ(STATE_ERROR, app_->state());
  EXPECT_EQ(PingEvent::EVENT_RESULT_ERROR, GetCompletionResult(app_));
  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_FAILED_START, GetResultCode(app_));
  EXPECT_STREQ(_T("The installer failed to start."),
               GetCompletionMessage(app_));

  EXPECT_FALSE(RegKey::HasKey(kFullAppClientsKeyPath));
  EXPECT_FALSE(RegKey::HasKey(kFullAppClientStateKeyPath));
  EXPECT_TRUE(GetPostInstallLaunchCommandLine(app_).IsEmpty());
  EXPECT_TRUE(GetPostInstallUrl(app_).IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, GetPostInstallAction(app_));
}

//
// EXE Installer Tests
//

// TODO(omaha3): Add InstallApp_ExeInstallerWithoutArgumentsSucceeds using
// SaveArguments.exe. Do the same in InstallerWrapperUserTest.

// TODO(omaha3): Add InstallApp tests that cause
// ReadInstallerRegistrationValues & CheckApplicationRegistration to fail.

// This test uses cmd.exe as an installer that leaves the payload
// kPayloadFileName.
TEST_F(InstallManagerInstallAppUserTest,
       InstallApp_ExeInstallerWithArgumentsSucceeds) {
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

  app_->next_version()->AddPackage(kCmdExecutable, 100, _T("sha256hash"));
  EXPECT_SUCCEEDED(app_->put_displayName(CComBSTR(_T("Exe App"))));

  SetArgumentsInManifest(arguments, _T("0.10.69.5"), app_);

  EXPECT_SUCCEEDED(InstallApp(_T(""), app_, cmd_exe_dir_));

  EXPECT_EQ(STATE_INSTALL_COMPLETE, app_->state());
  EXPECT_EQ(PingEvent::EVENT_RESULT_SUCCESS, GetCompletionResult(app_));
  EXPECT_EQ(S_OK, GetResultCode(app_));
  EXPECT_STREQ(_T("Thanks for installing."), GetCompletionMessage(app_));
  EXPECT_TRUE(GetPostInstallLaunchCommandLine(app_).IsEmpty());
  EXPECT_TRUE(GetPostInstallUrl(app_).IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, GetPostInstallAction(app_));

  RegKey state_key;
  EXPECT_SUCCEEDED(state_key.Open(kFullAppClientStateKeyPath));
  EXPECT_EQ(1 + kNumAutoPopulatedValues, state_key.GetValueCount());
  EXPECT_STREQ(_T("0.10.69.5"), GetSzValue(kFullAppClientStateKeyPath,
                                           kRegValueProductVersion));

  EXPECT_TRUE(File::Exists(kPayloadFileName));
  EXPECT_SUCCEEDED(File::Remove(kPayloadFileName));
}

TEST_F(InstallManagerInstallAppUserTest,
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

  app_->next_version()->AddPackage(kCmdExecutable, 100, _T("sha256hash"));

  SetArgumentsInManifest(arguments, _T("0.10.69.5"), app_);

  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_FAILED,
            InstallApp(_T(""), app_, cmd_exe_dir_));

  EXPECT_EQ(STATE_ERROR, app_->state());
  EXPECT_EQ(PingEvent::EVENT_RESULT_INSTALLER_ERROR_OTHER,
            GetCompletionResult(app_));
  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_FAILED, GetResultCode(app_));
  EXPECT_STREQ(_T("The installer encountered error 1."),
               GetCompletionMessage(app_));
  EXPECT_TRUE(GetPostInstallLaunchCommandLine(app_).IsEmpty());
  EXPECT_TRUE(GetPostInstallUrl(app_).IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, GetPostInstallAction(app_));
}

TEST_F(InstallManagerInstallAppMachineTest, InstallApp_MsiInstallerSucceeds) {
  if (!vista_util::IsUserAdmin()) {
    std::wcout << _T("\tTest did not run because the user is not an admin.")
               << std::endl;
    return;
  }

  const CString kIid = _T("{F7598FEE-4BA9-4755-A1FC-6EB0A6F3D126}");

  // We can't fake the registry keys because we are interacting with a real
  // installer.
  RestoreRegistryHives();

  AdjustMsiTries(installer_wrapper_.get());

  CString installer_dir(app_util::GetCurrentModuleDirectory());

  CString installer_full_path(
      ConcatenatePath(installer_dir, kSetupFooV1RelativeLocation));
  ASSERT_TRUE(File::Exists(installer_full_path));

  CString installer_log_full_path;
  installer_log_full_path.Format(kMsiLogFormat, installer_full_path);

  ASSERT_SUCCEEDED(File::Remove(installer_log_full_path));
  ASSERT_FALSE(File::Exists(installer_log_full_path));

  RegKey::DeleteKey(kFullFooAppClientKeyPath);
  ASSERT_FALSE(RegKey::HasKey(kFullFooAppClientKeyPath));
  RegKey::DeleteKey(kFullFooAppClientStateKeyPath);
  ASSERT_FALSE(RegKey::HasKey(kFullFooAppClientStateKeyPath));

  // Accepting the EULA prevents "eulaaccepted" value from being written.
  EXPECT_SUCCEEDED(app_->put_isEulaAccepted(VARIANT_TRUE));

  // TODO(omaha): This should be just a filename.
  app_->next_version()->AddPackage(kSetupFooV1RelativeLocation,
                                   100,
                                   _T("sha256hash"));
  app_->set_app_guid(StringToGuid(kFooId));
  EXPECT_SUCCEEDED(app_->put_displayName(CComBSTR(_T("Foo"))));
  EXPECT_SUCCEEDED(app_->put_iid(CComBSTR(kIid)));

  // TODO(omaha): We should be able to eliminate this.
  SetArgumentsInManifest(CString(), kFooVersion, app_);

  EXPECT_SUCCEEDED(InstallApp(_T(""), app_, installer_dir));

  EXPECT_EQ(STATE_INSTALL_COMPLETE, app_->state());
  EXPECT_EQ(PingEvent::EVENT_RESULT_SUCCESS, GetCompletionResult(app_));
  EXPECT_EQ(S_OK, GetResultCode(app_));
  EXPECT_STREQ(_T("Thanks for installing."),
               GetCompletionMessage(app_));
  EXPECT_TRUE(GetPostInstallLaunchCommandLine(app_).IsEmpty());
  EXPECT_TRUE(GetPostInstallUrl(app_).IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, GetPostInstallAction(app_));

  EXPECT_TRUE(File::Exists(installer_log_full_path));

  EXPECT_TRUE(RegKey::HasKey(kFullFooAppClientKeyPath));
  EXPECT_TRUE(RegKey::HasKey(kFullFooAppClientStateKeyPath));
  RegKey state_key;
  EXPECT_SUCCEEDED(state_key.Open(kFullFooAppClientStateKeyPath));
  EXPECT_EQ(4 + kNumAutoPopulatedValues, state_key.GetValueCount());
  EXPECT_STREQ(kFooVersion, GetSzValue(kFullFooAppClientStateKeyPath,
                                       kRegValueProductVersion));
  EXPECT_FALSE(RegKey::HasValue(kFullFooAppClientStateKeyPath,
                                kRegValueLanguage));
  EXPECT_STREQ(kIid, GetSzValue(kFullFooAppClientStateKeyPath,
                                kRegValueInstallationId));

  // Verify the installer did not write a value that is to be written only in
  // the presence of an MSI property that was not specified.
  EXPECT_FALSE(RegKey::HasValue(kFullFooAppClientKeyPath,
                                kFooInstallerBarValueName));

  UninstallTestMsi(installer_full_path);

  EXPECT_FALSE(RegKey::HasKey(kFullFooAppClientKeyPath));
  EXPECT_SUCCEEDED(RegKey::DeleteKey(kFullFooAppClientKeyPath));
}

TEST_F(InstallManagerInstallAppMachineTest,
       InstallApp_MsiInstallerWithArgumentSucceeds) {
  if (!vista_util::IsUserAdmin()) {
    std::wcout << _T("\tTest did not run because the user is not an admin.")
               << std::endl;
    return;
  }

  // We can't fake the registry keys because we are interacting with a real
  // installer.
  RestoreRegistryHives();

  AdjustMsiTries(installer_wrapper_.get());

  CString installer_dir(app_util::GetCurrentModuleDirectory());

  CString installer_full_path(
      ConcatenatePath(installer_dir, kSetupFooV1RelativeLocation));
  ASSERT_TRUE(File::Exists(installer_full_path));

  CString installer_log_full_path;
  installer_log_full_path.Format(kMsiLogFormat, installer_full_path);

  ASSERT_SUCCEEDED(File::Remove(installer_log_full_path));
  ASSERT_FALSE(File::Exists(installer_log_full_path));

  RegKey::DeleteKey(kFullFooAppClientKeyPath);
  ASSERT_FALSE(RegKey::HasKey(kFullFooAppClientKeyPath));
  RegKey::DeleteKey(kFullFooAppClientStateKeyPath);
  ASSERT_FALSE(RegKey::HasKey(kFullFooAppClientStateKeyPath));

  // Write an iid to verify it gets deleted below.
  EXPECT_SUCCEEDED(
      RegKey::SetValue(kFullFooAppClientStateKeyPath,
                       kRegValueInstallationId,
                       _T("{A30B6C0A-B491-473e-9D24-E1AC1BC1D42F}")));

  // Accepting the EULA prevents "eulaaccepted" value from being written.
  EXPECT_SUCCEEDED(app_->put_isEulaAccepted(VARIANT_TRUE));

  // TODO(omaha): This should be just a filename.
  app_->next_version()->AddPackage(kSetupFooV1RelativeLocation,
                                   100,
                                   _T("sha256hash"));
  app_->set_app_guid(StringToGuid(kFooId));
  EXPECT_SUCCEEDED(app_->put_displayName(CComBSTR(_T("Foo"))));

  SetArgumentsInManifest(kFooInstallerBarPropertyArg, kFooVersion, app_);

  EXPECT_SUCCEEDED(InstallApp(_T(""), app_, installer_dir));

  EXPECT_EQ(STATE_INSTALL_COMPLETE, app_->state());
  EXPECT_EQ(PingEvent::EVENT_RESULT_SUCCESS, GetCompletionResult(app_));
  EXPECT_EQ(S_OK, GetResultCode(app_));
  EXPECT_STREQ(_T("Thanks for installing."),
               GetCompletionMessage(app_));
  EXPECT_TRUE(GetPostInstallLaunchCommandLine(app_).IsEmpty());
  EXPECT_TRUE(GetPostInstallUrl(app_).IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, GetPostInstallAction(app_));

  EXPECT_TRUE(File::Exists(installer_log_full_path));

  EXPECT_TRUE(RegKey::HasKey(kFullFooAppClientKeyPath));
  EXPECT_TRUE(RegKey::HasKey(kFullFooAppClientStateKeyPath));
  RegKey state_key;
  EXPECT_SUCCEEDED(state_key.Open(kFullFooAppClientStateKeyPath));
  EXPECT_EQ(4 + kNumAutoPopulatedValues, state_key.GetValueCount());
  EXPECT_STREQ(kFooVersion, GetSzValue(kFullFooAppClientStateKeyPath,
                                       kRegValueProductVersion));
  EXPECT_FALSE(RegKey::HasValue(kFullFooAppClientStateKeyPath,
                                kRegValueLanguage));
  EXPECT_FALSE(RegKey::HasValue(kFullFooAppClientStateKeyPath,
                                kRegValueInstallationId));

  EXPECT_TRUE(RegKey::HasValue(kFullFooAppClientStateKeyPath,
                               kFooInstallerBarValueName));
  DWORD barprop_value;
  EXPECT_SUCCEEDED(RegKey::GetValue(kFullFooAppClientStateKeyPath,
                                    kFooInstallerBarValueName,
                                    &barprop_value));
  EXPECT_EQ(7, barprop_value);

  UninstallTestMsi(installer_full_path);

  EXPECT_FALSE(RegKey::HasKey(kFullFooAppClientKeyPath));
  EXPECT_SUCCEEDED(RegKey::DeleteKey(kFullFooAppClientKeyPath));
}

// The use of kGoogleUpdateAppId is the key to this test.
// Note that the version is not changed - this is the normal self-update case.
// Among other things, this test verifies that CheckApplicationRegistration() is
// not called for self-updates.
TEST_F(InstallManagerInstallAppUserTest, InstallApp_UpdateOmahaSucceeds) {
  const CString kExistingVersion(_T("0.9.69.5"));

  app_->next_version()->AddPackage(_T("SaveArguments.exe"),
                                   100,
                                   _T("sha256hash"));
  app_->set_app_guid(StringToGuid(kGoogleUpdateAppId));

  // TODO(omaha3): This isn't supported yet.
#if 0
  isupdate = true
#endif

  SetArgumentsInManifest(_T(""), _T("1.2.9.8"), app_);

  // Because we don't actually run the Omaha installer, we need to make sure
  // its Clients key and pv value exist to avoid an error.
  ASSERT_SUCCEEDED(RegKey::CreateKey(USER_REG_CLIENTS_GOOPDATE));
  ASSERT_TRUE(RegKey::HasKey(USER_REG_CLIENTS_GOOPDATE));
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                    kRegValueProductVersion,
                                    kExistingVersion));

  EXPECT_SUCCEEDED(InstallApp(kExistingVersion, app_, unittest_support_dir_));

  EXPECT_EQ(STATE_INSTALL_COMPLETE, app_->state());
  EXPECT_EQ(PingEvent::EVENT_RESULT_SUCCESS, GetCompletionResult(app_));
  EXPECT_EQ(S_OK, GetResultCode(app_));
  EXPECT_STREQ(_T("Thanks for installing."), GetCompletionMessage(app_));
  EXPECT_TRUE(GetPostInstallLaunchCommandLine(app_).IsEmpty());
  EXPECT_TRUE(GetPostInstallUrl(app_).IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, GetPostInstallAction(app_));

  EXPECT_TRUE(RegKey::HasKey(USER_REG_CLIENTS_GOOPDATE));
  CString version;
  EXPECT_SUCCEEDED(RegKey::GetValue(USER_REG_CLIENTS_GOOPDATE,
                                    kRegValueProductVersion,
                                    &version));
  EXPECT_STREQ(kExistingVersion, version);
}

// The main purpose of this test is to ensure that self-updates don't fail if
// Omaha's Clients key doesn't exist for some reason.
// In other words, it tests that CheckApplicationRegistration() is not called.
TEST_F(InstallManagerInstallAppUserTest,
       InstallApp_UpdateOmahaSucceedsWhenClientsKeyAbsent) {
  const CString kExistingVersion(_T("0.9.69.5"));

  app_->next_version()->AddPackage(_T("SaveArguments.exe"),
                                   100,
                                   _T("sha256hash"));
  app_->set_app_guid(StringToGuid(kGoogleUpdateAppId));

  // TODO(omaha3): This isn't supported yet.
#if 0
  isupdate = true
#endif

  SetArgumentsInManifest(_T(""), _T("1.2.9.8"), app_);

  RegKey::DeleteKey(USER_REG_CLIENTS_GOOPDATE);

  EXPECT_SUCCEEDED(InstallApp(kExistingVersion, app_, unittest_support_dir_));

  EXPECT_EQ(STATE_INSTALL_COMPLETE, app_->state());
  EXPECT_EQ(PingEvent::EVENT_RESULT_SUCCESS, GetCompletionResult(app_));
  EXPECT_EQ(S_OK, GetResultCode(app_));
  EXPECT_STREQ(_T("Thanks for installing."), GetCompletionMessage(app_));
  EXPECT_TRUE(GetPostInstallLaunchCommandLine(app_).IsEmpty());
  EXPECT_TRUE(GetPostInstallUrl(app_).IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, GetPostInstallAction(app_));

  EXPECT_FALSE(RegKey::HasKey(USER_REG_CLIENTS_GOOPDATE));
}

TEST_F(InstallManagerInstallAppUserTest,
       InstallApp_InstallerDoesNotWriteClientsKey) {
  CString arguments;
  arguments.Format(kExecuteCommandAndTerminateSwitch, _T(""));

  app_->next_version()->AddPackage(kCmdExecutable, 100, _T("sha256hash"));
  EXPECT_SUCCEEDED(app_->put_displayName(CComBSTR(_T("Some App"))));

  SetArgumentsInManifest(arguments, _T("5.6.7.8"), app_);

  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_DID_NOT_WRITE_CLIENTS_KEY,
            InstallApp(_T(""), app_, cmd_exe_dir_));

  EXPECT_FALSE(RegKey::HasKey(kFullAppClientsKeyPath));
  EXPECT_FALSE(RegKey::HasKey(kFullAppClientStateKeyPath));

  EXPECT_EQ(STATE_ERROR, app_->state());
  EXPECT_EQ(PingEvent::EVENT_RESULT_ERROR, GetCompletionResult(app_));
  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_DID_NOT_WRITE_CLIENTS_KEY,
            GetResultCode(app_));
  EXPECT_STREQ(
      _T("Installation failed. Please try again."),
      GetCompletionMessage(app_));
  EXPECT_TRUE(GetPostInstallLaunchCommandLine(app_).IsEmpty());
  EXPECT_TRUE(GetPostInstallUrl(app_).IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, GetPostInstallAction(app_));
}

// TODO(omaha3): Test for GOOPDATEINSTALL_E_INSTALLER_DID_NOT_CHANGE_VERSION
// once is_update is supported.

TEST_F(InstallManagerInstallAppUserTest,
       InstallApp_InstallerFailureMsiFileDoesNotExist) {
  CPath msi_dir(app_util::GetTempDir());
  CPath msi_path(msi_dir);
  msi_path.Append(_T("foo.msi"));
  const CString log_path = msi_path + _T(".log");

  AdjustMsiTries(installer_wrapper_.get());

  ASSERT_SUCCEEDED(File::Remove(log_path));
  ASSERT_FALSE(File::Exists(log_path));

  app_->next_version()->AddPackage(_T("foo.msi"), 100, _T("sha256hash"));

  // TODO(omaha): We should be able to eliminate this.
  SetArgumentsInManifest(CString(), _T("1.2.3.4"), app_);

  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_FAILED,
            InstallApp(_T(""), app_, msi_dir));

  EXPECT_EQ(STATE_ERROR, app_->state());
  EXPECT_EQ(PingEvent::EVENT_RESULT_INSTALLER_ERROR_MSI,
            GetCompletionResult(app_));
  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_FAILED, GetResultCode(app_));
  const CString message = GetCompletionMessage(app_);
  EXPECT_STREQ(kError1619MessagePrefix,
               message.Left(kError1619MessagePrefixLength));
  VerifyStringIsMsiPackageOpenFailedString(
      message.Mid(kError1619MessagePrefixLength));
  EXPECT_TRUE(GetPostInstallLaunchCommandLine(app_).IsEmpty());
  EXPECT_TRUE(GetPostInstallUrl(app_).IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, GetPostInstallAction(app_));

  // msiexec creates an empty log file.
  EXPECT_TRUE(File::Exists(log_path));
  EXPECT_SUCCEEDED(File::Remove(log_path));

  EXPECT_FALSE(RegKey::HasKey(kFullAppClientsKeyPath));
  EXPECT_FALSE(RegKey::HasKey(kFullAppClientStateKeyPath));
}

// Simulates the MSI busy error by having an exe installer return the error
// as its exit code and specifying MSI error using Installer Result API.
// Assumes reg.exe is in the path.
// This works because the number of retries is set before InstallApp() and thus
// defaults to 1 in the InstallerWrapper.
TEST_F(InstallManagerInstallAppUserTest, InstallApp_MsiIsBusy_NoRetries) {
  CString commands;
  commands.Format(kExecuteTwoCommandsFormat,
                  set_installer_result_type_msi_error_cmd_,
                  kMsiInstallerBusyExitCodeCmd);

  CString arguments;
  arguments.Format(kExecuteCommandAndTerminateSwitch, commands);

  app_->next_version()->AddPackage(kCmdExecutable, 100, _T("sha256hash"));
  EXPECT_SUCCEEDED(app_->put_displayName(CComBSTR(_T("Some App"))));

  SetArgumentsInManifest(arguments, _T("1.2.3.4"), app_);

  LowResTimer install_timer(true);

  // Disable the signature verification in order to run cmd.exe.
  app_->set_can_skip_signature_verification(true);
  EXPECT_EQ(GOOPDATEINSTALL_E_MSI_INSTALL_ALREADY_RUNNING,
            InstallApp(_T(""), app_, cmd_exe_dir_));

  EXPECT_GT(2, install_timer.GetSeconds());  // Check Omaha did not retry.

  EXPECT_EQ(STATE_ERROR, app_->state());
  // Even though the error came from the installer, the error type is not
  // set because we have a custom error for this case.
  EXPECT_EQ(PingEvent::EVENT_RESULT_ERROR, GetCompletionResult(app_));
  EXPECT_EQ(GOOPDATEINSTALL_E_MSI_INSTALL_ALREADY_RUNNING,
            GetResultCode(app_));
  EXPECT_STREQ(kMsiInstallerBusyErrorMessage, GetCompletionMessage(app_));
  EXPECT_TRUE(GetPostInstallLaunchCommandLine(app_).IsEmpty());
  EXPECT_TRUE(GetPostInstallUrl(app_).IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, GetPostInstallAction(app_));
}

// This test uses cmd.exe as an installer that leaves the payload files.
TEST_F(InstallManagerInstallAppUserTest, InstallApp_InstallMultipleApps) {
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

  app_->next_version()->AddPackage(kCmdExecutable, 100, _T("sha256hash"));
  EXPECT_SUCCEEDED(app_->put_displayName(CComBSTR(_T("Exe App"))));

  SetArgumentsInManifest(arguments1, _T("0.10.69.5"), app_);

  EXPECT_SUCCEEDED(InstallApp(_T(""), app_, cmd_exe_dir_));

  EXPECT_EQ(STATE_INSTALL_COMPLETE, app_->state());
  EXPECT_EQ(PingEvent::EVENT_RESULT_SUCCESS, GetCompletionResult(app_));
  EXPECT_EQ(S_OK, GetResultCode(app_));
  EXPECT_STREQ(_T("Thanks for installing."), GetCompletionMessage(app_));
  EXPECT_TRUE(GetPostInstallLaunchCommandLine(app_).IsEmpty());
  EXPECT_TRUE(GetPostInstallUrl(app_).IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, GetPostInstallAction(app_));

  EXPECT_TRUE(File::Exists(kPayloadFileName1));
  EXPECT_SUCCEEDED(File::Remove(kPayloadFileName1));

  // Run the second installer.

  App* app2 = NULL;
  ASSERT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kApp2Id), &app2));
  SetAppStateWaitingToInstall(app2);

  // Create the Clients key since this isn't an actual installer.
  EXPECT_SUCCEEDED(RegKey::CreateKey(kFullAppClientsKeyPath));
  EXPECT_TRUE(RegKey::HasKey(kFullAppClientsKeyPath));
  EXPECT_SUCCEEDED(RegKey::SetValue(kFullApp2ClientsKeyPath,
                                    kRegValueProductVersion,
                                    _T("0.10.69.5")));

  app2->next_version()->AddPackage(kCmdExecutable, 100, _T("sha256hash"));
  EXPECT_SUCCEEDED(app2->put_displayName(CComBSTR(_T("Exe App"))));

  SetArgumentsInManifest(arguments2, _T("0.10.69.5"), app2);

  EXPECT_SUCCEEDED(InstallApp(_T(""), app2, cmd_exe_dir_));

  EXPECT_EQ(STATE_INSTALL_COMPLETE, app2->state());
  EXPECT_EQ(PingEvent::EVENT_RESULT_SUCCESS, GetCompletionResult(app2));
  EXPECT_EQ(S_OK, GetResultCode(app2));
  EXPECT_STREQ(_T("Thanks for installing."), GetCompletionMessage(app2));
  EXPECT_TRUE(GetPostInstallLaunchCommandLine(app_).IsEmpty());
  EXPECT_TRUE(GetPostInstallUrl(app_).IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, GetPostInstallAction(app_));

  EXPECT_TRUE(File::Exists(kPayloadFileName2));
  EXPECT_SUCCEEDED(File::Remove(kPayloadFileName2));
}

TEST_F(InstallManagerInstallAppUserTest,
       PopulateSuccessfulInstallResultInfo_ExitSilentlyOnLaunchCommandWithNoCommand) {  // NOLINT
  SetPostInstallActionInManifest(SUCCESS_ACTION_EXIT_SILENTLY_ON_LAUNCH_CMD,
                                 _T(""),
                                 false,
                                 app_);

  InstallerResultInfo result_info;
  result_info.type = INSTALLER_RESULT_SUCCESS;
  PopulateSuccessfulInstallResultInfo(app_, &result_info);
  EXPECT_TRUE(GetPostInstallLaunchCommandLine(app_).IsEmpty());
  EXPECT_TRUE(GetPostInstallUrl(app_).IsEmpty());
  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, result_info.post_install_action);
}

// Verify that launch command is converted to silently launch command
// if success action is silently launch command. Also tests that launch cmd
// takes precedence over URL.
TEST_F(InstallManagerInstallAppUserTest,
       PopulateSuccessfulInstallResultInfo_ExitSilentlyOnLaunchCommand) {
  SetPostInstallActionInManifest(SUCCESS_ACTION_EXIT_SILENTLY_ON_LAUNCH_CMD,
                                 _T("http://post_install_succeeded"),
                                 true,
                                 app_);

  InstallerResultInfo result_info;
  result_info.type = INSTALLER_RESULT_SUCCESS;
  result_info.post_install_action = POST_INSTALL_ACTION_LAUNCH_COMMAND;
  result_info.post_install_launch_command_line = _T("notepad.exe");
  PopulateSuccessfulInstallResultInfo(app_, &result_info);

  EXPECT_EQ(POST_INSTALL_ACTION_EXIT_SILENTLY_ON_LAUNCH_COMMAND,
            result_info.post_install_action);
}

TEST_F(InstallManagerInstallAppUserTest,
    PopulateSuccessfulInstallResultInfo_LaunchCommandWithDefaultSuccessAction) {
  SetPostInstallActionInManifest(SUCCESS_ACTION_DEFAULT,
                                 _T(""),
                                 true,
                                 app_);

  InstallerResultInfo result_info;
  result_info.type = INSTALLER_RESULT_SUCCESS;
  result_info.post_install_action = POST_INSTALL_ACTION_LAUNCH_COMMAND;
  result_info.post_install_launch_command_line = _T("notepad.exe");
  PopulateSuccessfulInstallResultInfo(app_, &result_info);

  EXPECT_EQ(POST_INSTALL_ACTION_LAUNCH_COMMAND,
            result_info.post_install_action);
}

// Verify that default install action is converted to exit silently if
// if success action is exit silently.
TEST_F(InstallManagerInstallAppUserTest,
       PopulateSuccessfulInstallResultInfo_ExitSilently) {
  SetPostInstallActionInManifest(SUCCESS_ACTION_EXIT_SILENTLY,
                                 _T(""),
                                 false,
                                 app_);

  InstallerResultInfo result_info;
  result_info.type = INSTALLER_RESULT_SUCCESS;
  result_info.post_install_action = POST_INSTALL_ACTION_DEFAULT;
  PopulateSuccessfulInstallResultInfo(app_, &result_info);

  EXPECT_EQ(POST_INSTALL_ACTION_EXIT_SILENTLY, result_info.post_install_action);
}

TEST_F(InstallManagerInstallAppUserTest,
       PopulateSuccessfulInstallResultInfo_RestartBrowser) {
  SetPostInstallActionInManifest(SUCCESS_ACTION_DEFAULT,
                                 _T("http://www.google.com/foo/installed_ok"),
                                 false,
                                 app_);

  InstallerResultInfo result_info;
  result_info.type = INSTALLER_RESULT_SUCCESS;
  result_info.post_install_action = POST_INSTALL_ACTION_DEFAULT;
  PopulateSuccessfulInstallResultInfo(app_, &result_info);

  EXPECT_EQ(POST_INSTALL_ACTION_RESTART_BROWSER,
            result_info.post_install_action);
}

TEST_F(InstallManagerInstallAppUserTest,
       PopulateSuccessfulInstallResultInfo_RestartAllBrowsers) {
  SetPostInstallActionInManifest(SUCCESS_ACTION_DEFAULT,
                                 _T("http://www.google.com/gears/installed_ok"),
                                 true,
                                 app_);

  InstallerResultInfo result_info;
  result_info.type = INSTALLER_RESULT_SUCCESS;
  result_info.post_install_action = POST_INSTALL_ACTION_DEFAULT;
  PopulateSuccessfulInstallResultInfo(app_, &result_info);

  EXPECT_EQ(POST_INSTALL_ACTION_RESTART_ALL_BROWSERS,
            result_info.post_install_action);
}

TEST_F(InstallManagerInstallAppUserTest,
       PopulateSuccessfulInstallResultInfo_NoPostInstallUrl) {
  SetPostInstallActionInManifest(SUCCESS_ACTION_DEFAULT,
                                 _T(""),
                                 true,
                                 app_);

  InstallerResultInfo result_info;
  result_info.type = INSTALLER_RESULT_SUCCESS;
  result_info.post_install_action = POST_INSTALL_ACTION_DEFAULT;
  PopulateSuccessfulInstallResultInfo(app_, &result_info);

  EXPECT_EQ(POST_INSTALL_ACTION_DEFAULT, result_info.post_install_action);
}


TEST_F(InstallManagerInstallAppUserTest,
       PopulateSuccessfulInstallResultInfo_RebootShouldNotBeOverridden) {
  const SuccessfulInstallAction kSuccessActions[] = {
    SUCCESS_ACTION_DEFAULT,
    SUCCESS_ACTION_EXIT_SILENTLY,
    SUCCESS_ACTION_EXIT_SILENTLY_ON_LAUNCH_CMD,
  };

  for (int i = 0; i < arraysize(kSuccessActions); ++i) {
    SetPostInstallActionInManifest(kSuccessActions[i],
                                   _T("http://foo/bar"),
                                   true,
                                   app_);

    InstallerResultInfo result_info;
    result_info.type = INSTALLER_RESULT_SUCCESS;
    result_info.post_install_action = POST_INSTALL_ACTION_REBOOT;
    result_info.post_install_launch_command_line = _T("foo.exe");
    PopulateSuccessfulInstallResultInfo(app_, &result_info);

    EXPECT_EQ(POST_INSTALL_ACTION_REBOOT, result_info.post_install_action);
  }
}

INSTANTIATE_TEST_CASE_P(IsMachine, InstallManagerTest, ::testing::Bool());

TEST_P(InstallManagerTest, InstallDir_NoFiles) {
  const CString install_dir = GetInstallWorkingDir();
  EXPECT_SUCCEEDED(DeleteDirectory(install_dir));

  EXPECT_SUCCEEDED(CreateDir(install_dir, NULL));
  EXPECT_TRUE(::PathIsDirectoryEmpty(install_dir));

  FakeGLock fake_glock;
  InstallManager install_manager(&fake_glock, IsMachine());

  EXPECT_TRUE(File::Exists(install_dir));
  EXPECT_TRUE(::PathIsDirectoryEmpty(install_dir));

  EXPECT_SUCCEEDED(DeleteDirectory(install_dir));
}

TEST_P(InstallManagerTest, InstallDir_OnlyFilesNoDirs) {
  const CString install_dir = GetInstallWorkingDir();
  EXPECT_SUCCEEDED(DeleteDirectory(install_dir));

  EXPECT_SUCCEEDED(CreateDir(install_dir, NULL));

  const FileStruct kFiles[] = {
    { _T("tst8875"), FILE_ATTRIBUTE_NORMAL, },
    { _T("tst8876.tmp"), FILE_ATTRIBUTE_NORMAL, },
    { _T("tst8887.tmp"), FILE_ATTRIBUTE_NORMAL, },
  };

  CreateFiles(install_dir, kFiles, arraysize(kFiles));
  EXPECT_FALSE(::PathIsDirectoryEmpty(install_dir));

  FakeGLock fake_glock;
  InstallManager install_manager(&fake_glock, IsMachine());

  EXPECT_TRUE(File::Exists(install_dir));
  EXPECT_TRUE(::PathIsDirectoryEmpty(install_dir));

  EXPECT_SUCCEEDED(DeleteDirectory(install_dir));
}

TEST_P(InstallManagerTest, InstallDir_MixedAttributeFiles) {
  const CString install_dir = GetInstallWorkingDir();
  EXPECT_SUCCEEDED(DeleteDirectory(install_dir));

  EXPECT_SUCCEEDED(CreateDir(install_dir, NULL));

  const FileStruct kFiles[] = {
    { _T("tst8875"), FILE_ATTRIBUTE_SYSTEM, },
    { _T("tst8887.tmp"), FILE_ATTRIBUTE_HIDDEN, },
  };

  CreateFiles(install_dir, kFiles, arraysize(kFiles));
  EXPECT_FALSE(::PathIsDirectoryEmpty(install_dir));

  FakeGLock fake_glock;
  InstallManager install_manager(&fake_glock, IsMachine());

  EXPECT_TRUE(File::Exists(install_dir));
  EXPECT_TRUE(::PathIsDirectoryEmpty(install_dir));

  EXPECT_SUCCEEDED(DeleteDirectory(install_dir));
}

TEST_P(InstallManagerTest, InstallDir_ReadOnlyFiles) {
  const CString install_dir = GetInstallWorkingDir();
  EXPECT_SUCCEEDED(DeleteDirectory(install_dir));

  EXPECT_SUCCEEDED(CreateDir(install_dir, NULL));

  const FileStruct kFiles[] = {
    { _T("tst8875"), FILE_ATTRIBUTE_SYSTEM, },
    { _T("tst8876.tmp"), FILE_ATTRIBUTE_READONLY, },
    { _T("tst8887.tmp"), FILE_ATTRIBUTE_HIDDEN, },
  };

  CreateFiles(install_dir, kFiles, arraysize(kFiles));
  EXPECT_FALSE(::PathIsDirectoryEmpty(install_dir));

  FakeGLock fake_glock;
  InstallManager install_manager(&fake_glock, IsMachine());

  ::Sleep(10);

  EXPECT_TRUE(File::Exists(install_dir));
  EXPECT_TRUE(::PathIsDirectoryEmpty(install_dir));

  EXPECT_SUCCEEDED(DeleteDirectory(install_dir));
}

TEST_P(InstallManagerTest, InstallDir_FilesAndDirs) {
  const CString install_dir = GetInstallWorkingDir();
  EXPECT_SUCCEEDED(DeleteDirectory(install_dir));

  EXPECT_SUCCEEDED(CreateDir(install_dir, NULL));

  const TCHAR* const kDirectories[] = {
    _T("DirB"),
    _T("DirB\\DirC"),
    _T("DirB\\DirC\\DirD"),
    _T("DirB\\DirC\\DirE"),
    _T("DirB\\DirF"),
    _T("DirB\\DirF\\DirG"),
    _T("DirB\\DirF\\DirH"),
    _T("DirI"),
    _T("DirI\\DirJ"),
    _T("DirI\\DirJ\\DirK"),
    _T("DirI\\DirJ\\DirL"),
    _T("DirI\\DirM"),
    _T("DirI\\DirM\\DirN"),
    _T("DirI\\DirM\\DirO"),
  };

  const FileStruct kFiles[] = {
    { _T("tst8875"), FILE_ATTRIBUTE_NORMAL, },
    { _T("tst8876.tmp"), FILE_ATTRIBUTE_NORMAL, },
    { _T("tst8887.tmp"), FILE_ATTRIBUTE_NORMAL, },
    { _T("DirB\\tst8888.tmp"), FILE_ATTRIBUTE_NORMAL, },
    { _T("DirB\\tst8898.tmp"), FILE_ATTRIBUTE_NORMAL, },
    { _T("DirB\\tst88A9.tmp"), FILE_ATTRIBUTE_NORMAL, },
    { _T("DirB\\DirC\\tst88C9.tmp"), FILE_ATTRIBUTE_NORMAL, },
    { _T("DirB\\DirC\\tst88DA.tmp"), FILE_ATTRIBUTE_NORMAL, },
    { _T("DirB\\DirC\\tst88FA.tmp"), FILE_ATTRIBUTE_NORMAL, },
    { _T("DirB\\DirF\\tst8959.tmp"), FILE_ATTRIBUTE_NORMAL, },
    { _T("DirB\\DirF\\tst8969.tmp"), FILE_ATTRIBUTE_NORMAL, },
    { _T("DirB\\DirF\\tst898A.tmp"), FILE_ATTRIBUTE_NORMAL, },
    { _T("DirI\\tst89E8.tmp"), FILE_ATTRIBUTE_NORMAL, },
    { _T("DirI\\tst89F9.tmp"), FILE_ATTRIBUTE_NORMAL, },
    { _T("DirI\\tst8A0A.tmp"), FILE_ATTRIBUTE_NORMAL, },
    { _T("DirI\\DirJ\\tst8A2A.tmp"), FILE_ATTRIBUTE_NORMAL, },
    { _T("DirI\\DirJ\\tst8A3A.tmp"), FILE_ATTRIBUTE_NORMAL, },
    { _T("DirI\\DirJ\\tst8A5B.tmp"), FILE_ATTRIBUTE_NORMAL, },
    { _T("DirI\\DirM\\tst8A8B.tmp"), FILE_ATTRIBUTE_NORMAL, },
    { _T("DirI\\DirM\\tst8A9B.tmp"), FILE_ATTRIBUTE_NORMAL, },
    { _T("DirI\\DirM\\tst8A9C.tmp"), FILE_ATTRIBUTE_NORMAL, },
  };

  CreateDirs(install_dir, kDirectories, arraysize(kDirectories));
  CreateFiles(install_dir, kFiles, arraysize(kFiles));

  EXPECT_FALSE(::PathIsDirectoryEmpty(install_dir));

  FakeGLock fake_glock;
  InstallManager install_manager(&fake_glock, IsMachine());

  EXPECT_TRUE(File::Exists(install_dir));
  EXPECT_TRUE(::PathIsDirectoryEmpty(install_dir));

  EXPECT_SUCCEEDED(DeleteDirectory(install_dir));
}


}  // namespace omaha
