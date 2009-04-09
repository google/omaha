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
// Goopdate Xml Parser unit tests.

#include <windows.h>
#include <utility>
#include <vector>
#include "base/scoped_ptr.h"
#include "omaha/common/app_util.h"
#include "omaha/common/error.h"
#include "omaha/common/file.h"
#include "omaha/common/path.h"
#include "omaha/common/utils.h"
#include "omaha/common/reg_key.h"
#include "omaha/common/xml_utils.h"
#include "omaha/goopdate/goopdate_xml_parser.h"
#include "omaha/goopdate/update_response.h"
#include "omaha/goopdate/resources/goopdateres/goopdate.grh"
#include "omaha/testing/resource.h"
#include "omaha/testing/unit_test.h"

namespace {

const int kSeedManifestFileCount = 1;
const int kSeedManifestResponseCount = 6;

const TCHAR* const kPolicyKey =
    _T("HKLM\\Software\\Policies\\Google\\Update\\");

}  // namespace

namespace omaha {

const int kExpectedRequestLength = 2048;

// Do NOT override the registry as this causes the XML Parser to fail on Vista.
// Saves and restores registry values to prevent reading developer's update
// check period override from impacting tests.
class GoopdateXmlParserTest : public testing::Test {
 protected:
  GoopdateXmlParserTest()
      : is_updatedev_check_period_override_present_(false),
        updatedev_check_period_override_(0),
        is_policy_check_period_override_present_(false),
        policy_check_period_override_(0) {
  }

  virtual void SetUp() {
    is_updatedev_check_period_override_present_ =
        SUCCEEDED(RegKey::GetValue(MACHINE_REG_UPDATE_DEV,
                                   kRegValueLastCheckPeriodSec,
                                   &updatedev_check_period_override_));
    RegKey::DeleteValue(MACHINE_REG_UPDATE_DEV, kRegValueLastCheckPeriodSec);
    is_policy_check_period_override_present_ =
        SUCCEEDED(RegKey::GetValue(kPolicyKey,
                                   _T("AutoUpdateCheckPeriodMinutes"),
                                   &policy_check_period_override_));
    RegKey::DeleteValue(kPolicyKey, _T("AutoUpdateCheckPeriodMinutes"));
  }

  virtual void TearDown() {
    if (is_updatedev_check_period_override_present_) {
      EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE_DEV,
                                        kRegValueLastCheckPeriodSec,
                                        updatedev_check_period_override_));
    }
    if (is_policy_check_period_override_present_) {
      EXPECT_SUCCEEDED(RegKey::SetValue(kPolicyKey,
                                        _T("AutoUpdateCheckPeriodMinutes"),
                                        policy_check_period_override_));
    }
  }

  void CreateBaseAppData(bool is_machine, AppData* app_data) {
    ASSERT_TRUE(app_data != NULL);

    AppData data(
        StringToGuid(_T("{83F0F399-FA78-4a94-A45E-253D4F42A4C5}")),
        is_machine);
    data.set_version(_T("1.0.101.0"));
    data.set_language(_T("en_us"));
    *app_data = data;
  }

  SuccessfulInstallAction ConvertStringToSuccessAction(const CString& text) {
    return GoopdateXmlParser::ConvertStringToSuccessAction(text);
  }

  HRESULT VerifyProtocolCompatibility(const CString& actual_version,
                                      const CString& expected_version) {
    return GoopdateXmlParser::VerifyProtocolCompatibility(actual_version,
                                                          expected_version);
  }

  static HRESULT ReadStringValue(IXMLDOMNode* node, CString* value) {
    return GoopdateXmlParser::ReadStringValue(node, value);
  }

  bool is_updatedev_check_period_override_present_;
  DWORD updatedev_check_period_override_;
  bool is_policy_check_period_override_present_;
  DWORD policy_check_period_override_;

  static const int kServerManifestResponseCount = 5;
  static const int kServerManifestComponentsResponseCount = 4;
};

TEST_F(GoopdateXmlParserTest, GenerateRequest_Test1) {
  TCHAR expected_value[kExpectedRequestLength] = {0};
  EXPECT_TRUE(::LoadString(NULL, IDS_EXPECTED_UPDATE_REQUEST1, expected_value,
                           kExpectedRequestLength) != 0);

  Request req(false);
  req.set_machine_id(_T("{874E4D29-8671-40C8-859F-4DECA481CF42}"));
  req.set_user_id(_T("{8CD4D4C7-D42E-49B7-9E1A-DDDC8F8F77A8}"));
  req.set_request_id(_T("{8CD4D4C7-D42E-49B7-9E1A-DDDC8F8F77A8}"));
  req.set_os_version(_T("5.1"));
  req.set_os_service_pack(_T(""));
  req.set_version(_T("0.0.0.0"));
  req.set_test_source(_T("dev"));

  AppData app_data1;
  CreateBaseAppData(req.is_machine(), &app_data1);
  app_data1.set_did_run(AppData::ACTIVE_NOTRUN);
  app_data1.set_ap(_T("dev"));
  app_data1.set_iid(StringToGuid(_T("{A972BB39-CCA3-4f25-9737-3308F5FA19B5}")));
  app_data1.set_client_id(_T("_one_client"));
  app_data1.set_install_source(_T("oneclick"));
  app_data1.set_brand_code(_T("GGLG"));
  app_data1.set_install_source(_T("oneclick"));

  AppRequestData app_request_data1(app_data1);
  AppRequest app_request1(app_request_data1);
  req.AddAppRequest(app_request1);

  AppData app_data2;
  CreateBaseAppData(req.is_machine(), &app_data2);
  app_data2.set_did_run(AppData::ACTIVE_NOTRUN);
  app_data2.set_iid(StringToGuid(_T("{E9EF60A1-B254-4898-A1B3-6C9B60FAC94A}")));
  app_data2.set_client_id(_T("_another_client"));
  app_data2.set_brand_code(_T("GooG"));
  AppRequestData app_request_data2(app_data2);
  AppRequest app_request2(app_request_data2);
  req.AddAppRequest(app_request2);

  CString request_string;
  ASSERT_SUCCEEDED(
      GoopdateXmlParser::GenerateRequest(req, true, &request_string));

  ASSERT_STREQ(expected_value, request_string);
}

