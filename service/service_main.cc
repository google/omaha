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


#include "omaha/service/service_main.h"

#include "omaha/common/const_cmd_line.h"
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/logging.h"
#include "omaha/common/queue_timer.h"
#include "omaha/core/google_update_core.h"
#include "omaha/goopdate/config_manager.h"
#include "omaha/goopdate/command_line_builder.h"
#include "omaha/goopdate/goopdate_utils.h"

namespace omaha {

_ATL_OBJMAP_ENTRY ServiceModule::object_map_[] = {
  OBJECT_ENTRY(__uuidof(GoogleUpdateCoreClass), GoogleUpdateCore)
END_OBJECT_MAP()

ServiceModule::ServiceModule()
    : timer_queue_(NULL),
      service_thread_(NULL),
      is_service_com_server_(false) {
  SERVICE_LOG(L1, (_T("[ServiceModule::ServiceModule]")));
  _tcscpy(m_szServiceName, ConfigManager::GetCurrentServiceName());
}

ServiceModule::~ServiceModule() {
  SERVICE_LOG(L1, (_T("[ServiceModule::~ServiceModule]")));
  ASSERT1(!service_thread_);
  ASSERT1(!timer_queue_);
  ASSERT1(!idle_check_timer_.get());

  // ServiceModule is typically created on the stack. We therefore reset the
  // global ATL Module to NULL when ServiceModule is destroyed.
  _pAtlModule = NULL;
}

// Security descriptor for the Service's CoInitializeSecurity
void GetCOMSecDesc(CSecurityDesc* security_descriptor) {
  security_descriptor->SetOwner(Sids::Admins());
  security_descriptor->SetGroup(Sids::Admins());
  CDacl dacl;
  dacl.AddAllowedAce(Sids::System(), COM_RIGHTS_EXECUTE);
  dacl.AddAllowedAce(Sids::Admins(), COM_RIGHTS_EXECUTE);
  dacl.AddAllowedAce(Sids::Interactive(), COM_RIGHTS_EXECUTE);
  security_descriptor->SetDacl(dacl);
  security_descriptor->MakeAbsolute();
}

HRESULT ServiceModule::InitializeSecurity() throw() {
  SERVICE_LOG(L3, (_T("[ServiceModule::InitializeSecurity]")));

  CSecurityDesc security_descriptor;
  GetCOMSecDesc(&security_descriptor);
  HRESULT hr = ::CoInitializeSecurity(
      const_cast<SECURITY_DESCRIPTOR*>(
          security_descriptor.GetPSECURITY_DESCRIPTOR()),
      -1,
      NULL,
      NULL,
      RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
      RPC_C_IMP_LEVEL_IDENTIFY,
      NULL,
      EOAC_DYNAMIC_CLOAKING | EOAC_DISABLE_AAA | EOAC_NO_CUSTOM_MARSHAL,
      NULL);
  ASSERT(SUCCEEDED(hr), (_T("[ServiceModule::InitializeSecurity][0x%x]"), hr));
  return hr;
}

void ServiceModule::ServiceMain(DWORD argc, LPTSTR* argv) throw() {
  ASSERT1(argc <= 2);
  is_service_com_server_ =
      argc == 2 && !CString(argv[1]).CompareNoCase(kCmdLineServiceComServer);
  SERVICE_LOG(L3, (_T("[ServiceMain][is_service_com_server_][%d]"),
                   is_service_com_server_));
  Base::ServiceMain(argc, argv);
}

HRESULT ServiceModule::RegisterClassObjects(DWORD class_context,
                                            DWORD flags) throw() {
  SERVICE_LOG(L3, (_T("[ServiceModule::RegisterClassObjects]")));
  for (_ATL_OBJMAP_ENTRY* objmap_entry = object_map_;
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

HRESULT ServiceModule::RevokeClassObjects() throw() {
  SERVICE_LOG(L3, (_T("[ServiceModule::RevokeClassObjects]")));
  for (_ATL_OBJMAP_ENTRY* objmap_entry = object_map_;
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

HRESULT ServiceModule::RegisterServer(BOOL, const CLSID*) throw() {
  SERVICE_LOG(L3, (_T("[ServiceModule::RegisterServer]")));
  for (_ATL_OBJMAP_ENTRY* objmap_entry = object_map_;
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
      SERVICE_LOG(LE, (_T("[AtlRegisterClassCategoriesHelper fail][%s][0x%x]"),
                       GuidToString(*objmap_entry->pclsid), hr));
      return hr;
    }
  }

  return S_OK;
}

HRESULT ServiceModule::UnregisterServer(BOOL, const CLSID*) throw() {
  SERVICE_LOG(L3, (_T("[ServiceModule::UnregisterServer]")));
  for (_ATL_OBJMAP_ENTRY* objmap_entry = object_map_;
       objmap_entry->pclsid != NULL;
       objmap_entry++) {
    HRESULT hr = AtlRegisterClassCategoriesHelper(*objmap_entry->pclsid,
                     objmap_entry->pfnGetCategoryMap(),
                     FALSE);
    if (FAILED(hr)) {
      SERVICE_LOG(LE, (_T("[AtlRegisterClassCategoriesHelper fail][%s][0x%x]"),
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

HRESULT ServiceModule::RegisterCOMService() {
  SERVICE_LOG(L3, (_T("[ServiceModule::RegisterCOMService]")));
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
    SERVICE_LOG(LE, (_T("[Fail open HKLM\\AppID\\%s][0x%x]"), GetAppId(), hr));
    return hr;
  }

  // m_szServiceName is set in the constructor.
  hr = key.SetValue(_T("LocalService"), m_szServiceName);
  if (FAILED(hr)) {
    SERVICE_LOG(LE, (_T("[Could not set LocalService value][0x%x]"), hr));
    return hr;
  }

  // The SCM will pass this switch to ServiceMain() during COM activation.
  hr = key.SetValue(_T("ServiceParameters"), kCmdLineServiceComServer);
  if (FAILED(hr)) {
    SERVICE_LOG(LE, (_T("[Could not set ServiceParameters value][0x%x]"), hr));
    return hr;
  }

  return RegisterServer(FALSE);
}

HRESULT ServiceModule::UnregisterCOMService() {
  SERVICE_LOG(L3, (_T("[ServiceModule::UnregisterCOMService]")));
  HRESULT hr = UnregisterServer(FALSE);
  if (FAILED(hr)) {
    SERVICE_LOG(LE, (_T("[UnregisterServer failed][0x%x]"), hr));
    return hr;
  }

  return UpdateRegistryAppId(FALSE);
}

HRESULT ServiceModule::PreMessageLoop(int show_cmd) {
  SERVICE_LOG(L1, (_T("[ServiceModule::PreMessageLoop]")));

  m_dwThreadID = ::GetCurrentThreadId();
  // Use the delayed monitor thread COM shutdown mechanism.
  m_bDelayShutdown = true;
  service_thread_ = ::OpenThread(SYNCHRONIZE, false, m_dwThreadID);

  // Initialize COM, COM security, register COM class objects.
  HRESULT hr = Base::PreMessageLoop(show_cmd);
  ASSERT(SUCCEEDED(hr), (_T("[Base::PreMessageLoop failed][0x%x]"), hr));
  if (is_service_com_server_) {
    SERVICE_LOG(L3, (_T("[Service in COM server mode.]")));
    return hr;
  }

  // This is the regular service case. Increment the ATL module count. Run an
  // idle timer, at the end of which the module count is decremented. The
  // service will exit if the module count drops to zero.
  Lock();
  VERIFY1(SUCCEEDED(StartIdleCheckTimer()));

  SERVICE_LOG(L1, (_T("[Starting Google Update core...]")));
  CommandLineBuilder builder(COMMANDLINE_MODE_CORE);
  CString args = builder.GetCommandLineArgs();
  hr = goopdate_utils::StartGoogleUpdateWithArgs(true, args, NULL);
  if (FAILED(hr)) {
    SERVICE_LOG(LE, (_T("[Starting Google Update failed][0x%08x]"), hr));
  }

  return S_OK;
}

HRESULT ServiceModule::PostMessageLoop() {
  SERVICE_LOG(L1, (_T("[ServiceModule::PostMessageLoop]")));
  if (!is_service_com_server_) {
    VERIFY1(SUCCEEDED(StopIdleCheckTimer()));
  }

  return Base::PostMessageLoop();
}

int ServiceModule::Main(int show_cmd) {
  if (CAtlBaseModule::m_bInitFailed) {
    SERVICE_LOG(LE, (_T("[CAtlBaseModule init failed]")));
    return -1;
  }
  HRESULT hr = Start(show_cmd);
  return static_cast<int>(hr);
}

// When ServiceModule::Start executes, it blocks on StartServiceCtrlDispatcher.
// Internally, the SCM creates a service thread which starts executing the code
// specified by the SERVICE_TABLE_ENTRY. The ATL code then calls PreMessageLoop
// and PostMessageLoop on this thread. When the service stops, the execution
// flow returns from StartServiceCtrlDispatcher and the main thread blocks
// waiting on the service thread to exit.
// Before synchronizing the main thread and the service thread, a race condition
// resulted in http://b/1134747.
HRESULT ServiceModule::Start(int show_cmd) {
  SERVICE_LOG(L1, (_T("[ServiceModule::Start]")));
  UNREFERENCED_PARAMETER(show_cmd);

  SERVICE_TABLE_ENTRY st[] = {
    { m_szServiceName, Base::_ServiceMain },
    { NULL, NULL }
  };

  m_status.dwWin32ExitCode = 0;
  if (!::StartServiceCtrlDispatcher(st)) {
    m_status.dwWin32ExitCode = ::GetLastError();
  }

  if (service_thread_) {
    DWORD result = ::WaitForSingleObject(service_thread_, kShutdownIntervalMs);
    ASSERT1(result == WAIT_OBJECT_0);
    ::CloseHandle(service_thread_);
    service_thread_ = NULL;
  }

  return m_status.dwWin32ExitCode;
}

HRESULT ServiceModule::StartIdleCheckTimer() {
  SERVICE_LOG(L1, (_T("[ServiceModule::StartIdleCheckTimer]")));

  timer_queue_ = ::CreateTimerQueue();
  if (!timer_queue_) {
    HRESULT hr = HRESULTFromLastError();
    SERVICE_LOG(LE, (_T("[CreateTimerQueue failed][0x%08x]"), hr));
    return hr;
  }
  idle_check_timer_.reset(new QueueTimer(timer_queue_,
                                         &ServiceModule::IdleCheckTimerCallback,
                                         this));
  HRESULT hr = idle_check_timer_->Start(kIdleCheckIntervalMs,
                                        0,
                                        WT_EXECUTEDEFAULT | WT_EXECUTEONLYONCE);
  if (FAILED(hr)) {
    SERVICE_LOG(LE, (_T("[idle check time failed to start][0x%08x]"), hr));
    return hr;
  }
  return S_OK;
}

HRESULT ServiceModule::StopIdleCheckTimer() {
  SERVICE_LOG(L1, (_T("[ServiceModule::StopIdleCheckTimer]")));
  idle_check_timer_.reset();
  if (timer_queue_) {
    VERIFY1(::DeleteTimerQueueEx(timer_queue_, INVALID_HANDLE_VALUE));
  }
  timer_queue_ = NULL;
  return S_OK;
}


void ServiceModule::IdleCheckTimerCallback(QueueTimer* timer) {
  SERVICE_LOG(L1, (_T("[ServiceModule::IdleCheckTimerCallback]")));

  ASSERT1(timer);
  ASSERT1(timer->ctx());

  ServiceModule* service_module = static_cast<ServiceModule*>(timer->ctx());
  service_module->Unlock();
}

// If the module count drops to zero, Unlock() signals an event. There is a
// monitor thread listening to this event, which initiates shutdown of the
// service.
// This is cloned from CAtlExeModuleT.Unlock(). The only difference is the call
// to "OnStop()" instead of the "::PostThreadMessage(m_dwMainThreadID...".
// OnStop() correctly sets the service status to SERVICE_STOP_PENDING, and posts
// a WM_QUIT to the service thread.
LONG ServiceModule::Unlock() throw() {
  LONG retval = CComGlobalsThreadModel::Decrement(&m_nLockCnt);
  SERVICE_LOG(L3, (_T("[ServiceModule::Unlock][%d]"), retval));

  if (retval == 0) {
    if (m_bDelayShutdown) {
      m_bActivity = true;
      ::SetEvent(m_hEventShutdown);
    } else {
      OnStop();
    }
  }

  return retval;
}

// This is cloned from CAtlExeModuleT.MonitorShutdown(). The only difference is
// the call to "OnStop()" instead of the "::PostThreadMessage(m_dwMainThreadID".
void ServiceModule::MonitorShutdown() throw() {
  SERVICE_LOG(L3, (_T("[ServiceModule::MonitorShutdown]")));

  while (true) {
    ::WaitForSingleObject(m_hEventShutdown, INFINITE);
    SERVICE_LOG(L4, (_T("[Infinite Wait][%d][%d]"), m_bActivity, m_nLockCnt));

    DWORD wait = 0;
    do {
      m_bActivity = false;
      wait = ::WaitForSingleObject(m_hEventShutdown, m_dwTimeOut);
    } while (wait == WAIT_OBJECT_0);

    SERVICE_LOG(L4, (_T("[MonitorShutdown][%d][%d]"), m_bActivity, m_nLockCnt));
    if (!m_bActivity && m_nLockCnt == 0) {
      ::CoSuspendClassObjects();
      if (m_nLockCnt == 0) {
        break;
      }
    }
  }

  ::CloseHandle(m_hEventShutdown);
  OnStop();
}

// This is cloned from CAtlExeModuleT.StartMonitor().
HANDLE ServiceModule::StartMonitor() throw() {
  SERVICE_LOG(L3, (_T("[ServiceModule::StartMonitor]")));
  m_hEventShutdown = ::CreateEvent(NULL, false, false, NULL);
  if (m_hEventShutdown == NULL) {
    return NULL;
  }

  DWORD thread_id(0);
  HANDLE monitor = ::CreateThread(NULL, 0, MonitorProc, this, 0, &thread_id);
  if (monitor == NULL) {
    ::CloseHandle(m_hEventShutdown);
  }

  return monitor;
}

// This is cloned from CAtlExeModuleT.MonitorProc().
DWORD WINAPI ServiceModule::MonitorProc(void* pv) throw() {
  SERVICE_LOG(L3, (_T("[ServiceModule::MonitorProc]")));
  ServiceModule* service_module = static_cast<ServiceModule*>(pv);
  ASSERT1(service_module);

  service_module->MonitorShutdown();
  return 0;
}

}  // namespace omaha

