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
//
// ApplicationManager unit tests

#include <vector>
#include "base/scoped_ptr.h"
#include "omaha/common/utils.h"
#include "omaha/common/time.h"
#include "omaha/common/vistautil.h"
#include "omaha/enterprise/const_group_policy.h"
#include "omaha/goopdate/command_line.h"
#include "omaha/goopdate/config_manager.h"
#include "omaha/goopdate/const_goopdate.h"
#include "omaha/setup/setup_google_update.h"
#include "omaha/testing/unit_test.h"
#include "omaha/worker/application_data.h"
#include "omaha/worker/job.h"

namespace omaha {

namespace {

const TCHAR* const kGuid1 = _T("{21CD0965-0B0E-47cf-B421-2D191C16C0E2}");
const TCHAR* const kGuid2 = _T("{A979ACBD-1F55-4b12-A35F-4DBCA5A7CCB8}");
const TCHAR* const kGuid3 = _T("{661045C5-4429-4140-BC48-8CEA241D1DEF}");
const TCHAR* const kGuid4 = _T("{AAFA1CF9-E94F-42e6-A899-4CD27F37D5A7}");
const TCHAR* const kGuid5 = _T("{3B1A3CCA-0525-4418-93E6-A0DB3398EC9B}");
const TCHAR* const kGuid6 = _T("{F3F2CFD4-5F98-4bf0-ABB0-BEEEA46C62B4}");
const TCHAR* const kGuid7 = _T("{6FD2272F-8583-4bbd-895A-E65F8003FC7B}");

const TCHAR* const kGuid1ClientsKeyPathUser =
    _T("HKCU\\Software\\Google\\Update\\Clients\\")
    _T("{21CD0965-0B0E-47cf-B421-2D191C16C0E2}");
const TCHAR* const kGuid1ClientStateKeyPathUser =
    _T("HKCU\\Software\\Google\\Update\\ClientState\\")
    _T("{21CD0965-0B0E-47cf-B421-2D191C16C0E2}");
const TCHAR* const kGuid1ClientStateKeyPathMachine =
    _T("HKLM\\Software\\Google\\Update\\ClientState\\")
    _T("{21CD0965-0B0E-47cf-B421-2D191C16C0E2}");

}  // namespace

void ValidateExpectedValues(const AppData& expected, const AppData& actual);
void VerifyHklmKeyHasMediumIntegrity(const CString& key_full_name);
void VerifyHklmKeyHasDefaultIntegrity(const CString& key_full_name);

class AppManagerTest : public testing::Test {
 public:
  // Creates the application registration entries based on the passed in data.
  // If passed an application that is uninstalled, the method only creates
  // the registration entries in the client state and no information is written
  // in the clients.
  static void CreateAppRegistryState(const AppData& data) {
    bool is_machine = data.is_machine_app();
    CString client_key_name = AppendRegKeyPath(
        ConfigManager::Instance()->registry_clients(is_machine),
        GuidToString(data.app_guid()));
    CString client_state_key_name = AppendRegKeyPath(
        ConfigManager::Instance()->registry_client_state(is_machine),
        GuidToString(data.app_guid()));

    RegKey client_key;
    if (!data.is_uninstalled()) {
      ASSERT_SUCCEEDED(client_key.Create(client_key_name));

      if (!data.version().IsEmpty()) {
        ASSERT_SUCCEEDED(client_key.SetValue(kRegValueProductVersion,
                                             data.version()));
      }
    }

    RegKey client_state_key;
    ASSERT_SUCCEEDED(client_state_key.Create(client_state_key_name));

    if (!data.previous_version().IsEmpty()) {
      ASSERT_SUCCEEDED(client_state_key.SetValue(kRegValueProductVersion,
                                                 data.previous_version()));
    }

    if (!data.language().IsEmpty()) {
      if (data.is_uninstalled()) {
        ASSERT_SUCCEEDED(client_state_key.SetValue(kRegValueLanguage,
                                                   data.language()));
      } else {
        ASSERT_SUCCEEDED(client_key.SetValue(kRegValueLanguage,
                                             data.language()));
      }
    }

    if (data.did_run() != AppData::ACTIVE_UNKNOWN) {
      CString dr = (data.did_run() == AppData::ACTIVE_NOTRUN) ? _T("0") :
                                                                _T("1");
      ASSERT_SUCCEEDED(client_state_key.SetValue(kRegValueDidRun,
                                                 dr));
    }

    if (!data.ap().IsEmpty()) {
      ASSERT_SUCCEEDED(client_state_key.SetValue(kRegValueAdditionalParams,
                                                 data.ap()));
    }

    if (!data.tt_token().IsEmpty()) {
      ASSERT_SUCCEEDED(client_state_key.SetValue(kRegValueTTToken,
                                                 data.tt_token()));
    }

    if (!::IsEqualGUID(data.iid(), GUID_NULL)) {
      ASSERT_SUCCEEDED(client_state_key.SetValue(kRegValueInstallationId,
                                                 GuidToString(data.iid())));
    }

    if (!data.brand_code().IsEmpty()) {
      ASSERT_SUCCEEDED(client_state_key.SetValue(kRegValueBrandCode,
                                                 data.brand_code()));
    }

    if (!data.client_id().IsEmpty()) {
      ASSERT_SUCCEEDED(client_state_key.SetValue(kRegValueClientId,
                                                 data.client_id()));
    }

    if (!data.referral_id().IsEmpty()) {
      ASSERT_SUCCEEDED(client_state_key.SetValue(kRegValueReferralId,
                                                 data.referral_id()));
    }

    if (!data.referral_id().IsEmpty()) {
      ASSERT_SUCCEEDED(client_state_key.SetValue(kRegValueReferralId,
                                                 data.referral_id()));
    }

    if (data.install_time_diff_sec()) {
      const uint32 now = Time64ToInt32(GetCurrent100NSTime());
      const DWORD install_time = now - data.install_time_diff_sec();
      ASSERT_SUCCEEDED(client_state_key.SetValue(kRegValueInstallTimeSec,
                                                 install_time));
    }

    if (!data.is_eula_accepted()) {
      ASSERT_SUCCEEDED(client_state_key.SetValue(_T("eulaaccepted"),
                                                 static_cast<DWORD>(0)));
    }
  }

  static void PopulateExpectedAppData1(AppData* expected_app) {
    ASSERT_TRUE(expected_app);
    expected_app->set_version(_T("1.1.1.3"));
    expected_app->set_previous_version(_T("1.0.0.0"));
    expected_app->set_language(_T("abc"));
    expected_app->set_ap(_T("Test ap"));
    expected_app->set_tt_token(_T("Test TT Token"));
    expected_app->set_iid(
        StringToGuid(_T("{F723495F-8ACF-4746-8240-643741C797B5}")));
    expected_app->set_brand_code(_T("GOOG"));
    expected_app->set_client_id(_T("someclient"));
    // Do not set referral_id or install_time_diff_sec because these are not
    // expected in most cases.
    // This value must be ACTIVE_RUN for UpdateApplicationStateTest to work.
    expected_app->set_did_run(AppData::ACTIVE_RUN);
  }

