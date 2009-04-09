// Copyright 2008-2009 Google Inc.
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

#include "omaha/common/app_util.h"
#include "omaha/common/error.h"
#include "omaha/common/file.h"
#include "omaha/common/path.h"
#include "omaha/common/scoped_ptr_address.h"
#include "omaha/common/system.h"
#include "omaha/common/utils.h"
#include "omaha/common/vistautil.h"
#include "omaha/goopdate/config_manager.h"
#include "omaha/goopdate/const_goopdate.h"
#include "omaha/goopdate/goopdate_utils.h"
#include "omaha/goopdate/goopdate_xml_parser.h"
#include "omaha/testing/unit_test.h"
#include "omaha/worker/application_data.h"
#include "omaha/worker/application_manager.h"
#include "omaha/worker/job.h"
#include "omaha/worker/ping.h"

namespace omaha {

namespace {

void CreateTestAppData(AppData* app_data) {
  ASSERT_TRUE(app_data != NULL);
  app_data->set_version(_T("1.1.1.3"));
  app_data->set_previous_version(_T("1.0.0.0"));
  app_data->set_language(_T("abc"));
  app_data->set_ap(_T("Test ap"));
  app_data->set_tt_token(_T("Test TT Token"));
  app_data->set_iid(StringToGuid(_T("{F723495F-8ACF-4746-824d-643741C797B5}")));
  app_data->set_brand_code(_T("GOOG"));
  app_data->set_client_id(_T("someclient"));
  app_data->set_did_run(AppData::ACTIVE_RUN);
  app_data->set_install_source(_T("twoclick"));
}

const TCHAR kFooGuid[] = _T("{D6B08267-B440-4C85-9F79-E195E80D9937}");
const TCHAR kFullFooAppClientKeyPath[] =
    _T("HKLM\\Software\\Google\\Update\\Clients\\")
    _T("{D6B08267-B440-4C85-9F79-E195E80D9937}");
const TCHAR kFullFooAppClientStateKeyPath[] =
    _T("HKLM\\Software\\Google\\Update\\ClientState\\")
    _T("{D6B08267-B440-4C85-9F79-E195E80D9937}");
const TCHAR kSetupFooV1RelativeLocation[] =
    _T("unittest_support\\test_foo_v1.0.101.0.msi");

const TCHAR kMsiLogFormat[] = _T("%s.log");
const TCHAR kMsiUninstallArguments[] = _T("/quiet /uninstall %s");
const TCHAR kMsiCommand[] = _T("msiexec");

const TCHAR kJobExecutable[] = _T("cmd.exe");
const TCHAR kExecuteCommandAndTerminateSwitch[] = _T("/c %s");

const TCHAR expected_iid_string[] =
    _T("{BF66411E-8FAC-4E2C-920C-849DF562621C}");

}  // namespace

class JobTest : public testing::Test {
 protected:
  JobTest() : is_machine_(true) {}

  void SetUp() {
    // Default to an auto-update job.
    job_.reset(new Job(true, &ping_));
    job_->is_background_ = true;
  }

  void set_info(const CompletionInfo& info) {
    job_->info_ = info;
  }

  void set_job_state(JobState job_state) {
    job_->job_state_ = job_state;
  }

  void SetIsInstallJob() {
    job_->is_update_ = false;
    job_->is_background_ = false;
  }

  HRESULT SendStateChangePing(JobState previous_state) {
    return job_->SendStateChangePing(previous_state);
  }

  void SetUpdateResponseDataArguments(const CString& arguments) {
    job_->update_response_data_.set_arguments(arguments);
  }

  void SetAppData(const AppData& app_data) {
    job_->set_app_data(app_data);
  }

  void set_download_file_name(const CString& download_file) {
    job_->download_file_name_ = download_file;
  }

  HRESULT UpdateRegistry(AppData* data) {
    return job_->UpdateRegistry(data);
  }

  HRESULT UpdateJob() {
    return job_->UpdateJob();
  }

  const AppData& get_app_data() {
    return job_->app_data();
  }

  HRESULT DeleteJobDownloadDirectory() const {
    return job_->DeleteJobDownloadDirectory();
  }

  scoped_ptr<Job> job_;
  Ping ping_;
  bool is_machine_;
};

// Does not override registry hives because it would not affect the installer.
class JobInstallFooTest : public JobTest {
 protected:
  virtual void SetUp() {
    JobTest::SetUp();

    foo_installer_path_ = ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                                          kSetupFooV1RelativeLocation);
    ASSERT_TRUE(File::Exists(foo_installer_path_));

    foo_installer_log_path_.Format(kMsiLogFormat, foo_installer_path_);

    ASSERT_HRESULT_SUCCEEDED(File::Remove(foo_installer_log_path_));
    ASSERT_FALSE(File::Exists(foo_installer_log_path_));

    RegKey::DeleteKey(kFullFooAppClientKeyPath);
    ASSERT_FALSE(RegKey::HasKey(kFullFooAppClientKeyPath));
    RegKey::DeleteKey(kFullFooAppClientStateKeyPath);
    ASSERT_FALSE(RegKey::HasKey(kFullFooAppClientStateKeyPath));
  }

  virtual void TearDown() {
    RegKey::DeleteKey(kFullFooAppClientKeyPath);
    RegKey::DeleteKey(kFullFooAppClientStateKeyPath);
  }

  AppData PopulateFooAppData() {
    AppData app_data(StringToGuid(kFooGuid), is_machine_);
    app_data.set_display_name(_T("Foo"));
    app_data.set_language(_T("en"));
    app_data.set_ap(_T("test_ap"));
    app_data.set_iid(StringToGuid(expected_iid_string));
    app_data.set_brand_code(_T("GOOG"));
    app_data.set_client_id(_T("_some_partner"));
    app_data.set_browser_type(BROWSER_IE);
    app_data.set_usage_stats_enable(TRISTATE_TRUE);
    return app_data;
  }