// Also tests the presence of periodoverridesec.
TEST_F(GoopdateXmlParserTest, GenerateRequest_TestTTToken) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kPolicyKey,
                                    _T("AutoUpdateCheckPeriodMinutes"),
                                    static_cast<DWORD>(123456)));

  TCHAR expected_value[kExpectedRequestLength] = {0};
  EXPECT_TRUE(::LoadString(NULL, IDS_EXPECTED_UPDATE_REQUEST_TTTOKEN,
                           expected_value, kExpectedRequestLength) != 0);

  Request req(false);
  req.set_machine_id(_T("{874E4D29-8671-40C8-859F-4DECA481CF42}"));
  req.set_user_id(_T("{8CD4D4C7-D42E-49B7-9E1A-DDDC8F8F77A8}"));
  req.set_request_id(_T("{8CD4D4C7-D42E-49B7-9E1A-DDDC8F8F77A8}"));
  req.set_os_version(_T("5.1"));
  req.set_os_service_pack(_T(""));
  req.set_version(_T("0.0.0.0"));
  req.set_test_source(_T("dev"));

  AppData app_data1;
  CreateBaseAppData(req.is_machine(), &app_data1);
  app_data1.set_did_run(AppData::ACTIVE_NOTRUN);
  app_data1.set_ap(_T("dev"));
  app_data1.set_tt_token(_T("foobar"));
  app_data1.set_iid(StringToGuid(_T("{A972BB39-CCA3-4f25-9737-3308F5FA19B5}")));
  app_data1.set_client_id(_T("_one_client"));
  app_data1.set_install_source(_T("oneclick"));
  app_data1.set_brand_code(_T("GGLG"));
  app_data1.set_install_source(_T("oneclick"));

  AppRequestData app_request_data1(app_data1);
  AppRequest app_request1(app_request_data1);
  req.AddAppRequest(app_request1);

  AppData app_data2;
  CreateBaseAppData(req.is_machine(), &app_data2);
  app_data2.set_did_run(AppData::ACTIVE_NOTRUN);
  app_data2.set_iid(StringToGuid(_T("{E9EF60A1-B254-4898-A1B3-6C9B60FAC94A}")));
  app_data2.set_client_id(_T("_another_client"));
  app_data2.set_brand_code(_T("GooG"));
  AppRequestData app_request_data2(app_data2);
  AppRequest app_request2(app_request_data2);
  req.AddAppRequest(app_request2);

  CString request_string;
  ASSERT_SUCCEEDED(
      GoopdateXmlParser::GenerateRequest(req, true, &request_string));

  ASSERT_STREQ(expected_value, request_string);
}

TEST_F(GoopdateXmlParserTest, GenerateRequest_TestUpdateDisabled) {
  TCHAR expected_value[kExpectedRequestLength] = {0};
  EXPECT_TRUE(::LoadString(NULL, IDS_EXPECTED_UPDATE_REQUEST_UPDATE_DISABLED,
                           expected_value, kExpectedRequestLength) != 0);

  Request req(false);
  req.set_machine_id(_T("{874E4D29-8671-40C8-859F-4DECA481CF42}"));
  req.set_user_id(_T("{8CD4D4C7-D42E-49B7-9E1A-DDDC8F8F77A8}"));
  req.set_request_id(_T("{8CD4D4C7-D42E-49B7-9E1A-DDDC8F8F77A8}"));
  req.set_os_version(_T("5.1"));
  req.set_os_service_pack(_T(""));
  req.set_version(_T("0.0.0.0"));
  req.set_test_source(_T("dev"));

  AppData app_data1;
  CreateBaseAppData(req.is_machine(), &app_data1);
  app_data1.set_did_run(AppData::ACTIVE_NOTRUN);
  app_data1.set_ap(_T("dev"));
  app_data1.set_tt_token(_T("foobar"));
  app_data1.set_iid(StringToGuid(_T("{A972BB39-CCA3-4f25-9737-3308F5FA19B5}")));
  app_data1.set_client_id(_T("_one_client"));
  app_data1.set_install_source(_T("oneclick"));
  app_data1.set_brand_code(_T("GGLG"));
  app_data1.set_install_source(_T("oneclick"));
  app_data1.set_is_update_disabled(true);

  AppRequestData app_request_data1(app_data1);
  AppRequest app_request1(app_request_data1);
  req.AddAppRequest(app_request1);

  AppData app_data2;
  CreateBaseAppData(req.is_machine(), &app_data2);
  app_data2.set_did_run(AppData::ACTIVE_NOTRUN);
  app_data2.set_iid(StringToGuid(_T("{E9EF60A1-B254-4898-A1B3-6C9B60FAC94A}")));
  app_data2.set_client_id(_T("_another_client"));
  app_data2.set_brand_code(_T("GooG"));
  AppRequestData app_request_data2(app_data2);
  AppRequest app_request2(app_request_data2);
  req.AddAppRequest(app_request2);

  CString request_string;
  ASSERT_SUCCEEDED(
      GoopdateXmlParser::GenerateRequest(req, true, &request_string));

  ASSERT_STREQ(expected_value, request_string);
}

TEST_F(GoopdateXmlParserTest, GenerateRequest_Test2) {
  TCHAR expected_value[kExpectedRequestLength] = {0};
  EXPECT_TRUE(::LoadString(NULL, IDS_EXPECTED_UPDATE_REQUEST2, expected_value,
                           kExpectedRequestLength) != 0);

  Request req(true);
  req.set_machine_id(_T("{874E4D29-8671-40C8-859F-4DECA481CF42}"));
  req.set_user_id(_T("{8CD4D4C7-D42E-49B7-9E1A-DDDC8F8F77A8}"));
  req.set_request_id(_T("{8CD4D4C7-D42E-49B7-9E1A-DDDC8F8F77A8}"));
  req.set_os_version(_T("5.1"));
  req.set_os_service_pack(_T("Service Pack 2"));
  req.set_version(_T("8.9.10.11"));
  req.set_test_source(_T("qa"));

  AppData app_data1;
  CreateBaseAppData(req.is_machine(), &app_data1);
  AppRequestData app_request_data1(app_data1);
  PingEvent ping_event(PingEvent::EVENT_INSTALL_COMPLETE,
                       PingEvent::EVENT_RESULT_ERROR,
                       1234,
                       E_FAIL,
                       _T("Install error"));
  app_request_data1.AddPingEvent(ping_event);
  AppRequest app_request1(app_request_data1);
  req.AddAppRequest(app_request1);

  AppData app_data2;
  CreateBaseAppData(req.is_machine(), &app_data2);
  app_data2.set_ap(_T("stable"));
  app_data2.set_tt_token(_T("foobar"));
  app_data2.set_iid(StringToGuid(_T("{E9EF60A1-B254-4898-A1B3-6C9B60FAC94A}")));
  app_data2.set_client_id(_T("_some_client"));
  app_data2.set_brand_code(_T("GooG"));
  AppRequestData app_request_data2(app_data2);
  AppRequest app_request2(app_request_data2);
  req.AddAppRequest(app_request2);

  CString request_string;
  ASSERT_SUCCEEDED(GoopdateXmlParser::GenerateRequest(req,
                                                      false,
                                                      &request_string));

  ASSERT_STREQ(expected_value, request_string);
}

