// Copyright 2007-2010 Google Inc.
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
// Sets up and controls the Google Update service.

// TODO(omaha3): Consolidate all the service related code into one file in base
// and one file in service.

#ifndef OMAHA_SETUP_SETUP_SERVICE_H_
#define OMAHA_SETUP_SETUP_SERVICE_H_

#include <windows.h>
#include "base/basictypes.h"
#include "omaha/base/constants.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/path.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/service_utils.h"
#include "omaha/base/system_info.h"
#include "omaha/client/resource.h"
#include "omaha/common/command_line_builder.h"
#include "omaha/common/const_cmd_line.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/service/service_main.h"
#include "omaha/third_party/smartany/scoped_any.h"

namespace omaha {

const uint32 kMaxQueryConfigBufferBytes = 8 * 1024;

template <typename T>
class SetupService {
 public:
  static HRESULT StartService() {
    OPT_LOG(L1, (_T("[StartService]")));

    scoped_service scm(::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS));
    if (!scm) {
      return HRESULTFromLastError();
    }

    CString service_name(T::GetCurrentServiceName());
    scoped_service service(::OpenService(get(scm),
                                         service_name,
                                         SERVICE_QUERY_STATUS | SERVICE_START));
    if (!service) {
      return HRESULTFromLastError();
    }

    SERVICE_STATUS status = {0};
    if (::QueryServiceStatus(get(service), &status)) {
      if (status.dwCurrentState == SERVICE_RUNNING ||
          status.dwCurrentState == SERVICE_START_PENDING) {
        SETUP_LOG(L1, (_T("[QueryServiceStatus][Service already running][%u]"),
                       status.dwCurrentState));
        return S_OK;
      }
    }

    // Start the service.
    if (::StartService(get(service), 0, NULL)) {
      SETUP_LOG(L1, (_T("[StartService][started]")));
      return S_OK;
    }

    HRESULT hr = HRESULTFromLastError();
    ASSERT1(hr != HRESULT_FROM_WIN32(ERROR_SERVICE_ALREADY_RUNNING));
    if (hr == HRESULT_FROM_WIN32(ERROR_SERVICE_ALREADY_RUNNING)) {
      SETUP_LOG(L1, (_T("[StartService][ERROR_SERVICE_ALREADY_RUNNING]")));
      return S_OK;
    }

    return hr;
  }

  static HRESULT StopService() {
    SETUP_LOG(L1, (_T("[StopService]")));

    CString service_name(T::GetCurrentServiceName());
    return ServiceInstall::StopService(service_name);
  }

  static HRESULT InstallService(const TCHAR* file_path) {
    ASSERT1(file_path);

    // Append the arguments to be passed to the service entry point.
    CString file_path_service(file_path);
    EnclosePath(&file_path_service);
    CString service_cmd_line(file_path_service);

    CommandLineBuilder builder(T::commandline_mode());
    SafeCStringAppendFormat(&service_cmd_line, _T(" %s"),
                            builder.GetCommandLineArgs());

    SETUP_LOG(L2, (_T("[service command line][%s]"), service_cmd_line));

    HRESULT hr = DoInstallService(service_cmd_line);
    if (FAILED(hr)) {
      SETUP_LOG(LE, (_T("[DoInstallService failed][0x%08x]"), hr));
      return hr;
    }

    VERIFY_SUCCEEDED(SetDescription(GetServiceDescription()));
    VERIFY_SUCCEEDED(SetDelayedAutoStart());

    return InstallCOMService();
  }

  // Uninstalls the service by:
  // 1. unregistering it
  // 2. deleting it from SCM if needed.
  static HRESULT UninstallService() {
    SETUP_LOG(L3, (_T("[UninstallService][%s]"), T::GetCurrentServiceName()));

    HRESULT hr = StopService();
    if (FAILED(hr)) {
      SETUP_LOG(LW, (_T("[StopService failed][0x%08x]"), hr));
      ASSERT1(HRESULT_FROM_WIN32(ERROR_SERVICE_DOES_NOT_EXIST) == hr);
    }

    VERIFY_SUCCEEDED(UninstallCOMService());

    hr = DeleteService();
    if (FAILED(hr)) {
      OPT_LOG(LEVEL_ERROR, (_T("[Can't delete the service][0x%08x]"), hr));
      return hr;
    }

    return S_OK;
  }

  static bool IsServiceInstalled() {
    return ServiceInstall::IsServiceInstalled(T::GetCurrentServiceName());
  }

 private:
  static HRESULT InstallCOMService() {
    SETUP_LOG(L1, (_T("[InstallCOMService]")));

    HRESULT hr = ServiceModule<T>().RegisterCOMService();

    // We reset the _pAtlModule to allow for the case where multiple instances
    // of ServiceModule are installed serially.
    _pAtlModule = NULL;

    return hr;
  }