  // Verifies the values that are written to ClientState before installing.
  // Assumes the is Foo.
  void VerifyFooClientStateValuesWrittenBeforeInstall(bool is_first_install) {
    CString str_value;
    DWORD value;

    if (is_first_install) {
      EXPECT_HRESULT_SUCCEEDED(RegKey::GetValue(kFullFooAppClientStateKeyPath,
                                                kRegValueBrandCode,
                                                &str_value));
      EXPECT_STREQ(_T("GOOG"), str_value);
      EXPECT_HRESULT_SUCCEEDED(RegKey::GetValue(kFullFooAppClientStateKeyPath,
                                                kRegValueClientId,
                                                &str_value));
      EXPECT_STREQ(_T("_some_partner"), str_value);
    } else {
      EXPECT_HRESULT_SUCCEEDED(RegKey::GetValue(kFullFooAppClientStateKeyPath,
                                                kRegValueBrandCode,
                                                &str_value));
      EXPECT_STREQ(_T("g00g"), str_value);
      EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
                RegKey::GetValue(kFullFooAppClientStateKeyPath,
                                 kRegValueClientId,
                                 &str_value));
    }

    EXPECT_HRESULT_SUCCEEDED(RegKey::GetValue(kFullFooAppClientStateKeyPath,
                                              kRegValueAdditionalParams,
                                              &str_value));
    EXPECT_STREQ(_T("test_ap"), str_value);
    EXPECT_HRESULT_SUCCEEDED(RegKey::GetValue(kFullFooAppClientStateKeyPath,
                                              kRegValueBrowser,
                                              &value));
    EXPECT_EQ(BROWSER_IE, value);
    EXPECT_HRESULT_SUCCEEDED(RegKey::GetValue(kFullFooAppClientStateKeyPath,
                                              _T("usagestats"),
                                              &value));
    EXPECT_EQ(TRISTATE_TRUE, value);
    EXPECT_HRESULT_SUCCEEDED(RegKey::GetValue(kFullFooAppClientStateKeyPath,
                                              kRegValueLanguage,
                                              &str_value));
    EXPECT_STREQ(_T("en"), str_value);
  }

  void VerifyFooClientStateValuesWrittenBeforeInstallNotPresent(
      bool is_brand_code_present) {
    CString str_value;
    DWORD value;

    HRESULT hr = RegKey::GetValue(kFullFooAppClientStateKeyPath,
                                  kRegValueBrandCode,
                                  &str_value);
    if (is_brand_code_present) {
      EXPECT_HRESULT_SUCCEEDED(hr);
    } else {
      EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), hr);
    }
    EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
              RegKey::GetValue(kFullFooAppClientStateKeyPath,
                               kRegValueClientId,
                               &str_value));
    EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
              RegKey::GetValue(kFullFooAppClientStateKeyPath,
                               kRegValueBrowser,
                               &value));
    EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
              RegKey::GetValue(kFullFooAppClientStateKeyPath,
                               _T("usagestats"),
                               &value));
    EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
              RegKey::GetValue(kFullFooAppClientStateKeyPath,
                               kRegValueLanguage,
                               &str_value));
    EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
              RegKey::GetValue(kFullFooAppClientStateKeyPath,
                               kRegValueAdditionalParams,
                               &str_value));
    EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
              RegKey::GetValue(kFullFooAppClientStateKeyPath,
                               kRegValueTTToken,
                               &str_value));
  }

  // Verifies the values that are written to ClientState after successfully
  // installing. Assumes the is Foo.
  void VerifyFooClientStateValuesWrittenAfterSuccessfulInstall() {
    CString str_value;
    EXPECT_HRESULT_SUCCEEDED(RegKey::GetValue(kFullFooAppClientStateKeyPath,
                                              kRegValueProductVersion,
                                              &str_value));
    EXPECT_STREQ(_T("1.0.101.0"), str_value);
    // TODO(omaha): Verify language. Requires changing the MSI. Make sure the
    // language the MSI writes is different than in PopulateFooAppData().
    // When we do this, make sure we also have a test where the app does not
    // write lang in Client to verify that the ClientState value is not erased.
    EXPECT_HRESULT_SUCCEEDED(RegKey::GetValue(kFullFooAppClientStateKeyPath,
                                              kRegValueInstallationId,
                                              &str_value));
    EXPECT_STREQ(expected_iid_string, str_value);
  }

  void Install_MsiInstallerSucceeds(bool is_update, bool is_first_install);
  void Install_InstallerFailed_WhenNoExistingPreInstallData(bool is_update);
  void Install_InstallerFailed_WhenExistingPreInstallData(bool is_update);

  CString foo_installer_path_;
  CString foo_installer_log_path_;
};

// TODO(omaha): Test all methods of Job

TEST_F(JobTest, DeleteJobDownloadDirectory_Goopdate_Test) {
  const TCHAR* kNnonExistantDir = _T("testdirfoo");
  CompletionInfo info(COMPLETION_SUCCESS, 0, _T(""));

  set_info(info);
  set_download_file_name(ConcatenatePath(kNnonExistantDir, _T("foo.msi")));

  AppData app_data;
  app_data.set_app_guid(kGoopdateGuid);
  SetAppData(app_data);

  ASSERT_HRESULT_SUCCEEDED(DeleteJobDownloadDirectory());
}

// The download file name is not set and no attempt to delete it is made for 
// update checks only.
TEST_F(JobTest, DeleteJobDownloadDirectory_OnDemandUpdateCheckOnly) {
  job_.reset(new Job(true, &ping_));
  job_->set_is_update_check_only(true);

  CompletionInfo info(COMPLETION_SUCCESS, 0, _T(""));

  set_info(info);

  AppData app_data;
  app_data.set_app_guid(
      StringToGuid(_T("{55B9A9BD-16FC-4060-B667-892B312CAAA5}")));
  SetAppData(app_data);

  ASSERT_HRESULT_SUCCEEDED(DeleteJobDownloadDirectory());
}

TEST_F(JobTest, DeleteJobDownloadDirectory_OnDemandUpdate) {
  job_.reset(new Job(true, &ping_));

  GUID guid(GUID_NULL);
  ASSERT_HRESULT_SUCCEEDED(::CoCreateGuid(&guid));
  CString destination_path =
      ConcatenatePath(app_util::GetTempDir(), GuidToString(guid));
  ASSERT_HRESULT_SUCCEEDED(CreateDir(destination_path, NULL));
  CompletionInfo info(COMPLETION_SUCCESS, 0, _T(""));

  set_info(info);
  set_download_file_name(ConcatenatePath(destination_path, _T("foo.msi")));

  AppData app_data;
  app_data.set_app_guid(
      StringToGuid(_T("{55B9A9BD-16FC-4060-B667-892B312CAAA5}")));
  SetAppData(app_data);

  ASSERT_HRESULT_SUCCEEDED(DeleteJobDownloadDirectory());
  ASSERT_FALSE(File::Exists(destination_path));
}

