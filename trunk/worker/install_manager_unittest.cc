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

#include <string>
#include "omaha/common/app_util.h"
#include "omaha/common/error.h"
#include "omaha/common/file.h"
#include "omaha/common/path.h"
#include "omaha/common/reg_key.h"
#include "omaha/common/scoped_ptr_address.h"
#include "omaha/common/sta.h"
#include "omaha/common/system.h"
#include "omaha/common/vistautil.h"
#include "omaha/goopdate/config_manager.h"
#include "omaha/goopdate/const_goopdate.h"
#include "omaha/goopdate/goopdate_utils.h"
#include "omaha/goopdate/resource_manager.h"
#include "omaha/testing/unit_test.h"
#include "omaha/worker/install_manager.h"
#include "omaha/worker/ping.h"

namespace {

// Arbitrary values to fill in the job.
// We use this value for all installers, including test_foo.
const TCHAR kJobGuid[] = _T("{B18BC01B-E0BD-4BF0-A33E-1133055E5FDE}");
const int kJobSize = 31415;
const TCHAR kJobHash[] = _T("InvalidUnusedHashValue");
const TCHAR kJobUrl[] = _T("invalid://url/to/nowhere/");

const TCHAR kFullJobAppClientsKeyPath[] =
    _T("HKCU\\Software\\Google\\Update\\Clients\\")
    _T("{B18BC01B-E0BD-4BF0-A33E-1133055E5FDE}");
const TCHAR kFullJobAppClientStateKeyPath[] =
    _T("HKCU\\Software\\Google\\Update\\ClientState\\")
    _T("{B18BC01B-E0BD-4BF0-A33E-1133055E5FDE}");
const TCHAR kFullFooAppClientKeyPath[] =
    _T("HKLM\\Software\\Google\\Update\\Clients\\")
    _T("{D6B08267-B440-4C85-9F79-E195E80D9937}");
const TCHAR kFullFooAppClientStateKeyPath[] =
    _T("HKLM\\Software\\Google\\Update\\ClientState\\")
    _T("{D6B08267-B440-4C85-9F79-E195E80D9937}");

const TCHAR kSetupFooV1RelativeLocation[] =
  _T("unittest_support\\test_foo_v1.0.101.0.msi");
const TCHAR kFooGuid[] = _T("{D6B08267-B440-4C85-9F79-E195E80D9937}");
const TCHAR kFooInstallerBarPropertyArg[] = _T("PROPBAR=7");
const TCHAR kFooInstallerBarValueName[] = _T("propbar");

// Meaningful values that we do something with.
const TCHAR kJobExecutable[] = _T("cmd.exe");
const TCHAR kExecuteCommandAndTerminateSwitch[] = _T("/c %s");

const TCHAR kMsiLogFormat[] = _T("%s.log");

const TCHAR kMsiUninstallArguments[] = _T("/quiet /uninstall %s");
const TCHAR kMsiCommand[] = _T("msiexec");

const DWORD kInitialErrorValue = 5;
const TCHAR kMeaninglessErrorString[] = _T("This is an error string.");

// The US English error string for ERROR_INSTALL_PACKAGE_OPEN_FAILED.
// It is slightly different on Vista than XP - a space was removed.
// Therefore, the comparison must ignore that space.
const TCHAR kMsiPackageOpenFailedStringPartA[] =
    _T("This installation package could not be opened. ");
const TCHAR kMsiPackageOpenFailedStringPartB[] =
    _T("Verify that the package exists and that you can access it, ")
    _T("or contact the application vendor to verify that this is a ")
    _T("valid Windows Installer package. ");

void VerifyStringIsMsiPackageOpenFailedString(const CString& str) {
  EXPECT_STREQ(kMsiPackageOpenFailedStringPartA,
               str.Left(arraysize(kMsiPackageOpenFailedStringPartA) - 1));
  EXPECT_STREQ(kMsiPackageOpenFailedStringPartB,
               str.Right(arraysize(kMsiPackageOpenFailedStringPartB) - 1));
}

const TCHAR* const kError1603Text =
    _T("The installer encountered error 1603: Fatal error during ")
    _T("installation. ");

const TCHAR* const kError0x800B010FText =
    _T("The installer encountered error 0x800b010f: ")
    _T("The certificate's CN name does not match the passed value. ");

  const TCHAR* const kLaunchCmdLine =
      _T("\"C:\\Local\\Google\\Chrome\\Application\\chrome.exe\" -home");

}  // namespace

namespace omaha {

class InstallManagerTest : public testing::Test {
 protected:
  explicit InstallManagerTest(bool is_machine)
      : is_machine_(is_machine),
        hive_override_key_name_(kRegistryHiveOverrideRoot) {
  }

  virtual void SetUp() {
    install_manager_.reset(new InstallManager(is_machine_));

    RegKey::DeleteKey(hive_override_key_name_, true);
    OverrideRegistryHivesWithExecutionPermissions(hive_override_key_name_);

    ResourceManager manager(is_machine_, app_util::GetCurrentModuleDirectory());
    ASSERT_SUCCEEDED(manager.LoadResourceDll(_T("en")));
  }

  virtual void TearDown() {
    RestoreRegistryHives();
    ASSERT_SUCCEEDED(RegKey::DeleteKey(hive_override_key_name_, true));
  }

  void SetInstallManagerJob(Job* job) {
    ASSERT_TRUE(job);
    install_manager_->job_ = job;
  }

  HRESULT CheckApplicationRegistration(const CString& previous_version,
                                       Job* job) {
    ASSERT1(job);
    install_manager_->job_ = job;
    return install_manager_->CheckApplicationRegistration(previous_version);
  }

  void SetUpdateResponseDataArguments(const CString& arguments, Job* job) {
    ASSERT1(job);
    job->update_response_data_.set_arguments(arguments);
  }

  void SetupInstallerResultRegistry(const CString& app_guid,
                                    bool set_installer_result,
                                    DWORD installer_result,
                                    bool set_installer_error,
                                    DWORD installer_error,
                                    bool set_installer_result_uistring,
                                    const CString& installer_result_uistring,
                                    bool set_installer_launch_cmd_line,
                                    const CString& installer_launch_cmd_line) {
    CString app_client_state_key =
        goopdate_utils::GetAppClientStateKey(is_machine_, app_guid);
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
                                bool expect_installer_result_uistring,
                                const CString& expected_result_uistring,
                                bool expect_installer_launch_cmd_line,
                                const CString& expected_launch_cmd_line) {
    ASSERT_TRUE(expect_installer_result || !expected_installer_result);
    ASSERT_TRUE(expect_installer_error || !expected_installer_error);
    ASSERT_TRUE(expect_installer_result_uistring ||
                expected_result_uistring.IsEmpty());
    ASSERT_TRUE(expect_installer_launch_cmd_line ||
                expected_launch_cmd_line.IsEmpty());

    CString app_client_state_key =
        goopdate_utils::GetAppClientStateKey(is_machine_, app_guid);
    EXPECT_FALSE(RegKey::HasValue(app_client_state_key,
                                  kRegValueInstallerResult));
    EXPECT_FALSE(RegKey::HasValue(app_client_state_key,
                                  kRegValueInstallerError));
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
                             false, _T(""),
                             false, _T(""));
  }

