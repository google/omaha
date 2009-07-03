// Copyright 2004-2009 Google Inc.
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
// Defines class Process to incapsulate win32
// functions for creation and some manipulations of
// processes.

#include "omaha/common/process.h"

#include <ntsecapi.h>
#include <psapi.h>
#include <stierr.h>
#include <tlhelp32.h>
#include <vector>

#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) ((NTSTATUS)(Status) >= 0)
#endif

#include "omaha/common/debug.h"
#include "omaha/common/disk.h"
#include "omaha/common/error.h"
#include "omaha/common/logging.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/string.h"
#include "omaha/common/system.h"
#include "omaha/common/system_info.h"
#include "omaha/common/utils.h"
#include "omaha/common/user_info.h"
#include "omaha/common/window_utils.h"

namespace omaha {

const int kNumRetriesToFindProcess = 4;
const int kFindProcessRetryIntervalMs = 500;
const int kMaxCmdLineLengthBytes = 4096;

// Constructor
Process::Process(const TCHAR* name, const TCHAR* window_class_name)
    : process_id_(0),
      exit_code_(0),
      number_of_restarts_(static_cast<uint32>(-1)),
      name_(name),
      shutdown_event_(NULL) {
  ASSERT1(name);

  command_line_ = name;
  window_class_name_ = window_class_name;
}

// Constructor
Process::Process(uint32 process_id)
    : process_id_(process_id),
      exit_code_(0),
      number_of_restarts_(static_cast<uint32>(-1)),
      name_(itostr(static_cast<uint32>(process_id))),
      shutdown_event_(NULL) {
  reset(process_, ::OpenProcess(PROCESS_QUERY_INFORMATION | SYNCHRONIZE,
                                false,
                                process_id));
  if (!valid(process_)) {
    UTIL_LOG(LEVEL_ERROR,
             (_T("[Process::Process - failed to open process][%u][0x%x]"),
              process_id, HRESULTFromLastError()));
  }
}

// Destructor
Process::~Process() {
}

// Start with command params
bool Process::Start(const TCHAR* command_line_parameters) {
  return StartWithHwnd(command_line_parameters, NULL);
}

// Start with command params and specific hwnd
bool Process::StartWithHwnd(const TCHAR* command_line_parameters, HWND hwnd) {
  // command_line_parameters may be NULL
  // hwnd may be NULL

  // Can't start the same process twice in the same
  // containing object.
  if (Running()) {
    return false;
  }

  // Add command line params if any.
  if (command_line_parameters && *command_line_parameters) {
    command_line_parameters_ = command_line_parameters;
  }

  // just reuse the existing function, don't repeat the code.
  number_of_restarts_ = static_cast<uint32>(-1);
  time_of_start_      = GetTickCount();
  return Restart(hwnd);
}

// Restart with the old command params
bool Process::Restart(HWND hwnd) {
  // Can't start the same process twice in the same
  // containing object.
  if (Running()) {
    return false;
  }

  // start the process.
  HRESULT hr = System::ShellExecuteProcess(command_line_,
                                           command_line_parameters_,
                                           hwnd,
                                           address(process_));

  if (SUCCEEDED(hr)) {
    process_id_ = GetProcessIdFromHandle(get(process_));
    ASSERT1(process_id_);
    number_of_restarts_++;
  } else {
    UTIL_LOG(LEVEL_ERROR,
             (_T("System::ShellExecuteProcess '%s' failed with 0x%08x"),
             command_line_, hr));
  }

  return SUCCEEDED(hr);
}

// Check if the process is running.
bool Process::Running() const {
  if (!get(process_)) {
    return false;
  }

  return (::WaitForSingleObject(get(process_), 0) == WAIT_TIMEOUT);
}

// Create a job and assign the process to it
HANDLE Process::AssignToJob() {
  // Make sure that the process handle is valid
  if (!get(process_)) {
    return false;
  }

  // Create a job
  scoped_job job(::CreateJobObject(NULL, NULL));
  if (!valid(job)) {
    UTIL_LOG(LEVEL_ERROR,
             (_T("[Process::AssignToJob - CreateJobObject failed][0x%x]"),
              HRESULTFromLastError()));
    return false;
  }

  // Assign the process to the job
  if (!::AssignProcessToJobObject(get(job), get(process_))) {
    UTIL_LOG(LEVEL_ERROR,
             (_T("[Process::AssignToJob-AssignProcessToJobObject fail][0x%x]"),
              HRESULTFromLastError()));
    return false;
  }

  return release(job);
}

// Wait till the process finishes
bool Process::WaitUntilDead(uint32 timeout_msec) {
  ASSERT1(timeout_msec);

  if (!get(process_)) {
    return false;
  }

  uint32 ret = 0;
  if (shutdown_event_) {
    HANDLE wait_handles[2] = {0};
    wait_handles[0] = get(process_);
    wait_handles[1] = shutdown_event_;
    ret = ::WaitForMultipleObjectsEx(2,
                                     wait_handles,
                                     false,
                                     timeout_msec,
                                     true);
  } else {
    ret = ::WaitForSingleObjectEx(get(process_), timeout_msec, true);
  }
  if (ret == WAIT_OBJECT_0) {
    UTIL_LOG(L2, (_T("[Process::WaitUntilDead - succeeded to wait process]")
                  _T("[%s]"), GetName()));
    return true;
  } else if (ret == WAIT_IO_COMPLETION) {
    UTIL_LOG(LEVEL_ERROR, (_T("[Process::WaitUntilDead-recv APC][%s][%u][%u]"),
                           GetName(), process_id_));
    return false;
  } else {
    UTIL_LOG(LEVEL_ERROR, (_T("[Process::WaitUntilDead - fail to wait process,")
                           _T("possibly timeout][%s][%u][%u]"),
                           GetName(), process_id_, ret));
    return false;
  }
}

// Wait some time till the process and all its descendent processes finish
//
// Background:
//   Some process might spawn another process and get itself terminated
// without waiting the descendant process to finish.
//
// Args:
//   job:                Job to which the process is assigned
//                       AssignToJob() will be called when NULL value is passed
//   timeout_msec:       Timeout value in msec
//   path_to_exclude:    Path of descendant process to excluded from waiting
//                       (this should be in long format)
//   exit_code:          To hold the exit code being returned
bool Process::WaitUntilAllDead(HANDLE job,
                               uint32 timeout_msec,
                               const TCHAR* path_to_exclude,
                               uint32* exit_code) {
  ASSERT1(timeout_msec);

  UTIL_LOG(L2, (_T("[Process::WaitUntilAllDead][%u][%s]"),
                timeout_msec, path_to_exclude));

  if (exit_code) {
  *exit_code = 0;
  }

  scoped_job job_guard;
  if (!job) {
    reset(job_guard, AssignToJob());
    if (!valid(job_guard)) {
      return false;
    }
    job = get(job_guard);
  }

  return InternalWaitUntilAllDead(job,
                                  timeout_msec,
                                  path_to_exclude,
                                  exit_code);
}

// Helper function to wait till the process and all its descendent processes
// finish.
bool Process::InternalWaitUntilAllDead(HANDLE job,
                                       uint32 timeout_msec,
                                       const TCHAR* path_to_exclude,
                                       uint32* exit_code) {
  ASSERT1(job);
  ASSERT1(timeout_msec);

  // Wait until current process finishes
  if (!WaitUntilDead(timeout_msec)) {
    return false;
  }

  // Find descendant process
  uint32 desc_process_id = GetDescendantProcess(
                               job,
                               false,  // child_only
                               exit_code != NULL,  // sole_descendent
                               NULL,  // search_name
                               path_to_exclude);

  if (desc_process_id) {
    // Open descendent process
    Process desc_process(desc_process_id);

    // If descendant process dies too soon, do not need to wait for it
    if (desc_process.Running()) {
      // Release the parent process handle
      // This to handle the scenario that Firefox uninstall code will wait till
      // parent process handle becomes NULL
      reset(process_);

      UTIL_LOG(L2, (_T("[Process::InternalWaitUntilAllDead]")
                    _T("[waiting descendant process][%u]"), desc_process_id));

      // Propagate the shutdown event to descendent process
      if (shutdown_event_) {
        desc_process.SetShutdownEvent(shutdown_event_);
      }

      // Wait till descendant process finishes
      bool wait_ret = desc_process.InternalWaitUntilAllDead(job,
                                                            timeout_msec,
                                                            path_to_exclude,
                                                            exit_code);

      return wait_ret;
    }
  }

  // Use the exit code from parent process
  if (exit_code) {
  VERIFY1(GetExitCode(exit_code));
  }

  // Release the parent process handle
  reset(process_);

  return true;
}

// Wait until process is dead or a windows message arrives (for use in a message
// loop while waiting)
HRESULT Process::WaitUntilDeadOrInterrupt(uint32 msec) {
  if (!get(process_)) {
    return E_FAIL;
  }

  HANDLE events[1] = { get(process_) };
  uint32 dw = ::MsgWaitForMultipleObjects(1, events, FALSE, msec, QS_ALLEVENTS);
  switch (dw) {
    case WAIT_OBJECT_0:
      return CI_S_PROCESSWAIT_DEAD;
    case WAIT_OBJECT_0 + 1:
      return CI_S_PROCESSWAIT_MESSAGE;
    case WAIT_TIMEOUT:
      return CI_S_PROCESSWAIT_TIMEOUT;
    case WAIT_FAILED:
    default:
      return E_FAIL;
  }
}

#if !SHIPPING
CString Process::GetDebugInfo() const {
  return debug_info_;
}
#endif

// Return the process ID
uint32 Process::GetId() const {
  return process_id_;
}

// Return the process name
const TCHAR *Process::GetName() const {
  return name_;
}

// Return win32 handle to the process.
HANDLE Process::GetHandle() const {
  return get(process_);
}

// Get process exit code.
bool Process::GetExitCode(uint32* exit_code) const {
  ASSERT1(exit_code);

  if (!get(process_)) {
    return false;
  }

  if (!::GetExitCodeProcess(get(process_),
                            reinterpret_cast<DWORD*>(&exit_code_))) {
    UTIL_LOG(LEVEL_ERROR,
             (_T("[Process::GetExitCode - failed to get exit code][%u][0x%x]"),
              process_id_, HRESULTFromLastError()));
    return false;
  }
  if (exit_code_ == STILL_ACTIVE) {
    return false;
  }

  *exit_code = exit_code_;
  return true;
}

// default implementation allows termination
bool Process::IsTerminationAllowed() const {
  return true;
}

// Terminate the process. If wait_for_terminate_msec == 0 return value doesn't
// mean that the process actualy terminated. It becomes assync. operation.
// Check the status with Running accessor function in this case.
bool Process::Terminate(uint32 wait_for_terminate_msec) {
  if (!Running()) {
    return true;
  }

  if (!IsTerminationAllowed()) {
    return false;
  }

  if (!::TerminateProcess(get(process_), 1)) {
    return false;
  }

  return wait_for_terminate_msec ? WaitUntilDead(wait_for_terminate_msec) :
                                   true;
}

// Default returns INFINITE means never restart.
// Return any number of msec if overwriting
uint32 Process::GetRestartInterval() const {
  return INFINITE;
}

// How many times the process can be restarted
// in case it crashes. When overriding return any
// number or INFINITE to restart forever.
uint32 Process::GetMaxNumberOfRestarts() const {
  return 0;
}

// what is the time window for number of crashes returned by
// GetMaxNumberOfRestarts(). If crashed more that this number of restarts
// in a specified time window - do not restart it anymore.
// Default implementation returns INFINITE which means that this is not time
// based at all, if the process crashed more than the value returned by
// GetMaxNumberOfRestarts it will not be restarted no matter how long it took.
uint32 Process::GetTimeWindowForCrashes() const {
  return INFINITE;
}

uint32 Process::GetMaxMemory() const {
  return 0;
}

// Have we exceeded the number of maximum restarting?
bool Process::AllowedToRestart() const {
  uint32 max_number_of_restarts = GetMaxNumberOfRestarts();

  if ((max_number_of_restarts == INFINITE) ||
     (number_of_restarts_ < max_number_of_restarts)) {
    return true;
  }

  // process crashed too many times. Let's look at the rate of crashes.
  // Maybe we can "forgive" the process if it took some time for it to crash.
  if ((::GetTickCount() - time_of_start_) < GetTimeWindowForCrashes()) {
    return false;  // not forgiven
  }

  // Everything is forgiven. Give the process
  // new start in life.
  time_of_start_ = ::GetTickCount();
  number_of_restarts_ = static_cast<uint32>(-1);

  return true;
}

// Set shutdown event using in signaling the process watch
void Process::SetShutdownEvent(HANDLE shutdown_event) {
  ASSERT1(shutdown_event);

  shutdown_event_ = shutdown_event;
}

// Set priority class to the process.
bool Process::SetPriority(uint32 priority_class) const {
  if (!get(process_)) {
    return false;
  }

  VERIFY1(::SetPriorityClass(get(process_), priority_class));
  return true;
}

// Try to get a descendant process. Return process id if found.
uint32 Process::GetDescendantProcess(HANDLE job,
                                     bool child_only,
                                     bool sole_descedent,
                                     const TCHAR* search_name,
                                     const TCHAR* path_to_exclude) {
  ASSERT1(job);

  // Find all descendent processes
  std::vector<ProcessInfo> descendant_processes;
  if (FAILED(GetAllDescendantProcesses(job,
                                       child_only,
                                       search_name,
                                       path_to_exclude,
                                       &descendant_processes))) {
    return 0;
  }

  // If more than one decendent processes is found, filter out those that are
  // not direct children. This is because it might be the case that in a very
  // short period of time, process A spawns B and B spawns C, and we capture
  // both B and C.
  std::vector<ProcessInfo> child_processes;
  typedef std::vector<ProcessInfo>::const_iterator ProcessInfoConstIterator;
  if (descendant_processes.size() > 1) {
    for (ProcessInfoConstIterator it(descendant_processes.begin());
         it != descendant_processes.end(); ++it) {
      if (it->parent_id == process_id_) {
        child_processes.push_back(*it);
      }
    }
    if (!child_processes.empty()) {
      descendant_processes = child_processes;
    }
  }

  // Save the debugging information if needed
#if !SHIPPING
  if (sole_descedent && descendant_processes.size() > 1) {
    debug_info_ = _T("More than one descendent process is found for process ");
    debug_info_ += itostr(process_id_);
    debug_info_ += _T("\n");
    for (ProcessInfoConstIterator it(descendant_processes.begin());
         it != descendant_processes.end(); ++it) {
      debug_info_.AppendFormat(_T("%u %u %s\n"),
                               it->process_id,
                               it->parent_id,
                               it->exe_file);
    }
  }
#else
  sole_descedent;   // unreferenced formal parameter
#endif

  return descendant_processes.empty() ? 0 : descendant_processes[0].process_id;
}

BOOL Process::IsProcessInJob(HANDLE process_handle,
                             HANDLE job_handle,
                             PBOOL result)  {
  typedef BOOL (WINAPI *Fun)(HANDLE process_handle,
                             HANDLE job_handle,
                             PBOOL result);

  HINSTANCE kernel_instance = ::GetModuleHandle(_T("kernel32.dll"));
  ASSERT1(kernel_instance);
  Fun pfn = reinterpret_cast<Fun>(::GetProcAddress(kernel_instance,
                                                   "IsProcessInJob"));
  ASSERT(pfn, (_T("IsProcessInJob export not found in kernel32.dll")));
  return pfn ? (*pfn)(process_handle, job_handle, result) : FALSE;
}

// Try to get all matching descendant processes
HRESULT Process::GetAllDescendantProcesses(
                     HANDLE job,
                     bool child_only,
                     const TCHAR* search_name,
                     const TCHAR* path_to_exclude,
                     std::vector<ProcessInfo>* descendant_processes) {
  ASSERT1(job);
  ASSERT1(descendant_processes);

  // Take a snapshot
  // Note that we do not have a seperate scoped_* type defined to wrap the
  // handle returned by CreateToolhelp32Snapshot. So scoped_hfile with similar
  // behavior is used.
  scoped_hfile process_snap(::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
  if (!process_snap) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LEVEL_ERROR,
             (_T("[Process::GetAllDescendantProcesses - fail to get snapshot]")
              _T("[0x%x]"), hr));
    return hr;
  }

