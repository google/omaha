// Copyright 2008-2010 Google Inc.
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

#include "omaha/goopdate/policy_status.h"
#include "omaha/base/const_object_names.h"
#include "omaha/base/constants.h"
#include "omaha/base/debug.h"
#include "omaha/base/logging.h"
#include "omaha/common/config_manager.h"

namespace omaha {

PolicyStatus::PolicyStatus() : StdMarshalInfo(true) {
  CORE_LOG(L6, (_T("[PolicyStatus::PolicyStatus]")));
}

PolicyStatus::~PolicyStatus() {
  CORE_LOG(L6, (_T("[PolicyStatus::~PolicyStatus]")));
}

// IPolicyStatus
// Global Update Policies
STDMETHODIMP PolicyStatus::get_lastCheckPeriodMinutes(DWORD* minutes) {
  ASSERT1(minutes);

  bool is_overridden = false;
  int period = ConfigManager::Instance()->GetLastCheckPeriodSec(&is_overridden);
  *minutes = static_cast<DWORD>(period) / kSecPerMin;
  return S_OK;
}

STDMETHODIMP PolicyStatus::get_updatesSuppressedTimes(
    DWORD* start_hour,
    DWORD* start_min,
    DWORD* duration_min,
    VARIANT_BOOL* are_updates_suppressed) {
  ASSERT1(start_hour);
  ASSERT1(start_min);
  ASSERT1(duration_min);
  ASSERT1(are_updates_suppressed);

  bool updates_suppressed = false;
  HRESULT hr = ConfigManager::Instance()->GetUpdatesSuppressedTimes(
                   start_hour,
                   start_min,
                   duration_min,
                   &updates_suppressed);
  if (FAILED(hr)) {
    return hr;
  }

  *are_updates_suppressed = updates_suppressed ? VARIANT_TRUE : VARIANT_FALSE;

  return S_OK;
}

STDMETHODIMP PolicyStatus::get_downloadPreferenceGroupPolicy(BSTR* pref) {
  ASSERT1(pref);

  *pref = ConfigManager::Instance()->GetDownloadPreferenceGroupPolicy()
              .AllocSysString();
  return S_OK;
}

STDMETHODIMP PolicyStatus::get_packageCacheSizeLimitMBytes(DWORD* limit) {
  ASSERT1(limit);

  *limit = static_cast<DWORD>(
      ConfigManager::Instance()->GetPackageCacheSizeLimitMBytes());
  return S_OK;
}

STDMETHODIMP PolicyStatus::get_packageCacheExpirationTimeDays(DWORD* days) {
  ASSERT1(days);

  *days = static_cast<DWORD>(
      ConfigManager::Instance()->GetPackageCacheExpirationTimeDays());
  return S_OK;
}

// Application Update Policies
STDMETHODIMP PolicyStatus::get_effectivePolicyForAppInstalls(BSTR app_id,
                                                             DWORD* policy) {
  ASSERT1(policy);

  GUID app_guid = GUID_NULL;
  HRESULT hr = StringToGuidSafe(app_id, &app_guid);
  if (FAILED(hr)) {
    return hr;
  }

  *policy = static_cast<DWORD>(
      ConfigManager::Instance()->GetEffectivePolicyForAppInstalls(app_guid));

  return S_OK;
}

STDMETHODIMP PolicyStatus::get_effectivePolicyForAppUpdates(BSTR app_id,
                                                            DWORD* policy) {
  ASSERT1(policy);

  GUID app_guid = GUID_NULL;
  HRESULT hr = StringToGuidSafe(app_id, &app_guid);
  if (FAILED(hr)) {
    return hr;
  }

  *policy = static_cast<DWORD>(
      ConfigManager::Instance()->GetEffectivePolicyForAppUpdates(app_guid));

  return S_OK;
}

STDMETHODIMP PolicyStatus::get_targetVersionPrefix(BSTR app_id, BSTR* prefix) {
  ASSERT1(prefix);

  GUID app_guid = GUID_NULL;
  HRESULT hr = StringToGuidSafe(app_id, &app_guid);
  if (FAILED(hr)) {
    return hr;
  }

  *prefix = ConfigManager::Instance()->GetTargetVersionPrefix(app_guid)
                .AllocSysString();

  return S_OK;
}

STDMETHODIMP PolicyStatus::get_isRollbackToTargetVersionAllowed(
  BSTR app_id,
  VARIANT_BOOL* rollback_allowed) {
  ASSERT1(rollback_allowed);

  GUID app_guid = GUID_NULL;
  HRESULT hr = StringToGuidSafe(app_id, &app_guid);
  if (FAILED(hr)) {
    return hr;
  }

  *rollback_allowed =
      ConfigManager::Instance()->IsRollbackToTargetVersionAllowed(app_guid) ?
          VARIANT_TRUE :
          VARIANT_FALSE;

  return S_OK;
}

}  // namespace omaha