  static void PopulateExpectedAppData1InvalidBrand(AppData* expected_app) {
    PopulateExpectedAppData1(expected_app);
    expected_app->set_brand_code(_T("GOOG1122"));
  }

  static void PopulateExpectedAppData2(AppData* expected_app) {
    ASSERT_TRUE(expected_app);
    expected_app->set_version(_T("1.2.1.3"));
    expected_app->set_previous_version(_T("1.1.0.0"));
    expected_app->set_language(_T("de"));
    expected_app->set_ap(_T("beta"));
    expected_app->set_tt_token(_T("beta TT Token"));
    expected_app->set_iid(
        StringToGuid(_T("{431EC961-CFD8-49ea-AB7B-2B99BCA274AD}")));
    expected_app->set_brand_code(_T("GooG"));
    expected_app->set_client_id(_T("anotherclient"));
    expected_app->set_did_run(AppData::ACTIVE_NOTRUN);
  }

  static void PopulateExpectedUninstalledAppData(AppData* expected_app) {
    ASSERT_TRUE(expected_app);
    PopulateExpectedAppData2(expected_app);

    // Make the AppData represent an uninstall.
    expected_app->set_version(_T(""));
    expected_app->set_is_uninstalled(true);
  }

 protected:
  AppManagerTest()
      : hive_override_key_name_(kRegistryHiveOverrideRoot),
        guid1_(StringToGuid(kGuid1)) {}

  virtual void SetUp() {
    RegKey::DeleteKey(hive_override_key_name_);
    OverrideRegistryHives(hive_override_key_name_);
  }

  virtual void TearDown() {
    RestoreRegistryHives();
    RegKey::DeleteKey(hive_override_key_name_);
  }

  void ClearUpdateAvailableStats(const GUID& parent_app_guid,
                                 const GUID& app_guid,
                                 AppManager* app_manager) {
    ASSERT_TRUE(app_manager);
    app_manager->ClearUpdateAvailableStats(parent_app_guid, app_guid);
  }

  bool IsClientStateKeyPresent(const AppData& data) {
    CString client_state_key_name = AppendRegKeyPath(
        ConfigManager::Instance()->registry_client_state(data.is_machine_app()),
        GuidToString(data.app_guid()));

    return RegKey::HasKey(client_state_key_name);
  }

  void InitializeApplicationStateTest(bool is_machine) {
    // Create the test data.
    AppData expected_data(guid1_, is_machine);
    expected_data.set_version(_T("4.5.6.7"));
    expected_data.set_language(_T("de"));
    CreateAppRegistryState(expected_data);

    // Create the job that contains the test data.

    CommandLineAppArgs extra;
    extra.app_guid = guid1_;
    extra.app_name = _T("foo");
    extra.ap = _T("test ap");
    extra.tt_token = _T("test TT Token");

    CommandLineArgs args;
    args.extra.installation_id =
        StringToGuid(_T("{64333341-CA93-490d-9FB7-7FC5728721F4}"));
    args.extra.brand_code = _T("g00g");
    args.extra.client_id = _T("myclient");
    args.extra.referral_id = _T("somereferrer");
    args.extra.language = _T("en");
    args.extra.apps.push_back(extra);

    AppManager app_manager(is_machine);
    ProductDataVector products;
    app_manager.ConvertCommandLineToProductData(args, &products);
    ASSERT_EQ(1, products.size());

    AppData product_app_data = products[0].app_data();

    EXPECT_TRUE(product_app_data.is_eula_accepted());

    // Test the method.
    ASSERT_SUCCEEDED(
        app_manager.InitializeApplicationState(&product_app_data));

    // Validate the results.
    CString client_state_key_name = AppendRegKeyPath(
        ConfigManager::Instance()->registry_client_state(is_machine),
        kGuid1);
    RegKey client_state_key;
    ASSERT_SUCCEEDED(client_state_key.Create(client_state_key_name));

    ValidateClientStateMedium(is_machine, kGuid1);

    // Check version is copied to the client state.
    CString previous_version;
    EXPECT_SUCCEEDED(client_state_key.GetValue(kRegValueProductVersion,
                                               &previous_version));
    EXPECT_STREQ(_T("4.5.6.7"), product_app_data.version());
    EXPECT_STREQ(_T("4.5.6.7"), previous_version);

    // Check language is copied to the client state.
    CString language;
    EXPECT_SUCCEEDED(client_state_key.GetValue(kRegValueLanguage, &language));
    EXPECT_STREQ(_T("de"), product_app_data.language());
    EXPECT_STREQ(_T("de"), language);

    // Check iid is set correctly in ClientState.
    CString iid;
    EXPECT_SUCCEEDED(client_state_key.GetValue(kRegValueInstallationId, &iid));
    EXPECT_STREQ(_T("{64333341-CA93-490D-9FB7-7FC5728721F4}"),
                 GuidToString(product_app_data.iid()));
    EXPECT_STREQ(_T("{64333341-CA93-490D-9FB7-7FC5728721F4}"), iid);

    // Check other values were not written.
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
    EXPECT_FALSE(client_state_key.HasValue(kRegValueTTToken));
  }

