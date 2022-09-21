// Copyright 2021 Google LLC.
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

#include "omaha/goopdate/dm_messages.h"

#include "omaha/testing/unit_test.h"
#include "wireless/android/enterprise/devicemanagement/proto/dm_api.pb.h"
#include "wireless/android/enterprise/devicemanagement/proto/omaha_settings.pb.h"

namespace omaha {

class DmMessagesTest : public ::testing::Test {
 protected:
  void FillFetchResponseWithValidOmahaPolicy(
      enterprise_management::PolicyFetchResponse* response) {
    wireless_android_enterprise_devicemanagement::OmahaSettingsClientProto
        omaha_settings;

    omaha_settings.set_auto_update_check_period_minutes(111);
    omaha_settings.set_download_preference(
        CStringA(kDownloadPreferenceCacheable));
    omaha_settings.mutable_updates_suppressed()->set_start_hour(8);
    omaha_settings.mutable_updates_suppressed()->set_start_minute(8);
    omaha_settings.mutable_updates_suppressed()->set_duration_min(47);
    omaha_settings.set_proxy_mode(CStringA("PAC_script"));
    omaha_settings.set_proxy_pac_url("foo.c/proxy.pa");
    omaha_settings.set_install_default(
        wireless_android_enterprise_devicemanagement::INSTALL_DEFAULT_DISABLED);
    omaha_settings.set_update_default(
        wireless_android_enterprise_devicemanagement::MANUAL_UPDATES_ONLY);

    wireless_android_enterprise_devicemanagement::ApplicationSettings app;
    app.set_app_guid(CStringA(kChromeAppId));

    app.set_install(
        wireless_android_enterprise_devicemanagement::INSTALL_DISABLED);
    app.set_update(
        wireless_android_enterprise_devicemanagement::AUTOMATIC_UPDATES_ONLY);
    app.set_target_version_prefix("3.6.55");
    app.set_rollback_to_target_version(
        wireless_android_enterprise_devicemanagement::
            ROLLBACK_TO_TARGET_VERSION_ENABLED);
    app.set_target_channel("beta");

    auto repeated_app_settings = omaha_settings.mutable_application_settings();
    repeated_app_settings->Add(std::move(app));

    enterprise_management::PolicyData policy_data;
    policy_data.set_policy_value(omaha_settings.SerializeAsString());

    response->set_policy_data(policy_data.SerializeAsString());
  }

