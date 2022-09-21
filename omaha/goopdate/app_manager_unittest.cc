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

#include "omaha/goopdate/app_manager.h"

#include <memory>

#include "omaha/base/const_object_names.h"
#include "omaha/base/error.h"
#include "omaha/base/highres_timer-win32.h"
#include "omaha/base/reg_key.h"
#include "omaha/base/scope_guard.h"
#include "omaha/base/thread.h"
#include "omaha/base/time.h"
#include "omaha/base/utils.h"
#include "omaha/base/vistautil.h"
#include "omaha/base/wmi_query.h"
#include "omaha/common/app_registry_utils.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/goopdate/app_unittest_base.h"
#include "omaha/goopdate/worker.h"
#include "omaha/setup/setup_google_update.h"
#include "omaha/testing/unit_test.h"
#include "omaha/third_party/smartany/scoped_any.h"

using ::testing::_;
using ::testing::Return;

namespace omaha {

// TODO(omaha): there is a problem with this unit test. The model is built
// bottom up. This makes it impossible to set the references to parents. Will
// have to fix the code, eventually using Builder DP to create a bunch of
// models containing bundles, apps, and such.

namespace {

const TCHAR* const kGuid1 = _T("{21CD0965-0B0E-47cf-B421-2D191C16C0E2}");
const TCHAR* const kGuid2 = _T("{A979ACBD-1F55-4b12-A35F-4DBCA5A7CCB8}");
const TCHAR* const kGuid3 = _T("{661045C5-4429-4140-BC48-8CEA241D1DEF}");
const TCHAR* const kGuid4 = _T("{AAFA1CF9-E94F-42e6-A899-4CD27F37D5A7}");
const TCHAR* const kGuid5 = _T("{3B1A3CCA-0525-4418-93E6-A0DB3398EC9B}");
const TCHAR* const kGuid6 = _T("{F3F2CFD4-5F98-4bf0-ABB0-BEEEA46C62B4}");
const TCHAR* const kGuid7 = _T("{6FD2272F-8583-4bbd-895A-E65F8003FC7B}");
const TCHAR* const kIid1  = _T("{F723495F-8ACF-4746-8240-643741C797B5}");

const TCHAR* const kNonExistentClsid =
    _T("{BC00156D-3B01-4ba3-9F5E-2C46E8B6E824}");

const TCHAR* const kGuid1ClientsKeyPathUser =
    _T("HKCU\\Software\\") PATH_COMPANY_NAME
    _T("\\") PRODUCT_NAME _T("\\Clients\\")
    _T("{21CD0965-0B0E-47cf-B421-2D191C16C0E2}");
const TCHAR* const kGuid1ClientsKeyPathMachine =
    _T("HKLM\\Software\\") PATH_COMPANY_NAME
    _T("\\") PRODUCT_NAME _T("\\Clients\\")
    _T("{21CD0965-0B0E-47cf-B421-2D191C16C0E2}");
const TCHAR* const kGuid1ClientStateKeyPathUser =
    _T("HKCU\\Software\\") PATH_COMPANY_NAME
    _T("\\") PRODUCT_NAME _T("\\ClientState\\")
    _T("{21CD0965-0B0E-47cf-B421-2D191C16C0E2}");
const TCHAR* const kGuid1ClientStateKeyPathMachine =
    _T("HKLM\\Software\\") PATH_COMPANY_NAME
    _T("\\") PRODUCT_NAME _T("\\ClientState\\")
    _T("{21CD0965-0B0E-47cf-B421-2D191C16C0E2}");

const TCHAR* const kDefaultAppName = SHORT_COMPANY_NAME _T(" Application");

const uint32 kInitialInstallTimeDiff = static_cast<uint32>(-1 * kSecondsPerDay);

// Initializes a GLock in the same way AppManager does. Used for lock conflict
// tests.
void InitializeAppManagerRegistryLock(bool is_machine, GLock* lock) {
  ASSERT1(lock);
  NamedObjectAttributes lock_attr;
  GetNamedObjectAttributes(kRegistryAccessMutex, is_machine, &lock_attr);
  EXPECT_TRUE(lock->InitializeWithSecAttr(lock_attr.name, &lock_attr.sa));
}

}  // namespace

// Helper functions defined in other test files.
void ValidateExpectedValues(const App& expected, const App& actual);

class AppManagerTestBase : public AppTestBaseWithRegistryOverride {
 public:
  static void SetDisplayName(const CString& name, App* app) {
    ASSERT1(app);
    app->display_name_ = name;
  }

 protected:
  // Creates the application registration entries based on the passed in data.
  // If passed an application that is uninstalled, the function only creates
  // the registration entries in the client state and no information is written
  // in the clients.
  static void CreateAppRegistryState(const App& app,
                                     bool is_machine,
                                     const CString& previous_version,
                                     bool can_write_clients_key) {
    CString clients_key_name = AppendRegKeyPath(
        ConfigManager::Instance()->registry_clients(is_machine),
        app.app_guid_string());
    CString client_state_key_name = AppendRegKeyPath(
        ConfigManager::Instance()->registry_client_state(is_machine),
        app.app_guid_string());
    const uint32 now = Time64ToInt32(GetCurrent100NSTime());

    RegKey client_key;
    if (can_write_clients_key) {
      ASSERT_SUCCEEDED(client_key.Create(clients_key_name));

      CString current_version(app.current_version()->version());
      if (!current_version.IsEmpty()) {
        ASSERT_SUCCEEDED(client_key.SetValue(kRegValueProductVersion,
                                             current_version));
      }

      if (!app.display_name_.IsEmpty()) {
        ASSERT_SUCCEEDED(client_key.SetValue(kRegValueAppName,
                                             app.display_name_));
      }
    }

    RegKey client_state_key;
    ASSERT_SUCCEEDED(client_state_key.Create(client_state_key_name));

    if (!previous_version.IsEmpty()) {
      ASSERT_SUCCEEDED(client_state_key.SetValue(kRegValueProductVersion,
                                                 previous_version));
    }

    if (!app.language_.IsEmpty()) {
      // TODO(omaha3): This is some interesting logic wrt Clients/ClientState.
      // Does it still make sense for Omaha 3?
      if (can_write_clients_key) {
        ASSERT_SUCCEEDED(client_key.SetValue(kRegValueLanguage,
                                             app.language_));
      } else {
        ASSERT_SUCCEEDED(client_state_key.SetValue(kRegValueLanguage,
                                                   app.language_));
      }
    }

    if (app.did_run_ != ACTIVE_UNKNOWN) {
      CString dr = (app.did_run_ == ACTIVE_NOTRUN) ? _T("0") : _T("1");
      ASSERT_SUCCEEDED(client_state_key.SetValue(kRegValueDidRun,
                                                 dr));
    }

    if (!app.ap_.IsEmpty()) {
      ASSERT_SUCCEEDED(client_state_key.SetValue(kRegValueAdditionalParams,
                                                 app.ap_));
    }

    if (!app.tt_token_.IsEmpty()) {
      ASSERT_SUCCEEDED(client_state_key.SetValue(kRegValueTTToken,
                                                 app.tt_token_));
    }

    if (!::IsEqualGUID(app.iid_, GUID_NULL)) {
      ASSERT_SUCCEEDED(client_state_key.SetValue(kRegValueInstallationId,
                                                 GuidToString(app.iid_)));
    }

    if (!app.brand_code_.IsEmpty()) {
      ASSERT_SUCCEEDED(client_state_key.SetValue(kRegValueBrandCode,
                                                 app.brand_code_));
    }

    if (!app.client_id_.IsEmpty()) {
      ASSERT_SUCCEEDED(client_state_key.SetValue(kRegValueClientId,
                                                 app.client_id_));
    }

    if (!app.referral_id_.IsEmpty()) {
      ASSERT_SUCCEEDED(client_state_key.SetValue(kRegValueReferralId,
                                                 app.referral_id_));
    }

    if (!app.referral_id_.IsEmpty()) {
      ASSERT_SUCCEEDED(client_state_key.SetValue(kRegValueReferralId,
                                                 app.referral_id_));
    }

    if (app.install_time_diff_sec_) {
      const DWORD install_time = now - app.install_time_diff_sec_;
      ASSERT_SUCCEEDED(client_state_key.SetValue(kRegValueInstallTimeSec,
                                                 install_time));
    }

    if (app.day_of_install() > 0) {
      EXPECT_LE(kMinDaysSinceDatum, app.day_of_install());
      EXPECT_GE(kMaxDaysSinceDatum, app.day_of_install());
      ASSERT_SUCCEEDED(client_state_key.SetValue(
          kRegValueDayOfInstall, static_cast<DWORD>(app.day_of_install())));
    }

    if (app.is_eula_accepted_ == TRISTATE_FALSE) {
      ASSERT_SUCCEEDED(client_state_key.SetValue(_T("eulaaccepted"),
                                                 static_cast<DWORD>(0)));
    }

    int days = app.days_since_last_active_ping();

    if (days != -1) {
      EXPECT_GE(days, 0);
      ASSERT1(now > static_cast<uint32>(days * kSecondsPerDay));
      uint32 last_active_time = now - days * kSecondsPerDay;

      ASSERT_SUCCEEDED(client_state_key.SetValue(
          kRegValueActivePingDayStartSec,
          static_cast<DWORD>(last_active_time)));
    }

    days = app.days_since_last_roll_call();
    if (days != -1) {
      EXPECT_GE(days, 0);
      EXPECT_GE(now, static_cast<uint32>(days * kSecondsPerDay));

      uint32 last_roll_call_time = now - days * kSecondsPerDay;

      ASSERT_SUCCEEDED(client_state_key.SetValue(
          kRegValueRollCallDayStartSec,
          static_cast<DWORD>(last_roll_call_time)));
    }

    int num_days_since = app.day_of_last_activity();

    if (num_days_since > 0) {
      EXPECT_GE(num_days_since, kMinDaysSinceDatum);
      EXPECT_LE(num_days_since, kMaxDaysSinceDatum);

      ASSERT_SUCCEEDED(client_state_key.SetValue(
          kRegValueDayOfLastActivity,
          static_cast<DWORD>(num_days_since)));
    }

    num_days_since = app.day_of_last_roll_call();
    if (num_days_since > 0) {
      EXPECT_GE(num_days_since, kMinDaysSinceDatum);
      EXPECT_LE(num_days_since, kMaxDaysSinceDatum);

      ASSERT_SUCCEEDED(client_state_key.SetValue(
          kRegValueDayOfLastRollCall,
          static_cast<DWORD>(num_days_since)));
    }

    ASSERT_SUCCEEDED(app_registry_utils::SetUsageStatsEnable(
        is_machine, app.app_guid_string(), app.usage_stats_enable()));
  }

  // App will be cleaned up when bundle is destroyed.
  // This is a hack for creating registry data. Would be nice to have a
  // different mechanism.
  App* CreateAppForRegistryPopulation(const TCHAR* app_id) {
    App* app = NULL;
    EXPECT_SUCCEEDED(
        test_app_bundle_for_app_creation_->createApp(CComBSTR(app_id), &app));
    ASSERT1(app);

    // install_time_diff_sec_ is -1 day for new app. After that, the app
    // becomes registered and the install age will be 0. So set the time to 0
    // to make the expected value and actual equal.
    app->install_time_diff_sec_ = 0;
    return app;
  }

  static void PopulateExpectedApp1ClientsOnly(App* expected_app) {
    ASSERT_TRUE(expected_app);
    expected_app->current_version()->set_version(_T("1.1.1.3"));
    expected_app->language_ = _T("abc");
    expected_app->display_name_ = _T("My App");

    // This is the result when Client State does not exist.
    expected_app->install_time_diff_sec_ =
        static_cast<uint32>(-1 * kSecondsPerDay);
  }

  static void PopulateExpectedApp1(App* expected_app) {
    ASSERT_TRUE(expected_app);
    expected_app->current_version()->set_version(_T("1.1.1.3"));
    expected_app->language_ = _T("abc");
    expected_app->display_name_ = _T("My App");
    expected_app->ap_ = _T("Test ap");
    expected_app->tt_token_ = _T("Test TT Token");
    expected_app->iid_ =
        StringToGuid(_T("{F723495F-8ACF-4746-8240-643741C797B5}"));
    expected_app->brand_code_ = _T("GOOG");
    expected_app->client_id_ = _T("someclient");
    // Do not set referral_id or install_time_diff_sec because these are not
    // expected in most cases.
    expected_app->did_run_ = ACTIVE_RUN;
    expected_app->set_days_since_last_active_ping(3);
    expected_app->set_days_since_last_roll_call(1);
    expected_app->usage_stats_enable_ = TRISTATE_TRUE;
  }

  static void PopulateExpectedApp2(App* expected_app) {
    ASSERT_TRUE(expected_app);
    expected_app->current_version()->set_version(_T("1.2.1.3"));
    expected_app->language_ = _T("de");
    expected_app->ap_ = _T("beta");
    expected_app->tt_token_ = _T("beta TT Token");
    expected_app->iid_ =
        StringToGuid(_T("{431EC961-CFD8-49ea-AB7B-2B99BCA274AD}"));
    expected_app->brand_code_ = _T("GooG");
    expected_app->client_id_ = _T("anotherclient");
    expected_app->did_run_ = ACTIVE_NOTRUN;
    expected_app->set_days_since_last_active_ping(100);
    expected_app->set_days_since_last_roll_call(1);
    expected_app->usage_stats_enable_ = TRISTATE_FALSE;
  }

  static void PopulateExpectedUninstalledApp(const CString& uninstalled_version,
                                             App* expected_app) {
    ASSERT_TRUE(expected_app);
    PopulateExpectedApp2(expected_app);

    expected_app->current_version()->set_version(uninstalled_version);
  }

  static void SetAppInstallTimeDiffSec(App* app,
                                       uint32 install_time_diff_sec) {
    ASSERT_TRUE(app);
    app->install_time_diff_sec_ = install_time_diff_sec;
  }

  explicit AppManagerTestBase(bool is_machine)
      : AppTestBaseWithRegistryOverride(is_machine, true),
        app_manager_(NULL),
        app_(NULL),
        guid1_(StringToGuid(kGuid1)) {}

