// Copyright 2009-2010 Google Inc.
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

#include "omaha/goopdate/app_bundle.h"

#include <atlsecurity.h>

#include "omaha/base/app_util.h"
#include "omaha/base/error.h"
#include "omaha/base/reg_key.h"
#include "omaha/base/thread_pool.h"
#include "omaha/base/utils.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/common/lang.h"
#include "omaha/goopdate/app_bundle_state_busy.h"
#include "omaha/goopdate/app_bundle_state_initialized.h"
#include "omaha/goopdate/app_bundle_state_paused.h"
#include "omaha/goopdate/app_bundle_state_ready.h"
#include "omaha/goopdate/app_bundle_state_stopped.h"
#include "omaha/goopdate/app_manager.h"
#include "omaha/goopdate/goopdate.h"
#include "omaha/goopdate/model.h"
#include "omaha/goopdate/resource_manager.h"
#include "omaha/goopdate/worker.h"
#include "omaha/goopdate/worker_mock.h"
#include "omaha/testing/unit_test.h"
#include "omaha/third_party/smartany/scoped_any.h"

namespace omaha {

const int kKnownError = 0x87658765;

const TCHAR* const kDefaultAppName = SHORT_COMPANY_NAME _T(" Application");

const uint32 kInitialInstallTimeDiff = static_cast<uint32>(-1 * kSecondsPerDay);

// TODO(omaha): there is a problem with this unit test. The model is built
// bottom up. This makes it impossible to set the references to parents. Will
// have to fix the code, eventually using Builder DP to create a bunch of
// models containing bundles, apps, and such.


// Helper functions.
// TODO(omaha): helper functions need to go into their own compilation unit to
// avoid dependencies between unit test modules.

void ValidateFreshInstallDefaultValues(const App& app) {
  EXPECT_FALSE(::IsEqualGUID(GUID_NULL, app.app_guid()));
  EXPECT_TRUE(app.language().IsEmpty());
  EXPECT_TRUE(app.ap().IsEmpty());
  EXPECT_TRUE(app.tt_token().IsEmpty());
  EXPECT_TRUE(::IsEqualGUID(GUID_NULL, app.iid()));
  EXPECT_TRUE(app.brand_code().IsEmpty());
  EXPECT_TRUE(app.client_id().IsEmpty());
  EXPECT_TRUE(app.GetExperimentLabels().IsEmpty());
  EXPECT_TRUE(app.referral_id().IsEmpty());
  EXPECT_EQ(kInitialInstallTimeDiff, app.install_time_diff_sec());
  EXPECT_FALSE(app.is_eula_accepted());
  EXPECT_TRUE(app.display_name().IsEmpty());
  EXPECT_EQ(BROWSER_UNKNOWN, app.browser_type());
  EXPECT_TRUE(app.server_install_data_index().IsEmpty());
  EXPECT_EQ(TRISTATE_NONE, app.usage_stats_enable());
  EXPECT_TRUE(app.client_install_data().IsEmpty());
  EXPECT_TRUE(app.server_install_data().IsEmpty());
  EXPECT_EQ(ACTIVE_UNKNOWN, app.did_run());
  EXPECT_EQ(0, app.days_since_last_active_ping());
  EXPECT_EQ(0, app.days_since_last_roll_call());
  EXPECT_EQ(0, app.day_of_last_activity());
  EXPECT_EQ(0, app.day_of_last_roll_call());

  EXPECT_TRUE(app.current_version()->version().IsEmpty());
  EXPECT_TRUE(app.next_version()->version().IsEmpty());
    // TODO(omaha3): Add all the new values (state_, etc.).
}

void PopulateDataAndRegistryForRegisteredAndUnInstalledAppsTests(
    bool is_machine,
    App* expected_app0,
    App* expected_app1,
    App* expected_app2,
    App* opposite_hive_app1,
    App* opposite_hive_app2);

void SetDisplayName(const CString& name, App* app);

void ValidateExpectedValues(const App& expected, const App& actual) {
  EXPECT_STREQ(GuidToString(expected.app_guid()),
               GuidToString(actual.app_guid()));
  EXPECT_STREQ(expected.language(), actual.language());
  EXPECT_STREQ(expected.ap(), actual.ap());
  EXPECT_STREQ(expected.tt_token(), actual.tt_token());
  EXPECT_STREQ(GuidToString(expected.iid()), GuidToString(actual.iid()));
  EXPECT_STREQ(expected.brand_code(), actual.brand_code());
  EXPECT_STREQ(expected.client_id(), actual.client_id());
  EXPECT_STREQ(expected.GetExperimentLabels(), actual.GetExperimentLabels());
  EXPECT_STREQ(expected.referral_id(), actual.referral_id());
  EXPECT_EQ(expected.install_time_diff_sec(), actual.install_time_diff_sec());
  EXPECT_EQ(expected.is_eula_accepted(), actual.is_eula_accepted());
  EXPECT_STREQ(expected.display_name(), actual.display_name());
  EXPECT_EQ(expected.browser_type(), actual.browser_type());
  EXPECT_STREQ(expected.server_install_data_index(),
               actual.server_install_data_index());
  EXPECT_EQ(expected.usage_stats_enable(), actual.usage_stats_enable());
  EXPECT_STREQ(expected.client_install_data(), actual.client_install_data());
  EXPECT_STREQ(expected.server_install_data(), actual.server_install_data());
  EXPECT_EQ(expected.did_run(), actual.did_run());

  EXPECT_STREQ(expected.current_version()->version(),
               actual.current_version()->version());
  EXPECT_STREQ(expected.next_version()->version(),
               actual.next_version()->version());
  EXPECT_EQ(expected.app_defined_attributes(), actual.app_defined_attributes());

  EXPECT_EQ(expected.cohort().cohort, actual.cohort().cohort);
  EXPECT_EQ(expected.cohort().hint, actual.cohort().hint);
  EXPECT_EQ(expected.cohort().name, actual.cohort().name);

  // TODO(omaha3): Add all the new values (state(), etc.)?
}

using ::testing::_;
using ::testing::Return;

namespace {

// TODO(omaha): At least some of these and where they are used have to be kept
// in sync with app_manager_unittest.cc because the constants are hard-coded in
// functions used by these tests. Break this coupling.
const TCHAR* const kGuid1 = _T("{21CD0965-0B0E-47cf-B421-2D191C16C0E2}");
const TCHAR* const kGuid2 = _T("{A979ACBD-1F55-4b12-A35F-4DBCA5A7CCB8}");
const TCHAR* const kGuid3 = _T("{661045C5-4429-4140-BC48-8CEA241D1DEF}");
const TCHAR* const kGuid4 = _T("{AAFA1CF9-E94F-42e6-A899-4CD27F37D5A7}");
const TCHAR* const kGuid6 = _T("{F3F2CFD4-5F98-4bf0-ABB0-BEEEA46C62B4}");
const TCHAR* const kGuid7 = _T("{6FD2272F-8583-4bbd-895A-E65F8003FC7B}");

void InitializeRegistryForTest(bool is_machine) {
  RegKey::DeleteKey(ConfigManager::Instance()->registry_clients(is_machine));
  RegKey::DeleteKey(
      ConfigManager::Instance()->registry_client_state(is_machine));
}

class TestUserWorkItem : public UserWorkItem {
 private:
  virtual void DoProcess() {}
};

ACTION_P(SetWorkItem, work_item) {
  UNREFERENCED_ACTION_PARAMETERS;
  arg0->set_user_work_item(work_item);
  return 0;
}

}  // namespace

class AppBundleNoBundleTest : public testing::Test {
 protected:
  explicit AppBundleNoBundleTest(bool is_machine)
      : is_machine_(is_machine), goopdate_(is_machine) {}

  virtual void SetUp() {
    EXPECT_SUCCEEDED(AppManager::CreateInstance(is_machine_));

    // By default, no Worker methods should be called, so use StrictMock.
    // Override this behavior for specific methods in the individual test cases.
    worker_.reset(new testing::StrictMock<MockWorker>);
    model_.reset(new Model(worker_.get()));

    EXPECT_CALL(*worker_, Lock()).WillRepeatedly(Return(2));
    EXPECT_CALL(*worker_, Unlock()).WillRepeatedly(Return(1));
  }

  virtual void TearDown() {
    AppManager::DeleteInstance();
  }

  const bool is_machine_;
  std::unique_ptr<MockWorker> worker_;
  std::unique_ptr<Model> model_;

 private:
  Goopdate goopdate_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(AppBundleNoBundleTest);
};

class AppBundleTest : public AppBundleNoBundleTest {
 protected:
  explicit AppBundleTest(bool is_machine) : AppBundleNoBundleTest(is_machine) {}

  static void SetUpTestCase() {
    EXPECT_EQ(STATE_INIT,         fsm::AppBundleState::STATE_INIT);
    EXPECT_EQ(STATE_INITIALIZED,  fsm::AppBundleState::STATE_INITIALIZED);
    EXPECT_EQ(STATE_BUSY,         fsm::AppBundleState::STATE_BUSY);
    EXPECT_EQ(STATE_READY,        fsm::AppBundleState::STATE_READY);
    EXPECT_EQ(STATE_PAUSED,       fsm::AppBundleState::STATE_PAUSED);
    EXPECT_EQ(STATE_STOPPED,      fsm::AppBundleState::STATE_STOPPED);
  }

  virtual void SetUp() {
    AppBundleNoBundleTest::SetUp();
    app_bundle_ = model_->CreateAppBundle(is_machine_);
    ASSERT_TRUE(app_bundle_.get());
  }

  virtual void TearDown() {
    app_bundle_.reset();
    AppBundleNoBundleTest::TearDown();
  }

  void TestPropertyReflexiveness() {
    EXPECT_SUCCEEDED(app_bundle_->put_displayName(CComBSTR(_T("My Apps"))));
    CComBSTR name(_T(""));
    EXPECT_SUCCEEDED(app_bundle_->get_displayName(&name));
    EXPECT_STREQ(_T("My Apps"), name);

    EXPECT_SUCCEEDED(app_bundle_->put_displayLanguage(CComBSTR(_T("en"))));
    CComBSTR lang(_T(""));
    EXPECT_SUCCEEDED(app_bundle_->get_displayLanguage(&lang));
    EXPECT_STREQ(_T("en"), lang);

    EXPECT_SUCCEEDED(app_bundle_->put_installSource(CComBSTR(_T("unittest"))));
    CComBSTR source(_T(""));
    EXPECT_SUCCEEDED(app_bundle_->get_installSource(&source));
    EXPECT_STREQ(_T("unittest"), source);

    EXPECT_SUCCEEDED(app_bundle_->put_priority(INSTALL_PRIORITY_LOW));
    long priority = INSTALL_PRIORITY_HIGH;  // NOLINT
    EXPECT_SUCCEEDED(app_bundle_->get_priority(&priority));
    EXPECT_EQ(INSTALL_PRIORITY_LOW, priority);
  }

  // A copy of the protected enum in AppBundleState, allowing the individual
  // tests to use these values.
  enum BundleState {
    STATE_INIT,
    STATE_INITIALIZED,
    STATE_BUSY,
    STATE_READY,
    STATE_PAUSED,
    STATE_STOPPED,
  };

  BundleState GetBundleState() {
    return static_cast<BundleState>(app_bundle_->app_bundle_state_->state_);
  }

  std::shared_ptr<AppBundle> app_bundle_;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(AppBundleTest);
};

class AppBundleUninitializedTest : public AppBundleTest {
 protected:
  explicit AppBundleUninitializedTest(bool is_machine)
      : AppBundleTest(is_machine) {}

  void TestPropertyDefaults() {
    CComBSTR name;
    EXPECT_SUCCEEDED(app_bundle_->get_displayName(&name));
    EXPECT_STREQ(_T(""), name);

    CComBSTR lang;
    EXPECT_SUCCEEDED(app_bundle_->get_displayLanguage(&lang));
    EXPECT_STREQ(lang::GetDefaultLanguage(is_machine_), lang);

    CComBSTR install_source;
    EXPECT_SUCCEEDED(app_bundle_->get_installSource(&install_source));
    EXPECT_STREQ(_T("unknown"), install_source);

    CComBSTR offline_dir;
    EXPECT_SUCCEEDED(app_bundle_->get_offlineDirectory(&offline_dir));
    EXPECT_STREQ(_T(""), offline_dir);

    long priority;  // NOLINT
    EXPECT_SUCCEEDED(app_bundle_->get_priority(&priority));
    EXPECT_EQ(INSTALL_PRIORITY_HIGH, priority);
  }
};

class AppBundleUninitializedMachineTest : public AppBundleUninitializedTest {
 protected:
  AppBundleUninitializedMachineTest() : AppBundleUninitializedTest(true) {}
};

class AppBundleUninitializedUserTest : public AppBundleUninitializedTest {
 protected:
  AppBundleUninitializedUserTest() : AppBundleUninitializedTest(false) {}
};

TEST_F(AppBundleUninitializedMachineTest, Properties) {
  TestPropertyDefaults();
  TestPropertyReflexiveness();
}

TEST_F(AppBundleUninitializedUserTest, Properties) {
  TestPropertyDefaults();
  TestPropertyReflexiveness();
}

class AppBundleInitializedTest : public AppBundleTest {
 protected:
  explicit AppBundleInitializedTest(bool is_machine)
      : AppBundleTest(is_machine) {}