  void UpdateApplicationStateTest(bool is_machine, const CString& app_id) {
    const CString client_state_key_name = AppendRegKeyPath(
        ConfigManager::Instance()->registry_client_state(is_machine),
        app_id);

    // Create the test data.
    AppData expected_data(StringToGuid(app_id), is_machine);
    PopulateExpectedAppData1(&expected_data);
    expected_data.set_referral_id(_T("referrer"));
    expected_data.set_is_eula_accepted(false);
    expected_data.set_install_time_diff_sec(141516);
    CreateAppRegistryState(expected_data);

    EXPECT_TRUE(RegKey::HasValue(client_state_key_name, _T("referral")));

    // Call the test method.
    ProductData product_data;
    AppManager app_manager(is_machine);
    EXPECT_SUCCEEDED(app_manager.ReadProductDataFromStore(StringToGuid(app_id),
                                                          &product_data));

    EXPECT_TRUE(product_data.app_data().referral_id().IsEmpty());

    AppData app_data_temp = product_data.app_data();
    EXPECT_SUCCEEDED(app_manager.UpdateApplicationState(&app_data_temp));
    product_data.set_app_data(app_data_temp);

    EXPECT_TRUE(app_data_temp.referral_id().IsEmpty());

    // Need to call again to refresh the values created/copied by
    // UpdateApplicationState().
    EXPECT_SUCCEEDED(app_manager.ReadProductDataFromStore(StringToGuid(app_id),
                                                          &product_data));
    const uint32 now = Time64ToInt32(GetCurrent100NSTime());

    EXPECT_TRUE(product_data.app_data().referral_id().IsEmpty());

    // Validate the results.
    RegKey client_state_key;
    EXPECT_SUCCEEDED(client_state_key.Open(client_state_key_name));

    // Check version and language have been copied to client state.
    CString previous_version;
    EXPECT_SUCCEEDED(client_state_key.GetValue(kRegValueProductVersion,
                                               &previous_version));
    EXPECT_STREQ(expected_data.version(), previous_version);

    CString language;
    EXPECT_SUCCEEDED(client_state_key.GetValue(kRegValueLanguage,
                                               &language));
    EXPECT_STREQ(expected_data.language(), language);

    // Check installation id removed.
    CString iid;
    EXPECT_FAILED(client_state_key.GetValue(kRegValueInstallationId,
                                            &iid));
    EXPECT_TRUE(iid.IsEmpty());

    // Check did_run is cleared.
    CString did_run;
    EXPECT_SUCCEEDED(client_state_key.GetValue(kRegValueDidRun,
                                               &did_run));
    EXPECT_STREQ(_T("0"), did_run);

    // Check that ap, brand_code, and client_id are not changed.
    CString ap;
    EXPECT_SUCCEEDED(client_state_key.GetValue(kRegValueAdditionalParams,
                                               &ap));
    EXPECT_STREQ(expected_data.ap(), ap);

    CString tt_token;
    EXPECT_SUCCEEDED(client_state_key.GetValue(kRegValueTTToken,
                                               &tt_token));
    EXPECT_STREQ(expected_data.tt_token(), tt_token);

    CString brand_code;
    EXPECT_SUCCEEDED(client_state_key.GetValue(kRegValueBrandCode,
                                               &brand_code));
    EXPECT_STREQ(expected_data.brand_code(), brand_code);

    CString client_id;
    EXPECT_SUCCEEDED(client_state_key.GetValue(kRegValueClientId,
                                               &client_id));
    EXPECT_STREQ(expected_data.client_id(), client_id);

    // install_time_diff_sec should be roughly the same as now - installed.
    DWORD install_time(0);
    EXPECT_SUCCEEDED(client_state_key.GetValue(kRegValueInstallTimeSec,
                                               &install_time));
    const DWORD calculated_install_diff = now - install_time;
    EXPECT_GE(calculated_install_diff, expected_data.install_time_diff_sec());
    EXPECT_GE(static_cast<uint32>(500),
              calculated_install_diff - expected_data.install_time_diff_sec());

    DWORD eula_accepted = 0;
    EXPECT_SUCCEEDED(client_state_key.GetValue(_T("eulaaccepted"),
                                               &eula_accepted));
    EXPECT_EQ(0, eula_accepted);
    EXPECT_FALSE(expected_data.is_eula_accepted());
  }

  void WritePreInstallDataTest(const AppData& app_data_in) {
    const bool is_machine = app_data_in.is_machine_app();
    const CString client_key_name = AppendRegKeyPath(
        ConfigManager::Instance()->registry_clients(is_machine), kGuid1);
    const CString client_state_key_name = AppendRegKeyPath(
        ConfigManager::Instance()->registry_client_state(is_machine),
        kGuid1);

    const bool expect_has_client_key = RegKey::HasKey(client_key_name);

    // Populate the test data.
    AppData app_data(app_data_in);
    app_data.set_brand_code(_T("GGLG"));
    app_data.set_client_id(_T("someclient"));
    app_data.set_referral_id(_T("referrer"));
    app_data.set_install_time_diff_sec(657812);   // Not used.
    app_data.set_usage_stats_enable(TRISTATE_TRUE);
    app_data.set_browser_type(BROWSER_FIREFOX);
    app_data.set_ap(_T("test_ap"));
    app_data.set_language(_T("en"));
    app_data.set_version(_T("1.2.3.4"));

    AppManager app_manager(is_machine);
    app_manager.WritePreInstallData(app_data);
    const uint32 now = Time64ToInt32(GetCurrent100NSTime());

    // Validate the results.

    // WritePreInstallData should never write to client_key, so it shouldn't
    // exist if it did not before the method call.
    EXPECT_EQ(expect_has_client_key, RegKey::HasKey(client_key_name));

    // ClientStateKey should exist.
    RegKey client_state_key;
    EXPECT_SUCCEEDED(client_state_key.Open(client_state_key_name));

    ValidateClientStateMedium(is_machine, kGuid1);

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

    DWORD usage_stats_enable = 0;
    EXPECT_SUCCEEDED(client_state_key.GetValue(_T("usagestats"),
                                               &usage_stats_enable));
    EXPECT_EQ(TRISTATE_TRUE, usage_stats_enable);

    DWORD browser_type = 0;
    EXPECT_SUCCEEDED(client_state_key.GetValue(kRegValueBrowser,
                                               &browser_type));
    EXPECT_EQ(BROWSER_FIREFOX, browser_type);

    CString ap;
    EXPECT_SUCCEEDED(client_state_key.GetValue(kRegValueAdditionalParams, &ap));
    EXPECT_STREQ(_T("test_ap"), ap);

    CString lang;
    EXPECT_SUCCEEDED(client_state_key.GetValue(kRegValueLanguage, &lang));
    EXPECT_STREQ(_T("en"), lang);

    // Version should not be written to clientstate by WritePreInstallData().
    EXPECT_FALSE(RegKey::HasValue(client_state_key_name,
                                  kRegValueProductVersion));
  }

