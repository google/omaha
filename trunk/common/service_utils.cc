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
// Service-related utilities.
//

#include "omaha/common/service_utils.h"

#include <windows.h>
#include "omaha/common/constants.h"
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/reg_key.h"
#include "omaha/common/smart_handle.h"
#include "omaha/common/string.h"
#include "omaha/common/timer.h"
#include "omaha/common/utils.h"

namespace omaha {

HRESULT ScmDatabase::EnumerateServices(
    ScmDatabase::EnumerateServicesCallback callback,
    void* callback_context) {
  ASSERT1(callback);
  if (!callback)
    return E_POINTER;

  const wchar_t* kServicesRegKeyFromRoot =
    L"SYSTEM\\CurrentControlSet\\Services";

  HRESULT hr = E_FAIL;

  RegKey services_key;
  if (FAILED(hr = services_key.Open(HKEY_LOCAL_MACHINE,
                                    kServicesRegKeyFromRoot,
                                    KEY_ENUMERATE_SUB_KEYS))) {
    ASSERT1(false);
    REPORT(false, R_ERROR, (L"Couldn't open services subkey, hr=0x%x", hr),
           9834572);
    return hr;
  }

  CString service_name;
  int key_index = 0;
  while (SUCCEEDED(hr = services_key.GetSubkeyNameAt(key_index++,
                                                     &service_name))) {
    hr = callback(callback_context, service_name);
    if (FAILED(hr) || hr == S_FALSE) {
      // Callback asked to terminate enumeration.
      return hr;
    }
  }

  if (hr != HRESULT_FROM_WIN32(ERROR_NO_MORE_ITEMS)) {
    ASSERT1(false);
    REPORT(false, R_ERROR, (L"Failed enumerating service subkeys: 0x%x", hr),
           1499372);
    return hr;
  }

  return S_OK;
}

bool ScmDatabase::IsServiceStateEqual(SC_HANDLE service, DWORD state) {
  ASSERT1(service);

  DWORD bytes_needed_ignored = 0;
  byte buffer[8 * 1024] = { 0 };
  QUERY_SERVICE_CONFIG* service_config =
    reinterpret_cast<QUERY_SERVICE_CONFIG*>(buffer);
  if (!::QueryServiceConfig(service, service_config, sizeof(buffer),
                            &bytes_needed_ignored)) {
    ASSERT(false, (L"Failed to query service config, perhaps handle is missing "
                   L"SERVICE_QUERY_CONFIG rights?"));
    return false;
  }

  return (service_config[0].dwStartType == state);
}

bool ScmDatabase::IsServiceMarkedDeleted(SC_HANDLE service) {
  ASSERT1(service);

  // Services that have been marked deleted are always in the
  // SERVICE_DISABLED state.  The converse is not true, and unfortunately
  // there is no way to check if a service has been marked deleted except by
  // attempting to change one of its configuration parameters, at which
  // point you get a specific error indicating it has been marked deleted.
  //
  // The following call to ChangeServiceConfig does not actually change any
  // of the service's configuration, but should hopefully return the
  // specific error if the service has been marked deleted.
  if (!::ChangeServiceConfig(service, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE,
                             SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL,
                             NULL, NULL, NULL) &&
      ::GetLastError() == ERROR_SERVICE_MARKED_FOR_DELETE) {
    ASSERT1(IsServiceStateEqual(service, SERVICE_DISABLED));
    return true;
  } else {
    return false;
  }
}

HRESULT ServiceInstall::UninstallByPrefix(void* context,
                                          const wchar_t* service_name) {
  ASSERT1(context != NULL);
  if (!context)
    return E_POINTER;

  UninstallByPrefixParams* params =
    reinterpret_cast<UninstallByPrefixParams*>(context);

  if (String_StartsWith(service_name, params->prefix, true) &&
      lstrcmpiW(service_name, params->unless_matches) != 0) {
    // The service must be stopped before attempting to remove it from the
    // database. Otherwise, the SCM database remains dirty and all service
    // functions return ERROR_SERVICE_MARKED_FOR_DELETE until the system is
    // restarted.
    StopService(service_name);

    scoped_service scm(::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS));
    if (!scm) {
      HRESULT hr = HRESULTFromLastError();
      ASSERT1(false);
      REPORT(false, R_ERROR, (L"Failed to open SCM: 0x%x", hr), 77223399);
      return hr;
    }
    scoped_service service(::OpenService(get(scm),
                                         service_name,
                                         SERVICE_CHANGE_CONFIG | DELETE));
    if (service) {
      // The service may not get deleted immediately; if there are handles to
      // it open, it won't get deleted until the last one is closed.  If the
      // service is running, it won't get deleted immediately but rather will be
      // marked for deletion (which happens on next reboot).  Having to wait for
      // a while and even until reboot doesn't matter much to us as our new
      // service is installed under a new name and we are just cleaning up old
      // ones.
      if (!::DeleteService(get(service))) {
        // We do not assert but just report so that we know if this happens
        // abnormally often.
        if (::GetLastError() == ERROR_SERVICE_MARKED_FOR_DELETE) {
          REPORT(false, R_INFO,
                 (L"Failed to immediately delete service %s", service_name),
                 5440098);
        } else {
          ASSERT(false, (L"Failed to delete service %s, error %d",
                         service_name, ::GetLastError()));
        }
        // DO NOT return an error here; we want to keep going through all the
        // services.
      } else {
        SERVICE_LOG(L1,
                    (L"Deleted old service %s", service_name));
      }
    } else {
      // Per documentation of the EnumerateServicesCallback interface we can
      // expect not to be able to open the service with one of the following two
      // error codes, because of discrepancies between the registry and the SCM
      // database in memory.
      DWORD last_error = ::GetLastError();
      ASSERT(last_error == ERROR_SERVICE_DOES_NOT_EXIST ||
             last_error == ERROR_INVALID_NAME,
             (L"Failed to open service %s, last error %d", service_name,
              last_error));
      REPORT(last_error == ERROR_SERVICE_DOES_NOT_EXIST ||
             last_error == ERROR_INVALID_NAME, R_ERROR,
             (L"Failed to open service %s, last error %d", service_name,
              last_error), 5576234);
    }
  }

