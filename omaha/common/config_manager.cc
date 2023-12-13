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
#include "omaha/base/file.h"
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
#include "omaha/goopdate/policy_status_value.h"
#include "omaha/statsreport/metrics.h"

namespace omaha {

namespace {

// This class aggregates a source/value pair for a single policy value, as well
// as a conflict-source/conflict-value pair if a conflict exists.
template <typename T>
class PolicyValue {
 public:
  PolicyValue() : has_conflict_(false) {}

  // The update method is called multiple times, once for each policy source.
  // The values are stored in the order in which the Update() method is called,
  // and |is_managed| sources have precedence.
  void Update(bool is_managed, const CString& source, const T& value) {
    if (is_managed && source_.IsEmpty()) {
      source_ = source;
      value_ = value;
    } else if (conflict_source_.IsEmpty()) {
      conflict_source_ = source;
      conflict_value_ = value;
      has_conflict_ = !!GetValueString().Compare(GetConflictValueString());
    }
  }

  // This method is called once at the conclusion of updates to a policy value,
  // with a local (machine) source and value.
  // This method constructs and returns a IPolicyStatusValue using the final
  // aggregation of the PolicyValue, if the optional |policy_status_value| is
  // provided.
  void UpdateFinal(const T& value, IPolicyStatusValue** policy_status_value) {
    if (source_.IsEmpty()) {
      source_ = _T("Default");
      value_ = value;
    }

    if (policy_status_value) {
      VERIFY1(SUCCEEDED(PolicyStatusValue::Create(source_,
                                                  GetValueString(),
                                                  has_conflict_,
                                                  conflict_source_,
                                                  GetConflictValueString(),
                                                  policy_status_value)));
    }
  }

  CString ToString() const {
    CString result(_T("[PolicyValue]"));
    SafeCStringAppendFormat(&result, _T("[source_][%s]"), source_);
    SafeCStringAppendFormat(&result, _T("[value_][%s]"), GetValueString());

    if (has_conflict_) {
      SafeCStringAppendFormat(&result, _T("[conflict_source_][%s]"),
                                       conflict_source_);
      SafeCStringAppendFormat(&result, _T("[conflict_value_][%s]"),
                                       GetConflictValueString());
    }

    return result;
  }

  CString source() const { return source_; }
  T value() const { return value_; }

  // These are the values used in the PolicyStatusValue COM object creation.
  // These values do not necessarily correspond to value() and conflict_value().
  CString GetValueString() const { return value_; }
  CString GetConflictValueString() const { return conflict_value_; }

 private:
  CString source_;
  T value_;
  bool has_conflict_;
  CString conflict_source_;
  T conflict_value_;