TEST_F(JobTest, DeleteJobDownloadDirectory_Success) {
  GUID guid(GUID_NULL);
  ASSERT_HRESULT_SUCCEEDED(::CoCreateGuid(&guid));
  CString destination_path =
      ConcatenatePath(app_util::GetTempDir(), GuidToString(guid));
  ASSERT_HRESULT_SUCCEEDED(CreateDir(destination_path, NULL));
  CompletionInfo info(COMPLETION_SUCCESS, 0, _T(""));

  set_info(info);
  set_download_file_name(ConcatenatePath(destination_path, _T("foo.msi")));

  AppData app_data;
  app_data.set_app_guid(
      StringToGuid(_T("{55B9A9BD-16FC-4060-B667-892B312CAAA5}")));
  SetAppData(app_data);

  ASSERT_HRESULT_SUCCEEDED(DeleteJobDownloadDirectory());
  ASSERT_FALSE(File::Exists(destination_path));
}

TEST_F(JobTest, SendStateChangePing) {
  if (!vista_util::IsUserAdmin()) {
    std::wcout << _T("\tTest did not run because the user is not an admin.")
               << std::endl;
    return;
  }

  AppManager app_manager(is_machine_);
  AppData app_data(StringToGuid(_T("{4F1A02DC-E965-4518-AED4-E15A3E1B1219}")),
                   is_machine_);

  app_data.set_brand_code(_T("GOOG"));
  app_data.set_client_id(_T("otherclient"));
  app_data.set_ap(_T("Test ap"));
  app_data.set_tt_token(_T("Test TT Token"));
  app_data.set_language(_T("abc"));
  app_data.set_iid(
      StringToGuid(_T("{C16050EA-6D4C-4275-A8EC-22D4C59E942A}")));
  app_data.set_display_name(_T("UnitTest"));
  app_data.set_browser_type(BROWSER_DEFAULT);
  job_->set_app_data(app_data);

  set_job_state(JOBSTATE_INSTALLERSTARTED);
  ASSERT_SUCCEEDED(SendStateChangePing(JOBSTATE_DOWNLOADCOMPLETED));

  CompletionInfo info(COMPLETION_ERROR, 123, _T(""));

  RegKey::DeleteKey(kRegistryHiveOverrideRoot);
  OverrideRegistryHives(kRegistryHiveOverrideRoot);
  CString app_goopdate_key_name = goopdate_utils::GetAppClientsKey(
      is_machine_,
      GOOPDATE_APP_ID);

  RegKey goopdate_key;
  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(app_goopdate_key_name,
                                            kRegValueProductVersion,
                                            _T("1.2.3.4")));
  job_->NotifyCompleted(info);

  RestoreRegistryHives();
  RegKey::DeleteKey(kRegistryHiveOverrideRoot);
  ASSERT_SUCCEEDED(SendStateChangePing(JOBSTATE_INSTALLERSTARTED));
}

TEST_F(JobTest, UpdateRegistry) {
  RegKey::DeleteKey(kRegistryHiveOverrideRoot, true);
  OverrideRegistryHivesWithExecutionPermissions(kRegistryHiveOverrideRoot);

  CString app_guid = _T("{4F1A02DC-E965-4518-AED4-E15A3E1B1219}");
  CString expected_version = _T("1.3.3.3");
  CString previous_version = _T("1.0.0.0");
  CString expected_lang = _T("abc");
  CString reg_key_path = AppendRegKeyPath(MACHINE_REG_CLIENTS, app_guid);
  ASSERT_SUCCEEDED(RegKey::CreateKey(reg_key_path));
  ASSERT_SUCCEEDED(RegKey::SetValue(reg_key_path,
                                    kRegValueProductVersion,
                                    expected_version));
  ASSERT_SUCCEEDED(RegKey::SetValue(reg_key_path,
                                    kRegValueLanguage,
                                    expected_lang));

  AppManager app_manager(is_machine_);
  AppData app_data(StringToGuid(app_guid), is_machine_);
  app_data.set_version(expected_version);
  app_data.set_previous_version(previous_version);
  app_data.set_language(expected_lang);
  app_data.set_ap(_T("Test ap"));
  app_data.set_tt_token(_T("Test TT Token"));
  app_data.set_iid(
      StringToGuid(_T("{C16050EA-6D4C-4275-A8EC-22D4C59E942A}")));
  app_data.set_brand_code(_T("GOOG"));
  app_data.set_client_id(_T("otherclient"));
  app_data.set_display_name(_T("UnitTest"));
  app_data.set_browser_type(BROWSER_DEFAULT);
  app_data.set_install_source(_T("install source"));
  job_->set_app_data(app_data);
  set_job_state(JOBSTATE_COMPLETED);

  // Call the test method.
  AppData new_app_data;
  ASSERT_SUCCEEDED(UpdateRegistry(&new_app_data));

  // Check the results.
  reg_key_path = AppendRegKeyPath(MACHINE_REG_CLIENT_STATE, app_guid);
  ASSERT_SUCCEEDED(RegKey::HasKey(reg_key_path));

  CString actual_previous_version;
  ASSERT_SUCCEEDED(RegKey::GetValue(reg_key_path,
                                    kRegValueProductVersion,
                                    &actual_previous_version));
  CString actual_lang;
  ASSERT_SUCCEEDED(RegKey::GetValue(reg_key_path,
                                    kRegValueLanguage,
                                    &actual_lang));

  // The client state registry should have been updated.
  EXPECT_STREQ(expected_version, actual_previous_version);
  EXPECT_STREQ(expected_lang, actual_lang);

  // The job's previous version should not have changed.
  EXPECT_STREQ(expected_version, get_app_data().version());
  EXPECT_STREQ(previous_version, get_app_data().previous_version());

  // new_app_data's previous_version should have been updated.
  EXPECT_STREQ(expected_version, new_app_data.version());
  EXPECT_STREQ(expected_version, new_app_data.previous_version());

  RestoreRegistryHives();
  ASSERT_SUCCEEDED(RegKey::DeleteKey(kRegistryHiveOverrideRoot, true));
}

