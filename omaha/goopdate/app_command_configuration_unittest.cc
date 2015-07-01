// Copyright 2013 Google Inc.
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

#include <atlstr.h>
#include <atlsimpstr.h>
#include <windows.h>
#include <algorithm>
#include "base/scoped_ptr.h"
#include "omaha/base/app_util.h"
#include "omaha/base/file.h"
#include "omaha/base/scoped_any.h"
#include "omaha/base/scoped_ptr_address.h"
#include "omaha/goopdate/app_command_configuration.h"
#include "omaha/goopdate/app_command_test_base.h"

namespace omaha {

namespace {

const TCHAR* const kAppGuid1 = _T("{3B1A3CCA-0525-4418-93E6-A0DB3398EC9B}");
const TCHAR* const kAppGuid2 = _T("{81E5F427-8854-4c9a-A8D3-93F75F3D50DC}");

const TCHAR* const kCmdLineExit0 = _T("cmd.exe /c \"exit 0\"");

const TCHAR* const kCmdId1 = _T("command 1");
const TCHAR* const kCmdId2 = _T("command 2");
const TCHAR* const kCmdId3 = _T("command 3");

const DWORD kZero = 0;
const DWORD kOne = 1;
const DWORD kTwo = 2;

}  // namespace

class AppCommandConfigurationTest : public AppCommandTestBase {
 protected:
  typedef bool(AppCommandConfiguration::*BoolMember)(void) const;
  typedef int(AppCommandConfiguration::*IntMember)(void) const;

  void TestValueMapping(const TCHAR* name, BoolMember mapped_member) {
     scoped_ptr<AppCommandConfiguration> configuration;
     CreateCommand(kAppGuid1, true, kCmdId1, kCmdLineExit0);

     SetCommandValue(kAppGuid1, true, kCmdId1, name, &kZero);
     ASSERT_HRESULT_SUCCEEDED(AppCommandConfiguration::Load(
         kAppGuid1, true, kCmdId1, address(configuration)));
     ASSERT_FALSE((configuration.get()->*mapped_member)());
     configuration.reset();

     SetCommandValue(kAppGuid1, true, kCmdId1, name, &kOne);
     ASSERT_HRESULT_SUCCEEDED(AppCommandConfiguration::Load(
         kAppGuid1, true, kCmdId1, address(configuration)));
     ASSERT_TRUE((configuration.get()->*mapped_member)());
     configuration.reset();

     SetCommandValue(kAppGuid1, true, kCmdId1, name, NULL);
     ASSERT_HRESULT_SUCCEEDED(AppCommandConfiguration::Load(
         kAppGuid1, true, kCmdId1, address(configuration)));
     ASSERT_FALSE((configuration.get()->*mapped_member)());
  }

