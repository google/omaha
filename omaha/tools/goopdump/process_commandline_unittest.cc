// Copyright 2008-2009 Google Inc.
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

#include "omaha/tools/goopdump/process_commandline.h"

#include "omaha/common/scoped_any.h"
#include "omaha/common/system.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

TEST(GoopdumpProcessCommandLineTest, TestRunNotepad) {
  // TODO(omaha): Can we use Process::GetCommandLine() instead of
  // GetProcessCommandLine? Should be safer than injecting a thread and can
  // be used in coverage builds.
#ifdef COVERAGE_ENABLED
  std::wcout << _T("\tTest does not run in coverage builds because the ")
             << _T("instrumentation of InjectFunction results in references ")
             << _T("to invalid memory locations in Notepad.exe.")
             << std::endl;
#else
  TCHAR notepad_path[MAX_PATH] = {0};
  TCHAR system_directory[MAX_PATH] = {0};
  ::GetSystemDirectory(system_directory, arraysize(system_directory));
  _stprintf_s(notepad_path,
            arraysize(notepad_path),
            _T("%s\\notepad.exe %s\\loadfix.com"),
            system_directory, system_directory);

  PROCESS_INFORMATION pi = {0};
  EXPECT_SUCCEEDED(System::StartProcess(NULL, notepad_path, &pi));
  ::CloseHandle(pi.hThread);
  scoped_process process_handle(pi.hProcess);

  CString command_line;
  EXPECT_SUCCEEDED(GetProcessCommandLine(pi.dwProcessId, &command_line));

  ::TerminateProcess(get(process_handle), 0);

  EXPECT_STREQ(notepad_path, command_line);
#endif
}

}  // namespace omaha