  // Eumerate all processes in the snapshot
  PROCESSENTRY32 pe32;
  SetZero(pe32);
  pe32.dwSize = sizeof(PROCESSENTRY32);
  if (!::Process32First(get(process_snap), &pe32)) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LEVEL_ERROR, (_T("[Process::GetAllDescendantProcesses - failed to")
                           _T("get first process][0x%x]"), hr));
    return hr;
  }

  do {
    // Skip process 0 and current process
    if (pe32.th32ProcessID == 0 || pe32.th32ProcessID == process_id_) {
      continue;
    }

    // If searching for child only, perform the check
    if (child_only && pe32.th32ParentProcessID != process_id_) {
      continue;
    }

    // Open the process
    scoped_process process(::OpenProcess(PROCESS_QUERY_INFORMATION |
                                         SYNCHRONIZE,
                                         false,
                                         pe32.th32ProcessID));
    if (!valid(process)) {
      continue;
    }

    // Determines whether the process is running in the specified job
    BOOL result = FALSE;
    if (!IsProcessInJob(get(process), job, &result) || !result) {
      continue;
    }

    // Check whether the process is still running
    if (::WaitForSingleObject(get(process), 0) != WAIT_TIMEOUT) {
      continue;
    }

    // Compare the name if needed
    if (search_name && *search_name) {
      if (_tcsicmp(pe32.szExeFile, search_name) != 0) {
        continue;
      }
    }

    // If we need to exclude certain path, check it now
    if (path_to_exclude && *path_to_exclude) {
      if (IsProcessRunningWithPath(pe32.th32ProcessID, path_to_exclude)) {
        continue;
      }
    }

    // Add to the list
    ProcessInfo proc_info;
    proc_info.process_id = pe32.th32ProcessID;
    proc_info.parent_id = pe32.th32ParentProcessID;
#if !SHIPPING
    proc_info.exe_file = pe32.szExeFile;
#endif
    descendant_processes->push_back(proc_info);
  } while (::Process32Next(get(process_snap), &pe32));

  return S_OK;
}

