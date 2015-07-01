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

#include "omaha/goopdate/app_command_test_base.h"

#include "omaha/base/reg_key.h"
#include "omaha/base/utils.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/const_goopdate.h"

namespace omaha {

namespace {
CString GetCommandKeyName(const CString& guid,
                          bool is_machine,
                          const CString& cmd_id) {
  CString client_key_name = AppendRegKeyPath(
      ConfigManager::Instance()->registry_clients(is_machine), guid);

  return AppendRegKeyPath(client_key_name, kCommandsRegKeyName, cmd_id);
}
}  // namespace

AppCommandTestBase::AppCommandTestBase()
    : AppTestBaseWithRegistryOverride(false, true) {
}

void AppCommandTestBase::CreateAppClientKey(const CString& guid,
                                            bool is_machine) {
  CString client_key_name = AppendRegKeyPath(
      ConfigManager::Instance()->registry_clients(is_machine), guid);

  RegKey client_key;

  ASSERT_SUCCEEDED(client_key.Create(client_key_name));
  ASSERT_SUCCEEDED(client_key.SetValue(kRegValueProductVersion,
                                         _T("1.1.1.3")));
  ASSERT_SUCCEEDED(client_key.SetValue(kRegValueAppName,
                                       _T("Dispay Name of ") + guid));
}

void AppCommandTestBase::DeleteAppClientKey(const CString& guid,
                                            bool is_machine) {
  CString client_key_name = AppendRegKeyPath(
      ConfigManager::Instance()->registry_clients(is_machine), guid);
  ASSERT_SUCCEEDED(RegKey::DeleteKey(client_key_name, true));
}

void AppCommandTestBase::CreateLegacyCommand(const CString& guid,
                                             bool is_machine,
                                             const CString& cmd_id,
                                             const CString& cmd_line) {
  CString client_key_name = AppendRegKeyPath(
      ConfigManager::Instance()->registry_clients(is_machine), guid);

  RegKey client_key;

  ASSERT_SUCCEEDED(client_key.Create(client_key_name));
  ASSERT_SUCCEEDED(client_key.SetValue(cmd_id, cmd_line));
}

void AppCommandTestBase::CreateAutoRunOnOSUpgradeCommand(
    const CString& guid,
    bool is_machine,
    const CString& cmd_id,
    const CString& cmd_line) {
  RegKey command_key;
  ASSERT_SUCCEEDED(
      command_key.Create(GetCommandKeyName(guid, is_machine, cmd_id)));
  ASSERT_SUCCEEDED(command_key.SetValue(kRegValueCommandLine, cmd_line));
  ASSERT_SUCCEEDED(command_key.SetValue(kRegValueAutoRunOnOSUpgrade,
                                        static_cast<DWORD>(1)));
}

void AppCommandTestBase::CreateCommand(const CString& guid,
                                       bool is_machine,
                                       const CString& cmd_id,
                                       const CString& cmd_line) {
  RegKey command_key;
  ASSERT_SUCCEEDED(
      command_key.Create(GetCommandKeyName(guid, is_machine, cmd_id)));
  ASSERT_SUCCEEDED(command_key.SetValue(kRegValueCommandLine, cmd_line));
}

void AppCommandTestBase::SetCommandValue(const CString& guid,
                                         bool is_machine,
                                         const CString& cmd_id,
                                         const TCHAR* name,
                                         const DWORD* value) {
  RegKey command_key;
  ASSERT_SUCCEEDED(
      command_key.Open(GetCommandKeyName(guid, is_machine, cmd_id)));

  if (value != NULL) {
    ASSERT_SUCCEEDED(command_key.SetValue(name, *value));
  } else if (command_key.HasValue(name)) {
    ASSERT_SUCCEEDED(command_key.DeleteValue(name));
  }
}

void AppCommandTestBase::CreateEmptyCommandKey(const CString& guid,
                                               bool is_machine) {
  CString command_key_name = AppendRegKeyPath(
      ConfigManager::Instance()->registry_clients(is_machine),
      guid,
      kCommandsRegKeyName);

  ASSERT_SUCCEEDED(RegKey::CreateKey(command_key_name));
}

}  // namespace omaha
