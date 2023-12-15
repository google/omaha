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

#include "omaha/goopdate/worker.h"

#include <tuple>
#include <utility>

#include "omaha/base/app_util.h"
#include "omaha/base/const_addresses.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/update_response.h"
#include "omaha/common/web_services_client.h"
#include "omaha/goopdate/app_bundle_state_busy.h"
#include "omaha/goopdate/app_manager.h"
#include "omaha/goopdate/app_state_ready_to_install.h"
#include "omaha/goopdate/app_state_update_available.h"
#include "omaha/goopdate/download_manager.h"
#include "omaha/goopdate/goopdate.h"
#include "omaha/goopdate/install_manager.h"
#include "omaha/goopdate/installer_result_info.h"
#include "omaha/goopdate/model.h"
#include "omaha/goopdate/resource_manager.h"
#include "omaha/goopdate/worker_internal.h"
#include "omaha/goopdate/worker_metrics.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;

namespace {

// TODO(omaha): Change these to invalid IDs once the TODO above
// UpdateResponse::GetResult is fixed. Until then, invalid IDs cause asserts.
// Also, the large tests need valid files to download.
#if 0
const TCHAR* const kGuid1 = _T("{8A001254-1003-465e-A970-0748961C5293}");
const TCHAR* const kGuid2 = _T("{058ADDBE-BF10-4ba1-93C0-6F4A52C03C7E}");
#else
const TCHAR* const kGuid1 = _T("{65E60E95-0DE9-43FF-9F3F-4F7D2DFF04B5}");
const TCHAR* const kGuid2 = _T("{8A69D345-D564-463C-AFF1-A69D9E530F96}");
#endif

const uint64 kApp1GuidUpper = 0x0C480772AC73418f;
const uint64 kApp2GuidUpper = 0x89906BCD4D124c9b;

const TCHAR* const kApp1ClientsKeyPathUser =
    _T("HKCU\\Software\\") PATH_COMPANY_NAME _T("\\") PRODUCT_NAME
    _T("\\Clients\\{0C480772-AC73-418f-9603-66303DA4C7AA}");
const TCHAR* const kApp2ClientsKeyPathUser =
    _T("HKCU\\Software\\") PATH_COMPANY_NAME _T("\\") PRODUCT_NAME
    _T("\\Clients\\{89906BCD-4D12-4c9b-B5BA-8286051CB8D9}");
const TCHAR* const kApp3ClientsKeyPathUser =
    _T("HKCU\\Software\\") PATH_COMPANY_NAME _T("\\") PRODUCT_NAME
    _T("\\Clients\\{F5A1FE97-CF5A-47b8-8B28-2A72F9A57A45}");

const TCHAR* const kApp1ClientStateKeyPathUser =
    _T("HKCU\\Software\\") PATH_COMPANY_NAME _T("\\") PRODUCT_NAME
    _T("\\ClientState\\{0C480772-AC73-418f-9603-66303DA4C7AA}");
const TCHAR* const kApp2ClientStateKeyPathUser =
    _T("HKCU\\Software\\") PATH_COMPANY_NAME _T("\\") PRODUCT_NAME
    _T("\\ClientState\\{89906BCD-4D12-4c9b-B5BA-8286051CB8D9}");
const TCHAR* const kApp3ClientStateKeyPathUser =
    _T("HKCU\\Software\\") PATH_COMPANY_NAME _T("\\") PRODUCT_NAME
    _T("\\ClientState\\{F5A1FE97-CF5A-47b8-8B28-2A72F9A57A45}");

void SetAppStateUpdateAvailable(App* app) {
  SetAppStateForUnitTest(app, new fsm::AppStateUpdateAvailable);
}

void SetAppStateReadyToInstall(App* app) {
  SetAppStateForUnitTest(app, new fsm::AppStateReadyToInstall);
}

typedef HRESULT WebServiceClientSendMethod(
    bool is_foreground,
    const xml::UpdateRequest* update_request,
    xml::UpdateResponse* update_response);

// Create a customized action to modify the output parameters in the mock
// web service client.
class WebServiceClientSendAction
    : public ::testing::ActionInterface<WebServiceClientSendMethod> {
 public:
  virtual HRESULT Perform(const ArgumentTuple& args) {
    xml::UpdateResponse* response = std::get<2>(args);
    xml::response::Response r;

    // Omaha expects |elapsed_days| to be in range. So set it to a valid
    // number.
    r.day_start.elapsed_days = kMinDaysSinceDatum + 123;
    SetResponseForUnitTest(response, r);
    return S_OK;
  }
};

::testing::Action<WebServiceClientSendMethod> WebServiceClientSend() {
  return ::testing::MakeAction(new WebServiceClientSendAction);
}

class MockWebServicesClient : public WebServicesClientInterface {
 public:
  MOCK_METHOD3(Send,
      HRESULT(bool is_foreground,
              const xml::UpdateRequest* update_request,
              xml::UpdateResponse* update_response));
  MOCK_METHOD3(SendString,
      HRESULT(bool is_foreground,
              const CString* request_string,
              xml::UpdateResponse* update_response));
  MOCK_METHOD0(Cancel,
      void());
  MOCK_METHOD1(set_proxy_auth_config,
      void(const ProxyAuthConfig& config));
  MOCK_CONST_METHOD0(is_http_success,
      bool());
  MOCK_CONST_METHOD0(http_status_code,
      int());
  MOCK_CONST_METHOD0(http_trace,
      CString());
  MOCK_CONST_METHOD0(http_used_ssl,
      bool());
  MOCK_CONST_METHOD0(http_ssl_result,
      HRESULT());
  MOCK_CONST_METHOD0(http_xdaystart_header_value,
      int());
  MOCK_CONST_METHOD0(http_xdaynum_header_value,
      int());
  MOCK_CONST_METHOD0(retry_after_sec,
      int());
};

class MockDownloadManager : public DownloadManagerInterface {
 public:
  MOCK_METHOD0(Initialize,
      HRESULT());
  MOCK_METHOD2(PurgeAppLowerVersions,
      HRESULT(const CString&, const CString&));
  MOCK_METHOD3(CachePackage,
      HRESULT(const Package*, File*, const CString*));
  MOCK_METHOD1(DownloadApp,
      HRESULT(App* app));
  MOCK_METHOD1(DownloadPackage,
      HRESULT(Package* package));
  MOCK_CONST_METHOD2(GetPackage,
      HRESULT(const Package*, const CString&));
  MOCK_METHOD1(Cancel,
      void(App* app));
  MOCK_METHOD0(CancelAll,
      void());
  MOCK_CONST_METHOD0(IsBusy,
      bool());
  MOCK_CONST_METHOD1(IsPackageAvailable,
      bool(const Package* package));      // NOLINT
};

class MockInstallManager : public InstallManagerInterface {
 public:
  MOCK_METHOD0(Initialize,
      HRESULT());
  MOCK_CONST_METHOD0(install_working_dir,
      CString());
  MOCK_METHOD2(InstallApp,
      void(App* app, const CString& dir));
};

ACTION(SimulateDownloadAppStateTransition) {
  UNREFERENCED_ACTION_PARAMETERS;
  arg0->Downloading();
  arg0->DownloadComplete();
  // TODO(omaha3): Simulate extract and differential update once implemented.
  arg0->MarkReadyToInstall();
  return 0;
}

ACTION(SimulateInstallAppStateTransition) {
  UNREFERENCED_ACTION_PARAMETERS;
  arg0->Installing();

  AppManager& app_manager = *AppManager::Instance();
  __mutexScope(app_manager.GetRegistryStableStateLock());

  InstallerResultInfo result_info;
  result_info.type = INSTALLER_RESULT_SUCCESS;
  result_info.text = _T("success");
  arg0->ReportInstallerComplete(result_info);
}

void WaitForAppToEnterState(const App& app,
                            CurrentState expected_state,
                            int timeout_sec) {
  const int kPeriodMs = 50;
  const int max_tries = timeout_sec * 1000 / kPeriodMs;
  for (int tries = 0; tries < max_tries; ++tries) {
    if (expected_state == app.state()) {
        break;
    }
    ::Sleep(kPeriodMs);
  }
  EXPECT_EQ(expected_state, app.state());
}

// Assumes the caller has verified the bundle is busy. Otherwise, this could
// return before the bundle enters the busy state.
void WaitForBundleToBeReady(const AppBundle& app_bundle, int timeout_sec) {
  const int kPeriodMs = 50;
  const int max_tries = timeout_sec * 1000 / kPeriodMs;
  for (int tries = 0; tries < max_tries; ++tries) {
    if (!app_bundle.IsBusy()) {
      return;
    }
    ::Sleep(kPeriodMs);
  }
  ADD_FAILURE() << _T("Timed out waiting for AppBundle to be ready.");
}

}  // namespace