  return S_OK;
}

CString ServiceInstall::GenerateServiceName(const TCHAR* service_prefix) {
  FILETIME ft = {0};
  ::GetSystemTimeAsFileTime(&ft);
  CString versioned_service_name;
  versioned_service_name.Format(_T("%s%x%x"),
                                service_prefix,
                                ft.dwHighDateTime,
                                ft.dwLowDateTime);

  ASSERT1(!versioned_service_name.IsEmpty());
  return versioned_service_name;
}

HRESULT ServiceInstall::UninstallServices(const TCHAR* service_prefix,
                                          const TCHAR* exclude_service) {
  SERVICE_LOG(L2, (L"ServiceInstall::UninstallServices"));

  UninstallByPrefixParams params = {
    service_prefix,
    exclude_service,
  };

  return ScmDatabase::EnumerateServices(UninstallByPrefix, &params);
}

bool ServiceInstall::CanInstallWithoutReboot() {
  scoped_service scm(::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS));
  if (!scm) {
    ASSERT1(false);
    REPORT(false, R_ERROR, (L"Failed to open SCM: %d", ::GetLastError()),
           77224449);
    return false;  // request reboot just in case
  }

  scoped_service service(::OpenService(get(scm),
                                       _T("gupdate"),
                                       SERVICE_QUERY_CONFIG |
                                       SERVICE_CHANGE_CONFIG));
  if (!service) {
    DWORD last_error = ::GetLastError();
    if (last_error == ERROR_ACCESS_DENIED ||
        last_error == ERROR_INVALID_HANDLE) {
      // unable to verify the service is fully deleted, so request reboot
      ASSERT(false, (L"Expected access and correct handle"));
      return false;
    } else {
      // service does not exist
      return true;
    }
  }

  return !ScmDatabase::IsServiceMarkedDeleted(get(service));
}

