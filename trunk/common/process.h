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
// Declares class Process to incapsulate win32
// functions for creation and some manipulations of
// processes.

#ifndef OMAHA_COMMON_PROCESS_H__
#define OMAHA_COMMON_PROCESS_H__

#include <windows.h>
#include <psapi.h>
#include <atlstr.h>
#include <vector>
#include "base/basictypes.h"
#include "omaha/common/commontypes.h"
#include "omaha/common/constants.h"
#include "omaha/common/scoped_any.h"

namespace omaha {

const int kMaxProcesses = 1024;
const int kMaxProcessModules = 1024;

// Exclude mask for finding processes.
enum FindProcessesExcludeMask {
  EXCLUDE_NONE = 0,
  EXCLUDE_CURRENT_PROCESS = 0x1,
  EXCLUDE_PROCESS_OWNED_BY_CURRENT_USER = 0x2,
  EXCLUDE_PROCESS_OWNED_BY_SYSTEM = 0x4,
  INCLUDE_ONLY_PROCESS_OWNED_BY_USER = 0x08,
  EXCLUDE_PROCESS_COMMAND_LINE_CONTAINING_STRING = 0x10,
  INCLUDE_PROCESS_COMMAND_LINE_CONTAINING_STRING = 0x20
};

// Process info used in finding descendent processes.
struct ProcessInfo {
  uint32 process_id;
  uint32 parent_id;
#if !SHIPPING
  CString exe_file;
#endif
};

// Process class
class Process {
 public:
  // Constructor
  // Init the process object with the executable name
  // and if known the window class name of the process.
  // If window_class_name is known it will be easy
  // to stop the process just by sending messages to
  // a window.
  Process(const TCHAR* name, const TCHAR* window_class_name);

  // Constructor.
  // Init the process object with the process id.
  explicit Process(uint32 process_id);

  // Destructor
  virtual ~Process();

  // Start the process with some command line params if any.
  virtual bool Start(const TCHAR* command_line_parameters);

  // Start with command params and specific hwnd.
  virtual bool StartWithHwnd(const TCHAR* command_line_parameters, HWND hwnd);

  // Restart the process with the old command line params.
  bool Restart(HWND hwnd);

  // Set shutdown event using in signaling the process watch.
  void SetShutdownEvent(HANDLE shutdown_event);

  // Sets the specified priority class to the process.
  bool SetPriority(uint32 priority_class) const;

  // Check if the process is running.
  bool Running() const;

  // Create a job and assign the process to it.
  HANDLE AssignToJob();

  // Wait some time till the process finishes.
  bool WaitUntilDead(uint32 timeout_msec);

  // Wait some time till the process and all its descendent processes finish.
  bool WaitUntilAllDead(HANDLE job,
                        uint32 timeout_msec,
                        const TCHAR* path_to_exclude,
                        uint32* exit_code);

  // Wait until process is dead or a windows message arrives. For use in a
  // message loop while waiting.
  HRESULT WaitUntilDeadOrInterrupt(uint32 msec);
  // Return values include CI_S_PROCESSWAIT_DEAD, CI_S_PROCESSWAIT_TIMEOUT,
  // CI_S_PROCESSWAIT_MESSAGE.

#if !SHIPPING
  CString GetDebugInfo() const;
#endif

  // Return the process ID.
  uint32 GetId() const;

  // Return a readable representation of the process's name.
  const TCHAR *GetName() const;

  // Get win32 handle to process.
  HANDLE GetHandle() const;

  // Get process exit code.
  bool GetExitCode(uint32* exit_code) const;

  // can we kill the process via terminating
  // some processes are not safe to terminate.
  virtual bool IsTerminationAllowed() const;

  // Second, more rude method to stop the process. window_class_name was
  // not given or CloseWithMessage didn't succeed.
  bool Terminate(uint32 wait_for_terminate_msec);

  // Try to get a descendant process. Return process id if found.
  uint32 GetDescendantProcess(HANDLE job,
                              bool child_only,
                              bool sole_descedent,
                              const TCHAR* search_name,
                              const TCHAR* path_to_exclude);

  // Dynamically links and calls ::IsProcessInJob() in kernel32.dll.
  static BOOL IsProcessInJob(HANDLE process_handle,
                             HANDLE job_handle,
                             PBOOL result);

  // Try to get all matching descendant processes.
  HRESULT GetAllDescendantProcesses(
      HANDLE job,
      bool child_only,
      const TCHAR* search_name,
      const TCHAR* path_to_exclude,
      std::vector<ProcessInfo>* descendant_proc_ids);

  // Finds the processes based on passed criteria.
  static HRESULT FindProcesses(uint32 exclude_mask,
                               const TCHAR* search_name,
                               bool search_main_executable_only,
                               std::vector<uint32>* process_ids_found);

  // Find processes which loads the specified exe/dll. Uses the user_sid only
  // if the INCLUDE_ONLY_PROCESS_OWNED_BY_USER flag has been set.
  // The command_line is only used when
  // EXCLUDE_PROCESS_COMMAND_LINE_CONTAINING_STRING is set.
  static HRESULT FindProcesses(uint32 exclude_mask,
                               const TCHAR* search_name,
                               bool search_main_executable_only,
                               const CString& user_sid,
                               const std::vector<CString>& command_line,
                               std::vector<uint32>* process_ids_found);