  static HRESULT UninstallCOMService() {
    SETUP_LOG(L1, (_T("[UninstallCOMService]")));

    HRESULT hr = ServiceModule<T>().UnregisterCOMService();

    // We reset the _pAtlModule to allow for the case where multiple instances
    // of ServiceModule are uninstalled serially.
    _pAtlModule = NULL;

    return hr;
  }

  static HRESULT DoInstallService(const TCHAR* service_cmd_line) {
    SETUP_LOG(L1, (_T("[DoInstallService][%s]"), service_cmd_line));

    ASSERT1(service_cmd_line);

    if (IsServiceInstalled()) {
      // Lightweight upgrade of existing service.
      HRESULT hr = UpgradeService(service_cmd_line);
      ASSERT(SUCCEEDED(hr), (_T("[UpgradeService failed][0x%x]"), hr));
      if (SUCCEEDED(hr)) {
        return hr;
      }

      // Delete the previous version of the service. Then create a new service
      // name, and fall through to install that.
      VERIFY_SUCCEEDED(DeleteService());
      VERIFY_SUCCEEDED(CreateAndSetVersionedServiceNameInRegistry());
      ASSERT1(!IsServiceInstalled());
    }

    return DoInstallNewService(service_cmd_line);
  }

  static HRESULT DoInstallNewService(const TCHAR* service_cmd_line) {
    ASSERT1(service_cmd_line);

    scoped_service scm(::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS));
    if (!scm) {
      const DWORD error = ::GetLastError();
      SETUP_LOG(LE, (_T("[Failed to open SC Manager][%u]"), error));
      return HRESULT_FROM_WIN32(error);
    }

