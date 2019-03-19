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

#include "omaha/common/config_manager.h"
#include <intsafe.h>
#include <stdlib.h>
#include <lm.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <wininet.h>
#include <atlstr.h>
#include <atlsecurity.h>
#include <atltime.h>
#include <math.h>
#include "base/rand_util.h"
#include "omaha/base/app_util.h"
#include "omaha/base/constants.h"
#include "omaha/base/const_addresses.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/scope_guard.h"
#include "omaha/base/string.h"
#include "omaha/base/utils.h"
#include "omaha/base/vistautil.h"
#include "omaha/common/app_registry_utils.h"
#include "omaha/common/const_group_policy.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/common/crash_utils.h"
#include "omaha/common/oem_install_utils.h"
#include "omaha/statsreport/metrics.h"

namespace omaha {

namespace {

int MapCSIDLFor64Bit(int csidl) {
  // We assume, for now, that Omaha will always be deployed in a 32-bit form.
  // If any 64-bit components (such as the crash handler) need to query paths,
  // they need to be directed to the 32-bit equivalents.

  switch (csidl) {
    case CSIDL_PROGRAM_FILES:
      return CSIDL_PROGRAM_FILESX86;
    case CSIDL_PROGRAM_FILES_COMMON:
      return CSIDL_PROGRAM_FILES_COMMONX86;
    case CSIDL_SYSTEM:
      return CSIDL_SYSTEMX86;
    default:
      return csidl;
  }
}

// GetDir32 is named as such because it will always look for 32-bit versions
// of directories; i.e. on a 64-bit OS, CSIDL_PROGRAM_FILES will return
// Program Files (x86).  If we need to genuinely find 64-bit locations on
// 64-bit code in the future, we need to add a GetDir64() which omits the call
// to MapCSIDLFor64Bit().
HRESULT GetDir32(int csidl,
                 const CString& path_tail,
                 bool create_dir,
                 CString* dir) {
  ASSERT1(dir);

#ifdef _WIN64
  csidl = MapCSIDLFor64Bit(csidl);
#endif

  CString path;
  HRESULT hr = GetFolderPath(csidl | CSIDL_FLAG_DONT_VERIFY, &path);
  if (FAILED(hr)) {
    CORE_LOG(LW, (_T("GetDir failed to find path][%d][0x%08x]"), csidl, hr));
    return hr;
  }
  if (!::PathAppend(CStrBuf(path, MAX_PATH), path_tail)) {
    CORE_LOG(LW, (_T("GetDir failed to append path][%s][%s]"),
        path, path_tail));
    return GOOPDATE_E_PATH_APPEND_FAILED;
  }
  dir->SetString(path);

  // Try to create the directory. Continue if the directory can't be created.
  if (create_dir) {
    hr = CreateDir(path, NULL);
    if (FAILED(hr)) {
      CORE_LOG(LW, (_T("[GetDir failed to create dir][%s][0x%08x]"), path, hr));
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
  if (!IsEnrolledToDomain()) {
    OPT_LOG(L5, (_T("[GetEffectivePolicyForApp][Ignoring group policy for %s]")
                 _T("[machine is not part of a domain]"),
                 GuidToString(app_guid)));
    return false;
  }

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

  if (!IsEnrolledToDomain()) {
    OPT_LOG(L5, (_T("[GetLastCheckPeriodSecFromRegistry]")
                 _T("[Ignoring group policy]")
                 _T("[machine is not part of a domain]")));
    return false;
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
    CORE_LOG(L5, (_T("[GetLastCheckPeriodSecFromRegistry][%d]"), *period_sec));

    return true;
  }

  return false;
}

bool GetUpdatesSuppressedTimes(DWORD* start_hour,
                               DWORD* start_min,
                               DWORD* duration_min) {
  if (!IsEnrolledToDomain()) {
    OPT_LOG(L5, (_T("[GetUpdatesSuppressedTimes][Ignoring group policy]")
                 _T("[machine is not part of a domain]")));
    return false;
  }

  if (FAILED(RegKey::GetValue(kRegKeyGoopdateGroupPolicy,
                              kRegValueUpdatesSuppressedStartHour,
                              start_hour)) ||
      FAILED(RegKey::GetValue(kRegKeyGoopdateGroupPolicy,
                              kRegValueUpdatesSuppressedStartMin,
                              start_min)) ||
      FAILED(RegKey::GetValue(kRegKeyGoopdateGroupPolicy,
                              kRegValueUpdatesSuppressedDurationMin,
                              duration_min))) {
    OPT_LOG(L5, (_T("[GetUpdatesSuppressedTimes][Missing time][%x][%x][%x]"),
                *start_hour, *start_min, *duration_min));
    return false;
  }

  // UpdatesSuppressedDurationMin is limited to 16 hours.
  if (*start_hour > 23 || *start_min > 59 || *duration_min > 16 * kMinPerHour) {
    OPT_LOG(L5, (_T("[GetUpdatesSuppressedTimes][Out of bounds][%x][%x][%x]"),
                *start_hour, *start_min, *duration_min));
    return false;
  }

  CORE_LOG(L5, (_T("[GetUpdatesSuppressedTimes][%x][%x][%x]"),
                *start_hour, *start_min, *duration_min));
  return true;
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

ConfigManager::ConfigManager() {
  CString current_module_directory(app_util::GetCurrentModuleDirectory());

  CString path;
  HRESULT hr = GetDir32(CSIDL_LOCAL_APPDATA,
                        CString(OMAHA_REL_GOOPDATE_INSTALL_DIR),
                        false,
                        &path);

  is_running_from_official_user_dir_ =
      SUCCEEDED(hr) ? (String_StrNCmp(path,
                                      current_module_directory,
                                      path.GetLength(),
                                      true) == 0) :
                      false;

  hr = GetDir32(CSIDL_PROGRAM_FILES,
                CString(OMAHA_REL_GOOPDATE_INSTALL_DIR),
                false,
                &path);

  is_running_from_official_machine_dir_ =
      SUCCEEDED(hr) ? (String_StrNCmp(path,
                                      current_module_directory,
                                      path.GetLength(),
                                      true) == 0) :
                      false;
}

CString ConfigManager::GetUserDownloadStorageDir() const {
  CString path;
  VERIFY1(SUCCEEDED(GetDir32(CSIDL_LOCAL_APPDATA,
                             CString(OMAHA_REL_DOWNLOAD_STORAGE_DIR),
                             true,
                             &path)));
  return path;
}

CString ConfigManager::GetUserInstallWorkingDir() const {
  CString path;
  VERIFY1(SUCCEEDED(GetDir32(CSIDL_LOCAL_APPDATA,
                             CString(OMAHA_REL_INSTALL_WORKING_DIR),
                             true,
                             &path)));
  return path;
}

CString ConfigManager::GetUserOfflineStorageDir() const {
  CString path;
  VERIFY1(SUCCEEDED(GetDir32(CSIDL_LOCAL_APPDATA,
                             CString(OMAHA_REL_OFFLINE_STORAGE_DIR),
                             true,
                             &path)));
  return path;
}

CString ConfigManager::GetUserGoopdateInstallDirNoCreate() const {
  CString path;
  VERIFY1(SUCCEEDED(GetDir32(CSIDL_LOCAL_APPDATA,
                             CString(OMAHA_REL_GOOPDATE_INSTALL_DIR),
                             false,
                             &path)));
  return path;
}

CString ConfigManager::GetUserGoopdateInstallDir() const {
  CString path;
  VERIFY1(SUCCEEDED(GetDir32(CSIDL_LOCAL_APPDATA,
                             CString(OMAHA_REL_GOOPDATE_INSTALL_DIR),
                             true,
                             &path)));
  return path;
}

bool ConfigManager::IsRunningFromUserGoopdateInstallDir() const {
  return is_running_from_official_user_dir_;
}

CString ConfigManager::GetUserCrashReportsDir() const {
  CString path;
  VERIFY1(SUCCEEDED(GetDir32(CSIDL_LOCAL_APPDATA,
                             CString(OMAHA_REL_CRASH_DIR),
                             true,
                             &path)));
  return path;
}

CString ConfigManager::GetMachineCrashReportsDir() const {
  CString path;
  VERIFY1(SUCCEEDED(GetDir32(CSIDL_PROGRAM_FILES,
                             CString(OMAHA_REL_CRASH_DIR),
                             true,
                             &path)));
  return path;
}

CString ConfigManager::GetMachineSecureDownloadStorageDir() const {
  CString path;
  VERIFY1(SUCCEEDED(GetDir32(CSIDL_PROGRAM_FILES,
                             CString(OMAHA_REL_DOWNLOAD_STORAGE_DIR),
                             true,
                             &path)));
  return path;
}

CString ConfigManager::GetMachineInstallWorkingDir() const {
  CString path;
  VERIFY1(SUCCEEDED(GetDir32(CSIDL_PROGRAM_FILES,
                             CString(OMAHA_REL_INSTALL_WORKING_DIR),
                             true,
                             &path)));
  return path;
}

CString ConfigManager::GetMachineSecureOfflineStorageDir() const {
  CString path;
  VERIFY1(SUCCEEDED(GetDir32(CSIDL_PROGRAM_FILES,
                             CString(OMAHA_REL_OFFLINE_STORAGE_DIR),
                             true,
                             &path)));
  return path;
}

CString ConfigManager::GetTempDownloadDir() const {
  CString temp_download_dir(app_util::GetTempDirForImpersonatedOrCurrentUser());
  ASSERT1(temp_download_dir);
  HRESULT hr = CreateDir(temp_download_dir, NULL);
  if (FAILED(hr)) {
    CORE_LOG(LW, (_T("[CreateDir failed][%s][0x%08x]"),
        temp_download_dir, hr));
  }
  return temp_download_dir;
}

int ConfigManager::GetPackageCacheSizeLimitMBytes() const {
  DWORD kDefaultCacheStorageLimit = 500;  // 500 MB
  DWORD kMaxCacheStorageLimit = 5000;     // 5 GB

  if (!IsEnrolledToDomain()) {
    OPT_LOG(L5, (_T("[GetPackageCacheSizeLimitMBytes][Ignoring group policy]")
                 _T("[machine is not part of a domain]")));
    return kDefaultCacheStorageLimit;
  }

  DWORD cache_size_limit = 0;
  if (FAILED(RegKey::GetValue(kRegKeyGoopdateGroupPolicy,
                              kRegValueCacheSizeLimitMBytes,
                              &cache_size_limit)) ||
      cache_size_limit > kMaxCacheStorageLimit ||
      cache_size_limit == 0) {
    cache_size_limit = kDefaultCacheStorageLimit;
  }

  return static_cast<int>(cache_size_limit);
}

int ConfigManager::GetPackageCacheExpirationTimeDays() const {
  DWORD kDefaultCacheLifeTimeInDays = 180;  // 180 days.
  DWORD kMaxCacheLifeTimeInDays = 1800;     // Roughly 5 years.

  if (!IsEnrolledToDomain()) {
    OPT_LOG(L5, (_T("[GetPackageCacheExpirationTimeDays]")
                 _T("[Ignoring group policy]")
                 _T("[machine is not part of a domain]")));
    return kDefaultCacheLifeTimeInDays;
  }

  DWORD cache_life_limit = 0;
  if (FAILED(RegKey::GetValue(kRegKeyGoopdateGroupPolicy,
                              kRegValueCacheLifeLimitDays,
                              &cache_life_limit)) ||
      cache_life_limit > kMaxCacheLifeTimeInDays ||
      cache_life_limit == 0) {
    cache_life_limit = kDefaultCacheLifeTimeInDays;
  }

  return static_cast<int>(cache_life_limit);
}

CString ConfigManager::GetMachineGoopdateInstallDirNoCreate() const {
  CString path;
  VERIFY1(SUCCEEDED(GetDir32(CSIDL_PROGRAM_FILES,
                             CString(OMAHA_REL_GOOPDATE_INSTALL_DIR),
                             false,
                             &path)));
  return path;
}

CString ConfigManager::GetMachineGoopdateInstallDir() const {
  CString path;
  VERIFY1(SUCCEEDED(GetDir32(CSIDL_PROGRAM_FILES,
                             CString(OMAHA_REL_GOOPDATE_INSTALL_DIR),
                             true,
                             &path)));
  return path;
}

bool ConfigManager::IsRunningFromMachineGoopdateInstallDir() const {
  return is_running_from_official_machine_dir_;
}

HRESULT ConfigManager::GetPingUrl(CString* url) const {
  ASSERT1(url);

  if (SUCCEEDED(RegKey::GetValue(MACHINE_REG_UPDATE_DEV,
                                 kRegValueNamePingUrl,
                                 url))) {
    CORE_LOG(L5, (_T("['ping url' override %s]"), *url));
    return S_OK;
  }

  *url = kUrlPing;
  return S_OK;
}

HRESULT ConfigManager::GetUpdateCheckUrl(CString* url) const {
  ASSERT1(url);

  if (SUCCEEDED(RegKey::GetValue(MACHINE_REG_UPDATE_DEV,
                                 kRegValueNameUrl,
                                 url))) {
    CORE_LOG(L5, (_T("['update check url' override %s]"), *url));
    return S_OK;
  }

  *url = kUrlUpdateCheck;
  return S_OK;
}

HRESULT ConfigManager::GetCrashReportUrl(CString* url) const {
  ASSERT1(url);

  if (SUCCEEDED(RegKey::GetValue(MACHINE_REG_UPDATE_DEV,
                                 kRegValueNameCrashReportUrl,
                                 url))) {
    CORE_LOG(L5, (_T("['crash report url' override %s]"), *url));
    return S_OK;
  }

  *url = kUrlCrashReport;
  return S_OK;
}

HRESULT ConfigManager::GetMoreInfoUrl(CString* url) const {
  ASSERT1(url);

  if (SUCCEEDED(RegKey::GetValue(MACHINE_REG_UPDATE_DEV,
                                 kRegValueNameGetMoreInfoUrl,
                                 url))) {
    CORE_LOG(L5, (_T("['more info url' override %s]"), *url));
    return S_OK;
  }

  *url = kUrlMoreInfo;
  return S_OK;
}

HRESULT ConfigManager::GetUsageStatsReportUrl(CString* url) const {
  ASSERT1(url);

  if (SUCCEEDED(RegKey::GetValue(MACHINE_REG_UPDATE_DEV,
                                 kRegValueNameUsageStatsReportUrl,
                                 url))) {
    CORE_LOG(L5, (_T("['usage stats report url' override %s]"), *url));
    return S_OK;
  }

  *url = kUrlUsageStatsReport;
  return S_OK;
}

// Returns the override from the registry locations if present. Otherwise,
// returns the default value.
// Default value is different value for internal users to make update checks
// more aggresive.
// Ensures returned value is between kMinLastCheckPeriodSec and INT_MAX except
// when the override is 0, which indicates updates are disabled.
int ConfigManager::GetLastCheckPeriodSec(bool* is_overridden) const {
  ASSERT1(is_overridden);
  DWORD registry_period_sec = 0;
  *is_overridden = GetLastCheckPeriodSecFromRegistry(&registry_period_sec);
  if (*is_overridden) {
    if (0 == registry_period_sec) {
      CORE_LOG(L5, (_T("[GetLastCheckPeriodSec][0 == registry_period_sec]")));
      return 0;
    }
    const int period_sec = registry_period_sec > INT_MAX ?
                           INT_MAX :
                           static_cast<int>(registry_period_sec);

    if (period_sec < kMinLastCheckPeriodSec) {
      return kMinLastCheckPeriodSec;
    }
    CORE_LOG(L5, (_T("[GetLastCheckPeriodSec][period_sec][%d]"), period_sec));
    return period_sec;
  }

  // Returns a lower value for internal users.
  if (IsInternalUser()) {
    return kLastCheckPeriodInternalUserSec;
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

DWORD ConfigManager::GetRetryAfterTime(bool is_machine) const {
  const TCHAR* reg_update_key = is_machine ? MACHINE_REG_UPDATE:
                                             USER_REG_UPDATE;
  DWORD retry_after_time = 0;
  if (SUCCEEDED(RegKey::GetValue(reg_update_key,
                                 kRegValueRetryAfter,
                                 &retry_after_time))) {
    return retry_after_time;
  }
  return 0;
}

HRESULT ConfigManager::SetRetryAfterTime(bool is_machine, DWORD time) const {
  const TCHAR* reg_update_key = is_machine ? MACHINE_REG_UPDATE:
                                             USER_REG_UPDATE;
  return RegKey::SetValue(reg_update_key, kRegValueRetryAfter, time);
}

bool ConfigManager::CanRetryNow(bool is_machine) const {
  const uint32 now = Time64ToInt32(GetCurrent100NSTime());
  const uint32 retry_after = GetRetryAfterTime(is_machine);

  const int kMaxRetryAfterSeconds = kSecondsPerDay;
  return now >= retry_after || retry_after > now + kMaxRetryAfterSeconds;
}

DEFINE_METRIC_integer(last_started_au);
HRESULT ConfigManager::SetLastStartedAU(bool is_machine) const {
  const TCHAR* reg_update_key = is_machine ? MACHINE_REG_UPDATE:
                                             USER_REG_UPDATE;
  DWORD now = Time64ToInt32(GetCurrent100NSTime());
  metric_last_started_au = now;
  return RegKey::SetValue(reg_update_key, kRegValueLastStartedAU, now);
}

DWORD ConfigManager::GetLastUpdateTime(bool is_machine) {
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

bool ConfigManager::Is24HoursSinceLastUpdate(bool is_machine) {
  const int kDaySec = 24 * 60 * 60;
  const uint32 now = Time64ToInt32(GetCurrent100NSTime());

  const uint32 install_time = GetLastUpdateTime(is_machine);
  if (now < install_time) {
    CORE_LOG(LW, (_T("[Incorrect clock time detected]")
                  _T("[now %u][install_time %u]"), now, install_time));
  }
  const int time_difference = abs(static_cast<int>(now - install_time));
  return time_difference >= kDaySec;
}

// Uses app_registry_utils because this needs to be called in the server and
// client and it is a best effort so locking isn't necessary.
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

    if (app_registry_utils::AreAppUsageStatsEnabled(is_machine, sub_key_name)) {
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

  // Returns a lower value for internal users.
  if (IsInternalUser()) {
    return kAUCheckPeriodInternalUserMs;
  }

  return kAUCheckPeriodMs;
}

int ConfigManager::GetUpdateWorkerStartUpDelayMs() const {
  const int au_timer_interval_ms = GetAutoUpdateTimerIntervalMs();

  // If the AuCheckPeriod is overriden then use that as the delay.
  if (RegKey::HasValue(MACHINE_REG_UPDATE_DEV, kRegValueAuCheckPeriodMs)) {
    return au_timer_interval_ms;
  }

  uint32 random_value = 0;
  if (!RandUint32(&random_value)) {
    return au_timer_interval_ms;
  }

  // Scale down the |random_value| and return a random jitter value in the
  // following range:
  //    [kUpdateTimerStartupDelayMinMs, kUpdateTimerStartupDelayMaxMs]
  const int kRangeMs =
      kUpdateTimerStartupDelayMaxMs - kUpdateTimerStartupDelayMinMs + 1;
  ASSERT1(kRangeMs >= 0);

  return kUpdateTimerStartupDelayMinMs + random_value % kRangeMs;
}

int ConfigManager::GetAutoUpdateJitterMs() const {
  const int kMaxJitterMs = 60000;
  DWORD auto_update_jitter_ms(0);
  if (SUCCEEDED(RegKey::GetValue(MACHINE_REG_UPDATE_DEV,
                                 kRegValueAutoUpdateJitterMs,
                                 &auto_update_jitter_ms))) {
    return auto_update_jitter_ms >= kMaxJitterMs ? kMaxJitterMs - 1 :
                                                   auto_update_jitter_ms;
  }

  uint32 random_delay = 0;
  if (!RandUint32(&random_delay)) {
    random_delay = 0;
  }

  // There is slight bias toward lower values in the way the scaling of the
  // random delay is done here but the simplicity of the scaling formula is
  // a good trade off for the purpose of this function.
  return (random_delay % kMaxJitterMs);
}

time64 ConfigManager::GetTimeSinceLastCoreRunMs(bool is_machine) const {
  const time64 now = GetCurrentMsTime();
  const time64 last_core_run = GetLastCoreRunTimeMs(is_machine);

  if (now < last_core_run) {
    return ULONG64_MAX;
  }

  return now - last_core_run;
}

time64 ConfigManager::GetLastCoreRunTimeMs(bool is_machine) const {
  const TCHAR* reg_update_key(is_machine ? MACHINE_REG_UPDATE :
                                           USER_REG_UPDATE);
  time64 last_core_run_time = 0;
  if (SUCCEEDED(RegKey::GetValue(reg_update_key,
                                 kRegValueLastCoreRun,
                                 &last_core_run_time))) {
    return last_core_run_time;
  }

  return 0;
}

HRESULT ConfigManager::SetLastCoreRunTimeMs(bool is_machine, time64 time) {
  const TCHAR* reg_update_key(is_machine ? MACHINE_REG_UPDATE :
                                           USER_REG_UPDATE);
  return RegKey::SetValue(reg_update_key, kRegValueLastCoreRun, time);
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

time64 ConfigManager::GetTimeSinceLastCodeRedCheckMs(bool is_machine) const {
  const time64 now = GetCurrentMsTime();
  const time64 last_code_red_check = GetLastCodeRedCheckTimeMs(is_machine);

  if (now < last_code_red_check) {
    return ULONG64_MAX;
  }

  const time64 time_difference = now - last_code_red_check;
  return time_difference;
}

time64 ConfigManager::GetLastCodeRedCheckTimeMs(bool is_machine) const {
  const TCHAR* reg_update_key(is_machine ? MACHINE_REG_UPDATE: USER_REG_UPDATE);
  time64 last_code_red_check_time = 0;
  if (SUCCEEDED(RegKey::GetValue(reg_update_key,
                                 kRegValueLastCodeRedCheck,
                                 &last_code_red_check_time))) {
    return last_code_red_check_time;
  }

  return 0;
}

HRESULT ConfigManager::SetLastCodeRedCheckTimeMs(bool is_machine,
                                                 time64 time) {
  const TCHAR* reg_update_key(is_machine ? MACHINE_REG_UPDATE: USER_REG_UPDATE);
  return RegKey::SetValue(reg_update_key, kRegValueLastCodeRedCheck, time);
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

  if (oem_install_utils::IsOemInstalling(is_machine)) {
    CORE_LOG(L3, (_T("[CanUseNetwork][OEM installing][false]")));
    return false;
  }

  return true;
}

// USE oem_install_utils::IsOemInstalling() INSTEAD in most cases.
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

// Checks if the computer name ends with ".google.com" or the NetBIOS domain is
// "google".
bool ConfigManager::IsInternalUser() const {
  CORE_LOG(L4, (_T("[ConfigManager::IsInternalUser]")));
  TCHAR dns_name[INTERNET_MAX_HOST_NAME_LENGTH] = {0};
  DWORD dns_name_size(arraysize(dns_name));
  if (::GetComputerNameEx(ComputerNameDnsFullyQualified,
                          dns_name, &dns_name_size)) {
     CORE_LOG(L4, (_T("[dns name %s]"), dns_name));
     if (String_EndsWith(dns_name, kCompanyInternalDnsName, true)) {
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
        _tcsicmp(info->wki100_langroup, kCompanyInternalLanGroupName) == 0) {
      return true;
    }
  }

  CORE_LOG(L4, (_T("[ConfigManager::IsInternalUser][false]")));
  return false;
}

DWORD ConfigManager::GetEffectivePolicyForAppInstalls(const GUID& app_guid) {
  DWORD effective_policy = kPolicyDisabled;
  if (!GetEffectivePolicyForApp(kRegValueInstallAppsDefault,
                                kRegValueInstallAppPrefix,
                                app_guid,
                                &effective_policy)) {
    return kInstallPolicyDefault;
  }

  return effective_policy;
}

DWORD ConfigManager::GetEffectivePolicyForAppUpdates(const GUID& app_guid) {
  DWORD effective_policy = kPolicyDisabled;
  if (!GetEffectivePolicyForApp(kRegValueUpdateAppsDefault,
                                kRegValueUpdateAppPrefix,
                                app_guid,
                                &effective_policy)) {
    return kUpdatePolicyDefault;
  }

  return effective_policy;
}

CString ConfigManager::GetTargetVersionPrefix(const GUID& app_guid) {
  if (!IsEnrolledToDomain()) {
    OPT_LOG(L5, (_T("[GetTargetVersionPrefix][Ignoring group policy for %s]")
                 _T("[machine is not part of a domain]"),
                 GuidToString(app_guid)));
    return CString();
  }

  CString app_value_name(kRegValueTargetVersionPrefix);
  app_value_name.Append(GuidToString(app_guid));

  CString target_version_prefix;
  RegKey::GetValue(kRegKeyGoopdateGroupPolicy,
                   app_value_name,
                   &target_version_prefix);
  return target_version_prefix;
}

bool ConfigManager::IsRollbackToTargetVersionAllowed(const GUID& app_guid) {
  if (!IsEnrolledToDomain()) {
    OPT_LOG(L5, (_T("[IsRollbackToTargetVersionAllowed][false][%s]")
                 _T("[machine is not part of a domain]"),
                 GuidToString(app_guid)));
    return false;
  }

  CString app_value_name(kRegValueRollbackToTargetVersion);
  app_value_name.Append(GuidToString(app_guid));

  DWORD is_rollback_allowed = 0;
  RegKey::GetValue(kRegKeyGoopdateGroupPolicy,
                   app_value_name,
                   &is_rollback_allowed);
  return !!is_rollback_allowed;
}

bool ConfigManager::AreUpdatesSuppressedNow() {
  DWORD start_hour = 0;
  DWORD start_min = 0;
  DWORD duration_min = 0;
  if (!GetUpdatesSuppressedTimes(&start_hour, &start_min, &duration_min)) {
    return false;
  }

  CTime now(CTime::GetCurrentTime());
  tm local = {};
  now.GetLocalTm(&local);

  // tm_year is relative to 1900. tm_mon is 0-based.
  CTime start_updates_suppressed(local.tm_year + 1900,
                                 local.tm_mon + 1,
                                 local.tm_mday,
                                 start_hour,
                                 start_min,
                                 local.tm_sec,
                                 local.tm_isdst);
  CTimeSpan duration_updates_suppressed(0, 0, duration_min, 0);
  CTime end_updates_suppressed =
      start_updates_suppressed + duration_updates_suppressed;
  return now >= start_updates_suppressed && now <= end_updates_suppressed;
}

bool ConfigManager::CanInstallApp(const GUID& app_guid) const {
  // Google Update should never be checking whether it can install itself.
  ASSERT1(!::IsEqualGUID(kGoopdateGuid, app_guid));

  return kPolicyDisabled != GetEffectivePolicyForAppInstalls(app_guid);
}

// Self-updates cannot be disabled.
bool ConfigManager::CanUpdateApp(const GUID& app_guid, bool is_manual) const {
  if (::IsEqualGUID(kGoopdateGuid, app_guid)) {
    return true;
  }

  const DWORD effective_policy = GetEffectivePolicyForAppUpdates(app_guid);

  if (kPolicyDisabled == effective_policy) {
    return false;
  }
  if ((kPolicyManualUpdatesOnly == effective_policy) && !is_manual) {
    return false;
  }
  if ((kPolicyAutomaticUpdatesOnly == effective_policy) && is_manual) {
    return false;
  }

  return true;
}

bool ConfigManager::AlwaysAllowCrashUploads() const {
  DWORD always_allow_crash_uploads = 0;
  RegKey::GetValue(MACHINE_REG_UPDATE_DEV,
                   kRegValueAlwaysAllowCrashUploads,
                   &always_allow_crash_uploads);
  return always_allow_crash_uploads != 0;
}

int ConfigManager::MaxCrashUploadsPerDay() const {
  DWORD num_uploads = 0;
  if (FAILED(RegKey::GetValue(MACHINE_REG_UPDATE_DEV,
                              kRegValueMaxCrashUploadsPerDay,
                              &num_uploads))) {
    num_uploads = kDefaultCrashUploadsPerDay;
  }

  if (num_uploads > INT_MAX) {
    num_uploads = INT_MAX;
  }

  return static_cast<int>(num_uploads);
}

CString ConfigManager::GetDownloadPreferenceGroupPolicy() const {
  CString download_preference;

  if (!IsEnrolledToDomain()) {
    OPT_LOG(L5, (_T("[GetDownloadPreferenceGroupPolicy]")
                 _T("[Ignoring group policy]")
                 _T("[machine is not part of a domain]")));
    return CString();
  }

  HRESULT hr = RegKey::GetValue(kRegKeyGoopdateGroupPolicy,
                                kRegValueDownloadPreference,
                                &download_preference);
  if (SUCCEEDED(hr) && download_preference == kDownloadPreferenceCacheable) {
    return download_preference;
  }

  return CString();
}

}  // namespace omaha
