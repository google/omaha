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

#include "omaha/core/google_update_core.h"

#include <memory>

#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/reg_key.h"
#include "omaha/base/scope_guard.h"
#include "omaha/base/system.h"
#include "omaha/base/utils.h"
#include "omaha/common/config_manager.h"
#include "omaha/goopdate/app_command.h"
#include "omaha/goopdate/app_command_configuration.h"
#include "omaha/third_party/smartany/scoped_any.h"

namespace omaha {

GoogleUpdateCoreBase::GoogleUpdateCoreBase() : StdMarshalInfo(true) {
  CORE_LOG(L3, (_T("[GoogleUpdateCoreBase::GoogleUpdateCoreBase]")));
}

GoogleUpdateCoreBase::~GoogleUpdateCoreBase() {
  CORE_LOG(L3, (_T("[GoogleUpdateCoreBase::~GoogleUpdateCoreBase]")));
}

STDMETHODIMP GoogleUpdateCoreBase::LaunchCmdElevated(const WCHAR* app_guid,
                                                     const WCHAR* cmd_id,
                                                     DWORD caller_proc_id,
                                                     ULONG_PTR* proc_handle) {
  CORE_LOG(L3, (_T("[GoogleUpdateCoreBase::LaunchCmdElevated]")
                _T("[app %s][cmd %s][pid %d]"),
                app_guid, cmd_id, caller_proc_id));
  ASSERT1(app_guid);
  ASSERT1(cmd_id);
  ASSERT1(proc_handle);

  if (!(IsGuid(app_guid) && cmd_id && _tcslen(cmd_id) && proc_handle)) {
    return E_INVALIDARG;
  }

  scoped_process caller_proc_handle;
  HRESULT hr = OpenCallerProcessHandle(caller_proc_id,
                                       address(caller_proc_handle));
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[failed to open caller's handle][0x%x]"), hr));
    return hr;
  }

  // Allocate a session ID for the ping that this call will generate.  (I'd
  // really like to be able to pipe an external session ID through this API,
  // but this is old and I don't feel comfortable changing the signature on it.)
  CString session_id;
  GetGuid(&session_id);

  std::unique_ptr<AppCommandConfiguration> configuration;
  // true == machine level
  hr = AppCommandConfiguration::Load(app_guid,
                                     true,
                                     cmd_id,
                                     &configuration);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[failed to load command configuration][0x%x]"), hr));
    return hr;
  }

  // This is a pseudo handle that must not be closed.
  HANDLE this_process_handle = ::GetCurrentProcess();

  scoped_process command_process;
  scoped_process duplicate_proc_handle;

  std::unique_ptr<AppCommand> app_command(
    configuration->Instantiate(session_id));

  hr = app_command->Execute(
      NULL, std::vector<CString>(), address(command_process));
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[failed to launch app command][0x%x]"), hr));
    return hr;
  }

  DWORD desired_access = PROCESS_QUERY_INFORMATION | SYNCHRONIZE;
  bool res = ::DuplicateHandle(
      this_process_handle,             // Current process.
      get(command_process),            // Process handle to duplicate.
      get(caller_proc_handle),         // Process receiving the handle.
      address(duplicate_proc_handle),  // Duplicated handle.
      desired_access,                  // Access requested for the new handle.
      false,                           // Don't inherit the new handle.
      0) != 0;                         // Flags.

  if (!res) {
    hr = HRESULTFromLastError();
    CORE_LOG(LE, (_T("[failed to duplicate the handle][0x%08x]"), hr));
    return hr;
  }

  // Transfer the ownership of the new handle to the caller. The caller must
  // close this handle.
  *proc_handle = reinterpret_cast<ULONG_PTR>(release(duplicate_proc_handle));

  return S_OK;
}

HRESULT GoogleUpdateCoreBase::OpenCallerProcessHandle(DWORD proc_id,
                                                      HANDLE* proc_handle) {
  ASSERT1(proc_handle);
  *proc_handle = NULL;

  HRESULT hr = ::CoImpersonateClient();
  if (FAILED(hr)) {
    return hr;
  }
  ON_SCOPE_EXIT(::CoRevertToSelf);

  *proc_handle = ::OpenProcess(PROCESS_DUP_HANDLE, false, proc_id);
  return *proc_handle ? S_OK : HRESULTFromLastError();
}

}  // namespace omaha

