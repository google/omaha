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
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/exception_barrier.h"
#include "omaha/common/logging.h"
#include "omaha/common/reg_key.h"
#include "omaha/common/scope_guard.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/system.h"
#include "omaha/common/utils.h"
#include "omaha/goopdate/config_manager.h"

namespace omaha {

GoogleUpdateCore::GoogleUpdateCore() {
  CORE_LOG(L3, (_T("[GoogleUpdateCore::GoogleUpdateCore]")));
}

GoogleUpdateCore::~GoogleUpdateCore() {
  CORE_LOG(L3, (_T("[GoogleUpdateCore::~GoogleUpdateCore]")));
}

STDMETHODIMP GoogleUpdateCore::LaunchCmdElevated(const WCHAR* app_guid,
                                                 const WCHAR* cmd_id,
                                                 DWORD caller_proc_id,
                                                 ULONG_PTR* proc_handle) {
  CORE_LOG(L3, (_T("[GoogleUpdateCore::LaunchCmdElevated]")
                _T("[app %s][cmd %s][pid %d]"),
                app_guid, cmd_id, caller_proc_id));

  ExceptionBarrier barrier;

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

  CString cmd(GetCommandToLaunch(app_guid, cmd_id));
  CORE_LOG(L3, (_T("[GoogleUpdateCore::LaunchCmdElevated][cmd %s]"), cmd));
  if (cmd.IsEmpty()) {
    return GOOPDATE_E_CORE_MISSING_CMD;
  }
  return LaunchCmd(&cmd, get(caller_proc_handle), proc_handle);
}

HRESULT GoogleUpdateCore::OpenCallerProcessHandle(DWORD proc_id,
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

CString GoogleUpdateCore::GetCommandToLaunch(const TCHAR* app_guid,
                                             const TCHAR* cmd_id) {
  CString cmd_line;
  if (!app_guid || !cmd_id) {
    return cmd_line;
  }

  ConfigManager* config_manager = ConfigManager::Instance();
  CString clients_key_name = config_manager->machine_registry_clients();
  CString app_key_name = AppendRegKeyPath(clients_key_name, app_guid);

  RegKey::GetValue(app_key_name, cmd_id, &cmd_line);
  return cmd_line;
}

HRESULT GoogleUpdateCore::LaunchCmd(CString* cmd,
                                    HANDLE caller_proc_handle,
                                    ULONG_PTR* proc_handle) {
  if (!cmd || !caller_proc_handle || !proc_handle) {
    return E_INVALIDARG;
  }

  *proc_handle = NULL;
  HRESULT hr = S_OK;

  // This is a pseudo handle that must not be closed.
  HANDLE this_process_handle = ::GetCurrentProcess();

  PROCESS_INFORMATION pi = {0};
  hr = System::StartProcess(NULL, cmd->GetBuffer(), &pi);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[failed to launch cmd][%s][0x%08x]"), *cmd, hr));
    return hr;
  }

  // DuplicateHandle call will close the source handle regardless of any error
  // status returned.
  ASSERT1(pi.hProcess);
  VERIFY1(::CloseHandle(pi.hThread));

  scoped_process duplicate_proc_handle;

  DWORD desired_access = PROCESS_QUERY_INFORMATION | SYNCHRONIZE;
  bool res = ::DuplicateHandle(
      this_process_handle,             // Current process.
      pi.hProcess,                     // Process handle to duplicate.
      caller_proc_handle,              // Process receiving the handle.
      address(duplicate_proc_handle),  // Duplicated handle.
      desired_access,                  // Access requested for the new handle.
      false,                           // Don't inherit the new handle.
      DUPLICATE_CLOSE_SOURCE) != 0;    // Closes the source handle.
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


}  // namespace omaha

