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
// Saves the arguments passed to this application to saved_arguments.txt in the
// same directory as the executable.

#include <atlstr.h>
#include <stdio.h>
#include <tchar.h>
#include <windows.h>
#include "omaha/common/app_util.h"
#include "omaha/common/scoped_any.h"

namespace {


const TCHAR kSavedArgumentsFileName[] = _T("saved_arguments.txt");

// Reports a Win32 error to the command line and debug output then exits the
// process. Call immediately after the failed Win32 API call.
void HandleWin32ErrorAndExit(const TCHAR* method) {
  _ASSERTE(method);
  int res = ::GetLastError();
  CString error_message;
  error_message.Format(_T("%s failed with error %i."), method, res);


  _tprintf(_T("%s\n"), error_message);

  CString debug_message;
  debug_message.Format(_T("[SaveArguments.exe][%s]"), error_message);
  ::OutputDebugString(debug_message);

  exit(res);
}

// Performs actions that are useful for debugging.
void DoDebugHelper() {
  // This is useful for debugging Code Red.
  CString message;
  message.AppendFormat(_T("[SaveArguments.exe][Temp directory: %s]"),
                       omaha::app_util::GetTempDir());
  ::OutputDebugString(message);
}

// Performs actions that are helpful or required for Omaha unit tests.
void DoUnitTestHelpers() {
  // The following code adapted from mi.cpp allows unit test to verify that the
  // executable file still exists and has not been deleted after the process
  // is created.
  TCHAR file_name[MAX_PATH] = {0};
  if (!::GetModuleFileName(NULL, file_name, MAX_PATH)) {
    HandleWin32ErrorAndExit(_T("GetModuleFileName"));
  }
  DWORD handle = 0;
  DWORD ver_info_size = ::GetFileVersionInfoSize(file_name, &handle);
  if (ver_info_size == 0) {
    HandleWin32ErrorAndExit(_T("GetFileVersionInfoSize"));
  }
}

// Writes the provided arguments to the file.
// Returns whether it was successful.
int WriteArgsToFile(const CString& arguments) {
  CString file_path(omaha::app_util::GetCurrentModuleDirectory());
  if (!::PathAppend(CStrBuf(file_path, MAX_PATH), kSavedArgumentsFileName)) {
    _ASSERTE(false);
    return -1;
  }

  scoped_hfile file(::CreateFile(file_path,
                             GENERIC_READ | GENERIC_WRITE,
                             0,                        // do not share
                             NULL,                     // default security
                             CREATE_ALWAYS,            // overwrite existing
                             FILE_ATTRIBUTE_NORMAL,
                             NULL));                    // no template
  if (get(file) == INVALID_HANDLE_VALUE) {
    HandleWin32ErrorAndExit(_T("CreateFile"));
  }

  DWORD bytes_written = 0;
  if (!::WriteFile(get(file),
                   arguments.GetString(),
                   arguments.GetLength() * sizeof(TCHAR),
                   &bytes_written,
                   NULL)) {
    HandleWin32ErrorAndExit(_T("WriteFile"));
  }

  return 0;
}

}  // namespace


// Returns 0 on success and non-zero otherwise.
int _tmain(int argc, TCHAR* argv[]) {
  DoDebugHelper();
  DoUnitTestHelpers();

  CString arguments;

  // Skip the first argument, which is the executable path.
  for (int i = 1; i < argc; i++) {
    arguments += argv[i];
    if (i < argc - 1) {
      arguments += " ";
    }
  }

  return WriteArgsToFile(arguments);
}