  void ValidateClientStateMedium(bool is_machine, const CString& app_guid) {
    const CString client_state_medium_key_name = AppendRegKeyPath(
        ConfigManager::Instance()->machine_registry_client_state_medium(),
        app_guid);
    if (is_machine) {
      RegKey client_state_medium_key;
      EXPECT_SUCCEEDED(
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
    CommandLineArgs args;
    SetupGoogleUpdate setup_google_update(true, &args);
    EXPECT_SUCCEEDED(setup_google_update.CreateClientStateMedium());
  }

  CString hive_override_key_name_;
  const GUID guid1_;
};

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

  CommandLineArgs args;
  args.is_interactive_set = true;  // Not used.
  args.is_machine_set = true;  // Not used.
  args.is_crash_handler_disabled = true;  // Not used.
  args.is_eula_required_set = true;
  args.is_eula_required_set = true;  // Not used.
  args.webplugin_urldomain = _T("http://nothing.google.com");  // Not used.
  args.webplugin_args = _T("blah");  // Not used.
  args.install_source = _T("one_click");
  args.code_red_metainstaller_path = _T("foo.exe");  // Not used.
  args.legacy_manifest_path = _T("bar.exe");  // Not used.
  args.crash_filename = _T("foo.dmp");  // Not used.
  args.extra.installation_id =
      StringToGuid(_T("{F723495F-8ACF-4746-8240-643741C797B5}"));
  args.extra.brand_code = _T("GOOG");
  args.extra.client_id = _T("someclient");
  args.extra.referral_id = _T("referrer1");
  args.extra.browser_type = BROWSER_IE;
  args.extra.language = _T("abc");
  args.extra.usage_stats_enable = TRISTATE_TRUE;
  args.extra.apps.push_back(extra1);
  args.extra.apps.push_back(extra2);

  AppData expected_data1(guid1_, false);
  PopulateExpectedAppData1(&expected_data1);
  expected_data1.set_version(_T(""));  // Clear value.
  expected_data1.set_previous_version(_T(""));  // Clear value.
  expected_data1.set_did_run(AppData::ACTIVE_UNKNOWN);  // Clear value.
  expected_data1.set_display_name(_T("foo"));
  expected_data1.set_browser_type(BROWSER_IE);
  expected_data1.set_install_source(_T("one_click"));
  expected_data1.set_encoded_installer_data(_T("%20foobar"));
  expected_data1.set_install_data_index(_T("foobar"));
  expected_data1.set_usage_stats_enable(TRISTATE_TRUE);
  expected_data1.set_referral_id(_T("referrer1"));
  expected_data1.set_is_eula_accepted(false);

  AppData expected_data2(StringToGuid(kGuid2), false);
  PopulateExpectedAppData2(&expected_data2);
  expected_data2.set_version(_T(""));  // Clear value.
  expected_data2.set_previous_version(_T(""));  // Clear value.
  expected_data2.set_did_run(AppData::ACTIVE_UNKNOWN);  // Clear value.
  expected_data2.set_language(_T("abc"));
  expected_data2.set_display_name(_T("bar"));
  expected_data2.set_browser_type(BROWSER_IE);
  expected_data2.set_install_source(_T("one_click"));
  expected_data2.set_usage_stats_enable(TRISTATE_TRUE);
  // Override unique expected data because the args apply to all apps.
  expected_data2.set_iid(
      StringToGuid(_T("{F723495F-8ACF-4746-8240-643741C797B5}")));
  expected_data2.set_brand_code(_T("GOOG"));
  expected_data2.set_client_id(_T("someclient"));
  expected_data2.set_referral_id(_T("referrer1"));
  expected_data2.set_is_eula_accepted(false);

  ProductDataVector products;
  AppManager app_manager(false);
  app_manager.ConvertCommandLineToProductData(args, &products);

  ASSERT_EQ(2, products.size());
  ASSERT_EQ(0, products[0].num_components());
  ASSERT_EQ(0, products[1].num_components());
  ValidateExpectedValues(expected_data1, products[0].app_data());
  ValidateExpectedValues(expected_data2, products[1].app_data());
}

TEST_F(AppManagerTest, WritePreInstallData_Machine) {
  AppData app_data(guid1_, true);
  ASSERT1(app_data.is_eula_accepted());
  WritePreInstallDataTest(app_data);

  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathMachine,
                                _T("oeminstall")));
  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathMachine,
                                _T("eulaaccepted")));
}

TEST_F(AppManagerTest, WritePreInstallData_Machine_IsOem) {
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

  AppData app_data(guid1_, true);
  ASSERT1(app_data.is_eula_accepted());
  WritePreInstallDataTest(app_data);

  CString oeminstall;
  EXPECT_SUCCEEDED(RegKey::GetValue(kGuid1ClientStateKeyPathMachine,
                                    _T("oeminstall"),
                                    &oeminstall));
  EXPECT_STREQ(_T("1"), oeminstall);

  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathMachine,
                                _T("eulaaccepted")));
}

// Creates the ClientStateMedium key with the appropriate permissions then
// verifies that the created app subkey inherits those.
// The Update key must be created first to avoid applying ClientStateMedium's
// permissions to all its parent keys.
// This keys in this test need to inherit the HKLM privileges, so put the
// override root in HKLM.
TEST_F(AppManagerTest,
       WritePreInstallData_Machine_CheckClientStateMediumPermissions) {
  const TCHAR kRegistryHiveOverrideRootInHklm[] =
      _T("HKLM\\Software\\Google\\Update\\UnitTest\\");
  RestoreRegistryHives();
  hive_override_key_name_ = kRegistryHiveOverrideRootInHklm;
  RegKey::DeleteKey(hive_override_key_name_);
  OverrideRegistryHives(hive_override_key_name_);

  EXPECT_SUCCEEDED(RegKey::CreateKey(
      ConfigManager::Instance()->machine_registry_update()));
  CreateClientStateMediumKey();

  AppData app_data(guid1_, true);
  ASSERT1(app_data.is_eula_accepted());
  WritePreInstallDataTest(app_data);

  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathMachine,
                                _T("oeminstall")));
  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathMachine,
                                _T("eulaaccepted")));

  const CString app_client_state_medium_key_name = AppendRegKeyPath(
      _T("HKLM\\Software\\Google\\Update\\ClientStateMedium\\"),
      kGuid1);
  VerifyHklmKeyHasMediumIntegrity(app_client_state_medium_key_name);
  VerifyHklmKeyHasDefaultIntegrity(
      _T("HKLM\\Software\\Google\\Update\\ClientStateMedium\\"));
}

TEST_F(AppManagerTest,
       WritePreInstallData_Machine_ClearClientStateMediumUsageStats) {
  const CString client_state_key_name =
      AppendRegKeyPath(MACHINE_REG_CLIENT_STATE_MEDIUM, kGuid1);
  EXPECT_SUCCEEDED(RegKey::SetValue(client_state_key_name,
                                    _T("usagestats"),
                                    static_cast<DWORD>(1)));

  AppData app_data(guid1_, true);
  WritePreInstallDataTest(app_data);

  EXPECT_FALSE(RegKey::HasValue(client_state_key_name, _T("usagestats")));
}

// Tests the EULA accepted case too.
TEST_F(AppManagerTest, WritePreInstallData_User) {
  AppData app_data(guid1_, false);
  ASSERT1(app_data.is_eula_accepted());
  WritePreInstallDataTest(app_data);

  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathUser,
                                _T("oeminstall")));
  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathUser,
                                _T("eulaaccepted")));
}

TEST_F(AppManagerTest,
       WritePreInstallData_User_EulaNotAcceptedAppNotRegistered) {
  AppData app_data(guid1_, false);
  app_data.set_is_eula_accepted(false);

  WritePreInstallDataTest(app_data);

  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathUser,
                                _T("oeminstall")));

  DWORD eula_accepted = 99;
  EXPECT_SUCCEEDED(RegKey::GetValue(kGuid1ClientStateKeyPathUser,
                                    _T("eulaaccepted"),
                                    &eula_accepted));
  EXPECT_EQ(0, eula_accepted);
}