  void GetInstallerResultHelper(const CString& app_guid,
                                int installer_type,
                                uint32 exit_code,
                                CompletionInfo* completion_info) {
    install_manager_->GetInstallerResultHelper(
        app_guid,
        static_cast<InstallManager::InstallerType>(installer_type),
        exit_code,
        completion_info);
  }

  static const int kResultSuccess = InstallManager::INSTALLER_RESULT_SUCCESS;
  static const int kResultFailedCustomError =
      InstallManager::INSTALLER_RESULT_FAILED_CUSTOM_ERROR;
  static const int kResultFailedMsiError =
      InstallManager::INSTALLER_RESULT_FAILED_MSI_ERROR;
  static const int kResultFailedSystemError =
      InstallManager::INSTALLER_RESULT_FAILED_SYSTEM_ERROR;
  static const int kResultExitCode = InstallManager::INSTALLER_RESULT_EXIT_CODE;

  static const int kMsiInstaller = InstallManager::MSI_INSTALLER;
  static const int kOtherInstaller = InstallManager::CUSTOM_INSTALLER;

  bool is_machine_;
  CString hive_override_key_name_;
  scoped_ptr<InstallManager> install_manager_;
  Ping ping_;
};

class InstallManagerMachineTest : public InstallManagerTest {
 protected:
  InstallManagerMachineTest()
    : InstallManagerTest(true) {
  }
};

class InstallManagerUserTest : public InstallManagerTest {
 protected:
  InstallManagerUserTest()
    : InstallManagerTest(false) {
  }
};

class InstallManagerUserGetInstallerResultHelperTest
    : public InstallManagerUserTest {
 protected:
  InstallManagerUserGetInstallerResultHelperTest()
      : InstallManagerUserTest() {
  }

  virtual void SetUp() {
    InstallManagerUserTest::SetUp();

    completion_info_.error_code = kInitialErrorValue;
    completion_info_.text = kMeaninglessErrorString;

    job_.reset(new Job(false, &ping_));
    SetInstallManagerJob(job_.get());
  }

  CompletionInfo completion_info_;
  scoped_ptr<Job> job_;
};


//
// Helper method tests
//

bool GetMessageForSystemErrorCode(DWORD system_error_code, CString* message);

TEST(InstallManagerTest, TestGetMessageForSystemErrorCode) {
  CString message;

  EXPECT_TRUE(GetMessageForSystemErrorCode(ERROR_INSTALL_PACKAGE_OPEN_FAILED,
                                           &message));
  VerifyStringIsMsiPackageOpenFailedString(message);
}

TEST_F(InstallManagerUserTest,
       CheckApplicationRegistration_InstallFailsWhenClientsKeyAbsent) {
  AppData app_data(StringToGuid(kJobGuid), is_machine_);
  Job job(false, &ping_);
  job.set_app_data(app_data);

  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_DID_NOT_WRITE_CLIENT_KEY,
            CheckApplicationRegistration(CString(), &job));

  EXPECT_FALSE(RegKey::HasKey(kFullJobAppClientsKeyPath));
  EXPECT_FALSE(RegKey::HasKey(kFullJobAppClientStateKeyPath));
}

TEST_F(InstallManagerUserTest,
       CheckApplicationRegistration_InstallFailsWhenVersionValueAbsent) {
  ASSERT_SUCCEEDED(RegKey::CreateKey(kFullJobAppClientsKeyPath));

  AppData app_data(StringToGuid(kJobGuid), is_machine_);
  Job job(false, &ping_);
  job.set_app_data(app_data);

  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_DID_NOT_WRITE_CLIENT_KEY,
            CheckApplicationRegistration(CString(), &job));

  EXPECT_TRUE(RegKey::HasKey(kFullJobAppClientsKeyPath));
  EXPECT_FALSE(RegKey::HasKey(kFullJobAppClientStateKeyPath));
}

TEST_F(InstallManagerUserTest,
       CheckApplicationRegistration_InstallSucceedsWhenStateKeyAbsent) {
  ASSERT_SUCCEEDED(RegKey::SetValue(kFullJobAppClientsKeyPath,
                                    kRegValueProductVersion,
                                    _T("0.9.68.4")));

  AppData app_data(StringToGuid(kJobGuid), is_machine_);
  Job job(false, &ping_);
  job.set_app_data(app_data);

  EXPECT_SUCCEEDED(CheckApplicationRegistration(CString(), &job));
}

TEST_F(InstallManagerUserTest,
       CheckApplicationRegistration_InstallSucceedsWhenStateKeyPresent) {
  const TCHAR* keys_to_create[] = {kFullJobAppClientsKeyPath,
                                   kFullJobAppClientStateKeyPath};
  ASSERT_SUCCEEDED(RegKey::CreateKeys(keys_to_create, 2));
  ASSERT_TRUE(RegKey::HasKey(kFullJobAppClientsKeyPath));
  ASSERT_TRUE(RegKey::HasKey(kFullJobAppClientStateKeyPath));
  ASSERT_SUCCEEDED(RegKey::SetValue(kFullJobAppClientsKeyPath,
                                    kRegValueProductVersion,
                                    _T("0.9.70.0")));

  AppData app_data(StringToGuid(kJobGuid), is_machine_);
  Job job(false, &ping_);
  job.set_app_data(app_data);

  // The install should succeed even if the version is the same.
  EXPECT_SUCCEEDED(CheckApplicationRegistration(_T("0.9.70.0"), &job));
}

TEST_F(InstallManagerUserTest, CheckApplicationRegistration_UpdateSucceeds) {
  const TCHAR* keys_to_create[] = {kFullJobAppClientsKeyPath,
                                   kFullJobAppClientStateKeyPath};
  ASSERT_SUCCEEDED(RegKey::CreateKeys(keys_to_create, 2));
  ASSERT_TRUE(RegKey::HasKey(kFullJobAppClientsKeyPath));
  ASSERT_TRUE(RegKey::HasKey(kFullJobAppClientStateKeyPath));
  ASSERT_SUCCEEDED(RegKey::SetValue(kFullJobAppClientsKeyPath,
                                    kRegValueProductVersion,
                                    _T("0.9.70.0")));

  AppData app_data(StringToGuid(kJobGuid), is_machine_);
  Job job(true, &ping_);
  job.set_is_background(true);
  job.set_app_data(app_data);

  EXPECT_SUCCEEDED(CheckApplicationRegistration(_T("0.9.70.1"), &job));
}