  DISALLOW_COPY_AND_ASSIGN(PolicyValue);
};

template <>
CString PolicyValue<DWORD>::GetValueString() const {
  return itostr(static_cast<uint32>(value_));
}

template <>
CString PolicyValue<DWORD>::GetConflictValueString() const {
  return itostr(static_cast<uint32>(conflict_value_));
}

template <>
CString PolicyValue<bool>::GetValueString() const {
  return String_BoolToString(value_);
}

template <>
CString PolicyValue<bool>::GetConflictValueString() const {
  return String_BoolToString(conflict_value_);
}

template <>
CString PolicyValue<std::vector<CString>>::GetValueString() const {
  if (value_.empty()) {
    return CString();
  }

  CString result;
  for (const auto& app_id : value_) {
    result += app_id;
    result += ";";
  }
  result.Delete(result.GetLength() - 1);

  return result;
}

template <>
CString PolicyValue<std::vector<CString>>::GetConflictValueString() const {
  if (conflict_value_.empty()) {
    return CString();
  }

  CString result;
  for (const auto& app_id : conflict_value_) {
    result += app_id;
    result += ";";
  }
  result.Delete(result.GetLength() - 1);

  return result;
}

template <>
CString PolicyValue<UpdatesSuppressedTimes>::GetValueString() const {
  CString result;
  SafeCStringFormat(&result, _T("%d, %d, %d"), value_.start_hour,
                                               value_.start_min,
                                               value_.duration_min);
  return result;
}

template <>
CString PolicyValue<UpdatesSuppressedTimes>::GetConflictValueString() const {
  CString result;
  SafeCStringFormat(&result, _T("%d, %d, %d"), conflict_value_.start_hour,
                                               conflict_value_.start_min,
                                               conflict_value_.duration_min);
  return result;
}

struct SecondsMinutes {
  DWORD seconds = 0;
};

// These methods return the string value in minutes. This is because the
// LastCheckPeriodMinutes policy is in minutes, and therefore the external
// interface returns values in minutes.
template <>
CString PolicyValue<SecondsMinutes>::GetValueString() const {
  return itostr(static_cast<uint32>(value_.seconds / 60));
}

template <>
CString PolicyValue<SecondsMinutes>::GetConflictValueString() const {
  return itostr(static_cast<uint32>(conflict_value_.seconds / 60));
}

template <typename T>
void GetPolicyDword(const TCHAR* policy_name, T* out) {
  ASSERT1(out);

  DWORD value = 0;
  if (SUCCEEDED(
          RegKey::GetValue(kRegKeyGoopdateGroupPolicy, policy_name, &value))) {
    *out = static_cast<T>(value);
  }
}

void GetPolicyString(const TCHAR* policy_name, CString* out) {
  ASSERT1(out);

  CString value;
  if (SUCCEEDED(
          RegKey::GetValue(kRegKeyGoopdateGroupPolicy, policy_name, &value))) {
    *out = value;
  }
}

}  // namespace

bool OmahaPolicyManager::IsManaged() {
  OPT_LOG(L1, (_T("[IsManaged][%s][%d]"), source(), policy_.is_managed));
  return policy_.is_managed;
}

HRESULT OmahaPolicyManager::GetLastCheckPeriodMinutes(DWORD* minutes) {
  if (!policy_.is_initialized) {
    return E_FAIL;
  }

  if (policy_.auto_update_check_period_minutes == -1) {
    return E_FAIL;
  }

  *minutes = static_cast<DWORD>(policy_.auto_update_check_period_minutes);
  OPT_LOG(L5, (_T("[GetLastCheckPeriodMinutes][%s][%d]"), source(), *minutes));
  return S_OK;
}

HRESULT OmahaPolicyManager::GetUpdatesSuppressedTimes(
    UpdatesSuppressedTimes* times) {
  if (!policy_.is_initialized) {
    return E_FAIL;
  }

  if (policy_.updates_suppressed.start_hour == -1 ||
      policy_.updates_suppressed.start_minute == -1 ||
      policy_.updates_suppressed.duration_min == -1) {
    OPT_LOG(L5,
            (_T("[GetUpdatesSuppressedTimes][%s][Missing time]"), source()));
    return E_FAIL;
  }

  times->start_hour = static_cast<DWORD>(policy_.updates_suppressed.start_hour);
  times->start_min =
      static_cast<DWORD>(policy_.updates_suppressed.start_minute);
  times->duration_min =
      static_cast<DWORD>(policy_.updates_suppressed.duration_min);

  return S_OK;
}

HRESULT OmahaPolicyManager::GetDownloadPreferenceGroupPolicy(
    CString* download_preference) {
  if (!policy_.is_initialized || policy_.download_preference.IsEmpty()) {
    return E_FAIL;
  }

  *download_preference = policy_.download_preference;
  return S_OK;
}

HRESULT OmahaPolicyManager::GetPackageCacheSizeLimitMBytes(
    DWORD* cache_size_limit) {
  if (!policy_.is_initialized || policy_.cache_size_limit == -1) {
    return E_FAIL;
  }

  *cache_size_limit = static_cast<DWORD>(policy_.cache_size_limit);
  return S_OK;
}

HRESULT OmahaPolicyManager::GetPackageCacheExpirationTimeDays(
    DWORD* cache_life_limit) {
  if (!policy_.is_initialized || policy_.cache_life_limit == -1) {
    return E_FAIL;
  }

  *cache_life_limit = static_cast<DWORD>(policy_.cache_life_limit);
  return S_OK;
}

HRESULT OmahaPolicyManager::GetProxyMode(CString* proxy_mode) {
  if (!policy_.is_initialized || policy_.proxy_mode.IsEmpty()) {
    return E_FAIL;
  }

  *proxy_mode = policy_.proxy_mode;
  return S_OK;
}

HRESULT OmahaPolicyManager::GetProxyPacUrl(CString* proxy_pac_url) {
  if (!policy_.is_initialized ||
      policy_.proxy_mode.CompareNoCase(kProxyModePacScript) != 0 ||
      policy_.proxy_pac_url.IsEmpty()) {
    return E_FAIL;
  }

  *proxy_pac_url = policy_.proxy_pac_url;
  return S_OK;
}

HRESULT OmahaPolicyManager::GetProxyServer(CString* proxy_server) {
  if (!policy_.is_initialized ||
      policy_.proxy_mode.CompareNoCase(kProxyModeFixedServers) != 0 ||
      policy_.proxy_server.IsEmpty()) {
    return E_FAIL;
  }

  *proxy_server = policy_.proxy_server;
  return S_OK;
}

HRESULT OmahaPolicyManager::GetForceInstallApps(bool is_machine,
                                                std::vector<CString>* app_ids) {
  ASSERT1(app_ids);
  ASSERT1(app_ids->empty());

  if (!policy_.is_initialized) {
    return E_FAIL;
  }

  for (const auto& app_settings : policy_.application_settings) {
    const DWORD expected = is_machine ? kPolicyForceInstallMachine :
                                        kPolicyForceInstallUser;
    if (static_cast<DWORD>(app_settings.second.install) != expected) {
      continue;
    }

    CString app_id_string = GuidToString(app_settings.first);
    app_ids->push_back(app_id_string);
  }

  return !app_ids->empty() ? S_OK : E_FAIL;
}

HRESULT OmahaPolicyManager::GetEffectivePolicyForAppInstalls(
    const GUID& app_guid, DWORD* install_policy) {
  if (!policy_.is_initialized) {
    return E_FAIL;
  }

  if (policy_.application_settings.count(app_guid) &&
      policy_.application_settings.at(app_guid).install != -1) {
    *install_policy = policy_.application_settings.at(app_guid).install;
    return S_OK;
  }

  if (policy_.install_default == -1) {
    return E_FAIL;
  }

  *install_policy = policy_.install_default;
  return S_OK;
}

HRESULT OmahaPolicyManager::GetEffectivePolicyForAppUpdates(
    const GUID& app_guid, DWORD* update_policy) {
  if (!policy_.is_initialized) {
    return E_FAIL;
  }

  if (policy_.application_settings.count(app_guid) &&
      policy_.application_settings.at(app_guid).update != -1) {
    *update_policy = policy_.application_settings.at(app_guid).update;
    return S_OK;
  }

  if (policy_.update_default == -1) {
    return E_FAIL;
  }

  *update_policy = policy_.update_default;
  return S_OK;
}

HRESULT OmahaPolicyManager::GetTargetChannel(const GUID& app_guid,
                                             CString* target_channel) {
  if (!policy_.is_initialized ||
      !policy_.application_settings.count(app_guid) ||
      policy_.application_settings.at(app_guid).target_channel.IsEmpty()) {
    return E_FAIL;
  }

  *target_channel = policy_.application_settings.at(app_guid).target_channel;
  return S_OK;
}

HRESULT OmahaPolicyManager::GetTargetVersionPrefix(
    const GUID& app_guid, CString* target_version_prefix) {
  if (!policy_.is_initialized ||
      !policy_.application_settings.count(app_guid) ||
      policy_.application_settings.at(app_guid)
          .target_version_prefix.IsEmpty()) {
    return E_FAIL;
  }

  *target_version_prefix =
      policy_.application_settings.at(app_guid).target_version_prefix;
  return S_OK;
}

HRESULT OmahaPolicyManager::IsRollbackToTargetVersionAllowed(
    const GUID& app_guid, bool* rollback_allowed) {
  if (!policy_.is_initialized ||
      !policy_.application_settings.count(app_guid) ||
      policy_.application_settings.at(app_guid).rollback_to_target_version ==
          -1) {
    return E_FAIL;
  }

  *rollback_allowed =
      !!policy_.application_settings.at(app_guid).rollback_to_target_version;
  return S_OK;
}

void OmahaPolicyManager::set_policy(const CachedOmahaPolicy& policy) {
  OPT_LOG(L1, (_T("[OmahaPolicyManager::set_policy][%s][%s]"), source(),
               policy.ToString()));
  policy_ = policy;
}

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
  config_manager_ = NULL;
}