// Also tests the presence of periodoverridesec in non-update checks and integer
// overflow of the registry value.
TEST_F(GoopdateXmlParserTest, GenerateRequest_Test3_Components) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kPolicyKey,
                                    _T("AutoUpdateCheckPeriodMinutes"),
                                    static_cast<DWORD>(UINT_MAX / 60)));

  TCHAR expected_value[kExpectedRequestLength] = {0};
  EXPECT_TRUE(::LoadString(NULL, IDS_EXPECTED_UPDATE_REQUEST3, expected_value,
                           kExpectedRequestLength) != 0);

  Request req(true);
  req.set_machine_id(_T("{874E4D29-8671-40C8-859F-4DECA481CF42}"));
  req.set_user_id(_T("{8CD4D4C7-D42E-49B7-9E1A-DDDC8F8F77A8}"));
  req.set_request_id(_T("{8CD4D4C7-D42E-49B7-9E1A-DDDC8F8F77A8}"));
  req.set_os_version(_T("5.1"));
  req.set_os_service_pack(_T("Service Pack 2"));
  req.set_version(_T("8.9.10.11"));
  req.set_test_source(_T("qa"));

  AppData app_data1;
  CreateBaseAppData(req.is_machine(), &app_data1);

  const TCHAR* kComponent1Guid = _T("{AC001D35-5F30-473A-9D7B-8FD3877AC28E}");
  const TCHAR* kComponent1Version = _T("1.0.3.1");

  const TCHAR* kComponent2Guid = _T("{F4E490BE-83BB-4D5A-8386-263DE047255E}");
  const TCHAR* kComponent2Version = _T("1.1.4.2");

  AppRequest app_request1;
  AppRequestData app_request_data1(app_data1);

  AppData component1;
  component1.set_app_guid(StringToGuid(kComponent1Guid));
  component1.set_is_machine_app(req.is_machine());
  component1.set_version(kComponent1Version);
  AppRequestData app_request_data_component1(component1);
  app_request1.AddComponentRequest(app_request_data_component1);

  AppData component2;
  component2.set_app_guid(StringToGuid(kComponent2Guid));
  component2.set_is_machine_app(req.is_machine());
  component2.set_version(kComponent2Version);
  AppRequestData app_request_data_component2(component2);
  app_request1.AddComponentRequest(app_request_data_component2);

  PingEvent ping_event(PingEvent::EVENT_INSTALL_COMPLETE,
                       PingEvent::EVENT_RESULT_ERROR,
                       1234,
                       E_FAIL,
                       _T("Install error"));
  app_request_data1.AddPingEvent(ping_event);

  app_request1.set_request_data(app_request_data1);
  req.AddAppRequest(app_request1);

  AppData app_data2;
  CreateBaseAppData(req.is_machine(), &app_data2);
  app_data2.set_ap(_T("stable"));
  app_data2.set_iid(StringToGuid(_T("{E9EF60A1-B254-4898-A1B3-6C9B60FAC94A}")));
  app_data2.set_client_id(_T("_some_client"));
  app_data2.set_brand_code(_T("GooG"));
  AppRequestData app_request_data2(app_data2);
  AppRequest app_request2(app_request_data2);
  req.AddAppRequest(app_request2);

  CString request_string;
  bool encrypt = false;
  ASSERT_SUCCEEDED(GoopdateXmlParser::GenerateRequest(req,
                                                      false,
                                                      &request_string));

  ASSERT_FALSE(encrypt);
  ASSERT_STREQ(expected_value, request_string);
}

TEST_F(GoopdateXmlParserTest, GenerateRequest_TestInstallDataIndex) {
  TCHAR expected_value[kExpectedRequestLength] = {0};
  EXPECT_TRUE(::LoadString(NULL, IDS_EXPECTED_UPDATE_REQUEST4, expected_value,
                           kExpectedRequestLength) != 0);

  Request req(false);
  req.set_machine_id(_T("{874E4D29-8671-40C8-859F-4DECA481CF42}"));
  req.set_user_id(_T("{8CD4D4C7-D42E-49B7-9E1A-DDDC8F8F77A8}"));
  req.set_request_id(_T("{8CD4D4C7-D42E-49B7-9E1A-DDDC8F8F77A8}"));
  req.set_os_version(_T("5.1"));
  req.set_os_service_pack(_T(""));
  req.set_version(_T("0.0.0.0"));
  req.set_test_source(_T("dev"));

  AppData app_data1;
  CreateBaseAppData(req.is_machine(), &app_data1);
  app_data1.set_did_run(AppData::ACTIVE_NOTRUN);
  app_data1.set_ap(_T("dev"));
  app_data1.set_iid(StringToGuid(_T("{A972BB39-CCA3-4f25-9737-3308F5FA19B5}")));
  app_data1.set_brand_code(_T("GGLG"));
  app_data1.set_client_id(_T("_one_client"));
  app_data1.set_install_source(_T("oneclick"));
  app_data1.set_install_data_index(_T("foobar"));

  AppRequestData app_request_data1(app_data1);
  AppRequest app_request1(app_request_data1);
  req.AddAppRequest(app_request1);

  AppData app_data2;
  CreateBaseAppData(req.is_machine(), &app_data2);
  app_data2.set_did_run(AppData::ACTIVE_NOTRUN);
  app_data2.set_iid(StringToGuid(_T("{E9EF60A1-B254-4898-A1B3-6C9B60FAC94A}")));
  app_data2.set_client_id(_T("_another_client"));
  app_data2.set_brand_code(_T("GooG"));
  AppRequestData app_request_data2(app_data2);
  AppRequest app_request2(app_request_data2);
  req.AddAppRequest(app_request2);

  CString request_string;
  ASSERT_SUCCEEDED(
      GoopdateXmlParser::GenerateRequest(req, true, &request_string));

  ASSERT_STREQ(expected_value, request_string);
}

