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
//
// ApplicationData unit tests

#include <vector>
#include "base/scoped_ptr.h"
#include "omaha/common/utils.h"
#include "omaha/common/time.h"
#include "omaha/goopdate/command_line.h"
#include "omaha/goopdate/config_manager.h"
#include "omaha/goopdate/const_goopdate.h"
#include "omaha/testing/unit_test.h"
#include "omaha/worker/application_data.h"
#include "omaha/worker/job.h"

namespace omaha {

const TCHAR* const kGuid1 = _T("{21CD0965-0B0E-47cf-B421-2D191C16C0E2}");
const TCHAR* const kGuid2 = _T("{A979ACBD-1F55-4b12-A35F-4DBCA5A7CCB8}");
const TCHAR* const kGuid3 = _T("{661045C5-4429-4140-BC48-8CEA241D1DEF}");

  void ValidateDefaltValues(const AppData& data) {
    EXPECT_TRUE(::IsEqualGUID(GUID_NULL, data.app_guid()));
    EXPECT_TRUE(::IsEqualGUID(GUID_NULL, data.parent_app_guid()));
    EXPECT_FALSE(data.is_machine_app());
    EXPECT_TRUE(data.version().IsEmpty());
    EXPECT_TRUE(data.previous_version().IsEmpty());
    EXPECT_TRUE(data.language().IsEmpty());
    EXPECT_TRUE(data.ap().IsEmpty());
    EXPECT_TRUE(data.tt_token().IsEmpty());
    EXPECT_TRUE(::IsEqualGUID(GUID_NULL, data.iid()));
    EXPECT_TRUE(data.brand_code().IsEmpty());
    EXPECT_TRUE(data.client_id().IsEmpty());
    EXPECT_TRUE(data.referral_id().IsEmpty());
    EXPECT_EQ(0, data.install_time_diff_sec());
    EXPECT_FALSE(data.is_oem_install());
    EXPECT_TRUE(data.is_eula_accepted());
    EXPECT_TRUE(data.display_name().IsEmpty());
    EXPECT_EQ(BROWSER_UNKNOWN, data.browser_type());
    EXPECT_TRUE(data.install_source().IsEmpty());
    EXPECT_TRUE(data.encoded_installer_data().IsEmpty());
    EXPECT_TRUE(data.install_data_index().IsEmpty());
    EXPECT_EQ(TRISTATE_NONE, data.usage_stats_enable());
    EXPECT_EQ(AppData::ACTIVE_UNKNOWN, data.did_run());
    EXPECT_EQ(0, data.days_since_last_active_ping());
    EXPECT_EQ(0, data.days_since_last_roll_call());
    EXPECT_FALSE(data.is_uninstalled());
    EXPECT_FALSE(data.is_update_disabled());
  }

