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

#include "omaha/goopdate/update_response_utils.h"

#include "omaha/base/app_util.h"
#include "omaha/base/constants.h"
#include "omaha/base/error.h"
#include "omaha/base/system_info.h"
#include "omaha/goopdate/app_unittest_base.h"
#include "omaha/goopdate/resource_manager.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace update_response_utils {

using xml::UpdateResponseResult;

namespace {

const TCHAR* const kAppId1 = _T("{CE9C207B-232D-492b-AF03-E590A8FBE8FB}");
const TCHAR* const kAppId2 = _T("{5881940A-72E4-4194-9DA5-4EA4089F867C}");
const TCHAR* const kAppId3 = _T("{2DE92FA0-C8A1-4368-8654-16FDBA91817D}");

const TCHAR* const kAppIdWithLowerCase =
    _T("{C38D11BA-6244-45e3-AC2A-21F2077F2C10}");
const TCHAR* const kAppIdWithLowerCaseAllUpperCase =
    _T("{C38D11BA-6244-45E3-AC2A-21F2077F2C10}");


const TCHAR* const kGOOPDATE_E_NO_SERVER_RESPONSEString =
    _T("Installation failed due to a server side error. Please try again ")
    _T("later.");

const TCHAR* const GOOPDATE_E_HW_NOT_SUPPORTEDString =
    _T("Installation failed because your computer does not meet minimum ")
    _T("hardware requirements for Google Chrome.");

const TCHAR* const GOOPDATE_E_OS_NOT_SUPPORTEDString =
    _T("Installation failed because your version of Windows is not supported.");

const UpdateResponseResult kUpdateAvailableResult =
    std::make_pair(S_OK, CString());

const UpdateResponseResult kAppNotFoundResult = std::make_pair(
      GOOPDATE_E_NO_SERVER_RESPONSE,
      CString(kGOOPDATE_E_NO_SERVER_RESPONSEString));

const UpdateResponseResult kHwNotSupported = std::make_pair(
      GOOPDATE_E_HW_NOT_SUPPORTED,
      CString(GOOPDATE_E_HW_NOT_SUPPORTEDString));

const UpdateResponseResult kOk = std::make_pair(S_OK, CString());

const UpdateResponseResult kOsNotSupported = std::make_pair(
      GOOPDATE_E_OS_NOT_SUPPORTED,
      CString(GOOPDATE_E_OS_NOT_SUPPORTEDString));

}  // namespace


class UpdateResponseUtilsGetResultTest : public testing::Test {
 protected:
  virtual void SetUp() {
    // Needed for error strings.
    EXPECT_SUCCEEDED(ResourceManager::Create(
      false, app_util::GetCurrentModuleDirectory(), _T("en")));

    update_response_.reset(xml::UpdateResponse::Create());
  }

  virtual void TearDown() {
    ResourceManager::Delete();
  }

  std::unique_ptr<xml::UpdateResponse> update_response_;
};

class UpdateResponseUtilsTest : public AppTestBase,
                                public ::testing::WithParamInterface<bool> {
 protected:
  UpdateResponseUtilsTest() : AppTestBase(IsMachine(), true) {}

  bool IsMachine() {
    return GetParam();
  }
};


TEST(UpdateResponseUtilsGetAppTest, AppNotFound) {
  xml::response::Response response;
  xml::response::App app;
  app.status = xml::response::kStatusOkValue;
  app.appid = kAppId1;
  response.apps.push_back(app);

  EXPECT_TRUE(NULL != GetApp(response, kAppId1));
  EXPECT_EQ(NULL, GetApp(response, kAppId2));
}

TEST(UpdateResponseUtilsGetAppTest, MultipleApps) {
  xml::response::Response response;
  xml::response::App app;
  app.status = xml::response::kStatusOkValue;
  app.appid = kAppId1;
  response.apps.push_back(app);
  app.appid = kAppId2;
  response.apps.push_back(app);

  EXPECT_EQ(&response.apps[0], GetApp(response, kAppId1));
  EXPECT_EQ(&response.apps[1], GetApp(response, kAppId2));
  EXPECT_EQ(&response.apps[1], GetApp(response, kAppId2));
  EXPECT_EQ(&response.apps[0], GetApp(response, kAppId1));
  EXPECT_NE(GetApp(response, kAppId1), GetApp(response, kAppId2));
}

TEST(UpdateResponseUtilsGetAppTest, ResponseAppIdHasLowerCase) {
  xml::response::Response response;
  xml::response::App app;
  app.status = xml::response::kStatusOkValue;
  app.appid = kAppIdWithLowerCase;
  response.apps.push_back(app);

  EXPECT_EQ(&response.apps[0], GetApp(response, kAppIdWithLowerCase));
  EXPECT_EQ(&response.apps[0],
            GetApp(response, kAppIdWithLowerCaseAllUpperCase));
}