TEST_F(GoopdateXmlParserTest, ReadStringValue) {
  const TCHAR* verbose_log = _T("\n  {\n    \"distribution\": {\n      ")
                             _T("\"verbose_logging\": true\n    }\n  }\n  ");

  CString file_name(ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                                    _T("server_manifest.xml")));
  CComPtr<IXMLDOMDocument> document;
  ASSERT_SUCCEEDED(LoadXMLFromFile(file_name, false, &document));

  CComBSTR data_element_name(_T("data"));
  CComPtr<IXMLDOMNodeList> data_elements;
  ASSERT_SUCCEEDED(document->getElementsByTagName(data_element_name,
                                                  &data_elements));

  CComPtr<IXMLDOMNode> data_element;
  ASSERT_SUCCEEDED(data_elements->nextNode(&data_element));
  ASSERT_TRUE(data_element);

  CString value;
  ASSERT_SUCCEEDED(ReadStringValue(data_element, &value));
  ASSERT_STREQ(verbose_log, value);
}

TEST_F(GoopdateXmlParserTest, ParseManifestFile_SeedManifest) {
  CString filename_v2(ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                                      _T("seed_manifest.xml")));
  CString *filenames[1] = {&filename_v2};

  for (int j = 0; j < kSeedManifestFileCount; j++) {
    UpdateResponses responses;
    ASSERT_SUCCEEDED(GoopdateXmlParser::ParseManifestFile(*filenames[j],
                                                          &responses));
    ASSERT_EQ(kSeedManifestResponseCount, responses.size());

    GUID expected_guids[] = {
        StringToGuid(_T("{D6B08267-B440-4c85-9F79-E195E80D9937}")),
        StringToGuid(_T("{D6B08267-B440-4c85-9F79-E195E80D9938}")),
        StringToGuid(_T("{D6B08267-B440-4c85-9F79-E195E80D9939}")),
        StringToGuid(_T("{D6B08267-B440-4c85-9F79-E195E80D9940}")),
        StringToGuid(_T("{D6B08267-B440-4c85-9F79-E195E80D9941}")),
        StringToGuid(_T("{D6B08267-B440-4c85-9F79-E195E80D9942}"))
    };

    BrowserType expected_types[] = {
        BROWSER_UNKNOWN,
        BROWSER_UNKNOWN,
        BROWSER_DEFAULT,
        BROWSER_IE,
        BROWSER_FIREFOX,
        BROWSER_UNKNOWN
    };

    for (int i = 0; i < kSeedManifestResponseCount; i++) {
      UpdateResponse response = responses[expected_guids[i]];
      const UpdateResponseData& response_data = response.update_response_data();
      EXPECT_TRUE(response_data.url().IsEmpty());
      EXPECT_EQ(0, response_data.size());
      EXPECT_TRUE(response_data.hash().IsEmpty());
      EXPECT_EQ(NEEDS_ADMIN_NO, response_data.needs_admin());
      EXPECT_TRUE(response_data.arguments().IsEmpty());
      EXPECT_TRUE(expected_guids[i] == response_data.guid());
      EXPECT_TRUE(response_data.status().IsEmpty());
      EXPECT_TRUE(GUID_NULL == response_data.installation_id());
      EXPECT_TRUE(response_data.ap().IsEmpty());
      EXPECT_TRUE(response_data.success_url().IsEmpty());
      EXPECT_TRUE(response_data.error_url().IsEmpty());
      EXPECT_EQ(expected_types[i], response_data.browser_type());
      EXPECT_STREQ(_T("en-US"), response_data.language());
      EXPECT_STREQ(_T("Test App"), response_data.app_name());
    }
  }
}

