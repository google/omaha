// Copyright 2009 Google Inc.
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
// This file implements a Custom Action DLL for standalone MSI installers that
// uninstall Omaha-managed products by exposing a UninstallOmahaProduct
// method.
//
// The custom action makes the following assumptions:
// 1) The software to be uninstalled has a corresponding Omaha key under
//    kOmahaClientsKey\{AppGuid}
//
// 2) {AppGuid} and any extra flags needed to cause the product's uninstall
//    to execute silently are passed in via the CustomActionData property (if
//    using Wix, set a Property of type 'immediate' with the same name as your
//    'deferred' custom action - this causes the value to get passed in as
//    the CustomActionData property here.)
//    The expected format for the CustomActionData is:
//      {AppGuid}|<uninstall flags>
//    For example:
//      {8BA986DA-5100-405E-AA35-86F34A02ACBF}|--force-uninstall
//
// 3) The app's "Clients" key contains a 'name' string value that is used as
//    the key to the list of Windows uninstall shortcuts that reside in
//    kUninstallKey.
//
// 4) The program to be uninstalled has made its registrations under HKLM.
//
// TODO(robertshield): Make 4) an argument, don't assume HKLM.
// TODO(robertshield): Make this work for non-Omaha-managed products as well.

#include <Windows.h>
#include <Msi.h>
#include <MsiQuery.h>
#include <Shlwapi.h>

#include <string>
#include <vector>

const wchar_t kOmahaClientsKey[] = L"Software\\Google\\Update\\Clients\\";
const wchar_t kOmahaProductName[] = L"name";
const wchar_t kUninstallKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\";
const wchar_t kUninstallCmdName[] = L"UninstallString";

// This is how long (in ms) we wait for the uninstall command to complete.
const DWORD kUninstallCmdTimeoutMs = 120 * 1000;

static void MsiLogInfo(MSIHANDLE install, const std::wstring& msg) {
  // Note that PMSIHANDLES clean up after themselves.
  PMSIHANDLE record = ::MsiCreateRecord(2);
  std::wstring log_msg(L"UNINSTALL LOG: ");
  log_msg += msg;
  ::MsiRecordSetString(record, 0, log_msg.c_str());
  ::MsiProcessMessage(install, INSTALLMESSAGE(INSTALLMESSAGE_INFO), record);
}

// Retrieve the named property from the database and stuff it in
// return_value. Returns true on success, false otherwise.
//
// Note that in practice, if this is run as a deferred custom action, you can
// only query for CustomActionData and a few others as per
// http://msdn.microsoft.com/en-us/library/aa370543(VS.85).aspx.
static bool GetMsiProperty(MSIHANDLE msi_handle,
                           const std::wstring& name,
                           std::wstring* return_value) {
  if (!return_value) {
    return false;
  }

  DWORD size = 1024;
  return_value->resize(size + 1);
  UINT result = ::MsiGetProperty(msi_handle, name.c_str(), &(*return_value)[0],
                                 &size);
  if (result == ERROR_MORE_DATA) {
    return_value->resize(size + 1);
    result = ::MsiGetProperty(msi_handle, name.c_str(), &(*return_value)[0],
                              &size);
  }
  // Resize the string down to the actual number of characters copied.
  return_value->resize(size + 1);

  if (result != ERROR_SUCCESS) {
    std::wstring msg(L"Failed to retrieve property: ");
    msg += name;
    MsiLogInfo(msi_handle, msg);
  }

  return (result == ERROR_SUCCESS);
}

// Reads in the registry value called value_name residing at key_path under
// root_key and stuffs it in value. Returns true on success, false
// otherwise.
static bool ReadRegStringValue(MSIHANDLE msi_handle,
                               HKEY root_key,
                               const std::wstring& key_path,
                               const std::wstring& value_name,
                               std::wstring* value) {
  if (!value) {
    return false;
  }

  LSTATUS result;
  DWORD size = 0;
  result = ::SHRegGetValue(root_key, key_path.c_str(), value_name.c_str(),
                           SRRF_RT_REG_SZ, NULL, NULL, &size);

  std::vector<BYTE> buffer;
  // Note that since we passed NULL in as the buffer, SHRegGetValue will
  // have returned ERROR_SUCCESS if the key exists.
  if (result == ERROR_SUCCESS && size > 0) {
    buffer.resize(size);
    result = ::SHRegGetValue(root_key, key_path.c_str(), value_name.c_str(),
                             SRRF_RT_REG_SZ, NULL, &buffer[0], &size);
  }

  if (result != ERROR_SUCCESS || buffer.empty()) {
    std::wstring error_message(L"Failed to read reg value: ");
    error_message += key_path;
    error_message += L"\\";
    error_message += value_name;
    MsiLogInfo(msi_handle, error_message);
  } else {
    size_t string_length = size / sizeof(wchar_t);
    // Note that we must subtract one from the length to account for the extra
    // null terminator added by SHRegValue().
    value->assign(reinterpret_cast<wchar_t*>(&buffer[0]), string_length - 1);
  }

  return (result == ERROR_SUCCESS);
}