  virtual void SetUp() {
    AppTestBaseWithRegistryOverride::SetUp();

    app_manager_ = AppManager::Instance();
    ASSERT_TRUE(app_manager_);

    // Initialize the second bundle.
    test_app_bundle_for_app_creation_ = model_->CreateAppBundle(is_machine_);
    ASSERT_TRUE(test_app_bundle_for_app_creation_.get());

    EXPECT_SUCCEEDED(test_app_bundle_for_app_creation_->put_displayName(
                         CComBSTR(_T("My Bundle"))));
    EXPECT_SUCCEEDED(test_app_bundle_for_app_creation_->put_displayLanguage(
                         CComBSTR(_T("en"))));
    EXPECT_SUCCEEDED(test_app_bundle_for_app_creation_->put_installSource(
                         CComBSTR(_T("unittest"))));
    // TODO(omaha3): Address with the TODO in AppBundleInitializedTest::SetUp().
    if (is_machine_) {
      SetAppBundleStateForUnitTest(test_app_bundle_for_app_creation_.get(),
                                   new fsm::AppBundleStateInitialized);
    } else {
      EXPECT_SUCCEEDED(test_app_bundle_for_app_creation_->initialize());
    }

    EXPECT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kGuid1), &app_));
    ASSERT_TRUE(app_);
  }

  virtual void TearDown() {
    app_manager_ = NULL;

    AppTestBaseWithRegistryOverride::TearDown();
  }

  static void UpdateUpdateAvailableStats(const GUID& app_guid,
                                         AppManager* app_manager) {
    ASSERT1(app_manager);
    app_manager->UpdateUpdateAvailableStats(app_guid);
  }

  CString GetClientKeyName(const GUID& app_guid) const {
    return app_manager_->GetClientKeyName(app_guid);
  }

  static void SetAppGuid(const CString& guid, App* app) {
    ASSERT1(app);
    app->app_guid_ = StringToGuid(guid);
  }

  bool IsClientStateKeyPresent(const App& app) {
    CString client_state_key_name = AppendRegKeyPath(
        ConfigManager::Instance()->registry_client_state(is_machine_),
        GuidToString(app.app_guid_));

    return RegKey::HasKey(client_state_key_name);
  }

  // TODO(omaha3): In this test or elsewhere, test the new Omaha 3 behaviors:
  // IID is deleted if GUID_NULL and dr is cleared.
  // TODO(omaha3): Add checks for values set/not set by
  // app_registry_utils::PersistSuccessfulInstall().
  void PersistSuccessfulInstallTest() {
    // Create the data the installer would have written. These values should
    // not be read below because the caller is responsible for updating these
    // values.
    CString clients_key_name = AppendRegKeyPath(
        ConfigManager::Instance()->registry_clients(is_machine_),
        kGuid1);
    EXPECT_SUCCEEDED(RegKey::SetValue(clients_key_name,
                                     kRegValueProductVersion,
                                     _T("9.8.7.6")));
    EXPECT_SUCCEEDED(
        RegKey::SetValue(clients_key_name, kRegValueLanguage, _T("fr")));

    // Populate the App structure. For the most part, these values are not
    // used. Exceptions are:
    // * pv and language are written.
    // * iid is written to the registry.
    app_->display_name_ = _T("foo");
    app_->next_version()->set_version(_T("4.5.6.7"));
    app_->language_ = _T("de");
    app_->ap_ = _T("test ap");
    app_->tt_token_ = _T("test TT Token");
    app_->iid_ =
        StringToGuid(_T("{64333341-CA93-490d-9FB7-7FC5728721F4}"));
    app_->brand_code_ = _T("g00g");
    app_->client_id_ = _T("myclient");
    app_->referral_id_ = _T("somereferrer");
    app_->set_days_since_last_active_ping(-1);
    app_->set_days_since_last_roll_call(-1);
    EXPECT_SUCCEEDED(app_->put_isEulaAccepted(VARIANT_TRUE));

    __mutexScope(AppManager::Instance()->GetRegistryStableStateLock());

    app_manager_->PersistSuccessfulInstall(*app_);

    const uint32 now = Time64ToInt32(GetCurrent100NSTime());

    // Validate the results.
    CString client_state_key_name = AppendRegKeyPath(
        ConfigManager::Instance()->registry_client_state(is_machine_),
        kGuid1);
    RegKey client_state_key;
    EXPECT_SUCCEEDED(client_state_key.Create(client_state_key_name));

    ValidateClientStateMedium(is_machine_, kGuid1);

    // Check version is based on app_ and not read from Clients key.
    CString client_state_version;
    EXPECT_SUCCEEDED(client_state_key.GetValue(kRegValueProductVersion,
                                               &client_state_version));
    EXPECT_STREQ(_T("4.5.6.7"), app_->next_version()->version());
    EXPECT_STREQ(_T("4.5.6.7"), client_state_version);

    // Check language is based on app_ and not read from Clients key.
    CString language;
    EXPECT_SUCCEEDED(client_state_key.GetValue(kRegValueLanguage, &language));
    EXPECT_STREQ(_T("de"), app_->language_);
    EXPECT_STREQ(_T("de"), language);

    // Check iid is set correctly in ClientState.
    CString iid;
    EXPECT_SUCCEEDED(client_state_key.GetValue(kRegValueInstallationId, &iid));
    EXPECT_STREQ(_T("{64333341-CA93-490D-9FB7-7FC5728721F4}"),
                 GuidToString(app_->iid_));
    EXPECT_STREQ(_T("{64333341-CA93-490D-9FB7-7FC5728721F4}"), iid);

    DWORD last_successful_check(0);
    EXPECT_SUCCEEDED(
        client_state_key.GetValue(kRegValueLastSuccessfulCheckSec,
                                  &last_successful_check));
    EXPECT_GE(now, last_successful_check);
    EXPECT_GE(static_cast<uint32>(200), now - last_successful_check);

    // Check other values were not written.
    EXPECT_EQ(4, client_state_key.GetValueCount());
    EXPECT_FALSE(client_state_key.HasValue(kRegValueAdditionalParams));
    EXPECT_FALSE(client_state_key.HasValue(kRegValueBrandCode));
    EXPECT_FALSE(client_state_key.HasValue(kRegValueBrowser));
    EXPECT_FALSE(client_state_key.HasValue(kRegValueClientId));
    EXPECT_FALSE(client_state_key.HasValue(kRegValueDidRun));
    EXPECT_FALSE(client_state_key.HasValue(kRegValueOemInstall));
    EXPECT_FALSE(client_state_key.HasValue(kRegValueReferralId));
    EXPECT_FALSE(client_state_key.HasValue(kRegValueEulaAccepted));
    EXPECT_FALSE(client_state_key.HasValue(kRegValueUsageStats));
    EXPECT_FALSE(client_state_key.HasValue(kRegValueInstallTimeSec));
    EXPECT_FALSE(client_state_key.HasValue(kRegValueDayOfInstall));
    EXPECT_FALSE(client_state_key.HasValue(kRegValueLastUpdateTimeSec));
    EXPECT_FALSE(client_state_key.HasValue(kRegValueTTToken));
    EXPECT_FALSE(client_state_key.HasValue(kRegValueActivePingDayStartSec));
    EXPECT_FALSE(client_state_key.HasValue(kRegValueRollCallDayStartSec));
    EXPECT_FALSE(client_state_key.HasValue(kRegValueDayOfLastActivity));
    EXPECT_FALSE(client_state_key.HasValue(kRegValueDayOfLastRollCall));

    EXPECT_TRUE(app_registry_utils::IsAppEulaAccepted(is_machine_,
                                                      kGuid1,
                                                      false));
  }

  void WriteStateValueTest() {
    app_->iid_ =
        StringToGuid(_T("{64333341-CA93-490d-9FB7-7FC5728721F4}"));
    EXPECT_SUCCEEDED(
        app_manager_->ResetCurrentStateKey(app_->app_guid_string()));

    const CurrentState expected_state_value = STATE_ERROR;
    app_manager_->WriteStateValue(*app_, expected_state_value);

    const CString current_state_key_name(
        app_manager_->GetCurrentStateKeyName(app_->app_guid_string()));

    DWORD state_value = STATE_INIT;
    EXPECT_SUCCEEDED(RegKey::GetValue(current_state_key_name,
                                      kRegValueStateValue,
                                      &state_value));
    EXPECT_EQ(expected_state_value, state_value);
  }

  void WriteDownloadProgressTest() {
    app_->iid_ =
        StringToGuid(_T("{64333341-CA93-490d-9FB7-7FC5728721F4}"));
    EXPECT_SUCCEEDED(
        app_manager_->ResetCurrentStateKey(app_->app_guid_string()));

    const uint64 expected_bytes_downloaded = 10;
    const uint64 expected_bytes_total = 100;
    const LONG expected_download_time_remaining_ms = 300;
    LONG expected_download_progress_percentage =
        static_cast<LONG>(100ULL *
                          expected_bytes_downloaded / expected_bytes_total);

    app_manager_->WriteDownloadProgress(*app_,
                                        expected_bytes_downloaded,
                                        expected_bytes_total,
                                        expected_download_time_remaining_ms);

    RegKey current_state_key;
    ASSERT_SUCCEEDED(current_state_key.Open(
        app_manager_->GetCurrentStateKeyName(app_->app_guid_string())));

    DWORD download_progress_percentage = 0;
    DWORD download_time_remaining_ms = 0;

    EXPECT_SUCCEEDED(current_state_key.GetValue(
        kRegValueDownloadTimeRemainingMs, &download_time_remaining_ms));
    EXPECT_EQ(expected_download_time_remaining_ms, download_time_remaining_ms);

    EXPECT_SUCCEEDED(current_state_key.GetValue(
        kRegValueDownloadProgressPercent, &download_progress_percentage));
    EXPECT_EQ(expected_download_progress_percentage,
              download_progress_percentage);
  }

  void WriteInstallProgressTest() {
    app_->iid_ =
        StringToGuid(_T("{64333341-CA93-490d-9FB7-7FC5728721F4}"));
    EXPECT_SUCCEEDED(
        app_manager_->ResetCurrentStateKey(app_->app_guid_string()));

    const LONG expected_install_time_remaining_ms = 600;
    LONG expected_install_progress_percentage = 30;

    app_manager_->WriteInstallProgress(*app_,
                                       expected_install_progress_percentage,
                                       expected_install_time_remaining_ms);

    RegKey current_state_key;
    ASSERT_SUCCEEDED(current_state_key.Open(
        app_manager_->GetCurrentStateKeyName(app_->app_guid_string())));

    DWORD install_progress_percentage = 0;
    DWORD install_time_remaining_ms = 0;

    EXPECT_SUCCEEDED(current_state_key.GetValue(
        kRegValueInstallTimeRemainingMs, &install_time_remaining_ms));
    EXPECT_EQ(expected_install_time_remaining_ms, install_time_remaining_ms);

    EXPECT_SUCCEEDED(current_state_key.GetValue(
        kRegValueInstallProgressPercent, &install_progress_percentage));
    EXPECT_EQ(expected_install_progress_percentage,
              install_progress_percentage);
  }

  void PersistUpdateCheckSuccessfullySent_AllUpdated(bool was_active) {
    const CString client_state_key_name = AppendRegKeyPath(
        ConfigManager::Instance()->registry_client_state(is_machine_),
        kGuid1);

    // Create the test data.
    App* expected_app = CreateAppForRegistryPopulation(kGuid1);
    expected_app->current_version()->set_version(_T("1.0.0.0"));
    expected_app->iid_ = StringToGuid(kIid1);
    expected_app->did_run_ = was_active ? ACTIVE_RUN : ACTIVE_UNKNOWN;
    // Set non-zero values for activities so that the registry values can
    // be updated.
    expected_app->set_days_since_last_active_ping(4);
    expected_app->set_days_since_last_roll_call(2);
    CreateAppRegistryState(*expected_app, is_machine_, _T(""), true);

    __mutexScope(AppManager::Instance()->GetRegistryStableStateLock());

    const int elpased_days_since_datum = kMinDaysSinceDatum + 55;
    __mutexBlock(app_->model()->lock()) {
      EXPECT_SUCCEEDED(app_manager_->ReadAppPersistentData(app_));

      // We only want to make sure the timestamps in the registry are updated
      // and don't care about time_since_midnight_sec here, so just pass a 0 as
      // the first parameter.
      EXPECT_SUCCEEDED(
          app_manager_->PersistUpdateCheckSuccessfullySent(
              *app_, elpased_days_since_datum, 0));
    }

    // Validate the results.
    RegKey client_state_key;
    EXPECT_SUCCEEDED(client_state_key.Open(client_state_key_name));

    CString iid;
    if (was_active) {
      // Installation id should be cleared.
      EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
                client_state_key.GetValue(kRegValueInstallationId, &iid));
    } else {
      // did_run is unknown so installation id should still exist.
      EXPECT_SUCCEEDED(
          client_state_key.GetValue(kRegValueInstallationId, &iid));
      EXPECT_STREQ(kIid1, iid);
    }

    // Check ping timestamps are updated.
    const uint32 now = Time64ToInt32(GetCurrent100NSTime());

    DWORD last_active_ping_day_start_sec = 0;
    EXPECT_SUCCEEDED(client_state_key.GetValue(kRegValueActivePingDayStartSec,
        &last_active_ping_day_start_sec));
    if (was_active) {
      EXPECT_GE(now, last_active_ping_day_start_sec);
      EXPECT_LE(now, last_active_ping_day_start_sec + kMaxTimeSinceMidnightSec);
    } else {
      EXPECT_LE(last_active_ping_day_start_sec,
          now - expected_app->days_since_last_active_ping() * kSecondsPerDay);
    }

    DWORD last_roll_call_day_start_sec = 0;
    EXPECT_SUCCEEDED(client_state_key.GetValue(kRegValueRollCallDayStartSec,
        &last_roll_call_day_start_sec));
    EXPECT_GE(now, last_roll_call_day_start_sec);
    EXPECT_LE(now, last_roll_call_day_start_sec + kMaxTimeSinceMidnightSec);

    DWORD day_of_last_activity = 0;
    if (was_active) {
      EXPECT_SUCCEEDED(client_state_key.GetValue(kRegValueDayOfLastActivity,
          &day_of_last_activity));
      EXPECT_EQ(elpased_days_since_datum, day_of_last_activity);
    } else {
      EXPECT_FAILED(client_state_key.GetValue(kRegValueDayOfLastActivity,
          &day_of_last_activity));
    }

    DWORD day_of_last_roll_call = 0;
    EXPECT_SUCCEEDED(client_state_key.GetValue(kRegValueDayOfLastRollCall,
        &day_of_last_roll_call));
    EXPECT_EQ(elpased_days_since_datum, day_of_last_roll_call);

    // Check did_run is cleared.
    if (was_active) {
      CString did_run;
      EXPECT_SUCCEEDED(client_state_key.GetValue(kRegValueDidRun, &did_run));
      EXPECT_STREQ(_T("0"), did_run);
    } else {
      EXPECT_EQ(false, client_state_key.HasValue(kRegValueDidRun));
    }

    // Check that members in app_ are not changed.
    EXPECT_TRUE(expected_app->iid() == app_->iid());
    EXPECT_EQ(expected_app->days_since_last_active_ping(),
              app_->days_since_last_active_ping());
    EXPECT_EQ(expected_app->days_since_last_roll_call(),
              app_->days_since_last_roll_call());
    EXPECT_EQ(expected_app->did_run(), app_->did_run());
    EXPECT_EQ(expected_app->day_of_last_activity(),
              app_->day_of_last_activity());
    EXPECT_EQ(expected_app->day_of_last_roll_call(),
              app_->day_of_last_roll_call());
  }

  void PersistUpdateCheckSuccessfullySent_NotRun() {
    const CString client_state_key_name = AppendRegKeyPath(
        ConfigManager::Instance()->registry_client_state(is_machine_),
        kGuid1);
    const int kDaysSinceLastActivePing = 2;

    // Create the test data.
    App* expected_app = CreateAppForRegistryPopulation(kGuid1);
    expected_app->current_version()->set_version(_T("1.0.0.0"));
    expected_app->iid_ = StringToGuid(kIid1);
    expected_app->did_run_ = ACTIVE_NOTRUN;
    expected_app->set_days_since_last_active_ping(kDaysSinceLastActivePing);
    expected_app->set_days_since_last_roll_call(0);
    expected_app->set_day_of_last_activity(kMinDaysSinceDatum + 13);
    expected_app->set_day_of_last_roll_call(kMinDaysSinceDatum + 15);
    CreateAppRegistryState(*expected_app, is_machine_, _T(""), true);

    // Choose a time that is close to current time but with some skew so that
    // if the registry is rewritten, we won't write the same value again and
    // the change would be detected.
    const uint32 base_time = Time64ToInt32(GetCurrent100NSTime()) - 2;

    RegKey client_state_key;
    EXPECT_SUCCEEDED(client_state_key.Open(client_state_key_name));
    const uint32 last_active_time =
        base_time - kDaysSinceLastActivePing * kSecondsPerDay;
    ASSERT_SUCCEEDED(client_state_key.SetValue(
        kRegValueActivePingDayStartSec,
        static_cast<DWORD>(last_active_time)));

    ASSERT_SUCCEEDED(client_state_key.SetValue(
        kRegValueRollCallDayStartSec,
        static_cast<DWORD>(base_time)));

    __mutexScope(AppManager::Instance()->GetRegistryStableStateLock());

    const int elpased_days_since_datum = kMinDaysSinceDatum + 55;
    __mutexBlock(app_->model()->lock()) {
      EXPECT_SUCCEEDED(app_manager_->ReadAppPersistentData(app_));

      // We only want to make sure the timestamps in the registry are updated
      // and don't care about time_since_midnight_sec here, so just pass a 0
      // for elapsed_seconds_since_mid_night.
      EXPECT_SUCCEEDED(
          app_manager_->PersistUpdateCheckSuccessfullySent(
              *app_, elpased_days_since_datum, 0));
    }

    // Validate the results.

    // did_run is false so installation id should still exist.
    CString iid;
    EXPECT_SUCCEEDED(client_state_key.GetValue(kRegValueInstallationId, &iid));
    EXPECT_STREQ(kIid1, iid);

    // did_run is false so active ping timestamp should not be updated.
    DWORD last_active_ping_day_start_sec = 0;
    EXPECT_SUCCEEDED(client_state_key.GetValue(kRegValueActivePingDayStartSec,
        &last_active_ping_day_start_sec));
    EXPECT_EQ(last_active_time, last_active_ping_day_start_sec);

    // Previous days_since_last_roll_call is 0 so that timestamp should
    // not change.
    DWORD last_roll_call_day_start_sec = 0;
    EXPECT_SUCCEEDED(client_state_key.GetValue(kRegValueRollCallDayStartSec,
        &last_roll_call_day_start_sec));
    EXPECT_EQ(base_time, last_roll_call_day_start_sec);

    // did_run is false so day_of_last_activity should not be updated.
    DWORD day_of_last_activity = 0;
    EXPECT_SUCCEEDED(client_state_key.GetValue(kRegValueDayOfLastActivity,
        &day_of_last_activity));
    EXPECT_EQ(expected_app->day_of_last_activity(), day_of_last_activity);

    // Day of last roll call will be updated.
    DWORD day_of_last_roll_call = 0;
    EXPECT_SUCCEEDED(client_state_key.GetValue(kRegValueDayOfLastRollCall,
        &day_of_last_roll_call));
    EXPECT_EQ(elpased_days_since_datum, day_of_last_roll_call);

    // did_run is still not set.
    CString did_run;
    EXPECT_SUCCEEDED(client_state_key.GetValue(kRegValueDidRun, &did_run));
    EXPECT_STREQ(_T("0"), did_run);

    // Checks that members in app_ are not changed.
    EXPECT_TRUE(expected_app->iid() == app_->iid());
    EXPECT_EQ(expected_app->days_since_last_active_ping(),
              app_->days_since_last_active_ping());
    EXPECT_EQ(expected_app->days_since_last_roll_call(),
              app_->days_since_last_roll_call());
    EXPECT_EQ(expected_app->day_of_last_activity(),
              app_->day_of_last_activity());
    EXPECT_EQ(expected_app->day_of_last_roll_call(),
              app_->day_of_last_roll_call());
    EXPECT_EQ(expected_app->did_run(), app_->did_run());
  }

  void PersistUpdateCheckSuccessfullySent_NoPreviousPing(bool was_active) {
    const CString client_state_key_name = AppendRegKeyPath(
        ConfigManager::Instance()->registry_client_state(is_machine_),
        kGuid1);
    const int kDaysSinceLastActivePing = 2;

    // Create the test data.
    App* expected_app = CreateAppForRegistryPopulation(kGuid1);
    expected_app->current_version()->set_version(_T("1.0.0.0"));
    expected_app->iid_ = StringToGuid(kIid1);
    expected_app->did_run_ = was_active ? ACTIVE_RUN : ACTIVE_UNKNOWN;
    expected_app->set_days_since_last_active_ping(-1);
    expected_app->set_days_since_last_roll_call(-1);
    expected_app->set_day_of_last_activity(-1);
    expected_app->set_day_of_last_roll_call(-1);
    CreateAppRegistryState(*expected_app, is_machine_, _T(""), true);

    __mutexScope(AppManager::Instance()->GetRegistryStableStateLock());

    const int elpased_days_since_datum = kMinDaysSinceDatum + 55;
    __mutexBlock(app_->model()->lock()) {
      EXPECT_SUCCEEDED(app_manager_->ReadAppPersistentData(app_));

      // We only want to make sure the timestamps in the registry are updated
      // and don't care about time_since_midnight_sec here, so just pass a 0
      // for elapsed_seconds_since_mid_night.
      EXPECT_SUCCEEDED(
          app_manager_->PersistUpdateCheckSuccessfullySent(
              *app_, elpased_days_since_datum, 0));
    }

    // Validate the results.
    RegKey client_state_key;
    EXPECT_SUCCEEDED(client_state_key.Open(client_state_key_name));

    CString iid;
    if (was_active) {
      // Installation id should be cleared.
      EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
                client_state_key.GetValue(kRegValueInstallationId, &iid));
    } else {
      // did_run is unknown so installation id should still exist.
      EXPECT_SUCCEEDED(
          client_state_key.GetValue(kRegValueInstallationId, &iid));
      EXPECT_STREQ(kIid1, iid);
    }

    const uint32 now = Time64ToInt32(GetCurrent100NSTime());

    if (was_active) {
      DWORD last_active_day_start_sec = 0;
      EXPECT_SUCCEEDED(client_state_key.GetValue(kRegValueRollCallDayStartSec,
          &last_active_day_start_sec));
      EXPECT_GE(now, last_active_day_start_sec);
      EXPECT_LE(now, last_active_day_start_sec + kMaxTimeSinceMidnightSec);
    } else {
      // did_run is unknown so active ping timestamp should not be updated.
      EXPECT_FALSE(client_state_key.HasValue(kRegValueActivePingDayStartSec));
    }

    DWORD last_roll_call_day_start_sec = 0;
    EXPECT_SUCCEEDED(client_state_key.GetValue(kRegValueRollCallDayStartSec,
        &last_roll_call_day_start_sec));
    EXPECT_GE(now, last_roll_call_day_start_sec);
    EXPECT_LE(now, last_roll_call_day_start_sec + kMaxTimeSinceMidnightSec);

    DWORD day_of_last_activity = 0;
    if (was_active) {
      EXPECT_SUCCEEDED(client_state_key.GetValue(kRegValueDayOfLastActivity,
          &day_of_last_activity));
      EXPECT_EQ(elpased_days_since_datum, day_of_last_activity);
    } else {
      // did_run is unknown so day_of_last_activity should not be updated.
      EXPECT_FAILED(client_state_key.GetValue(kRegValueDayOfLastActivity,
          &day_of_last_activity));
    }

    // Day of last roll call will be updated.
    DWORD day_of_last_roll_call = 0;
    EXPECT_SUCCEEDED(client_state_key.GetValue(kRegValueDayOfLastRollCall,
        &day_of_last_roll_call));
    EXPECT_EQ(elpased_days_since_datum, day_of_last_roll_call);

    EXPECT_EQ(was_active, client_state_key.HasValue(kRegValueDidRun));

    // Checks that members in app_ are not changed.
    EXPECT_TRUE(expected_app->iid() == app_->iid());
    EXPECT_EQ(expected_app->days_since_last_active_ping(),
              app_->days_since_last_active_ping());
    EXPECT_EQ(expected_app->days_since_last_roll_call(),
              app_->days_since_last_roll_call());
    EXPECT_EQ(expected_app->day_of_last_activity(),
              app_->day_of_last_activity());
    EXPECT_EQ(expected_app->day_of_last_roll_call(),
              app_->day_of_last_roll_call());
    EXPECT_EQ(expected_app->did_run(), app_->did_run());
  }

  void SynchronizeClientStateTest(const CString& app_id) {
    const CString client_state_key_name = AppendRegKeyPath(
        ConfigManager::Instance()->registry_client_state(is_machine_),
        app_id);

    App* expected_app = CreateAppForRegistryPopulation(app_id);
    PopulateExpectedApp1(expected_app);
    expected_app->referral_id_ = _T("referrer");
    EXPECT_SUCCEEDED(expected_app->put_isEulaAccepted(VARIANT_FALSE));
    expected_app->install_time_diff_sec_ = 141516;
    expected_app->day_of_install_ = 10001;
    CreateAppRegistryState(*expected_app, is_machine_, _T(""), true);

    EXPECT_TRUE(RegKey::HasValue(client_state_key_name, _T("referral")));

    __mutexScope(AppManager::Instance()->GetRegistryStableStateLock());

    App* app = NULL;
    EXPECT_SUCCEEDED(app_bundle_->createApp(CComBSTR(app_id), &app));
    ASSERT_TRUE(app);

    __mutexBlock(app_->model()->lock()) {
      EXPECT_SUCCEEDED(app_manager_->ReadAppPersistentData(app));
    }

    EXPECT_TRUE(app->referral_id_.IsEmpty());

    __mutexBlock(app_->model()->lock()) {
      EXPECT_SUCCEEDED(app_manager_->SynchronizeClientState(app->app_guid()));
    }

    EXPECT_TRUE(app->referral_id_.IsEmpty());

    const uint32 now = Time64ToInt32(GetCurrent100NSTime());

    EXPECT_TRUE(app->referral_id_.IsEmpty());

    // Validate the results.
    RegKey client_state_key;
    EXPECT_SUCCEEDED(client_state_key.Open(client_state_key_name));

    // Check version and language have been copied to client state.
    EXPECT_STREQ(expected_app->current_version()->version(),
                 GetSzValue(client_state_key_name, kRegValueProductVersion));
    EXPECT_STREQ(expected_app->language_,
                 GetSzValue(client_state_key_name, kRegValueLanguage));

    // Check that ap, brand_code, and client_id, etc. are not changed.
    EXPECT_STREQ(expected_app->ap_,
                 GetSzValue(client_state_key_name, kRegValueAdditionalParams));
    EXPECT_STREQ(expected_app->tt_token_,
                 GetSzValue(client_state_key_name, kRegValueTTToken));
    EXPECT_STREQ(expected_app->brand_code_,
                 GetSzValue(client_state_key_name, kRegValueBrandCode));
    EXPECT_STREQ(expected_app->client_id_,
                 GetSzValue(client_state_key_name, kRegValueClientId));

    // install_time_diff_sec should be roughly the same as now - installed.
    const DWORD install_time =
        GetDwordValue(client_state_key_name, kRegValueInstallTimeSec);
    const DWORD calculated_install_diff = now - install_time;
    EXPECT_GE(calculated_install_diff, expected_app->install_time_diff_sec_);
    EXPECT_GE(static_cast<uint32>(500),
              calculated_install_diff - expected_app->install_time_diff_sec_);

    const DWORD day_of_install =
        GetDwordValue(client_state_key_name, kRegValueDayOfInstall);
    EXPECT_EQ(10001, day_of_install);

    EXPECT_EQ(0, GetDwordValue(client_state_key_name, kRegValueEulaAccepted));
    EXPECT_FALSE(expected_app->is_eula_accepted_);
  }

  void WritePreInstallDataTest(App* app, bool test_clearing_values) {
    ASSERT1(is_machine_ == app->app_bundle()->is_machine());
    const CString clients_key_name = AppendRegKeyPath(
        ConfigManager::Instance()->registry_clients(is_machine_), kGuid1);
    const CString client_state_key_name = AppendRegKeyPath(
        ConfigManager::Instance()->registry_client_state(is_machine_),
        kGuid1);

    const bool expect_has_client_key = RegKey::HasKey(clients_key_name);

    // Populate the test data.
    app->brand_code_ = _T("GGLG");
    app->client_id_ = _T("someclient");
    app->referral_id_ = _T("referrer");
    app->install_time_diff_sec_ = 657812;   // Not used.
    app->usage_stats_enable_ = TRISTATE_TRUE;
    app->browser_type_ = BROWSER_FIREFOX;
    app->ap_ = _T("test_ap");
    app->language_ = _T("en");

    if (test_clearing_values) {
      // Set values in registry and clear them in the app.
      EXPECT_SUCCEEDED(RegKey::SetValue(
                           kGuid1ClientsKeyPathUser,
                           kRegValueBrowser,
                           static_cast<DWORD>(app->browser_type())));
      EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientsKeyPathUser,
                                        kRegValueAdditionalParams,
                                        app->ap()));

      app->browser_type_ = BROWSER_UNKNOWN;
      app->ap_.Empty();
    }

    const TCHAR* const kExpiredExperimentLabel =
        _T("omaha=v3_23_9_int|Fri, 14 Mar 2014 23:36:18 GMT");
    const TCHAR* const kValidExperimentLabel =
        _T("omaha=v3_23_9_int|Wed, 14 Mar 2029 23:36:18 GMT");

    CString experiment_labels;
    experiment_labels.Format(_T("%s;%s"),
                             kExpiredExperimentLabel, kValidExperimentLabel);
    EXPECT_SUCCEEDED(RegKey::SetValue(
      app_registry_utils::GetAppClientStateKey(is_machine_, kGuid1),
      kRegValueExperimentLabels,
      experiment_labels));

    __mutexScope(AppManager::Instance()->GetRegistryStableStateLock());

    EXPECT_SUCCEEDED(app_manager_->WritePreInstallData(*app));
    const uint32 now = Time64ToInt32(GetCurrent100NSTime());

    // Validate the results.

    // WritePreInstallData should never write to client_key, so it shouldn't
    // exist if it did not before the function call.
    EXPECT_EQ(expect_has_client_key, RegKey::HasKey(clients_key_name));

    // ClientStateKey should exist.
    RegKey client_state_key;
    EXPECT_SUCCEEDED(client_state_key.Open(client_state_key_name));

    ValidateClientStateMedium(is_machine_, kGuid1);

    CString brand_code;
    EXPECT_SUCCEEDED(client_state_key.GetValue(kRegValueBrandCode,
                                               &brand_code));
    EXPECT_STREQ(_T("GGLG"), brand_code);

    CString client_id;
    EXPECT_SUCCEEDED(client_state_key.GetValue(kRegValueClientId, &client_id));
    EXPECT_STREQ(_T("someclient"), client_id);

    CString referral_id;
    EXPECT_SUCCEEDED(client_state_key.GetValue(kRegValueReferralId,
                                               &referral_id));
    EXPECT_STREQ(_T("referrer"), referral_id);

    DWORD install_time(0);
    EXPECT_SUCCEEDED(client_state_key.GetValue(kRegValueInstallTimeSec,
                                               &install_time));
    EXPECT_GE(now, install_time);
    EXPECT_GE(static_cast<uint32>(200), now - install_time);

    DWORD day_of_install(0);
    EXPECT_SUCCEEDED(client_state_key.GetValue(kRegValueDayOfInstall,
                                               &day_of_install));
    EXPECT_EQ(static_cast<DWORD>(-1), day_of_install);

    DWORD usage_stats_enable = 0;
    EXPECT_SUCCEEDED(client_state_key.GetValue(_T("usagestats"),
                                               &usage_stats_enable));
    EXPECT_EQ(TRISTATE_TRUE, usage_stats_enable);

    if (test_clearing_values) {
      EXPECT_FALSE(client_state_key.HasValue(kRegValueBrowser));
    } else {
      DWORD browser_type = 0;
      EXPECT_SUCCEEDED(client_state_key.GetValue(kRegValueBrowser,
                                                 &browser_type));
      EXPECT_EQ(BROWSER_FIREFOX, browser_type);
    }

    if (test_clearing_values) {
      EXPECT_FALSE(client_state_key.HasValue(kRegValueAdditionalParams));
    } else {
      CString ap;
      EXPECT_SUCCEEDED(client_state_key.GetValue(kRegValueAdditionalParams,
                                                 &ap));
      EXPECT_STREQ(_T("test_ap"), ap);
    }

    CString lang;
    EXPECT_SUCCEEDED(client_state_key.GetValue(kRegValueLanguage, &lang));
    EXPECT_STREQ(_T("en"), lang);

    // Version should not be written to clientstate by WritePreInstallData().
    EXPECT_FALSE(RegKey::HasValue(client_state_key_name,
                                  kRegValueProductVersion));

    EXPECT_STREQ(kValidExperimentLabel, app->GetExperimentLabels());
  }

  static void ValidateClientStateMedium(bool is_machine,
                                        const CString& app_guid) {
    const CString client_state_medium_key_name = AppendRegKeyPath(
        ConfigManager::Instance()->machine_registry_client_state_medium(),
        app_guid);
    if (is_machine) {
      RegKey client_state_medium_key;
      ASSERT_SUCCEEDED(
          client_state_medium_key.Open(client_state_medium_key_name));
      EXPECT_EQ(0, client_state_medium_key.GetValueCount());
    } else {
      EXPECT_FALSE(RegKey::HasKey(client_state_medium_key_name));
      // There is no such thing as a user ClientStateMedium key.
      const CString user_client_state_medium_key_name = AppendRegKeyPath(
          USER_KEY GOOPDATE_REG_RELATIVE_CLIENT_STATE_MEDIUM,
          app_guid);
      EXPECT_FALSE(RegKey::HasKey(user_client_state_medium_key_name));
      return;
    }
  }

  // Uses SetupGoogleUpdate to create the ClientStateMedium key with the
  // appropriate permissions. Used to test that the permissions are inherited.
  void CreateClientStateMediumKey() {
    SetupGoogleUpdate setup_google_update(true, false);

    // On Windows 7, AddAllowedAce() can fail if the registry is redirected. So
    // we ignore errors from this call.
    setup_google_update.CreateClientStateMedium();
  }

  struct AttributePair {
    CString attribute_name;
    CString attribute_value;
  };

  struct SumValuePair {
    CString name;
    DWORD value;
  };

  struct AttributeSubkeyPair {
    CString attribute_name;
    SumValuePair* sum_values;
    size_t number_of_sum_values;
  };

  void CreateExpectedAppDefinedAttributes(bool is_machine,
                                          AttributePair pairs[],
                                          size_t number_of_pairs,
                                          AttributeSubkeyPair subkey_pairs[],
                                          size_t number_of_subkey_pairs,
                                          App** expected_app) {
    *expected_app = CreateAppForRegistryPopulation(kGuid1);
    PopulateExpectedApp1(*expected_app);
    CreateAppRegistryState(**expected_app, is_machine, _T("1.0.0.0"), true);
    EXPECT_SUCCEEDED((*expected_app)->put_isEulaAccepted(VARIANT_TRUE));

    std::vector<StringPair> attributes;

    for (size_t i = 0; i < number_of_pairs; ++i) {
      attributes.push_back(std::make_pair(pairs[i].attribute_name.MakeLower(),
                                          pairs[i].attribute_value));
    }

    for (size_t i = 0; i < number_of_subkey_pairs; ++i) {
      DWORD sum = 0;
      for (size_t j = 0; j < subkey_pairs[i].number_of_sum_values; ++j) {
        sum += subkey_pairs[i].sum_values[j].value;
      }

      CString sum_string;
      sum_string.Format(_T("%u"), sum);
      attributes.push_back(
          std::make_pair(subkey_pairs[i].attribute_name.MakeLower(),
                         sum_string));
    }

    (*expected_app)->app_defined_attributes_ = attributes;
  }

  void WriteAppDefinedAttributesToRegistry(bool is_machine,
                                           AttributePair pairs[],
                                           size_t number_of_pairs,
                                           AttributeSubkeyPair subkey_pairs[],
                                           size_t number_of_subkey_pairs) {
    const CString client_state_medium_key_name = AppendRegKeyPath(
        ConfigManager::Instance()->machine_registry_client_state_medium(),
        kGuid1);
    CString client_state_key_name = AppendRegKeyPath(
        ConfigManager::Instance()->registry_client_state(is_machine),
        kGuid1);

    CString key_name(is_machine ? client_state_medium_key_name :
                                  client_state_key_name);

    for (size_t i = 0; i < number_of_pairs; ++i) {
      EXPECT_SUCCEEDED(RegKey::SetValue(key_name,
                                        pairs[i].attribute_name,
                                        pairs[i].attribute_value));
    }

    for (size_t i = 0; i < number_of_subkey_pairs; ++i) {
      const CString subkey_name =
          AppendRegKeyPath(key_name, subkey_pairs[i].attribute_name);
      EXPECT_SUCCEEDED(RegKey::SetValue(subkey_name,
                                        kRegValueAppDefinedAggregate,
                                        kRegValueAppDefinedAggregateSum));

      for (size_t j = 0; j < subkey_pairs[i].number_of_sum_values; ++j) {
        EXPECT_SUCCEEDED(
            RegKey::SetValue(subkey_name,
                             subkey_pairs[i].sum_values[j].name,
                             subkey_pairs[i].sum_values[j].value));
      }
    }
  }

  void CreateExpectedCohortApp(bool is_machine,
                               const CString& cohort,
                               const CString& hint,
                               const CString& name,
                               App** expected_app) {
    *expected_app = CreateAppForRegistryPopulation(kGuid1);
    PopulateExpectedApp1(*expected_app);
    CreateAppRegistryState(**expected_app, is_machine, _T("1.0.0.0"), true);
    EXPECT_SUCCEEDED((*expected_app)->put_isEulaAccepted(VARIANT_TRUE));

    (*expected_app)->cohort_.cohort = cohort;
    (*expected_app)->cohort_.hint = hint;
    (*expected_app)->cohort_.name = name;
  }

  void WriteCohortToRegistry(bool is_machine,
                             const CString& cohort,
                             const CString& hint,
                             const CString& name) {
    CString client_state_key_name = AppendRegKeyPath(
        ConfigManager::Instance()->registry_client_state(is_machine),
        kGuid1);
    CString cohort_key_name(
        AppendRegKeyPath(client_state_key_name, kRegSubkeyCohort));

    RegKey cohort_key;
    ASSERT_SUCCEEDED(cohort_key.Create(cohort_key_name));
    EXPECT_SUCCEEDED(cohort_key.SetValue(NULL, cohort));
    EXPECT_SUCCEEDED(cohort_key.SetValue(kRegValueCohortHint, hint));
    EXPECT_SUCCEEDED(cohort_key.SetValue(kRegValueCohortName, name));
  }

  void DeleteCohortKey(bool is_machine) {
    CString client_state_key_name = AppendRegKeyPath(
        ConfigManager::Instance()->registry_client_state(is_machine),
        kGuid1);
    CString cohort_key_name(
        AppendRegKeyPath(client_state_key_name, kRegSubkeyCohort));

    RegKey::DeleteKey(cohort_key_name);
  }

  AppManager* app_manager_;
  App* app_;
  // A second bundle is necessary because the same bundle cannot have the same
  // app in it more than once and many of these tests create an app to populate
  // the registry and another to read it.
  std::shared_ptr<AppBundle> test_app_bundle_for_app_creation_;

  const GUID guid1_;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(AppManagerTestBase);
};