HRESULT Process::FindProcesses(uint32 exclude_mask,
                               const TCHAR* search_name,
                               bool search_main_executable_only,
                               std::vector<uint32>* process_ids_found) {
  ASSERT1(process_ids_found);
  // Remove the only include processes owned by user mask from the exclude
  // mask. This is needed as this is the behavior expected by the method,
  // before the addition of the user_sid.
  exclude_mask &= (~INCLUDE_ONLY_PROCESS_OWNED_BY_USER);
  std::vector<CString> command_lines;
  return FindProcesses(exclude_mask, search_name, search_main_executable_only,
                       _T(""), command_lines, process_ids_found);
}

bool Process::IsStringPresentInList(const CString& process_command_line,
                                    const std::vector<CString>& list) {
  std::vector<CString>::const_iterator iter = list.begin();
  for (; iter != list.end(); ++iter) {
    CString value_to_find = *iter;

    // If we are able to open the process command line, then we should
    // ensure that it does not contain the value that we are looking for.
    if (process_command_line.Find(value_to_find) != -1) {
      // Found a match.
      return true;
    }
  }

  return false;
}

// TODO(omaha): Change the implementation of this method to take in a
// predicate that determines whether a process should be included in the
// result set.
HRESULT Process::FindProcesses(uint32 exclude_mask,
                               const TCHAR* search_name,
                               bool search_main_executable_only,
                               const CString& user_sid,
                               const std::vector<CString>& command_lines,
                               std::vector<uint32>* process_ids_found) {
  ASSERT1(search_name && *search_name);
  ASSERT1(process_ids_found);
  ASSERT1(!((exclude_mask & EXCLUDE_PROCESS_COMMAND_LINE_CONTAINING_STRING) &&
            (exclude_mask & INCLUDE_PROCESS_COMMAND_LINE_CONTAINING_STRING)));

  const TCHAR* const kLocalSystemSid = _T("S-1-5-18");

  // Clear the output queue
  process_ids_found->clear();

  // Get the list of process identifiers.
  uint32 process_ids[kMaxProcesses];
  SetZero(process_ids);
  uint32 bytes_returned = 0;
  if (!::EnumProcesses(reinterpret_cast<DWORD*>(process_ids),
                       sizeof(process_ids),
                       reinterpret_cast<DWORD*>(&bytes_returned))) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LEVEL_ERROR, (_T("[Process::FindProcesses-fail to EnumProcesses]")
                           _T("[0x%x]"), hr));
    return hr;
  }

  // Enumerate all processes
  int num_processes = bytes_returned / sizeof(process_ids[0]);
  // We have found an elevated number of crashes in 1.2.584.15114 on what
  // we believe are Italian systems. The first step to solving this Italian job
  // is to assert on the condition while we are further testing this.
  ASSERT1(num_processes <= kMaxProcesses);

  // In Vista, SeDebugPrivilege is required to open the process not owned by
  // current user. Also required for XP admins to open Local System processes
  // with PROCESS_QUERY_INFORMATION access rights.
  System::AdjustPrivilege(SE_DEBUG_NAME, true);

  // Get ID of current process
  uint32 curr_process_id = ::GetCurrentProcessId();

  // Get SID of current user
  CString curr_user_sid;
  HRESULT hr = omaha::user_info::GetCurrentUser(NULL, NULL, &curr_user_sid);
  if (FAILED(hr)) {
    return hr;
  }

  UTIL_LOG(L4, (_T("[Process::FindProcesses][processes=%d]"), num_processes));
  for (int i = 0; i < num_processes; ++i) {
    // Skip the system idle process.
    if (process_ids[i] == 0) {
      continue;
    }

    // Get the owner sid.
    // Note that we may fail to get the owner which is not current user.
    // So if the owner_sid is empty, the process is sure not to be owned by the
    // current user.
    CString owner_sid;
    Process::GetProcessOwner(process_ids[i], &owner_sid);

    if ((exclude_mask & INCLUDE_ONLY_PROCESS_OWNED_BY_USER) &&
      owner_sid != user_sid) {
      UTIL_LOG(L4,
          (_T("[Excluding process as not owned by user][%d]"), process_ids[i]));
      continue;
    }

    // Skip it if it is owned by the one specified in exclude_mask
    if ((exclude_mask & EXCLUDE_CURRENT_PROCESS) &&
        process_ids[i] == curr_process_id) {
      UTIL_LOG(L4, (_T("[Excluding current process %d"), process_ids[i]));
      continue;
    }
    if ((exclude_mask & EXCLUDE_PROCESS_OWNED_BY_CURRENT_USER) &&
        owner_sid == curr_user_sid) {
      UTIL_LOG(L4,
          (_T("[Excluding process as owned by current user][%d]"),
           process_ids[i]));
      continue;
    }
    if ((exclude_mask & EXCLUDE_PROCESS_OWNED_BY_SYSTEM) &&
        owner_sid == kLocalSystemSid) {
      UTIL_LOG(L4,
          (_T("[Excluding process as owned by system][%d]"), process_ids[i]));
      continue;
    }
    if (exclude_mask & EXCLUDE_PROCESS_COMMAND_LINE_CONTAINING_STRING ||
        exclude_mask & INCLUDE_PROCESS_COMMAND_LINE_CONTAINING_STRING) {
      CString process_command_line;
      HRESULT hr = GetCommandLine(process_ids[i], &process_command_line);
      if (FAILED(hr)) {
        UTIL_LOG(L4,
          (_T("[Excluding process could not get command line][%d]"),
           process_ids[i]));
        continue;
      }

      // If we are able to open the process command line, then we should
      // ensure that it does not contain the value that we are looking for if
      // we are excluding the command line or that it contains the command line
      // that we are looking for in case the include switch is specified.
      bool present = IsStringPresentInList(process_command_line, command_lines);
      if ((present &&
            (exclude_mask & EXCLUDE_PROCESS_COMMAND_LINE_CONTAINING_STRING)) ||
          (!present &&
            (exclude_mask & INCLUDE_PROCESS_COMMAND_LINE_CONTAINING_STRING))) {
        UTIL_LOG(L4, (_T("[Process command line matches criteria][%d]'[%s]'"),
                 process_ids[i], process_command_line));
        continue;
      }
    }

    // If search_name is provided, make sure it matches
    if (Process::IsProcessUsingExeOrDll(process_ids[i],
                                        search_name,
                                        search_main_executable_only)) {
      UTIL_LOG(L4,
          (_T("[Including process][%d][%s]"), process_ids[i], search_name));
      process_ids_found->push_back(process_ids[i]);
    }
  }

  return S_OK;
}

