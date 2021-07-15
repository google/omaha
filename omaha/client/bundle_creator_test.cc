// Copyright 2011 Google Inc.
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

#include "omaha/client/bundle_creator.h"

#include <memory>

#include "omaha/base/app_util.h"
#include "omaha/base/browser_utils.h"
#include "omaha/base/omaha_version.h"
#include "omaha/base/path.h"
#include "omaha/base/reg_key.h"
#include "omaha/base/system.h"
#include "omaha/base/utils.h"
#include "omaha/base/vistautil.h"
#include "omaha/client/client_utils.h"
#include "omaha/common/command_line.h"
#include "omaha/common/command_line_builder.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/common/update3_utils.h"
#include "omaha/goopdate/goopdate.h"
#include "omaha/goopdate/resource_manager.h"
#include "omaha/goopdate/worker.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace {

const TCHAR kUpdateDevKey[]               = MACHINE_REG_UPDATE;
const TCHAR kMachineGoopdateClientsKey[]  = MACHINE_REG_CLIENTS_GOOPDATE;
const TCHAR kMachineGoopdateStateKey[]    = MACHINE_REG_CLIENT_STATE_GOOPDATE;

}   // namespace

class BundleCreatorTest : public testing::Test {
 protected:
  BundleCreatorTest() {}

  static void SetUpTestCase() {
    // Goopdate instance is required by Worker instance.
    goopdates_.reset(new Goopdate(true));

    EXPECT_SUCCEEDED(ResourceManager::Create(
        true, app_util::GetCurrentModuleDirectory(), _T("en")));

    const CString shell_path = goopdate_utils::BuildGoogleUpdateExePath(true);
    EXPECT_SUCCEEDED(RegKey::SetValue(kUpdateDevKey,
                                      kRegValueInstalledPath,
                                      shell_path));

    EXPECT_SUCCEEDED(RegKey::SetValue(kUpdateDevKey,
                                      kRegValueInstalledVersion,
                                      GetVersionString()));

    EXPECT_SUCCEEDED(RegKey::SetValue(kMachineGoopdateClientsKey,
                                      kRegValueProductVersion,
                                      GetVersionString()));

    EXPECT_SUCCEEDED(RegKey::SetValue(kMachineGoopdateStateKey,
                                      kRegValueProductVersion,
                                      GetVersionString()));

    CopyGoopdateFiles(GetGoogleUpdateMachinePath(), GetVersionString());
  }

  static void TearDownTestCase() {
    ResourceManager::Delete();

    // bundle_creator::Create*() methods indirectly creates Worker instance,
    // delete it here (to avoid interference with other tests).
    Worker::DeleteInstance();

    goopdates_.reset();
  }

  virtual void SetUp() {
    RegisterOrUnregisterGoopdateLocalServer(true);
  }

  virtual void TearDown() {
    RegisterOrUnregisterGoopdateLocalServer(false);
  }

  static void VerifyAppMatchesCommandLineArguments(
      const CommandLineExtraArgs extra_arg,
      const CommandLineAppArgs& app_arg,
      IApp* app) {
    // Verify common data.
    CComBSTR language;
    EXPECT_SUCCEEDED(app->get_language(&language));
    EXPECT_STREQ(extra_arg.language, language);

    CComBSTR iid;
    EXPECT_SUCCEEDED(app->get_iid(&iid));

    GUID installation_id = {0};
    EXPECT_SUCCEEDED(StringToGuidSafe(CString(iid), &installation_id));
    EXPECT_TRUE(extra_arg.installation_id == installation_id);

    CComBSTR brand_code;
    EXPECT_SUCCEEDED(app->get_brandCode(&brand_code));
    EXPECT_STREQ(extra_arg.brand_code, brand_code);

    CComBSTR client_id;
    EXPECT_SUCCEEDED(app->get_clientId(&client_id));
    EXPECT_STREQ(extra_arg.client_id, client_id);

    CComBSTR referral_id;
    EXPECT_SUCCEEDED(app->get_referralId(&referral_id));
    EXPECT_STREQ(extra_arg.referral_id, referral_id);

    UINT browser_type = 0;
    EXPECT_SUCCEEDED(app->get_browserType(&browser_type));
    EXPECT_EQ(extra_arg.browser_type, browser_type);

    // Verify app specific data.
    CComBSTR app_name;
    EXPECT_SUCCEEDED(app->get_displayName(&app_name));
    EXPECT_STREQ(app_arg.app_name, app_name);

    CComBSTR tt_token;
    EXPECT_SUCCEEDED(app->get_ttToken(&tt_token));
    EXPECT_STREQ(app_arg.tt_token, tt_token);

    CComBSTR ap;
    EXPECT_SUCCEEDED(app->get_ap(&ap));
    EXPECT_STREQ(app_arg.ap, ap);
  }