// All tests use a user instance of the Worker.

class WorkerTest : public testing::Test {
 protected:
  WorkerTest() : is_machine_(false), goopdate_(is_machine_), worker_(NULL) {}

  virtual void SetUp() {
    worker_ = &Worker::Instance();

    worker_->Initialize(is_machine_);

    EXPECT_SUCCEEDED(ResourceManager::Create(
      is_machine_, app_util::GetCurrentModuleDirectory(), _T("en")));

    EXPECT_SUCCEEDED(RegKey::CreateKey(
        ConfigManager::Instance()->registry_client_state(is_machine_)));
  }

  virtual void TearDown() {
    worker_ = NULL;
    Worker::DeleteInstance();
    ResourceManager::Delete();
  }

  // Overrides the update check client of the bundle.
  void SetUpdateCheckClient(AppBundle* app_bundle,
                            WebServicesClientInterface* web_services_client) {
    app_bundle->update_check_client_.reset(web_services_client);
  }

  void SetWorkerDownloadManager(DownloadManagerInterface* download_manager) {
    ASSERT_TRUE(download_manager);
    worker_->download_manager_.reset(download_manager);
  }

  void SetWorkerInstallManager(InstallManagerInterface* install_manager) {
    ASSERT_TRUE(install_manager);
    worker_->install_manager_.reset(install_manager);
  }

  const bool is_machine_;
  Goopdate goopdate_;
  Worker* worker_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WorkerTest);
};

