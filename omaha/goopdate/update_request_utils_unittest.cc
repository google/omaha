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
#include "omaha/common/experiment_labels.h"
#include "omaha/common/update_response.h"
#include "omaha/goopdate/app_manager.h"
#include "omaha/goopdate/app_unittest_base.h"
#include "omaha/goopdate/update_request_utils.h"
#include "omaha/testing/unit_test.h"

using ::testing::Return;

namespace omaha {

namespace update_request_utils {

namespace {

#define USER_UPDATE_KEY \
    _T("HKCU\\Software\\") PATH_COMPANY_NAME _T("\\") PRODUCT_NAME _T("\\")
#define APP_ID1 _T("{DDE97E2B-A82C-4790-A630-FCA02F64E8BE}");
const TCHAR* const kAppId1 = APP_ID1
const TCHAR* const kAppId1ClientStateKeyPathUser =
    USER_UPDATE_KEY _T("ClientState\\") APP_ID1;
const TCHAR* const kInstallPolicyApp1 = _T("Install") APP_ID1;
const TCHAR* const kAppDidRunValueName = _T("dr");

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
  std::unique_ptr<xml::UpdateRequest> update_request_;

 private:
  DISALLOW_COPY_AND_ASSIGN(UpdateRequestUtilsTest);
};

INSTANTIATE_TEST_CASE_P(IsDomain, UpdateRequestUtilsTest, ::testing::Bool());

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

TEST_P(UpdateRequestUtilsTest,
       BuildRequest_UpdateCheck_GroupPolicy_InstallDisabled) {
  EXPECT_SUCCEEDED(app_->put_isEulaAccepted(VARIANT_TRUE));

  SetEnrolledPolicy(kInstallPolicyApp1, kPolicyDisabled);

  BuildRequest(app_, true, update_request_.get());

  const xml::request::Request& request = update_request_->request();
  ASSERT_EQ(1, request.apps.size());

  const xml::request::App& app = request.apps[0];
  const xml::request::UpdateCheck& update_check = app.update_check;
  EXPECT_TRUE(update_check.is_valid);
  EXPECT_EQ(IsDomain(), update_check.is_update_disabled);
}

TEST_P(UpdateRequestUtilsTest,
       BuildRequest_UpdateCheck_GroupPolicy_TargetVersionPrefix) {
  EXPECT_SUCCEEDED(app_->put_isEulaAccepted(VARIANT_TRUE));

  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE_DEV,
                                    kRegValueIsEnrolledToDomain,
                                    IsDomain() ? 1UL : 0UL));

  const TCHAR* const kTargetVersionPrefixApp1 =
      _T("TargetVersionPrefix") APP_ID1;
  SetPolicyString(kTargetVersionPrefixApp1, _T("55.3"));

  BuildRequest(app_, true, update_request_.get());

  const xml::request::Request& request = update_request_->request();
  ASSERT_EQ(1, request.apps.size());

  const xml::request::App& app = request.apps[0];
  const xml::request::UpdateCheck& update_check = app.update_check;
  EXPECT_TRUE(update_check.is_valid);
  EXPECT_STREQ(IsDomain() ? _T("55.3") : _T(""),
               update_check.target_version_prefix);
}

TEST_P(UpdateRequestUtilsTest,
       BuildRequest_UpdateCheck_GroupPolicy_RollbackToTargetVersion) {
  EXPECT_SUCCEEDED(app_->put_isEulaAccepted(VARIANT_TRUE));

  const TCHAR* const kRollbackToTargetVersionApp1 =
      _T("RollbackToTargetVersion") APP_ID1;
  SetEnrolledPolicy(kRollbackToTargetVersionApp1, 1UL);

  BuildRequest(app_, true, update_request_.get());

  const xml::request::Request& request = update_request_->request();
  ASSERT_EQ(1, request.apps.size());

  const xml::request::App& app = request.apps[0];
  const xml::request::UpdateCheck& update_check = app.update_check;
  EXPECT_TRUE(update_check.is_valid);
  EXPECT_EQ(IsDomain() ? true : false, update_check.is_rollback_allowed);
}

TEST_P(UpdateRequestUtilsTest,
       BuildRequest_UpdateCheck_GroupPolicy_TargetChannel) {
  EXPECT_SUCCEEDED(app_->put_isEulaAccepted(VARIANT_TRUE));

  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE_DEV,
                                    kRegValueIsEnrolledToDomain,
                                    IsDomain() ? 1UL : 0UL));

  const TCHAR* const kTargetChannelApp1 = _T("TargetChannel") APP_ID1;
  SetPolicyString(kTargetChannelApp1, _T("beta"));

  BuildRequest(app_, true, update_request_.get());

  const xml::request::Request& request = update_request_->request();
  ASSERT_EQ(1, request.apps.size());

  const xml::request::App& app = request.apps[0];
  const xml::request::UpdateCheck& update_check = app.update_check;
  EXPECT_TRUE(update_check.is_valid);
  EXPECT_STREQ(IsDomain() ? _T("beta") : _T(""), update_check.target_channel);
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
  EXPECT_EQ(0, ping.day_of_last_activity);
  EXPECT_EQ(0, ping.day_of_last_roll_call);
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
  EXPECT_EQ(-1, ping.day_of_last_activity);
  EXPECT_EQ(-1, ping.day_of_last_roll_call);
}