  static void CreateAppRegistryState(const CString& app_id, bool is_machine) {
    CString clients_key_name = AppendRegKeyPath(
        ConfigManager::Instance()->registry_clients(is_machine),
        app_id);
    RegKey client_key;
    ASSERT_SUCCEEDED(client_key.Create(clients_key_name));

    CString current_version(_T("1.0.0.0"));
    ASSERT_SUCCEEDED(client_key.SetValue(kRegValueProductVersion,
                                         current_version));
  }

  static void RemoveAppRegistryState(const CString& app_id, bool is_machine) {
    CString clients_key_name = AppendRegKeyPath(
        ConfigManager::Instance()->registry_clients(is_machine),
        app_id);
    EXPECT_SUCCEEDED(RegKey::DeleteKey(clients_key_name));
  }

  static std::unique_ptr<Goopdate> goopdates_;
};

std::unique_ptr<Goopdate> BundleCreatorTest::goopdates_;

TEST_F(BundleCreatorTest, Create) {
  const CString kDisplayLanguage = _T("en");
  const CString kInstallSource = _T("TestInstallSource");
  const CString kSessionId = _T("{6cb069db-b073-4a40-9983-846a3819876a}");
  const bool is_machine = true;
  const bool is_interactive = false;
  const bool send_pings = true;

  CComPtr<IAppBundle> app_bundle;
  ASSERT_SUCCEEDED(bundle_creator::Create(is_machine,
                                          kDisplayLanguage,
                                          kInstallSource,
                                          kSessionId,
                                          is_interactive,
                                          send_pings,
                                          &app_bundle));
  CComBSTR display_name;
  EXPECT_SUCCEEDED(app_bundle->get_displayName(&display_name));
  EXPECT_STREQ(client_utils::GetUpdateAllAppsBundleName(), display_name);

  CComBSTR install_source;
  EXPECT_SUCCEEDED(app_bundle->get_installSource(&install_source));
  EXPECT_STREQ(kInstallSource, install_source);

  CComBSTR session_id;
  EXPECT_SUCCEEDED(app_bundle->get_sessionId(&session_id));
  EXPECT_STREQ(kSessionId, session_id);

  long num_apps = 0;  // NOLINT(runtime/int)
  EXPECT_SUCCEEDED(app_bundle->get_Count(&num_apps));
  EXPECT_EQ(0, num_apps);

  long priority = INSTALL_PRIORITY_LOW;  // NOLINT(runtime/int)
  EXPECT_SUCCEEDED(app_bundle->get_priority(&priority));
  EXPECT_EQ(is_interactive ? INSTALL_PRIORITY_HIGH : INSTALL_PRIORITY_LOW,
            priority);

  CComBSTR display_language;
  EXPECT_SUCCEEDED(app_bundle->get_displayLanguage(&display_language));
  EXPECT_STREQ(kDisplayLanguage, display_language);
}