HRESULT Process::FindProcessesInSession(
    DWORD session_id,
    uint32 exclude_mask,
    const TCHAR* search_name,
    bool search_main_executable_only,
    const CString& user_sid,
    const std::vector<CString>& cmd_lines,
    std::vector<uint32>* process_ids_found) {
  HRESULT hr = FindProcesses(exclude_mask,
                             search_name,
                             search_main_executable_only,
                             user_sid,
                             cmd_lines,
                             process_ids_found);
  if (FAILED(hr)) {
    return hr;
  }

  // Filter to processes running under session_id.
  std::vector<uint32>::iterator iter = process_ids_found->begin();
  while (iter != process_ids_found->end()) {
    uint32 process_pid = *iter;
    DWORD process_session = 0;
    hr = S_OK;
    if (!::ProcessIdToSessionId(process_pid, &process_session)) {
      hr = HRESULTFromLastError();
      UTIL_LOG(LE,  (_T("[::ProcessIdToSessionId failed][0x%x]"), hr));
    }

    if (FAILED(hr) || process_session != session_id) {
      // Remove from list and continue.
      iter = process_ids_found->erase(iter);
      continue;
    }

    ++iter;
  }

  return S_OK;
}

bool Process::IsModuleMatchingExeOrDll(const TCHAR* module_name,
                                       const TCHAR* search_name,
                                       bool is_fully_qualified_name) {
  UTIL_LOG(L4, (_T("[Process::IsModuleMatchingExeOrDll]")
                _T("[module=%s][search=%s]"), module_name, search_name));
  CString module_file_name;
  if (is_fully_qualified_name) {
    if (FAILED(GetLongPathName(module_name, &module_file_name))) {
      return false;
    }
  } else {
    module_file_name = ::PathFindFileName(module_name);
    ASSERT1(!module_file_name.IsEmpty());
    if (module_file_name.IsEmpty()) {
      return false;
    }
  }

  return (module_file_name.CompareNoCase(search_name) == 0);
}