TEST(UpdateResponseUtilsGetAppTest, ResponseAppIdAllUpperCase) {
  xml::response::Response response;
  xml::response::App app;
  app.status = xml::response::kStatusOkValue;
  app.appid = kAppIdWithLowerCaseAllUpperCase;
  response.apps.push_back(app);

  EXPECT_EQ(&response.apps[0],
            GetApp(response, kAppIdWithLowerCaseAllUpperCase));
  EXPECT_EQ(&response.apps[0], GetApp(response, kAppIdWithLowerCase));
}

TEST_F(UpdateResponseUtilsGetResultTest, EmptyResponse) {
  EXPECT_TRUE(kAppNotFoundResult ==
              GetResult(update_response_.get(), kAppId1, _T(""), _T("en")));
}

TEST_F(UpdateResponseUtilsGetResultTest, AppFound) {
  xml::response::Response response;
  xml::response::App app;
  app.status = xml::response::kStatusOkValue;
  app.update_check.status = xml::response::kStatusOkValue;
  app.appid = kAppId1;
  response.apps.push_back(app);
  SetResponseForUnitTest(update_response_.get(), response);

  EXPECT_TRUE(kUpdateAvailableResult ==
              GetResult(update_response_.get(), kAppId1, _T(""), _T("en")));
}

TEST_F(UpdateResponseUtilsGetResultTest, AppNotFound) {
  xml::response::Response response;
  xml::response::App app;
  app.status = xml::response::kStatusOkValue;
  app.update_check.status = xml::response::kStatusOkValue;
  app.appid = kAppId1;
  response.apps.push_back(app);
  SetResponseForUnitTest(update_response_.get(), response);

  EXPECT_TRUE(kAppNotFoundResult ==
              GetResult(update_response_.get(), kAppId2, _T(""), _T("en")));
}

TEST_F(UpdateResponseUtilsGetResultTest, MultipleApps) {
  xml::response::Response response;
  xml::response::App app;
  app.status = xml::response::kStatusOkValue;
  app.update_check.status = xml::response::kStatusOkValue;
  app.appid = kAppId1;
  response.apps.push_back(app);
  app.appid = kAppId2;
  response.apps.push_back(app);
  SetResponseForUnitTest(update_response_.get(), response);

  EXPECT_TRUE(kUpdateAvailableResult ==
              GetResult(update_response_.get(), kAppId1, _T(""), _T("en")));
  EXPECT_TRUE(kUpdateAvailableResult ==
              GetResult(update_response_.get(), kAppId2, _T(""), _T("en")));
  EXPECT_TRUE(kAppNotFoundResult ==
              GetResult(update_response_.get(), kAppId3, _T(""), _T("en")));
}

TEST_F(UpdateResponseUtilsGetResultTest, ResponseAppIdHasLowerCase) {
  xml::response::Response response;
  xml::response::App app;
  app.status = xml::response::kStatusOkValue;
  app.appid = kAppIdWithLowerCase;
  response.apps.push_back(app);
  SetResponseForUnitTest(update_response_.get(), response);

  EXPECT_TRUE(kUpdateAvailableResult ==
              GetResult(update_response_.get(),
              kAppIdWithLowerCase,
              _T(""),
              _T("en")));
  EXPECT_TRUE(kUpdateAvailableResult ==
              GetResult(update_response_.get(),
                        kAppIdWithLowerCaseAllUpperCase,
                        _T(""),
                        _T("en")));
}

TEST_F(UpdateResponseUtilsGetResultTest, ResponseAppIdAllUpperCase) {
  xml::response::Response response;
  xml::response::App app;
  app.status = xml::response::kStatusOkValue;
  app.appid = kAppIdWithLowerCaseAllUpperCase;
  response.apps.push_back(app);
  SetResponseForUnitTest(update_response_.get(), response);

  EXPECT_TRUE(kUpdateAvailableResult ==
              GetResult(update_response_.get(),
                        kAppIdWithLowerCaseAllUpperCase,
                        _T(""),
                        _T("en")));
  EXPECT_TRUE(kUpdateAvailableResult ==
              GetResult(update_response_.get(),
                        kAppIdWithLowerCase,
                        _T(""),
                        _T("en")));
}

TEST(UpdateResponseUtils, ValidateUntrustedData) {
  std::vector<xml::response::Data> data;

  const xml::response::Data untrusted_data_ok = { _T("ok"), _T("untrusted") };
  data.push_back(untrusted_data_ok);
  EXPECT_EQ(S_OK, ValidateUntrustedData(data));
  data.clear();

  const xml::response::Data untrusted_data_invalid_args = {
      _T("error-invalidargs"),  _T("untrusted") };
  data.push_back(untrusted_data_invalid_args);
  EXPECT_EQ(GOOPDATEINSTALL_E_INVALID_UNTRUSTED_DATA,
            ValidateUntrustedData(data));
  data.clear();

  const xml::response::Data untrusted_data_other = {
      _T("other server status"), _T("untrusted") };
  data.push_back(untrusted_data_other);
  EXPECT_EQ(GOOPDATEINSTALL_E_INVALID_UNTRUSTED_DATA,
            ValidateUntrustedData(data));
  data.clear();

  const xml::response::Data untrusted_data_empty_status = {
      _T(""), _T("untrusted") };
  data.push_back(untrusted_data_empty_status);
  EXPECT_EQ(GOOPDATEINSTALL_E_INVALID_UNTRUSTED_DATA,
            ValidateUntrustedData(data));
  data.clear();

  const xml::response::Data untrusted_data_empty = { _T(""), _T("") };
  data.push_back(untrusted_data_empty);
  EXPECT_EQ(GOOPDATEINSTALL_E_INVALID_UNTRUSTED_DATA,
           ValidateUntrustedData(data));
  data.clear();
}