TEST_F(InstallManagerUserTest,
       CheckApplicationRegistration_UpdateFailsWhenVersionDoesNotChange) {
  const TCHAR* keys_to_create[] = {kFullJobAppClientsKeyPath,
                                   kFullJobAppClientStateKeyPath};
  ASSERT_SUCCEEDED(RegKey::CreateKeys(keys_to_create,
                                      arraysize(keys_to_create)));
  ASSERT_TRUE(RegKey::HasKey(kFullJobAppClientsKeyPath));
  ASSERT_TRUE(RegKey::HasKey(kFullJobAppClientStateKeyPath));
  ASSERT_SUCCEEDED(RegKey::SetValue(kFullJobAppClientsKeyPath,
                                    kRegValueProductVersion,
                                    _T("0.9.70.0")));

  AppData app_data(StringToGuid(kJobGuid), is_machine_);
  Job job(true, &ping_);
  job.set_is_background(true);
  job.set_app_data(app_data);

  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_DID_NOT_CHANGE_VERSION,
            CheckApplicationRegistration(_T("0.9.70.0"), &job));
}

//
// Negative Tests
//

TEST_F(InstallManagerUserTest, InstallJob_InstallerWithoutFilenameExtension) {
  AppData app_data(StringToGuid(kJobGuid), is_machine_);
  Job job(false, &ping_);
  job.set_download_file_name(_T("foo"));
  job.set_app_data(app_data);

  EXPECT_EQ(GOOPDATEINSTALL_E_FILENAME_INVALID,
            install_manager_->InstallJob(&job));

  const CompletionInfo completion_info = install_manager_->error_info();
  EXPECT_EQ(COMPLETION_ERROR, completion_info.status);
  EXPECT_EQ(GOOPDATEINSTALL_E_FILENAME_INVALID, completion_info.error_code);
  EXPECT_STREQ(_T("The installer filename foo is invalid or unsupported."),
               completion_info.text);
}

TEST_F(InstallManagerUserTest,
       InstallJob_UnsupportedInstallerFilenameExtension) {
  AppData app_data(StringToGuid(kJobGuid), is_machine_);
  Job job(false, &ping_);
  job.set_download_file_name(_T("foo.bar"));
  job.set_app_data(app_data);

  EXPECT_EQ(GOOPDATEINSTALL_E_FILENAME_INVALID,
            install_manager_->InstallJob(&job));

  const CompletionInfo completion_info = install_manager_->error_info();
  EXPECT_EQ(COMPLETION_ERROR, completion_info.status);
  EXPECT_EQ(GOOPDATEINSTALL_E_FILENAME_INVALID, completion_info.error_code);
  EXPECT_STREQ(_T("The installer filename foo.bar is invalid or unsupported."),
               completion_info.text);
}

TEST_F(InstallManagerUserTest, InstallJob_InstallerEmtpyFilename) {
  AppData app_data(StringToGuid(kJobGuid), is_machine_);
  Job job(false, &ping_);
  job.set_app_data(app_data);

  EXPECT_EQ(GOOPDATEINSTALL_E_FILENAME_INVALID,
            install_manager_->InstallJob(&job));

  const CompletionInfo completion_info = install_manager_->error_info();
  EXPECT_EQ(COMPLETION_ERROR, completion_info.status);
  EXPECT_EQ(GOOPDATEINSTALL_E_FILENAME_INVALID, completion_info.error_code);
  EXPECT_STREQ(_T("The installer filename  is invalid or unsupported."),
               completion_info.text);
}

TEST_F(InstallManagerUserTest, InstallJob_ExeFileDoesNotExist) {
  AppData app_data(StringToGuid(kJobGuid), is_machine_);
  Job job(false, &ping_);
  job.set_download_file_name(_T("foo.exe"));
  job.set_app_data(app_data);

  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_FAILED_START,
            install_manager_->InstallJob(&job));

  const CompletionInfo completion_info = install_manager_->error_info();
  EXPECT_EQ(COMPLETION_ERROR, completion_info.status);
  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_FAILED_START,
            completion_info.error_code);
  EXPECT_STREQ(_T("The installer failed to start."), completion_info.text);

  EXPECT_FALSE(RegKey::HasKey(kFullJobAppClientsKeyPath));
  EXPECT_FALSE(RegKey::HasKey(kFullJobAppClientStateKeyPath));
}

//
// MSI Installer Fails Tests
//

TEST_F(InstallManagerUserTest, InstallJob_MsiFileDoesNotExist) {
  const TCHAR kLogFileName[] = _T("foo.msi.log");
  const TCHAR kExpectedErrorStringPartA[] =
      _T("The installer encountered error 1619: ");
  const int kExpectedErrorStringPartALength =
      arraysize(kExpectedErrorStringPartA) - 1;

  ASSERT_SUCCEEDED(File::Remove(kLogFileName));
  ASSERT_FALSE(File::Exists(kLogFileName));

  AppData app_data(StringToGuid(kJobGuid), is_machine_);
  Job job(false, &ping_);
  job.set_download_file_name(_T("foo.msi"));
  job.set_app_data(app_data);

  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_FAILED,
            install_manager_->InstallJob(&job));

  const CompletionInfo completion_info = install_manager_->error_info();
  EXPECT_EQ(COMPLETION_INSTALLER_ERROR_MSI, completion_info.status);
  EXPECT_EQ(ERROR_INSTALL_PACKAGE_OPEN_FAILED, completion_info.error_code);
  EXPECT_STREQ(kExpectedErrorStringPartA,
               completion_info.text.Left(kExpectedErrorStringPartALength));
  VerifyStringIsMsiPackageOpenFailedString(
      completion_info.text.Mid(kExpectedErrorStringPartALength));

  // msiexec creates an empty log file.
  EXPECT_TRUE(File::Exists(kLogFileName));
  EXPECT_SUCCEEDED(File::Remove(kLogFileName));
}

//
// EXE Installer Tests
//

// This test uses cmd.exe as an installer that leaves the payload
// kPayloadFileName.
TEST_F(InstallManagerUserTest, InstallJob_ExeInstallerWithArgumentsSucceeds) {
  const TCHAR kPayloadFileName[] = _T("exe_payload.txt");
  const TCHAR kCommandToExecute[] = _T("echo \"hi\" > %s");

  CString full_command_to_execute;
  full_command_to_execute.Format(kCommandToExecute, kPayloadFileName);
  CString arguments;
  arguments.Format(kExecuteCommandAndTerminateSwitch, full_command_to_execute);

  ASSERT_SUCCEEDED(File::Remove(kPayloadFileName));
  ASSERT_FALSE(File::Exists(kPayloadFileName));

  // Create the Clients key since this isn't an actual installer.
  ASSERT_SUCCEEDED(RegKey::CreateKey(kFullJobAppClientsKeyPath));
  ASSERT_TRUE(RegKey::HasKey(kFullJobAppClientsKeyPath));
  ASSERT_SUCCEEDED(RegKey::SetValue(kFullJobAppClientsKeyPath,
                                    kRegValueProductVersion,
                                    _T("0.10.69.5")));

  AppData app_data(StringToGuid(kJobGuid), is_machine_);
  app_data.set_display_name(_T("Exe App"));
  Job job(false, &ping_);
  job.set_download_file_name(kJobExecutable);
  job.set_app_data(app_data);
  SetUpdateResponseDataArguments(arguments, &job);

  EXPECT_SUCCEEDED(install_manager_->InstallJob(&job));

  const CompletionInfo completion_info = install_manager_->error_info();
  EXPECT_EQ(COMPLETION_SUCCESS, completion_info.status);
  EXPECT_EQ(S_OK, completion_info.error_code);
  EXPECT_STREQ(_T("Thanks for installing Exe App."), completion_info.text);

  EXPECT_TRUE(File::Exists(kPayloadFileName));
  EXPECT_SUCCEEDED(File::Remove(kPayloadFileName));
}