// Runs the command given in cmd_line and waits kUninstallCmdTimeoutMs for it
// to complete and places the exit code in exit_code. Tries to kill the
// process if it hasn't completed before the timeout. Returns true on successful
// completion of the command, false otherwise.
static bool LaunchAppAndReturnExitCode(MSIHANDLE msi_handle,
                                       const std::wstring& cmd_line,
                                       DWORD* exit_code) {
  if (!exit_code) {
    return false;
  }

  bool is_successful = false;
  STARTUPINFO startup_info = {0};
  startup_info.cb = sizeof(startup_info);
  PROCESS_INFORMATION process_info = {0};
  if (::CreateProcess(NULL,
                      const_cast<wchar_t*>(cmd_line.c_str()), NULL, NULL,
                      FALSE, 0, NULL, NULL,
                      &startup_info, &process_info)) {
    DWORD wait_result = ::WaitForSingleObject(process_info.hProcess,
                                              kUninstallCmdTimeoutMs);
    if (wait_result == WAIT_TIMEOUT) {
      // Looks like our uninstall process is hung, try to kill it.
      ::TerminateProcess(process_info.hProcess, 0);
    } else if (wait_result == WAIT_OBJECT_0) {
      if (::GetExitCodeProcess(process_info.hProcess, exit_code)) {
        if (*exit_code != STILL_ACTIVE)
          is_successful = true;
      }
    } else {
      std::wstring error_message(L"Error waiting for process exit: ");
      error_message += cmd_line;
      MsiLogInfo(msi_handle, error_message);
    }

    // Don't leak the handles.
    ::CloseHandle(process_info.hThread);
    ::CloseHandle(process_info.hProcess);
  } else {
    std::wstring error_message(L"Failed to CreateProcess ");
    error_message += cmd_line;
    MsiLogInfo(msi_handle, error_message);
  }

  return is_successful;
}

static bool ParseCustomActionData(const std::wstring& custom_action_data,
                                  std::wstring* app_id,
                                  std::wstring* additional_uninstall_args) {
  if (!app_id || !additional_uninstall_args) {
    return false;
  }

  bool result = false;
  size_t separator_pos = custom_action_data.find(L'|');
  if (separator_pos != std::wstring::npos) {
    *app_id = custom_action_data.substr(0, separator_pos);
    *additional_uninstall_args = custom_action_data.substr(separator_pos + 1);
    result = true;
  }

  return result;
}

extern "C" UINT __stdcall UninstallOmahaProduct(MSIHANDLE msi_handle) {
  DWORD result = ERROR_INSTALL_FAILURE;

  // Get the app id we're interested in as well as the product uninstallation
  // parameters.
  bool valid_ca_data = false;
  std::wstring custom_action_data;
  std::wstring app_id;
  std::wstring additional_uninstall_args;
  if (GetMsiProperty(msi_handle, L"CustomActionData", &custom_action_data)) {
    valid_ca_data = ParseCustomActionData(custom_action_data, &app_id,
                                          &additional_uninstall_args);
  }

  std::wstring uninstall_cmd;
  if (valid_ca_data) {
    // Use the app id to look up the product name...
    std::wstring product_name;
    std::wstring omaha_client_key(kOmahaClientsKey);
    omaha_client_key += app_id;

    std::wstring product_msg(L"Looking for product name in: ");
    product_msg += omaha_client_key;
    MsiLogInfo(msi_handle, product_msg);

    if (ReadRegStringValue(msi_handle, HKEY_LOCAL_MACHINE,
                           omaha_client_key.c_str(), kOmahaProductName,
                           &product_name)) {
      std::wstring uninstall_key_path(kUninstallKey);
      uninstall_key_path += product_name;

      std::wstring key_msg(L"Looking for uninstall key: ");
      key_msg += uninstall_key_path;
      MsiLogInfo(msi_handle, key_msg);

      // ... and then use product name to look up the uninstall command line.
      if (ReadRegStringValue(msi_handle, HKEY_LOCAL_MACHINE,
                             uninstall_key_path.c_str(), kUninstallCmdName,
                             &uninstall_cmd)) {
        result = ERROR_SUCCESS;
      }
    }
  }

  if (result == ERROR_SUCCESS) {
    // Append the necessary flags to keep the uninstall silent.
    uninstall_cmd += L" ";
    uninstall_cmd += additional_uninstall_args;

    std::wstring uninstall_msg(L"Found uninstall command, executing: ");
    uninstall_msg += uninstall_cmd;
    MsiLogInfo(msi_handle, uninstall_msg);

    // We've found the uninstall command. Now run it and wait for the response.
    DWORD exit_code;
    if (LaunchAppAndReturnExitCode(msi_handle, uninstall_cmd, &exit_code)) {
      // TODO(robertshield): See about doing something based on the return code.
    } else {
      result = ERROR_INSTALL_FAILURE;
    }
  }

  return result;
}
