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
// Synchronization functions

#include "omaha/common/single_instance.h"
#include "omaha/common/constants.h"
#include "omaha/common/debug.h"
#include "omaha/common/logging.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/synchronized.h"
#include "omaha/common/utils.h"

namespace omaha {

// Check to see whether an instance is already running across all sessions.
// If not, enter single instance protection. The user must call Shutdown()
// on that SingleInstance once single instance protection is no longer needed.
bool SingleInstance::StartupSingleInstance(const TCHAR* id) {
  ASSERT1(id);

  bool already_running = false, already_running_in_different_session = false;
  HRESULT hr = Startup(id,
                       &already_running,
                       &already_running_in_different_session);
  ASSERT(SUCCEEDED(hr), (_T("")));

  return already_running || already_running_in_different_session;
}

// Check to see whether an instance is already running in this session. If not,
// enter session-only single instance protection. The user must call Shutdown()
// on that SingleInstance once single instance protection is no longer needed.
bool SingleInstance::StartupSingleSessionInstance(const TCHAR* id) {
  ASSERT1(id);

  bool already_running = false;
  HRESULT hr = Startup(id, &already_running, NULL);
  ASSERT(SUCCEEDED(hr), (_T("")));

  return already_running;
}

// Startup a single instance protection. The user must call Shutdown() on
// that SingleInstance once the single instance protection is no longer needed.
//
// Returns whether or not the process is already running
// already_running means "already running in same session".
// already_running_in_different_session means "already running on machine"
HRESULT SingleInstance::Startup(const TCHAR* id,
                                bool* already_running,
                                bool* already_running_in_different_session) {
  ASSERT1(id);
  ASSERT1(already_running);

  CString mutex_id;

  // Use two mutexes: one to check for being the only instance in this
  // session, and one for being the only instance in any terminal session.
  // Only create (and check) the global mutex for one-per-machine check if
  // the result is asked for.
  // We don't actually obtain ownership of the mutex
  // For information on the "Local" and "Global" namespace prefixes, see MSDN
  // article "Kernel Object Namespaces".

  // Create a user level mutex
  CreateSyncId(id, SYNC_USER, &mutex_id);
  RET_IF_FAILED(CreateInstanceMutex(mutex_id,
                                    &user_mutex_handle_,
                                    already_running));

  // Create a global mutex
  if (already_running_in_different_session) {
    CreateSyncId(id, SYNC_GLOBAL, &mutex_id);
    RET_IF_FAILED(CreateInstanceMutex(mutex_id,
                                      &global_mutex_handle_,
                                      already_running_in_different_session));
  }

  return S_OK;
}

// Create a mutex
HRESULT SingleInstance::CreateInstanceMutex(const TCHAR* mutex_id,
                                            HANDLE* mutex_handle,
                                            bool* already_running) {
  ASSERT1(mutex_id && *mutex_id);
  ASSERT1(mutex_handle);
  ASSERT1(already_running);

  *already_running = false;

  *mutex_handle = ::CreateMutex(NULL, false, mutex_id);
  DWORD last_error = ::GetLastError();

  // We check for both values because we sometimes see access
  // denied.  We expect this to mean that the mutex was created by a
  // different set of user credentials, which shouldn't happen under
  // normal circumstances in our applications, but in fact we did
  // see it happen.
  if (last_error == ERROR_ALREADY_EXISTS || last_error == ERROR_ACCESS_DENIED) {
    *already_running = true;
    return S_OK;
  }

  if (*mutex_handle == NULL) {
    HRESULT hr = HRESULT_FROM_WIN32(last_error);
    ASSERT(false, (_T("[SingleInstance::CreateInstanceMutex]")
                   _T("[failed to create mutex][%s][0x%x]"), mutex_id, hr));
    return hr;
  }

  return S_OK;
}

// Shutdown a single instance protection
HRESULT SingleInstance::Shutdown() {
  if (user_mutex_handle_) {
    VERIFY(::CloseHandle(user_mutex_handle_), (_T("")));
    user_mutex_handle_ = NULL;
  }

  if (global_mutex_handle_) {
    VERIFY(::CloseHandle(global_mutex_handle_), (_T("")));
    global_mutex_handle_ = NULL;
  }

  return S_OK;
}

// Check to see whether an instance is already running
HRESULT SingleInstance::CheckAlreadyRunning(
    const TCHAR* id,
    bool* already_running,
    bool* already_running_in_different_session) {
  ASSERT1(id);
  ASSERT1(already_running);

  CString mutex_id;

  // Open a user level mutex
  CreateSyncId(id, SYNC_USER, &mutex_id);
  RET_IF_FAILED(OpenInstanceMutex(mutex_id, already_running));

  // Open a global mutex
  if (already_running_in_different_session) {
    CreateSyncId(id, SYNC_GLOBAL, &mutex_id);
    RET_IF_FAILED(OpenInstanceMutex(mutex_id,
                                    already_running_in_different_session));
  }

  return S_OK;
}

// Open a mutex
HRESULT SingleInstance::OpenInstanceMutex(const TCHAR* mutex_id,
                                          bool* already_running) {
  ASSERT1(mutex_id && *mutex_id);
  ASSERT1(already_running);

  *already_running = false;

  scoped_handle mutex_handle(::OpenMutex(NULL, false, mutex_id));
  DWORD last_error = ::GetLastError();

  if (get(mutex_handle) || last_error == ERROR_ACCESS_DENIED) {
    UTIL_LOG(L3, (_T("[SingleInstance::OpenInstanceMutex]")
                  _T("[already running][0x%x]"), last_error));
    *already_running = true;
    return S_OK;
  }

  if (last_error != ERROR_FILE_NOT_FOUND) {
    HRESULT hr = HRESULT_FROM_WIN32(last_error);
    ASSERT(false, (_T("[SingleInstance::OpenInstanceMutex]")
                   _T("[failed to open mutex][%s][0x%x]"), mutex_id, hr));
    return hr;
  }

  return S_OK;
}

}  // namespace omaha

