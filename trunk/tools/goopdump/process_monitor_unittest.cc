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

#include <vector>

#include "omaha/common/system.h"
#include "omaha/testing/unit_test.h"
#include "omaha/tools/goopdump/process_monitor.h"

namespace omaha {

// TODO(omaha): use gMock.
class MockProcessMonitorCallback : public ProcessMonitorCallbackInterface {
 public:
  MockProcessMonitorCallback()
    : num_times_onprocessadded_called_(0),
      num_times_onprocessremoved_called_(0) {}
  virtual ~MockProcessMonitorCallback() {}

  virtual void OnProcessAdded(DWORD process_id,
                              const CString& process_pattern) {
    ++num_times_onprocessadded_called_;
    process_ids_added_.push_back(process_id);
    process_patterns_added_.push_back(process_pattern);
  }

  virtual void OnProcessRemoved(DWORD process_id) {
    ++num_times_onprocessremoved_called_;
    process_ids_removed_.push_back(process_id);
  }

  int num_times_onprocessadded_called() const {
    return num_times_onprocessadded_called_;
  }

  int num_times_onprocessremoved_called() const {
    return num_times_onprocessremoved_called_;
  }

 private:
  int num_times_onprocessadded_called_;
  int num_times_onprocessremoved_called_;

  std::vector<CString> process_patterns_added_;
  std::vector<DWORD> process_ids_added_;
  std::vector<DWORD> process_ids_removed_;

  DISALLOW_EVIL_CONSTRUCTORS(MockProcessMonitorCallback);
};

TEST(GoopdumpProcessMonitorTest, TestStartStopNoop) {
  ProcessMonitor process_monitor;
  EXPECT_SUCCEEDED(process_monitor.Start(NULL, _T("some_noop_process.exe")));
  EXPECT_SUCCEEDED(process_monitor.Stop());
}

TEST(GoopdumpProcessMonitorTest, TestStartStopNoopPatterns) {
  ProcessMonitor process_monitor;
  std::vector<CString> patterns;
  patterns.push_back(CString(_T("some_noop_process")));
  patterns.push_back(CString(_T("this_process")));
  patterns.push_back(CString(_T("that_process")));
  EXPECT_SUCCEEDED(process_monitor.StartWithPatterns(NULL, patterns));
  EXPECT_SUCCEEDED(process_monitor.Stop());
}

TEST(GoopdumpProcessMonitorTest, TestStartStopNoopWithCallback) {
  ProcessMonitor process_monitor;
  MockProcessMonitorCallback callback;
  EXPECT_SUCCEEDED(process_monitor.Start(&callback,
                                         _T("some_noop_process.exe")));
  EXPECT_SUCCEEDED(process_monitor.Stop());

  EXPECT_EQ(0, callback.num_times_onprocessadded_called());
  EXPECT_EQ(0, callback.num_times_onprocessremoved_called());
}

TEST(GoopdumpProcessMonitorTest, TestStartStopNotepadWithCallback) {
  ProcessMonitor process_monitor;
  MockProcessMonitorCallback callback;
  EXPECT_SUCCEEDED(process_monitor.Start(&callback,
                                         _T("notepad.exe")));
  TCHAR notepad_path[MAX_PATH] = {0};
  TCHAR system_directory[MAX_PATH] = {0};
  ::GetSystemDirectory(system_directory, arraysize(system_directory));
  _stprintf(notepad_path,
            arraysize(notepad_path),
            _T("%s\\notepad.exe"),
            system_directory);

  PROCESS_INFORMATION pi = {0};
  System::StartProcess(NULL, notepad_path, &pi);
  ::CloseHandle(pi.hThread);
  scoped_process process_handle(pi.hProcess);

  // Have to sleep since we're polling for process start/exit.
  // In order to get notified of process start we have to write a driver.
  ::Sleep(500);

  ::TerminateProcess(get(process_handle), 0);

  ::Sleep(500);

  EXPECT_SUCCEEDED(process_monitor.Stop());

  EXPECT_EQ(1, callback.num_times_onprocessadded_called());
  EXPECT_EQ(1, callback.num_times_onprocessremoved_called());
}

}  // namespace omaha