TEST_F(AppManagerTest,
       WritePreInstallData_User_EulaNotAcceptedAppAlreadyInstalled) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientsKeyPathUser,
                                    _T("pv"),
                                    _T("1.2.3.4")));

  AppData app_data(guid1_, false);
  app_data.set_is_eula_accepted(false);

  WritePreInstallDataTest(app_data);

  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathUser,
                                _T("oeminstall")));
  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathUser,
                                _T("eulaaccepted")));
}

TEST_F(AppManagerTest,
       WritePreInstallData_User_EulaAcceptedAppAlreadyInstalled) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientsKeyPathUser,
                                    _T("pv"),
                                    _T("1.2.3.4")));

  AppData app_data(guid1_, false);
  app_data.set_is_eula_accepted(true);

  WritePreInstallDataTest(app_data);

  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathUser,
                                _T("oeminstall")));
  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathUser,
                                _T("eulaaccepted")));
}

TEST_F(AppManagerTest,
       WritePreInstallData_User_EulaAcceptedAppAlreadyInstalledAccepted) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientsKeyPathUser,
                                    _T("pv"),
                                    _T("1.2.3.4")));
  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientStateKeyPathUser,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));

  AppData app_data(guid1_, false);
  app_data.set_is_eula_accepted(true);

  WritePreInstallDataTest(app_data);

  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathUser,
                                _T("oeminstall")));
  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathUser,
                                _T("eulaaccepted")));
}

TEST_F(AppManagerTest,
       WritePreInstallData_User_EulaAcceptedAppAlreadyInstalledNotAccepted) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientsKeyPathUser,
                                    _T("pv"),
                                    _T("1.2.3.4")));
  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientStateKeyPathUser,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));

  AppData app_data(guid1_, false);
  app_data.set_is_eula_accepted(true);

  WritePreInstallDataTest(app_data);

  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathUser,
                                _T("oeminstall")));
  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathUser,
                                _T("eulaaccepted")));
}

TEST_F(AppManagerTest, ReadProductDataFromStore_MachineNoAppTest) {
  ProductData product_data;
  AppManager app_manager(true);
  ASSERT_FAILED(app_manager.ReadProductDataFromStore(guid1_, &product_data));
}

TEST_F(AppManagerTest, ReadProductDataFromStore_UserAppTest) {
  AppData expected_data(guid1_, false);
  PopulateExpectedAppData1(&expected_data);
  CreateAppRegistryState(expected_data);

  AppManager app_manager(false);
  ProductData product_data;
  ASSERT_SUCCEEDED(app_manager.ReadProductDataFromStore(guid1_, &product_data));
  ValidateExpectedValues(expected_data, product_data.app_data());
}

TEST_F(AppManagerTest, ReadProductDataFromStore_MachineAppTest) {
  AppData expected_data(guid1_, true);
  PopulateExpectedAppData1(&expected_data);
  CreateAppRegistryState(expected_data);

  AppManager app_manager(true);
  ProductData product_data;
  ASSERT_SUCCEEDED(app_manager.ReadProductDataFromStore(guid1_, &product_data));
  ValidateExpectedValues(expected_data, product_data.app_data());
}

TEST_F(AppManagerTest, ReadProductDataFromStore_UserAppTest_EulaNotAccepted) {
  AppData expected_data(guid1_, false);
  PopulateExpectedAppData1(&expected_data);
  CreateAppRegistryState(expected_data);

  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientStateKeyPathUser,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  expected_data.set_is_eula_accepted(false);

  AppManager app_manager(false);
  ProductData product_data;
  ASSERT_SUCCEEDED(app_manager.ReadProductDataFromStore(guid1_, &product_data));
  ValidateExpectedValues(expected_data, product_data.app_data());
}

TEST_F(AppManagerTest, ReadProductDataFromStore_UserAppTest_EulaAccepted) {
  AppData expected_data(guid1_, false);
  PopulateExpectedAppData1(&expected_data);
  CreateAppRegistryState(expected_data);

  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientStateKeyPathUser,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  expected_data.set_is_eula_accepted(true);

  AppManager app_manager(false);
  ProductData product_data;
  ASSERT_SUCCEEDED(app_manager.ReadProductDataFromStore(guid1_, &product_data));
  ValidateExpectedValues(expected_data, product_data.app_data());
}

TEST_F(AppManagerTest,
       ReadProductDataFromStore_MachineAppTest_EulaNotAccepted) {
  AppData expected_data(guid1_, true);
  PopulateExpectedAppData1(&expected_data);
  CreateAppRegistryState(expected_data);

  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientStateKeyPathMachine,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  expected_data.set_is_eula_accepted(false);

  AppManager app_manager(true);
  ProductData product_data;
  ASSERT_SUCCEEDED(app_manager.ReadProductDataFromStore(guid1_, &product_data));
  ValidateExpectedValues(expected_data, product_data.app_data());
}

TEST_F(AppManagerTest,
       ReadProductDataFromStore_MachineAppTest_EulaAccepted) {
  AppData expected_data(guid1_, true);
  PopulateExpectedAppData1(&expected_data);
  CreateAppRegistryState(expected_data);

  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientStateKeyPathMachine,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));
  expected_data.set_is_eula_accepted(true);

  AppManager app_manager(true);
  ProductData product_data;
  ASSERT_SUCCEEDED(app_manager.ReadProductDataFromStore(guid1_, &product_data));
  ValidateExpectedValues(expected_data, product_data.app_data());
}

TEST_F(AppManagerTest, ReadProductDataFromStore_TwoUserAppTest) {
  AppData expected_data1(guid1_, false);
  PopulateExpectedAppData1(&expected_data1);
  CreateAppRegistryState(expected_data1);

  AppData expected_data2(StringToGuid(kGuid2), false);
  PopulateExpectedAppData1(&expected_data2);
  CreateAppRegistryState(expected_data2);

  AppManager app_manager(false);
  ProductData data1;
  ASSERT_SUCCEEDED(app_manager.ReadProductDataFromStore(guid1_, &data1));
  ValidateExpectedValues(expected_data1, data1.app_data());

  ProductData data2;
  ASSERT_SUCCEEDED(app_manager.ReadProductDataFromStore(StringToGuid(kGuid2),
                                                        &data2));
  ValidateExpectedValues(expected_data2, data2.app_data());
}

TEST_F(AppManagerTest, ReadProductDataFromStore_UserAppNoClientStateTest) {
  AppData expected_data(guid1_, false);
  PopulateExpectedAppData1(&expected_data);
  CreateAppRegistryState(expected_data);

  AppManager app_manager(false);
  ProductData data;
  ASSERT_SUCCEEDED(app_manager.ReadProductDataFromStore(guid1_, &data));
  ValidateExpectedValues(expected_data, data.app_data());
}

TEST_F(AppManagerTest, ReadProductDataFromStore_UninstalledUserApp) {
  AppData expected_data(guid1_, false);
  PopulateExpectedUninstalledAppData(&expected_data);
  CreateAppRegistryState(expected_data);

  AppManager app_manager(false);
  ProductData data;
  ASSERT_SUCCEEDED(app_manager.ReadProductDataFromStore(guid1_, &data));
  ValidateExpectedValues(expected_data, data.app_data());
}