  void TestValueMapping(const TCHAR* name, IntMember mapped_member) {
     scoped_ptr<AppCommandConfiguration> configuration;
     CreateCommand(kAppGuid1, true, kCmdId1, kCmdLineExit0);

     SetCommandValue(kAppGuid1, true, kCmdId1, name, &kZero);
     ASSERT_HRESULT_SUCCEEDED(AppCommandConfiguration::Load(
         kAppGuid1, true, kCmdId1, address(configuration)));
     ASSERT_EQ(0, (configuration.get()->*mapped_member)());
     configuration.reset();

     SetCommandValue(kAppGuid1, true, kCmdId1, name, &kOne);
     ASSERT_HRESULT_SUCCEEDED(AppCommandConfiguration::Load(
         kAppGuid1, true, kCmdId1, address(configuration)));
     ASSERT_EQ(1, (configuration.get()->*mapped_member)());
     configuration.reset();

     SetCommandValue(kAppGuid1, true, kCmdId1, name, &kTwo);
     ASSERT_HRESULT_SUCCEEDED(AppCommandConfiguration::Load(
         kAppGuid1, true, kCmdId1, address(configuration)));
     ASSERT_EQ(2, (configuration.get()->*mapped_member)());
     configuration.reset();

     SetCommandValue(kAppGuid1, true, kCmdId1, name, NULL);
     ASSERT_HRESULT_SUCCEEDED(AppCommandConfiguration::Load(
         kAppGuid1, true, kCmdId1, address(configuration)));
     ASSERT_EQ(0, (configuration.get()->*mapped_member)());
  }
};

TEST_F(AppCommandConfigurationTest, NoApp) {
  scoped_ptr<AppCommandConfiguration> configuration;
  ASSERT_HRESULT_FAILED(AppCommandConfiguration::Load(
      kAppGuid1, false, kCmdId1, address(configuration)));
}

TEST_F(AppCommandConfigurationTest, NoCmd) {
  scoped_ptr<AppCommandConfiguration> configuration;
  CreateAppClientKey(kAppGuid1, false);
  CreateCommand(kAppGuid1, false, kCmdId1, kCmdLineExit0);

  ASSERT_HRESULT_FAILED(AppCommandConfiguration::Load(
      kAppGuid1, false, kCmdId2, address(configuration)));
}

TEST_F(AppCommandConfigurationTest, WrongLevel) {
  scoped_ptr<AppCommandConfiguration> configuration;
  CreateAppClientKey(kAppGuid1, true);
  CreateCommand(kAppGuid1, true, kCmdId1, kCmdLineExit0);

  ASSERT_HRESULT_FAILED(AppCommandConfiguration::Load(
      kAppGuid1, false, kCmdId1, address(configuration)));
}

TEST_F(AppCommandConfigurationTest, LoadCommand) {
  scoped_ptr<AppCommandConfiguration> configuration;
  CreateAppClientKey(kAppGuid1, true);
  CreateCommand(kAppGuid1, true, kCmdId1, kCmdLineExit0);

  ASSERT_HRESULT_SUCCEEDED(AppCommandConfiguration::Load(
      kAppGuid1, true, kCmdId1, address(configuration)));
  ASSERT_EQ(kCmdLineExit0, configuration->command_line());
}

TEST_F(AppCommandConfigurationTest, MemberMappings) {
  TestValueMapping(kRegValueSendsPings, &AppCommandConfiguration::sends_pings);
  TestValueMapping(kRegValueWebAccessible,
                   &AppCommandConfiguration::is_web_accessible);
  TestValueMapping(kRegValueReportingId,
                   &AppCommandConfiguration::reporting_id);
  TestValueMapping(kRegValueCaptureOutput,
                   &AppCommandConfiguration::capture_output);
  TestValueMapping(kRegValueRunAsUser,
                   &AppCommandConfiguration::run_as_user);
  TestValueMapping(kRegValueAutoRunOnOSUpgrade,
                   &AppCommandConfiguration::auto_run_on_os_upgrade);
}

TEST_F(AppCommandConfigurationTest, EnumCommandsForApp_NoApp) {
  const bool is_machine = false;

  // It'd be nice to report this as a failure, but at the moment, we treat
  // it identically to a key that has no Commands subkey.
  std::vector<CString> commands;
  EXPECT_HRESULT_SUCCEEDED(
      AppCommandConfiguration::EnumCommandsForApp(false, kAppGuid1, &commands));
  EXPECT_TRUE(commands.empty());
}

TEST_F(AppCommandConfigurationTest, EnumCommandsForApp_NoCommands) {
  const bool is_machine = false;
  CreateAppClientKey(kAppGuid1, is_machine);

  std::vector<CString> commands;
  EXPECT_HRESULT_SUCCEEDED(
      AppCommandConfiguration::EnumCommandsForApp(false, kAppGuid1, &commands));
  EXPECT_TRUE(commands.empty());
}

TEST_F(AppCommandConfigurationTest, EnumCommandsForApp_CommandsEmpty) {
  const bool is_machine = false;
  CreateAppClientKey(kAppGuid1, is_machine);
  CreateEmptyCommandKey(kAppGuid1, is_machine);

  std::vector<CString> commands;
  EXPECT_HRESULT_SUCCEEDED(
      AppCommandConfiguration::EnumCommandsForApp(false, kAppGuid1, &commands));
  EXPECT_TRUE(commands.empty());
}

TEST_F(AppCommandConfigurationTest, EnumCommandsForApp_LegacyCommandsOnly) {
  const bool is_machine = false;
  CreateAppClientKey(kAppGuid1, is_machine);
  CreateLegacyCommand(kAppGuid1, is_machine, kCmdId1, kCmdLineExit0);

  std::vector<CString> commands;
  EXPECT_HRESULT_SUCCEEDED(
      AppCommandConfiguration::EnumCommandsForApp(false, kAppGuid1, &commands));
  EXPECT_TRUE(commands.empty());
}

TEST_F(AppCommandConfigurationTest,
       EnumCommandsForApp_CommandsEmptyAndLegacyCommands) {
  const bool is_machine = false;
  CreateAppClientKey(kAppGuid1, is_machine);
  CreateEmptyCommandKey(kAppGuid1, is_machine);
  CreateLegacyCommand(kAppGuid1, is_machine, kCmdId1, kCmdLineExit0);

  std::vector<CString> commands;
  EXPECT_HRESULT_SUCCEEDED(
      AppCommandConfiguration::EnumCommandsForApp(false, kAppGuid1, &commands));
  EXPECT_TRUE(commands.empty());
}

TEST_F(AppCommandConfigurationTest, EnumCommandsForApp_Normal) {
  const bool is_machine = false;
  CreateAppClientKey(kAppGuid1, is_machine);
  CreateCommand(kAppGuid1, is_machine, kCmdId1, kCmdLineExit0);
  CreateCommand(kAppGuid1, is_machine, kCmdId2, kCmdLineExit0);
  CreateLegacyCommand(kAppGuid1, is_machine, kCmdId3, kCmdLineExit0);

  std::vector<CString> commands;
  ASSERT_HRESULT_SUCCEEDED(
      AppCommandConfiguration::EnumCommandsForApp(false, kAppGuid1, &commands));
  ASSERT_EQ(2, commands.size());
  std::sort(commands.begin(), commands.end());
  EXPECT_STREQ(kCmdId1, commands[0]);
  EXPECT_STREQ(kCmdId2, commands[1]);
}

TEST_F(AppCommandConfigurationTest, EnumAllCommands_NoApps) {
  const bool is_machine = false;

  std::map<CString, std::vector<CString>> commands;
  ASSERT_HRESULT_FAILED(
      AppCommandConfiguration::EnumAllCommands(is_machine, &commands));
  EXPECT_TRUE(commands.empty());
}

TEST_F(AppCommandConfigurationTest, EnumAllCommands_NoAppsWithCommands) {
  const bool is_machine = false;
  CreateAppClientKey(kAppGuid1, is_machine);
  CreateAppClientKey(kAppGuid2, is_machine);

  std::map<CString, std::vector<CString>> commands;
  ASSERT_HRESULT_SUCCEEDED(
      AppCommandConfiguration::EnumAllCommands(is_machine, &commands));
  EXPECT_TRUE(commands.empty());
}

TEST_F(AppCommandConfigurationTest, EnumAllCommands_OneAppWithCommands) {
  const bool is_machine = false;
  CreateAppClientKey(kAppGuid1, is_machine);
  CreateAppClientKey(kAppGuid2, is_machine);
  CreateCommand(kAppGuid1, is_machine, kCmdId1, kCmdLineExit0);

  std::map<CString, std::vector<CString>> commands;
  ASSERT_HRESULT_SUCCEEDED(
      AppCommandConfiguration::EnumAllCommands(is_machine, &commands));

  ASSERT_EQ(1, commands.size());
  ASSERT_TRUE(commands.find(kAppGuid1) != commands.end());
  ASSERT_TRUE(commands.find(kAppGuid2) == commands.end());

  const std::vector<CString>& appcommands = commands[kAppGuid1];
  ASSERT_EQ(1, appcommands.size());
  EXPECT_STREQ(kCmdId1, appcommands[0]);
}

TEST_F(AppCommandConfigurationTest, EnumAllCommands_TwoAppsWithCommands) {
  const bool is_machine = false;
  CreateAppClientKey(kAppGuid1, is_machine);
  CreateAppClientKey(kAppGuid2, is_machine);
  CreateCommand(kAppGuid1, is_machine, kCmdId1, kCmdLineExit0);
  CreateCommand(kAppGuid2, is_machine, kCmdId2, kCmdLineExit0);
  CreateCommand(kAppGuid2, is_machine, kCmdId3, kCmdLineExit0);

  std::map<CString, std::vector<CString>> commands;
  ASSERT_HRESULT_SUCCEEDED(
      AppCommandConfiguration::EnumAllCommands(is_machine, &commands));

  ASSERT_EQ(2, commands.size());
  ASSERT_TRUE(commands.find(kAppGuid1) != commands.end());
  ASSERT_TRUE(commands.find(kAppGuid2) != commands.end());

  std::vector<CString>& appcommands1 = commands[kAppGuid1];
  ASSERT_EQ(1, appcommands1.size());
  EXPECT_STREQ(kCmdId1, appcommands1[0]);

  std::vector<CString>& appcommands2 = commands[kAppGuid2];
  ASSERT_EQ(2, appcommands2.size());
  std::sort(appcommands2.begin(), appcommands2.end());
  EXPECT_STREQ(kCmdId2, appcommands2[0]);
  EXPECT_STREQ(kCmdId3, appcommands2[1]);
}

}  // namespace omaha