class WorkerWithBundleTest : public WorkerTest {
 protected:
  WorkerWithBundleTest() : WorkerTest() {}

  virtual void SetUp() {
    WorkerTest::SetUp();

    app_bundle_ = worker_->model()->CreateAppBundle(is_machine_);
    ASSERT_TRUE(app_bundle_.get());

    EXPECT_SUCCEEDED(app_bundle_->put_displayName(CComBSTR(_T("My Bundle"))));
    EXPECT_SUCCEEDED(app_bundle_->put_displayLanguage(CComBSTR(_T("en"))));
    EXPECT_SUCCEEDED(app_bundle_->initialize());

    CString update_check_url;
    ConfigManager::Instance()->GetUpdateCheckUrl(&update_check_url);
    WebServicesClient* web_service_client = new WebServicesClient(is_machine_);
    EXPECT_HRESULT_SUCCEEDED(web_service_client->Initialize(update_check_url,
                                                            HeadersVector(),
                                                            true));
    SetUpdateCheckClient(app_bundle_.get(), web_service_client);
  }

  virtual void TearDown() {
    app_bundle_.reset();

    WorkerTest::TearDown();
  }

  std::shared_ptr<AppBundle> app_bundle_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WorkerWithBundleTest);
};

// The EULA is accepted for both apps.
class WorkerWithTwoAppsTest : public WorkerWithBundleTest {
 protected:
  WorkerWithTwoAppsTest() : WorkerWithBundleTest(), app1_(NULL), app2_(NULL) {}

  virtual void SetUp() {
    WorkerWithBundleTest::SetUp();

    EXPECT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kGuid1), &app1_));
    EXPECT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kGuid2), &app2_));

    EXPECT_SUCCEEDED(app1_->put_isEulaAccepted(VARIANT_TRUE));
    EXPECT_SUCCEEDED(app2_->put_isEulaAccepted(VARIANT_TRUE));
  }

  virtual void TearDown() {
    WorkerWithBundleTest::TearDown();
  }

  App* app1_;
  App* app2_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WorkerWithTwoAppsTest);
};

// Mocks the DownloadManager and InstallManager.
class WorkerMockedManagersTest : public WorkerWithTwoAppsTest {
 protected:
  WorkerMockedManagersTest()
      : WorkerWithTwoAppsTest(),
        mock_web_services_client_(),
        mock_download_manager_(NULL),
        mock_install_manager_(NULL) {
  }

  virtual void SetUp() {
    WorkerWithTwoAppsTest::SetUp();

    // By default, no methods should be called on web service client and install
    // manager, so make them StrictMock. Override this behavior for specific
    // methods in the individual test cases.
    mock_web_services_client_ = new testing::StrictMock<MockWebServicesClient>;
    mock_install_manager_ = new testing::StrictMock<MockInstallManager>;

    // Some functions will be called on the mock download manager, so make it
    // NiceMock.
    mock_download_manager_ = new testing::NiceMock<MockDownloadManager>;

    // The App Bundle takes ownership.
    SetUpdateCheckClient(app_bundle_.get(), mock_web_services_client_);

    // The Worker takes ownership of the Manager mocks.
    SetWorkerDownloadManager(mock_download_manager_);
    SetWorkerInstallManager(mock_install_manager_);
  }

  virtual void TearDown() {
    WorkerWithTwoAppsTest::TearDown();
  }

  // Pointers used by tests to set behavior. Not owned by this instance.
  MockWebServicesClient* mock_web_services_client_;
  MockDownloadManager* mock_download_manager_;
  MockInstallManager* mock_install_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WorkerMockedManagersTest);
};

TEST_F(WorkerMockedManagersTest, CheckForUpdateAsync) {
  EXPECT_CALL(*mock_web_services_client_, Send(_, _, _))
      .Times(1).WillOnce(WebServiceClientSend());
  ON_CALL(*mock_web_services_client_, http_trace())
      .WillByDefault(Return(_T("")));
  EXPECT_CALL(*mock_web_services_client_, http_trace())
      .Times(AnyNumber());
  EXPECT_CALL(*mock_web_services_client_, retry_after_sec())
      .Times(1);

  __mutexBlock(worker_->model()->lock()) {
    EXPECT_SUCCEEDED(worker_->CheckForUpdateAsync(app_bundle_.get()));

    EXPECT_EQ(STATE_WAITING_TO_CHECK_FOR_UPDATE, app1_->state());
    EXPECT_EQ(STATE_WAITING_TO_CHECK_FOR_UPDATE, app2_->state());

    SetAppBundleStateForUnitTest(app_bundle_.get(),
                                 new fsm::AppBundleStateBusy);
    EXPECT_TRUE(app_bundle_->IsBusy());
  }

  // TODO(omaha3): The update check fails because there is not a real response.
  // Fix this.
  WaitForBundleToBeReady(*app_bundle_, 5);
  EXPECT_FALSE(app_bundle_->IsBusy());
  EXPECT_EQ(STATE_ERROR, app1_->state());
  EXPECT_EQ(STATE_ERROR, app2_->state());
}

