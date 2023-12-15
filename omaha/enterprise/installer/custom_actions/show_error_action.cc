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
//
// Author: grt
//
// A Windows Installer custom action that displays and logs an app installer's
// InstallResultUIString via MsiProcessMessage.  The app's guid must be provided
// by the MSI wrapper by way of the CustomActionData property.

#include <windows.h>
#include <msi.h>
#include <msiquery.h>
#include <objbase.h>
#include <stdlib.h>

#include <algorithm>
#include <limits>
#include <string>

#include "omaha/enterprise/installer/custom_actions/msi_custom_action.h"

#define SOFTWARE_GOOGLE_UPDATE L"Software\\" PATH_COMPANY_NAME_ANSI "\\Update"
#define SOFTWARE_GOOGLE_UPDATE_CLIENTSTATE \
    SOFTWARE_GOOGLE_UPDATE L"\\ClientState"

namespace {

const DWORD kInstallerResultFailedCustomError = 1;
const wchar_t kPropertyCustomActionData[] = L"CustomActionData";
const wchar_t kRegKeyClientState[] = SOFTWARE_GOOGLE_UPDATE_CLIENTSTATE;
const wchar_t kRegKeyGoogleUpdate[] = SOFTWARE_GOOGLE_UPDATE;
const wchar_t kRegValueLastInstallerResult[] = L"LastInstallerResult";
const wchar_t kRegValueLastInstallerResultUIString[] =
    L"LastInstallerResultUIString";

// The type of a function that returns true if |c| is a valid char in a GUID.
typedef bool (*IsGuidCharFn)(wchar_t c);

// A function template that returns true if |c| equals some constant |C|.
template<wchar_t C>
bool IsChar(wchar_t c) {
  return c == C;
}

// Returns true if |c| is a valid hex character.
bool IsHexDigit(wchar_t c) {
  return ((c >= L'0' && c <= '9') ||
          (c >= L'a' && c <= 'f') ||
          (c >= L'A' && c <= 'F'));
}

// Returns true if |guid| is a well-formed GUID.
bool IsGuid(const std::wstring& guid) {
  static const IsGuidCharFn kGuidCharValidators[] = {
    IsChar<L'{'>,
    IsHexDigit,
    IsHexDigit,
    IsHexDigit,
    IsHexDigit,
    IsHexDigit,
    IsHexDigit,
    IsHexDigit,
    IsHexDigit,
    IsChar<L'-'>,
    IsHexDigit,
    IsHexDigit,
    IsHexDigit,
    IsHexDigit,
    IsChar<L'-'>,
    IsHexDigit,
    IsHexDigit,
    IsHexDigit,
    IsHexDigit,
    IsChar<L'-'>,
    IsHexDigit,
    IsHexDigit,
    IsHexDigit,
    IsHexDigit,
    IsChar<L'-'>,
    IsHexDigit,
    IsHexDigit,
    IsHexDigit,
    IsHexDigit,
    IsHexDigit,
    IsHexDigit,
    IsHexDigit,
    IsHexDigit,
    IsHexDigit,
    IsHexDigit,
    IsHexDigit,
    IsHexDigit,
    IsChar<L'}'>,
  };

  if (guid.size() != _countof(kGuidCharValidators))
    return false;

  for (size_t i = 0, end = guid.size(); i < end; ++i) {
    if (!kGuidCharValidators[i](guid[i]))
      return false;
  }

  return true;
}

// Gets the app guid for the product being installed. Returns false if a value
// that doesn't look like a GUID is read.
bool GetProductGuid(MSIHANDLE install, std::wstring* guid) {
  return custom_action::GetProperty(install,
                                    kPropertyCustomActionData,
                                    guid) &&
         IsGuid(*guid);
}

// Populates |key_name| with the full name of |app_guid|'s ClientState registry
// key.
void GetAppClientStateKey(const std::wstring& app_guid,
                          std::wstring* key_name) {
  const size_t base_len = _countof(kRegKeyClientState) - 1;
  key_name->reserve(base_len + 1 + app_guid.size());
  key_name->assign(kRegKeyClientState, base_len);
  key_name->append(1, L'\\');
  key_name->append(app_guid);
}

// Reads the string value named |value_name| in registry key |key| into
// |result_string|.  Returns ERROR_NOT_SUPPORTED if the value exists but is not
// of type REG_SZ.
LONG ReadRegistryStringValue(HKEY key, const wchar_t* value_name,
                             std::wstring* result_string) {
  LONG result;
  DWORD type;
  DWORD byte_length;

  // Use all of the provided buffer.
  result_string->resize(result_string->capacity());

  // Figure out how much we can hold there, being careful about overflow.
  byte_length = static_cast<DWORD>(std::min(
      result_string->size(),
      static_cast<size_t>(
          std::numeric_limits<DWORD>::max() / sizeof(wchar_t))));
  byte_length *= sizeof(wchar_t);

  do {
    // Read into the provided buffer.
    BYTE* buffer = reinterpret_cast<BYTE*>(
        result_string->empty() ? NULL : &(*result_string)[0]);
    result = RegQueryValueEx(key, value_name, NULL, &type, buffer,
                             &byte_length);
    if (result == ERROR_SUCCESS) {
      const size_t chars_read = byte_length / sizeof(wchar_t);
      if (type != REG_SZ) {
        // The value wasn't a string.
        result = ERROR_NOT_SUPPORTED;
      } else if (byte_length == 0) {
        // The string was empty.
        result_string->clear();
      } else if ((*result_string)[chars_read - 1] != L'\0') {
        // The string was not terminated.  Let std::basic_string do so.
        result_string->resize(chars_read);
      } else {
        // The string was terminated.  Trim off the terminator.
        result_string->resize(chars_read - 1);
      }
    } else if (result == ERROR_MORE_DATA) {
      // Increase the buffer and try again.
      result_string->resize(byte_length / sizeof(wchar_t));
    }
  } while (result == ERROR_MORE_DATA);

  return result;
}

// Reads the DWORD value named |value_name| in registry key |key| into |value|.
// Returns ERROR_NOT_SUPPORTED if the value exists but is not of type REG_DWORD.
LONG ReadRegistryDwordValue(HKEY key, const wchar_t* value_name, DWORD* value) {
  LONG result;
  DWORD type;
  DWORD byte_length = sizeof(*value);

  result = RegQueryValueEx(key, value_name, NULL, &type,
                           reinterpret_cast<BYTE*>(value), &byte_length);
  if (result == ERROR_SUCCESS && type != REG_DWORD) {
    // The value wasn't a DWORD.
    result = ERROR_NOT_SUPPORTED;
  }

  return result;
}

// Checks to see if the app installer failed with a custom error and provided a
// UI string.  If so, returns true and populates |result_string| with the UI
// string.  Otherwise, returns false.
bool GetLastInstallerResultUIString(const std::wstring& app_guid,
                                    std::wstring* result_string) {
  std::wstring client_state_name;
  HKEY key = NULL;

  GetAppClientStateKey(app_guid, &client_state_name);

  // First try looking in the app's ClientState key.  Failing that, fall back to
  // Google Update's own SOFTWARE\Google\Update key, into which GoogleUpdate.exe
  // copies the app's value (see AppManager::ClearInstallerResultApiValues).
  LONG result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, client_state_name.c_str(),
                             NULL, KEY_QUERY_VALUE, &key);
  if (result != ERROR_SUCCESS) {
    result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, kRegKeyGoogleUpdate, NULL,
                          KEY_QUERY_VALUE, &key);
  }

  if (result == ERROR_SUCCESS) {
    // Is LastInstallerResult == INSTALLER_RESULT_FAILED_CUSTOM_ERROR?
    DWORD last_installer_error = 0;
    result = ReadRegistryDwordValue(key, kRegValueLastInstallerResult,
                                    &last_installer_error);
    if (result == ERROR_SUCCESS &&
        last_installer_error == kInstallerResultFailedCustomError) {
      result = ReadRegistryStringValue(
          key, kRegValueLastInstallerResultUIString, result_string);
    }

    RegCloseKey(key);
  }

  return result == ERROR_SUCCESS;
}

}  // namespace

// A DLL custom action entrypoint that performs the work described at the top
// of this file.
extern "C" UINT __stdcall ShowInstallerResultUIString(MSIHANDLE install) {
  std::wstring app_guid;
  std::wstring result_string;

  if (GetProductGuid(install, &app_guid) &&
      GetLastInstallerResultUIString(app_guid, &result_string) &&
      !result_string.empty()) {
    PMSIHANDLE record = MsiCreateRecord(0);
    if (record != 0UL) {
      UINT result = MsiRecordSetString(record, 0, result_string.c_str());
      if (result == ERROR_SUCCESS)
        MsiProcessMessage(install, INSTALLMESSAGE_ERROR, record);
    }
  }

  return ERROR_SUCCESS;
}