ConfigManager::ConfigManager()
    : group_policy_manager_(new OmahaPolicyManager(_T("Group Policy"))),
      dm_policy_manager_(new OmahaPolicyManager(_T("Device Management"))),
      are_cloud_policies_preferred_(false) {
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

  // We initialize the policy managers without taking the Group Policy critical
  // section. Later, once we know the exact scenario that we are operating
  // under, we may reload the policies with the critical section lock. At the
  // moment, we reload the policies with the critical section lock for all User
  // installs and updates, as well as all Machine updates.
  VERIFY1(SUCCEEDED(LoadPolicies(false)));
}

CString ConfigManager::GetUserDownloadStorageDir() const {
  CString path;
  VERIFY_SUCCEEDED(GetDir32(CSIDL_LOCAL_APPDATA,
                             CString(OMAHA_REL_DOWNLOAD_STORAGE_DIR),
                             true,
                             &path));
  return path;
}

CString ConfigManager::GetUserInstallWorkingDir() const {
  CString path;
  VERIFY_SUCCEEDED(GetDir32(CSIDL_LOCAL_APPDATA,
                             CString(OMAHA_REL_INSTALL_WORKING_DIR),
                             true,
                             &path));
  return path;
}

CString ConfigManager::GetUserOfflineStorageDir() const {
  CString path;
  VERIFY_SUCCEEDED(GetDir32(CSIDL_LOCAL_APPDATA,
                             CString(OMAHA_REL_OFFLINE_STORAGE_DIR),
                             true,
                             &path));
  return path;
}

CString ConfigManager::GetUserGoopdateInstallDirNoCreate() const {
  CString path;
  VERIFY_SUCCEEDED(GetDir32(CSIDL_LOCAL_APPDATA,
                             CString(OMAHA_REL_GOOPDATE_INSTALL_DIR),
                             false,
                             &path));
  return path;
}

CString ConfigManager::GetUserGoopdateInstallDir() const {
  CString path;
  VERIFY_SUCCEEDED(GetDir32(CSIDL_LOCAL_APPDATA,
                             CString(OMAHA_REL_GOOPDATE_INSTALL_DIR),
                             true,
                             &path));
  return path;
}

bool ConfigManager::IsRunningFromUserGoopdateInstallDir() const {
  return is_running_from_official_user_dir_;
}

CString ConfigManager::GetUserCrashReportsDir() const {
  return app_util::GetTempDir();
}

CString ConfigManager::GetMachineCrashReportsDir() const {
  return GetSecureSystemTempDir();
}

CString ConfigManager::GetMachineSecureDownloadStorageDir() const {
  CString path;
  VERIFY_SUCCEEDED(GetDir32(CSIDL_PROGRAM_FILES,
                             CString(OMAHA_REL_DOWNLOAD_STORAGE_DIR),
                             true,
                             &path));
  return path;
}

CString ConfigManager::GetMachineInstallWorkingDir() const {
  CString path;
  VERIFY_SUCCEEDED(GetDir32(CSIDL_PROGRAM_FILES,
                             CString(OMAHA_REL_INSTALL_WORKING_DIR),
                             true,
                             &path));
  return path;
}

CString ConfigManager::GetMachineSecureOfflineStorageDir() const {
  CString path;
  VERIFY_SUCCEEDED(GetDir32(CSIDL_PROGRAM_FILES,
                             CString(OMAHA_REL_OFFLINE_STORAGE_DIR),
                             true,
                             &path));
  return path;
}

CString ConfigManager::GetTempDownloadDir() const {
  CString temp_download_dir(app_util::GetTempDirForImpersonatedOrCurrentUser());
  if (temp_download_dir.IsEmpty()) {
    return temp_download_dir;
  }

  HRESULT hr = CreateDir(temp_download_dir, NULL);
  if (FAILED(hr)) {
    CORE_LOG(LW, (_T("[CreateDir failed][%s][0x%08x]"),
        temp_download_dir, hr));
  }
  return temp_download_dir;
}