TEST_F(JobTest, UpdateJob) {
  RegKey::DeleteKey(kRegistryHiveOverrideRoot, true);
  OverrideRegistryHivesWithExecutionPermissions(kRegistryHiveOverrideRoot);

  CString app_guid = _T("{4F1A02DC-E965-4518-AED4-E15A3E1B1219}");
  CString expected_version = _T("1.3.3.3");
  CString previous_version = _T("1.0.0.0");
  CString expected_lang = _T("abc");
  CString reg_key_path = AppendRegKeyPath(MACHINE_REG_CLIENTS, app_guid);
  ASSERT_SUCCEEDED(RegKey::CreateKey(reg_key_path));
  ASSERT_SUCCEEDED(RegKey::SetValue(reg_key_path,
                                    kRegValueProductVersion,
                                    expected_version));
  ASSERT_SUCCEEDED(RegKey::SetValue(reg_key_path,
                                    kRegValueLanguage,
                                    expected_lang));

  AppManager app_manager(is_machine_);
  AppData app_data(StringToGuid(app_guid), is_machine_);
  app_data.set_version(_T("4.5.6.6"));
  app_data.set_previous_version(previous_version);
  app_data.set_language(expected_lang);
  app_data.set_ap(_T("Test ap"));
  app_data.set_tt_token(_T("Test TT Token"));
  app_data.set_iid(
      StringToGuid(_T("{C16050EA-6D4C-4275-A8EC-22D4C59E942A}")));
  app_data.set_brand_code(_T("GOOG"));
  app_data.set_client_id(_T("otherclient"));
  app_data.set_display_name(_T("UnitTest"));
  app_data.set_browser_type(BROWSER_DEFAULT);
  app_data.set_install_source(_T("install source"));
  job_->set_app_data(app_data);
  set_job_state(JOBSTATE_COMPLETED);

  // Call the test method.
  ASSERT_SUCCEEDED(UpdateJob());

  // Check the results.
  reg_key_path = AppendRegKeyPath(MACHINE_REG_CLIENT_STATE, app_guid);
  CString version;
  EXPECT_HRESULT_FAILED(RegKey::GetValue(MACHINE_REG_CLIENT_STATE_GOOPDATE,
                                         kRegValueProductVersion,
                                         &version));

  // The job's information should have been changed.
  EXPECT_STREQ(expected_version, get_app_data().version());
  EXPECT_STREQ(previous_version, get_app_data().previous_version());

  RestoreRegistryHives();
  ASSERT_SUCCEEDED(RegKey::DeleteKey(kRegistryHiveOverrideRoot, true));
}


// The use of kGoogleUpdateAppId is the key to this test.
// Overrides the registry hives.
TEST_F(JobTest, Install_UpdateOmahaSucceeds) {
  if (!vista_util::IsUserAdmin()) {
    std::wcout << _T("\tTest did not run because the user is not an admin.")
               << std::endl;
    return;
  }

  RegKey::DeleteKey(kRegistryHiveOverrideRoot, true);
  OverrideRegistryHivesWithExecutionPermissions(kRegistryHiveOverrideRoot);

  CString arguments;
  arguments.Format(kExecuteCommandAndTerminateSwitch, _T("echo hi"));

  AppData app_data(kGoopdateGuid, is_machine_);
  job_->set_download_file_name(kJobExecutable);
  job_->set_app_data(app_data);
  SetUpdateResponseDataArguments(arguments);

  CString expected_version(_T("0.9.69.5"));
  CString expected_lang(_T("en"));

  // Because we don't actually run the Omaha installer, we need to make sure
  // its Clients key and pv value exist to avoid an error.
  ASSERT_SUCCEEDED(RegKey::CreateKey(MACHINE_REG_CLIENTS_GOOPDATE));
  ASSERT_TRUE(RegKey::HasKey(MACHINE_REG_CLIENTS_GOOPDATE));
  ASSERT_HRESULT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_CLIENTS_GOOPDATE,
                                            kRegValueProductVersion,
                                            expected_version));
  ASSERT_HRESULT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_CLIENTS_GOOPDATE,
                                            kRegValueLanguage,
                                            expected_lang));

  ASSERT_FALSE(RegKey::HasKey(MACHINE_REG_CLIENT_STATE_GOOPDATE));

  set_job_state(JOBSTATE_DOWNLOADCOMPLETED);
  EXPECT_HRESULT_SUCCEEDED(job_->Install());

  EXPECT_EQ(JOBSTATE_COMPLETED, job_->job_state());

  EXPECT_EQ(COMPLETION_SUCCESS, job_->info().status);
  EXPECT_EQ(S_OK, job_->info().error_code);
  // The user never sees this, but it is odd we put this text in the structure.
  EXPECT_STREQ(_T("Thanks for installing ."), job_->info().text);

  EXPECT_TRUE(RegKey::HasKey(MACHINE_REG_CLIENTS_GOOPDATE));
  EXPECT_TRUE(RegKey::HasKey(MACHINE_REG_CLIENT_STATE_GOOPDATE));

  CString version;
  EXPECT_HRESULT_SUCCEEDED(RegKey::GetValue(MACHINE_REG_CLIENT_STATE_GOOPDATE,
                                            kRegValueProductVersion,
                                            &version));
  EXPECT_STREQ(expected_version, version);

  RestoreRegistryHives();
  ASSERT_HRESULT_SUCCEEDED(RegKey::DeleteKey(kRegistryHiveOverrideRoot, true));
}