    CString service_name(T::GetCurrentServiceName());
    CString service_display_name(GetCurrentServiceDisplayName());
    scoped_service service(::CreateService(get(scm),
                                           service_name,
                                           service_display_name,
                                           SERVICE_ALL_ACCESS,
                                           SERVICE_WIN32_OWN_PROCESS,
                                           T::service_start_type(),
                                           SERVICE_ERROR_NORMAL,
                                           service_cmd_line,
                                           NULL,
                                           NULL,
                                           _T("RPCSS\0"),
                                           NULL,
                                           NULL));
    if (!service) {
      const DWORD error = ::GetLastError();
      SETUP_LOG(LE, (_T("[CreateService failed][%u]"), error));
      return HRESULT_FROM_WIN32(error);
    }
    SETUP_LOG(L1, (_T("[DoInstallNewService][service installed]")));
    return S_OK;
  }

  static bool IsServiceCorrectlyConfigured(const TCHAR* service_cmd_line) {
    scoped_service scm(::OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT));
    if (!scm) {
      HRESULT hr = HRESULTFromLastError();
      SETUP_LOG(LE, (_T("[Failed to open SC Manager][0x%x]"), hr));
      return false;
    }

    CString service_name(T::GetCurrentServiceName());
    scoped_service service(::OpenService(get(scm),
                                         service_name,
                                         SERVICE_QUERY_CONFIG));
    if (!service) {
      HRESULT hr = HRESULTFromLastError();
      SETUP_LOG(LE, (_T("[OpenService SERVICE_QUERY_CONFIG fail][0x%x]"), hr));
      return false;
    }

    // ::QueryServiceConfig expects a buffer of at most 8K bytes, according to
    // documentation. While the size of the buffer can be dynamically computed,
    // we just assume the maximum size for simplicity.
    uint8 buffer[kMaxQueryConfigBufferBytes] = { 0 };
    DWORD bytes_needed_ignored = 0;
    QUERY_SERVICE_CONFIG* service_config =
      reinterpret_cast<QUERY_SERVICE_CONFIG*>(buffer);
    if (!::QueryServiceConfig(get(service),
                              service_config,
                              sizeof(buffer),
                              &bytes_needed_ignored)) {
      HRESULT hr = HRESULTFromLastError();
      SETUP_LOG(LE, (_T("[QueryServiceConfig failed][0x%x]"), hr));
      return false;
    }

    bool does_service_cmd_line_match = false;
    if (service_config->lpBinaryPathName == NULL || service_cmd_line == NULL) {
      does_service_cmd_line_match =
          (service_config->lpBinaryPathName == service_cmd_line);
    } else {
      does_service_cmd_line_match =
          !_tcsicmp(service_config->lpBinaryPathName, service_cmd_line);
    }
    return service_config->dwServiceType == SERVICE_WIN32_OWN_PROCESS &&
           service_config->dwStartType == T::service_start_type() &&
           service_config->dwErrorControl == SERVICE_ERROR_NORMAL &&
           does_service_cmd_line_match;
  }

  static HRESULT UpgradeService(const TCHAR* service_cmd_line) {
    ASSERT1(service_cmd_line);
    ASSERT1(IsServiceInstalled());

    if (IsServiceCorrectlyConfigured(service_cmd_line)) {
      return S_OK;
    }

    // Modify the configuration of the existing service.
    HRESULT hr(StopService());
    if (FAILED(hr)) {
      SETUP_LOG(LE, (_T("[Can't stop the service][0x%08x]"), hr));
      return hr;
    }

    scoped_service scm(::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS));
    if (!scm) {
      const DWORD error = ::GetLastError();
      SETUP_LOG(LE, (_T("[Failed to open SC Manager][%u]"), error));
      return HRESULT_FROM_WIN32(error);
    }

    CString service_name(T::GetCurrentServiceName());
    scoped_service service(::OpenService(get(scm),
                                         service_name,
                                         SERVICE_CHANGE_CONFIG));
    if (!service) {
      const DWORD error = ::GetLastError();
      SETUP_LOG(LE, (_T("[Failed to open service for update][%u]"), error));
      return HRESULT_FROM_WIN32(error);
    }

    if (!::ChangeServiceConfig(get(service),
                               SERVICE_WIN32_OWN_PROCESS,
                               T::service_start_type(),
                               SERVICE_ERROR_NORMAL,
                               service_cmd_line,
                               NULL,
                               NULL,
                               NULL,
                               NULL,
                               NULL,
                               NULL)) {
      const DWORD error = ::GetLastError();
      SETUP_LOG(LE, (_T("[Failed to change service config][%u]"), error));
      return HRESULT_FROM_WIN32(error);
    }

    SETUP_LOG(L3, (_T("[ChangeServiceConfig succeeded]")));
    return S_OK;
  }

  static bool IsDelayedAutoStart() {
    scoped_service scm(::OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT));
    if (!scm) {
      HRESULT hr = HRESULTFromLastError();
      SETUP_LOG(LE, (_T("[SC_MANAGER_CONNECT failed][0x%x]"), hr));
      return false;
    }

    CString name(T::GetCurrentServiceName());
    scoped_service service(::OpenService(get(scm), name, SERVICE_QUERY_CONFIG));
    if (!service) {
      HRESULT hr = HRESULTFromLastError();
      SETUP_LOG(LE, (_T("[SERVICE_QUERY_CONFIG failed][%s][0x%x]"), name, hr));
      return false;
    }

    uint8 buffer[kMaxQueryConfigBufferBytes] = { 0 };
    DWORD bytes_needed_ignored = 0;
    SERVICE_DELAYED_AUTO_START_INFO* service_config =
      reinterpret_cast<SERVICE_DELAYED_AUTO_START_INFO*>(buffer);
    if (!::QueryServiceConfig2(get(service),
                               SERVICE_CONFIG_DELAYED_AUTO_START_INFO,
                               buffer,
                               sizeof(buffer),
                               &bytes_needed_ignored)) {
      HRESULT hr = HRESULTFromLastError();
      SETUP_LOG(LE, (_T("[Query SERVICE_CONFIG_DELAYED_AUTO_START_INFO failed]")
                     _T("[0x%x]"), hr));
      return false;
    }

    return !!service_config->fDelayedAutostart;
  }

  static HRESULT SetDelayedAutoStart() {
    if (!SystemInfo::IsRunningOnVistaOrLater()) {
      return S_OK;
    }

    ASSERT1(IsServiceInstalled());

    if (IsDelayedAutoStart()) {
      return S_OK;
    }

    scoped_service scm(::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS));
    if (!scm) {
      HRESULT hr = HRESULTFromLastError();
      SETUP_LOG(LE, (_T("[OpenSCManager failed][0x%x]"), hr));
      return hr;
    }

    CString service_name(T::GetCurrentServiceName());
    scoped_service service(::OpenService(get(scm),
                                         service_name,
                                         SERVICE_CHANGE_CONFIG));
    if (!service) {
      HRESULT hr = HRESULTFromLastError();
      SETUP_LOG(LE, (_T("[OpenService failed][0x%x]"), hr));
      return hr;
    }

    SERVICE_DELAYED_AUTO_START_INFO auto_start_info = {TRUE};
    if (!::ChangeServiceConfig2(get(service),
                                SERVICE_CONFIG_DELAYED_AUTO_START_INFO,
                                &auto_start_info)) {
      HRESULT hr = HRESULTFromLastError();
      SETUP_LOG(LE, (_T("[ChangeServiceConfig2 failed][0x%x]"), hr));
      return hr;
    }

    SETUP_LOG(L3, (_T("[SetDelayedAutoStart succeeded]")));
    return S_OK;
  }

  static HRESULT DeleteService() {
    SETUP_LOG(L3, (_T("[DeleteService][%s]"), T::GetCurrentServiceName()));

    scoped_service scm(::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS));
    if (!scm) {
      return HRESULTFromLastError();
    }
    scoped_service service(::OpenService(get(scm),
                                         T::GetCurrentServiceName(),
                                         SERVICE_CHANGE_CONFIG | DELETE));
    if (!service) {
      return HRESULTFromLastError();
    }

    if (!::DeleteService(get(service))) {
      HRESULT hr = HRESULTFromLastError();
      SETUP_LOG(LW, (_T("[DeleteService failed][0x%x]"), hr));
      if (hr != HRESULT_FROM_WIN32(ERROR_SERVICE_MARKED_FOR_DELETE)) {
        return hr;
      }
    }

    return S_OK;
  }

  static bool DoesDescriptionMatch(const TCHAR* description) {
    ASSERT1(description);

    scoped_service scm(::OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT));
    if (!scm) {
      HRESULT hr = HRESULTFromLastError();
      SETUP_LOG(LE, (_T("[SC_MANAGER_CONNECT failed][0x%x]"), hr));
      return false;
    }

    CString name(T::GetCurrentServiceName());
    scoped_service service(::OpenService(get(scm), name, SERVICE_QUERY_CONFIG));
    if (!service) {
      HRESULT hr = HRESULTFromLastError();
      SETUP_LOG(LE, (_T("[SERVICE_QUERY_CONFIG failed][%s][0x%x]"), name, hr));
      return false;
    }

    uint8 buffer[kMaxQueryConfigBufferBytes] = { 0 };
    DWORD bytes_needed_ignored = 0;
    SERVICE_DESCRIPTION* service_config =
      reinterpret_cast<SERVICE_DESCRIPTION*>(buffer);
    if (!::QueryServiceConfig2(get(service),
                               SERVICE_CONFIG_DESCRIPTION,
                               buffer,
                               sizeof(buffer),
                               &bytes_needed_ignored)) {
      HRESULT hr = HRESULTFromLastError();
      SETUP_LOG(LE, (_T("[QuerySERVICE_CONFIG_DESCRIPTION failed][0x%x]"), hr));
      return false;
    }

    if (service_config->lpDescription == NULL || description == NULL) {
      return (service_config->lpDescription == description);
    }

    return !_tcsicmp(service_config->lpDescription, description);
  }

  static HRESULT SetDescription(const TCHAR* description) {
    ASSERT1(description);

    if (DoesDescriptionMatch(description)) {
      return S_OK;
    }

    scoped_service scm(::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS));
    if (!scm) {
      return HRESULTFromLastError();
    }

    CString name(T::GetCurrentServiceName());

    // Opening the service with less rights fails the ChangeServiceConfig2 call
    // with E_ACCESSDENIED.
    scoped_service service(::OpenService(get(scm), name, SERVICE_ALL_ACCESS));
    if (!service) {
      return HRESULTFromLastError();
    }
    SERVICE_DESCRIPTION info = { const_cast<TCHAR*>(description) };
    if (!::ChangeServiceConfig2(get(service),
                                SERVICE_CONFIG_DESCRIPTION,
                                &info)) {
      return HRESULTFromLastError();
    }
    SETUP_LOG(L3, (_T("[service description changed successfully]")));
    return S_OK;
  }

  static CString GetCurrentServiceDisplayName() {
    CORE_LOG(L3, (_T("[GetCurrentServiceDisplayName]")));

    CString product_name;
    VERIFY1(product_name.LoadString(IDS_PRODUCT_DISPLAY_NAME));

    CString display_name;
    display_name.FormatMessage(IDS_SERVICE_DISPLAY_NAME, product_name);
    SafeCStringAppendFormat(&display_name, _T(" (%s)"),
                            T::GetCurrentServiceName());
    return display_name;
  }

  static CString GetServiceDescription() {
    // TODO(omaha3): Do we need a different service description for the medium
    // service?

    CString company_name;
    VERIFY1(company_name.LoadString(IDS_FRIENDLY_COMPANY_NAME));

    CString service_description;
    service_description.FormatMessage(IDS_SERVICE_DESCRIPTION, company_name);
    return service_description;
  }

  static HRESULT CreateAndSetVersionedServiceNameInRegistry() {
    CORE_LOG(L3, (_T("CreateAndSetVersionedServiceNameInRegistry")));
    return goopdate_utils::CreateAndSetVersionedNameInRegistry(true,
        T::default_name(), T::reg_name());
  }

  friend class SetupServiceTest;
  friend class CoreUtilsTest;

  DISALLOW_IMPLICIT_CONSTRUCTORS(SetupService);
};

typedef SetupService<Update3ServiceMode> SetupUpdate3Service;
typedef SetupService<UpdateMediumServiceMode> SetupUpdateMediumService;

}  // namespace omaha

#endif  // OMAHA_SETUP_SETUP_SERVICE_H_
