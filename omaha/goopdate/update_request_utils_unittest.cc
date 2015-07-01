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

#include "omaha/base/reg_key.h"
#include "omaha/common/const_group_policy.h"
#include "omaha/common/update_response.h"
#include "omaha/goopdate/app_manager.h"
#include "omaha/goopdate/app_unittest_base.h"
#include "omaha/goopdate/update_request_utils.h"
#include "omaha/testing/unit_test.h"

using ::testing::Return;

namespace omaha {

namespace update_request_utils {

namespace {

#define USER_UPDATE_KEY _T("HKCU\\Software\\") SHORT_COMPANY_NAME _T("\\") PRODUCT_NAME _T("\\")
#define APP_ID1 _T("{DDE97E2B-A82C-4790-A630-FCA02F64E8BE}");
const TCHAR* const kAppId1 = APP_ID1
const TCHAR* const kAppId1ClientsKeyPathUser =
    USER_UPDATE_KEY _T("Clients\\") APP_ID1;
const TCHAR* const kAppId1ClientStateKeyPathUser =
    USER_UPDATE_KEY _T("ClientState\\") APP_ID1;
const TCHAR* const kInstallPolicyApp1 = _T("Install") APP_ID1;
const TCHAR* const kUpdatePolicyApp1 = _T("Update") APP_ID1;
const TCHAR* const kAppDidRunValueName = _T("dr");

void SetPolicy(const CString& policy, DWORD value) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kRegKeyGoopdateGroupPolicy,
                                    policy,
                                    value));
}

}  // namespace

class UpdateRequestUtilsTest : public AppTestBaseWithRegistryOverride {
 protected:
  UpdateRequestUtilsTest()
      : AppTestBaseWithRegistryOverride(false,  // is_machine
                                        true),  // use_strict_mock
        app_(NULL) {}

  virtual void SetUp() {
    AppTestBaseWithRegistryOverride::SetUp();

    update_request_.reset(xml::UpdateRequest::Create(is_machine_,
                                                     _T("unittest"),
                                                     _T("unittest"),
                                                     CString()));

    EXPECT_SUCCEEDED(
        app_bundle_->createApp(CComBSTR(kAppId1), &app_));
    ASSERT_TRUE(app_);
  }

  App* app_;
  scoped_ptr<xml::UpdateRequest> update_request_;

 private:
  DISALLOW_COPY_AND_ASSIGN(UpdateRequestUtilsTest);
};

// TODO(omaha): write tests.

// For now, this test is just checking !update_check.is_valid. Add more checks.
TEST_F(UpdateRequestUtilsTest, BuildRequest_Ping) {
  EXPECT_SUCCEEDED(app_->put_isEulaAccepted(VARIANT_TRUE));

  PingEventPtr ping_event(
      new PingEvent(PingEvent::EVENT_INSTALL_COMPLETE,
                    PingEvent::EVENT_RESULT_ERROR, E_FAIL, 0));
  app_->AddPingEvent(ping_event);

  BuildRequest(app_, false, update_request_.get());

  const xml::request::Request& request = update_request_->request();
  ASSERT_EQ(1, request.apps.size());

  const xml::request::App& app = request.apps[0];
  const xml::request::UpdateCheck& update_check = app.update_check;
  EXPECT_FALSE(update_check.is_valid);
}

TEST_F(UpdateRequestUtilsTest, BuildRequest_Ping_NoEvents) {
  BuildRequest(app_, false, update_request_.get());

  const xml::request::Request& request = update_request_->request();
  ASSERT_EQ(0, request.apps.size());
}

TEST_F(UpdateRequestUtilsTest, BuildRequest_Ping_EulaNotAccepted) {
  EXPECT_SUCCEEDED(app_->put_isEulaAccepted(VARIANT_FALSE));

  PingEventPtr ping_event(
      new PingEvent(PingEvent::EVENT_INSTALL_COMPLETE,
                    PingEvent::EVENT_RESULT_ERROR, E_FAIL, 0));
  app_->AddPingEvent(ping_event);

  BuildRequest(app_, false, update_request_.get());

  const xml::request::Request& request = update_request_->request();
  ASSERT_EQ(0, request.apps.size());
}