// The command we execute causes an exit code of 1 because the F and B colors
// are the same.
TEST_F(InstallManagerUserTest, InstallJob_ExeInstallerReturnsNonZeroExitCode) {
  const TCHAR kCommandToExecute[] = _T("color 00");

  CString arguments;
  arguments.Format(kExecuteCommandAndTerminateSwitch, kCommandToExecute);

  // Create the Clients key since this isn't an actual installer.
  ASSERT_SUCCEEDED(RegKey::CreateKey(kFullJobAppClientsKeyPath));
  ASSERT_TRUE(RegKey::HasKey(kFullJobAppClientsKeyPath));
  ASSERT_SUCCEEDED(RegKey::SetValue(kFullJobAppClientsKeyPath,
                                    kRegValueProductVersion,
                                    _T("0.10.69.5")));

  AppData app_data(StringToGuid(kJobGuid), is_machine_);
  Job job(false, &ping_);
  job.set_download_file_name(kJobExecutable);
  job.set_app_data(app_data);
  SetUpdateResponseDataArguments(arguments, &job);

  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_FAILED,
            install_manager_->InstallJob(&job));

  const CompletionInfo completion_info = install_manager_->error_info();
  EXPECT_EQ(COMPLETION_INSTALLER_ERROR_OTHER, completion_info.status);
  EXPECT_EQ(1, completion_info.error_code);
  EXPECT_STREQ(_T("The installer encountered error 1."),
               completion_info.text);
}

/* TODO(omaha): Figure out a way to perform this test.
   CleanupInstallerResultRegistry clears the result values it sets.
   TODO(omaha): Add another test that reports an error in using registry API.
// Also tests that the launch cmd is set.
TEST_F(InstallManagerUserTest,
       InstallJob_ExeInstallerReturnsNonZeroExitCode_InstallerResultSuccess) {
  const TCHAR kCommandToExecute[] = _T("color 00");

  CString arguments;
  arguments.Format(kExecuteCommandAndTerminateSwitch, kCommandToExecute);

  // Create the Clients key since this isn't an actual installer.
  ASSERT_SUCCEEDED(RegKey::CreateKey(kFullJobAppClientsKeyPath));
  ASSERT_TRUE(RegKey::HasKey(kFullJobAppClientsKeyPath));
  ASSERT_SUCCEEDED(RegKey::SetValue(kFullJobAppClientsKeyPath,
                                    kRegValueProductVersion,
                                    _T("0.10.69.5")));
  SetupInstallerResultRegistry(kJobGuid,
                               true, kResultSuccess,
                               false, 0,
                               false, _T(""),
                               true, kLaunchCmdLine);

  AppData app_data(StringToGuid(kJobGuid), is_machine_);
  app_data.set_display_name(_T("color 00"));
  Job job(false, &ping_);
  job.set_download_file_name(kJobExecutable);
  job.set_app_data(app_data);
  SetUpdateResponseDataArguments(arguments, &job);

  EXPECT_SUCCEEDED(install_manager_->InstallJob(&job));

  const CompletionInfo completion_info = install_manager_->error_info();
  EXPECT_EQ(COMPLETION_SUCCESS, completion_info.status);
  EXPECT_EQ(1, completion_info.error_code);
  EXPECT_STREQ(_T("Thanks for installing color 00."), completion_info.text);
  EXPECT_STREQ(kLaunchCmdLine, job.launch_cmd_line());

  VerifyLastRegistryValues(kJobGuid,
                           true, kResultSuccess,
                           false, 0,
                           false, _T(""),
                           true, kLaunchCmdLine);
}
*/

TEST_F(InstallManagerMachineTest, InstallJob_MsiInstallerSucceeds) {
  if (!vista_util::IsUserAdmin()) {
    std::wcout << _T("\tTest did not run because the user is not an admin.")
               << std::endl;
    return;
  }
  const TCHAR expected_iid_string[] =
      _T("{BF66411E-8FAC-4E2C-920C-849DF562621C}");
  const GUID expected_iid = StringToGuid(expected_iid_string);

  // We can't fake the registry keys because we are interacting with a real
  // installer.
  RestoreRegistryHives();

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

  AppData app_data(StringToGuid(kFooGuid), is_machine_);
  app_data.set_display_name(_T("Foo"));
  app_data.set_language(_T("en"));
  app_data.set_ap(_T("test_ap"));
  app_data.set_tt_token(_T("test_tt_token"));
  app_data.set_iid(expected_iid);
  app_data.set_brand_code(_T("GOOG"));
  app_data.set_client_id(_T("_some_partner"));

  Job job(false, &ping_);
  job.set_download_file_name(installer_full_path);
  job.set_app_data(app_data);

  EXPECT_SUCCEEDED(install_manager_->InstallJob(&job));

  const CompletionInfo completion_info = install_manager_->error_info();
  EXPECT_EQ(COMPLETION_SUCCESS, completion_info.status);
  EXPECT_EQ(S_OK, completion_info.error_code);
  EXPECT_STREQ(_T("Thanks for installing Foo."), completion_info.text);

  EXPECT_TRUE(File::Exists(installer_log_full_path));

  EXPECT_TRUE(RegKey::HasKey(kFullFooAppClientKeyPath));
  EXPECT_FALSE(RegKey::HasKey(kFullFooAppClientStateKeyPath));
  // Verify the value that is written based on an MSI property we didn't
  // specify wasn't written.
  EXPECT_FALSE(RegKey::HasValue(kFullFooAppClientKeyPath,
                                kFooInstallerBarValueName));
  EXPECT_SUCCEEDED(RegKey::DeleteKey(kFullFooAppClientKeyPath));

  CString uninstall_arguments;
  uninstall_arguments.Format(kMsiUninstallArguments, installer_full_path);
  EXPECT_SUCCEEDED(System::ShellExecuteProcess(kMsiCommand,
                                               uninstall_arguments,
                                               NULL,
                                               NULL));
}