int ConfigManager::GetPackageCacheSizeLimitMBytes(
    IPolicyStatusValue** policy_status_value) const {
  DWORD kDefaultCacheStorageLimit = 500;  // 500 MB
  DWORD kMaxCacheStorageLimit = 5000;     // 5 GB

  PolicyValue<DWORD> v;

  for (size_t i = 0; i != policies_.size(); ++i) {
    DWORD cache_size_limit = 0;
    HRESULT hr = policies_[i]->GetPackageCacheSizeLimitMBytes(
        &cache_size_limit);

    if (SUCCEEDED(hr)) {
      if (cache_size_limit <= kMaxCacheStorageLimit && cache_size_limit > 0) {
        v.Update(policies_[i]->IsManaged(),
                 policies_[i]->source(),
                 cache_size_limit);
      }
    }
  }

  v.UpdateFinal(kDefaultCacheStorageLimit, policy_status_value);

  OPT_LOG(L5, (_T("[GetPackageCacheSizeLimitMBytes][%s]"), v.ToString()));

  return v.value();
}

int ConfigManager::GetPackageCacheExpirationTimeDays(
    IPolicyStatusValue** policy_status_value) const {
  DWORD kDefaultCacheLifeTimeInDays = 180;  // 180 days.
  DWORD kMaxCacheLifeTimeInDays = 1800;     // Roughly 5 years.

  PolicyValue<DWORD> v;

  for (size_t i = 0; i != policies_.size(); ++i) {
    DWORD cache_life_limit = 0;
    HRESULT hr = policies_[i]->GetPackageCacheExpirationTimeDays(
        &cache_life_limit);

    if (SUCCEEDED(hr)) {
      if (cache_life_limit <= kMaxCacheLifeTimeInDays && cache_life_limit > 0) {
        v.Update(policies_[i]->IsManaged(),
                 policies_[i]->source(),
                 cache_life_limit);
      }
    }
  }

  v.UpdateFinal(kDefaultCacheLifeTimeInDays, policy_status_value);

  OPT_LOG(L5, (_T("[GetPackageCacheExpirationTimeDays][%s]"), v.ToString()));

  return v.value();
}

HRESULT ConfigManager::GetProxyMode(
    CString* proxy_mode,
    IPolicyStatusValue** policy_status_value) const {
  ASSERT1(proxy_mode);

  PolicyValue<CString> v;

  for (size_t i = 0; i != policies_.size(); ++i) {
    CString mode;
    HRESULT hr = policies_[i]->GetProxyMode(&mode);
    if (SUCCEEDED(hr)) {
      v.Update(policies_[i]->IsManaged(), policies_[i]->source(), mode);
    }
  }

  if (v.source().IsEmpty()) {
    // No managed source had a value set for this policy. There is no local
    // default value for this policy. So we return failure.
    return E_FAIL;
  }

  v.UpdateFinal(CString(), policy_status_value);

  OPT_LOG(L5, (_T("[GetProxyMode][%s]"), v.ToString()));

  *proxy_mode = v.value();
  return S_OK;
}

HRESULT ConfigManager::GetProxyPacUrl(
    CString* proxy_pac_url,
    IPolicyStatusValue** policy_status_value) const {
  ASSERT1(proxy_pac_url);

  PolicyValue<CString> v;

  for (size_t i = 0; i != policies_.size(); ++i) {
    CString pac_url;
    HRESULT hr = policies_[i]->GetProxyPacUrl(&pac_url);
    if (SUCCEEDED(hr)) {
      v.Update(policies_[i]->IsManaged(), policies_[i]->source(), pac_url);
    }
  }

  if (v.source().IsEmpty()) {
    // No managed source had a value set for this policy. There is no local
    // default value for this policy. So we return failure.
    return E_FAIL;
  }

  v.UpdateFinal(CString(), policy_status_value);

  OPT_LOG(L5, (_T("[GetProxyPacUrl][%s]"), v.ToString()));

  *proxy_pac_url = v.value();
  return S_OK;
}

HRESULT ConfigManager::GetProxyServer(
    CString* proxy_server,
    IPolicyStatusValue** policy_status_value) const {
  ASSERT1(proxy_server);

  PolicyValue<CString> v;

  for (size_t i = 0; i != policies_.size(); ++i) {
    CString server;
    HRESULT hr = policies_[i]->GetProxyServer(&server);
    if (SUCCEEDED(hr)) {
      v.Update(policies_[i]->IsManaged(), policies_[i]->source(), server);
    }
  }

  if (v.source().IsEmpty()) {
    // No managed source had a value set for this policy. There is no local
    // default value for this policy. So we return failure.
    return E_FAIL;
  }

  v.UpdateFinal(CString(), policy_status_value);

  OPT_LOG(L5, (_T("[GetProxyServer][%s]"), v.ToString()));

  *proxy_server = v.value();
  return S_OK;
}

HRESULT ConfigManager::GetForceInstallApps(
    bool is_machine,
    std::vector<CString>* app_ids,
    IPolicyStatusValue** policy_status_value) const {
  ASSERT1(app_ids);

  PolicyValue<std::vector<CString>> v;

  for (size_t i = 0; i != policies_.size(); ++i) {
    std::vector<CString> t;
    HRESULT hr = policies_[i]->GetForceInstallApps(is_machine, &t);
    if (SUCCEEDED(hr)) {
      v.Update(policies_[i]->IsManaged(), policies_[i]->source(), t);
    }
  }

  if (v.source().IsEmpty()) {
    // No managed source had a value set for this policy. There is no local
    // default value for this policy. So we return failure.
    return E_FAIL;
  }

  v.UpdateFinal(std::vector<CString>(), policy_status_value);

  OPT_LOG(L5, (_T("[GetForceInstallApps][is_machine][%d][%s]"), is_machine,
               v.ToString()));

  *app_ids = v.value();
  return S_OK;
}

