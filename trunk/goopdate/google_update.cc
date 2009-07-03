// Copyright 2007-2009 Google Inc.
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
// Contains the ATL exe server used for OnDemand, as well as the ProcessLauncher
// server used to launch a process.
// The ProcessLauncher server is to be used only by the machine google update.
// The idea is that the machine Google Update is elevated and launching
// a process, say a browser, from that elevated process. This will cause the
// browser to run elevated, which is a bad idea.
// What is needed is the ability to run medium integrity processes from a
// high integrity process. This can be done in two ways:
// 1. Create a service and expose some form of IPC. A service is required
//    because admins (even elevated) cannot call CreateProcessAsUser. Admins
//    do not posess the required privilege. However the local system account
//    has this privilege to call CreateProcessAsUser and hence can be used to
//    start the browser with the token of a medium integrity process.
// 2. Create a COM local server. Impersonate a medium integrity user in the
//    client. Fortunately the impersonation works, then create instance the COM
//    local server. If the COM security is set to use DYNAMIC_CLOAKING, then
//    the local server will be created using the thread credentials, allowing
//    the COM local server to be launched as medium integrity.
// This class implements the second method listed above.
// The server listens to the machine google update's shut down event.

#include "omaha/goopdate/google_update.h"

#include <windows.h>
#include "omaha/common/debug.h"
#include "omaha/common/event_handler.h"
#include "omaha/common/error.h"
#include "omaha/common/logging.h"
#include "omaha/common/const_object_names.h"
#include "omaha/common/reactor.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/utils.h"
#include "omaha/goopdate/command_line.h"
#include "omaha/goopdate/const_goopdate.h"
#include "omaha/goopdate/google_update_idl_datax.h"
#include "omaha/goopdate/goopdate_utils.h"
#include "omaha/worker/worker.h"

namespace omaha {

GoogleUpdate::GoogleUpdate() {}

GoogleUpdate::~GoogleUpdate() {}

HRESULT GoogleUpdate::Main() {
  // Disable the delay on shutdown mechanism inside ATL.
  m_bDelayShutdown = false;
  return CAtlExeModuleT<GoogleUpdate>::WinMain(0);
}

bool ShouldRegisterClsid(const CLSID& clsid) {
  bool is_machine = goopdate_utils::IsRunningFromOfficialGoopdateDir(true);

  // Machine-only CLSIDs.
  if (IsEqualGUID(clsid, __uuidof(ProcessLauncherClass)) ||
      IsEqualGUID(clsid, __uuidof(OnDemandMachineAppsClass))) {
    return is_machine;
  }

  // User-only CLSIDs.
  if (IsEqualGUID(clsid, __uuidof(OnDemandUserAppsClass))) {
    return !is_machine;
  }

  // Unknown CLSIDs.
  ASSERT(false, (_T("[Unknown CLSID][%s]"), GuidToString(clsid)));
  return false;
}

HRESULT GoogleUpdate::RegisterClassObjects(DWORD, DWORD) throw() {
  HRESULT hr = S_FALSE;
  for (_ATL_OBJMAP_ENTRY** entry = _AtlComModule.m_ppAutoObjMapFirst;
       entry < _AtlComModule.m_ppAutoObjMapLast && SUCCEEDED(hr);
       entry++) {
    if (*entry != NULL && ShouldRegisterClsid(*(*entry)->pclsid)) {
      hr = (*entry)->RegisterClassObject(CLSCTX_LOCAL_SERVER,
                                           REGCLS_SINGLEUSE | REGCLS_SUSPENDED);
    }
  }

  return hr;
}
HRESULT GoogleUpdate::RevokeClassObjects() throw() {
    return AtlComModuleRevokeClassObjects(&_AtlComModule);
}

HRESULT RegisterOrUnregisterExe(bool is_register) {
  HRESULT hr = S_FALSE;
  for (_ATL_OBJMAP_ENTRY** entry = _AtlComModule.m_ppAutoObjMapFirst;
       entry < _AtlComModule.m_ppAutoObjMapLast && SUCCEEDED(hr);
       entry++) {
    if (*entry != NULL) {
      const CLSID& clsid = *(*entry)->pclsid;
      if (!ShouldRegisterClsid(clsid)) {
        continue;
      }

      hr = is_register ? _AtlComModule.RegisterServer(false, &clsid) :
                         _AtlComModule.UnregisterServer(false, &clsid);
      ASSERT(SUCCEEDED(hr), (_T("[RegisterOrUnregisterExe fail][%d][0x%x][%s]"),
                             is_register, hr, GuidToString(clsid)));
    }
  }

  return hr;
}

HRESULT RegisterOrUnregisterProxy(bool is_register) {
  HRESULT hr = is_register ? PrxDllRegisterServer() : PrxDllUnregisterServer();
  if (FAILED(hr)) {
    CORE_LOG(LW, (_T("[RegisterOrUnregisterProxy failed][%d][0x%x]"),
                  is_register, hr));
  }
  return hr;
}

HRESULT GoogleUpdate::RegisterServer(BOOL, const CLSID*) throw() {
  HRESULT hr = goopdate_utils::RegisterOrUnregisterModule(
      true, &RegisterOrUnregisterProxy);
  if (FAILED(hr)) {
    return hr;
  }

  return goopdate_utils::RegisterOrUnregisterModule(true,
                                                    &RegisterOrUnregisterExe);
}

HRESULT GoogleUpdate::UnregisterServer(BOOL, const CLSID*) throw() {
  HRESULT hr = goopdate_utils::RegisterOrUnregisterModule(
      false, &RegisterOrUnregisterExe);
  if (FAILED(hr)) {
    return hr;
  }

  return goopdate_utils::RegisterOrUnregisterModule(false,
                                                    &RegisterOrUnregisterProxy);
}

HRESULT GoogleUpdate::PreMessageLoop(int show_cmd) throw() {
  bool is_machine = goopdate_utils::IsRunningFromOfficialGoopdateDir(true);
  worker_.reset(new Worker(is_machine));
  return CAtlExeModuleT<GoogleUpdate>::PreMessageLoop(show_cmd);
}

HRESULT GoogleUpdate::PostMessageLoop() throw() {
  HRESULT hr = CAtlExeModuleT<GoogleUpdate>::PostMessageLoop();
  worker_.reset();
  return hr;
}

}  // namespace omaha