TEST_F(InstallManagerMachineTest, InstallJob_MsiInstallerWithArgumentSucceeds) {
  if (!vista_util::IsUserAdmin()) {
    std::wcout << _T("\tTest did not run because the user is not an admin.")
               << std::endl;
    return;
  }

  // We can't fake the registry keys because we are interacting with a real
  // installer.
  RestoreRegistryHives();

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

  AppData app_data(StringToGuid(kFooGuid), is_machine_);
  app_data.set_display_name(_T("Foo"));
  app_data.set_language(_T("en"));
  Job job(false, &ping_);
  job.set_download_file_name(installer_full_path);
  job.set_app_data(app_data);
  SetUpdateResponseDataArguments(kFooInstallerBarPropertyArg, &job);

  EXPECT_SUCCEEDED(install_manager_->InstallJob(&job));

  const CompletionInfo completion_info = install_manager_->error_info();

  EXPECT_EQ(COMPLETION_SUCCESS, completion_info.status);
  EXPECT_EQ(S_OK, completion_info.error_code);
  EXPECT_STREQ(_T("Thanks for installing Foo."), completion_info.text);

  EXPECT_TRUE(File::Exists(installer_log_full_path));

  EXPECT_TRUE(RegKey::HasKey(kFullFooAppClientKeyPath));
  EXPECT_FALSE(RegKey::HasKey(kFullFooAppClientStateKeyPath));
  EXPECT_TRUE(RegKey::HasValue(kFullFooAppClientKeyPath,
                               kFooInstallerBarValueName));
  DWORD barprop_value;
  EXPECT_SUCCEEDED(RegKey::GetValue(kFullFooAppClientKeyPath,
                                    kFooInstallerBarValueName,
                                    &barprop_value));
  EXPECT_EQ(7, barprop_value);

  EXPECT_SUCCEEDED(RegKey::DeleteKey(kFullFooAppClientKeyPath));

  CString uninstall_arguments;
  uninstall_arguments.Format(kMsiUninstallArguments, installer_full_path);
  EXPECT_SUCCEEDED(System::ShellExecuteProcess(kMsiCommand,
                                               uninstall_arguments,
                                               NULL,
                                               NULL));
}

// The use of kGoogleUpdateAppId is the key to this test.
// Note that the version is not changed - this is the normal self-update case.
// Among other things, this test verifies that CheckApplicationRegistration() is
// not called for self-updates.
TEST_F(InstallManagerUserTest, InstallJob_UpdateOmahaSucceeds) {
  CString arguments;
  arguments.Format(kExecuteCommandAndTerminateSwitch, _T("echo hi"));

  const CString kExistingVersion(_T("0.9.69.5"));

  AppData app_data(kGoopdateGuid, is_machine_);
  app_data.set_previous_version(kExistingVersion);
  Job job(true, &ping_);
  job.set_is_background(true);
  job.set_download_file_name(kJobExecutable);
  job.set_app_data(app_data);
  SetUpdateResponseDataArguments(arguments, &job);

  // Because we don't actually run the Omaha installer, we need to make sure
  // its Clients key and pv value exist to avoid an error.
  ASSERT_SUCCEEDED(RegKey::CreateKey(USER_REG_CLIENTS_GOOPDATE));
  ASSERT_TRUE(RegKey::HasKey(USER_REG_CLIENTS_GOOPDATE));
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                    kRegValueProductVersion,
                                    kExistingVersion));

  EXPECT_SUCCEEDED(install_manager_->InstallJob(&job));

  const CompletionInfo completion_info = install_manager_->error_info();

  EXPECT_EQ(COMPLETION_SUCCESS, completion_info.status);
  EXPECT_EQ(S_OK, completion_info.error_code);
  // The user never sees this, but it is odd we put this text in the structure.
  EXPECT_STREQ(_T("Thanks for installing ."), completion_info.text);

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
TEST_F(InstallManagerUserTest,
       InstallJob_UpdateOmahaSucceedsWhenClientsKeyAbsent) {
  CString arguments;
  arguments.Format(kExecuteCommandAndTerminateSwitch, _T("echo hi"));

  const CString kExistingVersion(_T("0.9.69.5"));

  AppData app_data(kGoopdateGuid, is_machine_);
  app_data.set_previous_version(kExistingVersion);
  Job job(true, &ping_);
  job.set_is_background(true);
  job.set_download_file_name(kJobExecutable);
  job.set_app_data(app_data);
  SetUpdateResponseDataArguments(arguments, &job);

  EXPECT_SUCCEEDED(install_manager_->InstallJob(&job));

  EXPECT_FALSE(RegKey::HasKey(USER_REG_CLIENTS_GOOPDATE));
}

TEST_F(InstallManagerUserTest, InstallJob_InstallerDoesNotWriteClientsKey) {
  CString arguments;
  arguments.Format(kExecuteCommandAndTerminateSwitch, _T("echo hi"));

  AppData app_data(StringToGuid(kJobGuid), is_machine_);
  app_data.set_display_name(_T("Some App"));
  Job job(false, &ping_);
  job.set_download_file_name(kJobExecutable);
  job.set_app_data(app_data);
  SetUpdateResponseDataArguments(arguments, &job);

  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_DID_NOT_WRITE_CLIENT_KEY,
            install_manager_->InstallJob(&job));

  EXPECT_FALSE(RegKey::HasKey(kFullJobAppClientsKeyPath));
  EXPECT_FALSE(RegKey::HasKey(kFullJobAppClientStateKeyPath));

  const CompletionInfo completion_info = install_manager_->error_info();
  EXPECT_EQ(COMPLETION_ERROR, completion_info.status);
  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_DID_NOT_WRITE_CLIENT_KEY,
            completion_info.error_code);
  EXPECT_STREQ(
      _T("Installation failed. Please try again. Error code = 0x80040905"),
      completion_info.text);}

TEST_F(InstallManagerUserTest, InstallJob_InstallerFailureMsiFileDoesNotExist) {
  const TCHAR kLogFileName[] = _T("foo.msi.log");
  const TCHAR kExpectedErrorStringPartA[] =
      _T("The installer encountered error 1619: ");
  const int kExpectedErrorStringPartALength =
      arraysize(kExpectedErrorStringPartA) - 1;

  ASSERT_SUCCEEDED(File::Remove(kLogFileName));
  ASSERT_FALSE(File::Exists(kLogFileName));

  AppData app_data(StringToGuid(kJobGuid), is_machine_);
  Job job(false, &ping_);
  job.set_download_file_name(_T("foo.msi"));
  job.set_app_data(app_data);

  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_FAILED,
            install_manager_->InstallJob(&job));

  const CompletionInfo completion_info = install_manager_->error_info();

  EXPECT_EQ(COMPLETION_INSTALLER_ERROR_MSI, completion_info.status);
  EXPECT_EQ(ERROR_INSTALL_PACKAGE_OPEN_FAILED, completion_info.error_code);
  EXPECT_STREQ(kExpectedErrorStringPartA,
            completion_info.text.Left(kExpectedErrorStringPartALength));
  VerifyStringIsMsiPackageOpenFailedString(
      completion_info.text.Mid(kExpectedErrorStringPartALength));

  // msiexec creates an empty log file.
  EXPECT_TRUE(File::Exists(kLogFileName));
  EXPECT_SUCCEEDED(File::Remove(kLogFileName));

  EXPECT_FALSE(RegKey::HasKey(kFullJobAppClientsKeyPath));
  EXPECT_FALSE(RegKey::HasKey(kFullJobAppClientStateKeyPath));
}

