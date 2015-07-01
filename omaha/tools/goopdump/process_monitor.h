// Copyright 2009 Google Inc.
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
// Provides a class (ProcessMonitor) to watch for Win32 process names and fire
// callback events (via the ProcessMonitorCallbackInterface) when processes are
// created/exited.

#ifndef OMAHA_TOOLS_SRC_GOOPDUMP_PROCESS_MONITOR_H__
#define OMAHA_TOOLS_SRC_GOOPDUMP_PROCESS_MONITOR_H__

#include <windows.h>
#include <atlstr.h>
#include <map>
#include <vector>

#include "omaha/common/scoped_any.h"
#include "omaha/common/synchronized.h"

namespace omaha {

// Interface for users of the ProcessMonitor class to receive events when a
// process matching their desired pattern is created or exits.
class ProcessMonitorCallbackInterface {
 public:
  ProcessMonitorCallbackInterface() {}
  virtual ~ProcessMonitorCallbackInterface() {}

  // Called when a new process is found that matches the pattern.  The pattern
  // that matched is passed in as process_pattern.
  virtual void OnProcessAdded(DWORD process_id,
                              const CString& process_pattern) = 0;

  // Called when a process that was previously found has exited.
  virtual void OnProcessRemoved(DWORD process_id) = 0;

 private:
  DISALLOW_EVIL_CONSTRUCTORS(ProcessMonitorCallbackInterface);
};

// This class creates a thread to monitor running processes for particular
// process names.  Fires events via ProcessMonitorCallbackInterface when a new
// process is detected that matches a name pattern and also when those processes
// exit.
// This class uses polling to look for the processes since the only way to get
// event notification of process creation is to create a driver.
class ProcessMonitor {
 public:
  typedef std::map<HANDLE, DWORD> MapHandleToDword;
  typedef MapHandleToDword::iterator MapHandleToDwordIterator;

  ProcessMonitor();
  ~ProcessMonitor();

  // Starts the monitoring process to look for processes that match
  // process_name_pattern and fire events via the callback.
  HRESULT Start(ProcessMonitorCallbackInterface* callback,
                const TCHAR* process_name_pattern);

  // Starts the monitoring process with multiple patterns.
  HRESULT StartWithPatterns(ProcessMonitorCallbackInterface* callback,
                            const std::vector<CString>& process_name_patterns);

  // Stops the monitoring process and cleans up.
  HRESULT Stop();

 private:
  // Thread procedure for monitoring the processes in the background.
  static DWORD WINAPI MonitorThreadProc(void* param);
  DWORD MonitorProc();

  // Called when a process matching the process is found.
  void OnProcessAdded(DWORD process_id, const CString& process_pattern);

  // Called when a previously added process exits.
  void OnProcessRemoved(DWORD process_id);

  // Walks the active process list to look for matches against the pattern.  If
  // a process is found to match that's not in the list, it's added to the map
  // and OnProcessAdded() is called.
  bool UpdateProcessList(MapHandleToDword* map_handle_pid);

  // Walks through the map and calls CloseHandle() on each handle in the list.
  void CleanupHandleMap(MapHandleToDword* map_handle_pid);

  bool is_running_;   // Set to true while the process monitoring is running.
  ProcessMonitorCallbackInterface* callback_;   // Event callback interface.
  std::vector<CString> process_name_patterns_;  // List of patterns to match.
  CriticalSection lock_;              // Protects is_running_ flag.
  scoped_handle event_thread_exit_;   // Signal to exit monitor_thread_.
  scoped_handle monitor_thread_;      // Handle to the monitoring thread.

  DISALLOW_EVIL_CONSTRUCTORS(ProcessMonitor);
};

}  // namespace omaha

#endif  // OMAHA_TOOLS_SRC_GOOPDUMP_PROCESS_MONITOR_H__