  virtual void SetUp() {
    AppBundleTest::SetUp();
    // TODO(omaha): UserRights::GetCallerToken() fails with ERROR_NO_TOKEN
    // when initialize() is called for a machine bundle during these tests.
    // It might make sense to move theimpersonation stuff to the COM wrapper,
    // but I am not sure how this would work with the AppBundleStates.
    // If we can do something about this, call initialize() in all cases.
    if (is_machine_) {
      SetAppBundleStateForUnitTest(app_bundle_.get(),
                                   new fsm::AppBundleStateInitialized);
    } else {
      EXPECT_SUCCEEDED(app_bundle_->put_displayName(CComBSTR(_T("My Bundle"))));
      EXPECT_SUCCEEDED(app_bundle_->put_displayLanguage(CComBSTR(_T("en"))));
      EXPECT_SUCCEEDED(app_bundle_->initialize());
    }
  }
};

class AppBundleNoBundleMachineTest : public AppBundleNoBundleTest {
 protected:
  AppBundleNoBundleMachineTest() : AppBundleNoBundleTest(true) {}
};

class AppBundleNoBundleUserTest : public AppBundleNoBundleTest {
 protected:
  AppBundleNoBundleUserTest() : AppBundleNoBundleTest(false) {}
};

class AppBundleInitializedMachineTest : public AppBundleInitializedTest {
 protected:
  AppBundleInitializedMachineTest() : AppBundleInitializedTest(true) {}
  virtual void SetUp() {
    AppBundleInitializedTest::SetUp();
    InitializeRegistryForTest(true);
  }
};

class AppBundleInitializedUserTest : public AppBundleInitializedTest {
 protected:
  AppBundleInitializedUserTest() : AppBundleInitializedTest(false) {}
  virtual void SetUp() {
    AppBundleInitializedTest::SetUp();
    InitializeRegistryForTest(false);
  }
};


// The creation of an app bundle inserts it in the model and increments
// the server module count.
TEST_F(AppBundleNoBundleUserTest, ConstructorAndDestructor) {
  EXPECT_CALL(*worker_, Lock()).WillOnce(Return(2));
  EXPECT_CALL(*worker_, Unlock()).WillOnce(Return(1));

  std::shared_ptr<AppBundle> app_bundle(model_->CreateAppBundle(is_machine_));
  EXPECT_TRUE(app_bundle.get());
  EXPECT_EQ(1, model_->GetNumberOfAppBundles());
  EXPECT_EQ(app_bundle.get(), model_->GetAppBundle(0).get());
  app_bundle.reset();
  EXPECT_EQ(0, model_->GetNumberOfAppBundles());
}

TEST_F(AppBundleNoBundleMachineTest, ConstructorAndDestructor) {
  EXPECT_CALL(*worker_, Lock()).WillOnce(Return(2));
  EXPECT_CALL(*worker_, Unlock()).WillOnce(Return(1));

  std::shared_ptr<AppBundle> app_bundle(model_->CreateAppBundle(is_machine_));
  EXPECT_TRUE(app_bundle.get());
  EXPECT_EQ(1, model_->GetNumberOfAppBundles());
  EXPECT_EQ(app_bundle.get(), model_->GetAppBundle(0).get());
  app_bundle.reset();
  EXPECT_EQ(0, model_->GetNumberOfAppBundles());
}

class AppBundlePopulatedRegistryTest : public AppBundleInitializedTest {
 protected:
  explicit AppBundlePopulatedRegistryTest(bool is_machine)
      : AppBundleInitializedTest(is_machine) {}

  virtual void SetUp() {
    AppBundleInitializedTest::SetUp();

    InitializeRegistryForTest(is_machine_);

    EXPECT_SUCCEEDED(ResourceManager::Create(
      is_machine_, app_util::GetCurrentModuleDirectory(), _T("en")));

    test_app_bundle_for_expected_apps_ = model_->CreateAppBundle(is_machine_);
    ASSERT_TRUE(test_app_bundle_for_expected_apps_.get());
    // TODO(omaha): Address with the TODO in AppBundleInitializedTest::SetUp.
    if (is_machine_) {
      SetAppBundleStateForUnitTest(test_app_bundle_for_expected_apps_.get(),
                                   new fsm::AppBundleStateInitialized);
    } else {
      EXPECT_SUCCEEDED(test_app_bundle_for_expected_apps_->put_displayName(
                           CComBSTR(_T("My Bundle"))));
      EXPECT_SUCCEEDED(test_app_bundle_for_expected_apps_->put_displayLanguage(
                           CComBSTR(_T("en"))));
      EXPECT_SUCCEEDED(test_app_bundle_for_expected_apps_->initialize());
    }
  }

  virtual void TearDown() {
    test_app_bundle_for_expected_apps_.reset();

    ResourceManager::Delete();

    AppBundleInitializedTest::TearDown();
  }

  // App will be cleaned up when bundle is destroyed.
  // This is a hack for creating registry data. Would be nice to have a
  // different mechanism.
  App* CreateExpectedApp(const TCHAR* app_id) {
    App* app = NULL;
    EXPECT_SUCCEEDED(
        test_app_bundle_for_expected_apps_->createApp(CComBSTR(app_id), &app));
    ASSERT1(app);
    return app;
  }

  void PopulateDataAndRegistryForRegisteredAndUnInstalledAppsTests(
      App** expected_app0,
      App** expected_app1,
      App** expected_app2) {
    *expected_app0 = CreateExpectedApp(kGuid1);
    *expected_app1 = CreateExpectedApp(kGuid2);
    *expected_app2 = CreateExpectedApp(kGuid3);
    App* opposite_hive_app1 = CreateExpectedApp(kGuid6);
    App* opposite_hive_app2 = CreateExpectedApp(kGuid7);
    omaha::PopulateDataAndRegistryForRegisteredAndUnInstalledAppsTests(
        is_machine_,
        *expected_app0,
        *expected_app1,
        *expected_app2,
        opposite_hive_app1,
        opposite_hive_app2);

    // Reload install age since registry has been changed.
    AppManager::Instance()->ReadAppInstallTimeDiff(*expected_app0);
    AppManager::Instance()->ReadAppInstallTimeDiff(*expected_app1);
    AppManager::Instance()->ReadAppInstallTimeDiff(*expected_app2);
    AppManager::Instance()->ReadAppInstallTimeDiff(opposite_hive_app1);
    AppManager::Instance()->ReadAppInstallTimeDiff(opposite_hive_app2);
  }

  std::shared_ptr<AppBundle> test_app_bundle_for_expected_apps_;
  LLock lock_;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(AppBundlePopulatedRegistryTest);
};

class AppBundlePopulatedRegistryMachineTest
    : public AppBundlePopulatedRegistryTest {
 protected:
  AppBundlePopulatedRegistryMachineTest()
      : AppBundlePopulatedRegistryTest(true) {}
};

class AppBundlePopulatedRegistryUserTest
    : public AppBundlePopulatedRegistryTest {
 protected:
  AppBundlePopulatedRegistryUserTest()
      : AppBundlePopulatedRegistryTest(false) {}
};

TEST_F(AppBundleInitializedUserTest, CountAndItem_NoApps) {
  EXPECT_EQ(0, app_bundle_->GetNumberOfApps());
  long num_apps = 0;  // NOLINT
  EXPECT_SUCCEEDED(app_bundle_->get_Count(&num_apps));
  EXPECT_EQ(0, num_apps);

  {
    ExpectAsserts expect_asserts;
    EXPECT_FALSE(app_bundle_->GetApp(0));
  }
  App* app0_obtained = NULL;
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_INVALID_INDEX),
            app_bundle_->get_Item(0, &app0_obtained));
  EXPECT_FALSE(app0_obtained);

  {
    ExpectAsserts expect_asserts;
    EXPECT_FALSE(app_bundle_->GetApp(1));
  }
  App* app1_obtained = NULL;
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_INVALID_INDEX),
            app_bundle_->get_Item(1, &app1_obtained));
  EXPECT_FALSE(app1_obtained);
}

TEST_F(AppBundleInitializedUserTest, CountAndItem_OneApp) {
  App* app0_created = NULL;
  EXPECT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kGuid1), &app0_created));

  EXPECT_EQ(1, app_bundle_->GetNumberOfApps());
  long num_apps = 0;  // NOLINT
  EXPECT_SUCCEEDED(app_bundle_->get_Count(&num_apps));
  EXPECT_EQ(1, num_apps);

  EXPECT_TRUE(app_bundle_->GetApp(0));
  App* app0_obtained = NULL;
  EXPECT_SUCCEEDED(app_bundle_->get_Item(0, &app0_obtained));
  EXPECT_TRUE(app0_obtained);

  {
    ExpectAsserts expect_asserts;
    EXPECT_FALSE(app_bundle_->GetApp(1));
  }
  App* app1_obtained = NULL;
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_INVALID_INDEX),
            app_bundle_->get_Item(1, &app1_obtained));
  EXPECT_FALSE(app1_obtained);
}

TEST_F(AppBundleInitializedUserTest, CountAndItem_TwoApp) {
  App* app0_created = NULL;
  EXPECT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kGuid1), &app0_created));
  App* app1_created = NULL;
  EXPECT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kGuid2), &app1_created));

  EXPECT_EQ(2, app_bundle_->GetNumberOfApps());
  long num_apps = 0;  // NOLINT
  EXPECT_SUCCEEDED(app_bundle_->get_Count(&num_apps));
  EXPECT_EQ(2, num_apps);

  EXPECT_TRUE(app_bundle_->GetApp(0));
  App* app0_obtained = NULL;
  EXPECT_SUCCEEDED(app_bundle_->get_Item(0, &app0_obtained));
  EXPECT_TRUE(app0_obtained);
  EXPECT_EQ(app0_created, app0_obtained);

  EXPECT_TRUE(app_bundle_->GetApp(1));
  App* app1_obtained = NULL;
  EXPECT_SUCCEEDED(app_bundle_->get_Item(1, &app1_obtained));
  EXPECT_TRUE(app1_obtained);

  {
    ExpectAsserts expect_asserts;
    EXPECT_FALSE(app_bundle_->GetApp(2));
  }
  App* app2_obtained = NULL;
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_INVALID_INDEX),
            app_bundle_->get_Item(2, &app2_obtained));
  EXPECT_FALSE(app2_obtained);
}

TEST_F(AppBundleInitializedUserTest, createApp) {
  App* app0_created = NULL;
  EXPECT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kGuid1), &app0_created));
  EXPECT_TRUE(app0_created);
  EXPECT_EQ(1, app_bundle_->GetNumberOfApps());

  App* app0 = app_bundle_->GetApp(0);
  EXPECT_EQ(app0_created, app0);

  EXPECT_STREQ(CString(kGuid1).MakeUpper(), app0->app_guid_string());
  ValidateFreshInstallDefaultValues(*app0);
}

TEST_F(AppBundleInitializedUserTest, createApp_TwoApps) {
  App* app0_created = NULL;
  EXPECT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kGuid1), &app0_created));
  EXPECT_TRUE(app0_created);
  EXPECT_EQ(1, app_bundle_->GetNumberOfApps());

  App* app0 = app_bundle_->GetApp(0);
  EXPECT_EQ(app0_created, app0);

  EXPECT_STREQ(CString(kGuid1).MakeUpper(), app0->app_guid_string());
  ValidateFreshInstallDefaultValues(*app0);

  // Add a second app to the bundle.

  App* app1_created = NULL;
  EXPECT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kGuid2), &app1_created));
  EXPECT_TRUE(app1_created);
  EXPECT_EQ(2, app_bundle_->GetNumberOfApps());

  App* app1 = app_bundle_->GetApp(1);
  EXPECT_EQ(app1_created, app1);

  EXPECT_STREQ(CString(kGuid2).MakeUpper(), app1->app_guid_string());
  ValidateFreshInstallDefaultValues(*app1);
}

TEST_F(AppBundleInitializedUserTest, createApp_SameAppTwice) {
  App* app0_created = NULL;
  EXPECT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kGuid1), &app0_created));
  EXPECT_TRUE(app0_created);
  EXPECT_EQ(1, app_bundle_->GetNumberOfApps());

  App* app0 = app_bundle_->GetApp(0);
  EXPECT_EQ(app0_created, app0);

  EXPECT_STREQ(CString(kGuid1).MakeUpper(), app0->app_guid_string());
  ValidateFreshInstallDefaultValues(*app0);

  // Attempt to add the same app to the bundle again.

  App* app1_created = NULL;
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED,
            app_bundle_->createApp(CComBSTR(kGuid1), &app1_created));
  EXPECT_FALSE(app1_created);
  EXPECT_EQ(1, app_bundle_->GetNumberOfApps());
}

TEST_F(AppBundleInitializedUserTest, createApp_AfterUpdateCheck) {
  TestUserWorkItem test_work_item;
  EXPECT_CALL(*worker_, CheckForUpdateAsync(_))
      .WillOnce(SetWorkItem(&test_work_item));

  App* app0_created = NULL;
  EXPECT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kGuid1), &app0_created));
  EXPECT_SUCCEEDED(app_bundle_->checkForUpdate());

  App* app1_created = NULL;
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED,
            app_bundle_->createApp(CComBSTR(kGuid2), &app1_created));

  app_bundle_->CompleteAsyncCall();  // Simulate thread completion.
}

TEST_F(AppBundleInitializedMachineTest, createApp) {
  App* app0_created = NULL;
  EXPECT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kGuid1), &app0_created));
  EXPECT_TRUE(app0_created);
  EXPECT_EQ(1, app_bundle_->GetNumberOfApps());

  App* app0 = app_bundle_->GetApp(0);
  EXPECT_EQ(app0_created, app0);

  EXPECT_STREQ(CString(kGuid1).MakeUpper(), app0->app_guid_string());
  ValidateFreshInstallDefaultValues(*app0);
}

TEST_F(AppBundleInitializedUserTest, checkForUpdate_NoApps) {
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED, app_bundle_->checkForUpdate());
}