TEST_F(AppManagerTest, ReadProductDataFromStore_UninstalledMachineApp) {
  AppData expected_data(guid1_, true);
  PopulateExpectedUninstalledAppData(&expected_data);
  CreateAppRegistryState(expected_data);

  AppManager app_manager(true);
  ProductData data;
  ASSERT_SUCCEEDED(app_manager.ReadProductDataFromStore(guid1_, &data));
  ValidateExpectedValues(expected_data, data.app_data());
}

TEST_F(AppManagerTest,
       ReadProductDataFromStore_UninstalledUserApp_EulaNotAccepted) {
  AppData expected_data(guid1_, false);
  PopulateExpectedUninstalledAppData(&expected_data);
  expected_data.set_is_eula_accepted(false);
  CreateAppRegistryState(expected_data);

  AppManager app_manager(false);
  ProductData data;
  ASSERT_SUCCEEDED(app_manager.ReadProductDataFromStore(guid1_, &data));
  ValidateExpectedValues(expected_data, data.app_data());
}

// Tests the case where Omaha has created the Client State key before running
// the installer. Uses PopulateExpectedUninstalledAppData then clears pv before
// writing the data to the registry. is_uninstalled_ is not set to false until
// after CreateAppRegistryState to prevent Client key from being created.
TEST_F(AppManagerTest,
       ReadProductDataFromStore_UserClientStateExistsWithoutPvOrClientKey) {
  AppData expected_data(guid1_, false);
  PopulateExpectedUninstalledAppData(&expected_data);
  expected_data.set_previous_version(_T(""));
  CreateAppRegistryState(expected_data);

  AppManager app_manager(false);
  ProductData product_data;
  ASSERT_SUCCEEDED(app_manager.ReadProductDataFromStore(guid1_, &product_data));

  expected_data.set_is_uninstalled(false);
  ValidateExpectedValues(expected_data, product_data.app_data());
}

TEST_F(AppManagerTest,
       ReadProductDataFromStore_MachineClientStateExistsWithoutPvOrClientKey) {
  AppData expected_data(guid1_, true);
  PopulateExpectedUninstalledAppData(&expected_data);
  expected_data.set_previous_version(_T(""));
  CreateAppRegistryState(expected_data);

  AppManager app_manager(true);
  ProductData data;
  ASSERT_SUCCEEDED(app_manager.ReadProductDataFromStore(guid1_, &data));
  expected_data.set_is_uninstalled(false);
  ValidateExpectedValues(expected_data, data.app_data());
}

// An empty pv value is the same as a populated one for uninstall checks.
TEST_F(AppManagerTest,
       ReadProductDataFromStore_UserClientStateExistsWithEmptyPvNoClientKey) {
  AppData expected_data(guid1_, false);
  PopulateExpectedUninstalledAppData(&expected_data);
  expected_data.set_previous_version(_T(""));
  CreateAppRegistryState(expected_data);

  // Write the empty pv value.
  CString client_state_key_name = AppendRegKeyPath(
      ConfigManager::Instance()->registry_client_state(false),
      kGuid1);
  ASSERT_SUCCEEDED(RegKey::SetValue(client_state_key_name,
                                    kRegValueProductVersion,
                                    _T("")));

  AppManager app_manager(false);
  ProductData product_data;
  ASSERT_SUCCEEDED(app_manager.ReadProductDataFromStore(guid1_, &product_data));

  expected_data.set_is_uninstalled(true);
  ValidateExpectedValues(expected_data, product_data.app_data());
}

TEST_F(AppManagerTest, InitializeApplicationState_UserTest) {
  InitializeApplicationStateTest(false);
}

TEST_F(AppManagerTest, InitializeApplicationState_MachineTest) {
  InitializeApplicationStateTest(true);
}

TEST_F(AppManagerTest, UpdateApplicationState_UserTest) {
  UpdateApplicationStateTest(false, kGuid1);

  ValidateClientStateMedium(false, kGuid1);
}

TEST_F(AppManagerTest, UpdateApplicationState_MachineTest) {
  UpdateApplicationStateTest(true, kGuid1);

  ValidateClientStateMedium(true, kGuid1);
}

// Should not create ClientStateMedium key.
TEST_F(AppManagerTest, UpdateApplicationState_MachineTest_Omaha) {
  UpdateApplicationStateTest(true, kGoogleUpdateAppId);

  const CString client_state_medium_key_name = AppendRegKeyPath(
    ConfigManager::Instance()->machine_registry_client_state_medium(),
    kGoogleUpdateAppId);
  EXPECT_FALSE(RegKey::HasKey(client_state_medium_key_name));
}

TEST_F(AppManagerTest, UpdateUpdateAvailableStats_NoExistingStats) {
  const time64 before_time_in_100ns(GetCurrent100NSTime());

  AppManager app_manager(false);
  app_manager.UpdateUpdateAvailableStats(GUID_NULL, guid1_);

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

TEST_F(AppManagerTest, UpdateUpdateAvailableStats_WithExistingStats) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientStateKeyPathUser,
                                    _T("UpdateAvailableCount"),
                                    static_cast<DWORD>(123456)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientStateKeyPathUser,
                                    _T("UpdateAvailableSince"),
                                    static_cast<DWORD64>(9876543210)));

  AppManager app_manager(false);
  app_manager.UpdateUpdateAvailableStats(GUID_NULL, guid1_);

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

TEST_F(AppManagerTest, ClearUpdateAvailableStats_KeyNotPresent) {
  AppManager app_manager(false);
  ClearUpdateAvailableStats(GUID_NULL, guid1_, &app_manager);
}

TEST_F(AppManagerTest, ClearUpdateAvailableStats_DataPresent) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientStateKeyPathUser,
                                    _T("UpdateAvailableCount"),
                                    static_cast<DWORD>(123456)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientStateKeyPathUser,
                                    _T("UpdateAvailableSince"),
                                    static_cast<DWORD64>(9876543210)));

  AppManager app_manager(false);
  ClearUpdateAvailableStats(GUID_NULL, guid1_, &app_manager);

  EXPECT_TRUE(RegKey::HasKey(kGuid1ClientStateKeyPathUser));
  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathUser,
                                _T("UpdateAvailableCount")));
  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathUser,
                                _T("UpdateAvailableSince")));
}

TEST_F(AppManagerTest, ReadUpdateAvailableStats_DataNotPresent) {
  RegKey::CreateKey(kGuid1ClientStateKeyPathUser);

  DWORD update_responses(1);
  DWORD64 time_since_first_response_ms(1);
  AppManager app_manager(false);
  app_manager.ReadUpdateAvailableStats(GUID_NULL,
                                       guid1_,
                                       &update_responses,
                                       &time_since_first_response_ms);

  EXPECT_EQ(0, update_responses);
  EXPECT_EQ(0, time_since_first_response_ms);
}