TEST_F(UpdateRequestUtilsTest,
       BuildRequest_UpdateCheckShouldSendAppDefinedAttributes) {
  EXPECT_SUCCEEDED(app_->put_isEulaAccepted(VARIANT_TRUE));

  RegKey key;
  EXPECT_SUCCEEDED(key.Create(kAppId1ClientStateKeyPathUser));

  struct AttributePair {
    CString attribute_name;
    CString attribute_value;
  };

  AttributePair pairs[] = {
    {_T("_Foo"), _T("***")},
    {_T("_Bar"), _T("&&&&")},
    {_T("_Baz"), _T("BazBaz")},
  };

  for (int i = 0; i < arraysize(pairs); ++i) {
    EXPECT_SUCCEEDED(key.SetValue(pairs[i].attribute_name,
                                  pairs[i].attribute_value));
  }

  __mutexScope(app_->model()->lock());
  AppManager::Instance()->ReadAppPersistentData(app_);

  BuildRequest(app_, true, update_request_.get());

  const xml::request::Request& request = update_request_->request();
  EXPECT_EQ(1, request.apps.size());
  EXPECT_EQ(arraysize(pairs), request.apps[0].app_defined_attributes.size());
  for (int i = 0; i < arraysize(pairs); ++i) {
    EXPECT_STREQ(pairs[i].attribute_name.MakeLower(),
                 request.apps[0].app_defined_attributes[i].first);
    EXPECT_STREQ(pairs[i].attribute_value,
                 request.apps[0].app_defined_attributes[i].second);
  }
}

TEST_F(UpdateRequestUtilsTest,
       BuildRequest_UpdateCheckShouldSendCohortAttributes) {
  EXPECT_SUCCEEDED(app_->put_isEulaAccepted(VARIANT_TRUE));

  RegKey key;
  const CString cohort_key_name(
      AppendRegKeyPath(kAppId1ClientStateKeyPathUser, kRegSubkeyCohort));
  EXPECT_SUCCEEDED(key.Create(cohort_key_name));

  const Cohort cohorts[] = {
    {_T("Cohort1"), _T("Hint1"), _T("Name1")},
    {_T("Cohort2"), _T(""), _T("Name2")},
    {_T("Cohort3"), _T("Hint3"), _T("")},
  };

  for (size_t i = 0; i < arraysize(cohorts); ++i) {
    EXPECT_SUCCEEDED(key.SetValue(NULL, cohorts[i].cohort));
    EXPECT_SUCCEEDED(key.SetValue(kRegValueCohortHint, cohorts[i].hint));
    EXPECT_SUCCEEDED(key.SetValue(kRegValueCohortName, cohorts[i].name));

    __mutexScope(app_->model()->lock());
    AppManager::Instance()->ReadAppPersistentData(app_);

    BuildRequest(app_, true, update_request_.get());

    const xml::request::Request& request = update_request_->request();
    EXPECT_STREQ(cohorts[i].cohort, request.apps[i].cohort);
    EXPECT_STREQ(cohorts[i].hint, request.apps[i].cohort_hint);
    EXPECT_STREQ(cohorts[i].name, request.apps[i].cohort_name);
  }
}

TEST_F(UpdateRequestUtilsTest, PingFreshness) {
  const CString ping_freshness(_T("{e23bd96a-b6df-4cfe-89bc-70d1e71afca2}"));
  EXPECT_SUCCEEDED(RegKey::SetValue(kAppId1ClientStateKeyPathUser,
                                    kRegValuePingFreshness,
                                    ping_freshness));

  __mutexScope(app_->model()->lock());
  AppManager::Instance()->ReadAppPersistentData(app_);
  EXPECT_STREQ(ping_freshness, app_->ping_freshness());

  BuildRequest(app_, true, update_request_.get());

  const xml::request::Request& request = update_request_->request();
  EXPECT_EQ(1, request.apps.size());
  EXPECT_STREQ(ping_freshness, request.apps[0].ping.ping_freshness);
}

// Tests that the experiment label is picked up when the request is built and
// the experiment label does not contain a timestamp.
TEST_F(UpdateRequestUtilsTest, ExperimentLabels) {
  const TCHAR expiration_date[] = _T("Sun, 09 Mar 2025 16:13:03 GMT");
  const time64 expiration = 133860103830000000uI64;

  CString label(ExperimentLabels::CreateLabel(
      _T("label key"), _T("label value"), expiration));
  EXPECT_SUCCEEDED(ExperimentLabels::WriteRegistry(
      false, app_->app_guid_string(), label));

  EXPECT_SUCCEEDED(app_->put_isEulaAccepted(VARIANT_TRUE));
  BuildRequest(app_, true, update_request_.get());

  const xml::request::Request& request = update_request_->request();
  EXPECT_EQ(1, request.apps.size());
  EXPECT_STREQ(_T("label key=label value"), request.apps[0].experiments);
}

}  // namespace update_request_utils

}  // namespace omaha