// Does not verify the update check occurs.
TEST_F(AppBundleInitializedUserTest, checkForUpdate_OneApp) {
  TestUserWorkItem test_work_item;
  EXPECT_CALL(*worker_, CheckForUpdateAsync(_))
      .WillOnce(SetWorkItem(&test_work_item));

  App* app = NULL;
  EXPECT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kGuid1), &app));

  EXPECT_SUCCEEDED(app_bundle_->checkForUpdate());

  app_bundle_->CompleteAsyncCall();  // Simulate thread completion.
}

// Does not verify the update check occurs.
TEST_F(AppBundleInitializedUserTest, checkForUpdate_TwoApps) {
  TestUserWorkItem test_work_item;
  EXPECT_CALL(*worker_, CheckForUpdateAsync(_))
      .WillOnce(SetWorkItem(&test_work_item));

  App* app0 = NULL;
  App* app1 = NULL;
  EXPECT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kGuid1), &app0));
  EXPECT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kGuid2), &app1));

  EXPECT_SUCCEEDED(app_bundle_->checkForUpdate());

  app_bundle_->CompleteAsyncCall();  // Simulate thread completion.
}

TEST_F(AppBundleInitializedUserTest, checkForUpdate_Twice) {
  TestUserWorkItem test_work_item;
  EXPECT_CALL(*worker_, CheckForUpdateAsync(_))
      .WillOnce(SetWorkItem(&test_work_item));

  App* app = NULL;
  EXPECT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kGuid1), &app));

  EXPECT_SUCCEEDED(app_bundle_->checkForUpdate());
  app_bundle_->CompleteAsyncCall();  // Simulate thread completion.

  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED, app_bundle_->checkForUpdate());
}

TEST_F(AppBundleInitializedUserTest, checkForUpdate_WhileBundleIsBusy) {
  TestUserWorkItem test_work_item;
  EXPECT_CALL(*worker_, CheckForUpdateAsync(_))
      .WillOnce(SetWorkItem(&test_work_item));

  App* app = NULL;
  EXPECT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kGuid1), &app));

  EXPECT_SUCCEEDED(app_bundle_->checkForUpdate());
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED, app_bundle_->checkForUpdate());

  app_bundle_->CompleteAsyncCall();  // Simulate thread completion.
}

// Does not verify the update check occurs.
TEST_F(AppBundleInitializedMachineTest, checkForUpdate_OneApp) {
  TestUserWorkItem test_work_item;
  EXPECT_CALL(*worker_, CheckForUpdateAsync(_))
      .WillOnce(SetWorkItem(&test_work_item));

  App* app = NULL;
  EXPECT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kGuid1), &app));

  EXPECT_SUCCEEDED(app_bundle_->checkForUpdate());

  app_bundle_->CompleteAsyncCall();  // Simulate thread completion.
}

TEST_F(AppBundleInitializedUserTest, download_WithoutUpdateCheck) {
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED, app_bundle_->download());
}

// The AppBundle does not prevent this, but the apps may enter the error state.
TEST_F(AppBundleInitializedUserTest, download_AfterInstall) {
  TestUserWorkItem test_work_item;
  {
    ::testing::InSequence seq;
    EXPECT_CALL(*worker_, CheckForUpdateAsync(_))
        .WillOnce(SetWorkItem(&test_work_item));
    EXPECT_CALL(*worker_, DownloadAndInstallAsync(_))
        .WillOnce(SetWorkItem(&test_work_item));
    EXPECT_CALL(*worker_, DownloadAsync(_))
        .WillOnce(SetWorkItem(&test_work_item));
  }

  App* app = NULL;
  EXPECT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kGuid1), &app));

  EXPECT_SUCCEEDED(app_bundle_->checkForUpdate());
  app_bundle_->CompleteAsyncCall();  // Simulate thread completion.
  EXPECT_SUCCEEDED(app_bundle_->install());
  app_bundle_->CompleteAsyncCall();  // Simulate thread completion.

  EXPECT_SUCCEEDED(app_bundle_->download());

  app_bundle_->CompleteAsyncCall();  // Simulate thread completion.
}

// The AppBundle does not prevent this, but the apps may enter the error state.
TEST_F(AppBundleInitializedUserTest, download_Twice) {
  TestUserWorkItem test_work_item;
  {
    ::testing::InSequence seq;
    EXPECT_CALL(*worker_, CheckForUpdateAsync(_))
        .WillOnce(SetWorkItem(&test_work_item));
    EXPECT_CALL(*worker_, DownloadAsync(_))
        .Times(2)
        .WillRepeatedly(SetWorkItem(&test_work_item));
  }

  App* app = NULL;
  EXPECT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kGuid1), &app));

  EXPECT_SUCCEEDED(app_bundle_->checkForUpdate());
  app_bundle_->CompleteAsyncCall();  // Simulate thread completion.
  EXPECT_SUCCEEDED(app_bundle_->download());
  app_bundle_->CompleteAsyncCall();  // Simulate thread completion.

  EXPECT_SUCCEEDED(app_bundle_->download());

  app_bundle_->CompleteAsyncCall();  // Simulate thread completion.
}

// Simulates the update check still in progress when download() is called.
TEST_F(AppBundleInitializedUserTest, download_WhileBundleIsBusy) {
  TestUserWorkItem test_work_item;
  EXPECT_CALL(*worker_, CheckForUpdateAsync(_))
      .WillOnce(SetWorkItem(&test_work_item));

  App* app = NULL;
  EXPECT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kGuid1), &app));

  EXPECT_SUCCEEDED(app_bundle_->checkForUpdate());

  EXPECT_EQ(GOOPDATE_E_NON_BLOCKING_CALL_PENDING, app_bundle_->download());

  app_bundle_->CompleteAsyncCall();  // Simulate thread completion.
}

// The AppBundle does not prevent this, but the apps may enter the error state.
TEST_F(AppBundleInitializedMachineTest, download_Twice) {
  TestUserWorkItem test_work_item;
  {
    ::testing::InSequence seq;
    EXPECT_CALL(*worker_, CheckForUpdateAsync(_))
        .WillOnce(SetWorkItem(&test_work_item));
    EXPECT_CALL(*worker_, DownloadAsync(_))
        .Times(2)
        .WillRepeatedly(SetWorkItem(&test_work_item));
  }

  App* app = NULL;
  EXPECT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kGuid1), &app));

  EXPECT_SUCCEEDED(app_bundle_->checkForUpdate());
  app_bundle_->CompleteAsyncCall();  // Simulate thread completion.
  EXPECT_SUCCEEDED(app_bundle_->download());
  app_bundle_->CompleteAsyncCall();  // Simulate thread completion.

  EXPECT_SUCCEEDED(app_bundle_->download());

  app_bundle_->CompleteAsyncCall();  // Simulate thread completion.
}

TEST_F(AppBundleInitializedUserTest, install_WithoutUpdateCheck) {
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED, app_bundle_->install());
}

TEST_F(AppBundleInitializedUserTest, install_WithoutDownload) {
  TestUserWorkItem test_work_item;
  {
    ::testing::InSequence seq;
    EXPECT_CALL(*worker_, CheckForUpdateAsync(_))
        .WillOnce(SetWorkItem(&test_work_item));
    EXPECT_CALL(*worker_, DownloadAndInstallAsync(_))
        .WillOnce(SetWorkItem(&test_work_item));
  }

  App* app = NULL;
  EXPECT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kGuid1), &app));

  EXPECT_SUCCEEDED(app_bundle_->checkForUpdate());
  app_bundle_->CompleteAsyncCall();  // Simulate thread completion.

  EXPECT_SUCCEEDED(app_bundle_->install());

  app_bundle_->CompleteAsyncCall();  // Simulate thread completion.
}

TEST_F(AppBundleInitializedUserTest, install_AfterDownload) {
  TestUserWorkItem test_work_item;
  {
    ::testing::InSequence seq;
    EXPECT_CALL(*worker_, CheckForUpdateAsync(_))
        .WillOnce(SetWorkItem(&test_work_item));
    EXPECT_CALL(*worker_, DownloadAsync(_))
        .WillOnce(SetWorkItem(&test_work_item));
    EXPECT_CALL(*worker_, DownloadAndInstallAsync(_))
        .WillOnce(SetWorkItem(&test_work_item));
  }

  App* app = NULL;
  EXPECT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kGuid1), &app));

  EXPECT_SUCCEEDED(app_bundle_->checkForUpdate());
  app_bundle_->CompleteAsyncCall();  // Simulate thread completion.
  EXPECT_SUCCEEDED(app_bundle_->download());
  app_bundle_->CompleteAsyncCall();  // Simulate thread completion.

  EXPECT_SUCCEEDED(app_bundle_->install());

  app_bundle_->CompleteAsyncCall();  // Simulate thread completion.
}

// The AppBundle does not prevent this, but the apps may enter the error state.
TEST_F(AppBundleInitializedUserTest, install_Twice) {
  TestUserWorkItem test_work_item;
  {
    ::testing::InSequence seq;
    EXPECT_CALL(*worker_, CheckForUpdateAsync(_))
        .WillOnce(SetWorkItem(&test_work_item));
    EXPECT_CALL(*worker_, DownloadAndInstallAsync(_))
        .Times(2)
        .WillRepeatedly(SetWorkItem(&test_work_item));
  }

  App* app = NULL;
  EXPECT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kGuid1), &app));

  EXPECT_SUCCEEDED(app_bundle_->checkForUpdate());
  app_bundle_->CompleteAsyncCall();  // Simulate thread completion.
  EXPECT_SUCCEEDED(app_bundle_->install());
  app_bundle_->CompleteAsyncCall();  // Simulate thread completion.

  EXPECT_SUCCEEDED(app_bundle_->install());

  app_bundle_->CompleteAsyncCall();  // Simulate thread completion.
}

// TODO(omaha): These tests are disabled because CaptureCallerPrimaryToken()
// fails. We could move this to AppBundleWrapper, but we would need to expose
// some functions to AppBundleWrapper.
TEST_F(AppBundleInitializedMachineTest, DISABLED_install_WithoutDownload) {
  TestUserWorkItem test_work_item;
  {
    ::testing::InSequence seq;
    EXPECT_CALL(*worker_, CheckForUpdateAsync(_))
        .WillOnce(SetWorkItem(&test_work_item));
    EXPECT_CALL(*worker_, DownloadAndInstallAsync(_))
        .WillOnce(SetWorkItem(&test_work_item));
  }

  App* app = NULL;
  EXPECT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kGuid1), &app));

  EXPECT_SUCCEEDED(app_bundle_->checkForUpdate());
  app_bundle_->CompleteAsyncCall();  // Simulate thread completion.

  EXPECT_SUCCEEDED(app_bundle_->install());

  app_bundle_->CompleteAsyncCall();  // Simulate thread completion.
}

TEST_F(AppBundleInitializedMachineTest, DISABLED_install_AfterDownload) {
  TestUserWorkItem test_work_item;
  {
    ::testing::InSequence seq;
    EXPECT_CALL(*worker_, CheckForUpdateAsync(_))
        .WillOnce(SetWorkItem(&test_work_item));
    EXPECT_CALL(*worker_, DownloadAsync(_))
        .WillOnce(SetWorkItem(&test_work_item));
    EXPECT_CALL(*worker_, DownloadAndInstallAsync(_))
        .WillOnce(SetWorkItem(&test_work_item));
  }

  App* app = NULL;
  EXPECT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kGuid1), &app));

  EXPECT_SUCCEEDED(app_bundle_->checkForUpdate());
  app_bundle_->CompleteAsyncCall();  // Simulate thread completion.
  EXPECT_SUCCEEDED(app_bundle_->download());
  app_bundle_->CompleteAsyncCall();  // Simulate thread completion.

  EXPECT_SUCCEEDED(app_bundle_->install());

  app_bundle_->CompleteAsyncCall();  // Simulate thread completion.
}

// Simulates the update check still in progress when install is called.
TEST_F(AppBundleInitializedUserTest, install_WhileBundleIsBusy) {
  TestUserWorkItem test_work_item;
  EXPECT_CALL(*worker_, CheckForUpdateAsync(_))
      .WillOnce(SetWorkItem(&test_work_item));

  App* app = NULL;
  EXPECT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kGuid1), &app));

  EXPECT_SUCCEEDED(app_bundle_->checkForUpdate());
  EXPECT_EQ(GOOPDATE_E_NON_BLOCKING_CALL_PENDING, app_bundle_->install());

  app_bundle_->CompleteAsyncCall();  // Simulate thread completion.
}