TEST_F(BundleCreatorTest, CreateFromCommandLine) {
  const CString kDisplayLanguage = _T("en");
  const CString kTestBundleName = _T("CommandLineTestBundle");
  const GUID kInstallationId = {
    0x9a67c0e6, 0xe6f6, 0x400d,
    {0xa4, 0x30, 0xe3, 0xfe, 0x54, 0xcc, 0x10, 0x43}
  };
  const CString kBrandCode = _T("GOOG");
  const CString kClientId = _T("TestClient");
  const CString kReferralId = _T("TestReferral");
  const BrowserType kBrowserType = BROWSER_CHROME;

  const CString kInstallSource = _T("TestInstallSourceCmdLine");
  const CString kSessionId = _T("{6cb069db-b073-4a40-9983-846a3819876a}");
  const bool is_machine = true;
  const bool is_interactive = true;
  const bool is_eula_accepted = true;
  const bool is_offline = true;
  const bool send_pings = true;
  const CString offline_dir_name = _T("{B851CC84-A5C4-4769-92C1-DC6B0BB368B4}");

  const GUID kApp1Id = {
    0x433bd902, 0x6c0d, 0x4115,
    {0x97, 0xe8, 0x4f, 0xa8, 0x2e, 0x1f, 0x4b, 0x8f}
  };
  const CString kApp1Name = _T("Test App1");
  const CString kApp1AdditionalParameter = _T("App1 AP");
  const CString kApp1Tttoken = _T("T1");

  const GUID kApp2Id = {
    0x83ed8a95, 0xc4e2, 0x4da8,
    {0xbd, 0x0c, 0x00, 0xb9, 0xdf, 0xac, 0x6c, 0x88}
  };
  const CString kApp2Name = _T("Test App2");
  const CString kApp2AdditionalParameter = _T("App2 AP");
  const CString kApp2Tttoken = _T("T2");

  CommandLineAppArgs app1;
  app1.app_guid = kApp1Id;
  app1.app_name = kApp1Name;
  app1.needs_admin = NEEDS_ADMIN_YES;
  app1.ap = kApp1AdditionalParameter;
  app1.tt_token = kApp1Tttoken;

  CommandLineAppArgs app2;
  app2.app_guid = kApp2Id;
  app2.app_name = kApp2Name;
  app2.needs_admin = NEEDS_ADMIN_NO;
  app2.ap = kApp2AdditionalParameter;
  app2.tt_token = kApp2Tttoken;

  CommandLineExtraArgs extra_args;
  extra_args.bundle_name = kTestBundleName;
  extra_args.installation_id = kInstallationId;
  extra_args.brand_code = kBrandCode;
  extra_args.client_id = kClientId;
  extra_args.referral_id = kReferralId;
  extra_args.language = kDisplayLanguage;
  extra_args.browser_type = kBrowserType;
  extra_args.apps.push_back(app1);
  extra_args.apps.push_back(app2);

  CComPtr<IAppBundle> app_bundle;
  ASSERT_SUCCEEDED(bundle_creator::CreateFromCommandLine(
      is_machine,
      is_eula_accepted,
      is_offline,
      offline_dir_name,
      extra_args,
      kInstallSource,
      kSessionId,
      is_interactive,
      send_pings,
      &app_bundle));

  CComBSTR display_name;
  EXPECT_SUCCEEDED(app_bundle->get_displayName(&display_name));
  EXPECT_STREQ(kTestBundleName, display_name);

  CComBSTR install_source;
  EXPECT_SUCCEEDED(app_bundle->get_installSource(&install_source));
  EXPECT_STREQ(kInstallSource, install_source);

  CComBSTR session_id;
  EXPECT_SUCCEEDED(app_bundle->get_sessionId(&session_id));
  EXPECT_STREQ(kSessionId, session_id);

  long priority = INSTALL_PRIORITY_LOW;  // NOLINT(runtime/int)
  EXPECT_SUCCEEDED(app_bundle->get_priority(&priority));
  EXPECT_EQ(INSTALL_PRIORITY_HIGH, priority);

  CComBSTR display_language;
  EXPECT_SUCCEEDED(app_bundle->get_displayLanguage(&display_language));
  EXPECT_STREQ(kDisplayLanguage, display_language);

  long num_apps = 0;  // NOLINT(runtime/int)
  EXPECT_SUCCEEDED(app_bundle->get_Count(&num_apps));
  EXPECT_EQ(2, num_apps);

  for (long i = 0; i < num_apps; ++i) {  // NOLINT(runtime/int)
    CComPtr<IApp> app;
    EXPECT_SUCCEEDED(update3_utils::GetApp(app_bundle, i, &app));

    CComBSTR app_id;
    EXPECT_SUCCEEDED(app->get_appId(&app_id));
    GUID app_guid = {0};
    EXPECT_SUCCEEDED(StringToGuidSafe(CString(app_id), &app_guid));

    if (app_guid == kApp1Id) {
      VerifyAppMatchesCommandLineArguments(extra_args, app1, app);
    } else {
      EXPECT_TRUE(kApp2Id == app_guid);
      VerifyAppMatchesCommandLineArguments(extra_args, app2, app);
    }
  }
}