TEST_F(JobTest, Install_SuccessfulOmahaUpdateDoesNotClearUpdateAvailableStats) {
  is_machine_ = false;

  RegKey::DeleteKey(kRegistryHiveOverrideRoot, true);
  OverrideRegistryHivesWithExecutionPermissions(kRegistryHiveOverrideRoot);

  CString arguments;
  arguments.Format(kExecuteCommandAndTerminateSwitch, _T("echo hi"));

  AppData app_data(kGoopdateGuid, is_machine_);
  job_->set_download_file_name(kJobExecutable);
  job_->set_app_data(app_data);
  SetUpdateResponseDataArguments(arguments);

  CString expected_version(_T("0.9.69.5"));
  CString expected_lang(_T("en"));

  // Because we don't actually run the Omaha installer, we need to make sure
  // its Clients key and pv value exist to avoid an error.
  ASSERT_SUCCEEDED(RegKey::CreateKey(USER_REG_CLIENTS_GOOPDATE));
  ASSERT_TRUE(RegKey::HasKey(USER_REG_CLIENTS_GOOPDATE));
  ASSERT_HRESULT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                            kRegValueProductVersion,
                                            expected_version));
  ASSERT_HRESULT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                            kRegValueLanguage,
                                            expected_lang));

  // Set update available stats so can verify they are not modified or deleted.
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                    _T("UpdateAvailableCount"),
                                    static_cast<DWORD>(123456)));
  EXPECT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                    _T("UpdateAvailableSince"),
                                    static_cast<DWORD64>(9876543210)));

  set_job_state(JOBSTATE_DOWNLOADCOMPLETED);
  EXPECT_HRESULT_SUCCEEDED(job_->Install());

  EXPECT_EQ(JOBSTATE_COMPLETED, job_->job_state());

  EXPECT_EQ(COMPLETION_SUCCESS, job_->info().status);
  EXPECT_EQ(S_OK, job_->info().error_code);
  // The user never sees this, but it is odd we put this text in the structure.
  EXPECT_STREQ(_T("Thanks for installing ."), job_->info().text);

  EXPECT_TRUE(RegKey::HasKey(USER_REG_CLIENTS_GOOPDATE));
  EXPECT_TRUE(RegKey::HasKey(USER_REG_CLIENT_STATE_GOOPDATE));

  CString version;
  EXPECT_HRESULT_SUCCEEDED(RegKey::GetValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                            kRegValueProductVersion,
                                            &version));
  EXPECT_STREQ(expected_version, version);

  DWORD update_available_count(0);
  EXPECT_SUCCEEDED(RegKey::GetValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                    _T("UpdateAvailableCount"),
                                    &update_available_count));
  EXPECT_EQ(123456, update_available_count);

  DWORD64 update_available_since_time(0);
  EXPECT_SUCCEEDED(RegKey::GetValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                    _T("UpdateAvailableSince"),
                                    &update_available_since_time));
  EXPECT_EQ(9876543210, update_available_since_time);

  RestoreRegistryHives();
  ASSERT_HRESULT_SUCCEEDED(RegKey::DeleteKey(kRegistryHiveOverrideRoot, true));
}

TEST_F(JobTest, GetInstallerData) {
  UpdateResponses responses;
  CString file_name(ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                                    _T("server_manifest.xml")));
  GUID guid = StringToGuid(_T("{D6B08267-B440-4C85-9F79-E195E80D9937}"));

  const TCHAR* kVerboseLog = _T("\n  {\n    \"distribution\": {\n      ")
                             _T("\"verbose_logging\": true\n    }\n  }\n  ");
  const TCHAR* kSkipFirstRun = _T("{\n    \"distribution\": {\n      \"")
                               _T("skip_first_run_ui\": true,\n    }\n  }\n  ");
  CString skip_first_run_encoded;
  EXPECT_SUCCEEDED(WideStringToUtf8UrlEncodedString(kSkipFirstRun,
                                                    &skip_first_run_encoded));

  ASSERT_SUCCEEDED(GoopdateXmlParser::ParseManifestFile(file_name, &responses));
  UpdateResponseData response_data = responses[guid].update_response_data();
  job_->set_update_response_data(response_data);

  // Only set install_data_index to a valid value.
  AppData app_data1(guid, is_machine_);
  app_data1.set_install_data_index(_T("verboselogging"));
  job_->set_app_data(app_data1);

  CString installer_data1;
  EXPECT_SUCCEEDED(job_->GetInstallerData(&installer_data1));
  EXPECT_STREQ(kVerboseLog, installer_data1);

  // Set both installer_data and install_data_index to valid values.
  AppData app_data2(guid, is_machine_);
  app_data2.set_encoded_installer_data(skip_first_run_encoded);
  app_data2.set_install_data_index(_T("verboselogging"));
  job_->set_app_data(app_data2);

  CString installer_data2;
  EXPECT_SUCCEEDED(job_->GetInstallerData(&installer_data2));
  EXPECT_STREQ(kSkipFirstRun, installer_data2);

  // Set installer_data to invalid value, and install_data_index to valid value.
  AppData app_data3(guid, is_machine_);
  app_data3.set_encoded_installer_data(_T("%20%20"));
  app_data3.set_install_data_index(_T("verboselogging"));
  job_->set_app_data(app_data3);

  CString installer_data3;
  EXPECT_EQ(GOOPDATE_E_INVALID_INSTALLER_DATA_IN_APPARGS,
            job_->GetInstallerData(&installer_data3));

  // Set installer_data to valid value, and install_data_index to invalid value.
  AppData app_data4(guid, is_machine_);
  app_data4.set_encoded_installer_data(skip_first_run_encoded);
  app_data4.set_install_data_index(_T("foobar"));
  job_->set_app_data(app_data4);

  CString installer_data4;
  EXPECT_SUCCEEDED(job_->GetInstallerData(&installer_data4));
  EXPECT_STREQ(kSkipFirstRun, installer_data4);

  // Set only install_data_index to invalid value.
  AppData app_data5(guid, is_machine_);
  app_data5.set_install_data_index(_T("foobar"));
  job_->set_app_data(app_data5);

  CString installer_data5;
  EXPECT_EQ(GOOPDATE_E_INVALID_INSTALL_DATA_INDEX,
            job_->GetInstallerData(&installer_data5));

  // Set neither installer_data nor install_data_index.
  AppData app_data6(guid, is_machine_);
  job_->set_app_data(app_data6);

  CString installer_data6;
  EXPECT_SUCCEEDED(job_->GetInstallerData(&installer_data6));
  EXPECT_TRUE(installer_data6.IsEmpty());
}