TEST_F(UpdateResponseUtilsGetResultTest, HwNotSupported) {
  xml::response::Response response;
  xml::response::App app;
  app.status = xml::response::kStatusHwNotSupported;
  app.appid = kAppId1;
  response.apps.push_back(app);
  SetResponseForUnitTest(update_response_.get(), response);

  EXPECT_TRUE(kHwNotSupported == GetResult(update_response_.get(),
                                           kAppId1,
                                           _T("Google Chrome"),
                                           _T("en")));
}

TEST_F(UpdateResponseUtilsGetResultTest, CheckSystemRequirements) {
  const CString current_arch = SystemInfo::GetArchitecture();

  const struct {
    const TCHAR* platform;
    const TCHAR* arch_list;
    const TCHAR* min_os_version;
    const xml::UpdateResponseResult& expected_result;
  } test_cases[] = {
      {_T("win"), _T("x86"), _T("6.0"), kOk},
      {_T("mac"), _T("x86"), _T("6.0"), kOsNotSupported},
      {_T("win"), _T("unknown"), _T("6.0"), kOsNotSupported},
      {_T("win"), _T("x64"), _T("6.0"),
       current_arch == kArchAmd64 ? kOk : kOsNotSupported},
      {_T("win"), _T("-x64"), _T("6.0"),
       current_arch != kArchAmd64 ? kOk : kOsNotSupported},
      {_T("win"), _T("x86,-x64"), _T("6.0"),
       current_arch != kArchAmd64 ? kOk : kOsNotSupported},
      {_T("win"), _T("x86,x64,-arm64"), _T("6.0"),
       current_arch != kArchArm64 ? kOk : kOsNotSupported},
      {_T("win"), _T("x86"), _T("60.0"), kOsNotSupported},
      {_T("win"), _T("x86"), _T("0.01"), kOk},
  };

  for (const auto& test_case : test_cases) {
    xml::response::Response response;
    xml::response::SystemRequirements& sys_req = response.sys_req;
    sys_req.platform = test_case.platform;
    sys_req.arch = test_case.arch_list;
    sys_req.min_os_version = test_case.min_os_version;

    SetResponseForUnitTest(update_response_.get(), response);

    EXPECT_EQ(CheckSystemRequirements(update_response_.get(), _T("en")),
              test_case.expected_result)
        << test_case.platform << ": " << test_case.arch_list << ": "
        << test_case.min_os_version << ": " << current_arch << ": "
        << test_case.expected_result.first << ": "
        << test_case.expected_result.second;
  }
}

INSTANTIATE_TEST_CASE_P(IsMachine, UpdateResponseUtilsTest, ::testing::Bool());

TEST_P(UpdateResponseUtilsTest, BuildApp_Cohorts) {
  struct AppCohort {
    CString appid;
    Cohort cohort;
  };

  AppCohort appcohorts[] = {
    {kAppId1, {_T("Cohort1"), _T("Hint1"), _T("Name1")}},
    {kAppId2, {_T("Cohort2"), _T(""), _T("Name2")}},
    {kAppId3, {_T("Cohort3"), _T("Hint3"), _T("")}},
  };

  CStringA update_response_string =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<response protocol=\"3.0\">";

  for (int i = 0; i < arraysize(appcohorts); ++i) {
    App* a = NULL;
    ASSERT_SUCCEEDED(app_bundle_->createApp(CComBSTR(appcohorts[i].appid), &a));

    update_response_string.AppendFormat(
      "<app appid=\"%S\" status=\"ok\" "
          "cohort=\"%S\" cohorthint=\"%S\" cohortname=\"%S\">",
      appcohorts[i].appid,
      appcohorts[i].cohort.cohort,
      appcohorts[i].cohort.hint,
      appcohorts[i].cohort.name);

    update_response_string.Append(
        "<updatecheck status=\"ok\"/>"
      "</app>");
  }

  update_response_string.Append("</response>");

  EXPECT_SUCCEEDED(LoadBundleFromXml(app_bundle_.get(),
                                     update_response_string));

  for (int i = 0; i < arraysize(appcohorts); ++i) {
    Cohort cohort = app_bundle_->GetApp(i)->cohort();
    EXPECT_STREQ(appcohorts[i].cohort.cohort, cohort.cohort);
    EXPECT_STREQ(appcohorts[i].cohort.hint, cohort.hint);
    EXPECT_STREQ(appcohorts[i].cohort.name, cohort.name);
  }
}

// TODO(omaha3): Add tests for GetResult from Omaha2's job_creator_unittest.cc.

}  // namespace update_response_utils

}  // namespace omaha