TEST_F(WorkerMockedManagersTest, DownloadAsync) {
  SetAppStateUpdateAvailable(app1_);
  SetAppStateUpdateAvailable(app2_);

  {
    ::testing::InSequence order_is_guaranteed;
    EXPECT_CALL(*mock_download_manager_, DownloadApp(app1_))
        .WillOnce(SimulateDownloadAppStateTransition());
    EXPECT_CALL(*mock_download_manager_, DownloadApp(app2_))
        .WillOnce(SimulateDownloadAppStateTransition());
  }

  // Holding the lock prevents the state from changing in the other thread,
  // ensuring consistent results.
  __mutexBlock(worker_->model()->lock()) {
    EXPECT_SUCCEEDED(worker_->DownloadAsync(app_bundle_.get()));

    EXPECT_EQ(STATE_WAITING_TO_DOWNLOAD, app1_->state());
    EXPECT_EQ(STATE_WAITING_TO_DOWNLOAD, app2_->state());

    SetAppBundleStateForUnitTest(app_bundle_.get(),
                                 new fsm::AppBundleStateBusy);
    EXPECT_TRUE(app_bundle_->IsBusy());
  }

  WaitForBundleToBeReady(*app_bundle_, 5);
  EXPECT_EQ(STATE_READY_TO_INSTALL, app1_->state());
  EXPECT_EQ(STATE_READY_TO_INSTALL, app2_->state());
}

TEST_F(WorkerMockedManagersTest, DownloadAndInstallAsync_AlreadyDownloaded) {
  SetAppStateReadyToInstall(app1_);
  SetAppStateReadyToInstall(app2_);

  EXPECT_CALL(*mock_install_manager_, install_working_dir())
      .WillRepeatedly(Return(app_util::GetTempDir()));

  {
    ::testing::InSequence order_is_guaranteed;
    EXPECT_CALL(*mock_install_manager_, InstallApp(app1_, _))
        .WillOnce(SimulateInstallAppStateTransition());
    EXPECT_CALL(*mock_install_manager_, InstallApp(app2_, _))
        .WillOnce(SimulateInstallAppStateTransition());
  }

  __mutexBlock(worker_->model()->lock()) {
    EXPECT_SUCCEEDED(worker_->DownloadAndInstallAsync(app_bundle_.get()));

    EXPECT_EQ(STATE_WAITING_TO_INSTALL, app1_->state());
    EXPECT_EQ(STATE_WAITING_TO_INSTALL, app2_->state());

    SetAppBundleStateForUnitTest(app_bundle_.get(),
                                 new fsm::AppBundleStateBusy);
    EXPECT_TRUE(app_bundle_->IsBusy());
  }

  WaitForBundleToBeReady(*app_bundle_, 5);
  EXPECT_EQ(STATE_INSTALL_COMPLETE, app1_->state());
  EXPECT_EQ(STATE_INSTALL_COMPLETE, app2_->state());
}

TEST_F(WorkerMockedManagersTest, DownloadAndInstallAsync_NotAlreadyDownloaded) {
  SetAppStateUpdateAvailable(app1_);
  SetAppStateUpdateAvailable(app2_);

  EXPECT_CALL(*mock_install_manager_, install_working_dir())
      .WillRepeatedly(Return(app_util::GetTempDir()));

  {
    ::testing::InSequence order_is_guaranteed;
    EXPECT_CALL(*mock_download_manager_, DownloadApp(app1_))
        .WillOnce(SimulateDownloadAppStateTransition());
    EXPECT_CALL(*mock_install_manager_, InstallApp(app1_, _))
        .WillOnce(SimulateInstallAppStateTransition());
    EXPECT_CALL(*mock_download_manager_, DownloadApp(app2_))
        .WillOnce(SimulateDownloadAppStateTransition());
    EXPECT_CALL(*mock_install_manager_, InstallApp(app2_, _))
        .WillOnce(SimulateInstallAppStateTransition());
  }

  __mutexBlock(worker_->model()->lock()) {
    EXPECT_SUCCEEDED(worker_->DownloadAndInstallAsync(app_bundle_.get()));

    EXPECT_EQ(STATE_WAITING_TO_DOWNLOAD, app1_->state());
    EXPECT_EQ(STATE_WAITING_TO_DOWNLOAD, app2_->state());

    SetAppBundleStateForUnitTest(app_bundle_.get(),
                                 new fsm::AppBundleStateBusy);
    EXPECT_TRUE(app_bundle_->IsBusy());
  }

  WaitForBundleToBeReady(*app_bundle_, 5);
  EXPECT_EQ(STATE_INSTALL_COMPLETE, app1_->state());
  EXPECT_EQ(STATE_INSTALL_COMPLETE, app2_->state());
}