// It would be nice if the Foo installer actually used the installer data as a
// way to verify the data file is passed correctly. Alternatively, we could mock
// the installer execution.
TEST_F(JobInstallFooTest, InstallerData_ValidIndex) {
  if (!vista_util::IsUserAdmin()) {
    std::wcout << _T("\tTest did not run because the user is not an admin.")
               << std::endl;
    return;
  }

  UpdateResponses responses;
  CString file_name(ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                                    _T("server_manifest.xml")));
  GUID guid = StringToGuid(_T("{D6B08267-B440-4C85-9F79-E195E80D9937}"));

  ASSERT_SUCCEEDED(GoopdateXmlParser::ParseManifestFile(file_name, &responses));
  UpdateResponseData response_data = responses[guid].update_response_data();
  job_->set_update_response_data(response_data);

  AppData app_data(PopulateFooAppData());
  app_data.set_install_data_index(_T("verboselogging"));
  job_->set_download_file_name(foo_installer_path_);
  job_->set_app_data(app_data);

  set_job_state(JOBSTATE_DOWNLOADCOMPLETED);
  EXPECT_HRESULT_SUCCEEDED(job_->Install());

  EXPECT_EQ(JOBSTATE_COMPLETED, job_->job_state());

  EXPECT_EQ(COMPLETION_SUCCESS, job_->info().status);
  EXPECT_EQ(S_OK, job_->info().error_code);
  EXPECT_STREQ(_T("Thanks for installing Foo."), job_->info().text);
}

// Set installer_data to invalid value, and install_data_index to valid value.
TEST_F(JobInstallFooTest, InstallerData_InvalidData) {
  if (!vista_util::IsUserAdmin()) {
    std::wcout << _T("\tTest did not run because the user is not an admin.")
               << std::endl;
    return;
  }

  AppData app_data(PopulateFooAppData());
  app_data.set_encoded_installer_data(_T("%20%20"));
  app_data.set_install_data_index(_T("verboselogging"));
  job_->set_app_data(app_data);

  set_job_state(JOBSTATE_DOWNLOADCOMPLETED);
  EXPECT_EQ(GOOPDATE_E_INVALID_INSTALLER_DATA_IN_APPARGS, job_->Install());

  EXPECT_EQ(JOBSTATE_COMPLETED, job_->job_state());

  EXPECT_EQ(COMPLETION_ERROR, job_->info().status);
  EXPECT_EQ(GOOPDATE_E_INVALID_INSTALLER_DATA_IN_APPARGS,
            job_->info().error_code);
  EXPECT_STREQ(
      _T("Installation failed. Please try again. Error code = 0x8004090a"),
      job_->info().text);
}

TEST_F(JobInstallFooTest, InstallerData_NonExistentIndex) {
  if (!vista_util::IsUserAdmin()) {
    std::wcout << _T("\tTest did not run because the user is not an admin.")
               << std::endl;
    return;
  }

  AppData app_data(PopulateFooAppData());
  app_data.set_install_data_index(_T("foobar"));
  job_->set_app_data(app_data);

  set_job_state(JOBSTATE_DOWNLOADCOMPLETED);
  EXPECT_EQ(GOOPDATE_E_INVALID_INSTALL_DATA_INDEX, job_->Install());

  EXPECT_EQ(JOBSTATE_COMPLETED, job_->job_state());

  EXPECT_EQ(COMPLETION_ERROR, job_->info().status);
  EXPECT_EQ(GOOPDATE_E_INVALID_INSTALL_DATA_INDEX, job_->info().error_code);
  EXPECT_STREQ(
      _T("Installation failed. Please try again. Error code = 0x80040909"),
      job_->info().text);
}

/*
// TODO(omaha): Adapt into a test for SendStateChangePing by mocking ping.
TEST_F(JobTest, BuildPingRequestFromJob_PopulatesAppRequest) {
  const TCHAR* const kGuid = _T("{21CD0965-0B0E-47cf-B421-2D191C16C0E2}");
  AppData app_data(StringToGuid(kGuid), true);
  CreateTestAppData(&app_data);
  job_->set_app_data(app_data);
  set_job_state(JOBSTATE_COMPLETED);

  CompletionInfo info(COMPLETION_INSTALLER_ERROR_MSI, 0x12345678, _T("foo"));
  set_info(info);

  Request req(true);
  BuildPingRequestFromJob(&req);
  ASSERT_EQ(1, req.get_app_count());

  const AppRequest* app_request = req.GetApp(StringToGuid(kGuid));
  EXPECT_STREQ(_T("{21CD0965-0B0E-47CF-B421-2D191C16C0E2}"),
               GuidToString(app_request->guid()));
  EXPECT_EQ(_T("1.1.1.3"), app_request->version());
  EXPECT_EQ(_T("abc"), app_request->language());
  EXPECT_EQ(AppData::ACTIVE_UNKNOWN, app_request->active());
  EXPECT_TRUE(app_request->tag().IsEmpty());
  EXPECT_STREQ(_T("{F723495F-8ACF-4746-824D-643741C797B5}"),
               GuidToString(app_request->installation_id()));
  EXPECT_EQ(_T("GOOG"), app_request->brand_code());
  EXPECT_EQ(_T("someclient"), app_request->client_id());
  EXPECT_EQ(_T("twoclick"), app_request->install_source());

  ASSERT_TRUE(app_request->events_end() == ++app_request->events_begin());
  const AppEvent* app_event = *app_request->events_begin();
  EXPECT_EQ(AppEvent::EVENT_UPDATE_COMPLETE, app_event->event_type());
  EXPECT_EQ(AppEvent::EVENT_RESULT_INSTALLER_ERROR_MSI,
            app_event->event_result());
  EXPECT_EQ(0x12345678, app_event->error_code());
  EXPECT_EQ(0, app_event->extra_code1());
  EXPECT_EQ(_T("1.0.0.0"), app_event->previous_version());
}

// TODO(omaha): Adapt into a test for CreateRequestFromProducts
TEST_F(JobTest, BuildRequest_PopulatesAppRequest) {
  const TCHAR* const kGuid = _T("{21CD0965-0B0E-47cf-B421-2D191C16C0E2}");
  AppData app_data(StringToGuid(kGuid), true);
  CreateTestAppData(&app_data);
  job_->set_app_data(app_data);

  Request req(true);
  ASSERT_SUCCEEDED(job_->BuildRequest(&req));
  ASSERT_EQ(1, req.get_app_count());

  const AppRequest* app_request = req.GetApp(StringToGuid(kGuid));
  EXPECT_STREQ(_T("{21CD0965-0B0E-47CF-B421-2D191C16C0E2}"),
               GuidToString(app_request->guid()));
  EXPECT_EQ(_T("1.1.1.3"), app_request->version());
  EXPECT_EQ(_T("abc"), app_request->language());
  EXPECT_EQ(AppData::ACTIVE_RUN, app_request->active());
  EXPECT_EQ(_T("Test ap"), app_request->tag());
  EXPECT_STREQ(_T("{F723495F-8ACF-4746-824D-643741C797B5}"),
               GuidToString(app_request->installation_id()));
  EXPECT_EQ(_T("GOOG"), app_request->brand_code());
  EXPECT_EQ(_T("someclient"), app_request->client_id());
  EXPECT_EQ(_T("twoclick"), app_request->install_source());
}

// TODO(omaha): Move to some other test file.
TEST(RequestTest, TestInitialized) {
  AppRequest app_request;
  EXPECT_TRUE(::IsEqualGUID(GUID_NULL, app_request.guid()));
  EXPECT_TRUE(app_request.version().IsEmpty());
  EXPECT_TRUE(app_request.language().IsEmpty());
  EXPECT_EQ(AppData::ACTIVE_UNKNOWN, app_request.active());
  EXPECT_TRUE(app_request.tag().IsEmpty());
  EXPECT_TRUE(::IsEqualGUID(GUID_NULL, app_request.installation_id()));
  EXPECT_TRUE(app_request.brand_code().IsEmpty());
  EXPECT_TRUE(app_request.client_id().IsEmpty());
  EXPECT_TRUE(app_request.install_source().IsEmpty());
}
*/

