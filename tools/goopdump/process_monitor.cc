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

#include "omaha/tools/goopdump/process_monitor.h"

#include <tlhelp32.h>
#include <map>
#include <vector>

#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/logging.h"
#include "omaha/tools/goopdump/process_commandline.h"

namespace omaha {

ProcessMonitor::ProcessMonitor() : is_running_(false), callback_(NULL) {
}

ProcessMonitor::~ProcessMonitor() {
}

HRESULT ProcessMonitor::Start(ProcessMonitorCallbackInterface* callback,
                              const TCHAR* process_name_pattern) {
  std::vector<CString> patterns;
  patterns.push_back(CString(process_name_pattern));
  return StartWithPatterns(callback, patterns);
}

HRESULT ProcessMonitor::StartWithPatterns(
    ProcessMonitorCallbackInterface* callback,
    const std::vector<CString>& process_name_patterns) {
  SingleLock lock(&lock_);

  if (is_running_) {
    return E_UNEXPECTED;
  }

  EnableDebugPrivilege();

  std::vector<CString>::const_iterator it = process_name_patterns.begin();
  for (; it != process_name_patterns.end(); ++it) {
    CString pattern = *it;
    pattern.MakeLower();
    process_name_patterns_.push_back(pattern);
  }

  callback_ = callback;

  reset(event_thread_exit_, ::CreateEvent(NULL, FALSE, FALSE, NULL));
  if (!valid(event_thread_exit_)) {
    return HRESULTFromLastError();
  }

  reset(monitor_thread_, ::CreateThread(NULL,
                                        0,
                                        &ProcessMonitor::MonitorThreadProc,
                                        this,
                                        0,
                                        NULL));
  if (!valid(monitor_thread_)) {
    return HRESULTFromLastError();
  }

  is_running_ = true;
  return S_OK;
}

HRESULT ProcessMonitor::Stop() {
  SingleLock lock(&lock_);

  if (!is_running_) {
    return E_UNEXPECTED;
  }

  ::SetEvent(get(event_thread_exit_));
  ::WaitForSingleObject(get(monitor_thread_), INFINITE);

  is_running_ = false;
  return S_OK;
}

DWORD ProcessMonitor::MonitorThreadProc(void* param) {
  ProcessMonitor* monitor = static_cast<ProcessMonitor*>(param);
  if (monitor) {
    return monitor->MonitorProc();
  }
  return 1;
}

DWORD ProcessMonitor::MonitorProc() {
  // 200ms idle between polling for process creation.
  const DWORD kWaitTimeoutMs = 200;

  MapHandleToDword map_handle_pid;
  bool keep_running = true;

  do {
    size_t map_count = map_handle_pid.size();
    size_t pids_to_monitor = (map_count < MAXIMUM_WAIT_OBJECTS) ?
        map_count : MAXIMUM_WAIT_OBJECTS;

    std::vector<HANDLE> handles;
    handles.push_back(get(event_thread_exit_));

    MapHandleToDwordIterator iter = map_handle_pid.begin();
    for (; (iter != map_handle_pid.end()) && (handles.size() < pids_to_monitor);
         ++iter) {
      handles.push_back(iter->first);
    }

    DWORD wait_result = ::WaitForMultipleObjects(handles.size(),
                                                 &handles.front(),
                                                 FALSE,
                                                 kWaitTimeoutMs);
    if (wait_result < (WAIT_OBJECT_0 + handles.size())) {
      HANDLE signalled = handles[wait_result - WAIT_OBJECT_0];

      if (signalled == get(event_thread_exit_)) {
        // We've been signalled to exit.
        keep_running = false;
      } else {
        // One of the PIDs exited.
        MapHandleToDwordIterator iter = map_handle_pid.begin();
        for (; iter != map_handle_pid.end(); ++iter) {
          if (iter->first == signalled) {
            OnProcessRemoved(iter->second);
            ::CloseHandle(iter->first);
            map_handle_pid.erase(iter);
            break;
          }
        }
      }
    } else if (wait_result == WAIT_TIMEOUT) {
      // Our polling time is up.  Go look for running instances of the
      // process we're monitoring and look for differences in the list of PIDs.
      UpdateProcessList(&map_handle_pid);
    } else {
      // Some type of failure occurred.
      keep_running = false;
    }
  } while (keep_running);

  return 0;
}

void ProcessMonitor::CleanupHandleMap(MapHandleToDword* map_handle_pid) {
  ASSERT1(map_handle_pid);
  MapHandleToDwordIterator iter = map_handle_pid->begin();
  for (; iter != map_handle_pid->end(); ++iter) {
    ::CloseHandle(iter->first);
  }
  map_handle_pid->clear();
}

bool ProcessMonitor::UpdateProcessList(MapHandleToDword* map_handle_pid) {
  ASSERT1(map_handle_pid);
  scoped_handle process_snap;
  reset(process_snap, ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
  if (!valid(process_snap)) {
    return false;
  }

  PROCESSENTRY32 process_entry32 = {0};
  process_entry32.dwSize = sizeof(PROCESSENTRY32);

  if (!::Process32First(get(process_snap), &process_entry32)) {
    return false;
  }

  do {
    CString exe_file_name = process_entry32.szExeFile;
    exe_file_name.MakeLower();

    typedef std::vector<CString>::iterator VectorIterator;
    VectorIterator pattern_iter = process_name_patterns_.begin();
    for (; pattern_iter != process_name_patterns_.end(); ++pattern_iter) {
      CString process_pattern = *pattern_iter;
      if (exe_file_name.Find(process_pattern) >= 0) {
        // We've found a match.  See if this ProcessID is already in our list
        bool is_found = false;
        MapHandleToDwordIterator iter = map_handle_pid->begin();
        for (; iter != map_handle_pid->end(); ++iter) {
          if (iter->second == process_entry32.th32ProcessID) {
            is_found = true;
            break;
          }
        }

        if (!is_found) {
          // Opening this handle and we'll give ownership of this HANDLE to
          // map_handle_pid.
          HANDLE process_handle = ::OpenProcess(PROCESS_ALL_ACCESS,
                                                FALSE,
                                                process_entry32.th32ProcessID);
          if (process_handle) {
            (*map_handle_pid)[process_handle] = process_entry32.th32ProcessID;
            OnProcessAdded(process_entry32.th32ProcessID, process_pattern);
          }
        }
      }
    }
  } while (::Process32Next(get(process_snap), &process_entry32));

  return true;
}

void ProcessMonitor::OnProcessAdded(DWORD process_id,
                                    const CString& process_pattern) {
  if (callback_) {
    callback_->OnProcessAdded(process_id, process_pattern);
  }
}

void ProcessMonitor::OnProcessRemoved(DWORD process_id) {
  if (callback_) {
    callback_->OnProcessRemoved(process_id);
  }
}

}  // namespace omaha