DWORD Process::GetProcessImageFileName(HANDLE proc_handle,
                                       LPTSTR image_file,
                                       DWORD file_size)  {
  typedef DWORD (WINAPI *Fun)(HANDLE proc_handle,
                              LPWSTR image_file,
                              DWORD file_size);

  HINSTANCE psapi_instance = ::GetModuleHandle(_T("Psapi.dll"));
  ASSERT1(psapi_instance);
  Fun pfn = reinterpret_cast<Fun>(::GetProcAddress(psapi_instance,
                                                   "GetProcessImageFileNameW"));
  if (!pfn) {
    UTIL_LOG(L1, (_T("::GetProcessImageFileNameW() not found in Psapi.dll")));
    return 0;
  }
  return (*pfn)(proc_handle, image_file, file_size);
}

bool Process::IsProcImageMatch(HANDLE proc_handle,
                               const TCHAR* search_name,
                               bool is_fully_qualified_name)  {
  TCHAR image_name[MAX_PATH] = _T("");
  if (!GetProcessImageFileName(proc_handle,
                               image_name,
                               arraysize(image_name))) {
    UTIL_LOG(L4, (_T("[GetProcessImageFileName fail[0x%x]"),
                  HRESULTFromLastError()));
    return false;
  }

  UTIL_LOG(L4, (_T("[GetProcessImageFileName][%s]"), image_name));
  CString dos_name;
  HRESULT hr(DevicePathToDosPath(image_name, &dos_name));
  if (FAILED(hr)) {
    UTIL_LOG(L4, (_T("[DevicePathToDosPath fail[0x%x]"), hr));
    return false;
  }

  return IsModuleMatchingExeOrDll(dos_name,
                                  search_name,
                                  is_fully_qualified_name);
}

// Is the process using the specified exe/dll?
bool Process::IsProcessUsingExeOrDll(uint32 process_id,
                                     const TCHAR* search_name,
                                     bool search_main_executable_only) {
  UTIL_LOG(L4, (_T("[Process::IsProcessUsingExeOrDll]")
                _T("[pid=%d][search_name=%s]"), process_id, search_name));
  ASSERT1(search_name);

  // Open the process
  scoped_process process_handle(::OpenProcess(
                                    PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                    FALSE,
                                    process_id));
  if (!process_handle) {
    UTIL_LOG(L4, (_T("[::OpenProcess failed][0x%x]"), HRESULTFromLastError()));
    return false;
  }

  // Does the name represent a fully qualified name?
  // We only do a simple check here
  bool is_fully_qualified_name = String_FindChar(search_name, _T('\\')) != -1;
  CString long_search_name;
  if (is_fully_qualified_name) {
    HRESULT hr(GetLongPathName(search_name, &long_search_name));
    if (FAILED(hr)) {
      UTIL_LOG(L4, (_T("[GetLongPathName fail][hr=x%x]"), hr));
      return false;
    }
    search_name = long_search_name;
  }

  // Take a snapshot of all modules in the specified process
  int num_modules_to_fetch = search_main_executable_only ? 1 :
                                                           kMaxProcessModules;
  HMODULE module_handles[kMaxProcessModules];
  SetZero(module_handles);
  uint32 bytes_needed = 0;
  if (!::EnumProcessModules(get(process_handle),
                            module_handles,
                            num_modules_to_fetch * sizeof(HMODULE),
                            reinterpret_cast<DWORD*>(&bytes_needed))) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LEVEL_ERROR, (_T("[EnumProcessModules failed][0x%x]"), hr));

    if (IsWow64(::GetCurrentProcessId())) {
      // ::EnumProcessModules from a WoW64 process fails for x64 processes.
      // We try ::GetProcessImageFileName as a workaround here.
      return search_main_executable_only ?
                 IsProcImageMatch(get(process_handle),
                                  search_name,
                                  is_fully_qualified_name) :
                 false;
    } else {
      return false;
    }
  }

  int num_modules = bytes_needed / sizeof(HMODULE);
  if (num_modules > num_modules_to_fetch) {
    num_modules = num_modules_to_fetch;
  }

  for (int i = 0; i < num_modules; ++i) {
    TCHAR module_name[MAX_PATH];
    SetZero(module_name);
    if (!::GetModuleFileNameEx(get(process_handle),
                               module_handles[i],
                               module_name,
                               arraysize(module_name))) {
      UTIL_LOG(LEVEL_ERROR, (_T("[GetModuleFileNameEx fail[x%x]"),
                             HRESULTFromLastError()));
      continue;
    }

    if (IsModuleMatchingExeOrDll(module_name,
                                 search_name,
                                 is_fully_qualified_name)) {
      return true;
    }
  }

  return false;
}

// Helper function to get long path name
HRESULT Process::GetLongPathName(const TCHAR* short_name, CString* long_name) {
  ASSERT1(short_name);
  ASSERT1(long_name);

  TCHAR temp_name[MAX_PATH];
  SetZero(temp_name);

  HRESULT hr = S_OK;
  if (!::GetLongPathName(short_name, temp_name, arraysize(temp_name))) {
    hr = HRESULTFromLastError();
  } else {
    long_name->SetString(temp_name);
  }

  return hr;
}

// Type definitions needed for GetCommandLine() and GetProcessIdFromHandle()
// From MSDN document on NtQueryInformationProcess() and other sources
typedef struct _PROCESS_BASIC_INFORMATION {
  PVOID Reserved1;
  BYTE *PebBaseAddress;
  PVOID Reserved2[2];
  ULONG_PTR UniqueProcessId;
  PVOID Reserved3;
} PROCESS_BASIC_INFORMATION;

typedef enum _PROCESSINFOCLASS {
  ProcessBasicInformation = 0,
  ProcessWow64Information = 26
} PROCESSINFOCLASS;

typedef WINBASEAPI DWORD WINAPI
GetProcessIdFn(
    HANDLE Process
);

typedef LONG WINAPI
NtQueryInformationProcess(
  IN HANDLE ProcessHandle,
  IN PROCESSINFOCLASS ProcessInformationClass,
  OUT PVOID ProcessInformation,
  IN ULONG ProcessInformationLength,
  OUT PULONG ReturnLength OPTIONAL
);

typedef struct _RTL_DRIVE_LETTER_CURDIR {
  USHORT Flags;
  USHORT Length;
  ULONG TimeStamp;
  UNICODE_STRING DosPath;
} RTL_DRIVE_LETTER_CURDIR, *PRTL_DRIVE_LETTER_CURDIR;

