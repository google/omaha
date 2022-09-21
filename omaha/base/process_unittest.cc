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
// Process unit tests.

#include <vector>

#include "omaha/base/app_util.h"
#include "omaha/base/path.h"
#include "omaha/base/process.h"
#include "omaha/base/proc_utils.h"
#include "omaha/base/user_info.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

const int kWaitUntilDeadMs = 10000;

// Process class that terminates the associated process when deleted.
class ScopedProcess : public Process {
 public:
  explicit ScopedProcess(const TCHAR* name) : Process(name, NULL) {}
  virtual ~ScopedProcess() {
    Terminate(0);
    EXPECT_TRUE(WaitUntilDead(kWaitUntilDeadMs));
  }
};

TEST(ProcessTest, StartOneProcess) {
  const TCHAR kExecutableName[] = _T("cmd.exe");
  const TCHAR kExecutableArguments[] = _T("/c exit 702");
  const int kExpectedExitCode = 702;

  CString path = ConcatenatePath(app_util::GetSystemDir(), kExecutableName);
  ScopedProcess process(path);

  ASSERT_HRESULT_SUCCEEDED(process.Start(kExecutableArguments, NULL));
  EXPECT_TRUE(process.WaitUntilDead(kWaitUntilDeadMs));

  // Check the exit code to get some assurance that the process actually ran.
  uint32 exit_code = 0;
  EXPECT_TRUE(process.GetExitCode(&exit_code));
  EXPECT_EQ(kExpectedExitCode, exit_code);
}

// Test process to spin off and then find.  The numeric argument will make
// ping.exe run until it's killed by the ScopedProcess destructor.
const TCHAR kTestExecutable[] = _T("ping.exe");
const TCHAR kTestArguments[] = _T("-w 10000 2.2.2.2");
const TCHAR kTestExcludeArguments[] = _T("-n 5 -w 20000 2.2.2.2");
const TCHAR kTestExcludeString[] = _T("20000 2.2.2.2");
const TCHAR kTestIncludeArguments[] = _T("-n 6 -w 30000 2.2.2.2");
const TCHAR kTestIncludeString[] = _T("-w 30000 2.2.2.2");
const int kWaitForProcessStartMs = 500;
const int kMaxWaitIterations = 10;

TEST(ProcessTest, FindOneProcess) {
  CString path = ConcatenatePath(app_util::GetSystemDir(), kTestExecutable);
  ScopedProcess process(path);
  ASSERT_HRESULT_SUCCEEDED(process.Start(kTestArguments, NULL));
  for (int i = 0; i < kMaxWaitIterations; ++i) {
    ::Sleep(kWaitForProcessStartMs);
    if (process.Running())
      break;
  }
  EXPECT_TRUE(process.Running());

  // Try to find the test process.
  uint32 exclude_mask = INCLUDE_ONLY_PROCESS_OWNED_BY_USER;
  CString user_sid;
  std::vector<CString> command_lines;
  std::vector<uint32> process_ids;

  ASSERT_SUCCEEDED(omaha::user_info::GetProcessUser(NULL, NULL, &user_sid));

  // This test intermittently fails to find the process when run on Pulse.
  // This code attempts to ensure that the process is further along in the
  // initialization process by waiting until Process::GetCommandLine succeeds.
  // This test case does not result in FindProcesses using GetCommandLine, but
  // waiting until this point may be enough to address the intermitent failures.
  HRESULT hr = E_FAIL;
  CString process_cmd;
  for (int tries = 0; tries < 100 && FAILED(hr); ++tries) {
    ::Sleep(50);
    hr = Process::GetCommandLine(process.GetId(), &process_cmd);
  }
  EXPECT_SUCCEEDED(hr);

  ASSERT_SUCCEEDED(Process::FindProcesses(exclude_mask,
                                          kTestExecutable,
                                          true,
                                          user_sid,
                                          command_lines,
                                          &process_ids));
  ASSERT_EQ(1, static_cast<int>(process_ids.size()));
  EXPECT_EQ(process.GetId(), process_ids[0]);
}