void JobInstallFooTest::Install_MsiInstallerSucceeds(bool is_update,
                                                     bool is_first_install) {
  // TODO(omaha): Use UserFoo instead, change is_machine in the base class,
  // and remove all IsUserAdmin checks.
  if (!vista_util::IsUserAdmin()) {
    std::wcout << _T("\tTest did not run because the user is not an admin.")
               << std::endl;
    return;
  }

#ifdef _DEBUG
  if (!is_update) {
    // Event::WriteEvent() expects Omaha's version to exist.
    // Write it if it doesn't exist.
    ConfigManager& config_mgr = *ConfigManager::Instance();
    CString key_name = config_mgr.registry_clients_goopdate(is_machine_);
    if (!RegKey::HasValue(key_name, kRegValueLanguage)) {
      EXPECT_HRESULT_SUCCEEDED(
          RegKey::SetValue(key_name, kRegValueLanguage, _T("it")));
    }
    if (!RegKey::HasValue(key_name, kRegValueProductVersion)) {
      EXPECT_HRESULT_SUCCEEDED(
          RegKey::SetValue(key_name, kRegValueProductVersion, _T("0.1.0.0")));
    }
  }
#endif

  if (!is_update) {
    SetIsInstallJob();
  }

  if (!is_first_install) {
    // Make it appear to Omaha that Foo was already installed. This does not
    // affect the installer.
    EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(kFullFooAppClientStateKeyPath,
                                              kRegValueBrandCode,
                                              _T("g00g")));

    // Set update available stats so can verify they are deleted.
    EXPECT_SUCCEEDED(RegKey::SetValue(kFullFooAppClientStateKeyPath,
                                      _T("UpdateAvailableCount"),
                                      static_cast<DWORD>(123456)));
    EXPECT_SUCCEEDED(RegKey::SetValue(kFullFooAppClientStateKeyPath,
                                      _T("UpdateAvailableSince"),
                                      static_cast<DWORD64>(9876543210)));
  }

  AppData app_data(PopulateFooAppData());
  job_->set_download_file_name(foo_installer_path_);
  job_->set_app_data(app_data);

  set_job_state(JOBSTATE_DOWNLOADCOMPLETED);
  EXPECT_HRESULT_SUCCEEDED(job_->Install());

  EXPECT_EQ(JOBSTATE_COMPLETED, job_->job_state());

  EXPECT_EQ(COMPLETION_SUCCESS, job_->info().status);
  EXPECT_EQ(S_OK, job_->info().error_code);
  EXPECT_STREQ(_T("Thanks for installing Foo."), job_->info().text);

  EXPECT_TRUE(File::Exists(foo_installer_log_path_));

  EXPECT_TRUE(RegKey::HasKey(kFullFooAppClientKeyPath));
  EXPECT_TRUE(RegKey::HasKey(kFullFooAppClientStateKeyPath));

  if (is_update) {
    VerifyFooClientStateValuesWrittenBeforeInstallNotPresent(!is_first_install);
  } else {
    VerifyFooClientStateValuesWrittenBeforeInstall(is_first_install);
  }
  VerifyFooClientStateValuesWrittenAfterSuccessfulInstall();

  EXPECT_FALSE(RegKey::HasValue(kFullFooAppClientStateKeyPath,
                                _T("UpdateAvailableCount")));
  EXPECT_FALSE(RegKey::HasValue(kFullFooAppClientStateKeyPath,
                                _T("UpdateAvailableSince")));

  CString uninstall_arguments;
  uninstall_arguments.Format(kMsiUninstallArguments, foo_installer_path_);
  EXPECT_HRESULT_SUCCEEDED(System::ShellExecuteProcess(kMsiCommand,
                                                       uninstall_arguments,
                                                       NULL,
                                                       NULL));
}

TEST_F(JobInstallFooTest, Install_MsiInstallerSucceedsFirstInstall) {
  Install_MsiInstallerSucceeds(false, true);
}

TEST_F(JobInstallFooTest, Install_MsiInstallerSucceedsOverInstall) {
  Install_MsiInstallerSucceeds(false, false);
}

TEST_F(JobInstallFooTest,
       Install_MsiInstallerSucceedsUpdate_WhenNoExistingPreInstallData) {
  // AppManager::ClearUpdateAvailableStats() asserts that key open succeeds.
  EXPECT_SUCCEEDED(RegKey::CreateKey(kFullFooAppClientStateKeyPath));
  Install_MsiInstallerSucceeds(true, true);
}

TEST_F(JobInstallFooTest,
       Install_MsiInstallerSucceedsUpdate_WhenExistingPreInstallData) {
  Install_MsiInstallerSucceeds(true, false);
}