// For most tests, the EULA should be accepted.
class AppManagerMachineTest : public AppManagerTestBase {
 protected:
  AppManagerMachineTest() : AppManagerTestBase(true) {}
};

class AppManagerUserTest : public AppManagerTestBase {
 protected:
  AppManagerUserTest() : AppManagerTestBase(false) {}
};

// These fixtures are also used for ReadUninstalledAppPersistentData tests.
class AppManagerReadAppPersistentDataMachineTest
    : public AppManagerMachineTest {
};

class AppManagerReadAppPersistentDataUserTest
    : public AppManagerUserTest {
};

class AppManagerWithBundleTest : public AppManagerTestBase {
 public:
  explicit AppManagerWithBundleTest(bool is_machine)
      : AppManagerTestBase(is_machine) {
    // CoCreateInstance for registered hook CLSIDs returns ERROR_FILE_NOT_FOUND
    // instead of REGDB_E_CLASSNOTREG without this WMI hack. This has to be done
    // before the registry overriding that is done by the base class.
    WmiQuery wmi_query;
    EXPECT_SUCCEEDED(wmi_query.Connect(_T("root\\SecurityCenter")));
  }

  static void PopulateDataAndRegistryForRegisteredAndUnInstalledAppsTests(
    bool is_machine,
    App* expected_app0,
    App* expected_app1,
    App* expected_app2,
    App* opposite_hive_data1,
    App* opposite_hive_data2);