TEST_F(WorkerMockedManagersTest, DownloadAsync_Then_DownloadAndInstallAsync) {
  SetAppStateUpdateAvailable(app1_);
  SetAppStateUpdateAvailable(app2_);

  {
    ::testing::InSequence order_is_guaranteed;
    EXPECT_CALL(*mock_download_manager_, DownloadApp(app1_))
        .WillOnce(SimulateDownloadAppStateTransition());
    EXPECT_CALL(*mock_download_manager_, DownloadApp(app2_))
        .WillOnce(SimulateDownloadAppStateTransition());
  }

  __mutexBlock(worker_->model()->lock()) {
    EXPECT_SUCCEEDED(worker_->DownloadAsync(app_bundle_.get()));

    EXPECT_EQ(STATE_WAITING_TO_DOWNLOAD, app1_->state());
    EXPECT_EQ(STATE_WAITING_TO_DOWNLOAD, app2_->state());

    SetAppBundleStateForUnitTest(app_bundle_.get(),
                                 new fsm::AppBundleStateBusy);
    EXPECT_TRUE(app_bundle_->IsBusy());
  }

  WaitForBundleToBeReady(*app_bundle_, 5);
  EXPECT_EQ(STATE_READY_TO_INSTALL, app1_->state());
  EXPECT_EQ(STATE_READY_TO_INSTALL, app2_->state());

  EXPECT_CALL(*mock_install_manager_, install_working_dir())
      .WillRepeatedly(Return(app_util::GetTempDir()));

  {
    ::testing::InSequence order_is_guaranteed;
    EXPECT_CALL(*mock_install_manager_, InstallApp(app1_, _))
        .WillOnce(SimulateInstallAppStateTransition());
    EXPECT_CALL(*mock_install_manager_, InstallApp(app2_, _))
        .WillOnce(SimulateInstallAppStateTransition());
  }
  __mutexBlock(worker_->model()->lock()) {
    EXPECT_SUCCEEDED(worker_->DownloadAndInstallAsync(app_bundle_.get()));

    EXPECT_EQ(STATE_WAITING_TO_INSTALL, app1_->state());
    EXPECT_EQ(STATE_WAITING_TO_INSTALL, app2_->state());

    SetAppBundleStateForUnitTest(app_bundle_.get(),
                                 new fsm::AppBundleStateBusy);
    EXPECT_TRUE(app_bundle_->IsBusy());
  }

  WaitForBundleToBeReady(*app_bundle_, 5);
  EXPECT_EQ(STATE_INSTALL_COMPLETE, app1_->state());
  EXPECT_EQ(STATE_INSTALL_COMPLETE, app2_->state());
}

TEST_F(WorkerMockedManagersTest, UpdateAllAppsAsync) {
  EXPECT_CALL(*mock_web_services_client_, Send(_, _, _))
      .Times(1).WillOnce(WebServiceClientSend());
  ON_CALL(*mock_web_services_client_, http_trace())
      .WillByDefault(Return(_T("")));
  EXPECT_CALL(*mock_web_services_client_, http_trace())
      .Times(AnyNumber());
  EXPECT_CALL(*mock_web_services_client_, retry_after_sec())
      .Times(1);

  __mutexBlock(worker_->model()->lock()) {
    EXPECT_SUCCEEDED(worker_->UpdateAllAppsAsync(app_bundle_.get()));

    EXPECT_EQ(STATE_WAITING_TO_CHECK_FOR_UPDATE, app1_->state());
    EXPECT_EQ(STATE_WAITING_TO_CHECK_FOR_UPDATE, app2_->state());

    SetAppBundleStateForUnitTest(app_bundle_.get(),
                                 new fsm::AppBundleStateBusy);
    EXPECT_TRUE(app_bundle_->IsBusy());
  }

  // TODO(omaha3): The update check fails because there is not a real response.
  // Fix this and continue the test, ensuring it gets to install complete.
  WaitForBundleToBeReady(*app_bundle_, 5);
  EXPECT_FALSE(app_bundle_->IsBusy());
  EXPECT_EQ(STATE_ERROR, app1_->state());
  EXPECT_EQ(STATE_ERROR, app2_->state());
}

// TODO(omaha): Add tests for app already in error state, app failing download
// or install, all apps failed or failing, etc.

//
// Large Tests
// These are large tests because they use threads and access the network.
// TODO(omaha): Move these to a separate test executable when using Hammer to
// run tests.
//

TEST_F(WorkerWithTwoAppsTest, CheckForUpdateAsync_Large) {
  __mutexBlock(worker_->model()->lock()) {
    EXPECT_SUCCEEDED(worker_->CheckForUpdateAsync(app_bundle_.get()));

    EXPECT_EQ(STATE_WAITING_TO_CHECK_FOR_UPDATE, app1_->state());
    EXPECT_EQ(STATE_WAITING_TO_CHECK_FOR_UPDATE, app2_->state());

    SetAppBundleStateForUnitTest(app_bundle_.get(),
                                 new fsm::AppBundleStateBusy);
    EXPECT_TRUE(app_bundle_->IsBusy());
  }

  WaitForBundleToBeReady(*app_bundle_, 100);
  EXPECT_EQ(STATE_UPDATE_AVAILABLE, app1_->state());
  EXPECT_EQ(STATE_UPDATE_AVAILABLE, app2_->state());
}

