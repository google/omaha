// Copyright 2005-2009 Google Inc.
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
// The configuration manager that is used to provide the locations of the
// directory and the registration entries that are to be used by goopdate.

// TODO(omaha): consider removing some of the functions below and have a
// parameter is_machine instead. This is consistent with the rest of the code
// and it reduces the number of functions in the public interface.

#ifndef OMAHA_GOOPDATE_CONFIG_MANAGER_H__
#define OMAHA_GOOPDATE_CONFIG_MANAGER_H__

#include <windows.h>
#include <atlstr.h>
#include "base/basictypes.h"
#include "omaha/common/constants.h"
#include "omaha/common/synchronized.h"

namespace omaha {

class ConfigManager {
 public:
  const TCHAR* user_registry_clients() const { return USER_REG_CLIENTS; }
  const TCHAR* user_registry_clients_goopdate() const {
    return USER_REG_CLIENTS_GOOPDATE;
  }
  const TCHAR* user_registry_client_state() const {
    return USER_REG_CLIENT_STATE;
  }
  const TCHAR* user_registry_client_state_goopdate() const {
    return USER_REG_CLIENT_STATE_GOOPDATE;
  }
  const TCHAR* user_registry_update() const { return USER_REG_UPDATE; }
  const TCHAR* user_registry_google() const { return USER_REG_GOOGLE; }

  const TCHAR* machine_registry_clients() const { return MACHINE_REG_CLIENTS; }
  const TCHAR* machine_registry_clients_goopdate() const {
    return MACHINE_REG_CLIENTS_GOOPDATE;
  }
  const TCHAR* machine_registry_client_state() const {
    return MACHINE_REG_CLIENT_STATE;
  }
  const TCHAR* machine_registry_client_state_goopdate() const {
    return MACHINE_REG_CLIENT_STATE_GOOPDATE;
  }
  const TCHAR* machine_registry_client_state_medium() const {
    return MACHINE_REG_CLIENT_STATE_MEDIUM;
  }
  const TCHAR* machine_registry_update() const { return MACHINE_REG_UPDATE; }
  const TCHAR* machine_registry_google() const { return MACHINE_REG_GOOGLE; }

  const TCHAR* registry_clients(bool is_machine) const {
    return is_machine ? machine_registry_clients() : user_registry_clients();
  }
  const TCHAR* registry_clients_goopdate(bool is_machine) const {
    return is_machine ? machine_registry_clients_goopdate() :
                        user_registry_clients_goopdate();
  }
  const TCHAR* registry_client_state(bool is_machine) const {
    return is_machine ? machine_registry_client_state() :
                        user_registry_client_state();
  }
  const TCHAR* registry_client_state_goopdate(bool is_machine) const {
    return is_machine ? machine_registry_client_state_goopdate() :
                        user_registry_client_state_goopdate();
  }
  const TCHAR* registry_update(bool is_machine) const {
    return is_machine ? machine_registry_update() : user_registry_update();
  }
  const TCHAR* registry_google(bool is_machine) const {
    return is_machine ? machine_registry_google() : user_registry_google();
  }

  // Creates download data dir:
  // %UserProfile%/Application Data/Google/Update/Downloads
  CString GetDownloadStorage() const;

  // Creates download data dir:
  // %UserProfile%/Application Data/Google/Update/Downloads
  CString GetUserDownloadStorageDir() const;

  // Creates offline data dir:
  // %UserProfile%/Application Data/Google/Update/Offline
  CString GetUserOfflineStorageDir() const;

  // Creates initial manifest download dir:
  // %UserProfile%/Application Data/Google/Update/Manifests/Initial
  CString GetUserInitialManifestStorageDir() const;

  // Creates goopdate install dir:
  // %UserProfile%/Application Data/Google/Update
  CString GetUserGoopdateInstallDir() const;

  // Checks if the running program is executing from the User Goopdate dir.
  bool IsRunningFromUserGoopdateInstallDir() const;

  // Creates crash reports dir:
  // %UserProfile%/Local Settings/Application Data/Google/CrashReports
  CString GetUserCrashReportsDir() const;

  // Creates crash reports dir: %ProgramFiles%/Google/CrashReports
  CString GetMachineCrashReportsDir() const;

  // TODO(omaha): this is legacy Omaha1. Remove later.
  // Creates machine download data dir:
  // %All Users%/Google/Update/Downloads
  // This is the directory where all the machine downloads are initially
  // downloaded to. This is needed as the download could have occured as
  // a user who does not have permission to the machine download location.
  CString GetMachineDownloadStorageDir() const;

  // Creates machine download data dir:
  // %ProgramFiles%/Google/Update/Downloads
  // This is the directory where the installs for machine goopdate are copied
  // to once the download has succeeded.
  CString GetMachineSecureDownloadStorageDir() const;

  // Creates machine offline data dir:
  // %ProgramFiles%/Google/Update/Offline
  CString GetMachineSecureOfflineStorageDir() const;