typedef struct _RTL_USER_PROCESS_PARAMETERS {
  ULONG MaximumLength;
  ULONG Length;
  ULONG Flags;
  ULONG DebugFlags;
  PVOID ConsoleHandle;
  ULONG ConsoleFlags;
  HANDLE StdInputHandle;
  HANDLE StdOutputHandle;
  HANDLE StdErrorHandle;
  UNICODE_STRING CurrentDirectoryPath;
  HANDLE CurrentDirectoryHandle;
  UNICODE_STRING DllPath;
  UNICODE_STRING ImagePathName;
  UNICODE_STRING CommandLine;
  PVOID Environment;
  ULONG StartingPositionLeft;
  ULONG StartingPositionTop;
  ULONG Width;
  ULONG Height;
  ULONG CharWidth;
  ULONG CharHeight;
  ULONG ConsoleTextAttributes;
  ULONG WindowFlags;
  ULONG ShowWindowFlags;
  UNICODE_STRING WindowTitle;
  UNICODE_STRING DesktopName;
  UNICODE_STRING ShellInfo;
  UNICODE_STRING RuntimeData;
  RTL_DRIVE_LETTER_CURDIR DLCurrentDirectory[0x20];
} RTL_USER_PROCESS_PARAMETERS, *PRTL_USER_PROCESS_PARAMETERS;

// Get the function pointer to GetProcessId in KERNEL32.DLL
static HRESULT EnsureGPIFunction(GetProcessIdFn** gpi_func_ptr) {
  static GetProcessIdFn* gpi_func = NULL;
  if (!gpi_func) {
    HMODULE kernel32_module = ::GetModuleHandle(_T("kernel32.dll"));
    if (!kernel32_module) {
      return HRESULTFromLastError();
    }
    gpi_func = reinterpret_cast<GetProcessIdFn*>(
                  ::GetProcAddress(kernel32_module, "GetProcessId"));
    if (!gpi_func) {
      return HRESULTFromLastError();
    }
  }

  *gpi_func_ptr = gpi_func;
  return S_OK;
}

// Get the function pointer to NtQueryInformationProcess in NTDLL.DLL
static HRESULT EnsureQIPFunction(NtQueryInformationProcess** qip_func_ptr) {
  static NtQueryInformationProcess* qip_func = NULL;
  if (!qip_func) {
    HMODULE ntdll_module = ::GetModuleHandle(_T("ntdll.dll"));
    if (!ntdll_module) {
      return HRESULTFromLastError();
    }
    qip_func = reinterpret_cast<NtQueryInformationProcess*>(
                  ::GetProcAddress(ntdll_module, "NtQueryInformationProcess"));
    if (!qip_func) {
      return HRESULTFromLastError();
    }
  }

  *qip_func_ptr = qip_func;
  return S_OK;
}

// Obtain the process ID from a hProcess HANDLE
ULONG Process::GetProcessIdFromHandle(HANDLE hProcess) {
  if (SystemInfo::IsRunningOnXPSP1OrLater()) {
    // Thunk to the documented ::GetProcessId() API
    GetProcessIdFn* gpi_func = NULL;
    HRESULT hr = EnsureGPIFunction(&gpi_func);
    if (FAILED(hr)) {
      ASSERT(FALSE,
             (_T("Process::GetProcessIdFromHandle - EnsureGPIFunction")
              _T(" failed[0x%x]"), hr));
      return 0;
    }
    ASSERT1(gpi_func);
    return gpi_func(hProcess);
  }

  // For lower versions of Windows, we use undocumented
  // function NtQueryInformationProcess to get at the PID
  NtQueryInformationProcess* qip_func = NULL;
  HRESULT hr = EnsureQIPFunction(&qip_func);
  if (FAILED(hr)) {
    ASSERT(FALSE,
           (_T("Process::GetProcessIdFromHandle - EnsureQIPFunction")
            _T(" failed[0x%x]"), hr));
    return 0;
  }
  ASSERT1(qip_func);

  PROCESS_BASIC_INFORMATION info;
  SetZero(info);
  if (!NT_SUCCESS(qip_func(hProcess,
                           ProcessBasicInformation,
                           &info,
                           sizeof(info),
                           NULL))) {
    ASSERT(FALSE, (_T("Process::GetProcessIdFromHandle - ")
                   _T("NtQueryInformationProcess failed!")));
    return 0;
  }

  return info.UniqueProcessId;
}

// Get the command line of a process
HRESULT Process::GetCommandLine(uint32 process_id, CString* cmd_line) {
  ASSERT1(process_id);
  ASSERT1(cmd_line);

  // Open the process
  scoped_process process_handle(::OpenProcess(
                                    PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                    false,
                                    process_id));
  if (!process_handle) {
    return HRESULTFromLastError();
  }

  // Obtain Process Environment Block
  // Note that NtQueryInformationProcess is not available in Windows 95/98/ME
  NtQueryInformationProcess* qip_func = NULL;
  HRESULT hr = EnsureQIPFunction(&qip_func);

  if (FAILED(hr)) {
    return hr;
  }
  ASSERT1(qip_func);

  PROCESS_BASIC_INFORMATION info;
  SetZero(info);
  if (!NT_SUCCESS(qip_func(get(process_handle),
                           ProcessBasicInformation,
                           &info,
                           sizeof(info),
                           NULL))) {
    return E_FAIL;
  }
  BYTE* peb = info.PebBaseAddress;

  // Read address of parameters (see some PEB reference)
  // TODO(omaha): use offsetof(PEB, ProcessParameters) to replace 0x10
  // http://msdn.microsoft.com/en-us/library/aa813706.aspx
  SIZE_T bytes_read = 0;
  uint32 dw = 0;
  if (!::ReadProcessMemory(get(process_handle),
                           peb + 0x10,
                           &dw,
                           sizeof(dw),
                           &bytes_read)) {
    return HRESULTFromLastError();
  }

  // Read all the parameters
  RTL_USER_PROCESS_PARAMETERS params;
  SetZero(params);
  if (!::ReadProcessMemory(get(process_handle),
                           reinterpret_cast<PVOID>(dw),
                           &params,
                           sizeof(params),
                           &bytes_read)) {
    return HRESULTFromLastError();
  }

  // Read the command line parameter
  const int max_cmd_line_len = std::min(
      static_cast<int>(params.CommandLine.MaximumLength),
      kMaxCmdLineLengthBytes);
  if (!::ReadProcessMemory(get(process_handle),
                           params.CommandLine.Buffer,
                           cmd_line->GetBufferSetLength(max_cmd_line_len),
                           max_cmd_line_len,
                           &bytes_read)) {
    return HRESULTFromLastError();
  }

  cmd_line->ReleaseBuffer();

  return S_OK;
}

