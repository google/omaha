// Copyright 2010 Google Inc.
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

#include <atlbase.h>
#include <atlcom.h>
#include "omaha/base/error.h"
#include "omaha/base/time.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/common/const_group_policy.h"
#include "omaha/common/experiment_labels.h"
#include "omaha/common/update_request.h"
#include "omaha/common/update_response.h"
#include "omaha/goopdate/app_state_checking_for_update.h"
#include "omaha/goopdate/app_state_installing.h"
#include "omaha/goopdate/app_state_update_available.h"
#include "omaha/goopdate/app_state_waiting_to_check_for_update.h"
#include "omaha/goopdate/app_unittest_base.h"
#include "omaha/testing/unit_test.h"

using ::testing::_;

namespace omaha {

namespace {

#define APP_ID1 _T("{D9F05AEA-BEDA-4f91-B216-BE45DAE330CB}");
const TCHAR* const kAppId1 = APP_ID1
const TCHAR* const kInstallPolicyApp1 = _T("Install") APP_ID1;
const TCHAR* const kUpdatePolicyApp1 = _T("Update") APP_ID1;
const TCHAR* const kAppId1ClientsKeyPathUser =
    _T("HKCU\\Software\\") PATH_COMPANY_NAME _T("\\")
                           PRODUCT_NAME _T("\\Clients\\") APP_ID1;
const TCHAR* const kGuid1ClientStateKeyPathUser =
    _T("HKCU\\Software\\") PATH_COMPANY_NAME _T("\\")
                           PRODUCT_NAME _T("\\ClientState\\") APP_ID1;

#define APP_ID2 _T("{EF3CACD4-89EB-46b7-B9BF-B16B15F08584}");
const TCHAR* const kInstallPolicyApp2 = _T("Install") APP_ID2;
const TCHAR* const kUpdatePolicyApp2 = _T("Update") APP_ID2;

}  // namespace

class AppTest : public AppTestBaseWithRegistryOverride {
 protected:
  explicit AppTest(bool use_strict_mock)
      : AppTestBaseWithRegistryOverride(false,  // Always as user for now.
                                        use_strict_mock),
        app_(NULL) {}

  virtual void SetUp() {
    AppTestBaseWithRegistryOverride::SetUp();

    update_response_.reset(xml::UpdateResponse::Create());
  }

  void AddAppResponse(const CString& status,
                      const std::vector<xml::response::Data>& data) {
    xml::response::App app;
    app.status = xml::response::kStatusOkValue;
    app.appid = kAppId1;
    app.update_check.status = status;
    app.data = data;

    xml::response::Response response;

    // Client expects elapsed_days in response falls into range
    // [kMinDaysSinceDatum, kMaxDaysSinceDatum].
    response.day_start.elapsed_days = kMinDaysSinceDatum + 111;
    response.apps.push_back(app);

    SetResponseForUnitTest(update_response_.get(), response);
  }

  App* app_;
  std::unique_ptr<xml::UpdateResponse> update_response_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppTest);
};

class AppInstallTest : public AppTest {
 protected:
  AppInstallTest() : AppTest(true) {}

  virtual void SetUp() {
    AppTest::SetUp();
    EXPECT_SUCCEEDED(
        app_bundle_->createApp(CComBSTR(kAppId1), &app_));
    ASSERT_TRUE(app_);
  }
};

class AppManualUpdateTest : public AppTest {
 protected:
  AppManualUpdateTest() : AppTest(true) {}
  explicit AppManualUpdateTest(bool use_strict_mock)
      : AppTest(use_strict_mock) {}

  // Calling checkForUpdate() should leave AppBundle::is_auto_update as false.
  virtual void SetUp() {
    AppTest::SetUp();
    EXPECT_SUCCEEDED(RegKey::SetValue(kAppId1ClientsKeyPathUser,
                                      kRegValueProductVersion,
                                      _T("1.2.3.4")));
    EXPECT_SUCCEEDED(RegKey::SetValue(kAppId1ClientsKeyPathUser,
                                      kRegValueAppName,
                                      _T("Unit Test App")));
    EXPECT_SUCCEEDED(app_bundle_->createInstalledApp(
                         CComBSTR(kAppId1), &app_));
    ASSERT_TRUE(app_);

    EXPECT_CALL(*mock_worker_, CheckForUpdateAsync(_)).Times(1);
    EXPECT_SUCCEEDED(app_bundle_->checkForUpdate());
    EXPECT_FALSE(app_bundle_->is_auto_update());
  }
};

class AppAutoUpdateTest : public AppManualUpdateTest  {
 protected:
  AppAutoUpdateTest() : AppManualUpdateTest(false) {}

  // Calling UpdateAllAppsAsync() sets AppBundle::is_auto_update.
  virtual void SetUp() {
    AppTest::SetUp();
    EXPECT_SUCCEEDED(RegKey::SetValue(kAppId1ClientsKeyPathUser,
                                      kRegValueProductVersion,
                                      _T("1.2.3.4")));
    EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientStateKeyPathUser,
                                      kRegValueProductVersion,
                                      _T("1.2.3.4")));
    EXPECT_SUCCEEDED(RegKey::SetValue(kAppId1ClientsKeyPathUser,
                                      kRegValueAppName,
                                      _T("Unit Test App")));

    EXPECT_CALL(*mock_worker_, UpdateAllAppsAsync(_)).Times(1);
    EXPECT_SUCCEEDED(app_bundle_->updateAllApps());
    EXPECT_TRUE(app_bundle_->is_auto_update());

    app_ = app_bundle_->GetApp(0);
    ASSERT_TRUE(app_);
  }
};

