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

#include <io.h>
#include <stdio.h>
#include <atlstr.h>
#include <atlsimpstr.h>
#include <windows.h>
#include "base/scoped_ptr.h"
#include "omaha/base/app_util.h"
#include "omaha/base/file.h"
#include "omaha/base/reg_key.h"
#include "omaha/base/scope_guard.h"
#include "omaha/base/scoped_any.h"
#include "omaha/base/scoped_ptr_address.h"
#include "omaha/base/utils.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/goopdate/app_command.h"
#include "omaha/goopdate/app_unittest_base.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace {

const TCHAR* const kAppGuid1 = _T("{3B1A3CCA-0525-4418-93E6-A0DB3398EC9B}");

const TCHAR* const kCmdLineExit0 = _T("cmd.exe /c \"exit 0\"");

const TCHAR* const kCmdId1 = _T("command one");
const TCHAR* const kCmdId2 = _T("command two");

const TCHAR* const kSessionId = _T("unittest_session_id");

const bool kTrue = true;
const bool kFalse = false;

const DWORD kOne = 1;
const DWORD kTwo = 2;

}  // namespace

CString GetEchoCommandLine(CString string, CString output_file) {
  CString command_line;
  _sntprintf_s(CStrBuf(command_line, MAX_PATH),
               MAX_PATH,
               _TRUNCATE,
               _T("cmd.exe /c \"echo %s > \"%s\"\""),
               static_cast<const TCHAR*>(string),
               static_cast<const TCHAR*>(output_file));
  return command_line;
}

class AppCommandTest : public AppTestBaseWithRegistryOverride {
 protected:
  // false == is_machine
  AppCommandTest() : AppTestBaseWithRegistryOverride(false, true) {}

  static void CreateAppClientKey(const CString& guid, bool is_machine) {
    CString client_key_name = AppendRegKeyPath(
        ConfigManager::Instance()->registry_clients(is_machine), guid);

    RegKey client_key;

    ASSERT_SUCCEEDED(client_key.Create(client_key_name));
    ASSERT_SUCCEEDED(client_key.SetValue(kRegValueProductVersion,
                                         _T("1.1.1.3")));
    ASSERT_SUCCEEDED(client_key.SetValue(kRegValueAppName,
                                         _T("Dispay Name of ") + guid));
  }

  static void CreateLegacyCommand(const CString& guid,
                                  bool is_machine,
                                  const CString& cmd_id,
                                  const CString& cmd_line) {
    CString client_key_name = AppendRegKeyPath(
        ConfigManager::Instance()->registry_clients(is_machine), guid);

    RegKey client_key;

    ASSERT_SUCCEEDED(client_key.Create(client_key_name));
    ASSERT_SUCCEEDED(client_key.SetValue(cmd_id, cmd_line));
  }

  static void CreateCommand(const CString& guid,
                            bool is_machine,
                            const CString& cmd_id,
                            const CString& cmd_line,
                            const bool* sends_pings,
                            const bool* is_web_accessible,
                            const DWORD* reporting_id) {
    CString client_key_name = AppendRegKeyPath(
        ConfigManager::Instance()->registry_clients(is_machine), guid);

    CString command_key_name = AppendRegKeyPath(client_key_name,
                                                kCommandsRegKeyName,
                                                cmd_id);

    RegKey command_key;

    ASSERT_SUCCEEDED(command_key.Create(command_key_name));
    ASSERT_SUCCEEDED(command_key.SetValue(kRegValueCommandLine, cmd_line));
    if (sends_pings != NULL) {
      ASSERT_SUCCEEDED(command_key.SetValue(
          kRegValueSendsPings, static_cast<DWORD>(*sends_pings ? 1 : 0)));
    }
    if (is_web_accessible != NULL) {
      ASSERT_SUCCEEDED(command_key.SetValue(
          kRegValueWebAccessible,
          static_cast<DWORD>(*is_web_accessible ? 1 : 0)));
    }
    if (reporting_id != NULL) {
      ASSERT_SUCCEEDED(command_key.SetValue(kRegValueReportingId,
                                            *reporting_id));
    }
  }
};  // class AppCommandTest