// Check if the process is running with a specified path
bool Process::IsProcessRunningWithPath(uint32 process_id, const TCHAR* path) {
  ASSERT1(process_id);
  ASSERT1(path && *path);

  const int kProcessWaitModuleFullyUpMs = 100;
  const int kProcessWaitModuleRetries = 10;

  // Open the process
  scoped_process process(::OpenProcess(
                             PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                             false,
                             process_id));
  if (!process) {
    UTIL_LOG(LEVEL_ERROR,
             (_T("[Process::IsProcessRunningWithPath - OpenProcess failed]")
              _T("[%u][0x%x]"),
             process_id, HRESULTFromLastError()));
    return false;
  }

  for (int i = 0; i < kProcessWaitModuleRetries; ++i) {
    // Get the command line path of the main module
    // Note that we are using psapi functions which is not supported in Windows
    // 95/98/ME
    //
    // Sometimes it might be the case that the process is created but the main
    // module is not fully loaded. If so, wait a while and then try again
    TCHAR process_path[MAX_PATH];
    if (::GetModuleFileNameEx(get(process),
                              NULL,
                              process_path,
                              arraysize(process_path))) {
      // Do the check
      if (String_StartsWith(process_path, path, true)) {
        return true;
      }

      // Try again with short form
      TCHAR short_path[MAX_PATH];
      if (::GetShortPathName(path, short_path, arraysize(short_path)) &&
          String_StartsWith(process_path, short_path, true)) {
        return true;
      }

      return false;
    }

    UTIL_LOG(LEVEL_ERROR,
              (_T("[Process::IsProcessRunningWithPath - GetModuleFileNameEx ")
               _T("failed][%u][0x%x]"),
              process_id, HRESULTFromLastError()));

    ::Sleep(kProcessWaitModuleFullyUpMs);
  }

  UTIL_LOG(LEVEL_ERROR,
           (_T("[Process::IsProcessRunningWithPath - failed to get process ")
            _T("path][%u][0x%x]"),
            process_id, HRESULTFromLastError()));

  return false;
}

// Get the process owner
// Note that we may fail to get the owner which is not current user.
// TODO(omaha): merge with UserInfo::GetCurrentUser
HRESULT Process::GetProcessOwner(uint32 pid, CString* owner_sid) {
  ASSERT1(pid);
  ASSERT1(owner_sid);

  scoped_process process(::OpenProcess(PROCESS_QUERY_INFORMATION, false, pid));
  if (!valid(process)) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LEVEL_ERROR,
             (_T("[Process::GetProcessOwner - OpenProcess failed][%u][0x%x]"),
              pid, hr));
    return hr;
  }

  scoped_handle process_token;
  if (!::OpenProcessToken(get(process),
                          READ_CONTROL | TOKEN_QUERY,
                          address(process_token))) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(L4,
             (_T("[Process::GetProcessOwner - OpenProcessToken failed][0x%x]"),
              hr));
    return hr;
  }

  DWORD size_needed = 0;
  BOOL b = ::GetTokenInformation(get(process_token),
                                 TokenUser,
                                 NULL,
                                 0,
                                 &size_needed);
  ASSERT1(!b && ::GetLastError() == ERROR_INSUFFICIENT_BUFFER);

  scoped_array<byte> token_user(new byte[size_needed]);
  DWORD size_returned = 0;
  if (!::GetTokenInformation(get(process_token),
                             TokenUser,
                             token_user.get(),
                             size_needed,
                             &size_returned)) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LEVEL_ERROR,
             (_T("[Process::GetProcessOwner - GetTokenInformation fail][0x%x]"),
              hr));
    return hr;
  }

  PSID process_sid = (reinterpret_cast<TOKEN_USER*>(
                          token_user.get()))->User.Sid;
  ASSERT1(process_sid);
  if (!process_sid) {
    return E_FAIL;
  }

  TCHAR* process_sid_str = NULL;
  if (!::ConvertSidToStringSid(process_sid, &process_sid_str)) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LEVEL_ERROR,
             (_T("[IsOwnedByUser - ConvertSidToStringSid failed][0x%x]"),
              hr));
    return hr;
  }
  scoped_hlocal scoped_guard_sid_str(process_sid_str);

  *owner_sid = process_sid_str;

  return S_OK;
}

// Creates an impersonation token for the user running process_id.
// The caller is responsible for closing the returned handle.
HRESULT Process::GetImpersonationToken(DWORD process_id, HANDLE* user_token) {
  // Get a handle to the process.
  scoped_process process(::OpenProcess(
                             PROCESS_DUP_HANDLE | PROCESS_QUERY_INFORMATION,
                             TRUE,
                             process_id));
  if (!valid(process)) {
    HRESULT hr(HRESULTFromLastError());
    UTIL_LOG(LEVEL_ERROR,
             (_T("[GetImpersonationToken - ::OpenProcess failed][0x%x]"),
              hr));
    return hr;
  }

  HRESULT result = S_OK;
  scoped_handle process_token;
  if (!::OpenProcessToken(get(process), TOKEN_DUPLICATE | TOKEN_QUERY,
                          address(process_token))) {
    result = HRESULTFromLastError();
  } else {
    if (!::DuplicateTokenEx(get(process_token),
                            TOKEN_IMPERSONATE | TOKEN_QUERY |
                            TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE,
                            NULL,
                            SecurityImpersonation,
                            TokenPrimary,
                            user_token)) {
      result = HRESULTFromLastError();
    }
  }

  ASSERT(SUCCEEDED(result), (_T("[GetImpersonationToken Failed][hr=0x%x]"),
                             result));
  return result;
}

HRESULT Process::GetUsersOfProcesses(const TCHAR* task_name,
                                     int maximum_users,
                                     scoped_handle user_tokens[],
                                     int* number_of_users) {
  ASSERT1(task_name && *task_name);
  ASSERT1(maximum_users);
  ASSERT1(user_tokens);
  ASSERT1(number_of_users);

  scoped_hfile th32cs_snapshot(::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,
                                                          0));
  if (!valid(th32cs_snapshot)) {
    HRESULT hr(HRESULTFromLastError());
    UTIL_LOG(LEVEL_ERROR, (_T("[::CreateToolhelp32Snapshot fail][0x%x]"), hr));
    return hr;
  }

  HRESULT result = S_OK;
  *number_of_users = 0;
  // Walk the list of processes.
  PROCESSENTRY32 process = {0};
  process.dwSize = sizeof(PROCESSENTRY32);
  for (BOOL found = ::Process32First(get(th32cs_snapshot), &process); found;
       found = ::Process32Next(get(th32cs_snapshot), &process)) {
    // Check if it is one of the processes we are looking for.
    if (_tcsicmp(task_name, process.szExeFile) == 0) {
      // We match.  Get the user's token.
      scoped_handle user_token;
      if (FAILED(GetImpersonationToken(process.th32ProcessID,
                                       address(user_token))))
        continue;

      // Search through the existing list to see if it's a duplicate.
      // It's O(n^2) but we should have very few logged on users.
      int i = 0;
      for (; i < *number_of_users; i++) {
        if (get(user_tokens[i]) == get(user_token)) {
          // It's a duplicate.
          break;
        }
      }
      if (i >= *number_of_users) {
        // It's a new one.  Add it if there's room.
        ASSERT1(i < maximum_users);
        if (i < maximum_users) {
          // Release the user_token, we don't want it to be closed
          // by the user_token destructor
          reset(user_tokens[(*number_of_users)++], release(user_token));
        }
      }
     }
  }
  return result;
}