INSTANTIATE_TEST_CASE_P(IsDomain, AppInstallTest, ::testing::Bool());
INSTANTIATE_TEST_CASE_P(IsDomain, AppManualUpdateTest, ::testing::Bool());
INSTANTIATE_TEST_CASE_P(IsDomain, AppAutoUpdateTest, ::testing::Bool());

//
// CheckGroupPolicy Tests.
//

TEST_F(AppInstallTest, CheckGroupPolicy_NoPolicy) {
  EXPECT_SUCCEEDED(app_->CheckGroupPolicy());
}

TEST_F(AppManualUpdateTest, CheckGroupPolicy_NoPolicy) {
  EXPECT_SUCCEEDED(app_->CheckGroupPolicy());
}

TEST_F(AppAutoUpdateTest, CheckGroupPolicy_NoPolicy) {
  EXPECT_SUCCEEDED(app_->CheckGroupPolicy());
}

TEST_P(AppInstallTest, CheckGroupPolicy_InstallDisabled) {
  SetEnrolledPolicy(kInstallPolicyApp1, kPolicyDisabled);
  EXPECT_EQ(IsDomain() ? GOOPDATE_E_APP_INSTALL_DISABLED_BY_POLICY : S_OK,
            app_->CheckGroupPolicy());
}

TEST_P(AppManualUpdateTest, CheckGroupPolicy_InstallDisabled) {
  SetEnrolledPolicy(kInstallPolicyApp1, kPolicyDisabled);
  EXPECT_SUCCEEDED(app_->CheckGroupPolicy());
}

TEST_P(AppAutoUpdateTest, CheckGroupPolicy_InstallDisabled) {
  SetEnrolledPolicy(kInstallPolicyApp1, kPolicyDisabled);
  EXPECT_SUCCEEDED(app_->CheckGroupPolicy());
}

TEST_P(AppInstallTest, CheckGroupPolicy_AllUpdatesDisabled) {
  SetEnrolledPolicy(kUpdatePolicyApp1, kPolicyDisabled);
  EXPECT_SUCCEEDED(app_->CheckGroupPolicy());
}

TEST_P(AppManualUpdateTest, CheckGroupPolicy_AllUpdatesDisabled) {
  SetEnrolledPolicy(kUpdatePolicyApp1, kPolicyDisabled);
  EXPECT_EQ(IsDomain() ? GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY : S_OK,
            app_->CheckGroupPolicy());
}

TEST_P(AppAutoUpdateTest, CheckGroupPolicy_AllUpdatesDisabled) {
  SetEnrolledPolicy(kUpdatePolicyApp1, kPolicyDisabled);
  EXPECT_EQ(IsDomain() ? GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY : S_OK,
            app_->CheckGroupPolicy());
}

TEST_P(AppInstallTest, CheckGroupPolicy_AutoUpdatesDisabled) {
  SetEnrolledPolicy(kUpdatePolicyApp1, kPolicyManualUpdatesOnly);
  EXPECT_SUCCEEDED(app_->CheckGroupPolicy());
}

TEST_P(AppManualUpdateTest, CheckGroupPolicy_AutoUpdatesDisabled) {
  SetEnrolledPolicy(kUpdatePolicyApp1, kPolicyManualUpdatesOnly);
  EXPECT_SUCCEEDED(app_->CheckGroupPolicy());
}

TEST_P(AppAutoUpdateTest, CheckGroupPolicy_AutoUpdatesDisabled) {
  SetEnrolledPolicy(kUpdatePolicyApp1, kPolicyManualUpdatesOnly);
  EXPECT_EQ(IsDomain() ? GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY : S_OK,
            app_->CheckGroupPolicy());
}

TEST_P(AppInstallTest, CheckGroupPolicy_ManualUpdatesDisabled) {
  SetEnrolledPolicy(kUpdatePolicyApp1, kPolicyAutomaticUpdatesOnly);
  EXPECT_SUCCEEDED(app_->CheckGroupPolicy());
}

TEST_P(AppManualUpdateTest, CheckGroupPolicy_ManualUpdatesDisabled) {
  SetEnrolledPolicy(kUpdatePolicyApp1, kPolicyAutomaticUpdatesOnly);
  EXPECT_EQ(IsDomain() ? GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY_MANUAL : S_OK,
            app_->CheckGroupPolicy());
}

TEST_P(AppAutoUpdateTest, CheckGroupPolicy_ManualUpdatesDisabled) {
  SetEnrolledPolicy(kUpdatePolicyApp1, kPolicyAutomaticUpdatesOnly);
  EXPECT_SUCCEEDED(app_->CheckGroupPolicy());
}

//
// PostUpdateCheck Tests.
//

TEST_F(AppInstallTest, PostUpdateCheck_NoUpdate) {
  SetAppStateForUnitTest(app_, new fsm::AppStateCheckingForUpdate);
  AddAppResponse(xml::response::kStatusNoUpdate,
                 std::vector<xml::response::Data>());

  app_->PostUpdateCheck(S_OK, update_response_.get());
  EXPECT_EQ(STATE_ERROR, app_->state());
  EXPECT_EQ(GOOPDATE_E_NO_UPDATE_RESPONSE, app_->error_code());
}

TEST_F(AppInstallTest, PostUpdateCheck_UpdateAvailable) {
  SetAppStateForUnitTest(app_, new fsm::AppStateCheckingForUpdate);
  AddAppResponse(xml::response::kStatusOkValue,
                 std::vector<xml::response::Data>());

  app_->PostUpdateCheck(S_OK, update_response_.get());
  EXPECT_EQ(STATE_UPDATE_AVAILABLE, app_->state());
  EXPECT_EQ(S_OK, app_->error_code());
}