CString ConfigManager::GetMachineGoopdateInstallDirNoCreate() const {
  CString path;
  VERIFY_SUCCEEDED(GetDir32(CSIDL_PROGRAM_FILES,
                             CString(OMAHA_REL_GOOPDATE_INSTALL_DIR),
                             false,
                             &path));
  return path;
}

CString ConfigManager::GetMachineGoopdateInstallDir() const {
  CString path;
  VERIFY_SUCCEEDED(GetDir32(CSIDL_PROGRAM_FILES,
                             CString(OMAHA_REL_GOOPDATE_INSTALL_DIR),
                             true,
                             &path));
  return path;
}

CString ConfigManager::GetUserCompanyDir() const {
  CString path;
  VERIFY_SUCCEEDED(GetDir32(CSIDL_LOCAL_APPDATA,
                            CString(OMAHA_REL_COMPANY_DIR),
                            false,
                            &path));
  return path;
}

CString ConfigManager::GetMachineCompanyDir() const {
  CString path;
  VERIFY_SUCCEEDED(GetDir32(CSIDL_PROGRAM_FILES,
                            CString(OMAHA_REL_COMPANY_DIR),
                            false,
                            &path));
  return path;
}

CString ConfigManager::GetTempDir() const {
  return ::IsUserAnAdmin() ? GetSecureSystemTempDir() :
                             app_util::GetTempDirForImpersonatedOrCurrentUser();
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

HRESULT ConfigManager::GetAppLogoUrl(CString* url) const {
  ASSERT1(url);

  if (SUCCEEDED(RegKey::GetValue(MACHINE_REG_UPDATE_DEV,
                                 kRegValueNameAppLogoUrl,
                                 url))) {
    CORE_LOG(L5, (_T("['app logo url' override %s]"), *url));
    return S_OK;
  }

  *url = kUrlAppLogo;
  return S_OK;
}

#if defined(HAS_DEVICE_MANAGEMENT)

HRESULT ConfigManager::GetDeviceManagementUrl(CString* url) const {
  ASSERT1(url);

  if (SUCCEEDED(RegKey::GetValue(MACHINE_REG_UPDATE_DEV,
                                 kRegValueNameDeviceManagementUrl,
                                 url))) {
    CORE_LOG(L5, (_T("['device management url' override %s]"), *url));
    return S_OK;
  }

  *url = kUrlDeviceManagement;
  return S_OK;
}

CPath ConfigManager::GetPolicyResponsesDir() const {
  CString path;
  VERIFY_SUCCEEDED(GetDir32(CSIDL_PROGRAM_FILES,
                             CString(OMAHA_REL_POLICY_RESPONSES_DIR),
                             true,
                             &path));
  return CPath(path);
}

#endif  // defined(HAS_DEVICE_MANAGEMENT)

HRESULT ConfigManager::LoadPolicies(bool should_acquire_critical_section) {
  HRESULT hr = LoadGroupPolicies(should_acquire_critical_section);
  if (FAILED(hr)) {
    return hr;
  }

  policies_.clear();
  if (are_cloud_policies_preferred_) {
    policies_.push_back(dm_policy_manager_);
    policies_.push_back(group_policy_manager_);
  } else {
    policies_.push_back(group_policy_manager_);
    policies_.push_back(dm_policy_manager_);
  }

  return hr;
}

HRESULT ConfigManager::LoadGroupPolicies(bool should_acquire_critical_section) {
  CachedOmahaPolicy group_policies;
  ON_SCOPE_EXIT_OBJ(*group_policy_manager_, &OmahaPolicyManager::set_policy,
                    ByRef(group_policies));

  HANDLE policy_critical_section = NULL;
  ScopeGuard guard_policy_critical_section =
      MakeGuard(::LeaveCriticalPolicySection, ByRef(policy_critical_section));

  // The Policy critical section will only be taken if the machine is
  // Enterprise-joined and `should_acquire_critical_section` is true.
  if (IsEnterpriseManaged()) {
    group_policies.is_managed = true;

    if (should_acquire_critical_section) {
      policy_critical_section = ::EnterCriticalPolicySection(TRUE);
      // Not getting the policy critical section is a fatal error. So we will
      // crash here if `policy_critical_section` is NULL.
      if (!policy_critical_section) {
        __debugbreak();
      }
    } else {
      guard_policy_critical_section.Dismiss();
      // Fall through to read the policies.
    }

  } else {
    guard_policy_critical_section.Dismiss();
    OPT_LOG(L1, (_T("[ConfigManager::LoadGroupPolicies][Machine is not ")
                 _T("Enterprise Managed]")));

    // We still fall through to read the policies in order to store them as
    // a conflict source in chrome://policy. The policies will not be respected
    // however.
  }

  if (!RegKey::HasKey(kRegKeyGoopdateGroupPolicy)) {
    group_policies.is_managed = false;

    OPT_LOG(L1, (_T("[ConfigManager::LoadGroupPolicies][No Group Policies ")
                 _T("found under key][%s]"),
                 kRegKeyGoopdateGroupPolicy));

    return S_FALSE;
  }

  RegKey group_policy_key;
  HRESULT hr = group_policy_key.Open(kRegKeyGoopdateGroupPolicy, KEY_READ);
  if (FAILED(hr)) {
    if (group_policies.is_managed) {
      // Not getting the Group Policies is a fatal error if this is a managed
      // machine. So we will crash here.
      __debugbreak();
    }

    return hr;
  }

  group_policies.is_initialized = true;

  GetPolicyDword(kRegValueCloudPolicyOverridesPlatformPolicy,
                 &are_cloud_policies_preferred_);

  GetPolicyDword(kRegValueAutoUpdateCheckPeriodOverrideMinutes,
                 &group_policies.auto_update_check_period_minutes);
  GetPolicyString(kRegValueDownloadPreference,
                  &group_policies.download_preference);
  GetPolicyDword(kRegValueCacheSizeLimitMBytes,
                 &group_policies.cache_size_limit);
  GetPolicyDword(kRegValueCacheLifeLimitDays, &group_policies.cache_life_limit);

  GetPolicyDword(kRegValueUpdatesSuppressedStartHour,
                 &group_policies.updates_suppressed.start_hour);
  GetPolicyDword(kRegValueUpdatesSuppressedStartMin,
                 &group_policies.updates_suppressed.start_minute);
  GetPolicyDword(kRegValueUpdatesSuppressedDurationMin,
                 &group_policies.updates_suppressed.duration_min);

  GetPolicyString(kRegValueProxyMode, &group_policies.proxy_mode);
  GetPolicyString(kRegValueProxyServer, &group_policies.proxy_server);
  GetPolicyString(kRegValueProxyPacUrl, &group_policies.proxy_pac_url);

  GetPolicyDword(kRegValueInstallAppsDefault, &group_policies.install_default);
  GetPolicyDword(kRegValueUpdateAppsDefault, &group_policies.update_default);

  static const int kGuidLen = 38;
  int value_count = group_policy_key.GetValueCount();
  for (int i = 0; i < value_count; ++i) {
    CString value_name;
    DWORD type = 0;
    if (FAILED(group_policy_key.GetValueNameAt(i, &value_name, &type))) {
      continue;
    }

    int app_id_start = value_name.Find(_T('{'));
    if (app_id_start <= 0) {
      continue;
    }

    CString app_id_string = value_name.Mid(app_id_start);
    GUID app_id = {};
    if (app_id_string.GetLength() != kGuidLen ||
        FAILED(StringToGuidSafe(app_id_string, &app_id)) ||
        GUID_NULL == app_id) {
      continue;
    }

    CString policy_name = value_name.Left(app_id_start);

    switch (type) {
      case REG_DWORD: {
        DWORD dword_policy_value = 0;
        if (SUCCEEDED(
                group_policy_key.GetValue(value_name, &dword_policy_value))) {
          if (!policy_name.CompareNoCase(kRegValueInstallAppPrefix)) {
            group_policies.application_settings[app_id].install =
                dword_policy_value;
          } else if (!policy_name.CompareNoCase(kRegValueUpdateAppPrefix)) {
            group_policies.application_settings[app_id].update =
                dword_policy_value;
          } else if (!policy_name.CompareNoCase(
                         kRegValueRollbackToTargetVersion)) {
            group_policies.application_settings[app_id]
                .rollback_to_target_version = dword_policy_value;
          } else {
            OPT_LOG(LW, (_T("[ConfigManager::LoadGroupPolicies][Unexpected ")
                         _T("DWORD policy prefix encountered][%s][%d]"),
                         value_name, dword_policy_value));
          }
        }
        break;
      }

      case REG_SZ: {
        CString string_policy_value;
        if (SUCCEEDED(
                group_policy_key.GetValue(value_name, &string_policy_value))) {
          if (!policy_name.CompareNoCase(kRegValueTargetChannel)) {
            group_policies.application_settings[app_id].target_channel =
                string_policy_value;
          } else if (!policy_name.CompareNoCase(kRegValueTargetVersionPrefix)) {
            group_policies.application_settings[app_id].target_version_prefix =
                string_policy_value;
          } else {
            OPT_LOG(LW, (_T("[ConfigManager::LoadGroupPolicies][Unexpected ")
                         _T("String policy prefix encountered][%s][%s]"),
                         value_name, string_policy_value));
          }
        }
        break;
      }

      default:
        OPT_LOG(LW, (_T("[ConfigManager::LoadGroupPolicies][Unexpected Type ")
                     _T("for policy prefix encountered][%s][%d]"),
                     value_name, type));
        break;
    }
  }

  return S_OK;
}

void ConfigManager::SetOmahaDMPolicies(const CachedOmahaPolicy& dm_policy) {
  dm_policy_manager_->set_policy(dm_policy);
  REPORT_LOG(L1, (_T("[ConfigManager::SetOmahaDMPolicies][%s]"),
                  dm_policy.ToString()));
}

// Returns the override from the registry locations if present. Otherwise,
// returns the default value.
// Default value is different value for internal users to make update checks
// more aggresive.
// Ensures returned value is between kMinLastCheckPeriodSec and INT_MAX except
// when the override is 0, which indicates updates are disabled.
int ConfigManager::GetLastCheckPeriodSec(bool* is_overridden) const {
  ASSERT1(is_overridden);

  return GetLastCheckPeriodSec(is_overridden, NULL);
}

int ConfigManager::GetLastCheckPeriodSec(
    bool* is_overridden, IPolicyStatusValue** status_value_minutes) const {
  ASSERT1(is_overridden);

  PolicyValue<SecondsMinutes> v;

  DWORD policy_period_sec = 0;
  if (SUCCEEDED(RegKey::GetValue(MACHINE_REG_UPDATE_DEV,
                                 kRegValueLastCheckPeriodSec,
                                 &policy_period_sec))) {
    if (policy_period_sec > 0 && policy_period_sec < kMinLastCheckPeriodSec) {
      policy_period_sec = kMinLastCheckPeriodSec;
    } else if (policy_period_sec > INT_MAX) {
      policy_period_sec = INT_MAX;
    }
    v.Update(true, _T("UpdateDev"), {policy_period_sec});
  } else {
    DWORD minutes = 0;
    HRESULT hr = E_FAIL;
    for (size_t i = 0; i != policies_.size(); ++i) {
      hr = policies_[i]->GetLastCheckPeriodMinutes(&minutes);
      if (SUCCEEDED(hr)) {
        policy_period_sec = minutes * 60ULL > INT_MAX ? INT_MAX : minutes * 60;
        v.Update(policies_[i]->IsManaged(),
                 policies_[i]->source(),
                 {policy_period_sec});
      }
    }
  }

  *is_overridden = !v.source().IsEmpty();
  policy_period_sec = IsInternalUser() ? kLastCheckPeriodInternalUserSec :
                                         kLastCheckPeriodSec;
  v.UpdateFinal({policy_period_sec}, status_value_minutes);

  OPT_LOG(L5, (_T("[GetLastCheckPeriodMinutes][%s]"), v.ToString()));

  return v.value().seconds;
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

DWORD ConfigManager::GetEffectivePolicyForAppInstalls(
    const GUID& app_guid, IPolicyStatusValue** policy_status_value) const {
  PolicyValue<DWORD> v;

  for (size_t i = 0; i != policies_.size(); ++i) {
    DWORD effective_policy = kPolicyDisabled;
    HRESULT hr = policies_[i]->GetEffectivePolicyForAppInstalls(
        app_guid,
        &effective_policy);
    if (SUCCEEDED(hr)) {
      v.Update(policies_[i]->IsManaged(),
               policies_[i]->source(),
               effective_policy);
    }
  }

  v.UpdateFinal(kInstallPolicyDefault, policy_status_value);

  OPT_LOG(L5, (_T("[GetEffectivePolicyForAppInstalls][%s][%s]"),
               GuidToString(app_guid), v.ToString()));

  return v.value();
}

DWORD ConfigManager::GetEffectivePolicyForAppUpdates(
    const GUID& app_guid, IPolicyStatusValue** policy_status_value) const {
  PolicyValue<DWORD> v;

  for (size_t i = 0; i != policies_.size(); ++i) {
    DWORD effective_policy = kPolicyDisabled;
    HRESULT hr = policies_[i]->GetEffectivePolicyForAppUpdates(
        app_guid,
        &effective_policy);
    if (SUCCEEDED(hr)) {
      v.Update(policies_[i]->IsManaged(),
               policies_[i]->source(),
               effective_policy);
    }
  }

  v.UpdateFinal(kUpdatePolicyDefault, policy_status_value);

  OPT_LOG(L5, (_T("[GetEffectivePolicyForAppUpdates][%s][%s]"),
               GuidToString(app_guid), v.ToString()));

  return v.value();
}

CString ConfigManager::GetTargetChannel(
    const GUID& app_guid, IPolicyStatusValue** policy_status_value) const {
  PolicyValue<CString> v;

  for (size_t i = 0; i != policies_.size(); ++i) {
    CString target_channel;
    HRESULT hr = policies_[i]->GetTargetChannel(app_guid, &target_channel);
    if (SUCCEEDED(hr)) {
      v.Update(policies_[i]->IsManaged(),
               policies_[i]->source(),
               target_channel);
    }
  }

  v.UpdateFinal(CString(), policy_status_value);

  OPT_LOG(L5, (_T("[GetTargetChannel][%s][%s]"),
               GuidToString(app_guid), v.ToString()));
  return v.value();
}

CString ConfigManager::GetTargetVersionPrefix(
    const GUID& app_guid, IPolicyStatusValue** policy_status_value) const {
  PolicyValue<CString> v;

  for (size_t i = 0; i != policies_.size(); ++i) {
    CString target_version_prefix;
    HRESULT hr = policies_[i]->GetTargetVersionPrefix(app_guid,
                                                      &target_version_prefix);
    if (SUCCEEDED(hr)) {
      v.Update(policies_[i]->IsManaged(),
               policies_[i]->source(),
               target_version_prefix);
    }
  }

  v.UpdateFinal(CString(), policy_status_value);

  OPT_LOG(L5, (_T("[GetTargetVersionPrefix][%s][%s]"),
               GuidToString(app_guid), v.ToString()));

  return v.value();
}

bool ConfigManager::IsRollbackToTargetVersionAllowed(
    const GUID& app_guid, IPolicyStatusValue** policy_status_value) const {
  PolicyValue<bool> v;

  for (size_t i = 0; i != policies_.size(); ++i) {
    bool rollback_allowed = false;
    HRESULT hr = policies_[i]->IsRollbackToTargetVersionAllowed(
        app_guid,
        &rollback_allowed);
    if (SUCCEEDED(hr)) {
      v.Update(policies_[i]->IsManaged(),
               policies_[i]->source(),
               rollback_allowed);
    }
  }

  v.UpdateFinal(false, policy_status_value);

  OPT_LOG(L5, (_T("[IsRollbackToTargetVersionAllowed][%s][%s]"),
               GuidToString(app_guid), v.ToString()));

  return v.value();
}

HRESULT ConfigManager::GetUpdatesSuppressedTimes(
    const CTime& time,
    UpdatesSuppressedTimes* times,
    bool* are_updates_suppressed,
    IPolicyStatusValue** policy_status_value) const {
  ASSERT1(times);
  ASSERT1(are_updates_suppressed);

  PolicyValue<UpdatesSuppressedTimes> v;

  HRESULT hr = E_FAIL;
  for (size_t i = 0; i != policies_.size(); ++i) {
    UpdatesSuppressedTimes t;
    hr = policies_[i]->GetUpdatesSuppressedTimes(&t);
    if (SUCCEEDED(hr)) {
      v.Update(policies_[i]->IsManaged(), policies_[i]->source(), t);
    }
  }

  if (v.source().IsEmpty()) {
    // No managed source had a value set.
    return E_FAIL;
  }

  // UpdatesSuppressedDurationMin is limited to 16 hours.
  if (v.value().start_hour > 23 ||
      v.value().start_min > 59 ||
      v.value().duration_min > 16 * kMinPerHour) {
    OPT_LOG(L5, (_T("[GetUpdatesSuppressedTimes][Out of bounds][%x][%x][%x]"),
                 v.value().start_hour,
                 v.value().start_min,
                 v.value().duration_min));
    return E_UNEXPECTED;
  }

  v.UpdateFinal(UpdatesSuppressedTimes(), policy_status_value);
  *times = v.value();

  tm local_time = {};
  time.GetLocalTm(&local_time);
  int time_diff_minutes =
      (local_time.tm_hour - v.value().start_hour) * kMinPerHour +
      (local_time.tm_min - v.value().start_min);

  // Add 24 hours if `time_diff_minutes` is negative.
  if (time_diff_minutes < 0) {
    time_diff_minutes += 24 * kMinPerHour;
  }

  *are_updates_suppressed =
      time_diff_minutes < static_cast<int>(v.value().duration_min);
  OPT_LOG(L5, (_T("[GetUpdatesSuppressedTimes][v=%s][time=%s]")
               _T("[time_diff_minutes=%d][are_updates_suppressed=%d]"),
               v.ToString(), time.Format(L"%#c"),
               time_diff_minutes, *are_updates_suppressed));
  return S_OK;
}

bool ConfigManager::AreUpdatesSuppressedNow(const CTime& now) const {
  UpdatesSuppressedTimes times;
  bool are_updates_suppressed = false;

  HRESULT hr = GetUpdatesSuppressedTimes(now,
                                         &times,
                                         &are_updates_suppressed,
                                         NULL);
  return SUCCEEDED(hr) && are_updates_suppressed;
}

bool ConfigManager::CanInstallApp(const GUID& app_guid, bool is_machine) const {
  // Google Update should never be checking whether it can install itself.
  ASSERT1(!::IsEqualGUID(kGoopdateGuid, app_guid));

  auto policy = GetEffectivePolicyForAppInstalls(app_guid, NULL);
  return kPolicyDisabled != policy &&
         (is_machine || kPolicyEnabledMachineOnly != policy);
}

// Self-updates cannot be disabled.
bool ConfigManager::CanUpdateApp(const GUID& app_guid, bool is_manual) const {
  if (::IsEqualGUID(kGoopdateGuid, app_guid)) {
    return true;
  }

  const DWORD effective_policy = GetEffectivePolicyForAppUpdates(app_guid,
                                                                 NULL);
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

bool ConfigManager::ShouldVerifyPayloadAuthenticodeSignature() const {
#ifdef VERIFY_PAYLOAD_AUTHENTICODE_SIGNATURE
  DWORD disabled_in_registry = 0;
  RegKey::GetValue(MACHINE_REG_UPDATE_DEV,
                   kRegValueDisablePayloadAuthenticodeVerification,
                   &disabled_in_registry);
  return disabled_in_registry == 0;
#else
  return false;
#endif  // VERIFY_PAYLOAD_AUTHENTICODE_SIGNATURE
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

CString ConfigManager::GetDownloadPreferenceGroupPolicy(
    IPolicyStatusValue** policy_status_value) const {
  PolicyValue<CString> v;

  for (size_t i = 0; i != policies_.size(); ++i) {
    CString download_preference;
    HRESULT hr = policies_[i]->GetDownloadPreferenceGroupPolicy(
        &download_preference);
    if (SUCCEEDED(hr) && download_preference == kDownloadPreferenceCacheable) {
      v.Update(policies_[i]->IsManaged(),
               policies_[i]->source(),
               download_preference);
    }
  }

  v.UpdateFinal(CString(), policy_status_value);

  OPT_LOG(L5, (_T("[GetDownloadPreferenceGroupPolicy][%s]"), v.ToString()));

  return v.value();
}

#if defined(HAS_DEVICE_MANAGEMENT)

CString ConfigManager::GetCloudManagementEnrollmentToken() const {
  CString enrollment_token;
  HRESULT hr = RegKey::GetValue(kRegKeyCloudManagementGroupPolicy,
                                kRegValueEnrollmentToken, &enrollment_token);
  return (SUCCEEDED(hr) && IsUuid(enrollment_token)) ? enrollment_token :
                                                       CString();
}

bool ConfigManager::IsCloudManagementEnrollmentMandatory() const {
  DWORD is_mandatory = 0;
  HRESULT hr = RegKey::GetValue(kRegKeyCloudManagementGroupPolicy,
                                kRegValueEnrollmentMandatory, &is_mandatory);
  return SUCCEEDED(hr) && is_mandatory != 0;
}

#endif  // defined(HAS_DEVICE_MANAGEMENT)

}  // namespace omaha