// Also tests adding another app via the same method.
TEST_F(AppBundlePopulatedRegistryUserTest, createInstalledApp_Present_TwoApps) {
  App *expected_app0, *expected_app1, *expected_app2;
  PopulateDataAndRegistryForRegisteredAndUnInstalledAppsTests(&expected_app0,
                                                              &expected_app1,
                                                              &expected_app2);

  App* app0_created = NULL;
  EXPECT_SUCCEEDED(app_bundle_->createInstalledApp(CComBSTR(kGuid1),
                                                   &app0_created));
  EXPECT_TRUE(app0_created);
  EXPECT_EQ(1, app_bundle_->GetNumberOfApps());

  App* app0 = app_bundle_->GetApp(0);
  EXPECT_EQ(app0_created, app0);
  EXPECT_STREQ(CString(kGuid1).MakeUpper(), app0->app_guid_string());
  EXPECT_SUCCEEDED(expected_app0->put_isEulaAccepted(VARIANT_TRUE));
  ValidateExpectedValues(*expected_app0, *app0);

  // Add a second app to the bundle.

  App* app1_created = NULL;
  EXPECT_SUCCEEDED(app_bundle_->createInstalledApp(CComBSTR(kGuid2),
                                                   &app1_created));
  EXPECT_TRUE(app1_created);
  EXPECT_EQ(2, app_bundle_->GetNumberOfApps());

  long num_apps = 0;  // NOLINT
  EXPECT_SUCCEEDED(app_bundle_->get_Count(&num_apps));
  EXPECT_EQ(2, num_apps);

  App* app1 = app_bundle_->GetApp(1);
  EXPECT_EQ(app1_created, app1);
  EXPECT_STREQ(CString(kGuid2).MakeUpper(), app1->app_guid_string());
  SetDisplayName(kDefaultAppName, expected_app1);
  EXPECT_SUCCEEDED(expected_app1->put_isEulaAccepted(VARIANT_TRUE));
  ValidateExpectedValues(*expected_app1, *app1);

  // Verify COM methods return the same values as the C++ methods used above.
  EXPECT_SUCCEEDED(app_bundle_->get_Count(&num_apps));
  EXPECT_EQ(2, num_apps);
  App* app0_obtained = NULL;
  EXPECT_SUCCEEDED(app_bundle_->get_Item(0, &app0_obtained));
  EXPECT_EQ(app0, app0_obtained);
  App* app1_obtained = NULL;
  EXPECT_SUCCEEDED(app_bundle_->get_Item(1, &app1_obtained));
  EXPECT_EQ(app1, app1_obtained);
}

TEST_F(AppBundlePopulatedRegistryMachineTest, createInstalledApp_Present) {
  App *expected_app0, *expected_app1, *expected_app2;
  PopulateDataAndRegistryForRegisteredAndUnInstalledAppsTests(&expected_app0,
                                                              &expected_app1,
                                                              &expected_app2);

  App* app0_created = NULL;
  EXPECT_SUCCEEDED(app_bundle_->createInstalledApp(CComBSTR(kGuid1),
                                                   &app0_created));
  EXPECT_TRUE(app0_created);
  EXPECT_EQ(1, app_bundle_->GetNumberOfApps());

  App* app0 = app_bundle_->GetApp(0);
  EXPECT_EQ(app0_created, app0);
  EXPECT_STREQ(CString(kGuid1).MakeUpper(), app0->app_guid_string());
  EXPECT_SUCCEEDED(expected_app0->put_isEulaAccepted(VARIANT_TRUE));
  ValidateExpectedValues(*expected_app0, *app0);

  long num_apps = 0;  // NOLINT
  EXPECT_SUCCEEDED(app_bundle_->get_Count(&num_apps));
  EXPECT_EQ(1, num_apps);
}

TEST_F(AppBundlePopulatedRegistryMachineTest, ExperimentLabels) {
  const TCHAR* const kExpiredExperimentLabel =
      _T("omaha=v3_23_9_int|Fri, 14 Mar 2014 23:36:18 GMT");
  const TCHAR* const kValidExperimentLabel =
      _T("omaha=v3_23_9_int|Wed, 14 Mar 2029 23:36:18 GMT");

  App *expected_app0 = NULL;
  App *expected_app1 = NULL;
  App *expected_app2 = NULL;
  PopulateDataAndRegistryForRegisteredAndUnInstalledAppsTests(&expected_app0,
                                                              &expected_app1,
                                                              &expected_app2);
  EXPECT_SUCCEEDED(RegKey::SetValue(
      app_registry_utils::GetAppClientStateKey(true, kGuid1),
      kRegValueExperimentLabels,
      kExpiredExperimentLabel));
  EXPECT_STREQ(_T(""), expected_app0->GetExperimentLabels());

  EXPECT_SUCCEEDED(RegKey::SetValue(
      app_registry_utils::GetAppClientStateKey(true, kGuid1),
      kRegValueExperimentLabels,
      kValidExperimentLabel));
  EXPECT_STREQ(kValidExperimentLabel, expected_app0->GetExperimentLabels());
}

// TODO(omaha3): Test that the same app can be added to different bundles for
// each of the create methods.
TEST_F(AppBundlePopulatedRegistryUserTest, createInstalledApp_SameAppTwice) {
  App *expected_app0, *expected_app1, *expected_app2;
  PopulateDataAndRegistryForRegisteredAndUnInstalledAppsTests(&expected_app0,
                                                              &expected_app1,
                                                              &expected_app2);

  App* app0_created = NULL;
  EXPECT_SUCCEEDED(app_bundle_->createInstalledApp(CComBSTR(kGuid1),
                                                   &app0_created));
  EXPECT_TRUE(app0_created);
  EXPECT_EQ(1, app_bundle_->GetNumberOfApps());

  App* app0 = app_bundle_->GetApp(0);
  EXPECT_EQ(app0_created, app0);
  EXPECT_STREQ(CString(kGuid1).MakeUpper(), app0->app_guid_string());
  EXPECT_SUCCEEDED(expected_app0->put_isEulaAccepted(VARIANT_TRUE));
  ValidateExpectedValues(*expected_app0, *app0);

  // Attempt to add the same app to the bundle again.

  App* app1_created = NULL;
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED,
            app_bundle_->createInstalledApp(CComBSTR(kGuid1), &app1_created));
  EXPECT_FALSE(app1_created);
  EXPECT_EQ(1, app_bundle_->GetNumberOfApps());
}

TEST_F(AppBundlePopulatedRegistryUserTest, createInstalledApp_NotPresent) {
  App *expected_app0, *expected_app1, *expected_app2;
  PopulateDataAndRegistryForRegisteredAndUnInstalledAppsTests(&expected_app0,
                                                              &expected_app1,
                                                              &expected_app2);

  App* app0_created = NULL;
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            app_bundle_->createInstalledApp(
                CComBSTR(_T("{2D5F8E16-B56B-496a-BA8B-3A0B5EC17F4F}")),
                &app0_created));
  EXPECT_FALSE(app0_created);
  EXPECT_EQ(0, app_bundle_->GetNumberOfApps());
}

TEST_F(AppBundlePopulatedRegistryUserTest,
       createInstalledApp_NoAppsRegistered) {
  App* app0_created = NULL;
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            app_bundle_->createInstalledApp(CComBSTR(kGuid1), &app0_created));
  EXPECT_FALSE(app0_created);
  EXPECT_EQ(0, app_bundle_->GetNumberOfApps());
}

TEST_F(AppBundlePopulatedRegistryUserTest,
       createInstalledApp_ExternalUpdaterRunning_NotPresent) {
  scoped_event event;
  EXPECT_HRESULT_SUCCEEDED(
      goopdate_utils::CreateExternalUpdaterActiveEvent(kGuid1,
                                                       is_machine_,
                                                       &event));
  ASSERT_TRUE(get(event));

  App* app0_created = NULL;
  EXPECT_EQ(GOOPDATE_E_APP_USING_EXTERNAL_UPDATER,
            app_bundle_->createInstalledApp(CComBSTR(kGuid1), &app0_created));
  EXPECT_FALSE(app0_created);
  EXPECT_EQ(0, app_bundle_->GetNumberOfApps());
}

TEST_F(AppBundlePopulatedRegistryUserTest,
       createInstalledApp_ExternalUpdaterRunning_Present) {
  App *expected_app0, *expected_app1, *expected_app2;
  PopulateDataAndRegistryForRegisteredAndUnInstalledAppsTests(&expected_app0,
                                                              &expected_app1,
                                                              &expected_app2);
  scoped_event event;
  EXPECT_HRESULT_SUCCEEDED(
      goopdate_utils::CreateExternalUpdaterActiveEvent(kGuid1,
                                                       is_machine_,
                                                       &event));
  ASSERT_TRUE(get(event));

  App* app0_created = NULL;
  EXPECT_EQ(GOOPDATE_E_APP_USING_EXTERNAL_UPDATER,
            app_bundle_->createInstalledApp(CComBSTR(kGuid1), &app0_created));
  EXPECT_FALSE(app0_created);
  EXPECT_EQ(0, app_bundle_->GetNumberOfApps());
}

TEST_F(AppBundlePopulatedRegistryUserTest,
       createInstalledApp_ExternalUpdaterRunning_Present_Release) {
  App *expected_app0, *expected_app1, *expected_app2;
  PopulateDataAndRegistryForRegisteredAndUnInstalledAppsTests(&expected_app0,
                                                              &expected_app1,
                                                              &expected_app2);

  App* app0_created = NULL;
  EXPECT_HRESULT_SUCCEEDED(app_bundle_->createInstalledApp(CComBSTR(kGuid1),
                                                           &app0_created));
  EXPECT_TRUE(app0_created);
  EXPECT_EQ(1, app_bundle_->GetNumberOfApps());

  scoped_event event;
  EXPECT_EQ(GOOPDATE_E_APP_USING_EXTERNAL_UPDATER,
            goopdate_utils::CreateExternalUpdaterActiveEvent(kGuid1,
                                                             is_machine_,
                                                             &event));
  ASSERT_FALSE(get(event));

  app_bundle_.reset();
  app_bundle_ = model_->CreateAppBundle(is_machine_);
  EXPECT_TRUE(app_bundle_.get());

  EXPECT_HRESULT_SUCCEEDED(
      goopdate_utils::CreateExternalUpdaterActiveEvent(kGuid1,
                                                       is_machine_,
                                                       &event));
  ASSERT_TRUE(get(event));
}

TEST_F(AppBundlePopulatedRegistryMachineTest,
       createInstalledApp_ExternalUpdaterRunning_NotPresent) {
  scoped_event event;
  EXPECT_HRESULT_SUCCEEDED(
      goopdate_utils::CreateExternalUpdaterActiveEvent(kGuid1,
                                                       is_machine_,
                                                       &event));
  ASSERT_TRUE(get(event));

  App* app0_created = NULL;
  EXPECT_EQ(GOOPDATE_E_APP_USING_EXTERNAL_UPDATER,
            app_bundle_->createInstalledApp(CComBSTR(kGuid1), &app0_created));
  EXPECT_FALSE(app0_created);
  EXPECT_EQ(0, app_bundle_->GetNumberOfApps());
}

TEST_F(AppBundlePopulatedRegistryMachineTest,
       createInstalledApp_ExternalUpdaterRunning_Present) {
  App *expected_app0, *expected_app1, *expected_app2;
  PopulateDataAndRegistryForRegisteredAndUnInstalledAppsTests(&expected_app0,
                                                              &expected_app1,
                                                              &expected_app2);
  scoped_event event;
  EXPECT_HRESULT_SUCCEEDED(
      goopdate_utils::CreateExternalUpdaterActiveEvent(kGuid1,
                                                       is_machine_,
                                                       &event));
  ASSERT_TRUE(get(event));

  App* app0_created = NULL;
  EXPECT_EQ(GOOPDATE_E_APP_USING_EXTERNAL_UPDATER,
            app_bundle_->createInstalledApp(CComBSTR(kGuid1), &app0_created));
  EXPECT_FALSE(app0_created);
  EXPECT_EQ(0, app_bundle_->GetNumberOfApps());
}

TEST_F(AppBundlePopulatedRegistryMachineTest,
       createInstalledApp_ExternalUpdaterRunning_Present_Release) {
  App *expected_app0, *expected_app1, *expected_app2;
  PopulateDataAndRegistryForRegisteredAndUnInstalledAppsTests(&expected_app0,
                                                              &expected_app1,
                                                              &expected_app2);

  App* app0_created = NULL;
  EXPECT_SUCCEEDED(app_bundle_->createInstalledApp(CComBSTR(kGuid1),
                                                   &app0_created));
  EXPECT_TRUE(app0_created);
  EXPECT_EQ(1, app_bundle_->GetNumberOfApps());

  scoped_event event;
  EXPECT_EQ(GOOPDATE_E_APP_USING_EXTERNAL_UPDATER,
            goopdate_utils::CreateExternalUpdaterActiveEvent(kGuid1,
                                                             is_machine_,
                                                             &event));
  ASSERT_FALSE(get(event));

  app_bundle_.reset();
  app_bundle_ = model_->CreateAppBundle(is_machine_);
  ASSERT_TRUE(app_bundle_.get());

  EXPECT_HRESULT_SUCCEEDED(
      goopdate_utils::CreateExternalUpdaterActiveEvent(kGuid1,
                                                       is_machine_,
                                                       &event));
  ASSERT_TRUE(get(event));
}

TEST_F(AppBundlePopulatedRegistryUserTest,
       createInstalledApp_AfterUpdateCheck) {
  App *expected_app0, *expected_app1, *expected_app2;
  PopulateDataAndRegistryForRegisteredAndUnInstalledAppsTests(&expected_app0,
                                                              &expected_app1,
                                                              &expected_app2);

  TestUserWorkItem test_work_item;
  EXPECT_CALL(*worker_, CheckForUpdateAsync(_))
      .WillOnce(SetWorkItem(&test_work_item));

  App* app0_created = NULL;
  EXPECT_SUCCEEDED(app_bundle_->createInstalledApp(CComBSTR(kGuid1),
                                                   &app0_created));

  EXPECT_SUCCEEDED(app_bundle_->checkForUpdate());
  app_bundle_->CompleteAsyncCall();  // Simulate thread completion.

  App* app1_created = NULL;
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED,
            app_bundle_->createInstalledApp(CComBSTR(kGuid2), &app1_created));
}

