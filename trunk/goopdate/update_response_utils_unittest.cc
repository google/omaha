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

#include "base/scoped_ptr.h"
#include "omaha/base/app_util.h"
#include "omaha/base/constants.h"
#include "omaha/base/error.h"
#include "omaha/goopdate/resource_manager.h"
#include "omaha/goopdate/update_response_utils.h"
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
    _T("later. We apologize for the inconvenience.");

const UpdateResponseResult kUpdateAvailableResult =
    std::make_pair(S_OK, _T(""));

const UpdateResponseResult kAppNotFoundResult = std::make_pair(
      GOOPDATE_E_NO_SERVER_RESPONSE,
      kGOOPDATE_E_NO_SERVER_RESPONSEString);

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

  scoped_ptr<xml::UpdateResponse> update_response_;
};


// TODO(omaha): write tests.

TEST(UpdateResponseUtilsGetAppTest, AppNotFound) {
  xml::response::Response response;
  xml::response::App app;
  app.status = kResponseStatusOkValue;
  app.appid = kAppId1;
  response.apps.push_back(app);

  EXPECT_TRUE(NULL != GetApp(response, kAppId1));
  EXPECT_EQ(NULL, GetApp(response, kAppId2));
}

TEST(UpdateResponseUtilsGetAppTest, MultipleApps) {
  xml::response::Response response;
  xml::response::App app;
  app.status = kResponseStatusOkValue;
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
  app.status = kResponseStatusOkValue;
  app.appid = kAppIdWithLowerCase;
  response.apps.push_back(app);

  EXPECT_EQ(&response.apps[0], GetApp(response, kAppIdWithLowerCase));
  EXPECT_EQ(&response.apps[0],
            GetApp(response, kAppIdWithLowerCaseAllUpperCase));
}

TEST(UpdateResponseUtilsGetAppTest, ResponseAppIdAllUpperCase) {
  xml::response::Response response;
  xml::response::App app;
  app.status = kResponseStatusOkValue;
  app.appid = kAppIdWithLowerCaseAllUpperCase;
  response.apps.push_back(app);

  EXPECT_EQ(&response.apps[0],
            GetApp(response, kAppIdWithLowerCaseAllUpperCase));
  EXPECT_EQ(&response.apps[0], GetApp(response, kAppIdWithLowerCase));
}

TEST_F(UpdateResponseUtilsGetResultTest, EmptyResponse) {
  EXPECT_TRUE(kAppNotFoundResult ==
              GetResult(update_response_.get(), kAppId1, _T("en")));
}

TEST_F(UpdateResponseUtilsGetResultTest, AppFound) {
  xml::response::Response response;
  xml::response::App app;
  app.status = kResponseStatusOkValue;
  app.update_check.status = kResponseStatusOkValue;
  app.appid = kAppId1;
  response.apps.push_back(app);
  SetResponseForUnitTest(update_response_.get(), response);

  EXPECT_TRUE(kUpdateAvailableResult ==
              GetResult(update_response_.get(), kAppId1, _T("en")));
}

TEST_F(UpdateResponseUtilsGetResultTest, AppNotFound) {
  xml::response::Response response;
  xml::response::App app;
  app.status = kResponseStatusOkValue;
  app.update_check.status = kResponseStatusOkValue;
  app.appid = kAppId1;
  response.apps.push_back(app);
  SetResponseForUnitTest(update_response_.get(), response);

  EXPECT_TRUE(kAppNotFoundResult ==
              GetResult(update_response_.get(), kAppId2, _T("en")));
}

TEST_F(UpdateResponseUtilsGetResultTest, MultipleApps) {
  xml::response::Response response;
  xml::response::App app;
  app.status = kResponseStatusOkValue;
  app.update_check.status = kResponseStatusOkValue;
  app.appid = kAppId1;
  response.apps.push_back(app);
  app.appid = kAppId2;
  response.apps.push_back(app);
  SetResponseForUnitTest(update_response_.get(), response);

  EXPECT_TRUE(kUpdateAvailableResult ==
              GetResult(update_response_.get(), kAppId1, _T("en")));
  EXPECT_TRUE(kUpdateAvailableResult ==
              GetResult(update_response_.get(), kAppId2, _T("en")));
  EXPECT_TRUE(kAppNotFoundResult ==
              GetResult(update_response_.get(), kAppId3, _T("en")));
}

TEST_F(UpdateResponseUtilsGetResultTest, ResponseAppIdHasLowerCase) {
  xml::response::Response response;
  xml::response::App app;
  app.status = kResponseStatusOkValue;
  app.appid = kAppIdWithLowerCase;
  response.apps.push_back(app);
  SetResponseForUnitTest(update_response_.get(), response);

  EXPECT_TRUE(kUpdateAvailableResult ==
              GetResult(update_response_.get(), kAppIdWithLowerCase, _T("en")));
  EXPECT_TRUE(kUpdateAvailableResult ==
              GetResult(update_response_.get(), kAppIdWithLowerCaseAllUpperCase,
                        _T("en")));
}

TEST_F(UpdateResponseUtilsGetResultTest, ResponseAppIdAllUpperCase) {
  xml::response::Response response;
  xml::response::App app;
  app.status = kResponseStatusOkValue;
  app.appid = kAppIdWithLowerCaseAllUpperCase;
  response.apps.push_back(app);
  SetResponseForUnitTest(update_response_.get(), response);

  EXPECT_TRUE(kUpdateAvailableResult ==
              GetResult(update_response_.get(), kAppIdWithLowerCaseAllUpperCase,
                        _T("en")));
  EXPECT_TRUE(kUpdateAvailableResult ==
              GetResult(update_response_.get(), kAppIdWithLowerCase, _T("en")));
}

// TODO(omaha3): Add tests for GetResult from Omaha2's job_creator_unittest.cc.

}  // namespace update_response_utils

}  // namespace omaha
