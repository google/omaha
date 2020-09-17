// Copyright 2006-2010 Google Inc.
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
//
// The service can be started in one of two modes:
//   * As a regular service, typically at system startup.
//   * As a COM service, typically by an Omaha client using IGoogleUpdateCore.
// The COM case is distinguished from the regular service case by registering a
// ServiceParameters command line in the AppID registration.
//
// In all cases, the service initializes COM, and allows for IGoogleUpdateCore
// clients to connect to the service. In the regular service case, the service
// shuts down after a small idle check timeout, provided that there are no COM
// clients connected. In the COM server case, and in the case where there are
// COM clients connected in the regular service case, the service will shut down
// when the last client disconnects.
//
// To be exact, the service will initiate shutdown in all cases when the ATL
// module count drops to zero.
//
// ATL does not allow for directly reusing the delayed COM shutdown mechanism
// available for Local servers. The assumption likely being that services run
// forever. Since we do not want our service to run forever, we override some
// of the functions to get the same effect.

#ifndef OMAHA_SERVICE_SERVICE_MAIN_H_
#define OMAHA_SERVICE_SERVICE_MAIN_H_

#include <windows.h>

// Redefining the macro RegisterEventSource to evaluate to NULL so that
// CAtlServiceModuleT::LogEvent() code does not log to the event log. Doing this
// avoids duplicating the CAtlServiceModuleT code.
#undef RegisterEventSource
#define RegisterEventSource(x, ...) NULL

#include <atlbase.h>
#include <atlcom.h>
#include "base/basictypes.h"

#include "omaha/base/atlregmapex.h"
#include "omaha/base/debug.h"
// Use client/resource.h because it does not use StringFormatter and some of the
// strings are only used during Setup, which is part of the client.
// TODO(omaha3): It is a little unexpected to access strings in a header. It
// would be nice to avoid that. Also, this file is included by both client
// (setup_service.cc) and server (goopdate.cc) code.
#include "omaha/client/resource.h"
#include "omaha/common/command_line_builder.h"
#include "omaha/common/const_cmd_line.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/core/google_update_core.h"
#include "omaha/goopdate/google_update3.h"
#include "omaha/goopdate/non_localized_resource.h"
#include "omaha/goopdate/ondemand.h"
#include "omaha/goopdate/policy_status.h"
#include "omaha/goopdate/update3web.h"
#include "omaha/goopdate/worker.h"
#include "omaha/net/network_config.h"