TEST_F(GoopdateXmlParserTest, ParseManifestFile_ServerManifest) {
  UpdateResponses responses;
  CString file_name(ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                                    _T("server_manifest.xml")));
  GUID guid  = StringToGuid(_T("{D6B08267-B440-4C85-9F79-E195E80D9937}"));
  GUID guid2 = StringToGuid(_T("{104844D6-7DDA-460B-89F0-FBF8AFDD0A67}"));
  GUID guid3 = StringToGuid(_T("{884a01d9-fb67-430a-b491-28f960dd7309}"));
  GUID guid4 = StringToGuid(_T("{D6B08267-B440-4C85-9F79-E195E80D9936}"));
  GUID guid5 = StringToGuid(_T("{8CF15C17-7BB5-433a-8E6C-C018D79D00B1}"));

  const TCHAR* kVerboseLog = _T("\n  {\n    \"distribution\": {\n      ")
                             _T("\"verbose_logging\": true\n    }\n  }\n  ");
  const TCHAR* kSkipFirstRun = _T("{\n    \"distribution\": {\n      \"")
                               _T("skip_first_run_ui\": true,\n    }\n  }\n  ");

  ASSERT_SUCCEEDED(GoopdateXmlParser::ParseManifestFile(file_name, &responses));
  ASSERT_EQ(kServerManifestResponseCount, responses.size());

  UpdateResponseData response_data =
      responses[guid].update_response_data();
  EXPECT_STREQ(_T("http://dl.google.com/foo/1.0.101.0/test_foo_v1.0.101.0.msi"),
               response_data.url());
  EXPECT_STREQ(_T("6bPU7OnbKAGJ1LOw6fpIUuQl1FQ="), response_data.hash());
  EXPECT_EQ(80896, response_data.size());
  EXPECT_EQ(NEEDS_ADMIN_YES, response_data.needs_admin());
  EXPECT_TRUE(response_data.arguments().IsEmpty());
  EXPECT_TRUE(guid == response_data.guid());
  EXPECT_STREQ(kResponseStatusOkValue, response_data.status());
  EXPECT_TRUE(GUID_NULL == response_data.installation_id());
  EXPECT_TRUE(response_data.ap().IsEmpty());
  EXPECT_STREQ(_T("http://testsuccessurl.com"), response_data.success_url());
  EXPECT_TRUE(response_data.error_url().IsEmpty());
  EXPECT_TRUE(response_data.terminate_all_browsers());
  EXPECT_EQ(SUCCESS_ACTION_EXIT_SILENTLY, response_data.success_action());
  EXPECT_EQ(0, responses[guid].num_components());
  EXPECT_STREQ(kVerboseLog, response_data.GetInstallData(_T("verboselogging")));
  EXPECT_STREQ(kSkipFirstRun, response_data.GetInstallData(_T("skipfirstrun")));
  EXPECT_TRUE(response_data.GetInstallData(_T("foobar")).IsEmpty());

  response_data = responses[guid4].update_response_data();
  EXPECT_TRUE(response_data.url().IsEmpty());
  EXPECT_EQ(0, response_data.size());
  EXPECT_TRUE(response_data.hash().IsEmpty());
  EXPECT_EQ(NEEDS_ADMIN_NO, response_data.needs_admin());
  EXPECT_TRUE(response_data.arguments().IsEmpty());
  EXPECT_TRUE(guid4 == response_data.guid());
  EXPECT_STREQ(kResponseStatusNoUpdate, response_data.status());
  EXPECT_TRUE(GUID_NULL == response_data.installation_id());
  EXPECT_TRUE(response_data.ap().IsEmpty());
  EXPECT_TRUE(response_data.success_url().IsEmpty());
  EXPECT_TRUE(response_data.error_url().IsEmpty());
  EXPECT_FALSE(response_data.terminate_all_browsers());
  EXPECT_EQ(SUCCESS_ACTION_DEFAULT, response_data.success_action());
  EXPECT_EQ(0, responses[guid4].num_components());

  response_data = responses[guid2].update_response_data();
  EXPECT_STREQ(_T("http://dl.google.com/foo/1.0.102.0/user_foo_v1.0.102.0.msi"),
               response_data.url());
  EXPECT_STREQ(_T("/XzRh1rpwqrDr6ashpmQnYZIzDI="), response_data.hash());
  EXPECT_EQ(630152,               response_data.size());
  EXPECT_EQ(NEEDS_ADMIN_NO,      response_data.needs_admin());
  EXPECT_STREQ(kResponseStatusOkValue, response_data.status());
  EXPECT_STREQ(_T("/install"),    response_data.arguments());
  EXPECT_TRUE(guid2 ==            response_data.guid());
  EXPECT_TRUE(response_data.success_url().IsEmpty());
  EXPECT_TRUE(response_data.error_url().IsEmpty());
  EXPECT_FALSE(response_data.terminate_all_browsers());
  EXPECT_EQ(SUCCESS_ACTION_DEFAULT, response_data.success_action());
  EXPECT_EQ(0, responses[guid2].num_components());

  response_data = responses[guid3].update_response_data();
  EXPECT_TRUE(guid3 == response_data.guid());
  EXPECT_STREQ(kResponseStatusRestrictedExportCountry, response_data.status());
  EXPECT_FALSE(response_data.terminate_all_browsers());
  EXPECT_EQ(SUCCESS_ACTION_DEFAULT, response_data.success_action());
  EXPECT_EQ(0, responses[guid3].num_components());

  response_data = responses[guid5].update_response_data();
  EXPECT_TRUE(response_data.url().IsEmpty());
  EXPECT_TRUE(response_data.hash().IsEmpty());
  EXPECT_EQ(0, response_data.size());
  EXPECT_EQ(NEEDS_ADMIN_NO, response_data.needs_admin());
  EXPECT_TRUE(response_data.arguments().IsEmpty());
  EXPECT_TRUE(guid5 == response_data.guid());
  EXPECT_STREQ(kResponseStatusOsNotSupported, response_data.status());
  EXPECT_TRUE(GUID_NULL == response_data.installation_id());
  EXPECT_TRUE(response_data.ap().IsEmpty());
  EXPECT_TRUE(response_data.success_url().IsEmpty());
  EXPECT_STREQ(_T("http://foo.google.com/support/article.py?id=12345&")
               _T("hl=es-419&os=5.1"),
               response_data.error_url());
  EXPECT_FALSE(response_data.terminate_all_browsers());
  EXPECT_EQ(SUCCESS_ACTION_DEFAULT, response_data.success_action());
  EXPECT_EQ(0, responses[guid5].num_components());
}