//
// GetInstallerResultHelper tests
//

TEST_F(InstallManagerUserGetInstallerResultHelperTest,
       GetInstallerResultHelper_NoRegistry_MSI_ZeroExitCode) {
  GetInstallerResultHelper(kJobGuid, kMsiInstaller, 0, &completion_info_);

  EXPECT_EQ(COMPLETION_SUCCESS, completion_info_.status);
  EXPECT_EQ(0, completion_info_.error_code);
  EXPECT_TRUE(completion_info_.text.IsEmpty());
  EXPECT_TRUE(job_->launch_cmd_line().IsEmpty());

  VerifyNoLastRegistryValues(kJobGuid);
}

TEST_F(InstallManagerUserGetInstallerResultHelperTest,
       GetInstallerResultHelper_NoRegistry_MSI_NonZeroExitCode) {
  GetInstallerResultHelper(kJobGuid, kMsiInstaller, 1603, &completion_info_);

  EXPECT_EQ(COMPLETION_INSTALLER_ERROR_MSI, completion_info_.status);
  EXPECT_EQ(1603, completion_info_.error_code);
  EXPECT_STREQ(kError1603Text, completion_info_.text);
  EXPECT_TRUE(job_->launch_cmd_line().IsEmpty());

  VerifyNoLastRegistryValues(kJobGuid);
}

TEST_F(InstallManagerUserGetInstallerResultHelperTest,
       GetInstallerResultHelper_NoRegistry_EXE_ZeroExitCode) {
  GetInstallerResultHelper(kJobGuid, kOtherInstaller, 0, &completion_info_);

  EXPECT_EQ(COMPLETION_SUCCESS, completion_info_.status);
  EXPECT_EQ(0, completion_info_.error_code);
  EXPECT_TRUE(completion_info_.text.IsEmpty());
  EXPECT_TRUE(job_->launch_cmd_line().IsEmpty());

  VerifyNoLastRegistryValues(kJobGuid);
}

TEST_F(InstallManagerUserGetInstallerResultHelperTest,
       GetInstallerResultHelper_NoRegistry_EXE_NonZeroExitCode_SmallNumber) {
  GetInstallerResultHelper(kJobGuid, kOtherInstaller, 8, &completion_info_);

  EXPECT_EQ(COMPLETION_INSTALLER_ERROR_OTHER, completion_info_.status);
  EXPECT_EQ(8, completion_info_.error_code);
  EXPECT_STREQ(_T("The installer encountered error 8."), completion_info_.text);
  EXPECT_TRUE(job_->launch_cmd_line().IsEmpty());

  VerifyNoLastRegistryValues(kJobGuid);
}

TEST_F(InstallManagerUserGetInstallerResultHelperTest,
       GetInstallerResultHelper_NoRegistry_EXE_NonZeroExitCode_HRESULTFailure) {
  GetInstallerResultHelper(
      kJobGuid, kOtherInstaller, 0x80004005, &completion_info_);

  EXPECT_EQ(COMPLETION_INSTALLER_ERROR_OTHER, completion_info_.status);
  EXPECT_EQ(0x80004005, completion_info_.error_code);
  EXPECT_STREQ(_T("The installer encountered error 0x80004005."),
               completion_info_.text);
  EXPECT_TRUE(job_->launch_cmd_line().IsEmpty());

  VerifyNoLastRegistryValues(kJobGuid);
}

TEST_F(InstallManagerUserGetInstallerResultHelperTest,
       GetInstallerResultHelper_ExitCode_MSI) {
  SetupInstallerResultRegistry(kJobGuid,
                               true, kResultExitCode,
                               false, 0,
                               false, _T(""),
                               false, _T(""));

  GetInstallerResultHelper(kJobGuid, kMsiInstaller, 1603, &completion_info_);

  EXPECT_EQ(COMPLETION_INSTALLER_ERROR_MSI, completion_info_.status);
  EXPECT_EQ(1603, completion_info_.error_code);
  EXPECT_STREQ(kError1603Text, completion_info_.text);
  EXPECT_TRUE(job_->launch_cmd_line().IsEmpty());

  VerifyLastRegistryValues(kJobGuid,
                           true, kResultExitCode,
                           false, 0,
                           false, _T(""),
                           false, _T(""));
}

TEST_F(InstallManagerUserGetInstallerResultHelperTest,
       GetInstallerResultHelper_NoRegistry_MSI_RebootRequired) {
  GetInstallerResultHelper(kJobGuid,
                           kMsiInstaller,
                           ERROR_SUCCESS_REBOOT_REQUIRED,
                           &completion_info_);

  EXPECT_EQ(COMPLETION_SUCCESS_REBOOT_REQUIRED, completion_info_.status);
  EXPECT_EQ(0, completion_info_.error_code);
  EXPECT_TRUE(completion_info_.text.IsEmpty());
  EXPECT_TRUE(job_->launch_cmd_line().IsEmpty());

  VerifyNoLastRegistryValues(kJobGuid);
}

TEST_F(InstallManagerUserGetInstallerResultHelperTest,
       GetInstallerResultHelper_SystemError_EXE_RebootRequired) {
  SetupInstallerResultRegistry(kJobGuid,
                               true, kResultFailedSystemError,
                               true, ERROR_SUCCESS_REBOOT_REQUIRED,
                               false, _T(""),
                               true, kLaunchCmdLine);

  GetInstallerResultHelper(kJobGuid, kOtherInstaller, 1, &completion_info_);

  EXPECT_EQ(COMPLETION_SUCCESS_REBOOT_REQUIRED, completion_info_.status);
  EXPECT_EQ(0, completion_info_.error_code);
  EXPECT_TRUE(completion_info_.text.IsEmpty());
  EXPECT_TRUE(job_->launch_cmd_line().IsEmpty()) <<
      _T("Command line is not supported with reboot.");

  VerifyLastRegistryValues(kJobGuid,
                           true, kResultFailedSystemError,
                           true, ERROR_SUCCESS_REBOOT_REQUIRED,
                           false, _T(""),
                           true, kLaunchCmdLine);
}

TEST_F(InstallManagerUserGetInstallerResultHelperTest,
       GetInstallerResultHelper_Success_NoErrorCode) {
  SetupInstallerResultRegistry(kJobGuid,
                               true, kResultSuccess,
                               false, 0,
                               false, _T(""),
                               false, _T(""));

  GetInstallerResultHelper(kJobGuid, kOtherInstaller, 99, &completion_info_);

  EXPECT_EQ(COMPLETION_SUCCESS, completion_info_.status);
  EXPECT_EQ(99, completion_info_.error_code);
  EXPECT_TRUE(completion_info_.text.IsEmpty());
  EXPECT_TRUE(job_->launch_cmd_line().IsEmpty());

  VerifyLastRegistryValues(kJobGuid,
                           true, kResultSuccess,
                           false, 0,
                           false, _T(""),
                           false, _T(""));
}