namespace omaha {

class Update3ServiceMode {
 public:
  static CommandLineMode commandline_mode();
  static CString reg_name();
  static CString default_name();
  static DWORD service_start_type();
  static _ATL_OBJMAP_ENTRY* object_map();
  static bool allow_access_from_medium();
  static CString app_id_string();
  static CString GetCurrentServiceName();
  static HRESULT PreMessageLoop();
};

class UpdateMediumServiceMode {
 public:
  static CommandLineMode commandline_mode();
  static CString reg_name();
  static CString default_name();
  static DWORD service_start_type();
  static _ATL_OBJMAP_ENTRY* object_map();
  static bool allow_access_from_medium();
  static CString app_id_string();
  static CString GetCurrentServiceName();
  static HRESULT PreMessageLoop();
};

template <typename T>
class ServiceModule
    : public CAtlServiceModuleT<ServiceModule<T>, IDS_SERVICE_NAME> {
 public:
  typedef CAtlServiceModuleT<ServiceModule, IDS_SERVICE_NAME> Base;

  using Base::OnStop;
  using Base::SetServiceStatus;

  DECLARE_REGISTRY_APPID_RESOURCEID_EX(IDR_GOOGLE_UPDATE3_SERVICE_APPID,
                                       T::app_id_string())

  BEGIN_REGISTRY_MAP()
    REGMAP_ENTRY(_T("DESCRIPTION"),   _T("ServiceModule"))
    REGMAP_ENTRY(_T("FILENAME"),      kServiceFileName)
  END_REGISTRY_MAP()

  ServiceModule()
      : service_thread_(NULL),
        is_service_com_server_(false) {
    SERVICE_LOG(L1, (_T("[ServiceModule]")));
    _tcscpy(this->m_szServiceName, T::GetCurrentServiceName());
  }

  ~ServiceModule() {
    SERVICE_LOG(L1, (_T("[~ServiceModule]")));
    ASSERT1(!service_thread_);

    // ServiceModule is typically created on the stack. We cannot reset the
    // _pAtlModule here, because objects are destroyed at destructor unwind
    // time, and require access to the module.
  }

  HRESULT InitializeSecurity() throw() {
    SERVICE_LOG(L3, (_T("[InitializeSecurity]")));

    return InitializeServerSecurity(T::allow_access_from_medium());
  }

  void ServiceMain(DWORD argc, LPTSTR* argv) throw() {
    ASSERT1(argc <= 2);
    is_service_com_server_ =
        argc == 2 && !CString(argv[1]).CompareNoCase(kCmdLineServiceComServer);
    SERVICE_LOG(L3, (_T("[ServiceMain][is_service_com_server_][%d]"),
                     is_service_com_server_));
    Base::ServiceMain(argc, argv);
  }

  HRESULT RegisterClassObjects(DWORD class_context,
                                              DWORD flags) throw() {
    SERVICE_LOG(L3, (_T("[RegisterClassObjects]")));
    for (_ATL_OBJMAP_ENTRY* objmap_entry = T::object_map();
         objmap_entry->pclsid != NULL;
         objmap_entry++) {
      HRESULT hr = objmap_entry->RegisterClassObject(class_context, flags);
      if (FAILED(hr)) {
        SERVICE_LOG(LE, (_T("[RegisterClassObject failed][%s][0x%x]"),
                         GuidToString(*objmap_entry->pclsid), hr));
        return hr;
      }
    }

    return S_OK;
  }

  HRESULT RevokeClassObjects() throw() {
    SERVICE_LOG(L3, (_T("[RevokeClassObjects]")));
    for (_ATL_OBJMAP_ENTRY* objmap_entry = T::object_map();
         objmap_entry->pclsid != NULL;
         objmap_entry++) {
      HRESULT hr = objmap_entry->RevokeClassObject();
      if (FAILED(hr)) {
        SERVICE_LOG(LE, (_T("[RevokeClassObject failed][%s][0x%x]"),
                         GuidToString(*objmap_entry->pclsid), hr));
        return hr;
      }
    }

    return S_OK;
  }

  HRESULT RegisterServer(BOOL, const CLSID* = NULL) throw() {
    SERVICE_LOG(L3, (_T("[RegisterServer]")));
    for (_ATL_OBJMAP_ENTRY* objmap_entry = T::object_map();
         objmap_entry->pclsid != NULL;
         objmap_entry++) {
      HRESULT hr = objmap_entry->pfnUpdateRegistry(TRUE);
      if (FAILED(hr)) {
        SERVICE_LOG(LE, (_T("[pfnUpdateRegistry failed][%s][0x%x]"),
                         GuidToString(*objmap_entry->pclsid), hr));
        return hr;
      }

      hr = AtlRegisterClassCategoriesHelper(*objmap_entry->pclsid,
                                            objmap_entry->pfnGetCategoryMap(),
                                            TRUE);
      if (FAILED(hr)) {
        SERVICE_LOG(LE, (_T("[RegisterServer fail][%s][0x%x]"),
                         GuidToString(*objmap_entry->pclsid), hr));
        return hr;
      }
    }

    return S_OK;
  }

  HRESULT UnregisterServer(BOOL, const CLSID* = NULL) throw() {
    SERVICE_LOG(L3, (_T("[UnregisterServer]")));
    for (_ATL_OBJMAP_ENTRY* objmap_entry = T::object_map();
         objmap_entry->pclsid != NULL;
         objmap_entry++) {
      HRESULT hr = AtlRegisterClassCategoriesHelper(*objmap_entry->pclsid,
                       objmap_entry->pfnGetCategoryMap(),
                       FALSE);
      if (FAILED(hr)) {
        SERVICE_LOG(LE, (_T("[RegisterServer fail][%s][0x%x]"),
                         GuidToString(*objmap_entry->pclsid), hr));
        return hr;
      }

      hr = objmap_entry->pfnUpdateRegistry(FALSE);
      if (FAILED(hr)) {
        SERVICE_LOG(LE, (_T("[pfnUpdateRegistry failed][%s][0x%x]"),
                         GuidToString(*objmap_entry->pclsid), hr));
        return hr;
      }
    }

    return S_OK;
  }

  HRESULT RegisterCOMService() {
    SERVICE_LOG(L3, (_T("[RegisterCOMService]")));
    HRESULT hr = UpdateRegistryAppId(TRUE);
    if (FAILED(hr)) {
      SERVICE_LOG(LE, (_T("[UpdateRegistryAppId failed][0x%x]"), hr));
      return hr;
    }

    RegKey key_app_id;
    hr = key_app_id.Open(HKEY_CLASSES_ROOT, _T("AppID"), KEY_WRITE);
    if (FAILED(hr)) {
      SERVICE_LOG(LE, (_T("[Could not open HKLM\\AppID][0x%x]"), hr));
      return hr;
    }

    RegKey key;
    hr = key.Create(key_app_id.Key(), GetAppIdT());
    if (FAILED(hr)) {
      SERVICE_LOG(LE, (_T("[Fail open HKLM-AppID-%s][0x%x]"), GetAppId(), hr));
      return hr;
    }

    // m_szServiceName is set in the constructor.
    hr = key.SetValue(_T("LocalService"), this->m_szServiceName);
    if (FAILED(hr)) {
      SERVICE_LOG(LE, (_T("[Could not set LocalService value][0x%x]"), hr));
      return hr;
    }

    // The SCM will pass this switch to ServiceMain() during COM activation.
    hr = key.SetValue(_T("ServiceParameters"), kCmdLineServiceComServer);
    if (FAILED(hr)) {
      SERVICE_LOG(LE, (_T("[Set ServiceParameters value failed][0x%x]"), hr));
      return hr;
    }

    return RegisterServer(FALSE);
  }

  HRESULT UnregisterCOMService() {
    SERVICE_LOG(L3, (_T("[UnregisterCOMService]")));
    HRESULT hr = UnregisterServer(FALSE);
    if (FAILED(hr)) {
      SERVICE_LOG(LE, (_T("[UnregisterServer failed][0x%x]"), hr));
      return hr;
    }

    return UpdateRegistryAppId(FALSE);
  }

  HRESULT PreMessageLoop(int show_cmd) {
    UNREFERENCED_PARAMETER(show_cmd);

    SERVICE_LOG(L1, (_T("[PreMessageLoop]")));

    this->m_dwThreadID = ::GetCurrentThreadId();
    service_thread_ = ::OpenThread(SYNCHRONIZE, false, this->m_dwThreadID);

    if (is_service_com_server_) {
      return InitializeCOMServer();
    }

    // This is the regular service case. Call T::PreMessageLoop() and exit.
    SetServiceStatus(SERVICE_RUNNING);

    HRESULT hr = T::PreMessageLoop();
    if (FAILED(hr)) {
      SERVICE_LOG(LE, (_T("[T::PreMessageLoop() failed][0x%x]"), hr));
    }

    SetServiceStatus(SERVICE_STOP_PENDING);

    // S_FALSE is returned to exit the service immediately.
    return S_FALSE;
  }

  HRESULT PostMessageLoop() {
    SERVICE_LOG(L1, (_T("[PostMessageLoop]")));

    if (!is_service_com_server_) {
      return S_OK;
    }

    return Base::PostMessageLoop();
  }

  int Main(int show_cmd) {
    if (CAtlBaseModule::m_bInitFailed) {
      SERVICE_LOG(LE, (_T("[CAtlBaseModule init failed]")));
      return -1;
    }

    return static_cast<int>(Start(show_cmd));
  }

  // This is cloned from CAtlExeModuleT.Lock(). The one difference is the call
  // to ::CoAddRefServerProcess(). See the description for Unlock() below for
  // further information.
  virtual LONG Lock() throw() {
    ::CoAddRefServerProcess();
    LONG retval = CComGlobalsThreadModel::Increment(&this->m_nLockCnt);
    return retval;
  }

  // This is cloned from CAtlExeModuleT.Unlock(). The differences are:
  //
  // * the call to ::CoReleaseServerProcess(), to ensure that the class
  // factories are suspended once the lock count drops to zero. This fixes a
  // race condition where an activation request could come in in the middle
  // of shutting down. This shutdown mechanism works with free threaded servers.
  //
  // There are race issues with the ATL  delayed shutdown mechanism, hence the
  // associated code has been eliminated, and we have an assert to make sure
  // m_bDelayShutdown is not set.
  //
  // * the call to "OnStop()" instead of the "::PostThreadMessage(m_dwMainThre".
  // OnStop() correctly sets the service status to SERVICE_STOP_PENDING, and
  // posts a WM_QUIT to the service thread.
  virtual LONG Unlock() throw() {
    ASSERT1(!this->m_bDelayShutdown);

    ::CoReleaseServerProcess();
    LONG retval = CComGlobalsThreadModel::Decrement(&this->m_nLockCnt);

    if (retval == 0) {
      OnStop();
    }

    return retval;
  }

  // This is cloned from CAtlExeModuleT.MonitorShutdown(). The only difference
  // is the call to "OnStop()" instead of the
  // "::PostThreadMessage(m_dwMainThreadID".
  void MonitorShutdown() throw() {
    SERVICE_LOG(L3, (_T("[MonitorShutdown]")));

    while (true) {
      ::WaitForSingleObject(this->m_hEventShutdown, INFINITE);
      SERVICE_LOG(L4, (_T("[Infinite Wait][%d][%d]"),
                       this->m_bActivity, this->m_nLockCnt));
      DWORD wait = 0;
      do {
        this->m_bActivity = false;
        wait = ::WaitForSingleObject(this->m_hEventShutdown, this->m_dwTimeOut);
      } while (wait == WAIT_OBJECT_0);

      SERVICE_LOG(L4, (_T("[MonitorShutdown][%d][%d]"),
                       this->m_bActivity, this->m_nLockCnt));
      if (!this->m_bActivity && this->m_nLockCnt == 0) {
        ::CoSuspendClassObjects();
        if (this->m_nLockCnt == 0) {
          break;
        }
      }
    }

    ::CloseHandle(this->m_hEventShutdown);
    OnStop();
  }

  // This is cloned from CAtlExeModuleT.StartMonitor().
  HANDLE StartMonitor() throw() {
    SERVICE_LOG(L3, (_T("[StartMonitor]")));
    this->m_hEventShutdown = ::CreateEvent(NULL, false, false, NULL);
    if (this->m_hEventShutdown == NULL) {
      return NULL;
    }

    DWORD thread_id(0);
    HANDLE monitor = ::CreateThread(NULL, 0, MonitorProc, this, 0, &thread_id);
    if (monitor == NULL) {
      ::CloseHandle(this->m_hEventShutdown);
    }

    return monitor;
  }

  // This is cloned from CAtlExeModuleT.MonitorProc().
  static DWORD WINAPI MonitorProc(void* pv) throw() {
    SERVICE_LOG(L3, (_T("[MonitorProc]")));
    ServiceModule* service_module = static_cast<ServiceModule*>(pv);
    ASSERT1(service_module);

    service_module->MonitorShutdown();
    return 0;
  }

 private:
  // Should only be called from the client, such as during Setup, since it only
  // supports one language.
  // Assumes the resources have been loaded.
  static CString GetServiceDescription() {
    CString company_name;
    VERIFY1(company_name.LoadString(IDS_FRIENDLY_COMPANY_NAME));
    CString description;
    description.FormatMessage(IDS_SERVICE_DESCRIPTION, company_name);
    return description;
  }

  HRESULT InitializeCOMServer() {
    SERVICE_LOG(L1, (_T("[InitializeCOMServer]")));

    // Initialize COM security right at the beginning, because Worker
    // initialization can cause interface marshaling. The first few lines below
    // are adapted from the beginning of CAtlServiceModuleT::PreMessageLoop().
    ASSERT1(this->m_bService);
    HRESULT hr = InitializeSecurity();
    if (FAILED(hr)) {
      return hr;
    }

    DisableCOMExceptionHandling();

    NetworkConfigManager::set_is_machine(true);

    // Create NetworkConfigManager singleton by referencing it.
    NetworkConfigManager::Instance();

    // Register and resume the COM class objects. We call the CAtlExeModuleT
    // member instead of CAtlServiceModuleT, because the latter also tries to
    // initialize security, which we have already done above.
    hr = CAtlExeModuleT<ServiceModule>::PreMessageLoop(SW_HIDE);
    if (SUCCEEDED(hr)) {
      SetServiceStatus(SERVICE_RUNNING);
    }

    return hr;
  }

  // When Start executes, it blocks on StartServiceCtrlDispatcher.
  // Internally, the SCM creates a service thread which starts executing code
  // specified by SERVICE_TABLE_ENTRY. The ATL code then calls PreMessageLoop
  // and PostMessageLoop on this thread. When the service stops, the execution
  // flow returns from StartServiceCtrlDispatcher and the main thread blocks
  // waiting on the service thread to exit.
  // Before synchronizing the main thread and the service thread, race condition
  // resulted in http://b/1134747.
  HRESULT Start(int show_cmd) {
    SERVICE_LOG(L1, (_T("[Start]")));
    UNREFERENCED_PARAMETER(show_cmd);

    SERVICE_TABLE_ENTRY st[] = {
      { this->m_szServiceName, Base::_ServiceMain },
      { NULL, NULL }
    };

    this->m_status.dwWin32ExitCode = 0;
    if (!::StartServiceCtrlDispatcher(st)) {
      this->m_status.dwWin32ExitCode = ::GetLastError();
    }

    if (service_thread_) {
      DWORD result(::WaitForSingleObject(service_thread_, kShutdownIntervalMs));
      ASSERT1(result == WAIT_OBJECT_0);
      ::CloseHandle(service_thread_);
      service_thread_ = NULL;
    }

    return this->m_status.dwWin32ExitCode;
  }

  HANDLE service_thread_;   // The service thread provided by the SCM.
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

  DISALLOW_COPY_AND_ASSIGN(ServiceModule);
};

typedef ServiceModule<Update3ServiceMode> Update3ServiceModule;
typedef ServiceModule<UpdateMediumServiceMode> UpdateMediumServiceModule;

}  // namespace omaha

#endif  // OMAHA_SERVICE_SERVICE_MAIN_H_
