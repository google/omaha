// Copyright 2009-2010 Google Inc.
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

#include <shlwapi.h>
#include <tchar.h>
#include <strsafe.h>
#include <windows.h>
#include "base/basictypes.h"
#include "omaha/base/constants.h"
#include "omaha/common/const_cmd_line.h"

// TODO(omaha): Use the base libraries instead of selectively pulling in
// functionality such as HRESULTFromLastError(). May increase size, but the
// resulting code quality may be worth it.

// This is defined in error.cc, but that pulls in debug.cc, which has a lot
// of additional dependencies we do not want.
HRESULT HRESULTFromLastError() {
  DWORD error_code = ::GetLastError();
  return (error_code != NO_ERROR) ? HRESULT_FROM_WIN32(error_code) : E_FAIL;
}

// This is defined in utils.h, but that pulls in additional dependencies we do
// not want.
// This function calls ::SetDefaultDllDirectories to retrict DLL loads to either
// full paths or %SYSTEM32%. ::SetDefaultDllDirectories is available on Windows
// 8.1 and above, and on Windows Vista and above when KB2533623 is applied.
bool EnableSecureDllLoading() {
  typedef BOOL (WINAPI *SetDefaultDllDirectoriesFunction)(DWORD flags);
  SetDefaultDllDirectoriesFunction set_default_dll_directories =
      reinterpret_cast<SetDefaultDllDirectoriesFunction>(
          ::GetProcAddress(::GetModuleHandle(_T("kernel32.dll")),
                           "SetDefaultDllDirectories"));
  if (set_default_dll_directories) {
    return !!set_default_dll_directories(LOAD_LIBRARY_SEARCH_SYSTEM32);
  }

  return false;
}

// TODO(omaha): Use a registry override instead.
#if !OFFICIAL_BUILD
bool IsRunningFromStaging(const WCHAR* const command_line) {
  return !wcscmp(command_line + (wcslen(command_line) - wcslen(L"staging")),
                 L"staging");
}
#endif

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous_instance,
                    LPTSTR cmd_line, int show) {
  EnableSecureDllLoading();

  UNREFERENCED_PARAMETER(instance);
  UNREFERENCED_PARAMETER(previous_instance);
  UNREFERENCED_PARAMETER(cmd_line);
  UNREFERENCED_PARAMETER(show);

  // TODO(omaha3): We should probably update this code to use app_util
  // and CPath to find the path to the constant shell.  Maybe once we've
  // done the base/minibase refactoring for mi_exe_stub, we can do this.
  WCHAR command_line_quoted[MAX_PATH * 2] = { L"\"" };
  WCHAR* command_line = command_line_quoted + 1;
  DWORD command_line_size = arraysize(command_line_quoted) - 1;

  DWORD err = ::GetModuleFileName(NULL, command_line, command_line_size);
  if (err == 0 || err >= command_line_size) {
    return HRESULTFromLastError();
  }

  // TODO(omaha): Use the registry to get the path of the constant shell.
  // Remove filename and move up one directory, because we want to use the
  // constant shell GoogleUpdate.exe.
  ::PathRemoveFileSpec(command_line);
#if OFFICIAL_BUILD
  ::PathRemoveFileSpec(command_line);
#else
  // This is to facilitate unit tests such as
  // GoogleUpdateCoreTest.LaunchCmdElevated_LocalServerRegistered. If we are
  // running from the staging directory, the shell is in the same directory.
  if (!IsRunningFromStaging(command_line)) {
    ::PathRemoveFileSpec(command_line);
  }
#endif

  if (!::PathAppend(command_line, omaha::kOmahaShellFileName)) {
    return HRESULTFromLastError();
  }

  HRESULT hr = StringCchCat(command_line_quoted,
                            arraysize(command_line_quoted),
                            L"\" " CMD_LINE_SWITCH L" ");
  if (FAILED(hr)) {
    return hr;
  }

#if SHOULD_APPEND_CMD_LINE
  hr = StringCchCat(command_line_quoted,
                    arraysize(command_line_quoted),
                    cmd_line);
  if (FAILED(hr)) {
    return hr;
  }
#endif

  STARTUPINFO si = { sizeof(si) };
  PROCESS_INFORMATION pi = {};
  if (!::CreateProcess(NULL,
                       command_line_quoted,
                       NULL,
                       NULL,
                       FALSE,
                       0,
                       NULL,
                       NULL,
                       &si,
                       &pi)) {
    return HRESULTFromLastError();
  }

  ::CloseHandle(pi.hProcess);
  ::CloseHandle(pi.hThread);

  return 0;
}