TEST_F(GoopdateXmlParserTest, ParseManifestFile_ServerManifest_Components) {
  UpdateResponses responses;
  CString file_name(ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                                    _T("server_manifest_components.xml")));

  // App GUIDs.
  GUID guid  = StringToGuid(_T("{D6B08267-B440-4C85-9F79-E195E80D9937}"));
  GUID guid2 = StringToGuid(_T("{104844D6-7DDA-460B-89F0-FBF8AFDD0A67}"));
  GUID guid3 = StringToGuid(_T("{884a01d9-fb67-430a-b491-28f960dd7309}"));
  GUID guid4 = StringToGuid(_T("{D6B08267-B440-4C85-9F79-E195E80D9936}"));

  ASSERT_SUCCEEDED(GoopdateXmlParser::ParseManifestFile(file_name, &responses));
  ASSERT_EQ(kServerManifestComponentsResponseCount, responses.size());

  UpdateResponse response(responses[guid]);
  UpdateResponseData response_data(response.update_response_data());
  EXPECT_STREQ(_T("http://dl.google.com/foo/1.0.101.0/test_foo_v1.0.101.0.msi"),
               response_data.url());
  EXPECT_STREQ(_T("6bPU7OnbKAGJ1LOw6fpIUuQl1FQ="), response_data.hash());
  EXPECT_EQ(80896, response_data.size());
  EXPECT_EQ(NEEDS_ADMIN_YES, response_data.needs_admin());
  EXPECT_TRUE(response_data.arguments().IsEmpty());
  EXPECT_TRUE(guid == response_data.guid());
  EXPECT_STREQ(kResponseStatusOkValue, response_data.status());
  EXPECT_TRUE(GUID_NULL == response_data.installation_id());
  EXPECT_TRUE(response_data.ap().IsEmpty());
  EXPECT_STREQ(_T("http://testsuccessurl.com"), response_data.success_url());
  EXPECT_TRUE(response_data.error_url().IsEmpty());
  EXPECT_TRUE(response_data.terminate_all_browsers());
  EXPECT_EQ(SUCCESS_ACTION_EXIT_SILENTLY, response_data.success_action());

  UpdateResponseDatas expected_components;
  UpdateResponseData temp;
  temp.set_guid(StringToGuid(_T("{65C42695-84A0-41C4-B70F-D2786F674592}")));
  temp.set_status(_T("ok"));
  temp.set_url(_T("http://dl.google.com/foo/set_comp1.msi"));
  temp.set_size(66324);
  temp.set_hash(_T("6bPU7OnbKAGJ1LOw6fpIUuQl1FQ="));
  expected_components.insert(
      std::pair<GUID, UpdateResponseData>(temp.guid(), temp));

  UpdateResponseData temp2;
  temp2.set_guid(StringToGuid(_T("{B318029C-3607-48EB-8DBB-33E8BA17BAF1}")));
  temp2.set_status(_T("noupdate"));
  expected_components.insert(
      std::pair<GUID, UpdateResponseData>(temp2.guid(), temp2));

  UpdateResponseData temp3;
  temp3.set_guid(StringToGuid(_T("{D76AE6FC-1633-4131-B782-896804795DCB}")));
  temp3.set_status(_T("ok"));
  temp3.set_url(_T("http://tools.google.com/happy/some_comp_inst.msi"));
  temp3.set_size(829984);
  temp3.set_hash(_T("6bPU7OnbKAGJ1LOw6fpIUuQl1FQ="));
  expected_components.insert(
      std::pair<GUID, UpdateResponseData>(temp3.guid(), temp3));

  UpdateResponseData temp4;
  temp4.set_guid(StringToGuid(_T("{67A52AEE-6E9F-4411-B425-F210B962CD6F}")));
  temp4.set_status(_T("noupdate"));
  expected_components.insert(
      std::pair<GUID, UpdateResponseData>(temp4.guid(), temp4));

  EXPECT_EQ(expected_components.size(), response.num_components());

  UpdateResponseDatas::const_iterator it;
  UpdateResponseDatas::const_iterator it_exp;
  for (it = response.components_begin(), it_exp = expected_components.begin();
       it_exp != expected_components.end();
       ++it, ++it_exp) {
    const UpdateResponseData& component = (*it).second;
    const UpdateResponseData& component_expected = (*it_exp).second;

    EXPECT_STREQ(component_expected.url(), component.url());
    EXPECT_STREQ(component_expected.hash(), component.hash());
    EXPECT_EQ(component_expected.size(), component.size());
    EXPECT_EQ(component_expected.needs_admin(), component.needs_admin());
    EXPECT_STREQ(component_expected.arguments(), component.arguments());
    EXPECT_TRUE(::IsEqualGUID(component_expected.guid(), component.guid()));
    EXPECT_STREQ(component_expected.status(), component.status());
    EXPECT_TRUE(::IsEqualGUID(component_expected.installation_id(),
                              component.installation_id()));
    EXPECT_STREQ(component_expected.ap(), component.ap());
    EXPECT_STREQ(component_expected.success_url(), component.success_url());
    EXPECT_TRUE(response_data.error_url().IsEmpty());
    EXPECT_EQ(component_expected.terminate_all_browsers(),
              component.terminate_all_browsers());
    EXPECT_EQ(component_expected.success_action(), component.success_action());
  }

  response = responses[guid4];
  response_data = response.update_response_data();
  EXPECT_TRUE(response_data.url().IsEmpty());
  EXPECT_EQ(0, response_data.size());
  EXPECT_TRUE(response_data.hash().IsEmpty());
  EXPECT_EQ(NEEDS_ADMIN_NO, response_data.needs_admin());
  EXPECT_TRUE(response_data.arguments().IsEmpty());
  EXPECT_TRUE(guid4 == response_data.guid());
  EXPECT_STREQ(kResponseStatusNoUpdate, response_data.status());
  EXPECT_TRUE(GUID_NULL == response_data.installation_id());
  EXPECT_TRUE(response_data.ap().IsEmpty());
  EXPECT_TRUE(response_data.success_url().IsEmpty());
  EXPECT_TRUE(response_data.error_url().IsEmpty());
  EXPECT_FALSE(response_data.terminate_all_browsers());
  EXPECT_EQ(SUCCESS_ACTION_DEFAULT, response_data.success_action());
  EXPECT_EQ(1, response.num_components());

  response = responses[guid2];
  response_data = response.update_response_data();
  EXPECT_STREQ(_T("http://dl.google.com/foo/1.0.102.0/user_foo_v1.0.102.0.msi"),
               response_data.url());
  EXPECT_STREQ(_T("/XzRh1rpwqrDr6ashpmQnYZIzDI="), response_data.hash());
  EXPECT_EQ(630152,               response_data.size());
  EXPECT_EQ(NEEDS_ADMIN_NO,      response_data.needs_admin());
  EXPECT_STREQ(kResponseStatusOkValue, response_data.status());
  EXPECT_STREQ(_T("/install"),    response_data.arguments());
  EXPECT_TRUE(guid2 ==            response_data.guid());
  EXPECT_TRUE(response_data.success_url().IsEmpty());
  EXPECT_TRUE(response_data.error_url().IsEmpty());
  EXPECT_FALSE(response_data.terminate_all_browsers());
  EXPECT_EQ(SUCCESS_ACTION_DEFAULT, response_data.success_action());
  EXPECT_EQ(0, response.num_components());

  response = responses[guid3];
  response_data = response.update_response_data();
  EXPECT_TRUE(guid3 == response_data.guid());
  EXPECT_STREQ(kResponseStatusRestrictedExportCountry, response_data.status());
  EXPECT_FALSE(response_data.terminate_all_browsers());
  EXPECT_EQ(SUCCESS_ACTION_DEFAULT, response_data.success_action());
  EXPECT_EQ(0, response.num_components());
}