 protected:
  // Wrappers for static functions; simplifies callers using this test fixture.

  void PopulateDataAndRegistryForRegisteredAndUnInstalledAppsTests(
      bool is_machine,
      App** expected_app0,
      App** expected_app1,
      App** expected_app2) {
    *expected_app0 = CreateAppForRegistryPopulation(kGuid1);
    *expected_app1 = CreateAppForRegistryPopulation(kGuid2);
    *expected_app2 = CreateAppForRegistryPopulation(kGuid3);
    App* opposite_hive_data1 = CreateAppForRegistryPopulation(kGuid6);
    App* opposite_hive_data2 = CreateAppForRegistryPopulation(kGuid7);
    PopulateDataAndRegistryForRegisteredAndUnInstalledAppsTests(
        is_machine,
        *expected_app0,
        *expected_app1,
        *expected_app2,
        opposite_hive_data1,
        opposite_hive_data2);
  }

  void PopulateForRegistrationUpdateHookTests(
      bool is_machine,
      App** expected_app0,
      App** expected_app1,
      App** expected_app2,
      const CString& clsid_app0) {
    PopulateDataAndRegistryForRegisteredAndUnInstalledAppsTests(
        is_machine,
        expected_app0,
        expected_app1,
        expected_app2);

    CString clients_key_name = AppendRegKeyPath(
        ConfigManager::Instance()->registry_clients(is_machine),
        (*expected_app0)->app_guid_string());
    EXPECT_SUCCEEDED(RegKey::SetValue(clients_key_name,
                                      kRegValueUpdateHookClsid,
                                      clsid_app0));
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(AppManagerWithBundleTest);
};

class AppManagerWithBundleMachineTest : public AppManagerWithBundleTest {
 protected:
  AppManagerWithBundleMachineTest() : AppManagerWithBundleTest(true) {}
};

class AppManagerWithBundleUserTest : public AppManagerWithBundleTest {
 protected:
  AppManagerWithBundleUserTest() : AppManagerWithBundleTest(false) {}
};

class HoldAppManagerLock : public Runnable {
 public:
  explicit HoldAppManagerLock(bool is_machine, const int period)
      : period_(period) {
    reset(lock_acquired_event_, ::CreateEvent(NULL, false, false, NULL));
    reset(stop_event_, ::CreateEvent(NULL, false, false, NULL));

    InitializeAppManagerRegistryLock(is_machine, &lock_);
  }

  virtual void Run() {
    __mutexScope(lock_);

    EXPECT_TRUE(::SetEvent(get(lock_acquired_event_)));

    // TODO(omaha3): Could just use a sleep if we don't do more tests.
    EXPECT_EQ(WAIT_TIMEOUT, ::WaitForSingleObject(get(stop_event_), period_));
  }

  void Stop() {
    EXPECT_TRUE(::SetEvent(get(stop_event_)));
  }

  void WaitForLockToBeAcquired() {
    EXPECT_EQ(WAIT_OBJECT_0,
              ::WaitForSingleObject(get(lock_acquired_event_), 2000));
  }

 private:
  const int period_;
  GLock lock_;
  scoped_event lock_acquired_event_;
  scoped_event stop_event_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(HoldAppManagerLock);
};

// Provide access to the member functions for other tests without requiring them
// to know about the test fixture class.

void PopulateDataAndRegistryForRegisteredAndUnInstalledAppsTests(
    bool is_machine,
    App* expected_app0,
    App* expected_app1,
    App* expected_app2,
    App* opposite_hive_data1,
    App* opposite_hive_data2) {
  AppManagerWithBundleTest::
      PopulateDataAndRegistryForRegisteredAndUnInstalledAppsTests(
      is_machine,
      expected_app0,
      expected_app1,
      expected_app2,
      opposite_hive_data1,
      opposite_hive_data2);
}

void SetDisplayName(const CString& name, App* app) {
  AppManagerTestBase::SetDisplayName(name, app);
}

// Tests the ping freshness feature.
void PingFreshnessTest(bool is_machine) {
  const TCHAR* key_name = is_machine ?
      kGuid1ClientStateKeyPathMachine : kGuid1ClientStateKeyPathUser;
  EXPECT_FALSE(RegKey::HasValue(key_name, kRegValuePingFreshness));

  // The worker initialization creates its own instance of the AppManager.
  // Therefore, delete the singleton instance created by the unit test.
  AppManager::DeleteInstance();
  EXPECT_HRESULT_SUCCEEDED(Worker::Instance().Initialize(is_machine));
  AppManager* app_manager = AppManager::Instance();

  GUID app_guid = GUID_NULL;
  EXPECT_HRESULT_SUCCEEDED(StringToGuidSafe(
      _T("{21CD0965-0B0E-47cf-B421-2D191C16C0E2}"), &app_guid));

  std::unique_ptr<AppBundle> app_bundle;
  {
    // AppBundle code expects the model to be locked.
    __mutexScope(Worker::Instance().model()->lock());

    // Create an app bundle and an app. Test that every call of the function
    // PersistUpdateCheckSuccessfullySent results in a new ping freshness
    // value, which can be deserialized and read as a member of the app.
    app_bundle.reset(new AppBundle(is_machine, Worker::Instance().model()));
    App app(app_guid, true, app_bundle.get());
    EXPECT_HRESULT_SUCCEEDED(
        app_manager->PersistUpdateCheckSuccessfullySent(app,
                                                        kMinDaysSinceDatum,
                                                        0));
    EXPECT_HRESULT_SUCCEEDED(app_manager->ReadAppPersistentData(&app));
    CString ping_freshness1 = app.ping_freshness();
    EXPECT_FALSE(ping_freshness1.IsEmpty());

    EXPECT_HRESULT_SUCCEEDED(
        app_manager->PersistUpdateCheckSuccessfullySent(app,
                                                        kMinDaysSinceDatum,
                                                        0));
    EXPECT_HRESULT_SUCCEEDED(app_manager->ReadAppPersistentData(&app));
    CString ping_freshness2 = app.ping_freshness();
    EXPECT_FALSE(ping_freshness2.IsEmpty());

    EXPECT_STRNE(ping_freshness1, ping_freshness2);
  }
}

// TODO(omaha3): Maybe use this to test the similar code in install_apps.cc.
#if 0
TEST_F(AppManagerTest, ConvertCommandLineToProductData_Succeeds) {
  CommandLineAppArgs extra1;
  extra1.app_guid = guid1_;
  extra1.app_name = _T("foo");
  extra1.needs_admin = false;
  extra1.ap = _T("Test ap");
  extra1.tt_token = _T("Test TT Token");
  extra1.encoded_installer_data = _T("%20foobar");
  extra1.install_data_index = _T("foobar");

  CommandLineAppArgs extra2;
  extra2.app_guid = StringToGuid(kGuid2);
  extra2.app_name = _T("bar");
  extra2.needs_admin = true;    // This gets ignored.
  extra2.ap = _T("beta");
  extra2.tt_token = _T("beta TT Token");

  CommandLineAppArgs extra3;
  extra3.app_guid = StringToGuid(kGuid3);
  extra3.app_name = _T("bar");
  extra3.needs_admin = true;    // This gets ignored.
  extra3.ap = _T("beta");
  extra3.tt_token = _T("beta TT Token");

  CommandLineArgs args;
  args.is_interactive_set = true;  // Not used.
  args.is_machine_set = true;  // Not used.
  args.is_crash_handler_disabled = true;  // Not used.
  args.is_eula_required_set = true;
  args.is_eula_required_set = true;  // Not used.
  args.install_source = _T("one_click");
  args.code_red_metainstaller_path = _T("foo.exe");  // Not used.
  args.legacy_manifest_path = _T("bar.exe");  // Not used.
  args.crash_filename = _T("foo.dmp");  // Not used.
  args.extra.installation_id = StringToGuid(kIid1);
  args.extra.brand_code = _T("GOOG");
  args.extra.client_id = _T("someclient");
  args.extra.experiment = _T("exp1");
  args.extra.referral_id = _T("referrer1");
  args.extra.browser_type = BROWSER_IE;
  args.extra.language = _T("abc");
  args.extra.usage_stats_enable = TRISTATE_TRUE;
  args.extra.apps.push_back(extra1);
  args.extra.apps.push_back(extra2);
  args.extra.apps.push_back(extra3);

  AppData* expected_data1 = CreateAppData();
  PopulateExpectedAppData1(guid1_, false, &expected_data1);
  expected_data1.set_version(_T(""));  // Clear value.
  expected_data1.set_previous_version(_T(""));  // Clear value.
  expected_data1.set_did_run(ACTIVE_UNKNOWN);  // Clear value.
  expected_data1.set_display_name(_T("foo"));
  expected_data1.set_browser_type(BROWSER_IE);
  expected_data1.set_install_source(_T("one_click"));
  expected_data1.set_encoded_installer_data(_T("%20foobar"));
  expected_data1.set_install_data_index(_T("foobar"));
  expected_data1.set_usage_stats_enable(TRISTATE_TRUE);
  expected_data1.set_referral_id(_T("referrer1"));
  expected_data1.set_install_time_diff_sec(
      static_cast<uint32>(-1 * kSecondsPerDay));  // New install.
  expected_data1.set_is_eula_accepted(false);
  expected_data1.set_days_since_last_active_ping(0);
  expected_data1.set_days_since_last_roll_call(0);

  AppData* expected_data2 = CreateAppData();

  // Make the first app appear to already be installed but without an
  // InstallTime. This affects install_time_diff_sec.
  expected_data2.set_version(_T("4.5.6.7"));
  CreateAppRegistryState(expected_data2);

  PopulateExpectedAppData2(StringToGuid(kGuid2), false, &expected_data2);

  expected_data2.set_version(_T(""));  // Clear value.
  expected_data2.set_previous_version(_T(""));  // Clear value.
  expected_data2.set_did_run(ACTIVE_UNKNOWN);  // Clear value.
  expected_data2.set_language(_T("abc"));
  expected_data2.set_display_name(_T("bar"));
  expected_data2.set_browser_type(BROWSER_IE);
  expected_data2.set_install_source(_T("one_click"));
  expected_data2.set_usage_stats_enable(TRISTATE_TRUE);
  // Override unique expected data because the args apply to all apps.
  expected_data2.set_iid(StringToGuid(kIid1));
  expected_data2.set_brand_code(_T("GOOG"));
  expected_data2.set_client_id(_T("someclient"));
  expected_data2.set_experiment(_T("exp1"));
  expected_data2.set_referral_id(_T("referrer1"));
  expected_data2.set_install_time_diff_sec(0);  // InstallTime is unknown.
  expected_data2.set_is_eula_accepted(false);
  expected_data2.set_days_since_last_active_ping(0);
  expected_data2.set_days_since_last_roll_call(0);

  AppData expected_data3(StringToGuid(kGuid3), false);

  // Make the first app appear to already be installed with a valid InstallTime.
  // This affects install_time_diff_sec.
  expected_data3.set_version(_T("4.5.6.7"));
  expected_data3.set_install_time_diff_sec(123456);  // Known original time.
  CreateAppRegistryState(expected_data3);

  PopulateExpectedAppData2(&expected_data3);
  expected_data3.set_version(_T(""));  // Clear value.
  expected_data3.set_previous_version(_T(""));  // Clear value.
  expected_data3.set_did_run(AppData::ACTIVE_UNKNOWN);  // Clear value.
  expected_data3.set_language(_T("abc"));
  expected_data3.set_display_name(_T("bar"));
  expected_data3.set_browser_type(BROWSER_IE);
  expected_data3.set_install_source(_T("one_click"));
  expected_data3.set_usage_stats_enable(TRISTATE_TRUE);
  // Override unique expected data because the args apply to all apps.
  expected_data3.set_iid(StringToGuid(kIid1));
  expected_data3.set_brand_code(_T("GOOG"));
  expected_data3.set_client_id(_T("someclient"));
  expected_data3.set_referral_id(_T("referrer1"));
  expected_data3.set_is_eula_accepted(false);
  expected_data3.set_days_since_last_active_ping(0);
  expected_data3.set_days_since_last_roll_call(0);

  ProductDataVector products;
  AppManager app_manager(false);
  app_manager.ConvertCommandLineToProductData(args, &products);

  ASSERT_EQ(3, products.size());
  EXPECT_EQ(0, products[0].num_components());
  EXPECT_EQ(0, products[1].num_components());
  EXPECT_EQ(0, products[2].num_components());
  ValidateExpectedValues(expected_data1, products[0].app_data());
  ValidateExpectedValues(expected_data2, products[1].app_data());

  // install_time_diff_sec may be off by a second or so.
  const uint32 now = Time64ToInt32(GetCurrent100NSTime());
  EXPECT_GE(products[2].app_data().install_time_diff_sec(),
            expected_data3.install_time_diff_sec());
  EXPECT_GE(static_cast<uint32>(500),
            products[2].app_data().install_time_diff_sec() -
            expected_data3.install_time_diff_sec());
  // Fix up expected_data3 or it might fail verification.
  expected_data3.set_install_time_diff_sec(
      products[2].app_data().install_time_diff_sec());

  ValidateExpectedValues(expected_data3, products[2].app_data());
}
#endif

TEST_F(AppManagerMachineTest, WritePreInstallData) {
  SetAppGuid(kGuid1, app_);
  EXPECT_SUCCEEDED(app_->put_isEulaAccepted(VARIANT_TRUE));
  WritePreInstallDataTest(app_, false);

  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathMachine,
                                _T("oeminstall")));
  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathMachine,
                                _T("eulaaccepted")));
}

TEST_F(AppManagerMachineTest, WritePreInstallData_IsOem) {
  const DWORD now = Time64ToInt32(GetCurrent100NSTime());
  ASSERT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("OemInstallTime"),
                                    now));
  if (vista_util::IsVistaOrLater()) {
    ASSERT_SUCCEEDED(RegKey::SetValue(
        _T("HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Setup\\State"),
        _T("ImageState"),
        _T("IMAGE_STATE_UNDEPLOYABLE")));
  } else {
    ASSERT_SUCCEEDED(RegKey::SetValue(_T("HKLM\\System\\Setup"),
                                      _T("AuditInProgress"),
                                      static_cast<DWORD>(1)));
  }

  SetAppGuid(kGuid1, app_);
  EXPECT_SUCCEEDED(app_->put_isEulaAccepted(VARIANT_TRUE));
  WritePreInstallDataTest(app_, false);

  CString oeminstall;
  EXPECT_SUCCEEDED(RegKey::GetValue(kGuid1ClientStateKeyPathMachine,
                                    _T("oeminstall"),
                                    &oeminstall));
  EXPECT_STREQ(_T("1"), oeminstall);

  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathMachine,
                                _T("eulaaccepted")));
}

