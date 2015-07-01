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

#include "omaha/goopdate/app_command_configuration.h"

#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/reg_key.h"
#include "omaha/base/logging.h"
#include "omaha/base/utils.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/goopdate/app_command.h"
#include "omaha/goopdate/app_command_ping_delegate.h"

namespace omaha {

namespace {

// Attempts to read the command line from the given registry key and value.
// Logs a message in case of failure.
HRESULT ReadCommandLine(const CString& key_name,
                        const CString& value_name,
                        CString* command_line) {
  HRESULT hr = RegKey::GetValue(key_name, value_name, command_line);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[failed to read command line]")
                  _T("[key %s][value %s][0x%08x]"), key_name, value_name, hr));
  }

  return hr;
}

// Checks if the specified value exists in the registry under the specified key.
// If so, attempts to read the value's DWORD contents into 'parameter'. Succeeds
// if the value is absent or a DWORD value is successfully read.
HRESULT ReadCommandParameter(const CString& key_name,
                             const CString& value_name,
                             DWORD* parameter) {
  if (!RegKey::HasValue(key_name, value_name)) {
    return S_OK;
  }

  HRESULT hr = RegKey::GetValue(key_name, value_name, parameter);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[failed to read command parameter]")
                  _T("[key %s][value %s][0x%08x]"), key_name, value_name, hr));
  }

  return hr;
}

// Builds the path to the Commands subkey for a given app ID.
CString BuildAppCommandsPath(bool is_machine, const CString& app_guid) {
  const ConfigManager* config_manager = ConfigManager::Instance();
  return AppendRegKeyPath(config_manager->registry_clients(is_machine),
                          app_guid,
                          kCommandsRegKeyName);
}

}  // namespace

HRESULT AppCommandConfiguration::Load(const CString& app_guid,
                                      bool is_machine,
                                      const CString& command_id,
                                      AppCommandConfiguration** configuration) {
  ASSERT1(configuration);

  CString command_line;
  DWORD sends_pings = 0;
  DWORD is_web_accessible = 0;
  DWORD auto_run_on_os_upgrade = 0;
  DWORD reporting_id = 0;
  DWORD run_as_user = 0;
  DWORD capture_output = 0;

  ConfigManager* config_manager = ConfigManager::Instance();
  CString clients_key_name = config_manager->registry_clients(is_machine);

  CString app_key_name(AppendRegKeyPath(clients_key_name, app_guid));
  CString command_key_name(
      AppendRegKeyPath(app_key_name, kCommandsRegKeyName, command_id));

  // Prefer the new layout, otherwise look for the legacy layout. See comments
  // in app_command.h for description of each.
  if (!RegKey::HasKey(command_key_name)) {
    if (!RegKey::HasValue(app_key_name, command_id)) {
      return GOOPDATE_E_CORE_MISSING_CMD;
    }

    // Legacy command layout.
    HRESULT hr = ReadCommandLine(app_key_name, command_id, &command_line);
    if (FAILED(hr)) {
      return hr;
    }
  } else {
    // New command layout.
    HRESULT hr = ReadCommandLine(command_key_name, kRegValueCommandLine,
                                 &command_line);
    if (FAILED(hr)) {
      return hr;
    }

    struct {
      const TCHAR* name;
      DWORD* value;
    } const values[] = {
      kRegValueSendsPings, &sends_pings,
      kRegValueWebAccessible, &is_web_accessible,
      kRegValueAutoRunOnOSUpgrade, &auto_run_on_os_upgrade,
      kRegValueReportingId, &reporting_id,
      kRegValueRunAsUser, &run_as_user,
      kRegValueCaptureOutput, &capture_output,
    };
    for (size_t i = 0; i < arraysize(values); ++i) {
      hr = ReadCommandParameter(command_key_name,
                                values[i].name,
                                values[i].value);
      if (FAILED(hr)) {
        return hr;
      }
    }
  }

  if (!is_machine) {
    // This flag is irrelevant for user level.
    run_as_user = 0;
  }

  *configuration = new AppCommandConfiguration(app_guid,
                                               is_machine,
                                               command_id,
                                               command_line,
                                               sends_pings != 0,
                                               is_web_accessible != 0,
                                               auto_run_on_os_upgrade != 0,
                                               reporting_id,
                                               run_as_user != 0,
                                               capture_output != 0);
  return S_OK;
}