TEST_F(WorkerWithTwoAppsTest,
       DownloadAsyncThenDownloadAndInstallAsync_Large) {
  // Update Check: Request then wait for it to complete in the thread pool.

  __mutexBlock(worker_->model()->lock()) {
    EXPECT_SUCCEEDED(worker_->CheckForUpdateAsync(app_bundle_.get()));

    EXPECT_EQ(STATE_WAITING_TO_CHECK_FOR_UPDATE, app1_->state());
    EXPECT_EQ(STATE_WAITING_TO_CHECK_FOR_UPDATE, app2_->state());

    SetAppBundleStateForUnitTest(app_bundle_.get(),
                                 new fsm::AppBundleStateBusy);
    EXPECT_TRUE(app_bundle_->IsBusy());
  }

  WaitForBundleToBeReady(*app_bundle_, 10);
  EXPECT_EQ(STATE_UPDATE_AVAILABLE, app1_->state());
  EXPECT_EQ(STATE_UPDATE_AVAILABLE, app2_->state());

  // Download: Request then wait for it to complete in the thread pool.

  __mutexBlock(worker_->model()->lock()) {
    EXPECT_SUCCEEDED(worker_->DownloadAsync(app_bundle_.get()));

    EXPECT_EQ(STATE_WAITING_TO_DOWNLOAD, app1_->state());
    EXPECT_EQ(STATE_WAITING_TO_DOWNLOAD, app2_->state());

    SetAppBundleStateForUnitTest(app_bundle_.get(),
                                 new fsm::AppBundleStateBusy);
    EXPECT_TRUE(app_bundle_->IsBusy());
  }

  WaitForBundleToBeReady(*app_bundle_, 25);
  EXPECT_EQ(STATE_READY_TO_INSTALL, app1_->state());
  EXPECT_EQ(STATE_READY_TO_INSTALL, app2_->state());

  // Install: Request then wait for it to complete in the thread pool.

  // TODO(omaha): Make User Foo installer available from production, change
  // GUID(s), and enable the code below. Be sure to uninstall the app when done.
#if 0

  __mutexBlock(worker_->model()->lock()) {
    EXPECT_SUCCEEDED(worker_->DownloadAndInstallAsync(app_bundle_.get()));

    EXPECT_EQ(STATE_WAITING_TO_INSTALL, app1_->state());
    EXPECT_EQ(STATE_WAITING_TO_INSTALL, app2_->state());

    SetAppBundleStateForUnitTest(app_bundle_.get(),
                                 new fsm::AppBundleStateBusy);
    EXPECT_TRUE(app_bundle_->IsBusy());
  }

  WaitForBundleToBeReady(*app_bundle_, 10);
  EXPECT_EQ(STATE_INSTALL_COMPLETE, app1_->state());
  EXPECT_EQ(STATE_INSTALL_COMPLETE, app2_->state());
#endif
}

// TODO(omaha): This test is disabled because when the network is slow on Zerg,
// the state remains at STATE_CHECKING_FOR_UPDATE instead of progressing to
// STATE_UPDATE_AVAILABLE.
TEST_F(WorkerWithTwoAppsTest,
       DISABLED_DownloadAndInstallAsyncWithoutDownload_Large) {
  // Update Check: Request then wait for it to complete in the thread pool.

  __mutexBlock(worker_->model()->lock()) {
    EXPECT_SUCCEEDED(worker_->CheckForUpdateAsync(app_bundle_.get()));

    EXPECT_EQ(STATE_WAITING_TO_CHECK_FOR_UPDATE, app1_->state());
    EXPECT_EQ(STATE_WAITING_TO_CHECK_FOR_UPDATE, app2_->state());

    SetAppBundleStateForUnitTest(app_bundle_.get(),
                                 new fsm::AppBundleStateBusy);
    EXPECT_TRUE(app_bundle_->IsBusy());
  }

  WaitForBundleToBeReady(*app_bundle_, 10);
  EXPECT_EQ(STATE_UPDATE_AVAILABLE, app1_->state());
  EXPECT_EQ(STATE_UPDATE_AVAILABLE, app2_->state());

  // Download and install: Request then wait for it to complete in thread pool.

  // TODO(omaha): Make User Foo installer available from production, change
  // GUID(s), and enable the code below. Be sure to uninstall the app when done.
#if 0
  __mutexBlock(worker_->model()->lock()) {
    EXPECT_SUCCEEDED(worker_->DownloadAndInstallAsync(app_bundle_.get()));

    EXPECT_EQ(STATE_WAITING_TO_DOWNLOAD, app1_->state());
    EXPECT_EQ(STATE_WAITING_TO_DOWNLOAD, app2_->state());

    SetAppBundleStateForUnitTest(app_bundle_.get(),
                                 new fsm::AppBundleStateBusy);
    EXPECT_TRUE(app_bundle_->IsBusy());
  }

  WaitForBundleToBeReady(*app_bundle_, 10);
  EXPECT_EQ(STATE_INSTALL_COMPLETE, app1_->state());
  EXPECT_EQ(STATE_INSTALL_COMPLETE, app2_->state());
#endif
}