TEST_F(AppManagerTest, ReadUpdateAvailableStats_DataPresent) {
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
  AppManager app_manager(false);
  app_manager.ReadUpdateAvailableStats(GUID_NULL,
                                       guid1_,
                                       &update_responses,
                                       &time_since_first_response_ms);

  EXPECT_EQ(123456, update_responses);
  EXPECT_LE(2, time_since_first_response_ms);
  EXPECT_GT(10 * kMsPerSec, time_since_first_response_ms);
}

// TODO(omaha): Add *UpdateAvailableStats tests with components when
// component design is finalized and implemented

TEST_F(AppManagerTest, RecordSuccessfulInstall_Install_Online) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientStateKeyPathUser,
                                    _T("UpdateAvailableCount"),
                                    static_cast<DWORD>(123456)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientStateKeyPathUser,
                                    _T("UpdateAvailableSince"),
                                    static_cast<DWORD64>(9876543210)));

  AppManager app_manager(false);
  app_manager.RecordSuccessfulInstall(GUID_NULL, guid1_, false, false);
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

TEST_F(AppManagerTest, RecordSuccessfulInstall_Install_Offline) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientStateKeyPathUser,
                                    _T("UpdateAvailableCount"),
                                    static_cast<DWORD>(123456)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientStateKeyPathUser,
                                    _T("UpdateAvailableSince"),
                                    static_cast<DWORD64>(9876543210)));

  AppManager app_manager(false);
  app_manager.RecordSuccessfulInstall(GUID_NULL, guid1_, false, true);

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

TEST_F(AppManagerTest, RecordSuccessfulInstall_Update_ExistingTimes) {
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

  AppManager app_manager(false);
  app_manager.RecordSuccessfulInstall(GUID_NULL, guid1_, true, false);
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

TEST_F(AppManagerTest, RecordSuccessfulInstall_Update_StateKeyDoesNotExist) {
  AppManager app_manager(false);
  app_manager.RecordSuccessfulInstall(GUID_NULL, guid1_, true, false);
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

TEST_F(AppManagerTest, RecordSuccessfulUpdateCheck_ExistingTime) {
  const DWORD kExistingUpdateValue = 0x12345678;
  EXPECT_SUCCEEDED(RegKey::SetValue(kGuid1ClientStateKeyPathUser,
                                    kRegValueLastSuccessfulCheckSec,
                                    kExistingUpdateValue));

  AppManager app_manager(false);
  app_manager.RecordSuccessfulUpdateCheck(GUID_NULL, guid1_);
  const uint32 now = Time64ToInt32(GetCurrent100NSTime());

  const uint32 last_check_sec = GetDwordValue(kGuid1ClientStateKeyPathUser,
                                              kRegValueLastSuccessfulCheckSec);
  EXPECT_NE(kExistingUpdateValue, last_check_sec);
  EXPECT_GE(now, last_check_sec);
  EXPECT_GE(static_cast<uint32>(200), now - last_check_sec);

  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathUser,
                                kRegValueLastUpdateTimeSec));
}

TEST_F(AppManagerTest, RecordSuccessfulUpdateCheck_StateKeyDoesNotExist) {
  AppManager app_manager(false);
  app_manager.RecordSuccessfulUpdateCheck(GUID_NULL, guid1_);
  const uint32 now = Time64ToInt32(GetCurrent100NSTime());

  const uint32 last_check_sec = GetDwordValue(kGuid1ClientStateKeyPathUser,
                                              kRegValueLastSuccessfulCheckSec);
  EXPECT_GE(now, last_check_sec);
  EXPECT_GE(static_cast<uint32>(200), now - last_check_sec);

  EXPECT_FALSE(RegKey::HasValue(kGuid1ClientStateKeyPathUser,
                                kRegValueLastUpdateTimeSec));
}

TEST_F(AppManagerTest, RemoveClientState_Uninstalled) {
  AppData expected_data(guid1_, true);
  PopulateExpectedUninstalledAppData(&expected_data);
  CreateAppRegistryState(expected_data);

  AppManager app_manager(true);
  ProductData data;
  ASSERT_SUCCEEDED(app_manager.ReadProductDataFromStore(guid1_, &data));
  ASSERT_SUCCEEDED(app_manager.RemoveClientState(data.app_data()));
  ASSERT_FALSE(IsClientStateKeyPresent(expected_data));
}

class AppManagerTest2 : public testing::Test {
 protected:
  AppManagerTest2()
      : hive_override_key_name_(kRegistryHiveOverrideRoot) {}

  virtual void SetUp() {
    RegKey::DeleteKey(hive_override_key_name_);
    OverrideRegistryHives(hive_override_key_name_);
  }

  virtual void TearDown() {
    RestoreRegistryHives();
    RegKey::DeleteKey(hive_override_key_name_);
  }

  CString hive_override_key_name_;
};

// Create 2 registered app and 1 unregistered app and populates the parameters.
// Each is written to the registry.
// Also creates partial Clients and ClientState keys and creates an a registered
// and unregistered app in the opposite registry hive.
void PopulateDataAndRegistryForRegisteredAndUnRegisteredApplicationsTests(
    bool is_machine,
    AppData* expected_data1,
    AppData* expected_data2,
    AppData* expected_data3) {

  expected_data1->set_app_guid(StringToGuid(kGuid1));
  expected_data1->set_is_machine_app(is_machine);
  AppManagerTest::PopulateExpectedAppData1(expected_data1);
  AppManagerTest::CreateAppRegistryState(*expected_data1);

  expected_data2->set_app_guid(StringToGuid(kGuid2));
  expected_data2->set_is_machine_app(is_machine);
  AppManagerTest::PopulateExpectedAppData2(expected_data2);
  AppManagerTest::CreateAppRegistryState(*expected_data2);

  expected_data3->set_app_guid(StringToGuid(kGuid3));
  expected_data3->set_is_machine_app(is_machine);
  AppManagerTest::PopulateExpectedUninstalledAppData(expected_data3);
  AppManagerTest::CreateAppRegistryState(*expected_data3);

  // Add incomplete Clients and ClientState entries.
  ASSERT_HRESULT_SUCCEEDED(RegKey::SetValue(
      AppendRegKeyPath(is_machine ? MACHINE_REG_CLIENTS : USER_REG_CLIENTS,
                       kGuid4),
      _T("name"),
      _T("foo")));

  ASSERT_HRESULT_SUCCEEDED(RegKey::SetValue(
      AppendRegKeyPath(is_machine ? MACHINE_REG_CLIENT_STATE :
                                    USER_REG_CLIENT_STATE,
                       kGuid5),
      kRegValueDidRun,
      _T("1")));

  // Add registered and unregistered app to the opposite registry hive.
  AppData opposite_hive_data1(StringToGuid(kGuid6), !is_machine);
  AppManagerTest::PopulateExpectedAppData2(&opposite_hive_data1);
  AppManagerTest::CreateAppRegistryState(opposite_hive_data1);

  AppData opposite_hive_data2(StringToGuid(kGuid7), !is_machine);
  AppManagerTest::PopulateExpectedUninstalledAppData(&opposite_hive_data2);
  AppManagerTest::CreateAppRegistryState(opposite_hive_data2);
}