TEST_F(AppBundlePopulatedRegistryUserTest, createAllInstalledApps) {
  App *expected_app0, *expected_app1, *expected_app2;
  PopulateDataAndRegistryForRegisteredAndUnInstalledAppsTests(&expected_app0,
                                                              &expected_app1,
                                                              &expected_app2);
  EXPECT_HRESULT_SUCCEEDED(RegKey::CreateKey(
      AppendRegKeyPath(USER_REG_CLIENT_STATE, kGuid4)));  // Avoid assert.

  // See comment in machine createAllInstalledApps test.
  const CString incomplete_clients_key =
      AppendRegKeyPath(USER_REG_CLIENTS, kGuid4);
  EXPECT_TRUE(RegKey::HasKey(incomplete_clients_key));
  EXPECT_FALSE(RegKey::HasValue(incomplete_clients_key,
                                kRegValueProductVersion));

  EXPECT_SUCCEEDED(app_bundle_->createAllInstalledApps());
  EXPECT_EQ(2, app_bundle_->GetNumberOfApps());

  App* app0 = app_bundle_->GetApp(0);
  EXPECT_STREQ(CString(kGuid1).MakeUpper(), app0->app_guid_string());
  EXPECT_SUCCEEDED(expected_app0->put_isEulaAccepted(VARIANT_TRUE));
  ValidateExpectedValues(*expected_app0, *app0);

  App* app1 = app_bundle_->GetApp(1);
  EXPECT_STREQ(CString(kGuid2).MakeUpper(), app1->app_guid_string());
  SetDisplayName(kDefaultAppName, expected_app1);
  EXPECT_SUCCEEDED(expected_app1->put_isEulaAccepted(VARIANT_TRUE));
  ValidateExpectedValues(*expected_app1, *app1);

  // Verify COM methods return the same values as the C++ methods used above.
  long num_registered_apps = 0;  // NOLINT
  EXPECT_SUCCEEDED(app_bundle_->get_Count(&num_registered_apps));
  EXPECT_EQ(2, num_registered_apps);
  App* app0_obtained = NULL;
  EXPECT_SUCCEEDED(app_bundle_->get_Item(0, &app0_obtained));
  EXPECT_EQ(app0, app0_obtained);
  App* app1_obtained = NULL;
  EXPECT_SUCCEEDED(app_bundle_->get_Item(1, &app1_obtained));
  EXPECT_EQ(app1, app1_obtained);
}

TEST_F(AppBundlePopulatedRegistryMachineTest, createAllInstalledApps) {
  App *expected_app0, *expected_app1, *expected_app2;
  PopulateDataAndRegistryForRegisteredAndUnInstalledAppsTests(&expected_app0,
                                                              &expected_app1,
                                                              &expected_app2);
  EXPECT_HRESULT_SUCCEEDED(RegKey::CreateKey(
      AppendRegKeyPath(MACHINE_REG_CLIENT_STATE, kGuid4)));  // Avoid assert.

  // An important part of this test is that processing continues and
  // even though there is a key for an app that is not properly
  // registered (e.g. it does not have a pv value or has an invalid pv value).
  // Since this is so important, explicitly check registry was set up correctly.
  // Invalid pv value type is checked in a separate test since it causes an
  // assert.
  // An app without a pv is not added to the bundle.
  const CString incomplete_clients_key =
      AppendRegKeyPath(MACHINE_REG_CLIENTS, kGuid4);
  EXPECT_TRUE(RegKey::HasKey(incomplete_clients_key));
  EXPECT_FALSE(RegKey::HasValue(incomplete_clients_key,
                                kRegValueProductVersion));

  EXPECT_SUCCEEDED(app_bundle_->createAllInstalledApps());
  EXPECT_EQ(2, app_bundle_->GetNumberOfApps());

  App* app0 = app_bundle_->GetApp(0);
  EXPECT_STREQ(CString(kGuid1).MakeUpper(), app0->app_guid_string());
  EXPECT_SUCCEEDED(expected_app0->put_isEulaAccepted(VARIANT_TRUE));
  ValidateExpectedValues(*expected_app0, *app0);

  App* app1 = app_bundle_->GetApp(1);
  EXPECT_STREQ(CString(kGuid2).MakeUpper(), app1->app_guid_string());
  SetDisplayName(kDefaultAppName, expected_app1);
  EXPECT_SUCCEEDED(expected_app1->put_isEulaAccepted(VARIANT_TRUE));
  ValidateExpectedValues(*expected_app1, *app1);

  // Verify COM methods return the same values as the C++ methods used above.
  long num_registered_apps = 0;  // NOLINT
  EXPECT_SUCCEEDED(app_bundle_->get_Count(&num_registered_apps));
  EXPECT_EQ(2, num_registered_apps);
  App* app0_obtained = NULL;
  EXPECT_SUCCEEDED(app_bundle_->get_Item(0, &app0_obtained));
  EXPECT_EQ(app0, app0_obtained);
  App* app1_obtained = NULL;
  EXPECT_SUCCEEDED(app_bundle_->get_Item(1, &app1_obtained));
  EXPECT_EQ(app1, app1_obtained);
}

TEST_F(AppBundlePopulatedRegistryUserTest,
       createAllInstalledApps_InvalidPvValueType) {
  App *expected_app0, *expected_app1, *expected_app2;
  PopulateDataAndRegistryForRegisteredAndUnInstalledAppsTests(&expected_app0,
                                                              &expected_app1,
                                                              &expected_app2);

  // Incorrect data type for pv. See comment in machine createAllInstalledApps
  // test.
  // It is important that this value is alphabetically before the other AppIds.
  // This allows us to test that processing continues despite the bad pv.
  const TCHAR* const kInvalidPvTypeAppId =
      _T("{0150B619-867C-4985-B193-ED309A23EE36}");
  const CString invalid_pv_type_clients_key =
      AppendRegKeyPath(USER_REG_CLIENTS, kInvalidPvTypeAppId);
  const DWORD kInvalidPvDword = 0x3039;
  const TCHAR* const kInvalidPvDwordAsString = _T("\x3039");
  EXPECT_HRESULT_SUCCEEDED(RegKey::SetValue(invalid_pv_type_clients_key,
                                            kRegValueProductVersion,
                                            kInvalidPvDword));
  EXPECT_TRUE(RegKey::HasValue(invalid_pv_type_clients_key,
                               kRegValueProductVersion));
  DWORD value_type = REG_SZ;
  EXPECT_SUCCEEDED(RegKey::GetValueType(invalid_pv_type_clients_key,
                                        kRegValueProductVersion,
                                        &value_type));
  EXPECT_NE(REG_SZ, value_type);
  App* invalid_pv_app = CreateExpectedApp(kInvalidPvTypeAppId);
  invalid_pv_app->current_version()->set_version(kInvalidPvDwordAsString);

  invalid_pv_app->set_days_since_last_active_ping(-1);
  invalid_pv_app->set_days_since_last_roll_call(-1);
  SetDisplayName(kDefaultAppName, invalid_pv_app);

  {
    // An assert occurs when reading the wrong value type.
    ExpectAsserts expect_asserts;
    EXPECT_SUCCEEDED(app_bundle_->createAllInstalledApps());
  }

  EXPECT_EQ(3, app_bundle_->GetNumberOfApps());

  // The invalid pv app is added to the bundle with an unreadable pv.
  App* app0 = app_bundle_->GetApp(0);
  EXPECT_STREQ(kInvalidPvTypeAppId, app0->app_guid_string());
  EXPECT_STREQ(kInvalidPvDwordAsString, app0->current_version()->version());
  EXPECT_SUCCEEDED(invalid_pv_app->put_isEulaAccepted(VARIANT_TRUE));
  ValidateExpectedValues(*invalid_pv_app, *app0);

  App* app1 = app_bundle_->GetApp(1);
  EXPECT_STREQ(CString(kGuid1).MakeUpper(), app1->app_guid_string());
  EXPECT_SUCCEEDED(expected_app0->put_isEulaAccepted(VARIANT_TRUE));
  ValidateExpectedValues(*expected_app0, *app1);

  App* app2 = app_bundle_->GetApp(2);
  EXPECT_STREQ(CString(kGuid2).MakeUpper(), app2->app_guid_string());
  SetDisplayName(kDefaultAppName, expected_app1);
  EXPECT_SUCCEEDED(expected_app1->put_isEulaAccepted(VARIANT_TRUE));
  ValidateExpectedValues(*expected_app1, *app2);
}

TEST_F(AppBundlePopulatedRegistryUserTest,
       createAllInstalledApps_NoAppsRegistered) {
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            app_bundle_->createAllInstalledApps());
  EXPECT_EQ(0, app_bundle_->GetNumberOfApps());

  {
    ExpectAsserts expect_asserts;
    EXPECT_FALSE(app_bundle_->GetApp(0));
  }
}

TEST_F(AppBundlePopulatedRegistryUserTest,
       createAllInstalledApps_OneAppRegistered) {
  CString clients_key_name = AppendRegKeyPath(USER_REG_CLIENTS, kGuid1);
  EXPECT_SUCCEEDED(RegKey::SetValue(clients_key_name,
                                    kRegValueProductVersion,
                                    _T("1.0.0.0")));
  EXPECT_HRESULT_SUCCEEDED(RegKey::CreateKey(
      AppendRegKeyPath(USER_REG_CLIENT_STATE, kGuid1)));  // Avoid assert.

  EXPECT_SUCCEEDED(app_bundle_->createAllInstalledApps());
  EXPECT_EQ(1, app_bundle_->GetNumberOfApps());

  App* app0 = app_bundle_->GetApp(0);
  EXPECT_STREQ(CString(kGuid1).MakeUpper(), app0->app_guid_string());
}

TEST_F(AppBundlePopulatedRegistryUserTest,
       createAllInstalledApps_AfterUpdateCheck) {
  App *expected_app0, *expected_app1, *expected_app2;
  PopulateDataAndRegistryForRegisteredAndUnInstalledAppsTests(&expected_app0,
                                                              &expected_app1,
                                                              &expected_app2);
  EXPECT_HRESULT_SUCCEEDED(RegKey::CreateKey(
      AppendRegKeyPath(USER_REG_CLIENT_STATE, kGuid4)));  // Avoid assert.

  TestUserWorkItem test_work_item;
  EXPECT_CALL(*worker_, CheckForUpdateAsync(_))
      .WillOnce(SetWorkItem(&test_work_item));

  EXPECT_SUCCEEDED(app_bundle_->createAllInstalledApps());

  EXPECT_SUCCEEDED(app_bundle_->checkForUpdate());
  app_bundle_->CompleteAsyncCall();  // Simulate thread completion.

  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED, app_bundle_->createAllInstalledApps());
}

TEST_F(AppBundlePopulatedRegistryUserTest, createApp_After_createInstalledApp) {
  App *expected_app0, *expected_app1, *expected_app2;
  PopulateDataAndRegistryForRegisteredAndUnInstalledAppsTests(&expected_app0,
                                                              &expected_app1,
                                                              &expected_app2);

  App* app0_created = NULL;
  EXPECT_SUCCEEDED(app_bundle_->createInstalledApp(CComBSTR(kGuid1),
                                                   &app0_created));

  App* app1_created = NULL;
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED,
            app_bundle_->createApp(CComBSTR(kGuid2), &app1_created));
}

TEST_F(AppBundlePopulatedRegistryUserTest, createApp_createAllInstalledApps) {
  App *expected_app0, *expected_app1, *expected_app2;
  PopulateDataAndRegistryForRegisteredAndUnInstalledAppsTests(&expected_app0,
                                                              &expected_app1,
                                                              &expected_app2);
  EXPECT_HRESULT_SUCCEEDED(RegKey::CreateKey(
      AppendRegKeyPath(USER_REG_CLIENT_STATE, kGuid4)));  // Avoid assert.

  EXPECT_SUCCEEDED(app_bundle_->createAllInstalledApps());

  App* app0_created = NULL;
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED,
            app_bundle_->createApp(CComBSTR(kGuid1), &app0_created));
}

TEST_F(AppBundleInitializedUserTest, createInstalledApp_After_createApp) {
  App* app0_created = NULL;
  EXPECT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kGuid1), &app0_created));

  App* app1_created = NULL;
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED,
            app_bundle_->createInstalledApp(CComBSTR(kGuid2), &app1_created));
}

TEST_F(AppBundlePopulatedRegistryUserTest,
       createInstalledApp_After_createAllInstalledApps) {
  App *expected_app0, *expected_app1, *expected_app2;
  PopulateDataAndRegistryForRegisteredAndUnInstalledAppsTests(&expected_app0,
                                                              &expected_app1,
                                                              &expected_app2);
  EXPECT_HRESULT_SUCCEEDED(RegKey::CreateKey(
      AppendRegKeyPath(USER_REG_CLIENT_STATE, kGuid4)));  // Avoid assert.

  EXPECT_SUCCEEDED(app_bundle_->createAllInstalledApps());

  App* app0_created = NULL;
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED,
            app_bundle_->createInstalledApp(CComBSTR(kGuid1), &app0_created));
}

TEST_F(AppBundleInitializedUserTest, createAllInstalledApps_After_createApp) {
  App* app0_created = NULL;
  EXPECT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kGuid1), &app0_created));

  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED, app_bundle_->createAllInstalledApps());
}

TEST_F(AppBundlePopulatedRegistryUserTest,
       createAllInstalledApps_After_createInstalledApp) {
  App *expected_app0, *expected_app1, *expected_app2;
  PopulateDataAndRegistryForRegisteredAndUnInstalledAppsTests(&expected_app0,
                                                              &expected_app1,
                                                              &expected_app2);

  App* app0_created = NULL;
  EXPECT_SUCCEEDED(app_bundle_->createInstalledApp(CComBSTR(kGuid1),
                                                   &app0_created));

  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED, app_bundle_->createAllInstalledApps());
}

