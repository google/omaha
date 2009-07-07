// Copyright 2003-2009 Google Inc.
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
// proc_utils.cpp
//
// Useful functions that relate to process/thread manipulation/information
// (Originally moved from utils.cpp)

#include "omaha/common/proc_utils.h"

#include <psapi.h>
#include "base/scoped_ptr.h"
#include "omaha/common/app_util.h"
#include "omaha/common/const_config.h"
#include "omaha/common/const_timeouts.h"
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/logging.h"
#include "omaha/common/process.h"
#include "omaha/common/reg_key.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/string.h"
#include "omaha/common/utils.h"
#include "omaha/common/window_utils.h"

namespace omaha {

ProcessTerminator::ProcessTerminator(const CString& process_name)
    : recursion_level_(0),
      process_name_(process_name),
      flash_window_(false),
      session_id_(INVALID_SESSION_ID) {
  MakeLowerCString(process_name_);
}

ProcessTerminator::ProcessTerminator(const CString& process_name,
                                     const CString& user_sid)
    : recursion_level_(0),
      user_sid_(user_sid),
      process_name_(process_name),
      flash_window_(false),
      session_id_(INVALID_SESSION_ID) {
  MakeLowerCString(process_name_);
}

ProcessTerminator::ProcessTerminator(const CString& process_name,
                                     const CString& user_sid,
                                     int session_id)
    : recursion_level_(0),
      user_sid_(user_sid),
      session_id_(session_id),
      process_name_(process_name),
      flash_window_(false) {
  MakeLowerCString(process_name_);
}

ProcessTerminator::~ProcessTerminator() {
  CloseAllHandles();
}

// Will close all currently opened handles.
void ProcessTerminator::CloseAllHandles() {
  UTIL_LOG(L3, (_T("[CloseAllHandles]")));
  // Do clean up if we have opened handles.
  for (size_t i = 0; i < process_handles_.size(); i++) {
    VERIFY1(::CloseHandle(process_handles_[i]));
  }

  process_handles_.clear();
}

// Wait for a while till all process instances will die.
bool ProcessTerminator::WaitForProcessInstancesToDie(
    uint32 timeout_msec) const {
  UTIL_LOG(L3, (_T("[WaitForProcessInstancesToDie]")));
  size_t size = process_handles_.size();
  scoped_array<HANDLE> handles(new HANDLE[size]);

  for (size_t i = 0; i < size; i++) {
    handles[i] = process_handles_[i];
  }

  DWORD wait_result = ::WaitForMultipleObjectsEx(size,
                                                 handles.get(),
                                                 true,
                                                 timeout_msec,
                                                 false);
#pragma warning(disable : 4296)
// C4296: '>=' : expression is always true
  if ((wait_result >= WAIT_OBJECT_0) &&
      (wait_result < WAIT_OBJECT_0 + size)) {
    return true;
  }
#pragma warning(default : 4296)

  UTIL_LOG(L3, (_T("WaitForProcessToDie timed out for '%s'. Waited for %d ms."),
                process_name_, timeout_msec));
  return false;
}

// Finds all process ids for the process of a given name.
bool ProcessTerminator::FindProcessInstances() {
  UTIL_LOG(L3, (_T("[FindProcessInstances]")));

  DWORD exclude_mask = EXCLUDE_CURRENT_PROCESS;
  if (!user_sid_.IsEmpty()) {
    exclude_mask |= INCLUDE_ONLY_PROCESS_OWNED_BY_USER;
  }

  std::vector<CString> command_lines;
  HRESULT hr = S_OK;
  if (session_id_ != INVALID_SESSION_ID) {
    hr = Process::FindProcessesInSession(session_id_,
                                         exclude_mask,
                                         process_name_,
                                         true,
                                         user_sid_,
                                         command_lines,
                                         &process_ids_);
  } else {
    hr = Process::FindProcesses(exclude_mask,
                                process_name_,
                                true,
                                user_sid_,
                                command_lines,
                                &process_ids_);
  }

  return SUCCEEDED(hr) && !process_ids_.empty();
}

// Tries to kill all instances of the process that was specified in the
// constructor.
// 'method_mask' determines which technique to attempt.
// 'was_found' is optional and can be NULL.
// Returns S_OK if all instances were killed, S_FALSE if process wasn't running,
// and E_FAIL if one or more instances weren't killed.
// Always sets 'was_found' correctly, regardless of return value.
HRESULT ProcessTerminator::KillTheProcess(uint32 timeout_msec,
                                          bool* was_found,
                                          uint32 method_mask,
                                          bool flash_window) {
  UTIL_LOG(L3, (_T("[KillTheProcess]")));
  if (!FindProcessInstances()) {
    if (was_found != NULL) {
      *was_found = false;
    }
    return S_FALSE;  // process is not running, so don't return a FAILED hr
  }

  // If got here, found at least one process to kill
  if (was_found != NULL) {
    *was_found = true;
  }

  flash_window_ = flash_window;
  // Try the nicest, cleanest method of closing a process: window messages
  if (method_mask & KILL_METHOD_1_WINDOW_MESSAGE) {
    if (PrepareToKill(KILL_METHOD_1_WINDOW_MESSAGE)) {
      KillProcessViaWndMessages(timeout_msec);
    }

    // Are any instances of the process still running?
    if (!FindProcessInstances()) {
      return S_OK;  // killed them all
    }
  }

  // Also nice method
  if (method_mask & KILL_METHOD_2_THREAD_MESSAGE) {
    if (PrepareToKill(KILL_METHOD_2_THREAD_MESSAGE)) {
      KillProcessViaThreadMessages(timeout_msec);
    }
    // Are any instances of the process still running?
    if (!FindProcessInstances()) {
      return S_OK;  // killed them all
    }
  }

  // the crude one.
  if (method_mask & KILL_METHOD_4_TERMINATE_PROCESS) {
    if (PrepareToKill(KILL_METHOD_4_TERMINATE_PROCESS)) {
      KillProcessViaTerminate(timeout_msec);
    }
    // Are any instances of the process still running?
    if (!FindProcessInstances()) {
      return S_OK;  // killed them all
    }

    UTIL_LOG(LEVEL_ERROR, (_T("[ProcessTerminator::KillTheProcess]")
                           _T("[totally unable to kill process '%s']"),
                           process_name_));
  }

  return E_FAIL;
}

HRESULT ProcessTerminator::WaitForAllToDie(uint32 timeout_msec) {
  UTIL_LOG(L3, (_T("[WaitForAllToDie]")));
  if (!FindProcessInstances()) {
    return S_OK;
  }

  if (PrepareToKill(KILL_METHOD_1_WINDOW_MESSAGE)) {
    return WaitForProcessInstancesToDie(timeout_msec) ? S_OK :
              HRESULT_FROM_WIN32(WAIT_TIMEOUT);
  }

  return E_FAIL;
}

// Given process_ids array will try to
// open handle to each instance.
// Leaves process handles open (in member process_handles_)
// Will use access rights for opening appropriate for the purpose_of_opening.
// This function recursively calls itself if by the time it tries to open
// handles to process instances some of the processes died or naturally exited.
bool ProcessTerminator::PrepareToKill(uint32 method_mask) {
  UTIL_LOG(L3, (_T("[PrepareToKill]")));
  uint32 desired_access = 0;

  if (method_mask & KILL_METHOD_4_TERMINATE_PROCESS) {
    desired_access = SYNCHRONIZE       |
                     PROCESS_TERMINATE |
                     PROCESS_QUERY_INFORMATION;
  } else {
    desired_access = SYNCHRONIZE | PROCESS_QUERY_INFORMATION | PROCESS_VM_READ;
  }

  // do clean up in case some handles are opened.
  CloseAllHandles();

  if (process_ids_.empty()) {
    // no instances are running.
    return false;
  }

  for (size_t i = 0; i < process_ids_.size(); i++) {
    HANDLE handle = ::OpenProcess(desired_access, false, process_ids_[i]);
    if (handle) {
      process_handles_.push_back(handle);
    } else {
      if (::GetLastError() == ERROR_ACCESS_DENIED) {
        // If we are here that means that we do not have enough priveleges to
        // open the process for a given kill method. No reason to attempt other
        // instances. Just clean up and return false.
        UTIL_LOG(L3, (_T("PrepareToKill failed for '%s'. Kill method %d."),
                      process_name_, method_mask));
        CloseAllHandles();
        return false;
      }
    }
  }
  // We already handled the case when we don't have enough privileges to open
  // the process. So if we have less handles than process ids -> some of the
  // processes have died since we made a snapshot untill the time we tried to
  // open handles. We need to do another snapshot and try to open handles one
  // more time. We need number of handles and number of ids to be equal.
  // We can do it with recursion. The idea is: make the next snapshot and open
  // handles. Hopefully the number will be equal. Stop recursion at the third
  // level.

  if (process_handles_.size() != process_ids_.size()) {
    recursion_level_++;

    // we have a disbalance here. This is pretty bad.
    // Some of the processes died already so let's try
    // to balance them.
    if (!FindProcessInstances()) {
      // they are all dead.
      recursion_level_ = 0;
      return false;
    }

    // try to balance three times no more.
    if (recursion_level_ >= 3) {
      recursion_level_ = 0;
      UTIL_LOG(L3, (_T("Recursion level too deep in PrepareToKill for '%s'."),
                    process_name_));
      return false;
    }

    // recursively call the function
    return PrepareToKill(method_mask);
  }
  recursion_level_ = 0;
  return true;
}

// ProcessTerminator::FindProcessWindows
// Just calls enumeration function
bool ProcessTerminator::FindProcessWindows() {
  window_handles_.clear();
  return ::EnumWindows(EnumAllWindowsProc, reinterpret_cast<LPARAM>(this)) &&
         !window_handles_.empty();
}

// ProcessTerminator::EnumAllWindowsProc
// During enumeration this function will try to find a match between
// process id we already found and process id obtained from each window.
// if there is a match, we record the window in an array
BOOL ProcessTerminator::EnumAllWindowsProc(HWND hwnd, LPARAM lparam) {
  ProcessTerminator* this_pointer =
        reinterpret_cast<ProcessTerminator*>(lparam);
  ASSERT1(this_pointer);

  uint32 process_id = 0;
  uint32 thread_id =
    ::GetWindowThreadProcessId(hwnd, reinterpret_cast<DWORD*>(&process_id));

  typedef std::vector<uint32>::const_iterator ProcessIdIterator;
  for (ProcessIdIterator it = this_pointer->process_ids_.begin();
       it != this_pointer->process_ids_.end();
       ++it) {
    if (*it == process_id) {
      // The main idea is: Find all top level windows (NO PARENT!!!)
      // AND this windows must have system menu and be visible. So we make sure
      // that we send WM_CLOSE ONLY to the windows that user might close
      // interactively. This way we are safe. The last thing to check is if it
      // is tr hidden window.
      if (WindowUtils::IsMainWindow(hwnd) && WindowUtils::HasSystemMenu(hwnd)) {
        this_pointer->window_handles_.push_back(hwnd);
      }
    }
  }
  return TRUE;
}

// ProcessTerminator::KillProcessViaWndMessages()
// try to post a windows message
bool  ProcessTerminator::KillProcessViaWndMessages(uint32 timeout_msec) {
  UTIL_LOG(L3, (_T("[KillProcessViaWndMessages]")));
  if (!FindProcessWindows()) {
    UTIL_LOG(L1, (_T("[KillProcessViaWndMessages]")
                  _T("[failed to find any windows for '%s']"), process_name_));
    return false;
  }

  bool post_messages_succeeded = false;

  for (size_t i = 0; i < window_handles_.size(); i++) {
    // Previous method used WM_CLOSE, WM_SYSCOMMAND+SC_CLOSE is slightly better.
    // It closes our apps, and also works correctly on AOL!
    if (::PostMessage(window_handles_[i], WM_SYSCOMMAND, SC_CLOSE, 0)) {
      if (flash_window_) {
        UTIL_LOG(L3, (_T("[PostMessageSucceeded flashing window]")));
        ::FlashWindow(window_handles_[i], true);
      }
      post_messages_succeeded = true;
    }
  }

  if (!post_messages_succeeded) {
    UTIL_LOG(L3, (_T("[KillProcessViaWndMessages]")
                  _T("[failed to PostMessage to windows of '%s']"),
                  process_name_));
  }
  // If we succeeded in posting message at least one time we have to wait.
  // We don't know the relationship between windows in the process.
  return post_messages_succeeded && WaitForProcessInstancesToDie(timeout_msec);
}

// Try to post a thread message.
bool ProcessTerminator::KillProcessViaThreadMessages(uint32 timeout_msec) {
  UTIL_LOG(L3, (_T("[KillProcessViaThreadMessages]")));
  std::vector<uint32> thread_ids;

  if (!FindProcessThreads(&thread_ids)) {
    UTIL_LOG(L3, (_T("[KillProcessViaThreadMessages]")
                  _T("[failed to find any threads for '%s']"), process_name_));
    return false;
  }

  bool post_messages_succeeded = false;
  for (size_t i = 0; i < thread_ids.size(); i++) {
    if (::PostThreadMessage(thread_ids[i], WM_CLOSE, 0, 0)) {
      post_messages_succeeded = true;
    }
  }

  if (!post_messages_succeeded) {
    UTIL_LOG(L3, (_T("[KillProcessViaWndMessages]")
                  _T("[failed to PostMessage to threads of '%s'."),
                  process_name_));
  }
  // If we succeded in posting message to at least one thread we have to wait.
  // We don't know the relationship between threads in the process.
  return post_messages_succeeded && WaitForProcessInstancesToDie(timeout_msec);
}

// find all the threads running in a given process.
bool ProcessTerminator::FindProcessThreads(std::vector<uint32>* thread_ids) {
  HANDLE process_snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
  if (process_snapshot == INVALID_HANDLE_VALUE) {
    return false;
  }

  THREADENTRY32 thread_info = {0};  // zero it out just in case.
  thread_info.dwSize = sizeof(THREADENTRY32);

  if (::Thread32First(process_snapshot, &thread_info))  {
    do {
      for (std::vector<uint32>::const_iterator it = process_ids_.begin();
           it != process_ids_.end(); ++it) {
        if (*it == thread_info.th32OwnerProcessID) {
          // we have found it.
          thread_ids->push_back(thread_info.th32ThreadID);
        }
      }
      // system changes this value, do not forget to reset to
      // max possible.
      thread_info.dwSize = sizeof(THREADENTRY32);
    } while (::Thread32Next(process_snapshot, &thread_info));
  }

  return !thread_ids->empty();
}

// Last and crude method to kill the process. Should be used only
// if all other methods have failed.
bool ProcessTerminator::KillProcessViaTerminate(uint32 timeout_msec) {
  UTIL_LOG(L3, (_T("[KillProcessViaTerminate]")));
  bool at_least_one_terminated = false;

  for (size_t i = 0; i < process_handles_.size(); i++) {
    if (!::TerminateProcess(process_handles_[i], 0)) {
      UTIL_LOG(L3, (_T("[KillProcessViaTerminate]")
                    _T("[failed for instance of '%s'][System error %d]"),
                    process_name_, ::GetLastError()));
    } else {
       at_least_one_terminated = true;
    }
  }
  return at_least_one_terminated ? WaitForProcessInstancesToDie(timeout_msec) :
                                   false;
}

HRESULT SetProcessSilentShutdown() {
  DWORD shut_down_level(0), shut_down_flags(0);
  if (!::GetProcessShutdownParameters(&shut_down_level, &shut_down_flags)) {
    return HRESULTFromLastError();
  }
  shut_down_flags |= SHUTDOWN_NORETRY;
  if (!::SetProcessShutdownParameters(shut_down_level, shut_down_flags)) {
    return HRESULTFromLastError();
  }
  return S_OK;
}

}  // namespace omaha

