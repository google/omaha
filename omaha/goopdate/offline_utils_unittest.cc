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

#include "omaha/goopdate/offline_utils.h"

#include <atlpath.h>

#include "omaha/base/app_util.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/file.h"
#include "omaha/base/logging.h"
#include "omaha/base/path.h"
#include "omaha/base/utils.h"
#include "omaha/goopdate/update_response_utils.h"
#include "omaha/testing/resource.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace {

const TCHAR* kAppId1 = _T("{CDABE316-39CD-43BA-8440-6D1E0547AEE6}");

void CheckResponse(const xml::response::Response& xml_response,
                   const TCHAR* expected_protocol_version) {
  EXPECT_STREQ(expected_protocol_version, xml_response.protocol);
  EXPECT_EQ(1, xml_response.apps.size());

  const xml::response::App& app(xml_response.apps[0]);
  EXPECT_STREQ(kAppId1, app.appid);
  EXPECT_STREQ(_T("ok"), app.status);

  const xml::response::UpdateCheck& update_check(app.update_check);
  EXPECT_STREQ(_T("ok"), update_check.status);
  EXPECT_EQ(1, update_check.urls.size());
  EXPECT_STREQ(_T("http://dl.google.com/foo/install/1.2.3.4/"),
               update_check.urls[0]);

  const xml::InstallManifest& install_manifest(update_check.install_manifest);
  EXPECT_STREQ(_T("1.2.3.4"), install_manifest.version);
  EXPECT_EQ(1, install_manifest.packages.size());

  const xml::InstallPackage& install_package(install_manifest.packages[0]);
  EXPECT_STREQ(_T("foo_installer.exe"), install_package.name);
  EXPECT_TRUE(install_package.is_required);
  EXPECT_EQ(12345678, install_package.size);
  EXPECT_STREQ(_T("abcdef"), install_package.hash_sha1);
  if (CString(expected_protocol_version) == _T("2.0")) {
    EXPECT_TRUE(install_package.hash_sha256.IsEmpty());
  } else {
    EXPECT_STREQ(_T("sha256hash_foobar"), install_package.hash_sha256);
  }

  EXPECT_EQ(2, install_manifest.install_actions.size());

  const xml::InstallAction* install_action(
      &install_manifest.install_actions[0]);
  EXPECT_EQ(xml::InstallAction::kInstall, install_action->install_event);
  EXPECT_EQ(NEEDS_ADMIN_NO, install_action->needs_admin);
  EXPECT_STREQ(_T("foo_installer.exe"), install_action->program_to_run);
  EXPECT_STREQ(_T("-baz"), install_action->program_arguments);
  EXPECT_FALSE(install_action->terminate_all_browsers);
  EXPECT_EQ(SUCCESS_ACTION_DEFAULT, install_action->success_action);

  install_action = &install_manifest.install_actions[1];
  EXPECT_EQ(xml::InstallAction::kPostInstall, install_action->install_event);
  EXPECT_EQ(NEEDS_ADMIN_NO, install_action->needs_admin);
  EXPECT_FALSE(install_action->terminate_all_browsers);
  EXPECT_EQ(SUCCESS_ACTION_EXIT_SILENTLY_ON_LAUNCH_CMD,
            install_action->success_action);

  EXPECT_EQ(0, app.events.size());

  CString value;
  CString verboselogging_install_data(_T("\n      {\n        \"distribution\": {\n          \"verbose_logging\": true\n        }\n      }\n    "));  // NOLINT
  EXPECT_SUCCEEDED(update_response_utils::GetInstallData(app.data,
                                                         _T("verboselogging"),
                                                         &value));
  EXPECT_STREQ(verboselogging_install_data, value);

  CString foobarapp_install_data(_T("\n      {\n        \"distribution\": {\n          \"skip_first_run_ui\": true,\n          \"show_welcome_page\": true,\n          \"import_search_engine\": true,\n          \"import_history\": false,\n          \"create_all_shortcuts\": true,\n          \"do_not_launch_foo\": true,\n          \"make_foo_default\": false,\n          \"verbose_logging\": false\n        }\n      }\n    "));  // NOLINT
  EXPECT_SUCCEEDED(update_response_utils::GetInstallData(app.data,
                                                         _T("foobarapp"),
                                                         &value));
  EXPECT_STREQ(foobarapp_install_data, value);

  EXPECT_EQ(GOOPDATE_E_INVALID_INSTALL_DATA_INDEX,
            update_response_utils::GetInstallData(app.data, _T("foo"), &value));
}

