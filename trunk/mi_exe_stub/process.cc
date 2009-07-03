// Copyright 2006-2009 Google Inc.
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

#include "omaha/mi_exe_stub/process.h"
#include <strsafe.h>

static bool run_and_wait(const CString &command_line,
                         DWORD* exit_code,
                         bool wait,
                         int cmd_show) {
  CString cmd_line(command_line);
  STARTUPINFO si = {0};
  PROCESS_INFORMATION pi = {0};

  GetStartupInfo(&si);
  si.dwFlags |= STARTF_FORCEOFFFEEDBACK;
  si.dwFlags |= STARTF_USESHOWWINDOW;
  si.wShowWindow = static_cast<WORD>(cmd_show);

  BOOL create_process_result = CreateProcess(NULL,
                                             cmd_line.GetBuffer(),
                                             NULL,
                                             NULL,
                                             FALSE,
                                             CREATE_UNICODE_ENVIRONMENT,
                                             NULL,
                                             NULL,
                                             &si,
                                             &pi);
  if (!create_process_result) {
    *exit_code = GetLastError();
    return false;
  }

  if (wait) {
    WaitForSingleObject(pi.hProcess, INFINITE);
  }

  bool result = true;
  if (exit_code) {
    result = !!GetExitCodeProcess(pi.hProcess, exit_code);
  }

  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);

  return result;
}

bool RunAndWaitHidden(const CString &command_line, DWORD *exit_code) {
  return run_and_wait(command_line, exit_code, true, SW_HIDE);
}

bool RunAndWait(const CString &command_line, DWORD *exit_code) {
  return run_and_wait(command_line, exit_code, true, SW_SHOWNORMAL);
}

bool Run(const CString &command_line) {
  return run_and_wait(command_line, NULL, false, SW_SHOWNORMAL);
}