  // Find processes with the specified criteria running in specific session.
  static HRESULT FindProcessesInSession(DWORD session_id,
                                        uint32 exclude_mask,
                                        const TCHAR* search_name,
                                        bool search_main_executable_only,
                                        const CString& user_sid,
                                        const std::vector<CString>& cmd_lines,
                                        std::vector<uint32>* process_ids_found);

  // Is the process using the specified exe/dll.
  static bool IsProcessUsingExeOrDll(uint32 process_id,
                                     const TCHAR* search_name,
                                     bool search_main_executable_only);

  // Obtain the process ID from a hProcess HANDLE.
  static ULONG GetProcessIdFromHandle(HANDLE hProcess);

  // Get the command line of a process.
  static HRESULT GetCommandLine(uint32 process_id, CString* cmd_line);

  // Get the process owner.
  static HRESULT GetProcessOwner(uint32 pid, CString* owner_sid);

  // Creates an impersonation token for the user running process_id.
  // The caller is responsible for closing the returned handle.
  static HRESULT GetImpersonationToken(DWORD process_id, HANDLE* user_token);

  // Returns user token handles for the users currently running the named task.
  // maximum_users specifies the maximun number of handles to be retured.
  // The actual number filled is returned.
  static HRESULT GetUsersOfProcesses(const TCHAR* task_name,
                                     int maximum_users,
                                     scoped_handle users[],
                                     int* number_of_users);

  // Gets the on disk path from where the process image is loaded.
  static HRESULT GetImagePath(const CString& process_name,
                              const CString& user_sid,
                              CString* path);

  // Returns if the process is running under WOW64.
  static bool IsWow64(uint32 pid);

 public:
  // How many times the process can be restarted in case it crashes.
  virtual uint32 GetMaxNumberOfRestarts() const;

  // Maximum amount of memory process is allowed to use before it's killed.
  // Default of 0 means unlimited.
  virtual uint32 GetMaxMemory() const;

  // Have we exceeded the number of maximum restarting.
  bool AllowedToRestart() const;

  // In case of crash, how soon to restart.
  virtual uint32 GetRestartInterval() const;

  // The idea is the following. Each process has maximum number of restarts.
  // As soon as the process reaches that number of restarts in should no longer
  // be restarted unless the time window in which the process was crashing is
  // more than the value returned by this function. For example:
  // Process X returns 3 from the function GetMaxNumberOfRestarts.
  // The same process returns 30*1000*60 (30 minutes) from
  // GetTimeWindowForCrashes if  process X crashed more than 3 times in 30
  // minutes it will not be restarted. if it took more than 30 minutes for
  // process X to crash more than 3 times - internal counters for number of
  // crashes will be reset and the process will be happily restarted.
  // Each derived process can override this function to return its own time
  // window for crashes.
  // Default implementation returns INFINITE which means that this is not time
  // based at all, if the process crashed more than the value returned by
  // GetMaxNumberOfRestarts it will not be restarted no matter how long it took.
  virtual uint32 GetTimeWindowForCrashes() const;

  // Sets the main window of all process instances to the foreground.
  static HRESULT MakeProcessWindowForeground(const CString& executable);

 private:
  mutable uint32 number_of_restarts_;
  CString command_line_;
  CString command_line_parameters_;
  CString window_class_name_;
  CString name_;
  scoped_process process_;
  uint32 process_id_;
  mutable uint32 time_of_start_;
  mutable uint32 exit_code_;
  HANDLE shutdown_event_;

  // Helper function to wait till the process and all its descendent processes
  // finish.
  bool InternalWaitUntilAllDead(HANDLE job,
                                uint32 timeout_msec,
                                const TCHAR* path_to_exclude,
                                uint32* exit_code);

  // Check if the process is running with a specified path.
  static bool IsProcessRunningWithPath(uint32 process_id, const TCHAR* path);

  // Checks if the command line of the process has been specified as one to
  // ignore.
  static bool IsStringPresentInList(const CString& process_command_line,
                                    const std::vector<CString>& list);


  // Helper function to get long path name.
  static HRESULT GetLongPathName(const TCHAR* short_name, CString* long_name);

  // Helper for Process::IsProcessUsingExeOrDll(). Use GetProcessImageFileName
  // to get the filename, and match against search_name.
  static bool IsProcImageMatch(HANDLE proc_handle,
                               const TCHAR* search_name,
                               bool is_fully_qualified_name);

  // Dynamically links and calls ::GetProcessImageFileName() in psapi.dll.
  static DWORD GetProcessImageFileName(HANDLE proc_handle,
                                       LPTSTR image_file,
                                       DWORD file_size);

  // Helper for Process::IsProcessUsingExeOrDll().
  // Is there a match between the module and the specified exe/dll?
  static bool IsModuleMatchingExeOrDll(const TCHAR* module_name,
                                       const TCHAR* search_name,
                                       bool is_fully_qualified_name);

#if !SHIPPING
  CString debug_info_;
#endif
  DISALLOW_EVIL_CONSTRUCTORS(Process);
};

}  // namespace omaha

#endif  // OMAHA_COMMON_PROCESS_H__

