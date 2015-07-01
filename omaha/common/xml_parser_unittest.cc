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

#include <windows.h>
#include "base/utils.h"
#include "base/scoped_ptr.h"
#include "omaha/base/error.h"
#include "omaha/common/xml_parser.h"
#include "omaha/goopdate/update_response_utils.h"
#include "omaha/testing/unit_test.h"

namespace {

const int kSeedManifestFileCount = 1;
const int kSeedManifestResponseCount = 7;

const int kExpectedRequestLength = 2048;

}  // namespace

namespace omaha {

namespace xml {

// TODO(omaha): there were many tests related to
// updatedev_check_period_override and policy_check_period_override, which
// the current parser is unaware of. These parameters must be handled outside
// the parser itself. Not sure how we are handling them now.
class XmlParserTest : public testing::Test {
 protected:
  XmlParserTest() {
  }

  ~XmlParserTest() {
  }

  virtual void SetUp() {
  }

  virtual void TearDown() {
  }

  // Allows test fixtures access to implementation details of UpdateRequest.
  request::Request& get_xml_request(UpdateRequest* update_request) {
    return update_request->request_;
  }
};

// Creates a machine update request and serializes it.
TEST_F(XmlParserTest, GenerateRequestWithoutUserId_MachineUpdateRequest) {
  // The origin URL contains an invalid XML character, the double-quote. The
  // expectation is that this character should be escaped to "&quot;".
  scoped_ptr<UpdateRequest> update_request(
      UpdateRequest::Create(true,
                            _T("unittest_session"),
                            _T("unittest_install"),
                            _T("http://go/foo/\"")));

  request::Request& xml_request = get_xml_request(update_request.get());

  xml_request.omaha_version = _T("1.2.3.4");
  xml_request.test_source = _T("dev");
  xml_request.request_id = _T("{387E2718-B39C-4458-98CC-24B5293C8383}");
  xml_request.os.platform = _T("win");
  xml_request.os.version = _T("6.0");
  xml_request.os.service_pack = _T("Service Pack 1");
  xml_request.os.arch = _T("x86");
  xml_request.check_period_sec = 100000;
  xml_request.uid.Empty();

  request::App app1;
  app1.app_id = _T("{8A69D345-D564-463C-AFF1-A69D9E530F96}");
  app1.lang = _T("en");
  app1.iid = GuidToString(GUID_NULL);  // Prevents assert.
  app1.ap = _T("ap_with_update_check");
  app1.update_check.is_valid = true;
  app1.data.install_data_index = _T("verboselogging");
  app1.ping.active = ACTIVE_NOTRUN;
  app1.ping.days_since_last_active_ping = -1;
  app1.ping.days_since_last_roll_call = 5;
  xml_request.apps.push_back(app1);

  request::App app2;
  app2.app_id = _T("{AD3D0CC0-AD1E-4b1f-B98E-BAA41DCE396C}");
  app2.lang = _T("en");
  app2.iid = GuidToString(GUID_NULL);  // Prevents assert.
  app2.version = _T("1.0");
  app2.next_version = _T("2.0");
  app2.ap = _T("ap_with_no_update_check");
  app2.experiments = _T("url_exp_2=a|Fri, 14 Aug 2015 16:13:03 GMT");
  xml_request.apps.push_back(app2);

  CString expected_buffer = _T("<?xml version=\"1.0\" encoding=\"UTF-8\"?><request protocol=\"3.0\" version=\"1.2.3.4\" ismachine=\"1\" sessionid=\"unittest_session\" installsource=\"unittest_install\" originurl=\"http://go/foo/&quot;\" testsource=\"dev\" requestid=\"{387E2718-B39C-4458-98CC-24B5293C8383}\" periodoverridesec=\"100000\"><os platform=\"win\" version=\"6.0\" sp=\"Service Pack 1\" arch=\"x86\"/><app appid=\"{8A69D345-D564-463C-AFF1-A69D9E530F96}\" version=\"\" nextversion=\"\" ap=\"ap_with_update_check\" lang=\"en\" brand=\"\" client=\"\"><updatecheck/><data name=\"install\" index=\"verboselogging\"/><ping active=\"0\" r=\"5\"/></app><app appid=\"{AD3D0CC0-AD1E-4b1f-B98E-BAA41DCE396C}\" version=\"1.0\" nextversion=\"2.0\" ap=\"ap_with_no_update_check\" lang=\"en\" brand=\"\" client=\"\" experiments=\"url_exp_2=a|Fri, 14 Aug 2015 16:13:03 GMT\"/></request>");  // NOLINT

  CString actual_buffer;
  EXPECT_HRESULT_SUCCEEDED(XmlParser::SerializeRequest(*update_request,
                                                       &actual_buffer));
  EXPECT_STREQ(expected_buffer, actual_buffer);
}

// Creates a machine update request and serializes it.
TEST_F(XmlParserTest, GenerateRequestWithUserId_MachineUpdateRequest) {
  // The origin URL contains an invalid XML character, the double-quote. The
  // expectation is that this character should be escaped to "&quot;".
  scoped_ptr<UpdateRequest> update_request(
      UpdateRequest::Create(true,
                            _T("unittest_session"),
                            _T("unittest_install"),
                            _T("http://go/bar/\"")));

  request::Request& xml_request = get_xml_request(update_request.get());

  xml_request.uid = _T("{c5bcb37e-47eb-4331-a544-2f31101951ab}");

  xml_request.omaha_version = _T("4.3.2.1");
  xml_request.test_source = _T("dev");
  xml_request.request_id = _T("{387E2718-B39C-4458-98CC-24B5293C8384}");
  xml_request.os.platform = _T("win");
  xml_request.os.version = _T("7.0");
  xml_request.os.service_pack = _T("Service Pack 2");
  xml_request.os.arch = _T("x64");
  xml_request.check_period_sec = 200000;

  request::App app1;
  app1.app_id = _T("{8A69D345-D564-463C-AFF1-A69D9E530F97}");
  app1.lang = _T("en");
  app1.iid = GuidToString(GUID_NULL);  // Prevents assert.
  app1.ap = _T("ap_with_update_check");
  app1.update_check.is_valid = true;
  app1.data.install_data_index = _T("verboselogging");
  app1.ping.active = ACTIVE_NOTRUN;
  app1.ping.days_since_last_active_ping = -1;
  app1.ping.days_since_last_roll_call = 5;
  xml_request.apps.push_back(app1);

  request::App app2;
  app2.app_id = _T("{AD3D0CC0-AD1E-4b1f-B98E-BAA41DCE396D}");
  app2.lang = _T("en");
  app2.iid = GuidToString(GUID_NULL);  // Prevents assert.
  app2.version = _T("1.0");
  app2.next_version = _T("2.0");
  app2.ap = _T("ap_with_no_update_check");
  app2.experiments = _T("url_exp_2=a|Fri, 14 Aug 2015 16:13:03 GMT");
  xml_request.apps.push_back(app2);

  CString expected_buffer = _T("<?xml version=\"1.0\" encoding=\"UTF-8\"?><request protocol=\"3.0\" version=\"4.3.2.1\" ismachine=\"1\" sessionid=\"unittest_session\" userid=\"{c5bcb37e-47eb-4331-a544-2f31101951ab}\" installsource=\"unittest_install\" originurl=\"http://go/bar/&quot;\" testsource=\"dev\" requestid=\"{387E2718-B39C-4458-98CC-24B5293C8384}\" periodoverridesec=\"200000\"><os platform=\"win\" version=\"7.0\" sp=\"Service Pack 2\" arch=\"x64\"/><app appid=\"{8A69D345-D564-463C-AFF1-A69D9E530F97}\" version=\"\" nextversion=\"\" ap=\"ap_with_update_check\" lang=\"en\" brand=\"\" client=\"\"><updatecheck/><data name=\"install\" index=\"verboselogging\"/><ping active=\"0\" r=\"5\"/></app><app appid=\"{AD3D0CC0-AD1E-4b1f-B98E-BAA41DCE396D}\" version=\"1.0\" nextversion=\"2.0\" ap=\"ap_with_no_update_check\" lang=\"en\" brand=\"\" client=\"\" experiments=\"url_exp_2=a|Fri, 14 Aug 2015 16:13:03 GMT\"/></request>");  // NOLINT

  CString actual_buffer;
  EXPECT_HRESULT_SUCCEEDED(XmlParser::SerializeRequest(*update_request,
                                                       &actual_buffer));
  EXPECT_STREQ(expected_buffer, actual_buffer);
}

// TODO(omaha3): Add a UserUpdateRequest test with more values (brand, etc.).

// Parses a response for one application.
TEST_F(XmlParserTest, Parse) {
  // Array of two request strings that are almost same except the second one
  // contains some unsupported elements that we expect to be ignored.
  CStringA buffer_strings[] = {
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?><response protocol=\"3.0\"><daystart elapsed_seconds=\"8400\" /><app appid=\"{8A69D345-D564-463C-AFF1-A69D9E530F96}\" status=\"ok\" experiments=\"url_exp_2=a|Fri, 14 Aug 2015 16:13:03 GMT\"><updatecheck status=\"ok\"><urls><url codebase=\"http://cache.pack.google.com/edgedl/chrome/install/172.37/\"/></urls><manifest version=\"2.0.172.37\"><packages><package hash=\"NT/6ilbSjWgbVqHZ0rT1vTg1coE=\" name=\"chrome_installer.exe\" required=\"true\" size=\"9614320\"/></packages><actions><action arguments=\"--do-not-launch-chrome\" event=\"install\" needsadmin=\"false\" run=\"chrome_installer.exe\"/><action event=\"postinstall\" onsuccess=\"exitsilentlyonlaunchcmd\"/></actions></manifest></updatecheck><data index=\"verboselogging\" name=\"install\" status=\"ok\">{\n \"distribution\": {\n   \"verbose_logging\": true\n }\n}\n</data><ping status=\"ok\"/></app></response>",  // NOLINT
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?><response protocol=\"3.0\" ExtraUnsupportedAttribute=\"123\"><daystart elapsed_seconds=\"8400\" /><UnsupportedElement1 UnsupportedAttribute1=\"some value\" /><app appid=\"{8A69D345-D564-463C-AFF1-A69D9E530F96}\" status=\"ok\" experiments=\"url_exp_2=a|Fri, 14 Aug 2015 16:13:03 GMT\"><updatecheck status=\"ok\"><urls><url codebase=\"http://cache.pack.google.com/edgedl/chrome/install/172.37/\"/></urls><manifest version=\"2.0.172.37\"><packages><package hash=\"NT/6ilbSjWgbVqHZ0rT1vTg1coE=\" name=\"chrome_installer.exe\" required=\"true\" size=\"9614320\"/></packages><actions><action arguments=\"--do-not-launch-chrome\" event=\"install\" needsadmin=\"false\" run=\"chrome_installer.exe\"/><action event=\"postinstall\" onsuccess=\"exitsilentlyonlaunchcmd\"/></actions></manifest></updatecheck><data index=\"verboselogging\" name=\"install\" status=\"ok\">{\n \"distribution\": {\n   \"verbose_logging\": true\n }\n}\n</data><ping status=\"ok\"/></app><UnsupportedElement2 UnsupportedAttribute2=\"Unsupported value\" >Some strings inside an unsupported element, should be ignored.<ping status=\"ok\"/></UnsupportedElement2></response>",  // NOLINT
  };

  for (int i = 0; i < arraysize(buffer_strings); i++) {
    std::vector<uint8> buffer(buffer_strings[i].GetLength());
    memcpy(&buffer.front(), buffer_strings[i], buffer.size());

    scoped_ptr<UpdateResponse> update_response(UpdateResponse::Create());
    EXPECT_HRESULT_SUCCEEDED(XmlParser::DeserializeResponse(
        buffer,
        update_response.get()));

    const response::Response& xml_response(update_response->response());

    EXPECT_STREQ(_T("3.0"), xml_response.protocol);
    EXPECT_EQ(1, xml_response.apps.size());

    const response::App& app(xml_response.apps[0]);
    EXPECT_STREQ(_T("{8A69D345-D564-463C-AFF1-A69D9E530F96}"), app.appid);
    EXPECT_STREQ(_T("ok"), app.status);
    EXPECT_STREQ(_T("url_exp_2=a|Fri, 14 Aug 2015 16:13:03 GMT"),
        app.experiments);

    const response::UpdateCheck& update_check(app.update_check);
    EXPECT_STREQ(_T("ok"), update_check.status);
    EXPECT_EQ(1, update_check.urls.size());
    EXPECT_STREQ(
        _T("http://cache.pack.google.com/edgedl/chrome/install/172.37/"),
        update_check.urls[0]);

    const InstallManifest& install_manifest(update_check.install_manifest);
    EXPECT_STREQ(_T("2.0.172.37"), install_manifest.version);
    EXPECT_EQ(1, install_manifest.packages.size());

    const InstallPackage& install_package(install_manifest.packages[0]);
    EXPECT_STREQ(_T("chrome_installer.exe"), install_package.name);
    EXPECT_TRUE(install_package.is_required);
    EXPECT_EQ(9614320, install_package.size);
    EXPECT_STREQ(_T("NT/6ilbSjWgbVqHZ0rT1vTg1coE="), install_package.hash);

    EXPECT_EQ(2, install_manifest.install_actions.size());

    const InstallAction* install_action(&install_manifest.install_actions[0]);
    EXPECT_EQ(InstallAction::kInstall, install_action->install_event);
    EXPECT_EQ(NEEDS_ADMIN_NO, install_action->needs_admin);
    EXPECT_STREQ(_T("chrome_installer.exe"), install_action->program_to_run);
    EXPECT_STREQ(_T("--do-not-launch-chrome"),
                 install_action->program_arguments);
    EXPECT_FALSE(install_action->terminate_all_browsers);
    EXPECT_EQ(SUCCESS_ACTION_DEFAULT, install_action->success_action);

    install_action = &install_manifest.install_actions[1];
    EXPECT_EQ(InstallAction::kPostInstall, install_action->install_event);
    EXPECT_EQ(NEEDS_ADMIN_NO, install_action->needs_admin);
    EXPECT_FALSE(install_action->terminate_all_browsers);
    EXPECT_EQ(SUCCESS_ACTION_EXIT_SILENTLY_ON_LAUNCH_CMD,
              install_action->success_action);

    EXPECT_EQ(0, app.events.size());

    CString value;
    EXPECT_SUCCEEDED(update_response_utils::GetInstallData(app.data,
                                                           _T("verboselogging"),
                                                           &value));
    EXPECT_STREQ(
        _T("{\n \"distribution\": {\n   \"verbose_logging\": true\n }\n}\n"),
        value);
  }
}

// Parses a response for one application.
TEST_F(XmlParserTest, Parse_InvalidDataStatusError) {
  CStringA buffer_string = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><response protocol=\"3.0\"><app appid=\"{8A69D345-D564-463C-AFF1-A69D9E530F96}\" status=\"ok\"><updatecheck status=\"ok\"><urls><url codebase=\"http://cache.pack.google.com/edgedl/chrome/install/172.37/\"/></urls><manifest version=\"2.0.172.37\"><packages><package hash=\"NT/6ilbSjWgbVqHZ0rT1vTg1coE=\" name=\"chrome_installer.exe\" required=\"false\" size=\"9614320\"/></packages><actions><action arguments=\"--do-not-launch-chrome\" event=\"install\" needsadmin=\"false\" run=\"chrome_installer.exe\"/><action event=\"postinstall\" onsuccess=\"exitsilentlyonlaunchcmd\"/></actions></manifest></updatecheck><data index=\"verboselog\" name=\"install\" status=\"error-nodata\"/><ping status=\"ok\"/></app></response>";  // NOLINT
  std::vector<uint8> buffer(buffer_string.GetLength());
  memcpy(&buffer.front(), buffer_string, buffer.size());

  scoped_ptr<UpdateResponse> update_response(UpdateResponse::Create());
  EXPECT_HRESULT_SUCCEEDED(XmlParser::DeserializeResponse(
      buffer,
      update_response.get()));
  const response::Response& xml_response(update_response->response());

  EXPECT_STREQ(_T("3.0"), xml_response.protocol);
  EXPECT_EQ(1, xml_response.apps.size());

  const response::App& app(xml_response.apps[0]);
  EXPECT_STREQ(_T("{8A69D345-D564-463C-AFF1-A69D9E530F96}"), app.appid);
  EXPECT_STREQ(_T("ok"), app.status);

  const response::UpdateCheck& update_check(app.update_check);
  EXPECT_STREQ(_T("ok"), update_check.status);
  EXPECT_EQ(1, update_check.urls.size());
  EXPECT_STREQ(_T("http://cache.pack.google.com/edgedl/chrome/install/172.37/"),
               update_check.urls[0]);

  const InstallManifest& install_manifest(update_check.install_manifest);
  EXPECT_STREQ(_T("2.0.172.37"), install_manifest.version);
  EXPECT_EQ(1, install_manifest.packages.size());

  const InstallPackage& install_package(install_manifest.packages[0]);
  EXPECT_STREQ(_T("chrome_installer.exe"), install_package.name);
  EXPECT_FALSE(install_package.is_required);
  EXPECT_EQ(9614320, install_package.size);
  EXPECT_STREQ(_T("NT/6ilbSjWgbVqHZ0rT1vTg1coE="), install_package.hash);

  EXPECT_EQ(2, install_manifest.install_actions.size());

  const InstallAction* install_action(&install_manifest.install_actions[0]);
  EXPECT_EQ(InstallAction::kInstall, install_action->install_event);
  EXPECT_EQ(NEEDS_ADMIN_NO, install_action->needs_admin);
  EXPECT_STREQ(_T("chrome_installer.exe"), install_action->program_to_run);
  EXPECT_STREQ(_T("--do-not-launch-chrome"), install_action->program_arguments);
  EXPECT_FALSE(install_action->terminate_all_browsers);
  EXPECT_EQ(SUCCESS_ACTION_DEFAULT, install_action->success_action);

  install_action = &install_manifest.install_actions[1];
  EXPECT_EQ(InstallAction::kPostInstall, install_action->install_event);
  EXPECT_EQ(NEEDS_ADMIN_NO, install_action->needs_admin);
  EXPECT_FALSE(install_action->terminate_all_browsers);
  EXPECT_EQ(SUCCESS_ACTION_EXIT_SILENTLY_ON_LAUNCH_CMD,
            install_action->success_action);

  EXPECT_EQ(0, app.events.size());

  CString value;
  EXPECT_EQ(GOOPDATE_E_INVALID_INSTALL_DATA_INDEX,
            update_response_utils::GetInstallData(app.data, _T("verboselog"),
                                                  &value));
}

}  // namespace xml

}  // namespace omaha