TEST(ProcessTest, ExcludeProcess) {
  // We are going to terminate all kTestExecutable instances. On Windows 8.x,
  // terminating a process which has been terminated previously would result
  // in access denied error if this process adjusted token privilege in between.
  // So before killing the processes, do a context switch to give previously
  // killed instances of kTestExecutable a chance to clean themselves up.
  ::Sleep(0);

  // Make sure the test process is not already running.
  // Only kill processes that are owned by current user.
  CString cur_user_sid;
  EXPECT_SUCCEEDED(omaha::user_info::GetProcessUser(NULL, NULL, &cur_user_sid));
  ProcessTerminator process_terminator(kTestExecutable, cur_user_sid);

  EXPECT_SUCCEEDED(process_terminator.KillTheProcess(
      5000, NULL, ProcessTerminator::KILL_METHOD_4_TERMINATE_PROCESS, false));

  uint32 exclude_mask = INCLUDE_ONLY_PROCESS_OWNED_BY_USER;
  CString user_sid;
  std::vector<CString> command_lines;
  std::vector<uint32> process_ids;

  ASSERT_SUCCEEDED(omaha::user_info::GetProcessUser(NULL, NULL, &user_sid));
  ASSERT_SUCCEEDED(Process::FindProcesses(exclude_mask,
                                          kTestExecutable,
                                          true,
                                          user_sid,
                                          command_lines,
                                          &process_ids));
  ASSERT_EQ(0, process_ids.size());

  // Ok, test process not running. Let's continue running the test.
  CString path = ConcatenatePath(app_util::GetSystemDir(), kTestExecutable);
  ScopedProcess process(path);
  ScopedProcess exclude_process(path);

  ASSERT_HRESULT_SUCCEEDED(process.Start(kTestArguments, NULL));
  ASSERT_HRESULT_SUCCEEDED(exclude_process.Start(kTestExcludeArguments, NULL));
  for (int i = 0; i < kMaxWaitIterations; ++i) {
    ::Sleep(kWaitForProcessStartMs);
    if (process.Running() && exclude_process.Running())
      break;
  }
  EXPECT_TRUE(process.Running());
  EXPECT_TRUE(exclude_process.Running());

  // Try to find just the first process, excluding the other.
  exclude_mask = INCLUDE_ONLY_PROCESS_OWNED_BY_USER |
                 EXCLUDE_CURRENT_PROCESS |
                 EXCLUDE_PROCESS_COMMAND_LINE_CONTAINING_STRING;

  command_lines.push_back(kTestExcludeString);
  ASSERT_SUCCEEDED(Process::FindProcesses(exclude_mask,
                                          kTestExecutable,
                                          true,
                                          user_sid,
                                          command_lines,
                                          &process_ids));
  ASSERT_EQ(1, static_cast<int>(process_ids.size()));
  EXPECT_EQ(process.GetId(), process_ids[0]);
}

TEST(ProcessTest, IncludeProcess) {
  CString path = ConcatenatePath(app_util::GetSystemDir(), kTestExecutable);
  ScopedProcess process(path);
  ScopedProcess include_process(path);

  ASSERT_HRESULT_SUCCEEDED(process.Start(kTestArguments, NULL));
  ASSERT_HRESULT_SUCCEEDED(include_process.Start(kTestIncludeArguments, NULL));
  for (int i = 0; i < kMaxWaitIterations; ++i) {
    ::Sleep(kWaitForProcessStartMs);
    if (process.Running() && include_process.Running())
      break;
  }
  EXPECT_TRUE(process.Running());
  EXPECT_TRUE(include_process.Running());

  // Try to find just the first process, excluding the other.
  uint32 exclude_mask = INCLUDE_ONLY_PROCESS_OWNED_BY_USER |
                        EXCLUDE_CURRENT_PROCESS |
                        INCLUDE_PROCESS_COMMAND_LINE_CONTAINING_STRING;
  CString user_sid;
  std::vector<CString> command_lines;
  std::vector<uint32> process_ids;

  command_lines.push_back(kTestIncludeString);
  ASSERT_SUCCEEDED(omaha::user_info::GetProcessUser(NULL, NULL, &user_sid));
  ASSERT_SUCCEEDED(Process::FindProcesses(exclude_mask,
                                          kTestExecutable,
                                          true,
                                          user_sid,
                                          command_lines,
                                          &process_ids));
  ASSERT_EQ(1, static_cast<int>(process_ids.size()));
  EXPECT_EQ(include_process.GetId(), process_ids[0]);
}

TEST(ProcessTest, GetImagePath) {
  // Get this module's path.
  HMODULE handle = ::GetModuleHandle(NULL);
  ASSERT_TRUE(handle != NULL);

  CString filename = app_util::GetModulePath(handle);
  ASSERT_FALSE(filename.IsEmpty());

  CString exe = GetFileFromPath(filename);
  ASSERT_FALSE(exe.IsEmpty());

  CString user_sid;
  ASSERT_SUCCEEDED(omaha::user_info::GetProcessUser(NULL, NULL, &user_sid));

  // Test the method.
  CString path;
  ASSERT_SUCCEEDED(Process::GetImagePath(exe, user_sid, &path));

  // Compare the result.
  // Should use ASSERT_STRCASEEQ, but CmpHelperSTRCASEEQ does not work for
  // wchars.
  ASSERT_STREQ(filename.MakeLower(), path.MakeLower());
}

TEST(ProcessTest, GetParentProcessId_CurrentProcess) {
  Process process(::GetCurrentProcessId());
  uint32 parent_pid = 0;
  EXPECT_SUCCEEDED(process.GetParentProcessId(&parent_pid));
  EXPECT_NE(0, parent_pid);
}

TEST(ProcessTest, GetParentProcessId_ChildProcess) {
  CString path = ConcatenatePath(app_util::GetSystemDir(), kTestExecutable);
  ScopedProcess process(path);

  EXPECT_HRESULT_SUCCEEDED(process.Start(kTestArguments, NULL));
  for (int i = 0; i < kMaxWaitIterations; ++i) {
    ::Sleep(kWaitForProcessStartMs);
    if (process.Running()) {
      break;
    }
  }

  EXPECT_TRUE(process.Running());

  uint32 parent_pid = 0;
  EXPECT_SUCCEEDED(process.GetParentProcessId(&parent_pid));
  EXPECT_EQ(::GetCurrentProcessId(), parent_pid);
}

}  // namespace omaha

