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


#include "omaha/setup/setup_service.h"

#include "base/basictypes.h"
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/logging.h"
#include "omaha/common/path.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/service_utils.h"
#include "omaha/goopdate/command_line_builder.h"
#include "omaha/goopdate/config_manager.h"
#include "omaha/goopdate/const_goopdate.h"
#include "omaha/goopdate/goopdate_utils.h"
#include "omaha/service/service_main.h"

namespace omaha {

HRESULT SetupService::StartService() {
  OPT_LOG(L1, (_T("[SetupService::StartService]")));

  scoped_service scm(::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS));
  if (!scm) {
    return HRESULTFromLastError();
  }
  CString service_name(ConfigManager::GetCurrentServiceName());
  if (service_name.IsEmpty()) {
    return GOOPDATE_E_SERVICE_NAME_EMPTY;
  }
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
      SETUP_LOG(L1, (_T("[SetupService::StartService already running]")));
      return S_OK;
    }
  }

  // Start the service.
  if (!::StartService(get(service), 0, NULL)) {
    return HRESULTFromLastError();
  }

  SETUP_LOG(L1, (_T("[SetupService::StartService started]")));
  return S_OK;
}

HRESULT SetupService::StopService() {
  SETUP_LOG(L1, (_T("[SetupService::StopService]")));
  CString service_name(ConfigManager::GetCurrentServiceName());
  if (service_name.IsEmpty()) {
    return GOOPDATE_E_SERVICE_NAME_EMPTY;
  }

  return ServiceInstall::StopService(service_name);
}

HRESULT SetupService::InstallService(const TCHAR* file_path) {
  ASSERT1(file_path);

  // Quote the FilePath before creating/updating the service. Append the
  // arguments to be passed to the service entry point.
  CString file_path_service(file_path);
  EnclosePath(&file_path_service);
  CString service_cmd_line(file_path_service);
  CommandLineBuilder builder(COMMANDLINE_MODE_SERVICE);
  service_cmd_line.AppendFormat(_T(" %s"), builder.GetCommandLineArgs());
  SETUP_LOG(L2, (_T("[service command line ][%s]"), service_cmd_line));

  CString service_description;
  VERIFY1(service_description.LoadString(IDS_SERVICE_DESCRIPTION));

  HRESULT hr = DoInstallService(service_cmd_line, service_description);
  if (FAILED(hr)) {
    return hr;
  }

  return InstallCOMService();
}

HRESULT SetupService::InstallCOMService() {
  SETUP_LOG(L1, (_T("[SetupService::InstallCOMService]")));

  return ServiceModule().RegisterCOMService();
}

HRESULT SetupService::DoInstallService(const TCHAR* service_cmd_line,
                                       const TCHAR* description) {
  SETUP_LOG(L1, (_T("[SetupService::DoInstallService][%s]"), service_cmd_line));

  ASSERT1(service_cmd_line);
  ASSERT1(description);

  if (goopdate_utils::IsServiceInstalled()) {
    // Lightweight upgrade of existing service.
    HRESULT hr = UpgradeService(service_cmd_line);
    ASSERT1(SUCCEEDED(hr));
    if (SUCCEEDED(hr)) {
      VERIFY1(SUCCEEDED(SetDescription(ConfigManager::GetCurrentServiceName(),
                                       description)));
      return hr;
    }

    // Delete any previous versions of the service. Then create a new service
    // name, and fall through to install that.
    VERIFY1(SUCCEEDED(DeleteServices()));
    VERIFY1(SUCCEEDED(
        ConfigManager::CreateAndSetVersionedServiceNameInRegistry()));
    ASSERT1(!goopdate_utils::IsServiceInstalled());
  }

  return DoInstallNewService(ConfigManager::GetCurrentServiceName(),
                             ConfigManager::GetCurrentServiceDisplayName(),
                             service_cmd_line,
                             description);
}