TEST_F(AppBundlePopulatedRegistryUserTest,
       createAllInstalledApps_Twice) {
  App *expected_app0, *expected_app1, *expected_app2;
  PopulateDataAndRegistryForRegisteredAndUnInstalledAppsTests(&expected_app0,
                                                              &expected_app1,
                                                              &expected_app2);

  App* app0_created = NULL;
  EXPECT_SUCCEEDED(app_bundle_->createInstalledApp(CComBSTR(kGuid1),
                                                   &app0_created));

  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED, app_bundle_->createAllInstalledApps());
}

TEST_F(AppBundlePopulatedRegistryUserTest,
       createAllInstalledApps_ExternalUpdaterRunning) {
  App *expected_app0, *expected_app1, *expected_app2;
  PopulateDataAndRegistryForRegisteredAndUnInstalledAppsTests(&expected_app0,
                                                              &expected_app1,
                                                              &expected_app2);
  EXPECT_SUCCEEDED(RegKey::CreateKey(
      AppendRegKeyPath(USER_REG_CLIENT_STATE, kGuid4)));  // Avoid assert.

  scoped_event event;
  EXPECT_HRESULT_SUCCEEDED(
      goopdate_utils::CreateExternalUpdaterActiveEvent(kGuid1,
                                                       is_machine_,
                                                       &event));
  ASSERT_TRUE(get(event));

  EXPECT_SUCCEEDED(app_bundle_->createAllInstalledApps());

  ASSERT_EQ(1, app_bundle_->GetNumberOfApps());
  App* app1 = app_bundle_->GetApp(0);
  EXPECT_STREQ(CString(kGuid2).MakeUpper(), app1->app_guid_string());
  SetDisplayName(kDefaultAppName, expected_app1);
  EXPECT_SUCCEEDED(expected_app1->put_isEulaAccepted(VARIANT_TRUE));
  ValidateExpectedValues(*expected_app1, *app1);
}

TEST_F(AppBundlePopulatedRegistryMachineTest,
       createAllInstalledApps_ExternalUpdaterRunning) {
  App *expected_app0, *expected_app1, *expected_app2;
  PopulateDataAndRegistryForRegisteredAndUnInstalledAppsTests(&expected_app0,
                                                              &expected_app1,
                                                              &expected_app2);
  EXPECT_SUCCEEDED(RegKey::CreateKey(
      AppendRegKeyPath(MACHINE_REG_CLIENT_STATE, kGuid4)));  // Avoid assert.

  scoped_event event;
  EXPECT_HRESULT_SUCCEEDED(
      goopdate_utils::CreateExternalUpdaterActiveEvent(kGuid1,
                                                       is_machine_,
                                                       &event));
  ASSERT_TRUE(get(event));

  EXPECT_HRESULT_SUCCEEDED(app_bundle_->createAllInstalledApps());

  ASSERT_EQ(1, app_bundle_->GetNumberOfApps());
  App* app1 = app_bundle_->GetApp(0);
  EXPECT_STREQ(CString(kGuid2).MakeUpper(), app1->app_guid_string());
  SetDisplayName(kDefaultAppName, expected_app1);
  EXPECT_SUCCEEDED(expected_app1->put_isEulaAccepted(VARIANT_TRUE));
  ValidateExpectedValues(*expected_app1, *app1);
}

// TODO(omaha): Enable if we end up needing such a function.
#if 0
TEST_F(AppBundlePopulatedRegistryMachineTest, createUninstalledApps) {
  App *expected_app0, *expected_app1, *expected_app2;
  PopulateDataAndRegistryForRegisteredAndUnInstalledAppsTests(&expected_app0,
                                                              &expected_app1,
                                                              &expected_app2);

  EXPECT_SUCCEEDED(app_bundle_->createUninstalledApps());
  EXPECT_EQ(1, app_bundle_->GetNumberOfApps());

  App* app0 = app_bundle_->GetApp(0);
  EXPECT_STREQ(kGuid3, app0->app_guid_string());
  ValidateExpectedValues(*expected_app2, *app0);
}

TEST_F(AppBundlePopulatedRegistryUserTest, createUninstalledApps) {
  App *expected_app0, *expected_app1, *expected_app2;
  PopulateDataAndRegistryForRegisteredAndUnInstalledAppsTests(&expected_app0,
                                                              &expected_app1,
                                                              &expected_app2);

  EXPECT_SUCCEEDED(app_bundle_->createUninstalledApps());
  EXPECT_EQ(1, app_bundle_->GetNumberOfApps());

  App* app0 = app_bundle_->GetApp(0);
  EXPECT_STREQ(kGuid3, app0->app_guid_string());
  ValidateExpectedValues(*expected_app2, *app0);
}
#endif

//
// State tests.
//

class AppBundleStateUserTest : public AppBundleTest {
 protected:
  AppBundleStateUserTest()
      : AppBundleTest(false) {}

  virtual void SetUp() {
    AppBundleTest::SetUp();

    InitializeRegistryForTest(false);
  }

  virtual void TearDown() {
    AppBundleTest::TearDown();
  }

  // Writing name avoids needing to create the ResourceManager.
  void CreateClientsKeyForApp1() {
    CString clients_key = AppendRegKeyPath(USER_REG_CLIENTS, kGuid1);
    EXPECT_SUCCEEDED(RegKey::SetValue(clients_key,
                                      kRegValueProductVersion,
                                      _T("1.2.3.4")));
    EXPECT_SUCCEEDED(RegKey::SetValue(clients_key,
                                      kRegValueAppName,
                                      _T("Test App")));
  }
};


class AppBundleStateInitUserTest : public AppBundleStateUserTest {
 protected:
  AppBundleStateInitUserTest() : AppBundleStateUserTest() {}

  virtual void SetUp() {
    AppBundleStateUserTest::SetUp();

    // Nothing to do since the budle starts in this state.
  }
};

class AppBundleStateInitializedUserTest : public AppBundleStateUserTest {
 protected:
  AppBundleStateInitializedUserTest() : AppBundleStateUserTest() {}

  virtual void SetUp() {
    AppBundleStateUserTest::SetUp();
    EXPECT_SUCCEEDED(app_bundle_->put_displayName(CComBSTR(_T("My Bundle"))));
    EXPECT_SUCCEEDED(app_bundle_->put_displayLanguage(CComBSTR(_T("en"))));
    EXPECT_SUCCEEDED(app_bundle_->initialize());
  }
};

class AppBundleStateBusyUserTest : public AppBundleStateUserTest {
 protected:
  AppBundleStateBusyUserTest() : AppBundleStateUserTest() {}

  virtual void SetUp() {
    AppBundleStateUserTest::SetUp();

    TestUserWorkItem test_work_item;
    app_bundle_->set_user_work_item(&test_work_item);

    SetAppBundleStateForUnitTest(app_bundle_.get(),
                                 new fsm::AppBundleStateBusy);
  }
};

class AppBundleStateReadyUserTest : public AppBundleStateUserTest {
 protected:
  AppBundleStateReadyUserTest() : AppBundleStateUserTest() {}

  virtual void SetUp() {
    AppBundleStateUserTest::SetUp();
    EXPECT_SUCCEEDED(app_bundle_->put_displayName(CComBSTR(_T("My Bundle"))));
    EXPECT_SUCCEEDED(app_bundle_->put_displayLanguage(CComBSTR(_T("en"))));
    EXPECT_SUCCEEDED(app_bundle_->initialize());

    // In all cases, the bundle should have at least one app.
    // The downloadPackage test needs an installed app.
    CreateClientsKeyForApp1();
    App* app = NULL;
    EXPECT_SUCCEEDED(app_bundle_->createInstalledApp(CComBSTR(kGuid1), &app));

    SetAppBundleStateForUnitTest(app_bundle_.get(),
                                 new fsm::AppBundleStateReady);
  }
};

class AppBundleStatePausedUserTest : public AppBundleStateUserTest {
 protected:
  AppBundleStatePausedUserTest() : AppBundleStateUserTest() {}

  virtual void SetUp() {
    AppBundleStateUserTest::SetUp();
    SetAppBundleStateForUnitTest(app_bundle_.get(),
                                 new fsm::AppBundleStatePaused);
  }
};

class AppBundleStateStoppedUserTest : public AppBundleStateUserTest {
 protected:
  AppBundleStateStoppedUserTest() : AppBundleStateUserTest() {}

  virtual void SetUp() {
    AppBundleStateUserTest::SetUp();
    SetAppBundleStateForUnitTest(app_bundle_.get(),
                                 new fsm::AppBundleStateStopped);
  }
};

// Init.

TEST_F(AppBundleStateInitUserTest, Properties) {
  TestPropertyReflexiveness();
  EXPECT_EQ(STATE_INIT, GetBundleState());
}

TEST_F(AppBundleStateInitUserTest, CountAndItem) {
  long num_apps = 0;  // NOLINT
  EXPECT_SUCCEEDED(app_bundle_->get_Count(&num_apps));
  EXPECT_EQ(0, num_apps);

  App* app0_obtained = NULL;
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_INVALID_INDEX),
            app_bundle_->get_Item(0, &app0_obtained));
  EXPECT_FALSE(app0_obtained);

  EXPECT_EQ(STATE_INIT, GetBundleState());
}

TEST_F(AppBundleStateInitUserTest, put_altTokens) {
  CAccessToken access_token;
  access_token.GetProcessToken(TOKEN_DUPLICATE);
  ULONG_PTR process_token =
      reinterpret_cast<ULONG_PTR>(access_token.GetHandle());
  EXPECT_SUCCEEDED(app_bundle_->put_altTokens(process_token,
                                              process_token,
                                              ::GetCurrentProcessId()));
  EXPECT_EQ(STATE_INIT, GetBundleState());
}

TEST_F(AppBundleStateInitUserTest, initialize) {
  EXPECT_SUCCEEDED(app_bundle_->put_displayName(CComBSTR(_T("My Bundle"))));
  EXPECT_SUCCEEDED(app_bundle_->put_displayLanguage(CComBSTR(_T("en"))));
  EXPECT_SUCCEEDED(app_bundle_->initialize());
  EXPECT_EQ(STATE_INITIALIZED, GetBundleState());
}

TEST_F(AppBundleStateInitUserTest, createApp) {
  App* app = NULL;
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED,
            app_bundle_->createApp(CComBSTR(kGuid1), &app));
  EXPECT_EQ(STATE_INIT, GetBundleState());
}

TEST_F(AppBundleStateInitUserTest, createInstalledApp) {
  CreateClientsKeyForApp1();

  App* app = NULL;
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED,
            app_bundle_->createInstalledApp(CComBSTR(kGuid1), &app));
  EXPECT_EQ(STATE_INIT, GetBundleState());
}

TEST_F(AppBundleStateInitUserTest, createAllInstalledApps) {
  CreateClientsKeyForApp1();

  App* app = NULL;
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED,
            app_bundle_->createAllInstalledApps());
  EXPECT_EQ(STATE_INIT, GetBundleState());
}

TEST_F(AppBundleStateInitUserTest, checkForUpdate) {
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED, app_bundle_->checkForUpdate());
  EXPECT_EQ(STATE_INIT, GetBundleState());
}

TEST_F(AppBundleStateInitUserTest, download) {
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED, app_bundle_->download());
  EXPECT_EQ(STATE_INIT, GetBundleState());
}

TEST_F(AppBundleStateInitUserTest, install) {
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED, app_bundle_->install());
  EXPECT_EQ(STATE_INIT, GetBundleState());
}

TEST_F(AppBundleStateInitUserTest, updateAllApps) {
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED, app_bundle_->updateAllApps());
  EXPECT_EQ(STATE_INIT, GetBundleState());
}

TEST_F(AppBundleStateInitUserTest, stop) {
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED, app_bundle_->stop());
  EXPECT_EQ(STATE_INIT, GetBundleState());
}

TEST_F(AppBundleStateInitUserTest, pause) {
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED, app_bundle_->pause());
  EXPECT_EQ(STATE_INIT, GetBundleState());
}

TEST_F(AppBundleStateInitUserTest, resume) {
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED, app_bundle_->resume());
  EXPECT_EQ(STATE_INIT, GetBundleState());
}

TEST_F(AppBundleStateInitUserTest, isBusy) {
  VARIANT_BOOL is_busy = VARIANT_TRUE;
  EXPECT_SUCCEEDED(app_bundle_->isBusy(&is_busy));
  EXPECT_EQ(VARIANT_FALSE, is_busy);
  EXPECT_EQ(STATE_INIT, GetBundleState());
}

TEST_F(AppBundleStateInitUserTest, downloadPackage) {
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED,
            app_bundle_->downloadPackage(CComBSTR(kGuid1),
                                         CComBSTR(_T("package"))));
  EXPECT_EQ(STATE_INIT, GetBundleState());
}

// TODO(omaha): Check the state or remove this function.
TEST_F(AppBundleStateInitUserTest, get_currentState) {
  VARIANT current_state;
  ExpectAsserts expect_asserts;  // Not yet implemented.
  EXPECT_EQ(E_NOTIMPL, app_bundle_->get_currentState(&current_state));
  EXPECT_EQ(STATE_INIT, GetBundleState());
}

TEST_F(AppBundleStateInitUserTest, CompleteAsyncCall) {
  {
    ExpectAsserts expect_asserts;
    app_bundle_->CompleteAsyncCall();
  }

  EXPECT_EQ(STATE_INIT, GetBundleState());
}

// Initialized.

TEST_F(AppBundleStateInitializedUserTest, Properties) {
  TestPropertyReflexiveness();
  EXPECT_EQ(STATE_INITIALIZED, GetBundleState());
}

TEST_F(AppBundleStateInitializedUserTest, CountAndItem) {
  long num_apps = 0;  // NOLINT
  EXPECT_SUCCEEDED(app_bundle_->get_Count(&num_apps));
  EXPECT_EQ(0, num_apps);

  App* app0_obtained = NULL;
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_INVALID_INDEX),
            app_bundle_->get_Item(0, &app0_obtained));
  EXPECT_FALSE(app0_obtained);

  EXPECT_EQ(STATE_INITIALIZED, GetBundleState());
}