// Policy is not checked by this function.
TEST_P(AppInstallTest, PostUpdateCheck_UpdateAvailable_InstallDisabled) {
  SetEnrolledPolicy(kInstallPolicyApp1, kPolicyDisabled);
  SetAppStateForUnitTest(app_, new fsm::AppStateCheckingForUpdate);
  AddAppResponse(xml::response::kStatusOkValue,
                 std::vector<xml::response::Data>());

  app_->PostUpdateCheck(S_OK, update_response_.get());
  EXPECT_EQ(STATE_UPDATE_AVAILABLE, app_->state());
  EXPECT_EQ(S_OK, app_->error_code());
}

TEST_F(AppManualUpdateTest, PostUpdateCheck_NoUpdate) {
  SetAppStateForUnitTest(app_, new fsm::AppStateCheckingForUpdate);
  AddAppResponse(xml::response::kStatusNoUpdate,
                 std::vector<xml::response::Data>());

  app_->PostUpdateCheck(S_OK, update_response_.get());
  EXPECT_EQ(STATE_NO_UPDATE, app_->state());
  EXPECT_EQ(0, app_->error_code());
}

TEST_F(AppManualUpdateTest, PostUpdateCheck_UpdateAvailable) {
  SetAppStateForUnitTest(app_, new fsm::AppStateCheckingForUpdate);
  AddAppResponse(xml::response::kStatusOkValue,
                 std::vector<xml::response::Data>());

  app_->PostUpdateCheck(S_OK, update_response_.get());
  EXPECT_EQ(STATE_UPDATE_AVAILABLE, app_->state());
  EXPECT_EQ(S_OK, app_->error_code());
}

// Policy is not checked by this function.
TEST_P(AppManualUpdateTest,
       PostUpdateCheck_UpdateAvailable_AllUpdatesDisabled) {
  SetEnrolledPolicy(kUpdatePolicyApp1, kPolicyDisabled);
  SetAppStateForUnitTest(app_, new fsm::AppStateCheckingForUpdate);
  AddAppResponse(xml::response::kStatusOkValue,
                 std::vector<xml::response::Data>());

  app_->PostUpdateCheck(S_OK, update_response_.get());
  EXPECT_EQ(STATE_UPDATE_AVAILABLE, app_->state());
  EXPECT_EQ(S_OK, app_->error_code());
}

TEST_F(AppAutoUpdateTest, PostUpdateCheck_NoUpdate) {
  SetAppStateForUnitTest(app_, new fsm::AppStateCheckingForUpdate);
  AddAppResponse(xml::response::kStatusNoUpdate,
                 std::vector<xml::response::Data>());

  app_->PostUpdateCheck(S_OK, update_response_.get());
  EXPECT_EQ(STATE_NO_UPDATE, app_->state());
  EXPECT_EQ(0, app_->error_code());
}

TEST_F(AppAutoUpdateTest, PostUpdateCheck_UpdateAvailable) {
  SetAppStateForUnitTest(app_, new fsm::AppStateCheckingForUpdate);
  AddAppResponse(xml::response::kStatusOkValue,
                 std::vector<xml::response::Data>());

  app_->PostUpdateCheck(S_OK, update_response_.get());
  EXPECT_EQ(STATE_UPDATE_AVAILABLE, app_->state());
  EXPECT_EQ(S_OK, app_->error_code());
}

// Policy is not checked by this function.
TEST_P(AppAutoUpdateTest, PostUpdateCheck_UpdateAvailable_AllUpdatesDisabled) {
  SetEnrolledPolicy(kUpdatePolicyApp1, kPolicyDisabled);
  SetAppStateForUnitTest(app_, new fsm::AppStateCheckingForUpdate);
  AddAppResponse(xml::response::kStatusOkValue,
                 std::vector<xml::response::Data>());

  app_->PostUpdateCheck(S_OK, update_response_.get());
  EXPECT_EQ(STATE_UPDATE_AVAILABLE, app_->state());
  EXPECT_EQ(S_OK, app_->error_code());
}

TEST_F(AppAutoUpdateTest,
       PostUpdateCheck_UpdateAvailable_NonExistentInstallDataIndex) {
  SetAppStateForUnitTest(app_, new fsm::AppStateCheckingForUpdate);
  AddAppResponse(xml::response::kStatusOkValue,
                 std::vector<xml::response::Data>());
  app_->put_serverInstallDataIndex(CComBSTR(_T("NonExistent")));

  app_->PostUpdateCheck(S_OK, update_response_.get());
  EXPECT_EQ(STATE_ERROR, app_->state());
  EXPECT_EQ(GOOPDATE_E_INVALID_INSTALL_DATA_INDEX, app_->error_code());
}

TEST_F(AppAutoUpdateTest,
       PostUpdateCheck_UpdateAvailable_ExistingInstallDataIndex) {
  SetAppStateForUnitTest(app_, new fsm::AppStateCheckingForUpdate);
  std::vector<xml::response::Data> datas;
  xml::response::Data data;
  data.status = _T("ok");
  data.name = _T("install");
  data.install_data_index = _T("Existing");
  data.install_data = _T("{}");
  datas.push_back(data);
  AddAppResponse(xml::response::kStatusOkValue, datas);
  app_->put_serverInstallDataIndex(CComBSTR(_T("Existing")));

  app_->PostUpdateCheck(S_OK, update_response_.get());
  EXPECT_EQ(STATE_UPDATE_AVAILABLE, app_->state());
  EXPECT_EQ(S_OK, app_->error_code());
}

//
// QueueDownload Tests.
//

TEST_F(AppInstallTest, QueueDownload_NoPolicy) {
  SetAppStateForUnitTest(app_, new fsm::AppStateUpdateAvailable);

  app_->QueueDownload();
  EXPECT_EQ(STATE_WAITING_TO_DOWNLOAD, app_->state());
  EXPECT_EQ(S_OK, app_->error_code());
}