HRESULT ServiceInstall::StopService(const CString& service_name) {
  SERVICE_LOG(L1, (_T("[ServiceInstall::StopService]")));

  scoped_service scm(::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS));
  if (!scm) {
    return HRESULTFromLastError();
  }
  scoped_service service(::OpenService(get(scm),
                                       service_name,
                                       SERVICE_QUERY_STATUS | SERVICE_STOP));
  if (!service) {
    return HRESULTFromLastError();
  }

  SERVICE_STATUS status = {0};
  if (::QueryServiceStatus(get(service), &status)) {
    if (status.dwCurrentState != SERVICE_STOPPED &&
        status.dwCurrentState != SERVICE_STOP_PENDING) {
      // Stop the service.
      SetZero(status);
      if (!::ControlService(get(service), SERVICE_CONTROL_STOP, &status)) {
        return HRESULTFromLastError();
      }
    }
  }

  if (status.dwCurrentState != SERVICE_STOPPED) {
    SERVICE_LOG(L1, (_T("[Service is stopping...]")));

    const int kWaitForServiceToStopMs = 8000;
    LowResTimer t(true);

    while (status.dwCurrentState != SERVICE_STOPPED &&
           t.GetMilliseconds() < kWaitForServiceToStopMs) {
      const int kSleepTimeMs = 50;
      ::Sleep(kSleepTimeMs);
      SetZero(status);
      VERIFY1(::QueryServiceStatus(get(service), &status));
      SERVICE_LOG(L1, (_T("[Waiting for service to stop %d]"),
          static_cast<int>(t.GetMilliseconds())));
    }

    if (status.dwCurrentState != SERVICE_STOPPED) {
      SERVICE_LOG(LEVEL_WARNING, (_T("[Service did not stop! Not good...]")));
      return HRESULT_FROM_WIN32(ERROR_TIMEOUT);
    }
  }

  ASSERT1(status.dwCurrentState == SERVICE_STOPPED);
  SERVICE_LOG(L1, (_T("[ServiceInstall::StopService - service stopped]")));
  return S_OK;
}

bool ServiceInstall::IsServiceInstalled(const TCHAR* service_name) {
  ASSERT1(service_name);
  scoped_service scm(::OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT));
  if (!scm) {
    return false;
  }
  scoped_service service(::OpenService(get(scm),
                                       service_name,
                                       SERVICE_QUERY_CONFIG));
  return valid(service);
}

// TODO(Omaha): Move all functions under a common ServiceUtils namespace.
bool ServiceUtils::IsServiceRunning(const TCHAR* service_name) {
  ASSERT1(service_name);

  scoped_service scm(::OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT));
  if (!scm) {
    SERVICE_LOG(LE, (_T("[OpenSCManager fail][0x%x]"), HRESULTFromLastError()));
    return false;
  }

  scoped_service service(::OpenService(get(scm),
                                       service_name,
                                       SERVICE_QUERY_STATUS));
  if (!service) {
    SERVICE_LOG(LE, (_T("[OpenService failed][%s][0x%x]"),
                     service_name, HRESULTFromLastError()));
    return false;
  }

  SERVICE_STATUS status = {0};
  if (!::QueryServiceStatus(get(service), &status)) {
    SERVICE_LOG(LE, (_T("[QueryServiceStatus failed][%s][0x%x]"),
                     service_name, HRESULTFromLastError()));
    return false;
  }

  return status.dwCurrentState == SERVICE_RUNNING ||
         status.dwCurrentState == SERVICE_START_PENDING;
}

bool ServiceUtils::IsServiceDisabled(const TCHAR* service_name) {
  ASSERT1(service_name);

  scoped_service scm(::OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT));
  if (!scm) {
    SERVICE_LOG(LE, (_T("[OpenSCManager fail][0x%x]"), HRESULTFromLastError()));
    return false;
  }

  scoped_service service(::OpenService(get(scm),
                                       service_name,
                                       SERVICE_QUERY_CONFIG));
  if (!service) {
    SERVICE_LOG(LE, (_T("[OpenService failed][%s][0x%x]"),
                     service_name, HRESULTFromLastError()));
    return false;
  }

  return ScmDatabase::IsServiceStateEqual(get(service), SERVICE_DISABLED);
}

}  // namespace omaha