TEST_F(AppCommandTest, NoApp) {
  scoped_ptr<AppCommand> app_command;
  ASSERT_FAILED(AppCommand::Load(
      kAppGuid1, false, kCmdId1, kSessionId, address(app_command)));
}

TEST_F(AppCommandTest, NoCmd) {
  scoped_ptr<AppCommand> app_command;
  CreateAppClientKey(kAppGuid1, false);
  CreateCommand(
      kAppGuid1, false, kCmdId1, kCmdLineExit0, &kTrue, &kFalse, &kOne);

  ASSERT_FAILED(AppCommand::Load(
      kAppGuid1, false, kCmdId2, kSessionId, address(app_command)));
}

TEST_F(AppCommandTest, WrongLevel) {
  scoped_ptr<AppCommand> app_command;
  CreateAppClientKey(kAppGuid1, true);
  CreateCommand(
      kAppGuid1, true, kCmdId1, kCmdLineExit0, &kTrue, &kFalse, &kOne);

  ASSERT_FAILED(AppCommand::Load(
      kAppGuid1, false, kCmdId1, kSessionId, address(app_command)));
}

TEST_F(AppCommandTest, LoadCommand) {
  scoped_ptr<AppCommand> app_command;
  CreateAppClientKey(kAppGuid1, true);
  CreateCommand(
      kAppGuid1, true, kCmdId1, kCmdLineExit0, &kTrue, &kFalse, &kOne);

  ASSERT_SUCCEEDED(AppCommand::Load(
      kAppGuid1, true, kCmdId1, kSessionId, address(app_command)));
  ASSERT_FALSE(app_command->is_web_accessible());
}

TEST_F(AppCommandTest, LoadCommandDefaultValues) {
  scoped_ptr<AppCommand> app_command;
  CreateAppClientKey(kAppGuid1, true);
  CreateCommand(
      kAppGuid1, true, kCmdId1, kCmdLineExit0, NULL, NULL, NULL);

  ASSERT_SUCCEEDED(AppCommand::Load(
      kAppGuid1, true, kCmdId1, kSessionId, address(app_command)));
  ASSERT_FALSE(app_command->is_web_accessible());
}

TEST_F(AppCommandTest, LoadWebAccessibleCommand) {
  scoped_ptr<AppCommand> app_command;
  CreateAppClientKey(kAppGuid1, true);
  CreateCommand(
      kAppGuid1, true, kCmdId1, kCmdLineExit0, NULL, &kTrue, NULL);

  ASSERT_SUCCEEDED(AppCommand::Load(
      kAppGuid1, true, kCmdId1, kSessionId, address(app_command)));
  ASSERT_TRUE(app_command->is_web_accessible());
}

TEST_F(AppCommandTest, Execute) {
  CString temp_file;
  ASSERT_TRUE(::GetTempFileName(app_util::GetTempDir(),
                                _T("omaha"),
                                0,
                                CStrBuf(temp_file, MAX_PATH)));

  // GetTempFileName created an empty file. Delete it.
  ASSERT_EQ(0, _tunlink(temp_file));

  // Hopefully we will cause the file to be created. Cause its deletion at exit.
  ON_SCOPE_EXIT(_tunlink, temp_file);

  CString command_line = GetEchoCommandLine(_T("hello world!"), temp_file);

  scoped_ptr<AppCommand> app_command;
  CreateAppClientKey(kAppGuid1, true);
  CreateCommand(
      kAppGuid1, true, kCmdId1, command_line, &kTrue, &kTrue, NULL);

  ASSERT_SUCCEEDED(AppCommand::Load(
      kAppGuid1, true, kCmdId1, kSessionId, address(app_command)));

  scoped_process process;
  ASSERT_SUCCEEDED(app_command->Execute(address(process)));
  ASSERT_EQ(WAIT_OBJECT_0, WaitForSingleObject(get(process), 16 * kMsPerSec));

  ASSERT_TRUE(File::Exists(temp_file));
}

}  // namespace omaha