TEST_F(BundleCreatorTest, CreateForceInstallBundle) {
  const CString kDisplayLanguage = _T("en");
  const CString kInstallSource = _T("TestInstallSourceForceInstallBundle");
  const CString kSessionId = _T("{6cb069db-b073-4a40-9983-846a3819876a}");
  const bool is_machine = true;
  const bool is_interactive = true;
  const bool send_pings = true;

  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE_DEV,
                                    kRegValueIsEnrolledToDomain,
                                    1UL));
  EXPECT_SUCCEEDED(RegKey::CreateKey(kRegKeyGoopdateGroupPolicy));

  #define APP_ID1 _T("{D9F05AEA-BEDA-4f91-B216-BE45DAE330CB}")
  const TCHAR* const kInstallPolicyApp1 = _T("Install") APP_ID1;
  #define APP_ID2 _T("{EF3CACD4-89EB-46b7-B9BF-B16B15F08584}")
  const TCHAR* const kInstallPolicyApp2 = _T("Install") APP_ID2;

  EXPECT_SUCCEEDED(SetPolicy(kInstallPolicyApp1, kPolicyForceInstallMachine));
  EXPECT_SUCCEEDED(SetPolicy(kInstallPolicyApp2, kPolicyForceInstallMachine));

  CComPtr<IAppBundle> app_bundle;
  ASSERT_EQ(S_OK, bundle_creator::CreateForceInstallBundle(is_machine,
                                                           kDisplayLanguage,
                                                           kInstallSource,
                                                           kSessionId,
                                                           is_interactive,
                                                           send_pings,
                                                           &app_bundle));

  CComBSTR display_name;
  EXPECT_SUCCEEDED(app_bundle->get_displayName(&display_name));
  EXPECT_STREQ(client_utils::GetDefaultBundleName(), display_name);

  CComBSTR install_source;
  EXPECT_SUCCEEDED(app_bundle->get_installSource(&install_source));
  EXPECT_STREQ(kInstallSource, install_source);

  CComBSTR session_id;
  EXPECT_SUCCEEDED(app_bundle->get_sessionId(&session_id));
  EXPECT_STREQ(kSessionId, session_id);

  long priority = INSTALL_PRIORITY_LOW;  // NOLINT(runtime/int)
  EXPECT_SUCCEEDED(app_bundle->get_priority(&priority));
  EXPECT_EQ(INSTALL_PRIORITY_HIGH, priority);

  CComBSTR display_language;
  EXPECT_SUCCEEDED(app_bundle->get_displayLanguage(&display_language));
  EXPECT_STREQ(kDisplayLanguage, display_language);

  long num_apps = 0;  // NOLINT(runtime/int)
  EXPECT_SUCCEEDED(app_bundle->get_Count(&num_apps));
  EXPECT_EQ(2, num_apps);

  for (long i = 0; i < num_apps; ++i) {  // NOLINT(runtime/int)
    CComPtr<IApp> app;
    EXPECT_SUCCEEDED(update3_utils::GetApp(app_bundle, i, &app));

    CComBSTR app_id_bstr;
    EXPECT_SUCCEEDED(app->get_appId(&app_id_bstr));
    CString app_id(app_id_bstr);
    EXPECT_TRUE(!app_id.CompareNoCase(APP_ID1) ||
                !app_id.CompareNoCase(APP_ID2));

    CComBSTR app_name;
    EXPECT_SUCCEEDED(app->get_displayName(&app_name));
    EXPECT_STREQ(client_utils::GetDefaultApplicationName(), app_name);
  }

  RegKey::DeleteKey(kRegKeyGoopdateGroupPolicy);
  EXPECT_SUCCEEDED(RegKey::DeleteValue(MACHINE_REG_UPDATE_DEV,
                                       kRegValueIsEnrolledToDomain));
}