TEST_F(AppBundleStateInitializedUserTest, put_altTokens) {
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED, app_bundle_->put_altTokens(1, 2, 3));
  EXPECT_EQ(STATE_INITIALIZED, GetBundleState());
}

TEST_F(AppBundleStateInitializedUserTest, initialize) {
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED, app_bundle_->initialize());
  EXPECT_EQ(STATE_INITIALIZED, GetBundleState());
}

TEST_F(AppBundleStateInitializedUserTest, createApp) {
  App* app = NULL;
  EXPECT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kGuid1), &app));
  EXPECT_EQ(STATE_INITIALIZED, GetBundleState());
}

TEST_F(AppBundleStateInitializedUserTest, createInstalledApp) {
  CreateClientsKeyForApp1();

  App* app = NULL;
  EXPECT_SUCCEEDED(app_bundle_->createInstalledApp(CComBSTR(kGuid1), &app));
  EXPECT_EQ(STATE_INITIALIZED, GetBundleState());
}

TEST_F(AppBundleStateInitializedUserTest, createAllInstalledApps) {
  CreateClientsKeyForApp1();
  EXPECT_HRESULT_SUCCEEDED(RegKey::CreateKey(
      AppendRegKeyPath(USER_REG_CLIENT_STATE, kGuid1)));  // Avoid assert.

  App* app = NULL;
  EXPECT_SUCCEEDED(app_bundle_->createAllInstalledApps());
  EXPECT_EQ(STATE_INITIALIZED, GetBundleState());
}

TEST_F(AppBundleStateInitializedUserTest, checkForUpdate) {
  App* app = NULL;
  EXPECT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kGuid1), &app));
  TestUserWorkItem test_work_item;
  EXPECT_CALL(*worker_, CheckForUpdateAsync(_))
      .WillOnce(SetWorkItem(&test_work_item));

  EXPECT_SUCCEEDED(app_bundle_->checkForUpdate());
  EXPECT_EQ(STATE_BUSY, GetBundleState());

  EXPECT_FALSE(app_bundle_->is_auto_update());
}

TEST_F(AppBundleStateInitializedUserTest, download) {
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED, app_bundle_->download());
  EXPECT_EQ(STATE_INITIALIZED, GetBundleState());
}

TEST_F(AppBundleStateInitializedUserTest, install) {
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED, app_bundle_->install());
  EXPECT_EQ(STATE_INITIALIZED, GetBundleState());
}

TEST_F(AppBundleStateInitializedUserTest, updateAllApps) {
  CreateClientsKeyForApp1();
  EXPECT_HRESULT_SUCCEEDED(RegKey::CreateKey(
      AppendRegKeyPath(USER_REG_CLIENT_STATE, kGuid1)));  // Avoid assert.

  TestUserWorkItem test_work_item;
  EXPECT_CALL(*worker_, UpdateAllAppsAsync(_))
      .WillOnce(SetWorkItem(&test_work_item));

  EXPECT_SUCCEEDED(app_bundle_->updateAllApps());
  EXPECT_EQ(STATE_BUSY, GetBundleState());

  EXPECT_TRUE(app_bundle_->is_auto_update());
}

TEST_F(AppBundleStateInitializedUserTest, stop) {
  EXPECT_SUCCEEDED(app_bundle_->stop());
  EXPECT_EQ(STATE_STOPPED, GetBundleState());
}

TEST_F(AppBundleStateInitializedUserTest, pause) {
  EXPECT_SUCCEEDED(app_bundle_->pause());
  EXPECT_EQ(STATE_PAUSED, GetBundleState());
}

TEST_F(AppBundleStateInitializedUserTest, resume) {
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED, app_bundle_->resume());
  EXPECT_EQ(STATE_INITIALIZED, GetBundleState());
}

TEST_F(AppBundleStateInitializedUserTest, isBusy) {
  VARIANT_BOOL is_busy = VARIANT_TRUE;
  EXPECT_SUCCEEDED(app_bundle_->isBusy(&is_busy));
  EXPECT_EQ(VARIANT_FALSE, is_busy);
  EXPECT_EQ(STATE_INITIALIZED, GetBundleState());
}

// downloadPackage() returns E_INVALIDARG because the app has no packages.
// TODO(omaha): Add the package so downloadPackage can succeed.
TEST_F(AppBundleStateInitializedUserTest, downloadPackage) {
  CreateClientsKeyForApp1();
  App* app = NULL;
  EXPECT_SUCCEEDED(app_bundle_->createInstalledApp(CComBSTR(kGuid1), &app));

  EXPECT_EQ(
      E_INVALIDARG,
      app_bundle_->downloadPackage(CComBSTR(kGuid1), CComBSTR(_T("package"))));
  EXPECT_EQ(STATE_INITIALIZED, GetBundleState());

  EXPECT_FALSE(app_bundle_->is_auto_update());
}

TEST_F(AppBundleStateInitializedUserTest, get_currentState) {
  VARIANT current_state;
  ExpectAsserts expect_asserts;  // Not yet implemented.
  EXPECT_EQ(E_NOTIMPL, app_bundle_->get_currentState(&current_state));
  EXPECT_EQ(STATE_INITIALIZED, GetBundleState());
}

TEST_F(AppBundleStateInitializedUserTest, CompleteAsyncCall) {
  {
    ExpectAsserts expect_asserts;
    app_bundle_->CompleteAsyncCall();
  }

  EXPECT_EQ(STATE_INITIALIZED, GetBundleState());
}

// Busy.

TEST_F(AppBundleStateBusyUserTest, Properties) {
  TestPropertyReflexiveness();
  EXPECT_EQ(STATE_BUSY, GetBundleState());
}

TEST_F(AppBundleStateBusyUserTest, CountAndItem) {
  long num_apps = 0;  // NOLINT
  EXPECT_SUCCEEDED(app_bundle_->get_Count(&num_apps));
  EXPECT_EQ(0, num_apps);

  App* app0_obtained = NULL;
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_INVALID_INDEX),
            app_bundle_->get_Item(0, &app0_obtained));
  EXPECT_FALSE(app0_obtained);

  EXPECT_EQ(STATE_BUSY, GetBundleState());
}

TEST_F(AppBundleStateBusyUserTest, put_altTokens) {
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED, app_bundle_->put_altTokens(1, 2, 3));
  EXPECT_EQ(STATE_BUSY, GetBundleState());
}

TEST_F(AppBundleStateBusyUserTest, initialize) {
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED, app_bundle_->initialize());
  EXPECT_EQ(STATE_BUSY, GetBundleState());
}

TEST_F(AppBundleStateBusyUserTest, createApp) {
  App* app = NULL;
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED,
            app_bundle_->createApp(CComBSTR(kGuid1), &app));
  EXPECT_EQ(STATE_BUSY, GetBundleState());
}

TEST_F(AppBundleStateBusyUserTest, createInstalledApp) {
  CreateClientsKeyForApp1();

  App* app = NULL;
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED,
            app_bundle_->createInstalledApp(CComBSTR(kGuid1), &app));
  EXPECT_EQ(STATE_BUSY, GetBundleState());
}

TEST_F(AppBundleStateBusyUserTest, createAllInstalledApps) {
  CreateClientsKeyForApp1();

  App* app = NULL;
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED,
            app_bundle_->createAllInstalledApps());
  EXPECT_EQ(STATE_BUSY, GetBundleState());
}

TEST_F(AppBundleStateBusyUserTest, checkForUpdate) {
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED, app_bundle_->checkForUpdate());
  EXPECT_EQ(STATE_BUSY, GetBundleState());
}

TEST_F(AppBundleStateBusyUserTest, download) {
  EXPECT_EQ(GOOPDATE_E_NON_BLOCKING_CALL_PENDING, app_bundle_->download());
  EXPECT_EQ(STATE_BUSY, GetBundleState());
}

TEST_F(AppBundleStateBusyUserTest, install) {
  EXPECT_EQ(GOOPDATE_E_NON_BLOCKING_CALL_PENDING, app_bundle_->install());
  EXPECT_EQ(STATE_BUSY, GetBundleState());
}

TEST_F(AppBundleStateBusyUserTest, updateAllApps) {
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED, app_bundle_->updateAllApps());
  EXPECT_EQ(STATE_BUSY, GetBundleState());
}

TEST_F(AppBundleStateBusyUserTest, stop_Succeeds) {
  EXPECT_CALL(*worker_, Stop(app_bundle_.get()))
      .WillOnce(Return(S_OK));

  EXPECT_SUCCEEDED(app_bundle_->stop());
  EXPECT_EQ(STATE_STOPPED, GetBundleState());
}

TEST_F(AppBundleStateBusyUserTest, stop_Fails) {
  EXPECT_CALL(*worker_, Stop(app_bundle_.get()))
      .WillOnce(Return(kKnownError));

  EXPECT_EQ(kKnownError, app_bundle_->stop());
  EXPECT_EQ(STATE_BUSY, GetBundleState());
}

TEST_F(AppBundleStateBusyUserTest, pause_Succeeds) {
  EXPECT_CALL(*worker_, Pause(app_bundle_.get()))
      .WillOnce(Return(S_OK));

  EXPECT_EQ(S_OK, app_bundle_->pause());
  EXPECT_EQ(STATE_PAUSED, GetBundleState());
}

TEST_F(AppBundleStateBusyUserTest, pause_Fails) {
  EXPECT_CALL(*worker_, Pause(_))
      .WillOnce(Return(kKnownError));

  EXPECT_EQ(kKnownError, app_bundle_->pause());
  EXPECT_EQ(STATE_BUSY, GetBundleState());
}

TEST_F(AppBundleStateBusyUserTest, resume) {
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED, app_bundle_->resume());
  EXPECT_EQ(STATE_BUSY, GetBundleState());
}

TEST_F(AppBundleStateBusyUserTest, isBusy) {
  VARIANT_BOOL is_busy = VARIANT_TRUE;
  EXPECT_SUCCEEDED(app_bundle_->isBusy(&is_busy));
  EXPECT_EQ(VARIANT_TRUE, is_busy);
  EXPECT_EQ(STATE_BUSY, GetBundleState());
}

TEST_F(AppBundleStateBusyUserTest, downloadPackage) {
  EXPECT_EQ(GOOPDATE_E_NON_BLOCKING_CALL_PENDING,
            app_bundle_->downloadPackage(CComBSTR(kGuid1),
                                         CComBSTR(_T("package"))));
  EXPECT_EQ(STATE_BUSY, GetBundleState());
}

TEST_F(AppBundleStateBusyUserTest, get_currentState) {
  VARIANT current_state;
  ExpectAsserts expect_asserts;  // Not yet implemented.
  EXPECT_EQ(E_NOTIMPL, app_bundle_->get_currentState(&current_state));
  EXPECT_EQ(STATE_BUSY, GetBundleState());
}

TEST_F(AppBundleStateBusyUserTest, CompleteAsyncCall) {
  app_bundle_->CompleteAsyncCall();

  EXPECT_EQ(STATE_READY, GetBundleState());
}

// Ready.

TEST_F(AppBundleStateReadyUserTest, Properties) {
  TestPropertyReflexiveness();
  EXPECT_EQ(STATE_READY, GetBundleState());
}

TEST_F(AppBundleStateReadyUserTest, CountAndItem) {
  long num_apps = 0;  // NOLINT
  EXPECT_SUCCEEDED(app_bundle_->get_Count(&num_apps));
  EXPECT_EQ(1, num_apps);

  App* app0_obtained = NULL;
  EXPECT_SUCCEEDED(app_bundle_->get_Item(0, &app0_obtained));
  EXPECT_TRUE(app0_obtained);

  EXPECT_EQ(STATE_READY, GetBundleState());
}

TEST_F(AppBundleStateReadyUserTest, put_altTokens) {
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED, app_bundle_->put_altTokens(1, 2, 3));
  EXPECT_EQ(STATE_READY, GetBundleState());
}

TEST_F(AppBundleStateReadyUserTest, initialize) {
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED, app_bundle_->initialize());
  EXPECT_EQ(STATE_READY, GetBundleState());
}

TEST_F(AppBundleStateReadyUserTest, createApp) {
  App* app = NULL;
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED,
            app_bundle_->createApp(CComBSTR(kGuid1), &app));
  EXPECT_EQ(STATE_READY, GetBundleState());
}

TEST_F(AppBundleStateReadyUserTest, createInstalledApp) {
  CreateClientsKeyForApp1();

  App* app = NULL;
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED,
            app_bundle_->createInstalledApp(CComBSTR(kGuid1), &app));
  EXPECT_EQ(STATE_READY, GetBundleState());
}

TEST_F(AppBundleStateReadyUserTest, createAllInstalledApps) {
  CreateClientsKeyForApp1();

  App* app = NULL;
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED,
            app_bundle_->createAllInstalledApps());
  EXPECT_EQ(STATE_READY, GetBundleState());
}

TEST_F(AppBundleStateReadyUserTest, checkForUpdate) {
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED, app_bundle_->checkForUpdate());
  EXPECT_EQ(STATE_READY, GetBundleState());
}

TEST_F(AppBundleStateReadyUserTest, download) {
  TestUserWorkItem test_work_item;
  EXPECT_CALL(*worker_, DownloadAsync(_))
      .WillOnce(SetWorkItem(&test_work_item));

  EXPECT_SUCCEEDED(app_bundle_->download());
  EXPECT_EQ(STATE_BUSY, GetBundleState());

  EXPECT_FALSE(app_bundle_->is_auto_update());
}

