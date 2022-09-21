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
// An app installer that does nothing more than write error information to
// the registry as per the Google Update Installer Result API and return 1.

#include <windows.h>

#define GOOGLE_UPDATE_KEY L"SOFTWARE\\" PATH_COMPANY_NAME_ANSI "\\Update"
#define TEST_SETUP_APP_GUID L"{665BDD8E-F40C-4384-A9C6-CA3CD5665C83}"

namespace {

const DWORD kInstallerResultFailedCustomError = 1;
const wchar_t kErrorString[] = L"This is a detailed error message.";
const wchar_t kRegClientStateKey[] =
    GOOGLE_UPDATE_KEY L"\\ClientState\\" TEST_SETUP_APP_GUID;
const wchar_t kRegInstallerResultValue[] = L"InstallerResult";
const wchar_t kRegInstallerResultUIStringValue[] = L"InstallerResultUIString";

void WriteFailureValues() {
  HKEY client_state_key;
  LONG result = RegCreateKeyEx(HKEY_LOCAL_MACHINE, kRegClientStateKey, 0, NULL,
                               REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL,
                               &client_state_key, NULL);
  if (result == ERROR_SUCCESS) {
    RegSetValueEx(
        client_state_key, kRegInstallerResultValue, 0, REG_DWORD,
        reinterpret_cast<const BYTE*>(&kInstallerResultFailedCustomError),
        sizeof(kInstallerResultFailedCustomError));
    RegSetValueEx(
        client_state_key, kRegInstallerResultUIStringValue, 0, REG_SZ,
        reinterpret_cast<const BYTE*>(&kErrorString),
        sizeof(kErrorString));
    RegCloseKey(client_state_key);
  }
}

}  // namespace

int wmain(int /*argc*/, wchar_t* /*argv[]*/) {
  WriteFailureValues();
  return 1;
}
