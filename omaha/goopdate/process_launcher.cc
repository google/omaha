// Copyright 2008-2010 Google Inc.
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


#include "omaha/goopdate/process_launcher.h"
#include "omaha/base/browser_utils.h"
#include "omaha/base/const_object_names.h"
#include "omaha/base/debug.h"
#include "omaha/base/logging.h"
#include "omaha/base/system.h"
#include "omaha/base/vista_utils.h"
#include "omaha/base/vistautil.h"
#include "omaha/core/google_update_core.h"

namespace omaha {

ProcessLauncher::ProcessLauncher() : StdMarshalInfo(true) {
  CORE_LOG(L6, (_T("[ProcessLauncher::ProcessLauncher]")));
}

ProcessLauncher::~ProcessLauncher() {
  CORE_LOG(L6, (_T("[ProcessLauncher::~ProcessLauncher]")));
}

STDMETHODIMP ProcessLauncher::LaunchCmdLine(const TCHAR* cmd_line) {
  CORE_LOG(L1, (_T("[ProcessLauncher::LaunchCmdLine][%s]"), cmd_line));
  return LaunchCmdLineEx(cmd_line, NULL, NULL, NULL);
}

STDMETHODIMP ProcessLauncher::LaunchBrowser(DWORD type, const TCHAR* url) {
  CORE_LOG(L1, (_T("[ProcessLauncher::LaunchBrowser][%d][%s]"), type, url));
  if (type >= BROWSER_MAX || url == NULL) {
    return E_INVALIDARG;
  }
  return RunBrowser(static_cast<BrowserType>(type), url);
}

// This method delegates to the internal interface exposed by the system
// service, and if the service cannot be instantiated, exposed by a Local COM
// Server.
//
// Non elevated callers can request a command to be run elevated.
// The command must be registered before by elevated code to prevent
// launching untrusted commands. The security of the command is based on
// having the correct registry ACLs for the machine Omaha registry.
STDMETHODIMP ProcessLauncher::LaunchCmdElevated(const WCHAR* app_guid,
                                                const WCHAR* cmd_id,
                                                DWORD caller_proc_id,
                                                ULONG_PTR* proc_handle) {
  CORE_LOG(L3, (_T("[ProcessLauncher::LaunchCmdElevated]")
                _T("[app %s][cmd %s][pid %d]"),
                app_guid, cmd_id, caller_proc_id));

  ASSERT1(app_guid);
  ASSERT1(cmd_id);
  ASSERT1(proc_handle);

  CComPtr<IGoogleUpdateCore> google_update_core;
  HRESULT hr =
      google_update_core.CoCreateInstance(__uuidof(GoogleUpdateCoreClass));

  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[CoCreate GoogleUpdateCoreClass failed][0x%x]"), hr));

    if (!vista_util::IsVistaOrLater() && !vista_util::IsUserAdmin()) {
      return hr;
    }

    hr = System::CoCreateInstanceAsAdmin(NULL,
                                         __uuidof(GoogleUpdateCoreMachineClass),
                                         IID_PPV_ARGS(&google_update_core));
    if (FAILED(hr)) {
      CORE_LOG(LE, (_T("[GoogleUpdateCoreMachineClass failed][0x%x]"), hr));
      return hr;
    }
  }

  ASSERT1(google_update_core);
  hr = ::CoSetProxyBlanket(google_update_core, RPC_C_AUTHN_DEFAULT,
      RPC_C_AUTHZ_DEFAULT, COLE_DEFAULT_PRINCIPAL, RPC_C_AUTHN_LEVEL_DEFAULT,
      RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_DEFAULT);
  if (FAILED(hr)) {
    return hr;
  }

  return google_update_core->LaunchCmdElevated(app_guid,
                                               cmd_id,
                                               caller_proc_id,
                                               proc_handle);
}

STDMETHODIMP ProcessLauncher::LaunchCmdLineEx(const TCHAR* cmd_line,
                                              DWORD* server_proc_id,
                                              ULONG_PTR* proc_handle,
                                              ULONG_PTR* stdout_handle) {
  CORE_LOG(L1, (_T("[ProcessLauncher::LaunchCmdLineEx][%s]"), cmd_line));
  if (cmd_line == NULL) {
    return E_INVALIDARG;
  }
  if ((server_proc_id == NULL) != (proc_handle == NULL) ||
      (server_proc_id == NULL) != (stdout_handle == NULL)) {
    return E_INVALIDARG;
  }

  // http://b/3329538: In the impersonated case, need to create a fresh
  // environment block and ::CreateProcess. RunAsCurrentUser does just that.
  HRESULT hr = vista::RunAsCurrentUser(
      cmd_line,
      reinterpret_cast<HANDLE*>(stdout_handle),
      reinterpret_cast<HANDLE*>(proc_handle));
  if (FAILED(hr)) {
    UTIL_LOG(LW, (_T("[RunAsCurrentUser failed][0x%x]"), hr));
    return hr;
  }

  if (server_proc_id) {
    *server_proc_id = ::GetCurrentProcessId();
  }

  return S_OK;
}

}  // namespace omaha