HRESULT SetupService::DoInstallNewService(const TCHAR* service_name,
                                          const TCHAR* service_display_name,
                                          const TCHAR* service_cmd_line,
                                          const TCHAR* description) {
  ASSERT1(service_name);
  ASSERT1(service_display_name);
  ASSERT1(service_cmd_line);
  ASSERT1(description);
  if (!*service_name) {
    return GOOPDATE_E_SERVICE_NAME_EMPTY;
  }

  scoped_service scm(::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS));
  if (!scm) {
    ASSERT1(false);
    HRESULT hr = HRESULTFromLastError();
    SETUP_LOG(LEVEL_ERROR, (_T("Failed to open SC Manager 0x%08x"), hr));
    return hr;
  }

  scoped_service service(::CreateService(
                               get(scm),
                               service_name,
                               service_display_name,
                               SERVICE_ALL_ACCESS,
                               SERVICE_WIN32_OWN_PROCESS,
                               SERVICE_AUTO_START,
                               SERVICE_ERROR_NORMAL,
                               service_cmd_line,
                               NULL,
                               NULL,
                               _T("RPCSS\0"),
                               NULL,
                               NULL));
  if (!service) {
    HRESULT hr = HRESULTFromLastError();
    SETUP_LOG(LEVEL_ERROR, (_T("[CreateService failed][0x%08x]"), hr));
    return hr;
  }
  VERIFY1(SUCCEEDED(SetDescription(service_name, description)));
  SETUP_LOG(L1, (_T("[SetupService::InstallService - service installed]")));
  return S_OK;
}

// Uninstalls the service by:
// 1. unregistering it
// 2. deleting it from SCM if needed.
HRESULT SetupService::UninstallService() {
  SETUP_LOG(L3, (_T("[SetupService::UninstallService]")));

  HRESULT hr = StopService();
  if (FAILED(hr)) {
    SETUP_LOG(LW, (_T("[SetupService::StopService failed][0x%08x]"), hr));
    ASSERT1(HRESULT_FROM_WIN32(ERROR_SERVICE_DOES_NOT_EXIST) == hr);
  }

  VERIFY1(SUCCEEDED(ServiceModule().UnregisterCOMService()));

  hr = DeleteServices();
  if (FAILED(hr)) {
    OPT_LOG(LEVEL_ERROR, (_T("[Can't delete the service][0x%08x]"), hr));
    return hr;
  }

  return S_OK;
}

HRESULT SetupService::UpgradeService(const TCHAR* service_cmd_line) {
  ASSERT1(service_cmd_line);
  ASSERT1(goopdate_utils::IsServiceInstalled());
  if (!goopdate_utils::IsServiceInstalled()) {
    return E_UNEXPECTED;
  }

  // Modify the configuration of the existing service.
  HRESULT hr(StopService());
  if (FAILED(hr)) {
    ASSERT1(false);
    SETUP_LOG(LEVEL_ERROR, (_T("[Can't stop the service][0x%08x]"), hr));
    return hr;
  }

  scoped_service scm(::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS));
  if (!scm) {
    ASSERT1(false);
    SETUP_LOG(LEVEL_ERROR, (_T("Failed to open SC Manager 0x%08x"),
                           HRESULTFromLastError()));
    return HRESULTFromLastError();
  }

  CString service_name(ConfigManager::GetCurrentServiceName());
  if (service_name.IsEmpty()) {
    return GOOPDATE_E_SERVICE_NAME_EMPTY;
  }
  scoped_service service(::OpenService(get(scm),
                                       service_name,
                                       SERVICE_CHANGE_CONFIG));
  if (!service) {
    ASSERT1(false);
    SETUP_LOG(LEVEL_ERROR, (_T("Failed to open service for update 0x%08x"),
                           HRESULTFromLastError()));
    return HRESULTFromLastError();
  }

  if (!::ChangeServiceConfig(get(service),
                             SERVICE_WIN32_OWN_PROCESS,
                             SERVICE_AUTO_START,
                             SERVICE_ERROR_NORMAL,
                             service_cmd_line,
                             NULL,
                             NULL,
                             NULL,
                             NULL,
                             NULL,
                             NULL)) {
    ASSERT1(false);
    SETUP_LOG(LEVEL_ERROR, (_T("Failed to change service config 0x%08x"),
                           HRESULTFromLastError()));
    return HRESULTFromLastError();
  }

  SETUP_LOG(L3, (_T("[ChangeServiceConfig succeeded]")));
  return S_OK;
}

HRESULT SetupService::DeleteServices() {
  return ServiceInstall::UninstallServices(kServicePrefix, NULL);
}

HRESULT SetupService::SetDescription(const TCHAR* name,
                                     const TCHAR* description) {
  ASSERT1(name);
  ASSERT1(description);
  scoped_service scm(::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS));
  if (!scm) {
    return HRESULTFromLastError();
  }

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

}  // namespace omaha