TEST_F(AppBundleStateReadyUserTest, install) {
  TestUserWorkItem test_work_item;
  EXPECT_CALL(*worker_, DownloadAndInstallAsync(_))
      .WillOnce(SetWorkItem(&test_work_item));

  EXPECT_SUCCEEDED(app_bundle_->install());
  EXPECT_EQ(STATE_BUSY, GetBundleState());

  EXPECT_FALSE(app_bundle_->is_auto_update());
}

TEST_F(AppBundleStateReadyUserTest, updateAllApps) {
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED, app_bundle_->updateAllApps());
  EXPECT_EQ(STATE_READY, GetBundleState());
}

TEST_F(AppBundleStateReadyUserTest, stop) {
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED, app_bundle_->stop());
  EXPECT_EQ(STATE_READY, GetBundleState());
}

TEST_F(AppBundleStateReadyUserTest, pause) {
  EXPECT_SUCCEEDED(app_bundle_->pause());
  EXPECT_EQ(STATE_READY, GetBundleState());
}

TEST_F(AppBundleStateReadyUserTest, resume) {
  EXPECT_SUCCEEDED(app_bundle_->resume());
  EXPECT_EQ(STATE_READY, GetBundleState());
}

TEST_F(AppBundleStateReadyUserTest, isBusy) {
  VARIANT_BOOL is_busy = VARIANT_TRUE;
  EXPECT_SUCCEEDED(app_bundle_->isBusy(&is_busy));
  EXPECT_EQ(VARIANT_FALSE, is_busy);
  EXPECT_EQ(STATE_READY, GetBundleState());
}

// downloadPackage() returns E_INVALIDARG because the app has no packages.
// TODO(omaha): Add the package so downloadPackage can succeed.
TEST_F(AppBundleStateReadyUserTest, downloadPackage) {
  EXPECT_EQ(
      E_INVALIDARG,
      app_bundle_->downloadPackage(CComBSTR(kGuid1), CComBSTR(_T("package"))));
  EXPECT_EQ(STATE_READY, GetBundleState());
}

TEST_F(AppBundleStateReadyUserTest, get_currentState) {
  VARIANT current_state;
  ExpectAsserts expect_asserts;  // Not yet implemented.
  EXPECT_EQ(E_NOTIMPL, app_bundle_->get_currentState(&current_state));
  EXPECT_EQ(STATE_READY, GetBundleState());
}

TEST_F(AppBundleStateReadyUserTest, CompleteAsyncCall) {
  {
    ExpectAsserts expect_asserts;
    app_bundle_->CompleteAsyncCall();
  }

  EXPECT_EQ(STATE_READY, GetBundleState());
}

// Paused.

TEST_F(AppBundleStatePausedUserTest, Properties) {
  TestPropertyReflexiveness();
  EXPECT_EQ(STATE_PAUSED, GetBundleState());
}

TEST_F(AppBundleStatePausedUserTest, CountAndItem) {
  long num_apps = 0;  // NOLINT
  EXPECT_SUCCEEDED(app_bundle_->get_Count(&num_apps));
  EXPECT_EQ(0, num_apps);

  App* app0_obtained = NULL;
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_INVALID_INDEX),
            app_bundle_->get_Item(0, &app0_obtained));
  EXPECT_FALSE(app0_obtained);

  EXPECT_EQ(STATE_PAUSED, GetBundleState());
}

TEST_F(AppBundleStatePausedUserTest, put_altTokens) {
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED, app_bundle_->put_altTokens(1, 2, 3));
  EXPECT_EQ(STATE_PAUSED, GetBundleState());
}

TEST_F(AppBundleStatePausedUserTest, initialize) {
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED, app_bundle_->initialize());
  EXPECT_EQ(STATE_PAUSED, GetBundleState());
}

TEST_F(AppBundleStatePausedUserTest, createApp) {
  App* app = NULL;
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED,
            app_bundle_->createApp(CComBSTR(kGuid1), &app));
  EXPECT_EQ(STATE_PAUSED, GetBundleState());
}

TEST_F(AppBundleStatePausedUserTest, createInstalledApp) {
  CreateClientsKeyForApp1();

  App* app = NULL;
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED,
            app_bundle_->createInstalledApp(CComBSTR(kGuid1), &app));
  EXPECT_EQ(STATE_PAUSED, GetBundleState());
}

TEST_F(AppBundleStatePausedUserTest, createAllInstalledApps) {
  CreateClientsKeyForApp1();

  App* app = NULL;
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED,
            app_bundle_->createAllInstalledApps());
  EXPECT_EQ(STATE_PAUSED, GetBundleState());
}

TEST_F(AppBundleStatePausedUserTest, checkForUpdate) {
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED, app_bundle_->checkForUpdate());
  EXPECT_EQ(STATE_PAUSED, GetBundleState());
}

TEST_F(AppBundleStatePausedUserTest, download) {
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED, app_bundle_->download());
  EXPECT_EQ(STATE_PAUSED, GetBundleState());
}

TEST_F(AppBundleStatePausedUserTest, install) {
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED, app_bundle_->install());
  EXPECT_EQ(STATE_PAUSED, GetBundleState());
}

TEST_F(AppBundleStatePausedUserTest, updateAllApps) {
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED, app_bundle_->updateAllApps());
  EXPECT_EQ(STATE_PAUSED, GetBundleState());
}

// This may change when implemented.
TEST_F(AppBundleStatePausedUserTest, stop) {
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED, app_bundle_->stop());
  EXPECT_EQ(STATE_PAUSED, GetBundleState());
}

TEST_F(AppBundleStatePausedUserTest, pause) {
  EXPECT_SUCCEEDED(app_bundle_->pause());
  EXPECT_EQ(STATE_PAUSED, GetBundleState());
}

TEST_F(AppBundleStatePausedUserTest, resume_Succeeds_AsyncCallNotCompleted) {
  EXPECT_CALL(*worker_, Resume(app_bundle_.get()))
      .WillOnce(Return(S_OK));

  EXPECT_SUCCEEDED(app_bundle_->resume());
  EXPECT_EQ(STATE_BUSY, GetBundleState());
}

TEST_F(AppBundleStatePausedUserTest, resume_Succeeds_AsyncCallCompleted) {
  TestUserWorkItem test_work_item;
  app_bundle_->set_user_work_item(&test_work_item);
  app_bundle_->CompleteAsyncCall();

  EXPECT_CALL(*worker_, Resume(app_bundle_.get()))
      .WillOnce(Return(S_OK));

  EXPECT_SUCCEEDED(app_bundle_->resume());
  EXPECT_EQ(STATE_READY, GetBundleState());
}

TEST_F(AppBundleStatePausedUserTest, resume_Fails_AsyncCallNotCompleted) {
  EXPECT_CALL(*worker_, Resume(app_bundle_.get()))
      .WillOnce(Return(kKnownError));

  EXPECT_EQ(kKnownError, app_bundle_->resume());
  EXPECT_EQ(STATE_PAUSED, GetBundleState());
}

TEST_F(AppBundleStatePausedUserTest, resume_Fails_AsyncCallCompleted) {
  TestUserWorkItem test_work_item;
  app_bundle_->set_user_work_item(&test_work_item);
  app_bundle_->CompleteAsyncCall();

  EXPECT_CALL(*worker_, Resume(app_bundle_.get()))
      .WillOnce(Return(kKnownError));

  EXPECT_EQ(kKnownError, app_bundle_->resume());
  EXPECT_EQ(STATE_PAUSED, GetBundleState());
}

TEST_F(AppBundleStatePausedUserTest, isBusy) {
  VARIANT_BOOL is_busy = VARIANT_TRUE;
  EXPECT_SUCCEEDED(app_bundle_->isBusy(&is_busy));
  EXPECT_EQ(VARIANT_FALSE, is_busy);
  EXPECT_EQ(STATE_PAUSED, GetBundleState());
}

TEST_F(AppBundleStatePausedUserTest, downloadPackage) {
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED,
            app_bundle_->downloadPackage(CComBSTR(kGuid1),
                                         CComBSTR(_T("package"))));
  EXPECT_EQ(STATE_PAUSED, GetBundleState());
}

TEST_F(AppBundleStatePausedUserTest, get_currentState) {
  VARIANT current_state;
  ExpectAsserts expect_asserts;  // Not yet implemented.
  EXPECT_EQ(E_NOTIMPL, app_bundle_->get_currentState(&current_state));
  EXPECT_EQ(STATE_PAUSED, GetBundleState());
}

// Remains Paused until resumed.
TEST_F(AppBundleStatePausedUserTest, CompleteAsyncCall) {
  TestUserWorkItem test_work_item;
  app_bundle_->set_user_work_item(&test_work_item);
  SetAppBundleStateForUnitTest(app_bundle_.get(),
                               new fsm::AppBundleStatePaused);

  app_bundle_->CompleteAsyncCall();

  EXPECT_EQ(STATE_PAUSED, GetBundleState());
}

// Stopped.

TEST_F(AppBundleStateStoppedUserTest, Properties) {
  TestPropertyReflexiveness();
  EXPECT_EQ(STATE_STOPPED, GetBundleState());
}

TEST_F(AppBundleStateStoppedUserTest, CountAndItem) {
  long num_apps = 0;  // NOLINT
  EXPECT_SUCCEEDED(app_bundle_->get_Count(&num_apps));
  EXPECT_EQ(0, num_apps);

  App* app0_obtained = NULL;
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_INVALID_INDEX),
            app_bundle_->get_Item(0, &app0_obtained));
  EXPECT_FALSE(app0_obtained);

  EXPECT_EQ(STATE_STOPPED, GetBundleState());
}

TEST_F(AppBundleStateStoppedUserTest, put_altTokens) {
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED, app_bundle_->put_altTokens(1, 2, 3));
  EXPECT_EQ(STATE_STOPPED, GetBundleState());
}

TEST_F(AppBundleStateStoppedUserTest, initialize) {
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED, app_bundle_->initialize());
  EXPECT_EQ(STATE_STOPPED, GetBundleState());
}

TEST_F(AppBundleStateStoppedUserTest, createApp) {
  App* app = NULL;
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED,
            app_bundle_->createApp(CComBSTR(kGuid1), &app));

  EXPECT_EQ(S_OK, app_bundle_->stop());
  EXPECT_EQ(STATE_STOPPED, GetBundleState());
}

TEST_F(AppBundleStateStoppedUserTest, createInstalledApp) {
  CreateClientsKeyForApp1();

  App* app = NULL;
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED,
            app_bundle_->createInstalledApp(CComBSTR(kGuid1), &app));
  EXPECT_EQ(STATE_STOPPED, GetBundleState());
}

TEST_F(AppBundleStateStoppedUserTest, createAllInstalledApps) {
  CreateClientsKeyForApp1();

  App* app = NULL;
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED,
            app_bundle_->createAllInstalledApps());
  EXPECT_EQ(STATE_STOPPED, GetBundleState());
}

TEST_F(AppBundleStateStoppedUserTest, checkForUpdate) {
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED, app_bundle_->checkForUpdate());
  EXPECT_EQ(STATE_STOPPED, GetBundleState());
}

TEST_F(AppBundleStateStoppedUserTest, download) {
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED, app_bundle_->download());
  EXPECT_EQ(STATE_STOPPED, GetBundleState());
}

TEST_F(AppBundleStateStoppedUserTest, install) {
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED, app_bundle_->install());
  EXPECT_EQ(STATE_STOPPED, GetBundleState());
}

TEST_F(AppBundleStateStoppedUserTest, updateAllApps) {
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED, app_bundle_->updateAllApps());
  EXPECT_EQ(STATE_STOPPED, GetBundleState());
}

TEST_F(AppBundleStateStoppedUserTest, stop) {
  EXPECT_EQ(S_OK, app_bundle_->stop());
  EXPECT_EQ(STATE_STOPPED, GetBundleState());
}

TEST_F(AppBundleStateStoppedUserTest, pause) {
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED, app_bundle_->pause());
  EXPECT_EQ(STATE_STOPPED, GetBundleState());
}

TEST_F(AppBundleStateStoppedUserTest, resume) {
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED, app_bundle_->resume());
  EXPECT_EQ(STATE_STOPPED, GetBundleState());
}

TEST_F(AppBundleStateStoppedUserTest, isBusy) {
  VARIANT_BOOL is_busy = VARIANT_TRUE;
  EXPECT_SUCCEEDED(app_bundle_->isBusy(&is_busy));
  EXPECT_EQ(VARIANT_FALSE, is_busy);
  EXPECT_EQ(STATE_STOPPED, GetBundleState());
}

TEST_F(AppBundleStateStoppedUserTest, downloadPackage) {
  EXPECT_EQ(GOOPDATE_E_CALL_UNEXPECTED,
            app_bundle_->downloadPackage(CComBSTR(kGuid1),
                                         CComBSTR(_T("package"))));
  EXPECT_EQ(STATE_STOPPED, GetBundleState());
}

TEST_F(AppBundleStateStoppedUserTest, get_currentState) {
  VARIANT current_state;
  ExpectAsserts expect_asserts;  // Not yet implemented.
  EXPECT_EQ(E_NOTIMPL, app_bundle_->get_currentState(&current_state));
  EXPECT_EQ(STATE_STOPPED, GetBundleState());
}

TEST_F(AppBundleStateStoppedUserTest, CompleteAsyncCall) {
  TestUserWorkItem test_work_item;
  app_bundle_->set_user_work_item(&test_work_item);

  app_bundle_->CompleteAsyncCall();

  EXPECT_EQ(STATE_STOPPED, GetBundleState());
}

}  // namespace omaha