TEST_F(InstallManagerUserGetInstallerResultHelperTest,
       GetInstallerResultHelper_Success_AllValues) {
  SetupInstallerResultRegistry(kJobGuid,
                               true, kResultSuccess,
                               true, 555,
                               true, _T("an ignored error"),
                               true, kLaunchCmdLine);

  GetInstallerResultHelper(kJobGuid, kOtherInstaller, 99, &completion_info_);

  EXPECT_EQ(COMPLETION_SUCCESS, completion_info_.status);
  EXPECT_EQ(555, completion_info_.error_code) <<
      _T("InstallerError overwrites exit code.");
  EXPECT_TRUE(completion_info_.text.IsEmpty()) <<
      _T("UIString is ignored for Success.");
  EXPECT_STREQ(kLaunchCmdLine, job_->launch_cmd_line());

  VerifyLastRegistryValues(kJobGuid,
                           true, kResultSuccess,
                           true, 555,
                           true, _T("an ignored error"),
                           true, kLaunchCmdLine);
}

TEST_F(InstallManagerUserGetInstallerResultHelperTest,
       GetInstallerResultHelper_LaunchCmdOnly_MSI_ZeroExitCode) {
  SetupInstallerResultRegistry(kJobGuid,
                               false, 0,
                               false, 0,
                               false, _T(""),
                               true, kLaunchCmdLine);

  GetInstallerResultHelper(kJobGuid, kMsiInstaller, 0, &completion_info_);

  EXPECT_EQ(COMPLETION_SUCCESS, completion_info_.status);
  EXPECT_EQ(0, completion_info_.error_code);
  EXPECT_TRUE(completion_info_.text.IsEmpty());
  EXPECT_STREQ(kLaunchCmdLine, job_->launch_cmd_line());

  VerifyLastRegistryValues(kJobGuid,
                           false, 0,
                           false, 0,
                           false, _T(""),
                           true, kLaunchCmdLine);
}

// Exit code is used when no error code is present. It's interpreted as a system
// error even though the installer is not an MSI.
TEST_F(InstallManagerUserGetInstallerResultHelperTest,
       GetInstallerResultHelper_Failed_NoErrorCodeOrUiString) {
  SetupInstallerResultRegistry(kJobGuid,
                               true, kResultFailedCustomError,
                               false, 0,
                               false, _T(""),
                               false, _T(""));

  GetInstallerResultHelper(kJobGuid, kOtherInstaller, 8, &completion_info_);

  EXPECT_EQ(COMPLETION_INSTALLER_ERROR_OTHER, completion_info_.status);
  EXPECT_EQ(8, completion_info_.error_code);
  EXPECT_STREQ(_T("The installer encountered error 8."), completion_info_.text);
  EXPECT_TRUE(job_->launch_cmd_line().IsEmpty());

  VerifyLastRegistryValues(kJobGuid,
                           true, kResultFailedCustomError,
                           false, 0,
                           false, _T(""),
                           false, _T(""));
}

TEST_F(InstallManagerUserGetInstallerResultHelperTest,
       GetInstallerResultHelper_Failed_WithErrorCode) {
  SetupInstallerResultRegistry(kJobGuid,
                               true, kResultFailedCustomError,
                               true, 8,
                               false, _T(""),
                               false, _T(""));

  GetInstallerResultHelper(kJobGuid, kOtherInstaller, 1618, &completion_info_);

  EXPECT_EQ(COMPLETION_INSTALLER_ERROR_OTHER, completion_info_.status);
  EXPECT_EQ(8, completion_info_.error_code);
  EXPECT_STREQ(_T("The installer encountered error 8."), completion_info_.text);
  EXPECT_TRUE(job_->launch_cmd_line().IsEmpty());

  VerifyLastRegistryValues(kJobGuid,
                           true, kResultFailedCustomError,
                           true, 8,
                           false, _T(""),
                           false, _T(""));
}

TEST_F(InstallManagerUserGetInstallerResultHelperTest,
       GetInstallerResultHelper_Failed_AllValues) {
  const DWORD kInstallerErrorValue = 8;
  const TCHAR* const kUiString = _T("a message from the installer");

  SetupInstallerResultRegistry(kJobGuid,
                               true, kResultFailedCustomError,
                               true, kInstallerErrorValue,
                               true, kUiString,
                               true, kLaunchCmdLine);

  GetInstallerResultHelper(kJobGuid, kOtherInstaller, 1618, &completion_info_);

  EXPECT_EQ(COMPLETION_INSTALLER_ERROR_OTHER, completion_info_.status);
  EXPECT_EQ(kInstallerErrorValue, completion_info_.error_code);
  EXPECT_STREQ(kUiString, completion_info_.text);
  EXPECT_TRUE(job_->launch_cmd_line().IsEmpty()) <<
      _T("Command line is not supported with errors.");

  VerifyLastRegistryValues(kJobGuid,
                           true, kResultFailedCustomError,
                           true, kInstallerErrorValue,
                           true, kUiString,
                           true, kLaunchCmdLine);
}

// Exit code is used and interpreted as MSI error when no error code present.
TEST_F(InstallManagerUserGetInstallerResultHelperTest,
       GetInstallerResultHelper_FailedMsiError_NoErrorCode) {
  SetupInstallerResultRegistry(kJobGuid,
                               true, kResultFailedMsiError,
                               false, 0,
                               false, _T(""),
                               false, _T(""));

  GetInstallerResultHelper(kJobGuid, kOtherInstaller, 1603, &completion_info_);

  EXPECT_EQ(COMPLETION_INSTALLER_ERROR_MSI, completion_info_.status);
  EXPECT_EQ(1603, completion_info_.error_code);
  EXPECT_STREQ(kError1603Text, completion_info_.text);
  EXPECT_TRUE(job_->launch_cmd_line().IsEmpty());

  VerifyLastRegistryValues(kJobGuid,
                           true, kResultFailedMsiError,
                           false, 0,
                           false, _T(""),
                           false, _T(""));
}

TEST_F(InstallManagerUserGetInstallerResultHelperTest,
       GetInstallerResultHelper_FailedMsiError_WithErrorCode) {
  const DWORD kInstallerErrorValue = 1603;

  SetupInstallerResultRegistry(kJobGuid,
                               true, kResultFailedMsiError,
                               true, kInstallerErrorValue,
                               false, _T(""),
                               false, _T(""));

  GetInstallerResultHelper(kJobGuid, kOtherInstaller, 1618, &completion_info_);

  EXPECT_EQ(COMPLETION_INSTALLER_ERROR_MSI, completion_info_.status);
  EXPECT_EQ(kInstallerErrorValue, completion_info_.error_code);
  EXPECT_STREQ(kError1603Text, completion_info_.text);
  EXPECT_TRUE(job_->launch_cmd_line().IsEmpty());

  VerifyLastRegistryValues(kJobGuid,
                           true, kResultFailedMsiError,
                           true, kInstallerErrorValue,
                           false, _T(""),
                           false, _T(""));
}