// Also tests cancellation of a bundle during the update check phase.
TEST_F(WorkerWithTwoAppsTest, UpdateAllAppsAsync_Large) {
  __mutexBlock(worker_->model()->lock()) {
    EXPECT_SUCCEEDED(worker_->UpdateAllAppsAsync(app_bundle_.get()));

    EXPECT_EQ(STATE_WAITING_TO_CHECK_FOR_UPDATE, app1_->state());
    EXPECT_EQ(STATE_WAITING_TO_CHECK_FOR_UPDATE, app2_->state());

    SetAppBundleStateForUnitTest(app_bundle_.get(),
                                 new fsm::AppBundleStateBusy);
    EXPECT_TRUE(app_bundle_->IsBusy());

    // Stop the bundle to prevent it from actually trying to update app.
    EXPECT_SUCCEEDED(app_bundle_->stop());
  }

  // Warning: This unit test is pretty fragile.  Stopping a bundle returns
  // immediately, and the stop is handled async; so, despite calling stop(),
  // the UpdateAllApps DPC will still be in flight.
  //
  // If we try to tear down the worker before the DPC starts running, the
  // AppBundle will still have a reference on it (due to the DPC still being
  // in flight), so it will stay alive when Model::CleanupExpiredAppBundles()
  // is called. That, in turn, keeps the ATL module lock on the Worker alive,
  // which results in an assert at Worker destruction time.
  //
  // TODO(omaha3): We really need to rethink how we unit-test the Worker.  At
  // the very least, we could introduce a counter of "DPCs in flight" and wait
  // until that hits zero to do our teardown.  Alternately, find a way to block
  // until someone calls AppBundleState::CompleteAsyncCall().
  ::Sleep(1000);

  WaitForBundleToBeReady(*app_bundle_, 10);
  EXPECT_FALSE(app_bundle_->IsBusy());
  EXPECT_EQ(STATE_ERROR, app1_->state());
  EXPECT_EQ(STATE_ERROR, app2_->state());
}

class RecordUpdateAvailableUsageStatsTest : public testing::Test {
 protected:
  RecordUpdateAvailableUsageStatsTest() : is_machine_(false) {}

  static void SetUpTestCase() {
    stats_report::g_global_metrics.Initialize();
  }

  static void TearDownTestCase() {
    // The global metrics collection must be uninitialized before the metrics
    // destructors are called.
    stats_report::g_global_metrics.Uninitialize();
  }

  virtual void SetUp() {
    RegKey::DeleteKey(kRegistryHiveOverrideRoot);
    OverrideRegistryHives(kRegistryHiveOverrideRoot);

    EXPECT_SUCCEEDED(AppManager::CreateInstance(is_machine_));

    metric_worker_self_update_responses.Set(0);
    metric_worker_self_update_response_time_since_first_ms.Set(0);
    metric_worker_app_max_update_responses_app_high.Set(0);
    metric_worker_app_max_update_responses.Set(0);
    metric_worker_app_max_update_responses_ms_since_first.Set(0);

    ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENTS_GOOPDATE,
                                      kRegValueProductVersion,
                                      _T("0.1.0.0")));
    ASSERT_SUCCEEDED(RegKey::SetValue(kApp1ClientsKeyPathUser,
                                      kRegValueProductVersion,
                                      _T("0.1")));

    ASSERT_SUCCEEDED(RegKey::CreateKey(USER_REG_CLIENT_STATE_GOOPDATE));
    ASSERT_SUCCEEDED(RegKey::CreateKey(kApp1ClientStateKeyPathUser));
  }

  int GetNumProducts() {
    AppManager& app_manager = *AppManager::Instance();
    AppIdVector registered_app_ids;
    VERIFY_SUCCEEDED(app_manager.GetRegisteredApps(&registered_app_ids));
    return static_cast<int>(registered_app_ids.size());
  }

  virtual void TearDown() {
    AppManager::DeleteInstance();

    RestoreRegistryHives();
    RegKey::DeleteKey(kRegistryHiveOverrideRoot);
  }

  bool is_machine_;
};

TEST_F(RecordUpdateAvailableUsageStatsTest, NoData) {
  ASSERT_EQ(2, GetNumProducts());

  internal::RecordUpdateAvailableUsageStats();

  EXPECT_EQ(0, metric_worker_self_update_responses.value());
  EXPECT_EQ(0, metric_worker_self_update_response_time_since_first_ms.value());
  EXPECT_EQ(0, metric_worker_app_max_update_responses_app_high.value());
  EXPECT_EQ(0, metric_worker_app_max_update_responses.value());
  EXPECT_EQ(0, metric_worker_app_max_update_responses_ms_since_first.value());
}

TEST_F(RecordUpdateAvailableUsageStatsTest, OmahaDataOnly) {
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                    _T("UpdateAvailableCount"),
                                    static_cast<DWORD>(123456)));
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                    _T("UpdateAvailableSince"),
                                    static_cast<DWORD64>(10)));

  ASSERT_EQ(2, GetNumProducts());

  const time64 current_time_100ns(GetCurrent100NSTime());
  const int64 expected_ms_since_first_update =
      (current_time_100ns - 10) / kMillisecsTo100ns;

  internal::RecordUpdateAvailableUsageStats();

  EXPECT_EQ(123456, metric_worker_self_update_responses.value());

  EXPECT_LE(expected_ms_since_first_update,
            metric_worker_self_update_response_time_since_first_ms.value());
  EXPECT_GT(expected_ms_since_first_update + 10 * kMsPerSec,
            metric_worker_self_update_response_time_since_first_ms.value());

  EXPECT_EQ(0, metric_worker_app_max_update_responses_app_high.value());
  EXPECT_EQ(0, metric_worker_app_max_update_responses.value());
  EXPECT_EQ(0, metric_worker_app_max_update_responses_ms_since_first.value());
}