TEST_F(AppManagerMachineTest,
       WritePreInstallData_ClearClientStateMediumUsageStats) {
  const CString client_state_key_name =
      AppendRegKeyPath(MACHINE_REG_CLIENT_STATE_MEDIUM, kGuid1);
  EXPECT_SUCCEEDED(RegKey::SetValue(client_state_key_name,
                                    _T("usagestats"),
                                    static_cast<DWORD>(1)));

  SetAppGuid(kGuid1, app_);
  WritePreInstallDataTest(app_, false);

  EXPECT_FALSE(RegKey::HasValue(client_state_key_name, _T("usagestats")));
}

// Tests the EULA accepted case too.
TEST_F(AppManagerUserTest, WritePreInstallData) {
  SetAppGuid(kGuid1, app_);
  EXPECT_SUCCEEDED(app_->put_isEulaAccepted(VARIANT_TRUE));
  WritePreInstallDataTest(app_, false);

  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathUser,
                                _T("oeminstall")));
  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathUser,
                                _T("eulaaccepted")));
}

TEST_F(AppManagerUserTest,
       WritePreInstallData_EulaNotAcceptedAppNotRegistered) {
  SetAppGuid(kGuid1, app_);
  EXPECT_SUCCEEDED(app_->put_isEulaAccepted(VARIANT_FALSE));

  WritePreInstallDataTest(app_, false);

  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathUser,
                                _T("oeminstall")));

  DWORD eula_accepted = 99;
  EXPECT_SUCCEEDED(RegKey::GetValue(kGuid1ClientStateKeyPathUser,
                                    _T("eulaaccepted"),
                                    &eula_accepted));
  EXPECT_EQ(0, eula_accepted);
}

TEST_F(AppManagerUserTest,
       WritePreInstallData_EulaNotAcceptedAppAlreadyInstalled) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientsKeyPathUser,
                                    _T("pv"),
                                    _T("1.2.3.4")));

  SetAppGuid(kGuid1, app_);
  EXPECT_SUCCEEDED(app_->put_isEulaAccepted(VARIANT_FALSE));

  WritePreInstallDataTest(app_, false);

  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathUser,
                                _T("oeminstall")));
  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathUser,
                                _T("eulaaccepted")));
}

TEST_F(AppManagerUserTest,
       WritePreInstallData_EulaAcceptedAppAlreadyInstalled) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientsKeyPathUser,
                                    _T("pv"),
                                    _T("1.2.3.4")));

  SetAppGuid(kGuid1, app_);
  EXPECT_SUCCEEDED(app_->put_isEulaAccepted(VARIANT_TRUE));

  WritePreInstallDataTest(app_, false);

  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathUser,
                                _T("oeminstall")));
  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathUser,
                                _T("eulaaccepted")));
}

TEST_F(AppManagerUserTest,
       WritePreInstallData_EulaAcceptedAppAlreadyInstalledAccepted) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientsKeyPathUser,
                                    _T("pv"),
                                    _T("1.2.3.4")));
  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientStateKeyPathUser,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));

  SetAppGuid(kGuid1, app_);
  EXPECT_SUCCEEDED(app_->put_isEulaAccepted(VARIANT_TRUE));

  WritePreInstallDataTest(app_, false);

  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathUser,
                                _T("oeminstall")));
  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathUser,
                                _T("eulaaccepted")));
}

TEST_F(AppManagerUserTest,
       WritePreInstallData_EulaAcceptedAppAlreadyInstalledNotAccepted) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientsKeyPathUser,
                                    _T("pv"),
                                    _T("1.2.3.4")));
  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientStateKeyPathUser,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));

  SetAppGuid(kGuid1, app_);
  EXPECT_SUCCEEDED(app_->put_isEulaAccepted(VARIANT_TRUE));

  WritePreInstallDataTest(app_, false);

  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathUser,
                                _T("oeminstall")));
  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathUser,
                                _T("eulaaccepted")));
}

TEST_F(AppManagerReadAppPersistentDataMachineTest, NoApp) {
  __mutexScope(app_->model()->lock());
  EXPECT_FAILED(app_manager_->ReadAppPersistentData(app_));
}

// For new app, the install_time_diff_sec_ should be -1 day.
TEST_F(AppManagerMachineTest, ReadAppInstallTimeDiff_NewApp) {
  __mutexScope(app_->model()->lock());
  app_manager_->ReadAppInstallTimeDiff(app_);
  EXPECT_EQ(kInitialInstallTimeDiff, app_->install_time_diff_sec());
}

TEST_F(AppManagerUserTest, ReadAppInstallTimeDiff_NewApp) {
  __mutexScope(app_->model()->lock());
  app_manager_->ReadAppInstallTimeDiff(app_);
  EXPECT_EQ(kInitialInstallTimeDiff, app_->install_time_diff_sec());
}

// For registered app, the install_time_diff_sec_ should be 0 if InstallTime
// registry value does not exist.
TEST_F(AppManagerMachineTest, ReadAppInstallTimeDiff_RegisteredApp) {
  App* new_app = CreateAppForRegistryPopulation(kGuid1);
  PopulateExpectedApp1(new_app);
  CreateAppRegistryState(*new_app, is_machine_, _T("1.0.0.0"), true);

  __mutexScope(app_->model()->lock());
  app_manager_->ReadAppInstallTimeDiff(app_);
  EXPECT_EQ(0, app_->install_time_diff_sec());
}

// For over-install app, the app is already registered. So
// install_time_diff_sec_ will be read from InstallTime registry value if
// exists. Otherwise it should be 0 as verified by the previous test case.
TEST_F(AppManagerUserTest, ReadAppInstallTimeDiff_OverInstall) {
  const uint32 kInstallTimeDiffSec = 100000;
  App* over_install_app = CreateAppForRegistryPopulation(kGuid1);
  PopulateExpectedApp1(over_install_app);
  SetAppInstallTimeDiffSec(over_install_app, kInstallTimeDiffSec);
  CreateAppRegistryState(*over_install_app, is_machine_, _T("1.1.1.1"), true);

  __mutexScope(app_->model()->lock());
  app_manager_->ReadAppInstallTimeDiff(app_);
  EXPECT_GE(app_->install_time_diff_sec(), kInstallTimeDiffSec);
  EXPECT_LE(app_->install_time_diff_sec(), kInstallTimeDiffSec + 1);
}

// For uninstalled app, the install_time_diff_sec_ will be read from registry
// if InstallTime registry value exists.
TEST_F(AppManagerMachineTest, ReadAppInstallTimeDiff_UninstalledApp) {
  const uint32 kInstallTimeDiffSec = 123456;
  App* uninstalled_app = CreateAppForRegistryPopulation(kGuid1);
  PopulateExpectedUninstalledApp(_T("1.1.0.0"), uninstalled_app);
  SetAppInstallTimeDiffSec(uninstalled_app, kInstallTimeDiffSec);
  CreateAppRegistryState(*uninstalled_app, is_machine_, _T("1.1.0.0"), false);

  __mutexScope(app_->model()->lock());
  app_manager_->ReadAppInstallTimeDiff(app_);
  EXPECT_GE(app_->install_time_diff_sec(), kInstallTimeDiffSec);
  EXPECT_LE(app_->install_time_diff_sec(), kInstallTimeDiffSec + 1);
}

// A special case. If pv of the uninstalled app doesn't exist, then it is
// considered not in uninstalled status. InstallTime should be ignored.
TEST_F(AppManagerUserTest, ReadAppInstallTimeDiff_UninstalledAppWithoutPv) {
  const uint32 kInstallTimeDiffSec = 112233;
  App* uninstalled_app = CreateAppForRegistryPopulation(kGuid1);
  PopulateExpectedUninstalledApp(_T("1.1.0.0"), uninstalled_app);
  SetAppInstallTimeDiffSec(uninstalled_app, kInstallTimeDiffSec);
  CreateAppRegistryState(*uninstalled_app, is_machine_, _T(""), false);

  __mutexScope(app_->model()->lock());
  app_manager_->ReadAppInstallTimeDiff(app_);
  EXPECT_EQ(kInitialInstallTimeDiff, app_->install_time_diff_sec());
}

// Tests clearing of data that is already present when not present in app_.
TEST_F(AppManagerUserTest,
       WritePreInstallData_AppAlreadyInstalled_ClearExistingData) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientsKeyPathUser,
                                    _T("pv"),
                                    _T("1.2.3.4")));

  SetAppGuid(kGuid1, app_);

  WritePreInstallDataTest(app_, true);

  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathUser,
                                kRegValueBrowser));
  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathUser,
                                kRegValueAdditionalParams));
}

// Tests clearing of data that is already present when not present in app_.
TEST_F(AppManagerUserTest,
       WritePreInstallData_AppAlreadyInstalled_OverwriteExistingData) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientsKeyPathUser,
                                    _T("pv"),
                                    _T("1.2.3.4")));
  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientsKeyPathUser,
                                    kRegValueBrowser,
                                    static_cast<DWORD>(BROWSER_CHROME)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientsKeyPathUser,
                                    kRegValueAdditionalParams,
                                    _T("existingAP")));

  SetAppGuid(kGuid1, app_);

  WritePreInstallDataTest(app_, false);

  EXPECT_EQ(BROWSER_FIREFOX, GetDwordValue(kGuid1ClientStateKeyPathUser,
                                           kRegValueBrowser));
  EXPECT_EQ(_T("test_ap"), GetSzValue(kGuid1ClientStateKeyPathUser,
                                      kRegValueAdditionalParams));
}

// It is important that previous_version passed to CreateAppRegistryState() be
// different than the current_version in PopulateExpectedApp1() to ensure the
// version is populated from Clients and not ClientState.
TEST_F(AppManagerReadAppPersistentDataUserTest, AppExists) {
  App* expected_app = CreateAppForRegistryPopulation(kGuid1);
  PopulateExpectedApp1(expected_app);
  CreateAppRegistryState(*expected_app, is_machine_, _T("1.0.0.0"), true);

  __mutexScope(app_->model()->lock());
  EXPECT_SUCCEEDED(app_manager_->ReadAppPersistentData(app_));

  EXPECT_SUCCEEDED(expected_app->put_isEulaAccepted(VARIANT_TRUE));
  ValidateExpectedValues(*expected_app, *app_);
}

TEST_F(AppManagerReadAppPersistentDataMachineTest, AppExists) {
  App* expected_app = CreateAppForRegistryPopulation(kGuid1);
  PopulateExpectedApp1(expected_app);
  CreateAppRegistryState(*expected_app, is_machine_, _T("1.0.0.0"), true);

  __mutexScope(app_->model()->lock());
  EXPECT_SUCCEEDED(app_manager_->ReadAppPersistentData(app_));

  EXPECT_SUCCEEDED(expected_app->put_isEulaAccepted(VARIANT_TRUE));
  ValidateExpectedValues(*expected_app, *app_);
}

TEST_F(AppManagerReadAppPersistentDataUserTest, AppExists_NoDisplayName) {
  App* expected_app = CreateAppForRegistryPopulation(kGuid1);
  PopulateExpectedApp1(expected_app);
  SetDisplayName(_T(""), expected_app);
  CreateAppRegistryState(*expected_app, is_machine_, _T("1.0.0.0"), true);

  __mutexScope(app_->model()->lock());
  EXPECT_SUCCEEDED(app_manager_->ReadAppPersistentData(app_));

  SetDisplayName(kDefaultAppName, expected_app);

  EXPECT_SUCCEEDED(expected_app->put_isEulaAccepted(VARIANT_TRUE));
  ValidateExpectedValues(*expected_app, *app_);
}

TEST_F(AppManagerReadAppPersistentDataUserTest, AppExists_EmptyDisplayName) {
  App* expected_app = CreateAppForRegistryPopulation(kGuid1);
  PopulateExpectedApp1(expected_app);
  CreateAppRegistryState(*expected_app, is_machine_, _T("1.0.0.0"), true);
  EXPECT_SUCCEEDED(RegKey::SetValue(GetClientKeyName(guid1_),
                                    kRegValueAppName,
                                    _T("")));

  __mutexScope(app_->model()->lock());
  EXPECT_SUCCEEDED(app_manager_->ReadAppPersistentData(app_));

  SetDisplayName(kDefaultAppName, expected_app);
  EXPECT_SUCCEEDED(expected_app->put_isEulaAccepted(VARIANT_TRUE));
  ValidateExpectedValues(*expected_app, *app_);
}

TEST_F(AppManagerReadAppPersistentDataUserTest, EulaNotAccepted) {
  App* expected_app = CreateAppForRegistryPopulation(kGuid1);
  PopulateExpectedApp1(expected_app);
  CreateAppRegistryState(*expected_app, is_machine_, _T("1.0.0.0"), true);

  // TODO(omaha): We should be able to eliminate the SetValue calls in
  // AppManagerReadAppPersistentData* by moving put_isEulaAccepted() call above
  // CreateAppRegistryState().
  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientStateKeyPathUser,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(expected_app->put_isEulaAccepted(VARIANT_FALSE));

  __mutexScope(app_->model()->lock());
  EXPECT_SUCCEEDED(app_manager_->ReadAppPersistentData(app_));

  ValidateExpectedValues(*expected_app, *app_);
}

TEST_F(AppManagerReadAppPersistentDataUserTest, EulaAccepted) {
  App* expected_app = CreateAppForRegistryPopulation(kGuid1);
  PopulateExpectedApp1(expected_app);
  CreateAppRegistryState(*expected_app, is_machine_, _T("1.0.0.0"), true);

  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientStateKeyPathUser,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(expected_app->put_isEulaAccepted(VARIANT_TRUE));

  __mutexScope(app_->model()->lock());
  EXPECT_SUCCEEDED(app_manager_->ReadAppPersistentData(app_));

  ValidateExpectedValues(*expected_app, *app_);
}

TEST_F(AppManagerReadAppPersistentDataMachineTest, EulaNotAccepted) {
  App* expected_app = CreateAppForRegistryPopulation(kGuid1);
  PopulateExpectedApp1(expected_app);
  CreateAppRegistryState(*expected_app, is_machine_, _T("1.0.0.0"), true);

  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientStateKeyPathMachine,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(expected_app->put_isEulaAccepted(VARIANT_FALSE));

  __mutexScope(app_->model()->lock());
  EXPECT_SUCCEEDED(app_manager_->ReadAppPersistentData(app_));

  ValidateExpectedValues(*expected_app, *app_);
}

TEST_F(AppManagerReadAppPersistentDataMachineTest, EulaAccepted) {
  App* expected_app = CreateAppForRegistryPopulation(kGuid1);
  PopulateExpectedApp1(expected_app);
  CreateAppRegistryState(*expected_app, is_machine_, _T("1.0.0.0"), true);

  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientStateKeyPathMachine,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  EXPECT_SUCCEEDED(expected_app->put_isEulaAccepted(VARIANT_TRUE));

  __mutexScope(app_->model()->lock());
  EXPECT_SUCCEEDED(app_manager_->ReadAppPersistentData(app_));

  ValidateExpectedValues(*expected_app, *app_);
}

TEST_F(AppManagerReadAppPersistentDataUserTest, TwoApps) {
  App* app2 = NULL;
  EXPECT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kGuid2), &app2));
  ASSERT_TRUE(app2);

  App* expected_app1 = CreateAppForRegistryPopulation(kGuid1);
  PopulateExpectedApp1(expected_app1);
  CreateAppRegistryState(*expected_app1, is_machine_, _T("1.0.0.0"), true);

  App* expected_app2 = CreateAppForRegistryPopulation(kGuid2);
  PopulateExpectedApp1(expected_app2);
  CreateAppRegistryState(*expected_app2, is_machine_, _T("1.0.0.0"), true);

  __mutexScope(app_->model()->lock());

  EXPECT_SUCCEEDED(app_manager_->ReadAppPersistentData(app_));
  EXPECT_SUCCEEDED(expected_app1->put_isEulaAccepted(VARIANT_TRUE));
  ValidateExpectedValues(*expected_app1, *app_);

  EXPECT_SUCCEEDED(app_manager_->ReadAppPersistentData(app2));
  EXPECT_SUCCEEDED(expected_app2->put_isEulaAccepted(VARIANT_TRUE));
  ValidateExpectedValues(*expected_app2, *app2);
}

TEST_F(AppManagerReadAppPersistentDataUserTest, NoClientState) {
  App* expected_app = CreateAppForRegistryPopulation(kGuid1);
  PopulateExpectedApp1ClientsOnly(expected_app);
  CreateAppRegistryState(*expected_app, is_machine_, _T("1.0.0.0"), true);

  // CreateAppRegistryState always creates the ClientState key, so delete it.
  EXPECT_SUCCEEDED(RegKey::DeleteKey(kGuid1ClientStateKeyPathUser));

  EXPECT_FALSE(IsClientStateKeyPresent(*app_));

  __mutexScope(app_->model()->lock());
  EXPECT_SUCCEEDED(app_manager_->ReadAppPersistentData(app_));

  EXPECT_SUCCEEDED(expected_app->put_isEulaAccepted(VARIANT_TRUE));
  ValidateExpectedValues(*expected_app, *app_);
}

