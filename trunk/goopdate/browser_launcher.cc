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


#include "omaha/goopdate/browser_launcher.h"

#include "omaha/common/browser_utils.h"
#include "omaha/common/const_object_names.h"
#include "omaha/common/debug.h"
#include "omaha/common/exception_barrier.h"
#include "omaha/common/logging.h"
#include "omaha/common/system.h"
#include "omaha/common/vistautil.h"
#include "omaha/core/google_update_core.h"
#include "omaha/goopdate/google_update_proxy.h"

namespace omaha {

ProcessLauncher::ProcessLauncher() {}

ProcessLauncher::~ProcessLauncher() {}

STDMETHODIMP ProcessLauncher::LaunchCmdLine(const TCHAR* cmd_line) {
  CORE_LOG(L1, (_T("[ProcessLauncher::LaunchCmdLine][%s]"), cmd_line));
  // The exception barrier is needed, because any exceptions that are thrown
  // in this method will get caught by the COM run time. We compile with
  // exceptions off, and do not expect to throw any exceptions. This barrier
  // will treat an exception in this method as a unhandled exception.
  ExceptionBarrier barrier;
  if (cmd_line == NULL) {
    return E_INVALIDARG;
  }
  return System::ShellExecuteCommandLine(cmd_line, NULL, NULL);
}

STDMETHODIMP ProcessLauncher::LaunchBrowser(DWORD type, const TCHAR* url) {
  CORE_LOG(L1, (_T("[ProcessLauncher::LaunchBrowser][%d][%s]"), type, url));
  // The exception barrier is needed, because any exceptions that are thrown
  // in this method will get caught by the COM run time. We compile with
  // exceptions off, and do not expect to throw any exceptions. This barrier
  // will treat an exception in this method as a unhandled exception.
  ExceptionBarrier barrier;
  if (type >= BROWSER_MAX || url == NULL) {
    return E_INVALIDARG;
  }
  return ShellExecuteBrowser(static_cast<BrowserType>(type), url);
}

// This method delegates to the internal interface exposed by the system
// service, and if the service cannot be installed, exposed by the core.
// When starting, if the service is not installed, the core registers a proxy
// for its interface in shared memory.
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

  ExceptionBarrier barrier;

  ASSERT1(app_guid);
  ASSERT1(cmd_id);
  ASSERT1(proc_handle);

  CComPtr<IGoogleUpdateCore> google_update_core;
  HRESULT hr =
      google_update_core.CoCreateInstance(__uuidof(GoogleUpdateCoreClass));

  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[CoCreate GoogleUpdateCoreClass failed][0x%x]"), hr));

    SharedMemoryAttributes attr(kGoogleUpdateCoreSharedMemoryName,
                                CSecurityDesc());
    GoogleUpdateCoreProxy google_update_core_proxy(true, &attr);
    hr = google_update_core_proxy.GetObject(&google_update_core);
    if (FAILED(hr)) {
      CORE_LOG(LE, (_T("[GetObject for IGoogleUpdateCore failed][0x%x]"), hr));
      return hr;
    }
  }
  if (!google_update_core) {
    CORE_LOG(LE, (_T("[IGoogleUpdateCore is null]")));
    return E_UNEXPECTED;
  }
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

}  // namespace omaha