void ParseAndCheck(const TCHAR* source_manifest_extension,
                   const TCHAR* target_manifest_filename,
                   const TCHAR* expected_protocol_version) {
  CString source_manifest_path = ConcatenatePath(
      app_util::GetCurrentModuleDirectory(), _T("unittest_support"));
  source_manifest_path = ConcatenatePath(source_manifest_path, kAppId1);
  source_manifest_path += source_manifest_extension;

  CString target_manifest_path = ConcatenatePath(
      app_util::GetCurrentModuleDirectory(), target_manifest_filename);

  EXPECT_SUCCEEDED(File::Copy(source_manifest_path, target_manifest_path,
                              true));

  std::unique_ptr<xml::UpdateResponse> update_response(
      xml::UpdateResponse::Create());
  EXPECT_SUCCEEDED(offline_utils::ParseOfflineManifest(
                       kAppId1,
                       app_util::GetCurrentModuleDirectory(),
                       update_response.get()));

  CheckResponse(update_response->response(), expected_protocol_version);

  EXPECT_SUCCEEDED(File::Remove(target_manifest_path));
}

}  // namespace

namespace offline_utils {

TEST(OfflineUtilsTest, GetV2OfflineManifest) {
  CString manifest_path = offline_utils::GetV2OfflineManifest(
      kAppId1, app_util::GetCurrentModuleDirectory());

  EXPECT_STREQ(ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                               CString(kAppId1) + _T(".gup")),
               manifest_path);
}

TEST(OfflineUtilsTest, FindV2OfflinePackagePath_Success) {
  CString installer_exe = _T("foo_installer.exe");
  CString installer_path = ConcatenatePath(
                                app_util::GetCurrentModuleDirectory(),
                                kAppId1);
  EXPECT_SUCCEEDED(CreateDir(installer_path, NULL));
  EXPECT_SUCCEEDED(File::Copy(
      ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                      _T("unittest_support\\SaveArguments.exe")),
      ConcatenatePath(installer_path, installer_exe),
      true));

  CString package_path;
  EXPECT_SUCCEEDED(offline_utils::FindV2OfflinePackagePath(installer_path,
                                                           &package_path));
  EXPECT_STREQ(ConcatenatePath(installer_path, installer_exe), package_path);

  EXPECT_SUCCEEDED(DeleteDirectory(installer_path));
}

TEST(OfflineUtilsTest, FindV2OfflinePackagePath_Failure) {
  CString package_path;
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND),
            offline_utils::FindV2OfflinePackagePath(
                ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                                kAppId1),
                &package_path));
  EXPECT_TRUE(package_path.IsEmpty());
}

TEST(OfflineUtilsTest, ParseOfflineManifest_v3_Success) {
  ParseAndCheck(_T(".v3.gup"), kOfflineManifestFileName, _T("3.0"));
}

TEST(OfflineUtilsTest, ParseOfflineManifest_v2_Success) {
  CString target_manifest_filename = CString(kAppId1) + _T(".gup");
  ParseAndCheck(_T(".v2.gup"), target_manifest_filename, _T("2.0"));
}

TEST(OfflineUtilsTest, ParseOfflineManifest_FileDoesNotExist) {
  std::unique_ptr<xml::UpdateResponse> update_response(
      xml::UpdateResponse::Create());
  EXPECT_EQ(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
            offline_utils::ParseOfflineManifest(
                               kAppId1,
                               app_util::GetCurrentModuleDirectory(),
                               update_response.get()));
}

}  // namespace offline_utils

}  // namespace omaha