// This is the No Clients key case.
TEST_F(AppManagerReadAppPersistentDataUserTest,
       ReadUninstalledAppPersistentData_UninstalledApp) {
  App* expected_app = CreateAppForRegistryPopulation(kGuid1);
  PopulateExpectedUninstalledApp(_T("1.1.0.0"), expected_app);
  CreateAppRegistryState(*expected_app, is_machine_, _T("1.1.0.0"), false);

  __mutexScope(app_->model()->lock());
  EXPECT_SUCCEEDED(app_manager_->ReadUninstalledAppPersistentData(app_));

  SetDisplayName(kDefaultAppName, expected_app);
  EXPECT_SUCCEEDED(expected_app->put_isEulaAccepted(VARIANT_TRUE));
  ValidateExpectedValues(*expected_app, *app_);

  // Explicitly verify the absence of a Clients key and the version since these
  // are important to this test.
  EXPECT_FALSE(RegKey::HasKey(
      AppendRegKeyPath(ConfigManager::Instance()->registry_clients(is_machine_),
      kGuid1)));
  EXPECT_STREQ(_T("1.1.0.0"), expected_app->current_version()->version());

  // Verify that ReadAppPersistentData does not populate the version.
  app_->current_version()->set_version(_T(""));
  expected_app->current_version()->set_version(_T(""));
  EXPECT_SUCCEEDED(app_manager_->ReadAppPersistentData(app_));
  SetDisplayName(kDefaultAppName, expected_app);
  ValidateExpectedValues(*expected_app, *app_);
  EXPECT_TRUE(expected_app->current_version()->version().IsEmpty());
}

TEST_F(AppManagerReadAppPersistentDataMachineTest,
       ReadUninstalledAppPersistentData_UninstalledApp) {
  App* expected_app = CreateAppForRegistryPopulation(kGuid1);
  PopulateExpectedUninstalledApp(_T("1.1.0.0"), expected_app);
  EXPECT_SUCCEEDED(expected_app->put_isEulaAccepted(VARIANT_TRUE));
  CreateAppRegistryState(*expected_app, is_machine_, _T("1.1.0.0"), false);

  __mutexScope(app_->model()->lock());
  EXPECT_SUCCEEDED(app_manager_->ReadUninstalledAppPersistentData(app_));

  SetDisplayName(kDefaultAppName, expected_app);
  ValidateExpectedValues(*expected_app, *app_);

  // Explicitly verify the absence of a Clients key and the version since these
  // are important to this test.
  EXPECT_FALSE(RegKey::HasKey(
      AppendRegKeyPath(ConfigManager::Instance()->registry_clients(is_machine_),
      kGuid1)));
  EXPECT_STREQ(_T("1.1.0.0"), expected_app->current_version()->version());

  // Verify that ReadAppPersistentData does not populate the version.
  app_->current_version()->set_version(_T(""));
  expected_app->current_version()->set_version(_T(""));
  EXPECT_SUCCEEDED(app_manager_->ReadAppPersistentData(app_));
  SetDisplayName(kDefaultAppName, expected_app);
  ValidateExpectedValues(*expected_app, *app_);
  EXPECT_TRUE(expected_app->current_version()->version().IsEmpty());
}

TEST_F(AppManagerReadAppPersistentDataUserTest,
       ReadUninstalledAppPersistentData_EulaNotAccepted) {
  App* expected_app = CreateAppForRegistryPopulation(kGuid1);
  PopulateExpectedUninstalledApp(_T("1.1.0.0"), expected_app);
  EXPECT_SUCCEEDED(expected_app->put_isEulaAccepted(VARIANT_FALSE));
  CreateAppRegistryState(*expected_app, is_machine_, _T("1.1.0.0"), false);

  __mutexScope(app_->model()->lock());
  EXPECT_SUCCEEDED(app_manager_->ReadUninstalledAppPersistentData(app_));

  SetDisplayName(kDefaultAppName, expected_app);
  ValidateExpectedValues(*expected_app, *app_);
}

// Tests the case where Omaha has created the Client State key before running
// the installer. Uses PopulateExpectedUninstalledApp then clears pv before
// writing the data to the registry. is_uninstalled_ is not set to false until
// after CreateAppRegistryState to prevent Client key from being created.
TEST_F(AppManagerReadAppPersistentDataUserTest,
       ClientStateExistsWithoutPvOrClientsKey) {
  App* expected_app = CreateAppForRegistryPopulation(kGuid1);
  PopulateExpectedUninstalledApp(_T(""), expected_app);
  CreateAppRegistryState(*expected_app, is_machine_, _T(""), false);

  // Update expected_app->install_time_diff_sec_ value here since related
  // registry values have been changed. app_ will be created based on the
  // new registry status so to make the objects match, reloading here is
  // necessary.
  app_manager_->ReadAppInstallTimeDiff(expected_app);

  __mutexScope(app_->model()->lock());
  EXPECT_SUCCEEDED(app_manager_->ReadAppPersistentData(app_));

  SetDisplayName(kDefaultAppName, expected_app);

  // This is an unregistered app, and |AppManager::GetAppUsageStatsEnabled|
  // returns TRISTATE_NONE for unregistered apps.
  EXPECT_SUCCEEDED(expected_app->put_usageStatsEnable(TRISTATE_NONE));

  EXPECT_SUCCEEDED(expected_app->put_isEulaAccepted(VARIANT_TRUE));
  ValidateExpectedValues(*expected_app, *app_);
}

TEST_F(AppManagerReadAppPersistentDataMachineTest,
       ClientStateExistsWithoutPvOrClientsKey) {
  App* expected_app = CreateAppForRegistryPopulation(kGuid1);
  PopulateExpectedUninstalledApp(_T(""), expected_app);
  CreateAppRegistryState(*expected_app, is_machine_, _T(""), false);

  // Update expected_app->install_time_diff_sec_ value here since related
  // registry values have been changed. app_ will be created based on the
  // new registry status so to make the objects match, reloading here is
  // necessary.
  app_manager_->ReadAppInstallTimeDiff(expected_app);

  __mutexScope(app_->model()->lock());
  EXPECT_SUCCEEDED(app_manager_->ReadAppPersistentData(app_));

  SetDisplayName(kDefaultAppName, expected_app);

  // This is an unregistered app, and |AppManager::GetAppUsageStatsEnabled|
  // returns TRISTATE_NONE for unregistered apps.
  EXPECT_SUCCEEDED(expected_app->put_usageStatsEnable(TRISTATE_NONE));

  EXPECT_SUCCEEDED(expected_app->put_isEulaAccepted(VARIANT_TRUE));
  ValidateExpectedValues(*expected_app, *app_);
}

// An empty pv value is the same as a populated one for uninstall checks.
TEST_F(AppManagerReadAppPersistentDataUserTest,
       ClientStateExistsWithEmptyPvNoClientsKey) {
  App* expected_app = CreateAppForRegistryPopulation(kGuid1);
  PopulateExpectedUninstalledApp(_T(""), expected_app);
  CreateAppRegistryState(*expected_app, is_machine_, _T(""), false);

  // Write the empty pv value.
  CString client_state_key_name = AppendRegKeyPath(
      ConfigManager::Instance()->registry_client_state(false),
      kGuid1);
  EXPECT_SUCCEEDED(RegKey::SetValue(client_state_key_name,
                                     kRegValueProductVersion,
                                     _T("")));

  __mutexScope(app_->model()->lock());
  EXPECT_SUCCEEDED(app_manager_->ReadAppPersistentData(app_));

  SetDisplayName(kDefaultAppName, expected_app);
  EXPECT_SUCCEEDED(expected_app->put_isEulaAccepted(VARIANT_TRUE));
  ValidateExpectedValues(*expected_app, *app_);
}

TEST_F(AppManagerReadAppPersistentDataUserTest, AppDefinedAttributesExist) {
  AppManagerTestBase::AttributePair pairs[] = {
    {_T("_SignedIn"), _T("3")},
    {_T("_Total"), _T("7")},
  };

  AppManagerTestBase::SumValuePair profiles[] = {
    {_T("{0143423998748372984}"), 4},
    {_T("{8947328472934792342}"), 7},
  };

  AppManagerTestBase::AttributeSubkeyPair subkey_pairs[] = {
    {_T("_NumProfiles1"), profiles, arraysize(profiles)},
  };

  App* expected_app;
  AppManagerTestBase::CreateExpectedAppDefinedAttributes(
      is_machine_,
      pairs,
      arraysize(pairs),
      subkey_pairs,
      arraysize(subkey_pairs),
      &expected_app);

  AppManagerTestBase::WriteAppDefinedAttributesToRegistry(
      is_machine_,
      pairs,
      arraysize(pairs),
      subkey_pairs,
      arraysize(subkey_pairs));

  __mutexScope(app_->model()->lock());
  EXPECT_SUCCEEDED(app_manager_->ReadAppPersistentData(app_));

  ValidateExpectedValues(*expected_app, *app_);
}

TEST_F(AppManagerReadAppPersistentDataUserTest,
       AppDefinedAttributesDoNotExist) {
  App* expected_app;
  AppManagerTestBase::CreateExpectedAppDefinedAttributes(
      is_machine_, NULL, 0, NULL, 0, &expected_app);

  AppManagerTestBase::WriteAppDefinedAttributesToRegistry(
      is_machine_, NULL, 0, NULL, 0);

  __mutexScope(app_->model()->lock());
  EXPECT_SUCCEEDED(app_manager_->ReadAppPersistentData(app_));

  ValidateExpectedValues(*expected_app, *app_);
}

TEST_F(AppManagerReadAppPersistentDataMachineTest, AppDefinedAttributesExist) {
  AppManagerTestBase::AttributePair pairs[] = {
    {_T("_NotSignedIn"), _T("99")},
    {_T("_TotalSignedIn"), _T("207")},
    {_T("_RandomFoo"), _T("97")},
    {_T("_RandomBar"), _T("Bar")},
  };

  AppManagerTestBase::SumValuePair profiles1[] = {
    {_T("{0143423998748372984}"), 4},
    {_T("{8947328472934792342}"), 7},
  };

  AppManagerTestBase::SumValuePair profiles2[] = {
    {_T("{Blah}"), 3},
  };

  AppManagerTestBase::AttributeSubkeyPair subkey_pairs[] = {
    {_T("_NumProfiles1"), profiles1, arraysize(profiles1)},
    {_T("_NumProfiles2"), profiles2, arraysize(profiles2)},
  };

  App* expected_app;
  AppManagerTestBase::CreateExpectedAppDefinedAttributes(
      is_machine_,
      pairs,
      arraysize(pairs),
      subkey_pairs,
      arraysize(subkey_pairs),
      &expected_app);

  AppManagerTestBase::WriteAppDefinedAttributesToRegistry(
      is_machine_,
      pairs,
      arraysize(pairs),
      subkey_pairs,
      arraysize(subkey_pairs));

  __mutexScope(app_->model()->lock());
  EXPECT_SUCCEEDED(app_manager_->ReadAppPersistentData(app_));

  ValidateExpectedValues(*expected_app, *app_);
}

TEST_F(AppManagerReadAppPersistentDataMachineTest,
       AppDefinedAttributesDoNotExist) {
  App* expected_app;
  AppManagerTestBase::CreateExpectedAppDefinedAttributes(
      is_machine_, NULL, 0, NULL, 0, &expected_app);

  AppManagerTestBase::WriteAppDefinedAttributesToRegistry(
      is_machine_, NULL, 0, NULL, 0);

  __mutexScope(app_->model()->lock());
  EXPECT_SUCCEEDED(app_manager_->ReadAppPersistentData(app_));

  ValidateExpectedValues(*expected_app, *app_);
}

TEST_F(AppManagerReadAppPersistentDataUserTest, CohortExists) {
  const CString cohort(_T("Cohort1"));
  const CString hint(_T("Hint1"));
  const CString name(_T("Name1"));

  App* expected_app;
  AppManagerTestBase::CreateExpectedCohortApp(is_machine_,
                                              cohort,
                                              hint,
                                              name,
                                              &expected_app);
  AppManagerTestBase::WriteCohortToRegistry(is_machine_, cohort, hint, name);

  __mutexScope(app_->model()->lock());
  EXPECT_SUCCEEDED(app_manager_->ReadAppPersistentData(app_));

  ValidateExpectedValues(*expected_app, *app_);
}

TEST_F(AppManagerReadAppPersistentDataUserTest, CohortDoesNotExist) {
  App* expected_app;
  AppManagerTestBase::CreateExpectedCohortApp(is_machine_,
                                              _T(""),
                                              _T(""),
                                              _T(""),
                                              &expected_app);
  AppManagerTestBase::DeleteCohortKey(is_machine_);

  __mutexScope(app_->model()->lock());
  EXPECT_SUCCEEDED(app_manager_->ReadAppPersistentData(app_));

  ValidateExpectedValues(*expected_app, *app_);
}

TEST_F(AppManagerReadAppPersistentDataMachineTest, CohortExists) {
  const CString cohort(_T("Cohort1"));
  const CString hint(_T("Hint1"));
  const CString name(_T("Name1"));

  App* expected_app;
  AppManagerTestBase::CreateExpectedCohortApp(is_machine_,
                                              cohort,
                                              hint,
                                              name,
                                              &expected_app);
  AppManagerTestBase::WriteCohortToRegistry(is_machine_, cohort, hint, name);

  __mutexScope(app_->model()->lock());
  EXPECT_SUCCEEDED(app_manager_->ReadAppPersistentData(app_));

  ValidateExpectedValues(*expected_app, *app_);
}

TEST_F(AppManagerReadAppPersistentDataMachineTest, CohortDoesNotExist) {
  App* expected_app;
  AppManagerTestBase::CreateExpectedCohortApp(is_machine_,
                                              _T(""),
                                              _T(""),
                                              _T(""),
                                              &expected_app);
  AppManagerTestBase::DeleteCohortKey(is_machine_);

  __mutexScope(app_->model()->lock());
  EXPECT_SUCCEEDED(app_manager_->ReadAppPersistentData(app_));

  ValidateExpectedValues(*expected_app, *app_);
}

TEST_F(AppManagerUserTest,
       ReadInstallerRegistrationValues_FailsWhenClientsKeyAbsent) {
  __mutexBlock(app_->model()->lock()) {
    EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_DID_NOT_WRITE_CLIENTS_KEY,
              app_manager_->ReadInstallerRegistrationValues(app_));
  }

  EXPECT_TRUE(app_->next_version()->version().IsEmpty());
  EXPECT_TRUE(app_->language().IsEmpty());

  EXPECT_FALSE(RegKey::HasKey(kGuid1ClientsKeyPathUser));
  EXPECT_FALSE(RegKey::HasKey(kGuid1ClientStateKeyPathUser));
}

TEST_F(AppManagerUserTest,
       ReadInstallerRegistrationValues_FailsWhenVersionValueAbsent) {
  ASSERT_SUCCEEDED(RegKey::CreateKey(kGuid1ClientsKeyPathUser));

  __mutexBlock(app_->model()->lock()) {
    EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_DID_NOT_WRITE_CLIENTS_KEY,
              app_manager_->ReadInstallerRegistrationValues(app_));
  }

  EXPECT_TRUE(app_->next_version()->version().IsEmpty());
  EXPECT_TRUE(app_->language().IsEmpty());

  EXPECT_TRUE(RegKey::HasKey(kGuid1ClientsKeyPathUser));
  EXPECT_FALSE(RegKey::HasKey(kGuid1ClientStateKeyPathUser));
}

TEST_F(AppManagerUserTest,
       ReadInstallerRegistrationValues_FailsWhenVersionValueEmpty) {
  ASSERT_SUCCEEDED(RegKey::SetValue(kGuid1ClientsKeyPathUser,
                                    kRegValueProductVersion,
                                    _T("")));

  __mutexBlock(app_->model()->lock()) {
    EXPECT_EQ(GOOPDATEINSTALL_E_INSTALLER_DID_NOT_WRITE_CLIENTS_KEY,
              app_manager_->ReadInstallerRegistrationValues(app_));
  }

  EXPECT_TRUE(app_->next_version()->version().IsEmpty());
  EXPECT_TRUE(app_->language().IsEmpty());

  EXPECT_TRUE(RegKey::HasKey(kGuid1ClientsKeyPathUser));
  EXPECT_FALSE(RegKey::HasKey(kGuid1ClientStateKeyPathUser));
}

TEST_F(AppManagerUserTest,
       ReadInstallerRegistrationValues_SucceedsWhenStateKeyAbsent) {
  ASSERT_SUCCEEDED(RegKey::SetValue(kGuid1ClientsKeyPathUser,
                                    kRegValueProductVersion,
                                    _T("0.9.68.4")));

  __mutexBlock(app_->model()->lock()) {
    EXPECT_SUCCEEDED(app_manager_->ReadInstallerRegistrationValues(app_));
  }

  EXPECT_STREQ(_T("0.9.68.4"), app_->next_version()->version());
  EXPECT_TRUE(app_->language().IsEmpty());
}

TEST_F(AppManagerUserTest,
       ReadInstallerRegistrationValues_SucceedsWithLanguage) {
  ASSERT_SUCCEEDED(RegKey::SetValue(kGuid1ClientsKeyPathUser,
                                    kRegValueProductVersion,
                                    _T("0.9.68.4")));
  ASSERT_SUCCEEDED(RegKey::SetValue(kGuid1ClientsKeyPathUser,
                                    kRegValueLanguage,
                                    _T("zh-TW")));

  __mutexBlock(app_->model()->lock()) {
    EXPECT_SUCCEEDED(app_manager_->ReadInstallerRegistrationValues(app_));
  }

  EXPECT_STREQ(_T("0.9.68.4"), app_->next_version()->version());
  EXPECT_STREQ(_T("zh-TW"), app_->language());
}