TEST_P(AppInstallTest, QueueDownload_InstallDisabled) {
  SetEnrolledPolicy(kInstallPolicyApp1, kPolicyDisabled);
  SetAppStateForUnitTest(app_, new fsm::AppStateUpdateAvailable);

  app_->QueueDownload();

  if (IsDomain()) {
    EXPECT_EQ(STATE_ERROR, app_->state());
  }

  EXPECT_EQ(IsDomain() ? GOOPDATE_E_APP_INSTALL_DISABLED_BY_POLICY : S_OK,
            app_->error_code());
}

TEST_P(AppInstallTest,
       QueueDownload_InstallDisabledForDifferentApp) {
  SetEnrolledPolicy(kInstallPolicyApp2, kPolicyDisabled);
  SetAppStateForUnitTest(app_, new fsm::AppStateUpdateAvailable);

  app_->QueueDownload();
  EXPECT_EQ(STATE_WAITING_TO_DOWNLOAD, app_->state());
  EXPECT_EQ(S_OK, app_->error_code());
}

TEST_P(AppInstallTest, QueueDownload_AllUpdatesDisabled) {
  SetEnrolledPolicy(kUpdatePolicyApp1, kPolicyDisabled);
  SetAppStateForUnitTest(app_, new fsm::AppStateUpdateAvailable);

  app_->QueueDownload();
  EXPECT_EQ(STATE_WAITING_TO_DOWNLOAD, app_->state());
  EXPECT_EQ(S_OK, app_->error_code());
}

TEST_F(AppManualUpdateTest, QueueDownload_NoPolicy) {
  SetAppStateForUnitTest(app_, new fsm::AppStateUpdateAvailable);

  app_->QueueDownload();
  EXPECT_EQ(STATE_WAITING_TO_DOWNLOAD, app_->state());
  EXPECT_EQ(S_OK, app_->error_code());
}

TEST_P(AppManualUpdateTest, QueueDownload_AllUpdatesDisabled) {
  SetEnrolledPolicy(kUpdatePolicyApp1, kPolicyDisabled);
  SetAppStateForUnitTest(app_, new fsm::AppStateUpdateAvailable);

  app_->QueueDownload();

  if (IsDomain()) {
    EXPECT_EQ(STATE_ERROR, app_->state());
  }

  EXPECT_EQ(IsDomain() ? GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY : S_OK,
            app_->error_code());
}

TEST_P(AppManualUpdateTest,
       QueueDownload_AllUpdatesDisabledForDifferentApp) {
  SetEnrolledPolicy(kUpdatePolicyApp2, kPolicyDisabled);
  SetAppStateForUnitTest(app_, new fsm::AppStateUpdateAvailable);

  app_->QueueDownload();
  EXPECT_EQ(STATE_WAITING_TO_DOWNLOAD, app_->state());
  EXPECT_EQ(S_OK, app_->error_code());
}

TEST_P(AppManualUpdateTest, QueueDownload_AutoUpdatesDisabled) {
  SetEnrolledPolicy(kUpdatePolicyApp1, kPolicyManualUpdatesOnly);
  SetAppStateForUnitTest(app_, new fsm::AppStateUpdateAvailable);

  app_->QueueDownload();
  EXPECT_EQ(STATE_WAITING_TO_DOWNLOAD, app_->state());
  EXPECT_EQ(S_OK, app_->error_code());
}

TEST_P(AppManualUpdateTest, QueueDownload_InstallDisabled) {
  SetEnrolledPolicy(kInstallPolicyApp1, kPolicyDisabled);
  SetAppStateForUnitTest(app_, new fsm::AppStateUpdateAvailable);

  app_->QueueDownload();
  EXPECT_EQ(STATE_WAITING_TO_DOWNLOAD, app_->state());
  EXPECT_EQ(S_OK, app_->error_code());
}

TEST_P(AppAutoUpdateTest, QueueDownload_AllUpdatesDisabled_NoPolicy) {
  SetAppStateForUnitTest(app_, new fsm::AppStateUpdateAvailable);

  app_->QueueDownload();
  EXPECT_EQ(STATE_WAITING_TO_DOWNLOAD, app_->state());
  EXPECT_EQ(S_OK, app_->error_code());
}

TEST_P(AppAutoUpdateTest, QueueDownload_AllUpdatesDisabled) {
  SetEnrolledPolicy(kUpdatePolicyApp1, kPolicyDisabled);
  SetAppStateForUnitTest(app_, new fsm::AppStateUpdateAvailable);

  app_->QueueDownload();

  if (IsDomain()) {
    EXPECT_EQ(STATE_ERROR, app_->state());
  }

  EXPECT_EQ(IsDomain() ? GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY : S_OK,
            app_->error_code());
}

TEST_P(AppAutoUpdateTest, QueueDownload_AllUpdatesDisabledForDifferentApp) {
  SetEnrolledPolicy(kUpdatePolicyApp2, kPolicyDisabled);
  SetAppStateForUnitTest(app_, new fsm::AppStateUpdateAvailable);

  app_->QueueDownload();
  EXPECT_EQ(STATE_WAITING_TO_DOWNLOAD, app_->state());
  EXPECT_EQ(S_OK, app_->error_code());
}

TEST_P(AppAutoUpdateTest, QueueDownload_AutoUpdatesDisabled) {
  SetEnrolledPolicy(kUpdatePolicyApp1, kPolicyManualUpdatesOnly);
  SetAppStateForUnitTest(app_, new fsm::AppStateUpdateAvailable);

  app_->QueueDownload();

  if (IsDomain()) {
    EXPECT_EQ(STATE_ERROR, app_->state());
  }

  EXPECT_EQ(IsDomain() ? GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY : S_OK,
            app_->error_code());
}