HRESULT Process::GetImagePath(const CString& process_name,
                              const CString& user_sid,
                              CString* path) {
  ASSERT1(path);

  // Search for running processes with process_name.
  uint32 mask = INCLUDE_ONLY_PROCESS_OWNED_BY_USER;
  std::vector<CString> command_line;
  std::vector<uint32> process_ids;
  HRESULT hr = FindProcesses(mask,
                             process_name,
                             true,
                             user_sid,
                             command_line,
                             &process_ids);
  if (FAILED(hr)) {
    UTIL_LOG(LEVEL_WARNING, (_T("[FindProcesses failed][0x%08x]"), hr));
    return hr;
  }

  if (process_ids.empty()) {
    return E_FAIL;
  }

  uint32 process_id = process_ids[0];
  UTIL_LOG(L4, (_T("[GetImagePath][pid=%d]"), process_id));
  scoped_process process_handle(::OpenProcess(PROCESS_QUERY_INFORMATION |
                                              PROCESS_VM_READ,
                                              FALSE,
                                              process_id));
  if (!process_handle) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(L4, (_T("[OpenProcess failed][0x%08x]"), hr));
    return hr;
  }

  HMODULE module_handle = NULL;
  DWORD bytes_needed = 0;
  if (!::EnumProcessModules(get(process_handle),
                            &module_handle,
                            sizeof(HMODULE),
                            &bytes_needed)) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LEVEL_WARNING, (_T("[EnumProcessModules failed][0x%08x]"), hr));
    // ::EnumProcessModules from a WoW64 process fails for x64 processes. We try
    // ::GetProcessImageFileName as a workaround here.
    TCHAR image_name[MAX_PATH] = {0};
    if (!GetProcessImageFileName(get(process_handle),
                                 image_name,
                                 arraysize(image_name))) {
      HRESULT hr = HRESULTFromLastError();
      UTIL_LOG(LE, (_T("[GetProcessImageFileName failed][0x%08x]"), hr));
      return hr;
    } else {
      *path = image_name;
      return S_OK;
    }
  }

  TCHAR module_name[MAX_PATH] = {0};
  if (!::GetModuleFileNameEx(get(process_handle),
                             module_handle,
                             module_name,
                             arraysize(module_name))) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LEVEL_ERROR, (_T("[GetModuleFileNameEx failed][0x%08x]"), hr));
    return hr;
  }

  *path = module_name;
  return S_OK;
}

bool Process::IsWow64(uint32 pid) {
  typedef BOOL (WINAPI *IsWow64Process)(HANDLE, BOOL*);
  scoped_process handle(::OpenProcess(PROCESS_QUERY_INFORMATION | SYNCHRONIZE,
                                      false,
                                      pid));
  if (!handle) {
    return false;
  }

  HINSTANCE kernel_instance = ::GetModuleHandle(_T("kernel32.dll"));
  if (kernel_instance == NULL) {
    ASSERT1(false);
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LW, (_T("[::GetModuleHandle  kernel32.dll failed][0x%08x]"), hr));
    return false;
  }

  IsWow64Process pfn = reinterpret_cast<IsWow64Process>(::GetProcAddress(
      kernel_instance,
      "IsWow64Process"));
  if (!pfn) {
    UTIL_LOG(LW, (_T("[::IsWow64Process() not found in kernel32.dll]")));
    return false;
  }

  BOOL wow64 = FALSE;
  if (!(*pfn)(get(handle), &wow64)) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LW, (_T("[::IsWow64Process() failed][0x%08x]"), hr));
    return false;
  }

  return (wow64 != 0);
}

HRESULT Process::MakeProcessWindowForeground(const CString& executable) {
  UTIL_LOG(L3, (_T("[MakeProcessWindowForeground]")));

  CString sid;
  HRESULT hr = omaha::user_info::GetCurrentUser(NULL, NULL, &sid);
  if (FAILED(hr)) {
    return hr;
  }

  // This code does not handle two cases:
  // 1. If a new process instance is starting up but there are other process
  //    instances running, then we will not wait for the new process instance.
  //    One way to fix this is to pass the number of expected processes to this
  //    method.
  // 2. If we find multiple processes, and we are able to find the windows only
  //    for some of the processes (maybe because the rest are still starting up)
  //    then we will only set the windows of the one that we found to the
  //    foreground and ignore the rest.
  bool found = false;
  for (int retries = 0; retries < kNumRetriesToFindProcess && !found;
       ++retries) {
    std::vector<CString> command_lines;
    std::vector<uint32> processes;
    DWORD flags = EXCLUDE_CURRENT_PROCESS | INCLUDE_ONLY_PROCESS_OWNED_BY_USER;
    hr = Process::FindProcesses(flags,
                                executable,
                                true,
                                sid,
                                command_lines,
                                &processes);
    if (FAILED(hr)) {
      UTIL_LOG(LW, (_T("[FindProcesses failed][0x%08x]"), hr));
      return hr;
    }

    UTIL_LOG(L3, (_T("[Found %d processes]"), processes.size()));
    for (size_t i = 0; i < processes.size(); ++i) {
      CSimpleArray<HWND> windows;
      if (!WindowUtils::FindProcessWindows(processes[i], 0, &windows)) {
        UTIL_LOG(L3, (_T("[FindProcessWindows failed][0x%08x]"), hr));
        continue;
      }

      for (int j = 0; j < windows.GetSize(); ++j) {
        if (WindowUtils::IsMainWindow(windows[j])) {
          UTIL_LOG(L4, (_T("[Found main window of process %d]"), processes[i]));
          WindowUtils::MakeWindowForeground(windows[j]);
          ::FlashWindow(windows[j], true);
          found = true;
          break;
        }
      }
    }

    if (!found) {
      ::Sleep(kFindProcessRetryIntervalMs);
    }
  }

  return S_OK;
}

}  // namespace omaha