TEST_F(GoopdateXmlParserTest, ParseManifestFile_EmptyFilename) {
  CString empty_filename;
  UpdateResponses responses;
  EXPECT_EQ(E_INVALIDARG, GoopdateXmlParser::ParseManifestFile(empty_filename,
                                                               &responses));
}

TEST_F(GoopdateXmlParserTest, ParseManifestFile_NoSuchFile) {
  CString no_such_file(_T("no_such_file.xml"));
  UpdateResponses responses;
  EXPECT_EQ(0x800c0005, GoopdateXmlParser::ParseManifestFile(no_such_file,
                                                             &responses));
}

// This is a duplicate of the ParseManifestFile test except that it uses
// LoadXmlFileToMemory() and ParseManifestString().
TEST_F(GoopdateXmlParserTest, ParseManifestString_ServerManifest) {
  UpdateResponses responses;
  CString file_name(ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                                    _T("server_manifest.xml")));
  GUID guid  = StringToGuid(_T("{D6B08267-B440-4C85-9F79-E195E80D9937}"));
  GUID guid2 = StringToGuid(_T("{104844D6-7DDA-460B-89F0-FBF8AFDD0A67}"));
  GUID guid3 = StringToGuid(_T("{884a01d9-fb67-430a-b491-28f960dd7309}"));
  GUID guid4 = StringToGuid(_T("{D6B08267-B440-4C85-9F79-E195E80D9936}"));
  GUID guid5 = StringToGuid(_T("{8CF15C17-7BB5-433a-8E6C-C018D79D00B1}"));

  CString manifest_contents;
  EXPECT_SUCCEEDED(GoopdateXmlParser::LoadXmlFileToMemory(file_name,
                                                          &manifest_contents));

  ASSERT_SUCCEEDED(GoopdateXmlParser::ParseManifestString(manifest_contents,
                                                          &responses));
  ASSERT_EQ(kServerManifestResponseCount, responses.size());

  UpdateResponse response(responses[guid]);
  UpdateResponseData response_data = response.update_response_data();
  EXPECT_STREQ(_T("http://dl.google.com/foo/1.0.101.0/test_foo_v1.0.101.0.msi"),
               response_data.url());
  EXPECT_STREQ(_T("6bPU7OnbKAGJ1LOw6fpIUuQl1FQ="), response_data.hash());
  EXPECT_EQ(80896, response_data.size());
  EXPECT_EQ(NEEDS_ADMIN_YES, response_data.needs_admin());
  EXPECT_TRUE(response_data.arguments().IsEmpty());
  EXPECT_TRUE(guid == response_data.guid());
  EXPECT_STREQ(kResponseStatusOkValue, response_data.status());
  EXPECT_TRUE(GUID_NULL == response_data.installation_id());
  EXPECT_TRUE(response_data.ap().IsEmpty());
  EXPECT_STREQ(_T("http://testsuccessurl.com"), response_data.success_url());
  EXPECT_TRUE(response_data.error_url().IsEmpty());
  EXPECT_TRUE(response_data.terminate_all_browsers());
  EXPECT_EQ(SUCCESS_ACTION_EXIT_SILENTLY, response_data.success_action());

  response = responses[guid4];
  response_data = response.update_response_data();
  EXPECT_TRUE(response_data.url().IsEmpty());
  EXPECT_EQ(0, response_data.size());
  EXPECT_TRUE(response_data.hash().IsEmpty());
  EXPECT_EQ(NEEDS_ADMIN_NO, response_data.needs_admin());
  EXPECT_TRUE(response_data.arguments().IsEmpty());
  EXPECT_TRUE(guid4 == response_data.guid());
  EXPECT_STREQ(kResponseStatusNoUpdate, response_data.status());
  EXPECT_TRUE(GUID_NULL == response_data.installation_id());
  EXPECT_TRUE(response_data.ap().IsEmpty());
  EXPECT_TRUE(response_data.success_url().IsEmpty());
  EXPECT_TRUE(response_data.error_url().IsEmpty());
  EXPECT_FALSE(response_data.terminate_all_browsers());
  EXPECT_EQ(SUCCESS_ACTION_DEFAULT, response_data.success_action());

  response = responses[guid2];
  response_data = response.update_response_data();
  EXPECT_STREQ(_T("http://dl.google.com/foo/1.0.102.0/user_foo_v1.0.102.0.msi"),
               response_data.url());
  EXPECT_STREQ(_T("/XzRh1rpwqrDr6ashpmQnYZIzDI="), response_data.hash());
  EXPECT_EQ(630152,               response_data.size());
  EXPECT_EQ(NEEDS_ADMIN_NO,      response_data.needs_admin());
  EXPECT_STREQ(kResponseStatusOkValue, response_data.status());
  EXPECT_STREQ(_T("/install"),    response_data.arguments());
  EXPECT_TRUE(guid2 ==            response_data.guid());
  EXPECT_TRUE(response_data.success_url().IsEmpty());
  EXPECT_TRUE(response_data.error_url().IsEmpty());
  EXPECT_FALSE(response_data.terminate_all_browsers());
  EXPECT_EQ(SUCCESS_ACTION_DEFAULT, response_data.success_action());

  response = responses[guid3];
  response_data = response.update_response_data();
  EXPECT_TRUE(guid3 == response_data.guid());
  EXPECT_STREQ(kResponseStatusRestrictedExportCountry, response_data.status());
  EXPECT_FALSE(response_data.terminate_all_browsers());
  EXPECT_EQ(SUCCESS_ACTION_DEFAULT, response_data.success_action());

  response = responses[guid5];
  response_data = response.update_response_data();
  EXPECT_TRUE(response_data.url().IsEmpty());
  EXPECT_TRUE(response_data.hash().IsEmpty());
  EXPECT_EQ(0, response_data.size());
  EXPECT_EQ(NEEDS_ADMIN_NO, response_data.needs_admin());
  EXPECT_TRUE(response_data.arguments().IsEmpty());
  EXPECT_TRUE(guid5 == response_data.guid());
  EXPECT_STREQ(kResponseStatusOsNotSupported, response_data.status());
  EXPECT_TRUE(GUID_NULL == response_data.installation_id());
  EXPECT_TRUE(response_data.ap().IsEmpty());
  EXPECT_TRUE(response_data.success_url().IsEmpty());
  EXPECT_STREQ(_T("http://foo.google.com/support/article.py?id=12345&")
               _T("hl=es-419&os=5.1"),
               response_data.error_url());
  EXPECT_FALSE(response_data.terminate_all_browsers());
  EXPECT_EQ(SUCCESS_ACTION_DEFAULT, response_data.success_action());
  EXPECT_EQ(0, response.num_components());
}

