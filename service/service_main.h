// Copyright 2006-2009 Google Inc.
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

// The service is a bootstrap for a local system process to start
// when the computer starts. The service shuts itself down in 30 seconds.

#ifndef OMAHA_SERVICE_SERVICE_MAIN_H__
#define OMAHA_SERVICE_SERVICE_MAIN_H__

#include <atlbase.h>
#include <atlcom.h>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "goopdate/google_update_idl.h"
#include "omaha/common/atlregmapex.h"
#include "omaha/goopdate/const_goopdate.h"
#include "omaha/goopdate/resource.h"

namespace omaha {

class QueueTimer;

class ServiceModule
    : public CAtlServiceModuleT<ServiceModule, IDS_SERVICE_NAME> {
 public:
  typedef CAtlServiceModuleT<ServiceModule, IDS_SERVICE_NAME> Base;

  #pragma warning(push)
  // C4640: construction of local static object is not thread-safe
  #pragma warning(disable : 4640)

  DECLARE_REGISTRY_APPID_RESOURCEID_EX(IDR_GOOGLE_UPDATE_SERVICE_APPID,
      GuidToString(__uuidof(GoogleUpdateCoreClass)))

  BEGIN_REGISTRY_MAP()
    REGMAP_RESOURCE(_T("DESCRIPTION"), IDS_SERVICE_DESCRIPTION)
    REGMAP_ENTRY(_T("FILENAME"),       kServiceFileName)
  END_REGISTRY_MAP()

  #pragma warning(pop)

  ServiceModule();
  ~ServiceModule();

  // Runs the entry point for the service.
  int Main(int show_cmd);

  // These methods are called by ATL and they must be public.
  void ServiceMain(DWORD argc, LPTSTR* argv) throw();
  HRESULT PreMessageLoop(int show_cmd);
  HRESULT PostMessageLoop();
  HRESULT InitializeSecurity() throw();
  HRESULT RegisterClassObjects(DWORD class_context, DWORD flags) throw();
  HRESULT RevokeClassObjects() throw();
  HRESULT RegisterServer(BOOL reg_typelib = FALSE,
                         const CLSID* clsid = NULL) throw();
  HRESULT UnregisterServer(BOOL reg_typelib, const CLSID* clsid = NULL) throw();
  LONG Unlock() throw();
  void MonitorShutdown() throw();
  HANDLE StartMonitor() throw();
  static DWORD WINAPI MonitorProc(void* pv) throw();

  // These methods are helpers called by setup for service registration.
  HRESULT RegisterCOMService();
  HRESULT UnregisterCOMService();

 private:
  // Calls the service dispatcher to start the service.
  HRESULT Start(int show_cmd);

  // Creates and starts the idle check timer.
  HRESULT StartIdleCheckTimer();

  // Stops and destroys the idle check timer.
  HRESULT StopIdleCheckTimer();

  static void IdleCheckTimerCallback(QueueTimer* timer);

  HANDLE timer_queue_;
  HANDLE service_thread_;   // The service thread provided by the SCM.
  scoped_ptr<QueueTimer> idle_check_timer_;
  bool is_service_com_server_;  // True if the service is being invoked by COM.

  // Service shut down wait timeout. The main thread waits for the service
  // thread to exit, after the service stops.
  static const DWORD kShutdownIntervalMs = 1000L * 30;         // 30 seconds.

  // Service idle check timeout. The service shuts down itself after startup.
  static const DWORD kIdleCheckIntervalMs = 1000L * 30;        // 30 seconds.

  // Service failover constants.
  //
  // Time after which the SCM resets the failure count to zero if there are
  // no failures.
  static const DWORD kResetPeriodSec      = 60 * 60 * 24;     // 1 day.

  // Time to wait before performing the specified action.
  static const DWORD kActionDelayMs       = 1000L * 60 * 15;  // 15 minutes.

  // A private object map with custom registration works best, even though this
  // stuff is deprecated. This is because GoogleUpdate.exe has other objects
  // defined elsewhere and we do not want to expose those from the service.
  static _ATL_OBJMAP_ENTRY object_map_[2];

  DISALLOW_EVIL_CONSTRUCTORS(ServiceModule);
};

}  // namespace omaha

#endif  // OMAHA_SERVICE_SERVICE_MAIN_H__

