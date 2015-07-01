// Copyright 2012 Google Inc.
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

#include <atlbase.h>
#include "omaha/goopdate/app_command_model.h"
#include "omaha/goopdate/app_command_test_base.h"

namespace omaha {

namespace {

const TCHAR* const kAppGuid1 = _T("{3B1A3CCA-0525-4418-93E6-A0DB3398EC9B}");
const TCHAR* const kCmdLineExit0 = _T("cmd.exe /c \"exit 0\"");

const TCHAR* const kCmdId1 = _T("command one");
const TCHAR* const kCmdId2 = _T("command two");

}  // namespace

class AppCommandModelTest : public AppCommandTestBase { };

TEST_F(AppCommandModelTest, NoApp) {
  App* app = NULL;
  ASSERT_SUCCEEDED(app_bundle_->createApp(CComBSTR(kAppGuid1), &app));
  AppCommandModel* app_command = app->command(kCmdId1);
  ASSERT_TRUE(NULL == app_command);
}

TEST_F(AppCommandModelTest, NoCmd) {
  CreateAppClientKey(kAppGuid1, false);
  CreateCommand(kAppGuid1, false, kCmdId1, kCmdLineExit0);

  App* app = NULL;
  ASSERT_SUCCEEDED(app_bundle_->createInstalledApp(CComBSTR(kAppGuid1), &app));
  AppCommandModel* app_command = app->command(kCmdId2);
  ASSERT_TRUE(NULL == app_command);
}

TEST_F(AppCommandModelTest, WrongLevel) {
  CreateAppClientKey(kAppGuid1, false);
  CreateAppClientKey(kAppGuid1, true);
  CreateCommand(kAppGuid1, true, kCmdId1, kCmdLineExit0);

  App* app = NULL;
  ASSERT_SUCCEEDED(app_bundle_->createInstalledApp(CComBSTR(kAppGuid1), &app));
  AppCommandModel* app_command = app->command(kCmdId1);
  ASSERT_TRUE(NULL == app_command);
}

TEST_F(AppCommandModelTest, LoadCommand) {
  CreateAppClientKey(kAppGuid1, false);
  CreateCommand(kAppGuid1, false, kCmdId1, kCmdLineExit0);

  App* app = NULL;
  ASSERT_SUCCEEDED(app_bundle_->createInstalledApp(CComBSTR(kAppGuid1), &app));
  AppCommandModel* app_command = app->command(kCmdId1);
  ASSERT_TRUE(NULL != app_command);

  ASSERT_EQ(COMMAND_STATUS_INIT, app_command->GetStatus());
  ASSERT_EQ(MAXDWORD, app_command->GetExitCode());
}

}  // namespace omaha
