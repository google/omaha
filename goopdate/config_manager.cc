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

#include "omaha/goopdate/config_manager.h"
#include <lm.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <wininet.h>
#include <atlstr.h>
#include <math.h>
#include "omaha/common/app_util.h"
#include "omaha/common/constants.h"
#include "omaha/common/const_addresses.h"
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/file.h"
#include "omaha/common/scope_guard.h"
#include "omaha/common/service_utils.h"
#include "omaha/common/string.h"
#include "omaha/common/time.h"
#include "omaha/common/utils.h"
#include "omaha/enterprise/const_group_policy.h"
#include "omaha/goopdate/const_goopdate.h"
#include "omaha/goopdate/goopdate_utils.h"
#include "omaha/goopdate/resource.h"

namespace omaha {

namespace {

HRESULT GetDir(int csidl,
               const CString& path_tail,
               bool create_dir,
               CString* dir) {
  ASSERT1(dir);

  CString path;
  HRESULT hr = GetFolderPath(csidl | CSIDL_FLAG_DONT_VERIFY, &path);
  if (FAILED(hr)) {
    return hr;
  }
  if (!::PathAppend(CStrBuf(path, MAX_PATH), path_tail)) {
    return GOOPDATE_E_PATH_APPEND_FAILED;
  }
  dir->SetString(path);

  // Try to create the directory. Continue if the directory can't be created.
  if (create_dir) {
    hr = CreateDir(path, NULL);
    if (FAILED(hr)) {
      CORE_LOG(LE, (_T("[GetDir failed to create dir][%s][0x%08x]"), path, hr));
    }
  }
  return S_OK;
}

// The app-specific value overrides the disable all value so read the former
// first. If it doesn't exist, read the "disable all" value.
bool GetEffectivePolicyForApp(const TCHAR* apps_default_value_name,
                              const TCHAR* app_prefix_name,
                              const GUID& app_guid,
                              DWORD* effective_policy) {
  ASSERT1(apps_default_value_name);
  ASSERT1(app_prefix_name);
  ASSERT1(effective_policy);

  CString app_value_name(app_prefix_name);
  app_value_name.Append(GuidToString(app_guid));

  HRESULT hr = RegKey::GetValue(kRegKeyGoopdateGroupPolicy,
                                app_value_name,
                                effective_policy);
  if (SUCCEEDED(hr)) {
    return true;
  } else {
    CORE_LOG(L4, (_T("[Failed to read Group Policy value][%s]"),
                  app_value_name));
  }

  hr = RegKey::GetValue(kRegKeyGoopdateGroupPolicy,
                        apps_default_value_name,
                        effective_policy);
  if (SUCCEEDED(hr)) {
    return true;
  } else {
    CORE_LOG(L4, (_T("[Failed to read Group Policy value][%s]"),
                  apps_default_value_name));
  }

  return false;
}

// Gets the raw update check period override value in seconds from the registry.
// The value must be processed for limits and overflow before using.
// Checks UpdateDev and Group Policy.
// Returns true if either override was successefully read.
bool GetLastCheckPeriodSecFromRegistry(DWORD* period_sec) {
  ASSERT1(period_sec);

  DWORD update_dev_sec = 0;
  if (SUCCEEDED(RegKey::GetValue(MACHINE_REG_UPDATE_DEV,
                                 kRegValueLastCheckPeriodSec,
                                 &update_dev_sec))) {
    CORE_LOG(L5, (_T("['LastCheckPeriodSec' override %d]"), update_dev_sec));
    *period_sec = update_dev_sec;
    return true;
  }

  DWORD group_policy_minutes = 0;
  if (SUCCEEDED(RegKey::GetValue(kRegKeyGoopdateGroupPolicy,
                                 kRegValueAutoUpdateCheckPeriodOverrideMinutes,
                                 &group_policy_minutes))) {
    CORE_LOG(L5, (_T("[Group Policy check period override %d]"),
                  group_policy_minutes));


    *period_sec = (group_policy_minutes > UINT_MAX / 60) ?
                  UINT_MAX :
                  group_policy_minutes * 60;

    return true;
  }

  return false;
}

}  // namespace

LLock ConfigManager::lock_;
ConfigManager* ConfigManager::config_manager_ = NULL;

ConfigManager* ConfigManager::Instance() {
  __mutexScope(lock_);
  if (!config_manager_) {
    config_manager_ = new ConfigManager();
  }
  return config_manager_;
}

void ConfigManager::DeleteInstance() {
  delete config_manager_;
}

CString ConfigManager::GetUserDownloadStorageDir() const {
  CString path;
  VERIFY1(SUCCEEDED(GetDir(CSIDL_LOCAL_APPDATA,
                           CString(OMAHA_REL_DOWNLOAD_STORAGE_DIR),
                           true,
                           &path)));
  return path;
}

CString ConfigManager::GetUserOfflineStorageDir() const {
  CString path;
  VERIFY1(SUCCEEDED(GetDir(CSIDL_LOCAL_APPDATA,
                           CString(OMAHA_REL_OFFLINE_STORAGE_DIR),
                           true,
                           &path)));
  return path;
}

CString ConfigManager::GetUserInitialManifestStorageDir() const {
  CString path;
  VERIFY1(SUCCEEDED(GetDir(CSIDL_LOCAL_APPDATA,
                           CString(OMAHA_REL_INITIAL_MANIFEST_DIR),
                           true,
                           &path)));
  return path;
}

CString ConfigManager::GetUserGoopdateInstallDir() const {
  CString path;
  VERIFY1(SUCCEEDED(GetDir(CSIDL_LOCAL_APPDATA,
                           CString(OMAHA_REL_GOOPDATE_INSTALL_DIR),
                           true,
                           &path)));
  return path;
}

bool ConfigManager::IsRunningFromUserGoopdateInstallDir() const {
  CString path;
  HRESULT hr = GetDir(CSIDL_LOCAL_APPDATA,
                      CString(OMAHA_REL_GOOPDATE_INSTALL_DIR),
                      false,
                      &path);
  if (FAILED(hr)) {
    return false;
  }

  return (String_StrNCmp(path,
                         app_util::GetCurrentModuleDirectory(),
                         path.GetLength(),
                         true) == 0);
}

CString ConfigManager::GetUserCrashReportsDir() const {
  CString path;
  VERIFY1(SUCCEEDED(GetDir(CSIDL_LOCAL_APPDATA,
                           CString(OMAHA_REL_CRASH_DIR),
                           true,
                           &path)));
  return path;
}

CString ConfigManager::GetMachineCrashReportsDir() const {
  CString path;
  VERIFY1(SUCCEEDED(GetDir(CSIDL_PROGRAM_FILES,
                           CString(OMAHA_REL_CRASH_DIR),
                           true,
                           &path)));
  return path;
}

CString ConfigManager::GetMachineDownloadStorageDir() const {
  CString path;
  VERIFY1(SUCCEEDED(GetDir(CSIDL_COMMON_APPDATA,
                           CString(OMAHA_REL_DOWNLOAD_STORAGE_DIR),
                           true,
                           &path)));
  return path;
}

CString ConfigManager::GetMachineSecureDownloadStorageDir() const {
  CString path;
  VERIFY1(SUCCEEDED(GetDir(CSIDL_PROGRAM_FILES,
                           CString(OMAHA_REL_DOWNLOAD_STORAGE_DIR),
                           true,
                           &path)));
  return path;
}

CString ConfigManager::GetMachineSecureOfflineStorageDir() const {
  CString path;
  VERIFY1(SUCCEEDED(GetDir(CSIDL_PROGRAM_FILES,
                           CString(OMAHA_REL_OFFLINE_STORAGE_DIR),
                           true,
                           &path)));
  return path;
}

CString ConfigManager::GetTempDownloadDir() const {
  CString path;
  VERIFY1(SUCCEEDED(GetDir(CSIDL_LOCAL_APPDATA,
                           CString(OMAHA_REL_TEMP_DOWNLOAD_DIR),
                           true,
                           &path)));
  return path;
}

CString ConfigManager::GetMachineGoopdateInstallDir() const {
  CString path;
  VERIFY1(SUCCEEDED(GetDir(CSIDL_PROGRAM_FILES,
                           CString(OMAHA_REL_GOOPDATE_INSTALL_DIR),
                           true,
                           &path)));
  return path;
}

bool ConfigManager::IsRunningFromMachineGoopdateInstallDir() const {
  CString path;
  HRESULT hr = GetDir(CSIDL_PROGRAM_FILES,
                      CString(OMAHA_REL_GOOPDATE_INSTALL_DIR),
                      false,
                      &path);
  if (FAILED(hr)) {
    return false;
  }

  return (String_StrNCmp(path,
                         app_util::GetCurrentModuleDirectory(),
                         path.GetLength(),
                         true) == 0);
}

// TODO(omaha): create a generic way to override configuration parameters.
//
// Overrides PingUrl in debug builds.
HRESULT ConfigManager::GetPingUrl(CString* url) const {
  ASSERT1(url);

#ifdef DEBUG
  if (SUCCEEDED(RegKey::GetValue(MACHINE_REG_UPDATE_DEV,
                                 kRegValueNamePingUrl,
                                 url))) {
    CORE_LOG(L5, (_T("['ping url' override %s]"), *url));
    return S_OK;
  }
#endif

  *url = kUrlPing;
  return S_OK;
}

// Overrides Url (update check url) in debug builds.
HRESULT ConfigManager::GetUpdateCheckUrl(CString* url) const {
  ASSERT1(url);

#ifdef DEBUG
  if (SUCCEEDED(RegKey::GetValue(MACHINE_REG_UPDATE_DEV,
                                 kRegValueNameUrl,
                                 url))) {
    CORE_LOG(L5, (_T("['url' override %s]"), *url));
    return S_OK;
  }
#endif

  *url = kUrlUpdateCheck;
  return S_OK;
}

// Returns the override from the registry locations if present. Otherwise,
// returns the default value.
// Default value is different value for Googlers, to make update checks more
// aggresive.
// Ensures returned value is between kMinLastCheckPeriodSec and INT_MAX except
// when the override is 0, which indicates updates are disabled.
int ConfigManager::GetLastCheckPeriodSec(bool* is_overridden) const {
  ASSERT1(is_overridden);
  DWORD registry_period_sec = 0;
  *is_overridden = GetLastCheckPeriodSecFromRegistry(&registry_period_sec);
  if (*is_overridden) {
    if (0 == registry_period_sec) {
      return 0;
    }
    const int period_sec = registry_period_sec > INT_MAX ?
                           INT_MAX :
                           static_cast<int>(registry_period_sec);

    if (period_sec < kMinLastCheckPeriodSec) {
      return kMinLastCheckPeriodSec;
    }
    return period_sec;
  }

  // Returns a lower value for Googlers.
  if (IsGoogler()) {
    return kLastCheckPeriodGooglerSec;
  }

  return kLastCheckPeriodSec;
}

// All time values are in seconds.
int ConfigManager::GetTimeSinceLastCheckedSec(bool is_machine) const {
  const uint32 now = Time64ToInt32(GetCurrent100NSTime());
  const uint32 last_checked = GetLastCheckedTime(is_machine);
  if (now < last_checked) {
    CORE_LOG(LW, (_T("[possible time warp detected]")
                  _T("[now %u][last checked %u]"), now, last_checked));
  }
  const int time_difference = abs(static_cast<int>(now - last_checked));
  bool is_period_overridden = false;
  CORE_LOG(L3, (_T("[now %u][last checked %u][update interval %u]")
                _T("[time difference %u]"),
                now, last_checked, GetLastCheckPeriodSec(&is_period_overridden),
                time_difference));
  return time_difference;
}

DWORD ConfigManager::GetLastCheckedTime(bool is_machine) const {
  const TCHAR* reg_update_key = is_machine ? MACHINE_REG_UPDATE:
                                             USER_REG_UPDATE;
  DWORD last_checked_time = 0;
  if (SUCCEEDED(RegKey::GetValue(reg_update_key,
                                 kRegValueLastChecked,
                                 &last_checked_time))) {
    return last_checked_time;
  }
  return 0;
}

HRESULT ConfigManager::SetLastCheckedTime(bool is_machine, DWORD time) const {
  const TCHAR* reg_update_key = is_machine ? MACHINE_REG_UPDATE:
                                             USER_REG_UPDATE;
  return RegKey::SetValue(reg_update_key, kRegValueLastChecked, time);
}

DWORD ConfigManager::GetInstallTime(bool is_machine) {
  const CString client_state_key_name =
      ConfigManager::Instance()->registry_client_state_goopdate(is_machine);
  DWORD update_time(0);
  if (SUCCEEDED(RegKey::GetValue(client_state_key_name,
                                 kRegValueLastUpdateTimeSec,
                                 &update_time))) {
    return update_time;
  }

  DWORD install_time(0);
  if (SUCCEEDED(RegKey::GetValue(client_state_key_name,
                                 kRegValueInstallTimeSec,
                                 &install_time))) {
    return install_time;
  }

  return 0;
}

bool ConfigManager::Is24HoursSinceInstall(bool is_machine) {
  const int kDaySec = 24 * 60 * 60;
  const uint32 now = Time64ToInt32(GetCurrent100NSTime());

  const uint32 install_time = GetInstallTime(is_machine);
  if (now < install_time) {
    CORE_LOG(LW, (_T("[Incorrect clock time detected]")
                  _T("[now %u][install_time %u]"), now, install_time));
  }
  const int time_difference = abs(static_cast<int>(now - install_time));
  return time_difference >= kDaySec;
}

bool ConfigManager::CanCollectStats(bool is_machine) const {
  if (RegKey::HasValue(MACHINE_REG_UPDATE_DEV, kRegValueForceUsageStats)) {
    return true;
  }

  // TODO(omaha): This should actually be iterating over registered products
  // rather than present ClientState keys. These are identical in most cases.
  const TCHAR* state_key_name = registry_client_state(is_machine);

  RegKey state_key;
  HRESULT hr = state_key.Open(state_key_name, KEY_READ);
  if (FAILED(hr)) {
    return false;
  }

  int num_sub_keys = state_key.GetSubkeyCount();
  for (int i = 0; i < num_sub_keys; ++i) {
    CString sub_key_name;
    if (FAILED(state_key.GetSubkeyNameAt(i, &sub_key_name))) {
      continue;
    }

    if (goopdate_utils::AreAppUsageStatsEnabled(is_machine, sub_key_name)) {
      return true;
    }
  }

  return false;
}

// Overrides OverInstall in debug builds.
bool ConfigManager::CanOverInstall() const {
#ifdef DEBUG
  DWORD value = 0;
  if (SUCCEEDED(RegKey::GetValue(MACHINE_REG_UPDATE_DEV,
                                 kRegValueNameOverInstall,
                                 &value))) {
    CORE_LOG(L5, (_T("['OverInstall' override %d]"), value));
    return value != 0;
  }
#endif
  return !OFFICIAL_BUILD;
}

// Overrides AuCheckPeriodMs. Implements a lower bound value. Returns INT_MAX
// if the registry value exceeds INT_MAX.
int ConfigManager::GetAutoUpdateTimerIntervalMs() const {
  DWORD interval(0);
  if (SUCCEEDED(RegKey::GetValue(MACHINE_REG_UPDATE_DEV,
                                 kRegValueAuCheckPeriodMs,
                                 &interval))) {
    int ret_val = 0;
    if (interval > INT_MAX) {
      ret_val = INT_MAX;
    } else if (interval < kMinAUCheckPeriodMs) {
      ret_val = kMinAUCheckPeriodMs;
    } else {
      ret_val = interval;
    }
    ASSERT1(ret_val >= kMinAUCheckPeriodMs);
    CORE_LOG(L5, (_T("['AuCheckPeriodMs' override %d]"), interval));
    return ret_val;
  }

  // Returns a lower value for Googlers.
  if (IsGoogler()) {
    return kAUCheckPeriodGooglerMs;
  }

  return kAUCheckPeriodMs;
}

int ConfigManager::GetUpdateWorkerStartUpDelayMs() const {
  int au_timer_interval_ms = GetAutoUpdateTimerIntervalMs();

  // If the AuCheckPeriod is overriden then use that as the delay.
  if (RegKey::HasValue(MACHINE_REG_UPDATE_DEV, kRegValueAuCheckPeriodMs)) {
    return au_timer_interval_ms;
  }

  int random_delay = 0;
  if (!GenRandom(&random_delay, sizeof(random_delay))) {
    return au_timer_interval_ms;
  }

  // Scale the au_check_period number to be between
  // kUpdateTimerStartupDelayMinMs and kUpdateTimerStartupDelayMaxMs.
  int scale = kUpdateTimerStartupDelayMaxMs - kUpdateTimerStartupDelayMinMs;
  ASSERT1(scale >= 0);

  int random_addition = abs(random_delay) % scale;
  ASSERT1(random_addition < scale);

  au_timer_interval_ms = kUpdateTimerStartupDelayMinMs + random_addition;
  ASSERT1(au_timer_interval_ms >= kUpdateTimerStartupDelayMinMs &&
          au_timer_interval_ms <= kUpdateTimerStartupDelayMaxMs);

  return au_timer_interval_ms;
}

// Overrides CodeRedCheckPeriodMs. Implements a lower bound value. Returns
// INT_MAX if the registry value exceeds INT_MAX.
int ConfigManager::GetCodeRedTimerIntervalMs() const {
  DWORD interval(0);
  if (SUCCEEDED(RegKey::GetValue(MACHINE_REG_UPDATE_DEV,
                                 kRegValueCrCheckPeriodMs,
                                 &interval))) {
    int ret_val = 0;
    if (interval > INT_MAX) {
      ret_val = INT_MAX;
    } else if (interval < kMinCodeRedCheckPeriodMs) {
      ret_val = kMinCodeRedCheckPeriodMs;
    } else {
      ret_val = interval;
    }
    ASSERT1(ret_val >= kMinCodeRedCheckPeriodMs);
    CORE_LOG(L5, (_T("['CrCheckPeriodMs' override %d]"), interval));
    return ret_val;
  }
  return kCodeRedCheckPeriodMs;
}

// Returns true if logging is enabled for the event type.
// Logging of errors and warnings is enabled by default.
bool ConfigManager::CanLogEvents(WORD event_type) const {
  const TCHAR* reg_update_key = MACHINE_REG_UPDATE_DEV;
  DWORD log_events_level = LOG_EVENT_LEVEL_NONE;
  if (SUCCEEDED(RegKey::GetValue(reg_update_key,
                                 kRegValueEventLogLevel,
                                 &log_events_level))) {
    switch (log_events_level) {
      case LOG_EVENT_LEVEL_ALL:
        return true;
      case LOG_EVENT_LEVEL_WARN_AND_ERROR:
        return event_type == EVENTLOG_ERROR_TYPE ||
               event_type == EVENTLOG_WARNING_TYPE;
      case LOG_EVENT_LEVEL_NONE:
      default:
        return false;
    }
  }

  return event_type == EVENTLOG_ERROR_TYPE ||
         event_type == EVENTLOG_WARNING_TYPE;
}

CString ConfigManager::GetTestSource() const {
  CString test_source;
  HRESULT hr = RegKey::GetValue(MACHINE_REG_UPDATE_DEV,
                                kRegValueTestSource,
                                &test_source);
  if (SUCCEEDED(hr)) {
    if (test_source.IsEmpty()) {
      test_source = kRegValueTestSourceAuto;
    }
    return test_source;
  }

  DWORD interval = 0;
  hr = RegKey::GetValue(MACHINE_REG_UPDATE_DEV,
                        kRegValueAuCheckPeriodMs,
                        &interval);
  if (SUCCEEDED(hr)) {
    return kRegValueTestSourceAuto;
  }

#if defined(DEBUG) || !OFFICIAL_BUILD
  test_source = kRegValueTestSourceAuto;
#endif

  return test_source;
}

// Reads the current value under HKLM/HKCU\Google\Update\value_name. Returns
// default_val if value_name does not exist.
CString GetCurrentVersionedName(bool is_machine,
                                const TCHAR* value_name,
                                const TCHAR* default_val) {
  CORE_LOG(L3, (_T("[ConfigManager::GetCurrentVersionedName]")));
  ASSERT1(value_name && *value_name);
  ASSERT1(default_val && *default_val);

  const TCHAR* key_name = is_machine ? MACHINE_REG_UPDATE : USER_REG_UPDATE;
  CString name;
  HRESULT hr(RegKey::GetValue(key_name, value_name, &name));
  if (FAILED(hr)) {
    CORE_LOG(L4, (_T("[GetValue failed][%s][0x%x][Using default name][%s]"),
                  value_name, hr, default_val));
    name = default_val;
  }

  CORE_LOG(L3, (_T("[Versioned Name][%s]"), name));
  return name;
}

// Creates a unique name of the form "{prefix}1c9b3d6baf90df3" and stores it in
// the registry under HKLM/HKCU\Google\Update\value_name. Subsequent
// invocations of GetCurrentTaskName() will return this new value.
HRESULT CreateAndSetVersionedNameInRegistry(bool is_machine,
                                            const TCHAR* prefix,
                                            const TCHAR* value_name) {
  ASSERT1(prefix && *prefix);
  ASSERT1(value_name && *value_name);

  CString name(ServiceInstall::GenerateServiceName(prefix));
  CORE_LOG(L3, (_T("Versioned name[%s][%s][%s]"), prefix, value_name, name));

  const TCHAR* key_name = is_machine ? MACHINE_REG_UPDATE : USER_REG_UPDATE;
  return RegKey::SetValue(key_name, value_name, name);
}

CString ConfigManager::GetCurrentTaskNameCore(bool is_machine) {
  CORE_LOG(L3, (_T("[GetCurrentTaskNameCore[%d]"), is_machine));

  CString default_name(goopdate_utils::GetDefaultGoopdateTaskName(
                                           is_machine,
                                           COMMANDLINE_MODE_CORE));
  return GetCurrentVersionedName(is_machine, kRegValueTaskNameC, default_name);
}

HRESULT ConfigManager::CreateAndSetVersionedTaskNameCoreInRegistry(
    bool is_machine) {
  CORE_LOG(L3, (_T("CreateAndSetVersionedTaskNameCoreInRegistry[%d]"),
                is_machine));

  CString default_name(goopdate_utils::GetDefaultGoopdateTaskName(
                                           is_machine,
                                           COMMANDLINE_MODE_CORE));
  return CreateAndSetVersionedNameInRegistry(is_machine,
                                             default_name,
                                             kRegValueTaskNameC);
}

CString ConfigManager::GetCurrentTaskNameUA(bool is_machine) {
  CORE_LOG(L3, (_T("[GetCurrentTaskNameUA[%d]"), is_machine));

  CString default_name(goopdate_utils::GetDefaultGoopdateTaskName(
                                           is_machine,
                                           COMMANDLINE_MODE_UA));
  return GetCurrentVersionedName(is_machine, kRegValueTaskNameUA, default_name);
}

HRESULT ConfigManager::CreateAndSetVersionedTaskNameUAInRegistry(bool machine) {
  CORE_LOG(L3, (_T("CreateAndSetVersionedTaskNameUAInRegistry[%d]"), machine));

  CString default_name(goopdate_utils::GetDefaultGoopdateTaskName(
                                           machine,
                                           COMMANDLINE_MODE_UA));
  return CreateAndSetVersionedNameInRegistry(machine,
                                             default_name,
                                             kRegValueTaskNameUA);
}

CString ConfigManager::GetCurrentServiceName() {
  CORE_LOG(L3, (_T("[ConfigManager::GetCurrentServiceName]")));
  return GetCurrentVersionedName(true,
                                 kRegValueServiceName,
                                 kLegacyServiceName);
}

CString ConfigManager::GetCurrentServiceDisplayName() {
  CORE_LOG(L3, (_T("[ConfigManager::GetCurrentServiceDisplayName]")));
  CString display_name;
  VERIFY1(display_name.LoadString(IDS_SERVICE_DISPLAY_NAME));
  display_name.AppendFormat(_T(" (%s)"), GetCurrentServiceName());
  return display_name;
}

HRESULT ConfigManager::CreateAndSetVersionedServiceNameInRegistry() {
  CORE_LOG(L3, (_T("CreateAndSetVersionedServiceNameInRegistry")));
  return CreateAndSetVersionedNameInRegistry(true,
                                             kServicePrefix,
                                             kRegValueServiceName);
}

HRESULT ConfigManager::GetNetConfig(CString* net_config) {
  ASSERT1(net_config);
  CString val;
  HRESULT hr = RegKey::GetValue(MACHINE_REG_UPDATE_DEV,
                                kRegValueNetConfig,
                                &val);
  if (SUCCEEDED(hr)) {
    *net_config = val;
  }
  return hr;
}

// Returns false if running in the context of an OEM install or waiting for a
// EULA to be accepted.
bool ConfigManager::CanUseNetwork(bool is_machine) const {
  DWORD eula_accepted(0);
  HRESULT hr = RegKey::GetValue(registry_update(is_machine),
                                kRegValueOmahaEulaAccepted,
                                &eula_accepted);
  if (SUCCEEDED(hr) && 0 == eula_accepted) {
    CORE_LOG(L3, (_T("[CanUseNetwork][eulaaccepted=0][false]")));
    return false;
  }

  if (IsOemInstalling(is_machine)) {
    CORE_LOG(L3, (_T("[CanUseNetwork][OEM installing][false]")));
    return false;
  }

  return true;
}

// Always returns false if !is_machine. This prevents ever blocking per-user
// instances.
// Returns true if OEM install time is present and it has been less than
// kMinOemModeSec since the OEM install.
// Non-OEM installs can never be blocked from updating because OEM install time
// will not be present.
bool ConfigManager::IsOemInstalling(bool is_machine) const {
  if (!is_machine) {
    return false;
  }

  DWORD oem_install_time_seconds = 0;
  if (FAILED(RegKey::GetValue(MACHINE_REG_UPDATE,
                              kRegValueOemInstallTimeSec,
                              &oem_install_time_seconds))) {
    CORE_LOG(L3, (_T("[IsOemInstalling][OemInstallTime not found][false]")));
    return false;
  }

  const uint32 now_seconds = Time64ToInt32(GetCurrent100NSTime());
  if (now_seconds < oem_install_time_seconds) {
    CORE_LOG(LW, (_T("[possible time warp detected][now %u][last checked %u]"),
                  now_seconds, oem_install_time_seconds));
  }
  const int time_difference_seconds =
      abs(static_cast<int>(now_seconds - oem_install_time_seconds));

  ASSERT1(0 <= time_difference_seconds);
  const bool result = time_difference_seconds < kMinOemModeSec ? true : false;

  CORE_LOG(L3, (_T("[now %u][OEM install time %u][time difference %u][%d]"),
                now_seconds, oem_install_time_seconds, time_difference_seconds,
                result));
  return result;
}

// USE IsOemInstalling() INSTEAD in most cases.
bool ConfigManager::IsWindowsInstalling() const {
#if !OFFICIAL_BUILD
  DWORD value = 0;
  if (SUCCEEDED(RegKey::GetValue(MACHINE_REG_UPDATE_DEV,
                                 kRegValueNameWindowsInstalling,
                                 &value))) {
    CORE_LOG(L3, (_T("['WindowsInstalling' override %d]"), value));
    return value != 0;
  }
#endif

  return omaha::IsWindowsInstalling();
}

// Checks if the computer name ends with .google.com or the netbios domain is
// google.
bool ConfigManager::IsGoogler() const {
  CORE_LOG(L4, (_T("[ConfigManager::IsGoogler]")));
  TCHAR dns_name[INTERNET_MAX_HOST_NAME_LENGTH] = {0};
  DWORD dns_name_size(arraysize(dns_name));
  if (::GetComputerNameEx(ComputerNameDnsFullyQualified,
                          dns_name, &dns_name_size)) {
     CORE_LOG(L4, (_T("[dns name %s]"), dns_name));
     if (String_EndsWith(dns_name, _T(".google.com"), true)) {
       return true;
     }
  }

  WKSTA_INFO_100* info = NULL;
  int kInformationLevel = 100;
  NET_API_STATUS status = ::NetWkstaGetInfo(NULL,
                                            kInformationLevel,
                                            reinterpret_cast<BYTE**>(&info));
  ON_SCOPE_EXIT(::NetApiBufferFree, info);
  if (status == NERR_Success) {
    CORE_LOG(L4, (_T("[netbios name %s]"), info->wki100_langroup));
    if (info->wki100_langroup &&
        _tcsicmp(info->wki100_langroup, _T("google")) == 0) {
      return true;
    }
  }
  return false;
}

bool ConfigManager::CanInstallApp(const GUID& app_guid) const {
  // Google Update should never be checking whether it can install itself.
  ASSERT1(!::IsEqualGUID(kGoopdateGuid, app_guid));

  DWORD effective_policy = 0;
  if (!GetEffectivePolicyForApp(kRegValueInstallAppsDefault,
                                kRegValueInstallAppPrefix,
                                app_guid,
                                &effective_policy)) {
    return kInstallPolicyDefault;
  }

  return kPolicyDisabled != effective_policy;
}

// Self-updates cannot be disabled.
bool ConfigManager::CanUpdateApp(const GUID& app_guid,
                                 bool is_manual) const {
  if (::IsEqualGUID(kGoopdateGuid, app_guid)) {
    return true;
  }

  DWORD effective_policy = 0;
  if (!GetEffectivePolicyForApp(kRegValueUpdateAppsDefault,
                                kRegValueUpdateAppPrefix,
                                app_guid,
                                &effective_policy)) {
    return kUpdatePolicyDefault;
  }

  if (kPolicyDisabled == effective_policy) {
    return false;
  }
  if ((kPolicyManualUpdatesOnly == effective_policy) && !is_manual) {
    return false;
  }

  return kUpdatePolicyDefault;
}

}  // namespace omaha