  // Creates machine Gogole Update install dir:
  // %ProgramFiles%/Google/Update
  CString GetMachineGoopdateInstallDir() const;

  // Checks if the running program is executing from the User Goopdate dir.
  bool IsRunningFromMachineGoopdateInstallDir() const;

  // Returns the service endpoint where the install/update/uninstall pings
  // are being sent.
  HRESULT GetPingUrl(CString* url) const;

  // Returns the service endpoint where the manifest requests and update
  // checks are being sent.
  HRESULT GetUpdateCheckUrl(CString* url) const;

  // Returns the service endpoint where the webplugin parameters
  // are being sent for validation.
  HRESULT GetWebPluginCheckUrl(CString* url) const;

  // Returns the time interval between update checks in seconds.
  int GetLastCheckPeriodSec(bool* is_overridden) const;

  // Returns the number of seconds since the last successful update check.
  int GetTimeSinceLastCheckedSec(bool is_machine) const;

  // Gets and sets the last time a successful server update check was made.
  DWORD GetLastCheckedTime(bool is_machine) const;
  HRESULT SetLastCheckedTime(bool is_machine, DWORD time) const;

  // Checks registry to see if user has enabled us to collect anonymous
  // usage stats.
  bool CanCollectStats(bool is_machine) const;

  // Returns true if over-installing with the same version is allowed.
  bool CanOverInstall() const;

  // Returns the Autoupdate timer interval. This is the frequency of the
  // auto update timer run by the core.
  int GetAutoUpdateTimerIntervalMs() const;

  // Returns the wait time in ms to start the first worker.
  int GetUpdateWorkerStartUpDelayMs() const;

  // Returns the Code Red timer interval. This is the frequency of the
  // code red timer run by the core.
  int GetCodeRedTimerIntervalMs() const;

  // Returns true if event logging to the Windows Event Log is enabled.
  bool CanLogEvents(WORD event_type) const;

  // Retrieves TestSource which is to be set on dev, qa, and prober machines.
  CString GetTestSource() const;

  // Returns true if it is okay to do update checks and send pings.
  bool CanUseNetwork(bool is_machine) const;

  // Returns true if running in the context of an OEM install.
  // !CanUseNetwork() may be more appropriate.
  bool IsOemInstalling(bool is_machine) const;

  // Returns true if running in Windows Audit mode (OEM install).
  // USE IsOemInstalling() INSTEAD in most cases.
  bool IsWindowsInstalling() const;

  // Returns true if the user is considered a Googler.
  bool IsGoogler() const;

  // Returns true if installation of the specified app is allowed.
  bool CanInstallApp(const GUID& app_guid) const;

  // Returns true if updates are allowed for the specified app.
  bool CanUpdateApp(const GUID& app_guid, bool is_manual) const;

  // Gets the current name, say "GoogleUpdateTaskMachineCore", of the
  // GoogleUpdateCore scheduled task, either from the registry, or a default
  // value if there is no registration.
  static CString GetCurrentTaskNameCore(bool is_machine);

  // Creates a unique name, say "GoogleUpdateTaskMachineCore1c9b3d6baf90df3", of
  // the GoogleUpdateCore scheduled task, and stores it in the registry.
  // Subsequent invocations of GetCurrentTaskNameCore() will return this new
  // value.
  static HRESULT CreateAndSetVersionedTaskNameCoreInRegistry(bool machine);

  // Gets the current name, say "GoogleUpdateTaskMachineUA", of the
  // GoogleUpdateUA scheduled task, either from the registry, or a default value
  // if there is no registration.
  static CString GetCurrentTaskNameUA(bool is_machine);

  // Creates a unique name, say "GoogleUpdateTaskMachineUA1c9b3d6baf90df3", of
  // the GoogleUpdateUA scheduled task, and stores it in the registry.
  // Subsequent invocations of GetCurrentTaskNameUA() will return this new
  // value.
  static HRESULT CreateAndSetVersionedTaskNameUAInRegistry(bool machine);

  // Gets the current name, say "gupdate", of the goopdate system service,
  // either from the registry, or a default value if there is no registration.
  static CString GetCurrentServiceName();

  // Gets the current name and description of goopdate service.
  static CString GetCurrentServiceDisplayName();

  // Creates a unique versioned string and sets the version of goopdate service
  // in the registry. Subsequent invocations of GetCurrentServiceName() will
  // return this new value.
  static HRESULT CreateAndSetVersionedServiceNameInRegistry();

  // Returns the network configuration override as a string.
  static HRESULT GetNetConfig(CString* configuration_override);

  static ConfigManager* Instance();
  static void DeleteInstance();

 private:
  ConfigManager() {}

  static LLock lock_;
  static ConfigManager* config_manager_;

  DISALLOW_EVIL_CONSTRUCTORS(ConfigManager);
};

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_CONFIG_MANAGER_H__