TEST_F(AppManagerMachineTest,
       ReadInstallerRegistrationValues_SucceedsWithLanguage) {
  ASSERT_SUCCEEDED(RegKey::SetValue(kGuid1ClientsKeyPathMachine,
                                    kRegValueProductVersion,
                                    _T("0.9.68.4")));
  ASSERT_SUCCEEDED(RegKey::SetValue(kGuid1ClientsKeyPathMachine,
                                    kRegValueLanguage,
                                    _T("zh-TW")));

  __mutexBlock(app_->model()->lock()) {
    EXPECT_SUCCEEDED(app_manager_->ReadInstallerRegistrationValues(app_));
  }

  EXPECT_STREQ(_T("0.9.68.4"), app_->next_version()->version());
  EXPECT_STREQ(_T("zh-TW"), app_->language());
}

TEST_F(AppManagerUserTest, PersistSuccessfulInstall) {
  PersistSuccessfulInstallTest();
}

TEST_F(AppManagerMachineTest, PersistSuccessfulInstall) {
  PersistSuccessfulInstallTest();
}

TEST_F(AppManagerUserTest, WriteStateValue) {
  WriteStateValueTest();
}

TEST_F(AppManagerUserTest, WriteDownloadProgress) {
  WriteDownloadProgressTest();
}

TEST_F(AppManagerUserTest, WriteInstallProgress) {
  WriteInstallProgressTest();
}

TEST_F(AppManagerUserTest,
    PersistUpdateCheckSuccessfullySent_AllUpdated_WasActive) {
  PersistUpdateCheckSuccessfullySent_AllUpdated(true);
}

TEST_F(AppManagerUserTest,
    PersistUpdateCheckSuccessfullySent_AllUpdated_NotActive) {
  PersistUpdateCheckSuccessfullySent_AllUpdated(false);
}

TEST_F(AppManagerMachineTest,
    PersistUpdateCheckSuccessfullySent_AllUpdated_WasActive) {
  PersistUpdateCheckSuccessfullySent_AllUpdated(true);
}

TEST_F(AppManagerMachineTest,
    PersistUpdateCheckSuccessfullySent_AllUpdated_NotActive) {
  PersistUpdateCheckSuccessfullySent_AllUpdated(false);
}

TEST_F(AppManagerUserTest, PersistUpdateCheckSuccessfullySent_NotRun) {
  PersistUpdateCheckSuccessfullySent_NotRun();
}

TEST_F(AppManagerMachineTest, PersistUpdateCheckSuccessfullySent_NotRun) {
  PersistUpdateCheckSuccessfullySent_NotRun();
}

TEST_F(AppManagerUserTest,
    PersistUpdateCheckSuccessfullySent_NoPreviousPing_WasActive) {
  PersistUpdateCheckSuccessfullySent_NoPreviousPing(true);
}

TEST_F(AppManagerUserTest,
    PersistUpdateCheckSuccessfullySent_NoPreviousPing_NotActive) {
  PersistUpdateCheckSuccessfullySent_NoPreviousPing(false);
}

TEST_F(AppManagerMachineTest,
       PersistUpdateCheckSuccessfullySent_NoPreviousPing_WasActive) {
  PersistUpdateCheckSuccessfullySent_NoPreviousPing(true);
}

TEST_F(AppManagerMachineTest,
       PersistUpdateCheckSuccessfullySent_NoPreviousPing_NotActive) {
  PersistUpdateCheckSuccessfullySent_NoPreviousPing(false);
}

TEST_F(AppManagerUserTest, PersistUpdateCheckSuccessfullySent_PingFreshness) {
  PingFreshnessTest(false);
}

TEST_F(AppManagerMachineTest,
       PersistUpdateCheckSuccessfullySent_PingFreshness) {
  PingFreshnessTest(true);
}

TEST_F(AppManagerUserTest, SynchronizeClientStateTest) {
  SynchronizeClientStateTest(kGuid2);  // App ID must be different than app_.

  ValidateClientStateMedium(is_machine_, kGuid2);
}

TEST_F(AppManagerMachineTest, SynchronizeClientStateTest) {
  SynchronizeClientStateTest(kGuid2);  // App ID must be different than app_.

  ValidateClientStateMedium(is_machine_, kGuid2);
}

// Should not create ClientStateMedium key.
TEST_F(AppManagerMachineTest, SynchronizeClientState_Omaha) {
  SynchronizeClientStateTest(kGoogleUpdateAppId);

  const CString client_state_medium_key_name = AppendRegKeyPath(
    ConfigManager::Instance()->machine_registry_client_state_medium(),
    kGoogleUpdateAppId);
  EXPECT_FALSE(RegKey::HasKey(client_state_medium_key_name));
}

TEST_F(AppManagerUserTest, UpdateUpdateAvailableStats_NoExistingStats) {
  const time64 before_time_in_100ns(GetCurrent100NSTime());

  __mutexScope(AppManager::Instance()->GetRegistryStableStateLock());
  UpdateUpdateAvailableStats(guid1_, app_manager_);

  const time64 after_time_in_100ns(GetCurrent100NSTime());

  DWORD update_available_count(0);
  EXPECT_SUCCEEDED(RegKey::GetValue(kGuid1ClientStateKeyPathUser,
                                    _T("UpdateAvailableCount"),
                                    &update_available_count));
  EXPECT_EQ(1, update_available_count);

  DWORD64 update_available_since_time(0);
  EXPECT_SUCCEEDED(RegKey::GetValue(kGuid1ClientStateKeyPathUser,
                                    _T("UpdateAvailableSince"),
                                    &update_available_since_time));
  EXPECT_LE(before_time_in_100ns, update_available_since_time);
  EXPECT_GE(after_time_in_100ns, update_available_since_time);
  const DWORD64 time_since_first_update_available =
      after_time_in_100ns - update_available_since_time;
  EXPECT_GT(10 * kSecsTo100ns, time_since_first_update_available);
}

TEST_F(AppManagerUserTest, UpdateUpdateAvailableStats_WithExistingStats) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientStateKeyPathUser,
                                    _T("UpdateAvailableCount"),
                                    static_cast<DWORD>(123456)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientStateKeyPathUser,
                                    _T("UpdateAvailableSince"),
                                    static_cast<DWORD64>(9876543210)));

  __mutexScope(AppManager::Instance()->GetRegistryStableStateLock());
  UpdateUpdateAvailableStats(guid1_, app_manager_);

  DWORD update_available_count(0);
  EXPECT_SUCCEEDED(RegKey::GetValue(kGuid1ClientStateKeyPathUser,
                                    _T("UpdateAvailableCount"),
                                    &update_available_count));
  EXPECT_EQ(123457, update_available_count);

  DWORD64 update_available_since_time(0);
  EXPECT_SUCCEEDED(RegKey::GetValue(kGuid1ClientStateKeyPathUser,
                                    _T("UpdateAvailableSince"),
                                    &update_available_since_time));
  EXPECT_EQ(9876543210, update_available_since_time);
}

// TODO(omaha3): Test PersistSuccessfulUpdateCheckResponse().
// TODO(omaha): Move these tests to app_registry_utils_unittest.cc.
#if 0
TEST_F(AppManagerUserTest, ClearUpdateAvailableStats_KeyNotPresent) {
  __mutexScope(AppManager::Instance()->GetRegistryStableStateLock());
  ClearUpdateAvailableStats(guid1_, app_manager_);
}

TEST_F(AppManagerUserTest, ClearUpdateAvailableStats_DataPresent) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientStateKeyPathUser,
                                    _T("UpdateAvailableCount"),
                                    static_cast<DWORD>(123456)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientStateKeyPathUser,
                                    _T("UpdateAvailableSince"),
                                    static_cast<DWORD64>(9876543210)));

  __mutexScope(AppManager::Instance()->GetRegistryStableStateLock());
  ClearUpdateAvailableStats(guid1_, app_manager_);

  EXPECT_TRUE(RegKey::HasKey(kGuid1ClientStateKeyPathUser));
  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathUser,
                                _T("UpdateAvailableCount")));
  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathUser,
                                _T("UpdateAvailableSince")));
}
#endif

TEST_F(AppManagerUserTest, ReadUpdateAvailableStats_DataNotPresent) {
  RegKey::CreateKey(kGuid1ClientStateKeyPathUser);

  DWORD update_responses(1);
  DWORD64 time_since_first_response_ms(1);
  app_manager_->ReadUpdateAvailableStats(guid1_,
                                         &update_responses,
                                         &time_since_first_response_ms);

  EXPECT_EQ(0, update_responses);
  EXPECT_EQ(0, time_since_first_response_ms);
}

TEST_F(AppManagerUserTest, ReadUpdateAvailableStats_DataPresent) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientStateKeyPathUser,
                                    _T("UpdateAvailableCount"),
                                    static_cast<DWORD>(123456)));
  const DWORD64 kUpdateAvailableSince =
    GetCurrent100NSTime() - 2 * kMillisecsTo100ns;
  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientStateKeyPathUser,
                                    _T("UpdateAvailableSince"),
                                    kUpdateAvailableSince));

  DWORD update_responses(0);
  DWORD64 time_since_first_response_ms(0);
  app_manager_->ReadUpdateAvailableStats(guid1_,
                                         &update_responses,
                                         &time_since_first_response_ms);

  EXPECT_EQ(123456, update_responses);
  EXPECT_LE(2, time_since_first_response_ms);
  EXPECT_GT(10 * kMsPerSec, time_since_first_response_ms);
}

// TODO(omaha): Move these tests to app_registry_utils_unittest.cc.
#if 0
TEST_F(AppManagerUserTest, PersistSuccessfulInstall_Install_Online) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientStateKeyPathUser,
                                    _T("UpdateAvailableCount"),
                                    static_cast<DWORD>(123456)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientStateKeyPathUser,
                                    _T("UpdateAvailableSince"),
                                    static_cast<DWORD64>(9876543210)));

  __mutexScope(AppManager::Instance()->GetRegistryStableStateLock());
  app_manager_->PersistSuccessfulInstall(guid1_, false, false);
  const uint32 now = Time64ToInt32(GetCurrent100NSTime());

  // Verify ClearUpdateAvailableStats() was called.
  EXPECT_TRUE(RegKey::HasKey(kGuid1ClientStateKeyPathUser));
  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathUser,
                                _T("UpdateAvailableCount")));
  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathUser,
                                _T("UpdateAvailableSince")));

  // Verify update check value is written but update value is not.
  const uint32 last_check_sec = GetDwordValue(kGuid1ClientStateKeyPathUser,
                                              kRegValueLastSuccessfulCheckSec);
  EXPECT_GE(now, last_check_sec);
  EXPECT_GE(static_cast<uint32>(200), now - last_check_sec);

  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathUser,
                                kRegValueLastUpdateTimeSec));
}

TEST_F(AppManagerUserTest, PersistSuccessfulInstall_Install_Offline) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientStateKeyPathUser,
                                    _T("UpdateAvailableCount"),
                                    static_cast<DWORD>(123456)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientStateKeyPathUser,
                                    _T("UpdateAvailableSince"),
                                    static_cast<DWORD64>(9876543210)));

  __mutexScope(AppManager::Instance()->GetRegistryStableStateLock());
  app_manager_->PersistSuccessfulInstall(guid1_, false, true);

  // Verify ClearUpdateAvailableStats() was called.
  EXPECT_TRUE(RegKey::HasKey(kGuid1ClientStateKeyPathUser));
  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathUser,
                                _T("UpdateAvailableCount")));
  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathUser,
                                _T("UpdateAvailableSince")));

  // Verify update values are not written.
  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathUser,
                                kRegValueLastSuccessfulCheckSec));
  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathUser,
                                kRegValueLastUpdateTimeSec));
}

TEST_F(AppManagerUserTest, PersistSuccessfulInstall_Update_ExistingTimes) {
  const DWORD kExistingUpdateValues = 0x70123456;
  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientStateKeyPathUser,
                                    _T("UpdateAvailableCount"),
                                    static_cast<DWORD>(123456)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientStateKeyPathUser,
                                    _T("UpdateAvailableSince"),
                                    static_cast<DWORD64>(9876543210)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientStateKeyPathUser,
                                    kRegValueLastSuccessfulCheckSec,
                                    kExistingUpdateValues));
  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientStateKeyPathUser,
                                    kRegValueLastUpdateTimeSec,
                                    kExistingUpdateValues));

  __mutexScope(AppManager::Instance()->GetRegistryStableStateLock());
  app_manager_->PersistSuccessfulInstall(guid1_, true, false);
  const uint32 now = Time64ToInt32(GetCurrent100NSTime());

  // Verify ClearUpdateAvailableStats() was called.
  EXPECT_TRUE(RegKey::HasKey(kGuid1ClientStateKeyPathUser));
  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathUser,
                                _T("UpdateAvailableCount")));
  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathUser,
                                _T("UpdateAvailableSince")));

  // Verify update values updated.
  const uint32 last_check_sec = GetDwordValue(kGuid1ClientStateKeyPathUser,
                                              kRegValueLastSuccessfulCheckSec);
  EXPECT_NE(kExistingUpdateValues, last_check_sec);
  EXPECT_GE(now, last_check_sec);
  EXPECT_GE(static_cast<uint32>(200), now - last_check_sec);

  const uint32 last_update_sec =
      GetDwordValue(kGuid1ClientStateKeyPathUser, kRegValueLastUpdateTimeSec);
  EXPECT_NE(kExistingUpdateValues, last_update_sec);
  EXPECT_GE(now, last_update_sec);
  EXPECT_GE(static_cast<uint32>(200), now - last_update_sec);
}

TEST_F(AppManagerUserTest,
       PersistSuccessfulInstall_Update_StateKeyDoesNotExist) {
  __mutexScope(AppManager::Instance()->GetRegistryStableStateLock());
  app_manager_->PersistSuccessfulInstall(guid1_, true, false);
  const uint32 now = Time64ToInt32(GetCurrent100NSTime());

  // Verify update values updated.
  const uint32 last_check_sec = GetDwordValue(kGuid1ClientStateKeyPathUser,
                                              kRegValueLastSuccessfulCheckSec);
  EXPECT_GE(now, last_check_sec);
  EXPECT_GE(static_cast<uint32>(200), now - last_check_sec);

  const uint32 last_update_sec =
      GetDwordValue(kGuid1ClientStateKeyPathUser, kRegValueLastUpdateTimeSec);
  EXPECT_GE(now, last_update_sec);
  EXPECT_GE(static_cast<uint32>(200), now - last_update_sec);
}
#endif

// TODO(omaha): Move these tests to app_registry_utils_unittest.cc.
#if 0
TEST_F(AppManagerUserTest, PersistSuccessfulUpdateCheck_ExistingTime) {
  const DWORD kExistingUpdateValue = 0x12345678;
  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientStateKeyPathUser,
                                    kRegValueLastSuccessfulCheckSec,
                                    kExistingUpdateValue));

  __mutexScope(AppManager::Instance()->GetRegistryStableStateLock());
  app_manager_->PersistSuccessfulUpdateCheck(guid1_);
  const uint32 now = Time64ToInt32(GetCurrent100NSTime());

  const uint32 last_check_sec = GetDwordValue(kGuid1ClientStateKeyPathUser,
                                              kRegValueLastSuccessfulCheckSec);
  EXPECT_NE(kExistingUpdateValue, last_check_sec);
  EXPECT_GE(now, last_check_sec);
  EXPECT_GE(static_cast<uint32>(200), now - last_check_sec);

  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathUser,
                                kRegValueLastUpdateTimeSec));
}

TEST_F(AppManagerUserTest, PersistSuccessfulUpdateCheck_StateKeyDoesNotExist) {
  __mutexScope(AppManager::Instance()->GetRegistryStableStateLock());
  app_manager_->PersistSuccessfulUpdateCheck(guid1_);
  const uint32 now = Time64ToInt32(GetCurrent100NSTime());

  const uint32 last_check_sec = GetDwordValue(kGuid1ClientStateKeyPathUser,
                                              kRegValueLastSuccessfulCheckSec);
  EXPECT_GE(now, last_check_sec);
  EXPECT_GE(static_cast<uint32>(200), now - last_check_sec);

  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathUser,
                                kRegValueLastUpdateTimeSec));
}
#endif

TEST_F(AppManagerMachineTest, RemoveClientState_Uninstalled) {
  App* expected_app = CreateAppForRegistryPopulation(kGuid1);
  PopulateExpectedUninstalledApp(_T("1.1.0.0"), expected_app);
  CreateAppRegistryState(*expected_app, is_machine_, _T("1.1.0.0"), false);

  __mutexScope(AppManager::Instance()->GetRegistryStableStateLock());
  EXPECT_SUCCEEDED(app_manager_->RemoveClientState(guid1_));
  EXPECT_FALSE(IsClientStateKeyPresent(*expected_app));
}

// TODO(omaha): Move implementation up to class definition.
// Creates 2 registered app and 1 unregistered app and populates the parameters.
// Each is written to the registry.
// Also creates partial Clients and ClientState keys and creates a registered
// and an unregistered app in the opposite registry hive.
void AppManagerWithBundleTest::
    PopulateDataAndRegistryForRegisteredAndUnInstalledAppsTests(
    bool is_machine,
    App* expected_app0,
    App* expected_app1,
    App* expected_app2,
    App* opposite_hive_data1,
    App* opposite_hive_data2) {

  PopulateExpectedApp1(expected_app0);
  CreateAppRegistryState(*expected_app0, is_machine, _T("1.0.0.0"), true);

  PopulateExpectedApp2(expected_app1);
  CreateAppRegistryState(*expected_app1, is_machine, _T("1.1.0.0"), true);

  PopulateExpectedUninstalledApp(_T("2.3.0.0"), expected_app2);
  CreateAppRegistryState(*expected_app2, is_machine, _T("2.3.0.0"), false);

  //
  // Add incomplete Clients and ClientState entries.
  //

  // No pv.
  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(
      AppendRegKeyPath(is_machine ? MACHINE_REG_CLIENTS : USER_REG_CLIENTS,
                       kGuid4),
      _T("name"),
      _T("foo")));

  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(
      AppendRegKeyPath(is_machine ? MACHINE_REG_CLIENT_STATE :
                                    USER_REG_CLIENT_STATE,
                       kGuid5),
      kRegValueDidRun,
      _T("1")));

  // Add registered and unregistered app to the opposite registry hive.
  PopulateExpectedApp2(opposite_hive_data1);
  CreateAppRegistryState(*opposite_hive_data1,
                         !is_machine,
                         _T("1.1.0.0"),
                         true);

  PopulateExpectedUninstalledApp(_T("1.1.0.0"), opposite_hive_data2);
  CreateAppRegistryState(*opposite_hive_data2,
                         !is_machine,
                         _T("1.1.0.0"),
                         false);
}

