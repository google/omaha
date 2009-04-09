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
// Single Instance of a process running (plus some synchronization functions
// that should be moved elsewhere)
//
// synchronization functions

#ifndef OMAHA_COMMON_SINGLE_INSTANCE_H_
#define OMAHA_COMMON_SINGLE_INSTANCE_H_

#include <windows.h>

#include "base/basictypes.h"
#include "omaha/common/debug.h"

namespace omaha {

// Used to ensure only a single instance of a process per machine
// (e.g., even in terminal server sessions)
class SingleInstance {
 public:
  // Constructor
  SingleInstance() : user_mutex_handle_(NULL), global_mutex_handle_(NULL) {}

  // Destructor
  ~SingleInstance() { Shutdown(); }

  // Check to see whether an instance is already running across all sessions.
  // If not, enter single instance protection. The user must call Shutdown()
  // on that SingleInstance once single instance protection is no longer needed.
  bool StartupSingleInstance(const TCHAR* id);

  // Check to see whether an instance is already running for this user. If not,
  // enter user-only single instance protection. The user must call Shutdown()
  // on that SingleInstance once single instance protection is no longer needed.
  bool StartupSingleSessionInstance(const TCHAR* id);

  // Startup a single instance protection. The user must call Shutdown() on
  // that SingleInstance once the single instance protection is no longer needed.
  HRESULT Startup(const TCHAR* id,
                  bool* already_running,
                  bool* already_running_in_different_session);

  // Shutdown a single instance protection
  HRESULT Shutdown();

  // Check to see whether an instance is already running
  static HRESULT CheckAlreadyRunning(
      const TCHAR* id,
      bool* already_running,
      bool* already_running_in_different_session);

 private:
  // Create a mutex
  static HRESULT CreateInstanceMutex(const TCHAR* mutex_id,
                                     HANDLE* mutex_handle,
                                     bool* already_running);

  // Open a mutex
  static HRESULT OpenInstanceMutex(const TCHAR* mutex_id,
                                   bool* already_running);

  HANDLE user_mutex_handle_;
  HANDLE global_mutex_handle_;

  DISALLOW_EVIL_CONSTRUCTORS(SingleInstance);
};

}  // namespace omaha

#endif  // OMAHA_COMMON_SINGLE_INSTANCE_H_
