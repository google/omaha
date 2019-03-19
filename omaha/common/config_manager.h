// Copyright 2005-2010 Google Inc.
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

#ifndef OMAHA_COMMON_CONFIG_MANAGER_H_
#define OMAHA_COMMON_CONFIG_MANAGER_H_

#include <windows.h>
#include <atlstr.h>
#include "base/basictypes.h"
#include "omaha/base/constants.h"
#include "omaha/base/synchronized.h"
#include "omaha/base/time.h"

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

  // Gets the temporary download dir for the current thread token:
  // %UserProfile%/AppData/Local/Temp
  CString GetTempDownloadDir() const;

  // Gets the total disk size limit for cached packages. When this limit is hit,
  // packages should be deleted from oldest until total size is below the limit.
  int GetPackageCacheSizeLimitMBytes() const;

  // Gets the package cache life limit. If a cached package is older than this
  // limit, it should be removed.
  int GetPackageCacheExpirationTimeDays() const;

  // Creates download data dir:
  // %UserProfile%/Application Data/Google/Update/Download
  // This is the root of the package cache for the user.
  // TODO(omaha): consider renaming.
  CString GetUserDownloadStorageDir() const;

  // Creates install data dir:
  // %UserProfile%/Application Data/Google/Update/Install
  // Files pending user installs are copied in this directory.
  CString GetUserInstallWorkingDir() const;

  // Creates offline data dir:
  // %UserProfile%/Application Data/Google/Update/Offline
  CString GetUserOfflineStorageDir() const;

  // Returns goopdate install dir:
  // %UserProfile%/Application Data/Google/Update
  CString GetUserGoopdateInstallDirNoCreate() const;

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

  // Creates machine download data dir:
  // %ProgramFiles%/Google/Update/Download
  // This directory is the root of the package cache for the machine.
  // TODO(omaha): consider renaming.
  CString GetMachineSecureDownloadStorageDir() const;

  // Creates install data dir:
  // %ProgramFiles%/Google/Update/Install
  // Files pending machine installs are copied in this directory.
  CString GetMachineInstallWorkingDir() const;

  // Creates machine offline data dir:
  // %ProgramFiles%/Google/Update/Offline
  CString GetMachineSecureOfflineStorageDir() const;

  // Creates machine Gogole Update install dir:
  // %ProgramFiles%/Google/Update
  CString GetMachineGoopdateInstallDirNoCreate() const;

  // Creates machine Gogole Update install dir:
  // %ProgramFiles%/Google/Update
  CString GetMachineGoopdateInstallDir() const;

  // Checks if the running program is executing from the User Goopdate dir.
  bool IsRunningFromMachineGoopdateInstallDir() const;

  // Returns the service endpoint where the install/update/uninstall pings
  // are being sent.
  HRESULT GetPingUrl(CString* url) const;

  // Returns the service endpoint where the update checks are sent.
  HRESULT GetUpdateCheckUrl(CString* url) const;

  // Returns the service endpoint where the crashes are sent.
  HRESULT GetCrashReportUrl(CString* url) const;

  // Returns the web page url where the 'Get Help' requests are sent.
  HRESULT GetMoreInfoUrl(CString* url) const;

  // Returns the service endpoint where the usage stats requests are sent.
  HRESULT GetUsageStatsReportUrl(CString* url) const;

  // Returns the time interval between update checks in seconds.
  // 0 indicates updates are disabled.
  int GetLastCheckPeriodSec(bool* is_overridden) const;

  // Returns the number of seconds since the last successful update check.
  int GetTimeSinceLastCheckedSec(bool is_machine) const;

  // Functions that deal with the X-Retry-After header from the server.
  DWORD GetRetryAfterTime(bool is_machine) const;
  HRESULT SetRetryAfterTime(bool is_machine, DWORD time) const;
  bool CanRetryNow(bool is_machine) const;

  // Gets and sets the last time a successful server update check was made.
  DWORD GetLastCheckedTime(bool is_machine) const;
  HRESULT SetLastCheckedTime(bool is_machine, DWORD time) const;

  // Sets the last time we successfully launched a /ua process.
  HRESULT SetLastStartedAU(bool is_machine) const;

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

  // Returns the wait time in ms before making an update check The range of
  // the returned value is [0, 60000) ms, even if the value is overriden
  // by UpdateDev settings.
  int GetAutoUpdateJitterMs() const;

  // Core interval between runs functions.
  time64 GetTimeSinceLastCoreRunMs(bool is_machine) const;
  time64 GetLastCoreRunTimeMs(bool is_machine) const;
  HRESULT SetLastCoreRunTimeMs(bool is_machine, time64 time);

  // Code Red check interval functions.
  int GetCodeRedTimerIntervalMs() const;
  time64 GetTimeSinceLastCodeRedCheckMs(bool is_machine) const;
  time64 GetLastCodeRedCheckTimeMs(bool is_machine) const;
  HRESULT SetLastCodeRedCheckTimeMs(bool is_machine, time64 time);

  // Returns true if event logging to the Windows Event Log is enabled.
  bool CanLogEvents(WORD event_type) const;

  // Retrieves TestSource which is to be set on dev, qa, and prober machines.
  CString GetTestSource() const;

  // Returns true if it is okay to do update checks and send pings.
  bool CanUseNetwork(bool is_machine) const;

  // Returns true if running in Windows Audit mode (OEM install).
  // USE OemInstall::IsOemInstalling() INSTEAD in most cases.
  bool IsWindowsInstalling() const;

  // Returns true if the user is considered an internal user.
  bool IsInternalUser() const;

  // Returns true if installation of the specified app is allowed.
  bool CanInstallApp(const GUID& app_guid) const;

  // Returns true if updates are allowed for the specified app. The 'is_manual'
  // parameter is needed for context, because the update policy can be one of
  // kPolicyDisabled, kPolicyManualUpdatesOnly, or kPolicyAutomaticUpdatesOnly.
  bool CanUpdateApp(const GUID& app_guid, bool is_manual) const;

  // Returns true if crash uploading is allowed all the time, no matter the
  // build flavor or other configuration parameters.
  bool AlwaysAllowCrashUploads() const;

  // Returns the number of crashes to upload per day.
  int MaxCrashUploadsPerDay() const;

  // Returns the value of the "DownloadPreference" group policy or an
  // empty string if the group policy does not exist, the policy is unknown, or
  // an error happened.
  CString GetDownloadPreferenceGroupPolicy() const;

  // Returns the network configuration override as a string.
  static HRESULT GetNetConfig(CString* configuration_override);

  // Gets the time when Goopdate was last updated or installed.
  static DWORD GetLastUpdateTime(bool is_machine);

  // Returns true if it has been more than 24 hours since Goopdate was updated
  // or installed.
  static bool Is24HoursSinceLastUpdate(bool is_machine);

  // Returns kPolicyEnabled if installation of the specified app is allowed.
  // Otherwise, returns kPolicyDisabled.
  static DWORD GetEffectivePolicyForAppInstalls(const GUID& app_guid);

  // Returns kPolicyEnabled if updates of the specified app is allowed.
  // Otherwise, returns one of kPolicyDisabled, kPolicyManualUpdatesOnly, or
  // kPolicyAutomaticUpdatesOnly.
  static DWORD GetEffectivePolicyForAppUpdates(const GUID& app_guid);

  // Returns the target version prefix for the app, if the machine is joined to
  // a domain and has the corresponding group policy set.
  // Examples:
  // * "" (or not configured): update to latest version available.
  // * "55.": update to any minor version of 55 (e.g. 55.24.34 or 55.60.2).
  // * "55.2.": update to any minor version of 55.2 (e.g. 55.2.34 or 55.2.2).
  // * "55.24.34": update to this specific version only.
  static CString GetTargetVersionPrefix(const GUID& app_guid);

  // Returns whether the RollbackToTargetVersion policy has been set for the
  // app. If RollbackToTargetVersion is set, the TargetVersionPrefix policy
  // governs the version to rollback clients with higher versions to.
  static bool IsRollbackToTargetVersionAllowed(const GUID& app_guid);

  // For domain-joined machines, checks the current time against the times that
  // updates are suppressed. Returns true if the current time falls between the
  // start time and the duration.
  // The duration does not account for daylight savings time. For instance, if
  // the start time is 22:00 hours, and with a duration of 8 hours, the updates
  // will be suppressed for 8 hours regardless of whether daylight savings time
  // changes happen in between.
  static bool AreUpdatesSuppressedNow();

  static ConfigManager* Instance();
  static void DeleteInstance();

 private:
  static LLock lock_;
  static ConfigManager* config_manager_;

  ConfigManager();

  bool is_running_from_official_user_dir_;
  bool is_running_from_official_machine_dir_;

  DISALLOW_EVIL_CONSTRUCTORS(ConfigManager);
};

}  // namespace omaha

#endif  // OMAHA_COMMON_CONFIG_MANAGER_H_