void JobInstallFooTest::Install_InstallerFailed_WhenNoExistingPreInstallData(
    bool is_update) {
  if (!vista_util::IsUserAdmin()) {
    std::wcout << _T("\tTest did not run because the user is not an admin.")
               << std::endl;
    return;
  }

  if (!is_update) {
    SetIsInstallJob();
  }

  RegKey::DeleteKey(kRegistryHiveOverrideRoot, true);
  OverrideRegistryHivesWithExecutionPermissions(kRegistryHiveOverrideRoot);
#ifdef _DEBUG
  // Event::WriteEvent() expects Omaha's language and version to exist.
  ConfigManager& config_mgr = *ConfigManager::Instance();
  CString key_name = config_mgr.registry_clients_goopdate(is_machine_);
  EXPECT_HRESULT_SUCCEEDED(
      RegKey::SetValue(key_name, kRegValueLanguage, _T("it")));
  EXPECT_HRESULT_SUCCEEDED(
      RegKey::SetValue(key_name, kRegValueProductVersion, _T("0.1.2.3")));
#endif

  AppData app_data(PopulateFooAppData());
  job_->set_download_file_name(_T("DoesNotExist.exe"));
  job_->set_app_data(app_data);

  set_job_state(JOBSTATE_DOWNLOADCOMPLETED);
  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_FAILED_START, job_->Install());

  EXPECT_EQ(JOBSTATE_COMPLETED, job_->job_state());

  EXPECT_EQ(COMPLETION_ERROR, job_->info().status);
  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_FAILED_START, job_->info().error_code);
  EXPECT_STREQ(_T("The installer failed to start."), job_->info().text);

  EXPECT_FALSE(RegKey::HasKey(kFullFooAppClientKeyPath));
  EXPECT_FALSE(RegKey::HasKey(kFullFooAppClientStateKeyPath));

  // TODO(omaha): Install the job successfully and verify that the brand data
  // is replaced by the successful install.

  RestoreRegistryHives();
  ASSERT_SUCCEEDED(RegKey::DeleteKey(kRegistryHiveOverrideRoot, true));
}

// Overrides the registry hives.
TEST_F(JobInstallFooTest,
       Install_InstallerFailedFirstInstall_WhenNoExistingPreInstallData) {
  Install_InstallerFailed_WhenNoExistingPreInstallData(false);
}

TEST_F(JobInstallFooTest,
       Install_InstallerFailedUpdate_WhenNoExistingPreInstallData) {
  Install_InstallerFailed_WhenNoExistingPreInstallData(true);
}

void JobInstallFooTest::Install_InstallerFailed_WhenExistingPreInstallData(
    bool is_update) {
  if (!vista_util::IsUserAdmin()) {
    std::wcout << _T("\tTest did not run because the user is not an admin.")
               << std::endl;
    return;
  }

  if (!is_update) {
    SetIsInstallJob();
  }

  RegKey::DeleteKey(kRegistryHiveOverrideRoot, true);
  OverrideRegistryHivesWithExecutionPermissions(kRegistryHiveOverrideRoot);

#ifdef _DEBUG
  // Event::WriteEvent() expects Omaha's language and version to exist.
  ConfigManager& config_mgr = *ConfigManager::Instance();
  CString key_name = config_mgr.registry_clients_goopdate(is_machine_);
  EXPECT_HRESULT_SUCCEEDED(
      RegKey::SetValue(key_name, kRegValueLanguage, _T("it")));
  EXPECT_HRESULT_SUCCEEDED(
      RegKey::SetValue(key_name, kRegValueProductVersion, _T("0.1.2.3")));
#endif

  // Prepopulate data for this app in the ClientState registry
  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(kFullFooAppClientStateKeyPath,
                                            kRegValueProductVersion,
                                            _T("0.1.2.3")));

  // Set update available stats so can verify they are not modified or deleted.
  EXPECT_SUCCEEDED(RegKey::SetValue(kFullFooAppClientStateKeyPath,
                                    _T("UpdateAvailableCount"),
                                    static_cast<DWORD>(123456)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kFullFooAppClientStateKeyPath,
                                    _T("UpdateAvailableSince"),
                                    static_cast<DWORD64>(9876543210)));

  AppData app_data(PopulateFooAppData());
  job_->set_download_file_name(_T("DoesNotExist.exe"));
  job_->set_app_data(app_data);

  set_job_state(JOBSTATE_DOWNLOADCOMPLETED);
  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_FAILED_START, job_->Install());

  EXPECT_EQ(JOBSTATE_COMPLETED, job_->job_state());

  EXPECT_EQ(COMPLETION_ERROR, job_->info().status);
  EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_FAILED_START, job_->info().error_code);
  EXPECT_STREQ(_T("The installer failed to start."), job_->info().text);

  EXPECT_EQ(is_update, RegKey::HasKey(kFullFooAppClientStateKeyPath));
  EXPECT_FALSE(RegKey::HasKey(kFullFooAppClientKeyPath));

  // When an update fails, data remains. When an install fails, data is deleted.
  if (is_update) {
    DWORD update_available_count(0);
    EXPECT_SUCCEEDED(RegKey::GetValue(kFullFooAppClientStateKeyPath,
                                      _T("UpdateAvailableCount"),
                                      &update_available_count));
    EXPECT_EQ(123456, update_available_count);

    DWORD64 update_available_since_time(0);
    EXPECT_SUCCEEDED(RegKey::GetValue(kFullFooAppClientStateKeyPath,
                                      _T("UpdateAvailableSince"),
                                      &update_available_since_time));
    EXPECT_EQ(9876543210, update_available_since_time);
  }

  RestoreRegistryHives();
  ASSERT_SUCCEEDED(RegKey::DeleteKey(kRegistryHiveOverrideRoot, true));
}

TEST_F(JobInstallFooTest,
       Install_InstallerFailedFirstInstall_WhenExistingPreInstallData) {
  Install_InstallerFailed_WhenExistingPreInstallData(false);
}

TEST_F(JobInstallFooTest,
       Install_InstallerFailedUpdate_WhenExistingPreInstallData) {
  Install_InstallerFailed_WhenExistingPreInstallData(true);
}


}  // namespace omaha