TEST_P(AppAutoUpdateTest, QueueDownload_InstallDisabled) {
  SetEnrolledPolicy(kInstallPolicyApp1, kPolicyDisabled);
  SetAppStateForUnitTest(app_, new fsm::AppStateUpdateAvailable);

  app_->QueueDownload();
  EXPECT_EQ(STATE_WAITING_TO_DOWNLOAD, app_->state());
  EXPECT_EQ(S_OK, app_->error_code());
}

//
// PreUpdateCheck Tests.
//

TEST_F(AppInstallTest, PreUpdateCheck_EulaAccepted) {
  SetAppStateForUnitTest(app_, new fsm::AppStateWaitingToCheckForUpdate);
  EXPECT_SUCCEEDED(app_->put_isEulaAccepted(VARIANT_TRUE));

  std::unique_ptr<xml::UpdateRequest> update_request;
  update_request.reset(xml::UpdateRequest::Create(is_machine_,
                                                  _T("unittest_sessionid"),
                                                  _T("unittest_instsource"),
                                                  CString()));
  EXPECT_TRUE(update_request->IsEmpty());

  app_->PreUpdateCheck(update_request.get());
  EXPECT_EQ(STATE_CHECKING_FOR_UPDATE, app_->state());
  EXPECT_EQ(S_OK, app_->error_code());
  EXPECT_FALSE(update_request->IsEmpty());
}

TEST_F(AppAutoUpdateTest, PreUpdateCheck_EulaAccepted) {
  SetAppStateForUnitTest(app_, new fsm::AppStateWaitingToCheckForUpdate);
  EXPECT_SUCCEEDED(app_->put_isEulaAccepted(VARIANT_TRUE));

  std::unique_ptr<xml::UpdateRequest> update_request;
  update_request.reset(xml::UpdateRequest::Create(is_machine_,
                                                  _T("unittest_sessionid"),
                                                  _T("unittest_instsource"),
                                                  CString()));
  EXPECT_TRUE(update_request->IsEmpty());

  app_->PreUpdateCheck(update_request.get());
  EXPECT_EQ(STATE_CHECKING_FOR_UPDATE, app_->state());
  EXPECT_EQ(S_OK, app_->error_code());
  EXPECT_FALSE(update_request->IsEmpty());
}

TEST_F(AppInstallTest, PreUpdateCheck_EulaNotAccepted_Online) {
  SetAppStateForUnitTest(app_, new fsm::AppStateWaitingToCheckForUpdate);
  EXPECT_SUCCEEDED(app_->put_isEulaAccepted(VARIANT_FALSE));

  std::unique_ptr<xml::UpdateRequest> update_request;
  update_request.reset(xml::UpdateRequest::Create(is_machine_,
                                                  _T("unittest_sessionid"),
                                                  _T("unittest_instsource"),
                                                  CString()));

  ExpectAsserts expect_asserts;  // Asserts because not is_update.

  app_->PreUpdateCheck(update_request.get());
  EXPECT_EQ(STATE_ERROR, app_->state());
  EXPECT_EQ(GOOPDATE_E_APP_UPDATE_DISABLED_EULA_NOT_ACCEPTED,
            app_->error_code());
  EXPECT_TRUE(update_request->IsEmpty()) << _T("Should not add request.");
}

TEST_F(AppInstallTest, PreUpdateCheck_EulaNotAccepted_Offline) {
  SetAppStateForUnitTest(app_, new fsm::AppStateWaitingToCheckForUpdate);
  EXPECT_SUCCEEDED(app_->put_isEulaAccepted(VARIANT_FALSE));
  EXPECT_SUCCEEDED(app_bundle_->put_offlineDirectory(CComBSTR(_T("foo"))));

  std::unique_ptr<xml::UpdateRequest> update_request;
  update_request.reset(xml::UpdateRequest::Create(is_machine_,
                                                  _T("unittest_sessionid"),
                                                  _T("unittest_instsource"),
                                                  CString()));

  app_->PreUpdateCheck(update_request.get());
  EXPECT_EQ(STATE_CHECKING_FOR_UPDATE, app_->state());
  EXPECT_EQ(S_OK, app_->error_code());
  EXPECT_FALSE(update_request->IsEmpty()) << _T("Should add request.");
}

TEST_F(AppAutoUpdateTest, PreUpdateCheck_EulaNotAccepted) {
  SetAppStateForUnitTest(app_, new fsm::AppStateWaitingToCheckForUpdate);
  EXPECT_SUCCEEDED(app_->put_isEulaAccepted(VARIANT_FALSE));

  std::unique_ptr<xml::UpdateRequest> update_request;
  update_request.reset(xml::UpdateRequest::Create(is_machine_,
                                                  _T("unittest_sessionid"),
                                                  _T("unittest_instsource"),
                                                  CString()));

  app_->PreUpdateCheck(update_request.get());
  EXPECT_EQ(STATE_ERROR, app_->state());
  EXPECT_EQ(GOOPDATE_E_APP_UPDATE_DISABLED_EULA_NOT_ACCEPTED,
            app_->error_code());
  EXPECT_TRUE(update_request->IsEmpty()) << _T("Should not add request.");
}

