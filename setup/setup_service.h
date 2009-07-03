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
// Sets up and controls the Google Update service.

#ifndef OMAHA_SETUP_SETUP_SERVICE_H__
#define OMAHA_SETUP_SETUP_SERVICE_H__

#include <windows.h>
#include "base/basictypes.h"
#include "omaha/common/constants.h"

namespace omaha {

class SetupService {
 public:
  // Controls the service.
  static HRESULT StartService();
  static HRESULT StopService();

  // Installs and uninstalls the service.
  static HRESULT InstallService(const TCHAR* file_path);
  static HRESULT UninstallService();

 private:
  // COM registration for the GoogleUpdateCoreClass.
  static HRESULT InstallCOMService();

  static HRESULT DoInstallService(const TCHAR* service_cmd_line,
                                  const TCHAR* desc);

  static HRESULT DoInstallNewService(const TCHAR* service_name,
                                     const TCHAR* service_display_name,
                                     const TCHAR* service_cmd_line,
                                     const TCHAR* description);

  // Upgrades the service.
  static HRESULT UpgradeService(const TCHAR* service_cmd_line);

  // Deletes all services prefixed with "gupdate" from the SCM.
  static HRESULT DeleteServices();

  // Sets the service description. If the description is an empty string, the
  // current description is deleted.
  static HRESULT SetDescription(const TCHAR* name,
                                const TCHAR* description);

  friend class SetupServiceTest;
  friend class CoreUtilsTest;

  DISALLOW_IMPLICIT_CONSTRUCTORS(SetupService);
};

}  // namespace omaha

#endif  // OMAHA_SETUP_SETUP_SERVICE_H__