TEST_F(AppManagerWithBundleMachineTest, GetRegisteredApps) {
  App *expected_app0, *expected_app1, *expected_app2;
  PopulateDataAndRegistryForRegisteredAndUnInstalledAppsTests(
      true,
      &expected_app0,
      &expected_app1,
      &expected_app2);

  // An important part of this test is that GetRegisteredApps() continues and
  // returns success even though there is a key for an app that is not properly
  // registered (e.g. it does not have a pv value or has an invalid pv value).
  // Since this is so important, explicitly check registry was set up correctly.
  // Invalid pv value type is checked in a separate test since it causes an
  // assert.
  // Since GetRegisteredApps() does not rely on pv, the app is still returned
  // in the vector.
  // Note that kGuid4 is not upper-cased like the other GUIDs. This is because
  // the other GUIDs are converted to a GUID and back to a string by
  // PopulateDataAndRegistryForRegisteredAndUnInstalledAppsTests().
  const CString incomplete_clients_key =
      AppendRegKeyPath(MACHINE_REG_CLIENTS, kGuid4);
  EXPECT_TRUE(RegKey::HasKey(incomplete_clients_key));
  EXPECT_FALSE(RegKey::HasValue(incomplete_clients_key,
                                kRegValueProductVersion));

  AppIdVector registered_app_ids;
  EXPECT_SUCCEEDED(app_manager_->GetRegisteredApps(&registered_app_ids));
  EXPECT_EQ(3, static_cast<int>(registered_app_ids.size()));

  EXPECT_STREQ(CString(kGuid1).MakeUpper(), registered_app_ids[0]);
  EXPECT_STREQ(CString(kGuid2).MakeUpper(), registered_app_ids[1]);
  EXPECT_STREQ(kGuid4, registered_app_ids[2]);
}

TEST_F(AppManagerWithBundleUserTest, GetRegisteredApps) {
  App *expected_app0, *expected_app1, *expected_app2;
  PopulateDataAndRegistryForRegisteredAndUnInstalledAppsTests(
      false,
      &expected_app0,
      &expected_app1,
      &expected_app2);

  // See comment in machine GetRegisteredApps test.
  const CString incomplete_clients_key =
      AppendRegKeyPath(USER_REG_CLIENTS, kGuid4);
  EXPECT_TRUE(RegKey::HasKey(incomplete_clients_key));
  EXPECT_FALSE(RegKey::HasValue(incomplete_clients_key,
                                kRegValueProductVersion));

  AppIdVector registered_app_ids;
  EXPECT_SUCCEEDED(app_manager_->GetRegisteredApps(&registered_app_ids));
  EXPECT_EQ(3, static_cast<int>(registered_app_ids.size()));

  EXPECT_STREQ(CString(kGuid1).MakeUpper(), registered_app_ids[0]);
  EXPECT_STREQ(CString(kGuid2).MakeUpper(), registered_app_ids[1]);
  EXPECT_STREQ(kGuid4, registered_app_ids[2]);
}

TEST_F(AppManagerWithBundleUserTest, GetRegisteredApps_InvalidPvValueType) {
  App *expected_app0, *expected_app1, *expected_app2;
  PopulateDataAndRegistryForRegisteredAndUnInstalledAppsTests(
      false,
      &expected_app0,
      &expected_app1,
      &expected_app2);

  // It is important that this value is alphabetically before the other AppIds.
  // This allows us to test that processing continues despite the bad pv.
  const TCHAR* const kInvalidPvTypeAppId =
      _T("{0150B619-867C-4985-B193-ED309A23EE36}");
  const CString invalid_pv_type_clients_key =
      AppendRegKeyPath(USER_REG_CLIENTS, kInvalidPvTypeAppId);

  // Incorrect data type for pv. See comment in machine GetRegisteredApps test.
  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(invalid_pv_type_clients_key,
                                            kRegValueProductVersion,
                                            static_cast<DWORD>(12345)));
  EXPECT_TRUE(RegKey::HasValue(invalid_pv_type_clients_key,
                               kRegValueProductVersion));
  DWORD value_type = REG_SZ;
  EXPECT_SUCCEEDED(RegKey::GetValueType(invalid_pv_type_clients_key,
                                        kRegValueProductVersion,
                                        &value_type));
  EXPECT_NE(REG_SZ, value_type);

  AppIdVector registered_app_ids;
  EXPECT_SUCCEEDED(app_manager_->GetRegisteredApps(&registered_app_ids));

  EXPECT_EQ(4, static_cast<int>(registered_app_ids.size()));

  EXPECT_STREQ(kInvalidPvTypeAppId, registered_app_ids[0]);
  EXPECT_STREQ(CString(kGuid1).MakeUpper(), registered_app_ids[1]);
  EXPECT_STREQ(CString(kGuid2).MakeUpper(), registered_app_ids[2]);
  EXPECT_STREQ(kGuid4, registered_app_ids[3]);
}

TEST_F(AppManagerWithBundleMachineTest, GetUninstalledApps) {
  App *expected_app0, *expected_app1, *expected_app2;
  PopulateDataAndRegistryForRegisteredAndUnInstalledAppsTests(
      true,
      &expected_app0,
      &expected_app1,
      &expected_app2);

  AppIdVector registered_app_ids;
  EXPECT_SUCCEEDED(app_manager_->GetUninstalledApps(&registered_app_ids));
  EXPECT_EQ(1, static_cast<int>(registered_app_ids.size()));

  EXPECT_STREQ(CString(kGuid3).MakeUpper(), registered_app_ids[0]);
}

TEST_F(AppManagerWithBundleUserTest, GetUninstalledApps) {
  App *expected_app0, *expected_app1, *expected_app2;
  PopulateDataAndRegistryForRegisteredAndUnInstalledAppsTests(
      false,
      &expected_app0,
      &expected_app1,
      &expected_app2);

  AppIdVector registered_app_ids;
  EXPECT_SUCCEEDED(app_manager_->GetUninstalledApps(&registered_app_ids));
  EXPECT_EQ(1, static_cast<int>(registered_app_ids.size()));

  EXPECT_STREQ(CString(kGuid3).MakeUpper(), registered_app_ids[0]);
}

TEST_F(AppManagerWithBundleMachineTest, GetOemInstalledAndEulaAcceptedApps) {
  // Create an OEM installed app.
  App* expected_app1 = CreateAppForRegistryPopulation(kGuid1);
  PopulateExpectedApp1(expected_app1);
  CreateAppRegistryState(*expected_app1, is_machine_, _T("1.0.0.0"), true);

  CString client_state_key_name = AppendRegKeyPath(
      ConfigManager::Instance()->registry_client_state(is_machine_),
      expected_app1->app_guid_string());
  RegKey client_state_key;
  ASSERT_SUCCEEDED(client_state_key.Create(client_state_key_name));
  ASSERT_SUCCEEDED(client_state_key.SetValue(kRegValueOemInstall, _T("1")));
  DWORD now = Time64ToInt32(GetCurrent100NSTime());
  const DWORD one_day_back = now - kSecondsPerDay;
  ASSERT_SUCCEEDED(client_state_key.SetValue(kRegValueInstallTimeSec,
                                             one_day_back));

  // Create a non-OEM installed app.
  App* expected_app2 = CreateAppForRegistryPopulation(kGuid2);
  PopulateExpectedApp2(expected_app2);
  CreateAppRegistryState(*expected_app2, is_machine_, _T("2.0.0.0"), true);

  AppIdVector oem_installed_app_ids;
  EXPECT_SUCCEEDED(
      app_manager_->GetOemInstalledAndEulaAcceptedApps(&oem_installed_app_ids));
  EXPECT_EQ(1, static_cast<int>(oem_installed_app_ids.size()));
  EXPECT_STREQ(expected_app1->app_guid_string().MakeUpper(),
               oem_installed_app_ids[0]);

  app_manager_->ClearOemInstalled(oem_installed_app_ids);
  oem_installed_app_ids.clear();

  EXPECT_SUCCEEDED(
      app_manager_->GetOemInstalledAndEulaAcceptedApps(&oem_installed_app_ids));
  EXPECT_EQ(0, static_cast<int>(oem_installed_app_ids.size()));

  DWORD install_time(0);
  ASSERT_SUCCEEDED(client_state_key.GetValue(kRegValueInstallTimeSec,
                                             &install_time));
  EXPECT_GE(install_time, one_day_back + kSecondsPerDay);
  now = Time64ToInt32(GetCurrent100NSTime());
  EXPECT_GE(static_cast<uint32>(kSecPerMin), now - install_time);
}

TEST_F(AppManagerWithBundleUserTest, GetOemInstalledAndEulaAcceptedApps) {
  // Create an OEM installed app.
  App* expected_app1 = CreateAppForRegistryPopulation(kGuid1);
  PopulateExpectedApp1(expected_app1);
  CreateAppRegistryState(*expected_app1, is_machine_, _T("1.0.0.0"), true);

  CString client_state_key_name = AppendRegKeyPath(
      ConfigManager::Instance()->registry_client_state(is_machine_),
      expected_app1->app_guid_string());
  RegKey client_state_key;
  ASSERT_SUCCEEDED(client_state_key.Create(client_state_key_name));
  ASSERT_SUCCEEDED(client_state_key.SetValue(kRegValueOemInstall, _T("1")));
  DWORD now = Time64ToInt32(GetCurrent100NSTime());
  const DWORD one_day_back = now - kSecondsPerDay;
  ASSERT_SUCCEEDED(client_state_key.SetValue(kRegValueInstallTimeSec,
                                             one_day_back));

  // Create a non-OEM installed app.
  App* expected_app2 = CreateAppForRegistryPopulation(kGuid2);
  PopulateExpectedApp2(expected_app2);
  CreateAppRegistryState(*expected_app2, is_machine_, _T("2.0.0.0"), true);

  AppIdVector oem_installed_app_ids;
  EXPECT_SUCCEEDED(
      app_manager_->GetOemInstalledAndEulaAcceptedApps(&oem_installed_app_ids));
  EXPECT_EQ(1, static_cast<int>(oem_installed_app_ids.size()));
  EXPECT_STREQ(expected_app1->app_guid_string().MakeUpper(),
               oem_installed_app_ids[0]);

  app_manager_->ClearOemInstalled(oem_installed_app_ids);
  oem_installed_app_ids.clear();

  EXPECT_SUCCEEDED(
      app_manager_->GetOemInstalledAndEulaAcceptedApps(&oem_installed_app_ids));
  EXPECT_EQ(0, static_cast<int>(oem_installed_app_ids.size()));

  DWORD install_time(0);
  ASSERT_SUCCEEDED(client_state_key.GetValue(kRegValueInstallTimeSec,
                                             &install_time));
  EXPECT_GE(install_time, one_day_back + kSecondsPerDay);
  now = Time64ToInt32(GetCurrent100NSTime());
  EXPECT_GE(static_cast<uint32>(kSecPerMin), now - install_time);
}

// TODO(omaha): Perhaps CoCreate some real hooks and test further for the
// *UpdateHook* tests.
TEST_F(AppManagerWithBundleMachineTest, RunRegistrationUpdateHook) {
  App *expected_app0, *expected_app1, *expected_app2;
  PopulateForRegistrationUpdateHookTests(
      true,
      &expected_app0,
      &expected_app1,
      &expected_app2,
      kNonExistentClsid);

  EXPECT_EQ(REGDB_E_CLASSNOTREG,
            app_manager_->RunRegistrationUpdateHook(CString(kGuid1)));

  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            app_manager_->RunRegistrationUpdateHook(CString(kGuid7)));
}

TEST_F(AppManagerWithBundleUserTest, RunRegistrationUpdateHook) {
  App *expected_app0, *expected_app1, *expected_app2;
  PopulateForRegistrationUpdateHookTests(
      false,
      &expected_app0,
      &expected_app1,
      &expected_app2,
      kNonExistentClsid);

  EXPECT_EQ(REGDB_E_CLASSNOTREG,
            app_manager_->RunRegistrationUpdateHook(CString(kGuid1)));

  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            app_manager_->RunRegistrationUpdateHook(CString(kGuid7)));
}

// TODO(omaha): The RunAllRegistrationUpdateHooks tests do not actually
// CoCreate any hook. Need to find a way to test this.
TEST_F(AppManagerWithBundleMachineTest, RunAllRegistrationUpdateHooks) {
  App *expected_app0, *expected_app1, *expected_app2;
  PopulateForRegistrationUpdateHookTests(
      true,
      &expected_app0,
      &expected_app1,
      &expected_app2,
      GuidToString(GUID_NULL));

  EXPECT_SUCCEEDED(app_manager_->RunAllRegistrationUpdateHooks());
}

TEST_F(AppManagerWithBundleUserTest, RunAllRegistrationUpdateHooks) {
  App *expected_app0, *expected_app1, *expected_app2;
  PopulateForRegistrationUpdateHookTests(
      false,
      &expected_app0,
      &expected_app1,
      &expected_app2,
      GuidToString(GUID_NULL));

  EXPECT_SUCCEEDED(app_manager_->RunAllRegistrationUpdateHooks());
}

TEST_F(AppManagerMachineTest, AppLockContention) {
  const int kLockHeldTimeMs = 500;
  HoldAppManagerLock hold_lock(is_machine_, kLockHeldTimeMs);

  Thread thread;
  thread.Start(&hold_lock);
  hold_lock.WaitForLockToBeAcquired();

  HighresTimer lock_metrics_timer;
  __mutexScope(app_->model()->lock());
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            app_manager_->ReadAppPersistentData(app_));

  // -20 because this sometimes failed with 48x vs. 500.
  EXPECT_LE(kLockHeldTimeMs - 20, lock_metrics_timer.GetElapsedMs());

  thread.WaitTillExit(1000);
}

TEST_F(AppManagerUserTest, AppLockContention) {
  const int kLockHeldTimeMs = 500;
  HoldAppManagerLock hold_lock(is_machine_, kLockHeldTimeMs);

  Thread thread;
  thread.Start(&hold_lock);
  hold_lock.WaitForLockToBeAcquired();

  HighresTimer lock_metrics_timer;
  __mutexScope(app_->model()->lock());
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            app_manager_->ReadAppPersistentData(app_));

  // -20 because this sometimes failed with 48x vs. 500.
  EXPECT_LE(kLockHeldTimeMs - 20, lock_metrics_timer.GetElapsedMs());

  thread.WaitTillExit(1000);
}

TEST_F(AppManagerMachineTest, AppLock_MachineAndUser) {
  GLock app_manager_user_lock;
  InitializeAppManagerRegistryLock(false, &app_manager_user_lock);

  __mutexScope(app_manager_user_lock);

  HighresTimer lock_metrics_timer;
  __mutexScope(app_->model()->lock());
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            app_manager_->ReadAppPersistentData(app_));

  EXPECT_GT(40, lock_metrics_timer.GetElapsedMs())
      << _T("No delay is expected because there should not be a conflict.");
}

TEST_F(AppManagerUserTest, ReadAppVersionNoLock_NoApp) {
  CString version;
  EXPECT_FAILED(AppManager::ReadAppVersionNoLock(is_machine_,
                                                 StringToGuid(kGuid1),
                                                 &version));
}

TEST_F(AppManagerUserTest, ReadAppVersionNoLock_RegisteredApp) {
  ASSERT_SUCCEEDED(RegKey::SetValue(kGuid1ClientsKeyPathUser,
                                    kRegValueProductVersion,
                                    _T("0.9.75.4")));

  CString version;
  EXPECT_SUCCEEDED(AppManager::ReadAppVersionNoLock(is_machine_,
                                                    StringToGuid(kGuid1),
                                                    &version));
  EXPECT_STREQ(_T("0.9.75.4"), version);
  EXPECT_SUCCEEDED(RegKey::DeleteKey(kGuid1ClientsKeyPathUser));
}

TEST_F(AppManagerMachineTest, ReadAppVersionNoLock_NoApp) {
  CString version;
  EXPECT_FAILED(AppManager::ReadAppVersionNoLock(is_machine_,
                                                 StringToGuid(kGuid1),
                                                 &version));
}

TEST_F(AppManagerMachineTest, ReadAppVersionNoLock_RegisteredApp) {
  ASSERT_SUCCEEDED(RegKey::SetValue(kGuid1ClientsKeyPathMachine,
                                    kRegValueProductVersion,
                                    _T("0.9.80.4")));

  CString version;
  EXPECT_SUCCEEDED(AppManager::ReadAppVersionNoLock(is_machine_,
                                                    StringToGuid(kGuid1),
                                                    &version));
  EXPECT_STREQ(_T("0.9.80.4"), version);
  EXPECT_SUCCEEDED(RegKey::DeleteKey(kGuid1ClientsKeyPathMachine));
}

}  // namespace omaha