TEST_P(AppInstallTest, PreUpdateCheck_InstallDisabled) {
  SetEnrolledPolicy(kInstallPolicyApp1, kPolicyDisabled);
  SetAppStateForUnitTest(app_, new fsm::AppStateWaitingToCheckForUpdate);
  EXPECT_SUCCEEDED(app_->put_isEulaAccepted(VARIANT_TRUE));

  std::unique_ptr<xml::UpdateRequest> update_request;
  update_request.reset(xml::UpdateRequest::Create(is_machine_,
                                                  _T("unittest_sessionid"),
                                                  _T("unittest_instsource"),
                                                  CString()));
  EXPECT_TRUE(update_request->IsEmpty());

  app_->PreUpdateCheck(update_request.get());
  EXPECT_EQ(IsDomain() ? STATE_ERROR : STATE_CHECKING_FOR_UPDATE,
            app_->state());
  EXPECT_EQ(IsDomain() ? GOOPDATE_E_APP_INSTALL_DISABLED_BY_POLICY : S_OK,
            app_->error_code());
  EXPECT_EQ(IsDomain(), update_request->IsEmpty());
}

TEST_P(AppManualUpdateTest, PreUpdateCheck_InstallDisabled) {
  SetEnrolledPolicy(kInstallPolicyApp1, kPolicyDisabled);
  SetAppStateForUnitTest(app_, new fsm::AppStateWaitingToCheckForUpdate);
  EXPECT_SUCCEEDED(app_->put_isEulaAccepted(VARIANT_TRUE));

  std::unique_ptr<xml::UpdateRequest> update_request;
  update_request.reset(xml::UpdateRequest::Create(is_machine_,
                                                  _T("unittest_sessionid"),
                                                  _T("unittest_instsource"),
                                                  CString()));
  EXPECT_TRUE(update_request->IsEmpty());

  EXPECT_CALL(*mock_worker_, PurgeAppLowerVersions(_, _)).Times(1);

  app_->PreUpdateCheck(update_request.get());
  EXPECT_EQ(STATE_CHECKING_FOR_UPDATE, app_->state());
  EXPECT_EQ(S_OK, app_->error_code());
  EXPECT_FALSE(update_request->IsEmpty());
}

TEST_P(AppAutoUpdateTest, PreUpdateCheck_InstallDisabled) {
  SetEnrolledPolicy(kInstallPolicyApp1, kPolicyDisabled);
  SetAppStateForUnitTest(app_, new fsm::AppStateWaitingToCheckForUpdate);
  EXPECT_SUCCEEDED(app_->put_isEulaAccepted(VARIANT_TRUE));

  std::unique_ptr<xml::UpdateRequest> update_request;
  update_request.reset(xml::UpdateRequest::Create(is_machine_,
                                                  _T("unittest_sessionid"),
                                                  _T("unittest_instsource"),
                                                  CString()));
  EXPECT_TRUE(update_request->IsEmpty());

  EXPECT_CALL(*mock_worker_, PurgeAppLowerVersions(_, _)).Times(1);

  app_->PreUpdateCheck(update_request.get());
  EXPECT_EQ(STATE_CHECKING_FOR_UPDATE, app_->state());
  EXPECT_EQ(S_OK, app_->error_code());
  EXPECT_FALSE(update_request->IsEmpty());
}

TEST_P(AppInstallTest, PreUpdateCheck_InstallDisabledForDifferentApp) {
  SetEnrolledPolicy(kInstallPolicyApp2, kPolicyDisabled);
  SetAppStateForUnitTest(app_, new fsm::AppStateWaitingToCheckForUpdate);
  EXPECT_SUCCEEDED(app_->put_isEulaAccepted(VARIANT_TRUE));

  std::unique_ptr<xml::UpdateRequest> update_request;
  update_request.reset(xml::UpdateRequest::Create(is_machine_,
                                                  _T("unittest_sessionid"),
                                                  _T("unittest_instsource"),
                                                  CString()));
  EXPECT_TRUE(update_request->IsEmpty());

  app_->PreUpdateCheck(update_request.get());
  EXPECT_EQ(STATE_CHECKING_FOR_UPDATE, app_->state());
  EXPECT_EQ(S_OK, app_->error_code());
  EXPECT_FALSE(update_request->IsEmpty());
}

TEST_P(AppInstallTest, PreUpdateCheck_UpdateDisabled) {
  SetEnrolledPolicy(kUpdatePolicyApp1, kPolicyDisabled);
  SetAppStateForUnitTest(app_, new fsm::AppStateWaitingToCheckForUpdate);
  EXPECT_SUCCEEDED(app_->put_isEulaAccepted(VARIANT_TRUE));

  std::unique_ptr<xml::UpdateRequest> update_request;
  update_request.reset(xml::UpdateRequest::Create(is_machine_,
                                                  _T("unittest_sessionid"),
                                                  _T("unittest_instsource"),
                                                  CString()));
  EXPECT_TRUE(update_request->IsEmpty());

  app_->PreUpdateCheck(update_request.get());
  EXPECT_EQ(STATE_CHECKING_FOR_UPDATE, app_->state());
  EXPECT_EQ(S_OK, app_->error_code());
  EXPECT_FALSE(update_request->IsEmpty());
}

TEST_P(AppManualUpdateTest, PreUpdateCheck_UpdateDisabled) {
  SetEnrolledPolicy(kUpdatePolicyApp1, kPolicyDisabled);
  SetAppStateForUnitTest(app_, new fsm::AppStateWaitingToCheckForUpdate);
  EXPECT_SUCCEEDED(app_->put_isEulaAccepted(VARIANT_TRUE));

  std::unique_ptr<xml::UpdateRequest> update_request;
  update_request.reset(xml::UpdateRequest::Create(is_machine_,
                                                  _T("unittest_sessionid"),
                                                  _T("unittest_instsource"),
                                                  CString()));
  EXPECT_TRUE(update_request->IsEmpty());

  if (!IsDomain()) {
    EXPECT_CALL(*mock_worker_, PurgeAppLowerVersions(_, _)).Times(1);
  }

  app_->PreUpdateCheck(update_request.get());
  EXPECT_EQ(IsDomain() ? STATE_ERROR : STATE_CHECKING_FOR_UPDATE,
            app_->state());
  EXPECT_EQ(IsDomain() ? GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY : S_OK,
            app_->error_code());
  EXPECT_EQ(IsDomain(), update_request->IsEmpty());
}

