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

#ifndef OMAHA_COMMON_PROC_UTILS_H_
#define OMAHA_COMMON_PROC_UTILS_H_

#include <windows.h>
#include <tlhelp32.h>
#include <atlstr.h>
#include <vector>
#include "base/basictypes.h"

namespace omaha {

// Moved here from installation directory. Generic enought to be in common.
class ProcessTerminator {
 public:

  // constants for specifying which methods to attempt when killing a process
  static const int KILL_METHOD_1_WINDOW_MESSAGE = 0x01;
  static const int KILL_METHOD_2_THREAD_MESSAGE = 0x02;
  static const int KILL_METHOD_4_TERMINATE_PROCESS = 0x08;
  static const int INVALID_SESSION_ID = 0xFFFF;

  // Creates the object given the process name to kill.
  explicit ProcessTerminator(const CString& process_name);
  ProcessTerminator(const CString& process_name, const CString& user_sid);
  ProcessTerminator(const CString& process_name,
                    const CString& user_sid,
                    int session_id);

  // Performs necessary cleanup.
  ~ProcessTerminator();

  // Go through process list try to find the required one to kill,
  // trying three methods to kill, from easiest and cleanest to a
  // harsh one.  S_OK if no process by the right name was found, or if it was
  // found and was killed.  E_FAIL otherwise.  was_found returns true if
  // process was found. Kills all instances of a process.
  HRESULT KillTheProcess(uint32 timeout_msec,
                         bool* was_found,
                         uint32 method_mask,
                         bool flash_window);

  // Wait for all instances of the process to die.
  HRESULT WaitForAllToDie(uint32 timeout_msec);

  // Finds all process ids for the process of a given name.
  bool FindProcessInstances();

 private:

  // Will try to open handle to each instance.
  // Leaves process handles open (in member process_handles_)
  // Will use access rights for opening appropriate for the purpose_of_opening
  bool PrepareToKill(uint32 method_mask);

  // Wait for process instances to die for timeout_msec
  // return true if all are dead and false if timed out.
  bool WaitForProcessInstancesToDie(uint32 timeout_msec) const;

  // Will close all currently opened handles.
  void CloseAllHandles();


  //
  // Killing via messages to window
  //
  // Function which meet win32 requirements for callback
  // function passed into EnumWindows function.
  BOOL static CALLBACK EnumAllWindowsProc(HWND hwnd, LPARAM lparam);

  // Will return true if it succeeds in finding a window for the process
  // to be killed, otherwise false.  If there are such top-level windows
  // then returns an array of window handles.
  bool FindProcessWindows();

  // Will try to kill the process via posting windows messages
  // returns true on success otherwise false.
  // Timeout is maximum time to wait for WM_CLOSE to work before going to
  // next method.
  bool KillProcessViaWndMessages(uint32 timeout_msec);

  //
  // Killing via messages to thread
  //
  // Try to find the threads than run in
  // the process in question.
  bool FindProcessThreads(std::vector<uint32>* thread_ids);

  // Will try to kill the process via posing thread messages
  // returns true on success otherwise false.
  // Timeout is maximum time to wait for message to work before going to
  // next method.
  bool KillProcessViaThreadMessages(uint32 timeout_msec);

  // The last and crude method to kill the process.
  // Calls TerminateProcess function.
  bool KillProcessViaTerminate(uint32 timeout_msec);

  // Private member variables:
  CString process_name_;
  // One process can have several instances
  // running. This array will keep handles to all
  // instances of the process.
  std::vector<HANDLE> process_handles_;
  // Array of process ids which correspond to different
  // instances of the same process.
  std::vector<uint32>  process_ids_;
  // Function PrepareToKill can call itself
  // recursively under some conditions.
  // We need to stop the recursion at some point.
  // This is the purpose of this member.
  int recursion_level_;
  // Array of window handles.
  std::vector<HWND> window_handles_;
  // The sid of the user whose process needs to be terminated.
  CString user_sid_;
  // True if the window flashes on shut down.
  bool flash_window_;
  // The session to search the processes in.
  int session_id_;

  // Disable copy constructor and assignment operator.
  DISALLOW_EVIL_CONSTRUCTORS(ProcessTerminator);
};

// Application calling this function will be shut down
// by the system without displaying message boxes if the application
// fails to shutdown itself properly as a result of processing
// WM_QUERYENDSESSION.
HRESULT SetProcessSilentShutdown();

}  // namespace omaha

#endif  // OMAHA_COMMON_PROC_UTILS_H_