// For now, this test is just checking is_update_disabled. Add more checks.
TEST_F(UpdateRequestUtilsTest, BuildRequest_UpdateCheck) {
  EXPECT_SUCCEEDED(app_->put_isEulaAccepted(VARIANT_TRUE));

  BuildRequest(app_, true, update_request_.get());

  const xml::request::Request& request = update_request_->request();
  ASSERT_EQ(1, request.apps.size());

  const xml::request::App& app = request.apps[0];
  const xml::request::UpdateCheck& update_check = app.update_check;
  EXPECT_TRUE(update_check.is_valid);
  EXPECT_FALSE(update_check.is_update_disabled);
}

TEST_F(UpdateRequestUtilsTest,
       BuildRequest_UpdateCheck_GroupPolicy_InstallDisabled) {
  EXPECT_SUCCEEDED(app_->put_isEulaAccepted(VARIANT_TRUE));

  SetPolicy(kInstallPolicyApp1, kPolicyDisabled);

  BuildRequest(app_, true, update_request_.get());

  const xml::request::Request& request = update_request_->request();
  ASSERT_EQ(1, request.apps.size());

  const xml::request::App& app = request.apps[0];
  const xml::request::UpdateCheck& update_check = app.update_check;
  EXPECT_TRUE(update_check.is_valid);
  EXPECT_TRUE(update_check.is_update_disabled);
}

TEST_F(UpdateRequestUtilsTest,
       BuildRequest_DoNotPickUpDidRunValueWhenNotDoingUpdateCheck) {
  EXPECT_SUCCEEDED(app_->put_isEulaAccepted(VARIANT_TRUE));
  PingEventPtr ping_event(
      new PingEvent(PingEvent::EVENT_INSTALL_COMPLETE,
                    PingEvent::EVENT_RESULT_ERROR, E_FAIL, 0));
  app_->AddPingEvent(ping_event);

  RegKey key;
  ASSERT_SUCCEEDED(key.Create(kAppId1ClientStateKeyPathUser));
  ASSERT_SUCCEEDED(key.SetValue(kAppDidRunValueName, _T("1")));
  __mutexScope(app_->model()->lock());
  AppManager::Instance()->ReadAppPersistentData(app_);

  BuildRequest(app_, false, update_request_.get());

  const xml::request::Request& request = update_request_->request();
  ASSERT_EQ(1, request.apps.size());

  const xml::request::Ping& ping = request.apps[0].ping;
  EXPECT_EQ(ACTIVE_UNKNOWN, ping.active);
  EXPECT_EQ(0, ping.days_since_last_active_ping);
  EXPECT_EQ(0, ping.days_since_last_roll_call);
}

TEST_F(UpdateRequestUtilsTest,
       BuildRequest_UpdateCheckShouldSendDidRunValue) {
  EXPECT_SUCCEEDED(app_->put_isEulaAccepted(VARIANT_TRUE));
  PingEventPtr ping_event(
      new PingEvent(PingEvent::EVENT_INSTALL_COMPLETE,
                    PingEvent::EVENT_RESULT_ERROR, E_FAIL, 0));
  app_->AddPingEvent(ping_event);

  RegKey key;
  ASSERT_SUCCEEDED(key.Create(kAppId1ClientStateKeyPathUser));
  ASSERT_SUCCEEDED(key.SetValue(kAppDidRunValueName, _T("1")));
  __mutexScope(app_->model()->lock());
  AppManager::Instance()->ReadAppPersistentData(app_);

  BuildRequest(app_, true, update_request_.get());

  const xml::request::Request& request = update_request_->request();
  ASSERT_EQ(1, request.apps.size());

  const xml::request::Ping& ping = request.apps[0].ping;
  EXPECT_EQ(ACTIVE_RUN, ping.active);
  EXPECT_EQ(-1, ping.days_since_last_active_ping);
  EXPECT_EQ(-1, ping.days_since_last_roll_call);
}

}  // namespace update_request_utils

}  // namespace omaha