TEST_F(GoopdateXmlParserTest, ParseManifestString_EmptyString) {
  CString empty_string;
  UpdateResponses responses;
  EXPECT_EQ(0xC00CE558, GoopdateXmlParser::ParseManifestString(empty_string,
                                                               &responses));
}

TEST_F(GoopdateXmlParserTest, ParseManifestString_ManifestNotXml) {
  CString not_xml(_T("<this> is not XML"));
  UpdateResponses responses;
  EXPECT_EQ(0xC00CE553, GoopdateXmlParser::ParseManifestString(not_xml,
                                                               &responses));
}

TEST_F(GoopdateXmlParserTest, VerifyProtocolCompatibility) {
  // Compatible versions (same major, actual minor >= expected)
  EXPECT_SUCCEEDED(VerifyProtocolCompatibility(_T("2.0"), _T("2.0")));
  EXPECT_SUCCEEDED(VerifyProtocolCompatibility(_T("2.00"), _T("2.0")));
  EXPECT_SUCCEEDED(VerifyProtocolCompatibility(_T("2.001"), _T("2.0")));
  EXPECT_SUCCEEDED(VerifyProtocolCompatibility(_T("2.1"), _T("2.0")));
  EXPECT_SUCCEEDED(VerifyProtocolCompatibility(_T("2.9"), _T("2.0")));
  EXPECT_SUCCEEDED(VerifyProtocolCompatibility(_T("2.9"), _T("2.4")));

  // Incompatible versions (actual < expected)
  EXPECT_EQ(GOOPDATEXML_E_XMLVERSION,
            VerifyProtocolCompatibility(_T("2.0"), _T("2.1")));
  EXPECT_EQ(GOOPDATEXML_E_XMLVERSION,
            VerifyProtocolCompatibility(_T("2.1"), _T("3.0")));

  // Incompatible versions (actual major < expected major)
  EXPECT_EQ(GOOPDATEXML_E_XMLVERSION,
            VerifyProtocolCompatibility(_T("3.0"), _T("2.0")));
  EXPECT_EQ(GOOPDATEXML_E_XMLVERSION,
            VerifyProtocolCompatibility(_T("3.0"), _T("2.1")));
  EXPECT_EQ(GOOPDATEXML_E_XMLVERSION,
            VerifyProtocolCompatibility(_T("3.0"), _T("2.9")));

  // VerifyProtocolCompatibility isn't perfect.
  // This test case succeeds but should return GOOPDATEXML_E_XMLVERSION.
  // We shouldn't ever see this case in a real file.
  EXPECT_SUCCEEDED(VerifyProtocolCompatibility(
                       _T("3.0"), _T("2.99999999999999999999999999999")));
}

TEST_F(GoopdateXmlParserTest, LoadXmlFileToMemory) {
  CString base_seed_path(ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                                         _T("seed_manifest_with_args.xml")));
  const CString kExpectedManifestContents(
      _T("<?xml version=\"1.0\"?>\r\n")
      _T("<gupdate protocol=\"2.0\" signature=\"\" xmlns=\"http://www.google.com/update2/install\">\r\n")   // NOLINT
      _T("\t<install appguid=\"{283EAF47-8817-4c2b-A801-AD1FADFB7BAA}\" needsadmin=\"true\" iid=\"{874E4D29-8671-40C8-859F-4DECA4819999}\" client=\"someclient\" ap=\"1.0-dev\"/>\r\n")   // NOLINT
      _T("</gupdate>\r\n"));

  CString manifest_contents;
  EXPECT_SUCCEEDED(GoopdateXmlParser::LoadXmlFileToMemory(base_seed_path,
                                                          &manifest_contents));

  EXPECT_STREQ(kExpectedManifestContents, manifest_contents);
}

TEST_F(GoopdateXmlParserTest, LoadXmlFileToMemory_EmptyFilename) {
  CString empty_filename;
  CString manifest_contents;
  EXPECT_EQ(E_INVALIDARG, GoopdateXmlParser::LoadXmlFileToMemory(
                              empty_filename, &manifest_contents));
}

TEST_F(GoopdateXmlParserTest, LoadXmlFileToMemory_NoSuchFile) {
  CString no_such_file(_T("no_such_file.xml"));
  CString manifest_contents;
  EXPECT_EQ(0x800c0005, GoopdateXmlParser::LoadXmlFileToMemory(
                            no_such_file, &manifest_contents));
}

TEST_F(GoopdateXmlParserTest, ConvertStringToSuccessAction_EmptyString) {
  EXPECT_EQ(SUCCESS_ACTION_DEFAULT, ConvertStringToSuccessAction(_T("")));
}

TEST_F(GoopdateXmlParserTest, ConvertStringToSuccessAction_Default) {
  EXPECT_EQ(SUCCESS_ACTION_DEFAULT,
            ConvertStringToSuccessAction(_T("default")));
}

TEST_F(GoopdateXmlParserTest, ConvertStringToSuccessAction_ExitSilently) {
  EXPECT_EQ(SUCCESS_ACTION_EXIT_SILENTLY,
            ConvertStringToSuccessAction(_T("exitsilently")));
}

TEST_F(GoopdateXmlParserTest,
       ConvertStringToSuccessAction_ExitSilentlyOnLaunchCmd) {
  EXPECT_EQ(SUCCESS_ACTION_EXIT_SILENTLY_ON_LAUNCH_CMD,
            ConvertStringToSuccessAction(_T("exitsilentlyonlaunchcmd")));
}

TEST_F(GoopdateXmlParserTest, ConvertStringToSuccessAction_UnknownAction) {
  ExpectAsserts expect_asserts;
  EXPECT_EQ(SUCCESS_ACTION_DEFAULT,
            ConvertStringToSuccessAction(_T("foo bar")));
}

}  // namespace omaha

