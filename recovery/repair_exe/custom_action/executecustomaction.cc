// Copyright 2007-2009 Google Inc.
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
// Custom action helper DLL:
// The custom actions created in this project are for Google Update MSI patch,
// which is installed temporarily and uses this DLL to verify a downloaded
// executable and run it with elevated privileges.


#include "omaha/recovery/repair_exe/custom_action/executecustomaction.h"
#include <Msiquery.h>
#include "omaha/common/debug.h"
#include "omaha/recovery/repair_exe/mspexecutableelevator.h"
#include "omaha/recovery/repair_exe/custom_action/execute_repair_file.h"

namespace {

omaha::CustomActionModule _AtlModule;

}  // namespace

// DLL Entry Point
extern "C" BOOL WINAPI DllMain(HINSTANCE,
                               DWORD dwReason,
                               LPVOID lpReserved) {
  return _AtlModule.DllMain(dwReason, lpReserved);
}

// Verify an executable and run it
UINT __stdcall VerifyFileAndExecute(MSIHANDLE install_handle) {
  TCHAR custom_action_data[2048] = {0};
  DWORD size = ARRAYSIZE(custom_action_data) - 1;
  *custom_action_data = _T('\0');
  if (ERROR_SUCCESS == ::MsiGetProperty(install_handle,
                                        _T("CustomActionData"),
                                        custom_action_data,
                                        &size) &&
      _T('\0') != *custom_action_data) {
    custom_action_data[ARRAYSIZE(custom_action_data) - 1] = _T('\0');

    TCHAR* executable = NULL;
    TCHAR* arguments = NULL;
    DWORD calling_process_id = 0;
    if (omaha::msp_executable_elevator::ParseMSPCommandLine(
            custom_action_data,
            &executable,
            &arguments,
            &calling_process_id) &&
        executable && arguments) {
      HRESULT hr = omaha::VerifyFileAndExecute(executable, arguments);
      VERIFY1(omaha::msp_executable_elevator::SetResultOfExecute(NULL, hr));
    }
  }
  return 0;
}

// 4505: unreferenced local function has been removed
#pragma warning(disable : 4505)