TEST_F(BundleCreatorTest, CreateForOnDemand) {
  const CString& kAppId = _T("{5dace97e-9d8f-430b-acc7-ef04708b4725}");
  const CString kInstallSource = _T("TestInstallSourceOnDemand");
  const CString kSessionId = _T("{6cb069db-b073-4a40-9983-846a3819876a}");
  const bool is_machine = true;

  // Create app registry key to make it "installed".
  CreateAppRegistryState(kAppId, is_machine);

  CAccessToken process_token;
  if (is_machine) {
    process_token.GetEffectiveToken(TOKEN_ALL_ACCESS);
  }

  CComPtr<IAppBundle> app_bundle;
  HRESULT hr = bundle_creator::CreateForOnDemand(is_machine,
                                                 kAppId,
                                                 kInstallSource,
                                                 kSessionId,
                                                 process_token.GetHandle(),
                                                 process_token.GetHandle(),
                                                 &app_bundle);
  RemoveAppRegistryState(kAppId, is_machine);
  ASSERT_SUCCEEDED(hr);

  CComBSTR install_source;
  EXPECT_SUCCEEDED(app_bundle->get_installSource(&install_source));
  EXPECT_STREQ(kInstallSource, install_source);

  CComBSTR session_id;
  EXPECT_SUCCEEDED(app_bundle->get_sessionId(&session_id));
  EXPECT_STREQ(kSessionId, session_id);

  long num_apps = 0;  // NOLINT(runtime/int)
  EXPECT_SUCCEEDED(app_bundle->get_Count(&num_apps));
  EXPECT_EQ(1, num_apps);

  long priority = INSTALL_PRIORITY_LOW;  // NOLINT(runtime/int)
  EXPECT_SUCCEEDED(app_bundle->get_priority(&priority));
  EXPECT_EQ(INSTALL_PRIORITY_HIGH, priority);
}

TEST_F(BundleCreatorTest, CreateForOnDemand_NonExistApp) {
  const CString& kAppId = _T("{52e24bf9-d7d0-4b6e-b12d-9cef51fa45f2}");
  const CString kInstallSource = _T("TestInstallSourceOnDemand");
  const CString kSessionId = _T("{6cb069db-b073-4a40-9983-846a3819876a}");
  const bool is_machine = true;

  CAccessToken process_token;
  if (is_machine) {
    process_token.GetEffectiveToken(TOKEN_ALL_ACCESS);
  }

  CComPtr<IAppBundle> app_bundle;
  EXPECT_FAILED(bundle_creator::CreateForOnDemand(
      is_machine,
      kAppId,
      kInstallSource,
      kSessionId,
      process_token.GetHandle(),
      process_token.GetHandle(),
      &app_bundle));
}

}  // namespace omaha