  void FillFetchResponseWithErroneousOmahaPolicy(
      enterprise_management::PolicyFetchResponse* response) {
    wireless_android_enterprise_devicemanagement::OmahaSettingsClientProto
        omaha_settings;

    omaha_settings.set_auto_update_check_period_minutes(43201);
    omaha_settings.set_download_preference("InvalidDownloadPreference");
    omaha_settings.mutable_updates_suppressed()->set_start_hour(25);
    omaha_settings.mutable_updates_suppressed()->set_start_minute(-1);
    omaha_settings.mutable_updates_suppressed()->set_duration_min(1000);
    omaha_settings.set_proxy_mode("weird_proxy_mode");
    omaha_settings.set_proxy_server("unexpected_proxy");
    omaha_settings.set_proxy_pac_url("foo.c/proxy.pa");
    omaha_settings.set_install_default(
        wireless_android_enterprise_devicemanagement::INSTALL_DEFAULT_DISABLED);
    omaha_settings.set_update_default(
        wireless_android_enterprise_devicemanagement::MANUAL_UPDATES_ONLY);

    wireless_android_enterprise_devicemanagement::ApplicationSettings app;
    app.set_app_guid(CStringA(kChromeAppId));

    app.set_install(
        wireless_android_enterprise_devicemanagement::INSTALL_DISABLED);
    app.set_update(
        wireless_android_enterprise_devicemanagement::AUTOMATIC_UPDATES_ONLY);
    app.set_target_channel("");
    app.set_target_version_prefix("");

    auto repeated_app_settings = omaha_settings.mutable_application_settings();
    repeated_app_settings->Add(std::move(app));

    enterprise_management::PolicyData policy_data;
    policy_data.set_policy_value(omaha_settings.SerializeAsString());

    response->set_policy_data(policy_data.SerializeAsString());
  }
};

TEST_F(DmMessagesTest, ValidateOmahaPolicyResponse_RejectPolicyWithoutData) {
  enterprise_management::PolicyFetchResponse response;
  PolicyValidationResult validation_result;
  EXPECT_FALSE(ValidateOmahaPolicyResponse(response, &validation_result));
  EXPECT_EQ(validation_result.status,
            PolicyValidationResult::Status::kValidationPayloadParseError);
}

TEST_F(DmMessagesTest, ValidateOmahaPolicyResponse_RejectNonOmahaPolicy) {
  enterprise_management::PolicyFetchResponse response;
  enterprise_management::PolicyData policy_data;
  policy_data.set_policy_value("non-omaha-policy-data");
  response.set_policy_data(policy_data.SerializeAsString());
  PolicyValidationResult validation_result;
  EXPECT_FALSE(ValidateOmahaPolicyResponse(response, &validation_result));
  EXPECT_EQ(validation_result.status,
            PolicyValidationResult::Status::kValidationPolicyParseError);
  EXPECT_EQ(validation_result.issues.size(), 0);
}

TEST_F(DmMessagesTest, ValidateOmahaPolicyResponse_Success) {
  enterprise_management::PolicyFetchResponse response;
  FillFetchResponseWithValidOmahaPolicy(&response);
  PolicyValidationResult validation_result;
  EXPECT_TRUE(ValidateOmahaPolicyResponse(response, &validation_result));
  EXPECT_EQ(validation_result.status,
            PolicyValidationResult::Status::kValidationOK);
  EXPECT_EQ(validation_result.issues.size(), 0);
}

TEST_F(DmMessagesTest, ValidateOmahaPolicyResponse_ErrorPolicyValues) {
  enterprise_management::PolicyFetchResponse response;
  FillFetchResponseWithErroneousOmahaPolicy(&response);
  PolicyValidationResult validation_result;
  EXPECT_FALSE(ValidateOmahaPolicyResponse(response, &validation_result));
  EXPECT_EQ(validation_result.status,
            PolicyValidationResult::Status::kValidationOK);
  EXPECT_EQ(validation_result.issues.size(), 10);

  // auto_update_check_period_minutes
  EXPECT_STREQ(validation_result.issues[0].policy_name.c_str(),
               "auto_update_check_period_minutes");
  EXPECT_EQ(validation_result.issues[0].severity,
            PolicyValueValidationIssue::Severity::kError);
  EXPECT_STREQ(validation_result.issues[0].message.c_str(),
               "Value out of range (0 - 43200): 43201");

  // download_preference
  EXPECT_STREQ(validation_result.issues[1].policy_name.c_str(),
               "download_preference");
  EXPECT_EQ(validation_result.issues[1].severity,
            PolicyValueValidationIssue::Severity::kWarning);
  EXPECT_STREQ(validation_result.issues[1].message.c_str(),
               "Unrecognized download preference: InvalidDownloadPreference");

  // updates_suppressed.start_hour
  EXPECT_STREQ(validation_result.issues[2].policy_name.c_str(),
               "updates_suppressed.start_hour");
  EXPECT_EQ(validation_result.issues[2].severity,
            PolicyValueValidationIssue::Severity::kError);
  EXPECT_STREQ(validation_result.issues[2].message.c_str(),
               "Value out of range(0 - 23) : 25");

  // updates_suppressed.start_minute
  EXPECT_STREQ(validation_result.issues[3].policy_name.c_str(),
               "updates_suppressed.start_minute");
  EXPECT_EQ(validation_result.issues[3].severity,
            PolicyValueValidationIssue::Severity::kError);
  EXPECT_STREQ(validation_result.issues[3].message.c_str(),
               "Value out of range(0 - 59) : -1");

  // updates_suppressed.duration_min
  EXPECT_STREQ(validation_result.issues[4].policy_name.c_str(),
               "updates_suppressed.duration_min");
  EXPECT_EQ(validation_result.issues[4].severity,
            PolicyValueValidationIssue::Severity::kError);
  EXPECT_STREQ(validation_result.issues[4].message.c_str(),
               "Value out of range(0 - 960) : 1000");

  // proxy_mode
  EXPECT_STREQ(validation_result.issues[5].policy_name.c_str(), "proxy_mode");
  EXPECT_EQ(validation_result.issues[5].severity,
            PolicyValueValidationIssue::Severity::kError);
  EXPECT_STREQ(validation_result.issues[5].message.c_str(),
               "Unrecognized proxy mode: weird_proxy_mode");

  // proxy_server
  EXPECT_STREQ(validation_result.issues[6].policy_name.c_str(), "proxy_server");
  EXPECT_EQ(validation_result.issues[6].severity,
            PolicyValueValidationIssue::Severity::kWarning);
  EXPECT_STREQ(validation_result.issues[6].message.c_str(),
               "Proxy server setting [unexpected_proxy] is ignored because "
               "proxy mode is not fixed_servers");

  // proxy_pac_url
  EXPECT_STREQ(validation_result.issues[7].policy_name.c_str(),
               "proxy_pac_url");
  EXPECT_EQ(validation_result.issues[7].severity,
            PolicyValueValidationIssue::Severity::kWarning);
  EXPECT_STREQ(validation_result.issues[7].message.c_str(),
               "Proxy Pac URL setting [foo.c/proxy.pa] is ignored because "
               "proxy mode is not pac_script");

  // target_channel
  EXPECT_STREQ(validation_result.issues[8].policy_name.c_str(),
               "target_channel");
  EXPECT_EQ(validation_result.issues[8].severity,
            PolicyValueValidationIssue::Severity::kWarning);
  EXPECT_STREQ(validation_result.issues[8].message.c_str(),
               "{8A69D345-D564-463C-AFF1-A69D9E530F96} empty policy value");

  // target_version_prefix
  EXPECT_STREQ(validation_result.issues[9].policy_name.c_str(),
               "target_version_prefix");
  EXPECT_EQ(validation_result.issues[9].severity,
            PolicyValueValidationIssue::Severity::kWarning);
  EXPECT_STREQ(validation_result.issues[9].message.c_str(),
               "{8A69D345-D564-463C-AFF1-A69D9E530F96} empty policy value");
}

TEST_F(DmMessagesTest, ShouldDeleteDmToken) {
  EXPECT_FALSE(ShouldDeleteDmToken(std::vector<uint8>()));

  std::vector<uint8> response;
  std::string response_string("unparseable string");
  response.assign(response_string.begin(), response_string.end());
  EXPECT_FALSE(ShouldDeleteDmToken(response));

  enterprise_management::DeviceManagementResponse dm_response;
  enterprise_management::PolicyFetchResponse* policy_response =
      dm_response.mutable_policy_response()->add_responses();
  enterprise_management::PolicyData policy_data;
  policy_data.set_policy_type("test_policy_type");
  policy_data.set_policy_value("test policy value");
  policy_data.set_username("user");
  policy_data.set_request_token("TestToken");
  policy_data.set_device_id(CStringA("TestDeviceId"));
  policy_data.set_timestamp(time(NULL));
  policy_response->set_policy_data(policy_data.SerializeAsString());
  ASSERT_TRUE(dm_response.SerializeToString(&response_string));
  response.assign(response_string.begin(), response_string.end());
  EXPECT_FALSE(ShouldDeleteDmToken(response));

  dm_response.add_error_detail(
      enterprise_management::CBCM_DELETION_POLICY_PREFERENCE_DELETE_TOKEN);
  ASSERT_TRUE(dm_response.SerializeToString(&response_string));
  response.assign(response_string.begin(), response_string.end());
  EXPECT_TRUE(ShouldDeleteDmToken(response));
}

}  // namespace omaha
