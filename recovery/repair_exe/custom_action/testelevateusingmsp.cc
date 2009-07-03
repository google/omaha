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
// Command line utility that elevates an executable using an MSI Patch.
// The MSI patch is assumed to be in the same directory as this executable.

#include <windows.h>
#include <atlstr.h>
#include "omaha/common/constants.h"
#include "omaha/recovery/repair_exe/mspexecutableelevator.h"

int main(int argc, char** argv) {
  DWORD process_id = 0;
  if (2 <= argc) {
    CString arguments;
    if (3 <= argc) {
      arguments = argv[2];
    }
    HANDLE process = NULL;
    HRESULT hr = omaha::msp_executable_elevator::ExecuteGoogleSignedExe(
        CString(argv[1]),
        arguments,
        omaha::kHelperInstallerProductGuid,
        omaha::kHelperPatchGuid,
        omaha::kHelperPatchName,
        &process);
    if (process) {
      process_id = ::GetProcessId(process);
      ::CloseHandle(process);
    }
    wprintf(_T("%s (process handle:%x process id: %u hresult:%x)"),
           (SUCCEEDED(hr) ? _T("Success") : _T("Failure")),
           process,
           process_id,
           static_cast<int>(hr));
  } else {
    static TCHAR explain_test[] =
      _T("testelevateusingmsp\n\n")
      _T("To use this test, pass the full path to an executable containing ")
      _T("the Google Update Repair resource and is signed with a Google ")
      _T("code-signing certificate that has a certain subject and ")
      _T("organization unit name.")
      _T("\n\nAn optional parameter to that executable may be passed as well.");
    wprintf(explain_test);
  }
  return process_id;
}