TEST_F(InstallManagerUserGetInstallerResultHelperTest,
       GetInstallerResultHelper_FailedMsiError_AllValues) {
  const DWORD kInstallerErrorValue = 1603;

  SetupInstallerResultRegistry(kJobGuid,
                               true, kResultFailedMsiError,
                               true, kInstallerErrorValue,
                               true, _T("an ignored error"),
                               true, kLaunchCmdLine);

  GetInstallerResultHelper(kJobGuid, kOtherInstaller, 1618, &completion_info_);

  EXPECT_EQ(COMPLETION_INSTALLER_ERROR_MSI, completion_info_.status);
  EXPECT_EQ(kInstallerErrorValue, completion_info_.error_code);
  EXPECT_STREQ(kError1603Text, completion_info_.text) <<
      _T("UIString is ignored.");
  EXPECT_TRUE(job_->launch_cmd_line().IsEmpty()) <<
      _T("Command line is not supported with errors.");

  VerifyLastRegistryValues(kJobGuid,
                           true, kResultFailedMsiError,
                           true, kInstallerErrorValue,
                           true, _T("an ignored error"),
                           true, kLaunchCmdLine);
}

// Exit code is used and interpreted as system error when no error code present.
TEST_F(InstallManagerUserGetInstallerResultHelperTest,
       GetInstallerResultHelper_FailedSystemError_NoErrorCode) {
  SetupInstallerResultRegistry(kJobGuid,
                               true, kResultFailedSystemError,
                               false, 0,
                               false, _T(""),
                               false, _T(""));

  GetInstallerResultHelper(kJobGuid, kOtherInstaller, 1603, &completion_info_);

  EXPECT_EQ(COMPLETION_INSTALLER_ERROR_SYSTEM, completion_info_.status);
  EXPECT_EQ(1603, completion_info_.error_code);
  EXPECT_STREQ(kError1603Text, completion_info_.text);
  EXPECT_TRUE(job_->launch_cmd_line().IsEmpty());

  VerifyLastRegistryValues(kJobGuid,
                           true, kResultFailedSystemError,
                           false, 0,
                           false, _T(""),
                           false, _T(""));
}

TEST_F(InstallManagerUserGetInstallerResultHelperTest,
       GetInstallerResultHelper_FailedSystemError_WithErrorCode) {
  const DWORD kInstallerErrorValue = 1603;

  SetupInstallerResultRegistry(kJobGuid,
                               true, kResultFailedSystemError,
                               true, kInstallerErrorValue,
                               false, _T(""),
                               false, _T(""));

  GetInstallerResultHelper(kJobGuid, kOtherInstaller, 1618, &completion_info_);

  EXPECT_EQ(COMPLETION_INSTALLER_ERROR_SYSTEM, completion_info_.status);
  EXPECT_EQ(kInstallerErrorValue, completion_info_.error_code);
  EXPECT_STREQ(kError1603Text, completion_info_.text);
  EXPECT_TRUE(job_->launch_cmd_line().IsEmpty());

  VerifyLastRegistryValues(kJobGuid,
                           true, kResultFailedSystemError,
                           true, kInstallerErrorValue,
                           false, _T(""),
                           false, _T(""));
}

// INSTALLER_RESULT_FAILED_SYSTEM_ERROR supports values beyond the basic
// "System Error Codes" and their HRESULT equivalents.
TEST_F(InstallManagerUserGetInstallerResultHelperTest,
       GetInstallerResultHelper_FailedSystemError_WithHRESULTSystemError) {
  const DWORD kInstallerErrorValue = 0x800B010F;

  SetupInstallerResultRegistry(kJobGuid,
                               true, kResultFailedSystemError,
                               true, kInstallerErrorValue,
                               false, _T(""),
                               false, _T(""));

  GetInstallerResultHelper(kJobGuid, kOtherInstaller, 1618, &completion_info_);

  EXPECT_EQ(COMPLETION_INSTALLER_ERROR_SYSTEM, completion_info_.status);
  EXPECT_EQ(kInstallerErrorValue, completion_info_.error_code);
  EXPECT_STREQ(kError0x800B010FText, completion_info_.text);
  EXPECT_TRUE(job_->launch_cmd_line().IsEmpty());

  VerifyLastRegistryValues(kJobGuid,
                           true, kResultFailedSystemError,
                           true, kInstallerErrorValue,
                           false, _T(""),
                           false, _T(""));
}

TEST_F(InstallManagerUserGetInstallerResultHelperTest,
       GetInstallerResultHelper_FailedSystemError_WithUnrecognizedError) {
  const DWORD kInstallerErrorValue = 0x80040200;

  SetupInstallerResultRegistry(kJobGuid,
                               true, kResultFailedSystemError,
                               true, kInstallerErrorValue,
                               false, _T(""),
                               false, _T(""));

  // GetMessageForSystemErrorCode expects a valid system error.
  ExpectAsserts expect_asserts;

  GetInstallerResultHelper(kJobGuid, kOtherInstaller, 1618, &completion_info_);

  EXPECT_EQ(COMPLETION_INSTALLER_ERROR_SYSTEM, completion_info_.status);
  EXPECT_EQ(kInstallerErrorValue, completion_info_.error_code);
  EXPECT_STREQ(_T("The installer encountered error 0x80040200."),
               completion_info_.text);
  EXPECT_TRUE(job_->launch_cmd_line().IsEmpty());

  VerifyLastRegistryValues(kJobGuid,
                           true, kResultFailedSystemError,
                           true, kInstallerErrorValue,
                           false, _T(""),
                           false, _T(""));
}

TEST_F(InstallManagerUserGetInstallerResultHelperTest,
       GetInstallerResultHelper_FailedSystemError_AllValues) {
  const DWORD kInstallerErrorValue = 1603;

  SetupInstallerResultRegistry(kJobGuid,
                               true, kResultFailedSystemError,
                               true, kInstallerErrorValue,
                               true, _T("an ignored error"),
                               true, kLaunchCmdLine);

  GetInstallerResultHelper(kJobGuid, kOtherInstaller, 1618, &completion_info_);

  EXPECT_EQ(COMPLETION_INSTALLER_ERROR_SYSTEM, completion_info_.status);
  EXPECT_EQ(kInstallerErrorValue, completion_info_.error_code);
  EXPECT_STREQ(kError1603Text, completion_info_.text) <<
      _T("UIString is ignored.");
  EXPECT_TRUE(job_->launch_cmd_line().IsEmpty()) <<
      _T("Command line is not supported with errors.");

  VerifyLastRegistryValues(kJobGuid,
                           true, kResultFailedSystemError,
                           true, kInstallerErrorValue,
                           true, _T("an ignored error"),
                           true, kLaunchCmdLine);
}

// TODO(omaha): Add a machine test.

}  // namespace omaha