TEST_P(AppAutoUpdateTest, PreUpdateCheck_UpdateDisabled) {
  SetEnrolledPolicy(kUpdatePolicyApp1, kPolicyDisabled);
  SetAppStateForUnitTest(app_, new fsm::AppStateWaitingToCheckForUpdate);
  EXPECT_SUCCEEDED(app_->put_isEulaAccepted(VARIANT_TRUE));

  std::unique_ptr<xml::UpdateRequest> update_request;
  update_request.reset(xml::UpdateRequest::Create(is_machine_,
                                                  _T("unittest_sessionid"),
                                                  _T("unittest_instsource"),
                                                  CString()));
  EXPECT_TRUE(update_request->IsEmpty());

  EXPECT_CALL(*mock_worker_, PurgeAppLowerVersions(_, _)).Times(1);

  app_->PreUpdateCheck(update_request.get());
  EXPECT_EQ(STATE_CHECKING_FOR_UPDATE, app_->state());
  EXPECT_EQ(S_OK, app_->error_code());
  EXPECT_FALSE(update_request->IsEmpty());
}

TEST_P(AppManualUpdateTest, PreUpdateCheck_UpdateDisabledForDifferentApp) {
  SetEnrolledPolicy(kUpdatePolicyApp2, kPolicyDisabled);
  SetAppStateForUnitTest(app_, new fsm::AppStateWaitingToCheckForUpdate);
  EXPECT_SUCCEEDED(app_->put_isEulaAccepted(VARIANT_TRUE));

  std::unique_ptr<xml::UpdateRequest> update_request;
  update_request.reset(xml::UpdateRequest::Create(is_machine_,
                                                  _T("unittest_sessionid"),
                                                  _T("unittest_instsource"),
                                                  CString()));
  EXPECT_TRUE(update_request->IsEmpty());

  EXPECT_CALL(*mock_worker_, PurgeAppLowerVersions(_, _)).Times(1);

  app_->PreUpdateCheck(update_request.get());
  EXPECT_EQ(STATE_CHECKING_FOR_UPDATE, app_->state());
  EXPECT_EQ(S_OK, app_->error_code());
  EXPECT_FALSE(update_request->IsEmpty());
}

TEST_P(AppAutoUpdateTest, PreUpdateCheck_UpdateDisabledForDifferentApp) {
  SetEnrolledPolicy(kUpdatePolicyApp2, kPolicyDisabled);
  SetAppStateForUnitTest(app_, new fsm::AppStateWaitingToCheckForUpdate);
  EXPECT_SUCCEEDED(app_->put_isEulaAccepted(VARIANT_TRUE));

  std::unique_ptr<xml::UpdateRequest> update_request;
  update_request.reset(xml::UpdateRequest::Create(is_machine_,
                                                  _T("unittest_sessionid"),
                                                  _T("unittest_instsource"),
                                                  CString()));
  EXPECT_TRUE(update_request->IsEmpty());

  EXPECT_CALL(*mock_worker_, PurgeAppLowerVersions(_, _)).Times(1);

  app_->PreUpdateCheck(update_request.get());
  EXPECT_EQ(STATE_CHECKING_FOR_UPDATE, app_->state());
  EXPECT_EQ(S_OK, app_->error_code());
  EXPECT_FALSE(update_request->IsEmpty());
}

TEST_P(AppInstallTest, PreUpdateCheck_ManualUpdatesOnly) {
  SetEnrolledPolicy(kUpdatePolicyApp1, kPolicyManualUpdatesOnly);
  SetAppStateForUnitTest(app_, new fsm::AppStateWaitingToCheckForUpdate);
  EXPECT_SUCCEEDED(app_->put_isEulaAccepted(VARIANT_TRUE));

  std::unique_ptr<xml::UpdateRequest> update_request;
  update_request.reset(xml::UpdateRequest::Create(is_machine_,
                                                  _T("unittest_sessionid"),
                                                  _T("unittest_instsource"),
                                                  CString()));
  EXPECT_TRUE(update_request->IsEmpty());

  app_->PreUpdateCheck(update_request.get());
  EXPECT_EQ(STATE_CHECKING_FOR_UPDATE, app_->state());
  EXPECT_EQ(S_OK, app_->error_code());
  EXPECT_FALSE(update_request->IsEmpty());
}

TEST_P(AppManualUpdateTest, PreUpdateCheck_ManualUpdatesOnly) {
  SetEnrolledPolicy(kUpdatePolicyApp1, kPolicyManualUpdatesOnly);
  SetAppStateForUnitTest(app_, new fsm::AppStateWaitingToCheckForUpdate);
  EXPECT_SUCCEEDED(app_->put_isEulaAccepted(VARIANT_TRUE));

  std::unique_ptr<xml::UpdateRequest> update_request;
  update_request.reset(xml::UpdateRequest::Create(is_machine_,
                                                  _T("unittest_sessionid"),
                                                  _T("unittest_instsource"),
                                                  CString()));
  EXPECT_TRUE(update_request->IsEmpty());

  EXPECT_CALL(*mock_worker_, PurgeAppLowerVersions(_, _)).Times(1);

  app_->PreUpdateCheck(update_request.get());
  EXPECT_EQ(STATE_CHECKING_FOR_UPDATE, app_->state());
  EXPECT_EQ(S_OK, app_->error_code());
  EXPECT_FALSE(update_request->IsEmpty());
}

