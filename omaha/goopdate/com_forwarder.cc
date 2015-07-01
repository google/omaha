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

// TODO(omaha): Use a registry override instead.
#if !OFFICIAL_BUILD
bool IsRunningFromStaging(const WCHAR* const command_line) {
  return !wcscmp(command_line + (wcslen(command_line) - wcslen(L"staging")),
                 L"staging");
}
#endif

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous_instance,
                   LPSTR cmd_line, int show) {
  UNREFERENCED_PARAMETER(instance);
  UNREFERENCED_PARAMETER(previous_instance);
  UNREFERENCED_PARAMETER(cmd_line);
  UNREFERENCED_PARAMETER(show);

  WCHAR command_line[MAX_PATH * 2] = {};
  if (0 == ::GetModuleFileName(NULL,
                               command_line,
                               arraysize(command_line))) {
    return E_UNEXPECTED;
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
    return E_UNEXPECTED;
  }

  if (FAILED(StringCchCat(command_line, arraysize(command_line), L" ")) ||
      FAILED(StringCchCat(command_line, arraysize(command_line),
                          CMD_LINE_SWITCH))) {
    return E_UNEXPECTED;
  }

  STARTUPINFO si = { sizeof(si) };
  // XXX: Normally, you should close the handles returned in
  // PROCESS_INFORMATION. That step is skipped here since we are exiting
  // immediately once the new process is created.
  PROCESS_INFORMATION pi = {};
  if (!::CreateProcess(
          NULL,
          command_line,
          NULL,
          NULL,
          FALSE,
          0,
          NULL,
          NULL,
          &si,
          &pi)) {
    return E_UNEXPECTED;
  }

  return 0;
}