  void ValidateExpectedValues(const AppData& expected, const AppData& actual) {
    EXPECT_STREQ(GuidToString(expected.app_guid()),
                 GuidToString(actual.app_guid()));
    EXPECT_STREQ(GuidToString(expected.parent_app_guid()),
                 GuidToString(actual.parent_app_guid()));
    EXPECT_EQ(expected.is_machine_app(), actual.is_machine_app());
    EXPECT_STREQ(expected.version(), actual.version());
    EXPECT_STREQ(expected.previous_version(), actual.previous_version());
    EXPECT_STREQ(expected.language(), actual.language());
    EXPECT_STREQ(expected.ap(), actual.ap());
    EXPECT_STREQ(expected.tt_token(), actual.tt_token());
    EXPECT_STREQ(GuidToString(expected.iid()), GuidToString(actual.iid()));
    EXPECT_STREQ(expected.brand_code(), actual.brand_code());
    EXPECT_STREQ(expected.client_id(), actual.client_id());
    EXPECT_STREQ(expected.referral_id(), actual.referral_id());
    EXPECT_EQ(expected.install_time_diff_sec(), actual.install_time_diff_sec());
    EXPECT_EQ(expected.is_oem_install(), actual.is_oem_install());
    EXPECT_EQ(expected.is_eula_accepted(), actual.is_eula_accepted());
    EXPECT_STREQ(expected.display_name(), actual.display_name());
    EXPECT_EQ(expected.browser_type(), actual.browser_type());
    EXPECT_STREQ(expected.install_source(), actual.install_source());
    EXPECT_STREQ(expected.encoded_installer_data(),
                 actual.encoded_installer_data());
    EXPECT_STREQ(expected.install_data_index(), actual.install_data_index());
    EXPECT_EQ(expected.usage_stats_enable(), actual.usage_stats_enable());
    EXPECT_EQ(expected.did_run(), actual.did_run());
    EXPECT_EQ(expected.days_since_last_active_ping(),
              actual.days_since_last_active_ping());
    EXPECT_EQ(expected.days_since_last_roll_call(),
              actual.days_since_last_roll_call());
    EXPECT_EQ(expected.is_uninstalled(), actual.is_uninstalled());
    EXPECT_EQ(expected.is_update_disabled(), actual.is_update_disabled());
  }

void FillAppData(AppData* app_data) {
  const GUID app_id = StringToGuid(kGuid1);
  const bool is_machine_app = true;
  const GUID parent_app_id = StringToGuid(kGuid2);
  const CString version = _T("12345");
  const CString previous_version = _T("11111");
  const CString language = _T("en");
  const AppData::ActiveStates did_run = AppData::ACTIVE_RUN;
  const int days_since_last_active_ping = 2;
  const int days_since_last_roll_call = 1;
  const CString ap = _T("some_ap_value");
  const GUID iid = StringToGuid(kGuid3);
  const CString brand_code = _T("GOOG");
  const CString client_id = _T("some_client_id");
  const CString referral_id = _T("ABC987");
  const uint32 install_time_diff_sec = 98765;
  const bool is_oem_install = true;
  const bool is_eula_accepted = false;
  const CString encoded_installer_data = _T("%20foobar");
  const CString install_data_index = _T("foobar");
  const bool is_uninstalled = true;
  const bool is_update_disabled = false;

  AppData actual(app_id, is_machine_app);
  app_data->set_parent_app_guid(parent_app_id);
  app_data->set_version(version);
  app_data->set_previous_version(previous_version);
  app_data->set_language(language);
  app_data->set_did_run(did_run);
  app_data->set_days_since_last_active_ping(days_since_last_active_ping);
  app_data->set_days_since_last_roll_call(days_since_last_roll_call);
  app_data->set_ap(ap);
  app_data->set_iid(iid);
  app_data->set_brand_code(brand_code);
  app_data->set_client_id(client_id);
  app_data->set_referral_id(referral_id);
  app_data->set_install_time_diff_sec(install_time_diff_sec);
  app_data->set_is_oem_install(is_oem_install);
  app_data->set_is_eula_accepted(is_eula_accepted);
  app_data->set_encoded_installer_data(encoded_installer_data);
  app_data->set_install_data_index(install_data_index);
  app_data->set_is_uninstalled(is_uninstalled);
  app_data->set_is_update_disabled(is_update_disabled);
}

TEST(AppDataTest, TestAllParams) {
  const GUID app_guid = StringToGuid(kGuid1);
  const bool is_machine_app = true;
  const GUID parent_app_guid = StringToGuid(kGuid2);
  const CString version = _T("12345");
  const CString previous_version = _T("11111");
  const CString language = _T("en");
  const AppData::ActiveStates did_run = AppData::ACTIVE_RUN;
  const int days_since_last_active_ping = 3;
  const int days_since_last_roll_call = 2;
  const CString ap = _T("some_ap_value");
  const CString tt_token = _T("some_tt_token_value");
  const GUID iid = StringToGuid(kGuid3);
  const CString brand_code = _T("GOOG");
  const CString client_id = _T("some_client_id");
  const CString referral_id = _T("123456");
  const uint32 install_time_diff_sec = 123498;
  const bool is_oem_install = true;
  const bool is_eula_accepted = false;
  const CString encoded_installer_data = _T("%20foobar");
  const CString install_data_index = _T("foobar");
  const bool is_uninstalled = true;
  const bool is_update_disabled = false;

  AppData actual(app_guid, is_machine_app);
  actual.set_parent_app_guid(parent_app_guid);
  actual.set_version(version);
  actual.set_previous_version(previous_version);
  actual.set_language(language);
  actual.set_did_run(did_run);
  actual.set_days_since_last_active_ping(days_since_last_active_ping);
  actual.set_days_since_last_roll_call(days_since_last_roll_call);
  actual.set_ap(ap);
  actual.set_tt_token(tt_token);
  actual.set_iid(iid);
  actual.set_brand_code(brand_code);
  actual.set_client_id(client_id);
  actual.set_referral_id(referral_id);
  actual.set_install_time_diff_sec(install_time_diff_sec);
  actual.set_is_oem_install(is_oem_install);
  actual.set_is_eula_accepted(is_eula_accepted);
  actual.set_encoded_installer_data(encoded_installer_data);
  actual.set_install_data_index(install_data_index);
  actual.set_is_uninstalled(is_uninstalled);
  actual.set_is_update_disabled(is_update_disabled);

  EXPECT_TRUE(::IsEqualGUID(app_guid, actual.app_guid()));
  EXPECT_TRUE(::IsEqualGUID(parent_app_guid, actual.parent_app_guid()));
  EXPECT_EQ(is_machine_app, actual.is_machine_app());
  EXPECT_STREQ(version, actual.version());
  EXPECT_STREQ(previous_version, actual.previous_version());
  EXPECT_STREQ(language, actual.language());
  EXPECT_EQ(did_run, actual.did_run());
  EXPECT_EQ(days_since_last_active_ping, actual.days_since_last_active_ping());
  EXPECT_EQ(days_since_last_roll_call, actual.days_since_last_roll_call());
  EXPECT_STREQ(ap, actual.ap());
  EXPECT_STREQ(tt_token, actual.tt_token());
  EXPECT_TRUE(::IsEqualGUID(iid, actual.iid()));
  EXPECT_STREQ(brand_code, actual.brand_code());
  EXPECT_STREQ(client_id, actual.client_id());
  EXPECT_STREQ(referral_id, actual.referral_id());
  EXPECT_EQ(install_time_diff_sec, actual.install_time_diff_sec());
  EXPECT_EQ(is_oem_install, actual.is_oem_install());
  EXPECT_EQ(is_eula_accepted, actual.is_eula_accepted());
  EXPECT_STREQ(encoded_installer_data, actual.encoded_installer_data());
  EXPECT_STREQ(install_data_index, actual.install_data_index());
  EXPECT_EQ(is_uninstalled, actual.is_uninstalled());
  EXPECT_EQ(is_update_disabled, actual.is_update_disabled());
}

TEST(AppDataTest, TestInitialized) {
  AppData app_data1;
  ValidateDefaltValues(app_data1);

  AppData app_data2(GUID_NULL, false);
  ValidateDefaltValues(app_data2);
}

TEST(AppDataTest, TestAssignment) {
  AppData app_data;
  FillAppData(&app_data);
  AppData app_data2;
  app_data2 = app_data;

  ValidateExpectedValues(app_data, app_data2);
}

TEST(AppDataTest, TestCopyConstructor) {
  AppData app_data;
  FillAppData(&app_data);
  AppData app_data2(app_data);

  ValidateExpectedValues(app_data, app_data2);
}

}  // namespace omaha
