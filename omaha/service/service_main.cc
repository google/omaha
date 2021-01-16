// Copyright 2009-2010 Google Inc.
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
// Contains the ATL service.

#include "omaha/service/service_main.h"

namespace omaha {

// Template arguments need to be non-const TCHAR arrays.
TCHAR kHKRootService[] = _T("HKLM");
TCHAR kProgIDUpdate3COMClassServiceLocal[] = kProgIDUpdate3COMClassService;

// A private object map with custom registration works best, even though this
// stuff is deprecated. This is because GoogleUpdate.exe has other objects
// defined elsewhere and we do not want to expose those from the service.
BEGIN_OBJECT_MAP(object_map_google_update3)
  OBJECT_ENTRY(__uuidof(GoogleUpdate3ServiceClass), Update3COMClassService)
END_OBJECT_MAP()

BEGIN_OBJECT_MAP(object_map_google_update_medium)
  OBJECT_ENTRY(__uuidof(OnDemandMachineAppsServiceClass), OnDemandService)
  OBJECT_ENTRY(__uuidof(GoogleUpdate3WebServiceClass), Update3WebService)
  OBJECT_ENTRY(__uuidof(PolicyStatusMachineServiceClass), PolicyStatusService)
  OBJECT_ENTRY(__uuidof(GoogleUpdateCoreClass), GoogleUpdateCoreService)
END_OBJECT_MAP()

CommandLineMode Update3ServiceMode::commandline_mode() {
  return COMMANDLINE_MODE_SERVICE;
}

CString Update3ServiceMode::reg_name() {
  return kRegValueServiceName;
}

CString Update3ServiceMode::default_name() {
  return kServicePrefix;
}

DWORD Update3ServiceMode::service_start_type() {
  return SERVICE_AUTO_START;
}

_ATL_OBJMAP_ENTRY* Update3ServiceMode::object_map() {
  return object_map_google_update3;
}

bool Update3ServiceMode::allow_access_from_medium() {
  return false;
}

CString Update3ServiceMode::app_id_string() {
  return GuidToString(__uuidof(GoogleUpdate3ServiceClass));
}

CString Update3ServiceMode::GetCurrentServiceName() {
  return goopdate_utils::GetCurrentVersionedName(true, reg_name(),
                                                 default_name());
}

HRESULT Update3ServiceMode::PreMessageLoop() {
  SERVICE_LOG(L1, (_T("[Starting Google Update core...]")));
  CommandLineBuilder builder(COMMANDLINE_MODE_CORE);
  CString args = builder.GetCommandLineArgs();
  return goopdate_utils::StartGoogleUpdateWithArgs(true,
                                                   StartMode::kBackground,
                                                   args,
                                                   NULL);
}

CommandLineMode UpdateMediumServiceMode::commandline_mode() {
  return COMMANDLINE_MODE_MEDIUM_SERVICE;
}

CString UpdateMediumServiceMode::reg_name() {
  return kRegValueMediumServiceName;
}

CString UpdateMediumServiceMode::default_name() {
  return kMediumServicePrefix;
}

DWORD UpdateMediumServiceMode::service_start_type() {
  return SERVICE_DEMAND_START;
}

_ATL_OBJMAP_ENTRY* UpdateMediumServiceMode::object_map() {
  return object_map_google_update_medium;
}

bool UpdateMediumServiceMode::allow_access_from_medium() {
  return true;
}

CString UpdateMediumServiceMode::app_id_string() {
  return GuidToString(__uuidof(OnDemandMachineAppsServiceClass));
}

CString UpdateMediumServiceMode::GetCurrentServiceName() {
  return goopdate_utils::GetCurrentVersionedName(true, reg_name(),
                                                 default_name());
}

HRESULT UpdateMediumServiceMode::PreMessageLoop() {
  return S_OK;
}

}  // namespace omaha