AppCommand* AppCommandConfiguration::Instantiate(
    const CString& session_id) const {
  AppCommandPingDelegate* delegate = NULL;
  if (sends_pings_) {
    delegate = new AppCommandPingDelegate(
        app_guid_, is_machine_, session_id, reporting_id_);
  }
  return new AppCommand(command_line_,
                        is_web_accessible_,
                        run_as_user_,
                        capture_output_,
                        auto_run_on_os_upgrade_,
                        delegate);
}

HRESULT AppCommandConfiguration::EnumCommandsForApp(
    bool is_machine,
    const CString& app_guid,
    std::vector<CString>* commands) {
  ASSERT1(!app_guid.IsEmpty());
  ASSERT1(commands);
  ASSERT1(commands->empty());

  RegKey commands_key;
  HRESULT hr = commands_key.Open(BuildAppCommandsPath(is_machine, app_guid),
                                 KEY_READ);
  if (FAILED(hr)) {
    if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
      hr = S_FALSE;
    }
    return hr;
  }

  // Note that the RegKey class doesn't guarantee uniqueness or ordering of
  // values if the key's modified while this loop is running.  Unfortunately,
  // writing app commands is the duty of app installers and not Omaha, so we
  // can't protect this by holding the stable state lock.  The best we can do
  // is try to ensure that no installers are running.
  int num_command_subkeys = commands_key.GetSubkeyCount();
  for (int i = 0; i < num_command_subkeys; ++i) {
    CString command_id;
    hr = commands_key.GetSubkeyNameAt(i, &command_id);
    if (FAILED(hr)) {
      continue;
    }

    commands->push_back(command_id);
  }

  return S_OK;
}

HRESULT AppCommandConfiguration::EnumAllCommands(
    bool is_machine,
    std::map<CString, std::vector<CString> >* commands) {
  ASSERT1(commands);
  ASSERT1(commands->empty());

  const ConfigManager* config_manager = ConfigManager::Instance();
  const CString clients_path = config_manager->registry_clients(is_machine);

  RegKey clients_key;
  HRESULT hr = clients_key.Open(clients_path, KEY_READ);
  if (FAILED(hr)) {
    return hr;
  }

  int num_clients_subkeys = clients_key.GetSubkeyCount();
  for (int i = 0; i < num_clients_subkeys; ++i) {
    CString app_id;
    hr = clients_key.GetSubkeyNameAt(i, &app_id);
    if (FAILED(hr)) {
      continue;
    }

    CString commands_path = BuildAppCommandsPath(is_machine, app_id);
    if (RegKey::HasKey(commands_path) && !RegKey::IsKeyEmpty(commands_path)) {
      hr = EnumCommandsForApp(is_machine, app_id, &((*commands)[app_id]));
      if (FAILED(hr)) {
        continue;
      }
    }
  }

  return S_OK;
}

AppCommandConfiguration::AppCommandConfiguration(
    const CString& app_guid,
    bool is_machine,
    const CString& command_id,
    const CString& command_line,
    bool sends_pings,
    bool is_web_accessible,
    bool auto_run_on_os_upgrade,
    DWORD reporting_id,
    bool run_as_user,
    bool capture_output)
    : app_guid_(app_guid),
      is_machine_(is_machine),
      command_id_(command_id),
      command_line_(command_line),
      sends_pings_(sends_pings),
      is_web_accessible_(is_web_accessible),
      run_as_user_(run_as_user),
      capture_output_(capture_output),
      reporting_id_(reporting_id),
      auto_run_on_os_upgrade_(auto_run_on_os_upgrade) {
}

}  // namespace omaha