TEST_F(AppManagerTest2, GetRegisteredApplications_machine) {
  AppData expected_data1, expected_data2, expected_data3;
  PopulateDataAndRegistryForRegisteredAndUnRegisteredApplicationsTests(
      true,
      &expected_data1,
      &expected_data2,
      &expected_data3);

  AppManager app_manager(true);

  ProductDataVector products;
  ASSERT_HRESULT_SUCCEEDED(app_manager.GetRegisteredProducts(&products));
  ASSERT_EQ(2, products.size());

  ASSERT_TRUE(::IsEqualGUID(products[0].app_data().app_guid(),
              StringToGuid(kGuid1)));
  ValidateExpectedValues(expected_data1, products[0].app_data());

  ASSERT_TRUE(::IsEqualGUID(products[1].app_data().app_guid(),
              StringToGuid(kGuid2)));
  ValidateExpectedValues(expected_data2, products[1].app_data());
}

TEST_F(AppManagerTest2, GetRegisteredApplications_user) {
  AppData expected_data1, expected_data2, expected_data3;
  PopulateDataAndRegistryForRegisteredAndUnRegisteredApplicationsTests(
      false,
      &expected_data1,
      &expected_data2,
      &expected_data3);

  AppManager app_manager(false);


  ProductDataVector products;
  ASSERT_HRESULT_SUCCEEDED(app_manager.GetRegisteredProducts(&products));
  ASSERT_EQ(2, products.size());

  ASSERT_TRUE(::IsEqualGUID(products[0].app_data().app_guid(),
              StringToGuid(kGuid1)));
  ValidateExpectedValues(expected_data1, products[0].app_data());

  ASSERT_TRUE(::IsEqualGUID(products[1].app_data().app_guid(),
              StringToGuid(kGuid2)));
  ValidateExpectedValues(expected_data2, products[1].app_data());
}

TEST_F(AppManagerTest2, GetUnRegisteredApplications_machine) {
  AppData expected_data1, expected_data2, expected_data3;
  PopulateDataAndRegistryForRegisteredAndUnRegisteredApplicationsTests(
      true,
      &expected_data1,
      &expected_data2,
      &expected_data3);

  AppManager app_manager(true);

  ProductDataVector unreg_products;
  ASSERT_HRESULT_SUCCEEDED(
      app_manager.GetUnRegisteredProducts(&unreg_products));
  ASSERT_EQ(1, unreg_products.size());

  ValidateExpectedValues(expected_data3, unreg_products[0].app_data());
}

TEST_F(AppManagerTest2, GetUnRegisteredApplications_user) {
  AppData expected_data1, expected_data2, expected_data3;
  PopulateDataAndRegistryForRegisteredAndUnRegisteredApplicationsTests(
      false,
      &expected_data1,
      &expected_data2,
      &expected_data3);
  AppManager app_manager(false);

  ProductDataVector unreg_products;
  ASSERT_HRESULT_SUCCEEDED(
      app_manager.GetUnRegisteredProducts(&unreg_products));
  ASSERT_EQ(1, unreg_products.size());

  ValidateExpectedValues(expected_data3, unreg_products[0].app_data());
}

TEST_F(AppManagerTest2, UpdateLastChecked) {
  AppManager app_manager(false);

  EXPECT_SUCCEEDED(app_manager.UpdateLastChecked());
  EXPECT_FALSE(app_manager.ShouldCheckForUpdates());

  ConfigManager::Instance()->SetLastCheckedTime(false, 0);
  EXPECT_TRUE(app_manager.ShouldCheckForUpdates());
}

TEST_F(AppManagerTest, ShouldCheckForUpdates_NoLastCheckedPresent) {
  AppManager app_manager(false);
  EXPECT_TRUE(app_manager.ShouldCheckForUpdates());
}

TEST_F(AppManagerTest, ShouldCheckForUpdates_LastCheckedPresent) {
  const uint32 now = Time64ToInt32(GetCurrent100NSTime());
  AppManager app_manager(false);

  ConfigManager::Instance()->SetLastCheckedTime(false, now - 10);
  EXPECT_FALSE(app_manager.ShouldCheckForUpdates());

  ConfigManager::Instance()->SetLastCheckedTime(false,
                                                now - kLastCheckPeriodSec - 1);
  EXPECT_TRUE(app_manager.ShouldCheckForUpdates());
}

TEST_F(AppManagerTest, ShouldCheckForUpdates_LastCheckedInFuture) {
  const uint32 now = Time64ToInt32(GetCurrent100NSTime());
  AppManager app_manager(false);

  // The absolute difference is within the check period.
  ConfigManager::Instance()->SetLastCheckedTime(false, now + 600);
  EXPECT_FALSE(app_manager.ShouldCheckForUpdates());

  // The absolute difference is greater than the check period.
  ConfigManager::Instance()->SetLastCheckedTime(false,
                                                now + kLastCheckPeriodSec + 1);
  EXPECT_TRUE(app_manager.ShouldCheckForUpdates());
}

TEST_F(AppManagerTest, ShouldCheckForUpdates_PeriodZero) {
  EXPECT_SUCCEEDED(
      RegKey::SetValue(kRegKeyGoopdateGroupPolicy,
                       kRegValueAutoUpdateCheckPeriodOverrideMinutes,
                       static_cast<DWORD>(0)));

  AppManager app_manager(false);
  EXPECT_FALSE(app_manager.ShouldCheckForUpdates());
}

TEST_F(AppManagerTest, ShouldCheckForUpdates_PeriodOverride) {
  const DWORD kOverrideMinutes = 10;
  const DWORD kOverrideSeconds = kOverrideMinutes * 60;
  const uint32 now = Time64ToInt32(GetCurrent100NSTime());
  AppManager app_manager(false);

  EXPECT_SUCCEEDED(
      RegKey::SetValue(kRegKeyGoopdateGroupPolicy,
                       kRegValueAutoUpdateCheckPeriodOverrideMinutes,
                       kOverrideMinutes));

  ConfigManager::Instance()->SetLastCheckedTime(false, now - 10);
  EXPECT_FALSE(app_manager.ShouldCheckForUpdates());

  ConfigManager::Instance()->SetLastCheckedTime(false,
                                                now - kOverrideSeconds - 1);
  EXPECT_TRUE(app_manager.ShouldCheckForUpdates());
}

}  // namespace omaha