TEST_F(RecordUpdateAvailableUsageStatsTest, OneAppOnly) {
  ASSERT_SUCCEEDED(RegKey::SetValue(kApp1ClientStateKeyPathUser,
                                    _T("UpdateAvailableCount"),
                                    static_cast<DWORD>(123456)));
  ASSERT_SUCCEEDED(RegKey::SetValue(kApp1ClientStateKeyPathUser,
                                    _T("UpdateAvailableSince"),
                                    static_cast<DWORD64>(10)));

  ASSERT_EQ(2, GetNumProducts());

  const time64 current_time_100ns(GetCurrent100NSTime());
  const int64 expected_ms_since_first_update =
      (current_time_100ns - 10) / kMillisecsTo100ns;

  internal::RecordUpdateAvailableUsageStats();

  EXPECT_EQ(0, metric_worker_self_update_responses.value());
  EXPECT_EQ(0, metric_worker_self_update_response_time_since_first_ms.value());

  EXPECT_EQ(kApp1GuidUpper,
            metric_worker_app_max_update_responses_app_high.value());
  EXPECT_EQ(123456, metric_worker_app_max_update_responses.value());
  EXPECT_LE(expected_ms_since_first_update,
            metric_worker_app_max_update_responses_ms_since_first.value());
  EXPECT_GT(expected_ms_since_first_update + 10 * kMsPerSec,
            metric_worker_app_max_update_responses_ms_since_first.value());
}

// It is important that Omaha's count is the largest.
// All app data should be from app 2, which has the greatest count, a middle
// time, and an alphabetically middle GUID
TEST_F(RecordUpdateAvailableUsageStatsTest, OmahaAndSeveralApps) {
  const DWORD64 kApp2SinceTime = 1000 * kSecsTo100ns;

  ASSERT_SUCCEEDED(RegKey::SetValue(kApp2ClientsKeyPathUser,
                                    kRegValueProductVersion,
                                    _T("1.2")));
  ASSERT_SUCCEEDED(RegKey::SetValue(kApp3ClientsKeyPathUser,
                                    kRegValueProductVersion,
                                    _T("2.3")));

  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                    _T("UpdateAvailableCount"),
                                    static_cast<DWORD>(0x99887766)));
  ASSERT_SUCCEEDED(RegKey::SetValue(USER_REG_CLIENT_STATE_GOOPDATE,
                                    _T("UpdateAvailableSince"),
                                    static_cast<DWORD64>(1)));

  ASSERT_SUCCEEDED(RegKey::SetValue(kApp1ClientStateKeyPathUser,
                                    _T("UpdateAvailableCount"),
                                    static_cast<DWORD>(1)));
  ASSERT_SUCCEEDED(RegKey::SetValue(kApp1ClientStateKeyPathUser,
                                    _T("UpdateAvailableSince"),
                                    static_cast<DWORD64>(1)));

  ASSERT_SUCCEEDED(RegKey::SetValue(kApp2ClientStateKeyPathUser,
                                    _T("UpdateAvailableCount"),
                                    static_cast<DWORD>(9876543)));
  ASSERT_SUCCEEDED(RegKey::SetValue(kApp2ClientStateKeyPathUser,
                                    _T("UpdateAvailableSince"),
                                    kApp2SinceTime));

  ASSERT_SUCCEEDED(RegKey::SetValue(kApp3ClientStateKeyPathUser,
                                    _T("UpdateAvailableCount"),
                                    static_cast<DWORD>(234)));
  ASSERT_SUCCEEDED(RegKey::SetValue(kApp3ClientStateKeyPathUser,
                                    _T("UpdateAvailableSince"),
                                    static_cast<DWORD64>(128580000000000000)));

  ASSERT_EQ(4, GetNumProducts());

  const time64 current_time_100ns(GetCurrent100NSTime());
  const int64 goopdate_expected_ms_since_first_update =
      (current_time_100ns - 1) / kMillisecsTo100ns;

  const int64 app_expected_ms_since_first_update =
      (current_time_100ns - kApp2SinceTime) / kMillisecsTo100ns;

  internal::RecordUpdateAvailableUsageStats();

  EXPECT_EQ(0x99887766, metric_worker_self_update_responses.value());
  EXPECT_LE(goopdate_expected_ms_since_first_update,
            metric_worker_self_update_response_time_since_first_ms.value());
  EXPECT_GT(goopdate_expected_ms_since_first_update + 10 * kMsPerSec,
            metric_worker_self_update_response_time_since_first_ms.value());

  EXPECT_EQ(kApp2GuidUpper,
            metric_worker_app_max_update_responses_app_high.value());
  EXPECT_EQ(9876543, metric_worker_app_max_update_responses.value());
  EXPECT_LE(app_expected_ms_since_first_update,
            metric_worker_app_max_update_responses_ms_since_first.value());
  EXPECT_GT(app_expected_ms_since_first_update + 10 * kMsPerSec,
            metric_worker_app_max_update_responses_ms_since_first.value());
}

}  // namespace omaha