TEST_P(AppAutoUpdateTest, PreUpdateCheck_ManualUpdatesOnly) {
  SetEnrolledPolicy(kUpdatePolicyApp1, kPolicyManualUpdatesOnly);
  SetAppStateForUnitTest(app_, new fsm::AppStateWaitingToCheckForUpdate);
  EXPECT_SUCCEEDED(app_->put_isEulaAccepted(VARIANT_TRUE));

  std::unique_ptr<xml::UpdateRequest> update_request;
  update_request.reset(xml::UpdateRequest::Create(is_machine_,
                                                  _T("unittest_sessionid"),
                                                  _T("unittest_instsource"),
                                                  CString()));
  EXPECT_TRUE(update_request->IsEmpty());

  EXPECT_CALL(*mock_worker_, PurgeAppLowerVersions(_, _)).Times(1);

  app_->PreUpdateCheck(update_request.get());
  EXPECT_EQ(STATE_CHECKING_FOR_UPDATE, app_->state());
  EXPECT_EQ(S_OK, app_->error_code());
  EXPECT_FALSE(update_request->IsEmpty());
}

TEST_F(AppInstallTest, InstallProgress_MissingInstallerProgress) {
  SetAppStateForUnitTest(app_, new fsm::AppStateInstalling);
  EXPECT_EQ(STATE_INSTALLING, app_->state());

  CComPtr<ICurrentState> icurrent_state;
  CComPtr<IDispatch> idispatch;
  EXPECT_SUCCEEDED(app_->get_currentState(&idispatch));
  EXPECT_SUCCEEDED(idispatch.QueryInterface(&icurrent_state));

  LONG local_time_remaining_ms = kCurrentStateProgressUnknown;
  EXPECT_SUCCEEDED(
      icurrent_state->get_installTimeRemainingMs(&local_time_remaining_ms));
  EXPECT_EQ(kCurrentStateProgressUnknown, local_time_remaining_ms);

  LONG local_percentage = kCurrentStateProgressUnknown;
  EXPECT_SUCCEEDED(icurrent_state->get_installProgress(&local_percentage));
  EXPECT_EQ(kCurrentStateProgressUnknown, local_percentage);
}

TEST_F(AppInstallTest, InstallProgress_ValidInstallerProgress) {
  SetAppStateForUnitTest(app_, new fsm::AppStateInstalling);
  EXPECT_EQ(STATE_INSTALLING, app_->state());

  const DWORD progress_percent = 11;
  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientStateKeyPathUser,
                                    kRegValueInstallerProgress,
                                    progress_percent));

  CComPtr<ICurrentState> icurrent_state;
  CComPtr<IDispatch> idispatch;
  EXPECT_SUCCEEDED(app_->get_currentState(&idispatch));
  EXPECT_SUCCEEDED(idispatch.QueryInterface(&icurrent_state));

  LONG local_time_remaining_ms = kCurrentStateProgressUnknown;
  EXPECT_SUCCEEDED(
      icurrent_state->get_installTimeRemainingMs(&local_time_remaining_ms));
  EXPECT_EQ(kCurrentStateProgressUnknown, local_time_remaining_ms);

  LONG local_percentage = kCurrentStateProgressUnknown;
  EXPECT_SUCCEEDED(icurrent_state->get_installProgress(&local_percentage));
  EXPECT_EQ(progress_percent, local_percentage);
}

TEST_F(AppInstallTest, InstallProgress_InvalidInstallerProgress) {
  SetAppStateForUnitTest(app_, new fsm::AppStateInstalling);
  EXPECT_EQ(STATE_INSTALLING, app_->state());

  const DWORD progress_percent = 111;
  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientStateKeyPathUser,
                                    kRegValueInstallerProgress,
                                    progress_percent));

  CComPtr<ICurrentState> icurrent_state;
  CComPtr<IDispatch> idispatch;
  EXPECT_SUCCEEDED(app_->get_currentState(&idispatch));
  EXPECT_SUCCEEDED(idispatch.QueryInterface(&icurrent_state));

  LONG local_time_remaining_ms = kCurrentStateProgressUnknown;
  EXPECT_SUCCEEDED(
      icurrent_state->get_installTimeRemainingMs(&local_time_remaining_ms));
  EXPECT_EQ(kCurrentStateProgressUnknown, local_time_remaining_ms);

  LONG local_percentage = kCurrentStateProgressUnknown;
  EXPECT_SUCCEEDED(icurrent_state->get_installProgress(&local_percentage));
  EXPECT_EQ(100, local_percentage);
}

// Tests the interface for accessing experiments labels.
TEST_F(AppInstallTest, ExperimentLabels) {
  // Create a bundle of one app, set an experiment label for that app, and
  // verify that the experiment labels are retrieved with and without
  // timestamps, respectively.
  App* app = NULL;   // The app object is owned by the bundle.
  EXPECT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kChromeAppId), &app));
  ASSERT_TRUE(app);

  const TCHAR expiration_date[] = _T("Sun, 09 Mar 2025 16:13:03 GMT");
  const time64 expiration = 133860103830000000uI64;

  CString label(ExperimentLabels::CreateLabel(
      _T("label key"), _T("label value"), expiration));
  EXPECT_SUCCEEDED(ExperimentLabels::WriteRegistry(false, kChromeAppId, label));

  EXPECT_STREQ(_T("label key=label value|Sun, 09 Mar 2025 16:13:03 GMT"),
               app->GetExperimentLabels());
  EXPECT_STREQ(_T("label key=label value"),
               app->GetExperimentLabelsNoTimestamps());
}

}  // namespace omaha
