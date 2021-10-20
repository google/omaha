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

#include "omaha/goopdate/app_manager.h"

#include <cstdlib>
#include <algorithm>
#include <functional>
#include <map>
#include <memory>

#include "omaha/base/const_object_names.h"
#include "omaha/base/error.h"
#include "omaha/base/reg_key.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/time.h"
#include "omaha/base/utils.h"
#include "omaha/base/vistautil.h"
#include "omaha/common/app_registry_utils.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/common/oem_install_utils.h"
#include "omaha/goopdate/application_usage_data.h"
#include "omaha/goopdate/model.h"
#include "omaha/goopdate/server_resource.h"
#include "omaha/goopdate/string_formatter.h"

namespace omaha {

namespace {

const uint32 kInitialInstallTimeDiff = static_cast<uint32>(-1 * kSecondsPerDay);
const uint32 kInitialDayOfInstall = static_cast<uint32>(-1);
const uint32 kUnknownDayOfInstall = 0;

// Returns the number of days haven been passed since the given time.
// The parameter time is in the same format as C time() returns.
int GetNumberOfDaysSince(int time) {
  ASSERT1(time >= 0);
  const int now = Time64ToInt32(GetCurrent100NSTime());
  ASSERT1(now >= time);

  if (now < time) {
    // In case the client computer clock is adjusted in between.
    return 0;
  }
  return (now - time) / kSecondsPerDay;
}

// Enumerates all sub keys of the key and calls the function for each of them,
// ignoring errors to ensure all keys are processed.
HRESULT EnumerateSubKeys(const TCHAR* key_name,
                         std::function<void(const CString&)> fun) {
  RegKey client_key;
  HRESULT hr = client_key.Open(key_name, KEY_READ);
  if (FAILED(hr)) {
    return hr;
  }

  int num_sub_keys = client_key.GetSubkeyCount();
  for (int i = 0; i < num_sub_keys; ++i) {
    CString sub_key_name;
    hr = client_key.GetSubkeyNameAt(i, &sub_key_name);
    if (SUCCEEDED(hr)) {
      fun(sub_key_name);
    }
  }

  return S_OK;
}

}  // namespace


AppManager* AppManager::instance_ = NULL;

// We do not worry about contention on creation because only the Worker should
// create AppManager during its initialization.
HRESULT AppManager::CreateInstance(bool is_machine) {
  ASSERT1(!instance_);
  if (instance_) {
    return S_OK;
  }

  AppManager* instance(new AppManager(is_machine));
  if (!instance->InitializeRegistryLock()) {
    HRESULT hr(HRESULTFromLastError());
    delete instance;
    return hr;
  }

  instance_ = instance;
  return S_OK;
}

void AppManager::DeleteInstance() {
  delete instance_;
  instance_ = NULL;
}

AppManager* AppManager::Instance() {
  ASSERT1(instance_);
  return instance_;
}

HRESULT AppManager::ReadAppVersionNoLock(bool is_machine, const GUID& app_guid,
                                         CString* version) {
  ASSERT1(version);
  CORE_LOG(L2, (_T("[ReadAppVersionNoLock][%s]"), GuidToString(app_guid)));

  AppManager app_manager(is_machine);
  RegKey client_key;
  HRESULT hr = app_manager.OpenClientKey(app_guid, &client_key);
  if (FAILED(hr)) {
    return hr;
  }

  hr = client_key.GetValue(kRegValueProductVersion, version);
  if (FAILED(hr)) {
    return hr;
  }

  CORE_LOG(L3, (_T("[kRegValueProductVersion][%s]"), *version));
  return S_OK;
}

AppManager::AppManager(bool is_machine)
    : is_machine_(is_machine) {
  CORE_LOG(L3, (_T("[AppManager::AppManager][is_machine=%d]"), is_machine));
}

// App installers should use similar code to create a lock to acquire while
// modifying Omaha registry.
bool AppManager::InitializeRegistryLock() {
  NamedObjectAttributes lock_attr;
  GetNamedObjectAttributes(kRegistryAccessMutex, is_machine_, &lock_attr);
  return registry_access_lock_.InitializeWithSecAttr(lock_attr.name,
                                                     &lock_attr.sa);
}

// Vulnerable to a race condition with installers. To prevent this, acquire
// GetRegistryStableStateLock().
bool AppManager::IsAppRegistered(const GUID& app_guid) const {
  return IsAppRegistered(GuidToString(app_guid));
}

// Vulnerable to a race condition with installers. To prevent this, acquire
// GetRegistryStableStateLock().
bool AppManager::IsAppRegistered(const CString& app_id) const {
  bool is_registered = false;
  HRESULT hr = EnumerateSubKeys(
      ConfigManager::Instance()->registry_clients(is_machine_),
      [&](const CString& subkey) {
        if (subkey.CompareNoCase(app_id) == 0) {
          is_registered = true;
        }
      });

  if (FAILED(hr)) {
    return false;
  }

  return is_registered;
}

bool AppManager::IsAppUninstalled(const CString& app_id) const {
  GUID app_guid = {0};
  if (FAILED(StringToGuidSafe(app_id, &app_guid))) {
    ASSERT1(false);
    return false;
  }
  return IsAppUninstalled(app_guid);
}

// An app is considered uninstalled if:
//  * The app's Clients key does not exist AND
//  * The app's ClientState key exists and contains the pv value.
// We check for the pv key value in the ClientState to prevent Omaha from
// detecting the key created in the following scenarios as an uninstalled app.
//  * Per-machine apps may write dr to per-user Omaha's key. Per-user Omaha
//    must not detect this as an uninstalled app.
//  * Omaha may create the app's ClientState key and write values from the
//    metainstaller tag before running the installer, which creates the
//    Clients key.
bool AppManager::IsAppUninstalled(const GUID& app_guid) const {
  if (IsAppRegistered(app_guid)) {
    return false;
  }

  return RegKey::HasValue(GetClientStateKeyName(app_guid),
                          kRegValueProductVersion);
}

bool AppManager::IsAppOemInstalledAndEulaAccepted(const CString& app_id) const {
  GUID app_guid = GUID_NULL;
  if (FAILED(StringToGuidSafe(app_id, &app_guid))) {
    ASSERT1(false);
    return false;
  }

  if (IsAppUninstalled(app_guid)) {
    return false;
  }

  if (!app_registry_utils::IsAppEulaAccepted(is_machine_, app_id, false)) {
    CORE_LOG(L3, (_T("[EULA not accepted for app %s, its OEM ping not sent.]"),
                  app_id.GetString()));
    return false;
  }

  return RegKey::HasValue(GetClientStateKeyName(app_guid), kRegValueOemInstall);
}

HRESULT AppManager::RunRegistrationUpdateHook(const CString& app_id) const {
  GUID app_guid = GUID_NULL;
  HRESULT hr = StringToGuidSafe(app_id, &app_guid);
  if (FAILED(hr)) {
    return hr;
  }
  RegKey client_key;
  hr = OpenClientKey(app_guid, &client_key);
  if (FAILED(hr)) {
    return hr;
  }

  CString hook_clsid_str;
  hr = client_key.GetValue(kRegValueUpdateHookClsid, &hook_clsid_str);
  if (FAILED(hr)) {
    return hr;
  }
  GUID hook_clsid = GUID_NULL;
  hr = StringToGuidSafe(hook_clsid_str, &hook_clsid);
  if (FAILED(hr)) {
    return hr;
  }

  CORE_LOG(L3, (_T("[Update Hook Clsid][%s][%s]"), app_id, hook_clsid_str));

  CComPtr<IRegistrationUpdateHook> registration_hook;
  hr = registration_hook.CoCreateInstance(hook_clsid);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[IRegistrationUpdateHook CoCreate failed][0x%x]"), hr));
    return hr;
  }

  hr = registration_hook->UpdateRegistry(CComBSTR(app_id), is_machine_);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[registration_hook UpdateRegistry failed][0x%x]"), hr));
    return hr;
  }

  return S_OK;
}


// Vulnerable to a race condition with installers. To prevent this, hold
// GetRegistryStableStateLock() while calling this function and related
// functions, such as ReadAppPersistentData().
HRESULT AppManager::GetRegisteredApps(AppIdVector* app_ids) const {
  ASSERT1(app_ids);
  return EnumerateSubKeys(
      ConfigManager::Instance()->registry_clients(is_machine_),
      [&](const CString& subkey) {
        if (this->IsAppRegistered(subkey)) {
          app_ids->push_back(subkey);
        }
      });
}

// Vulnerable to a race condition with installers. To prevent this, acquire
// GetRegistryStableStateLock().
HRESULT AppManager::GetUninstalledApps(AppIdVector* app_ids) const {
  ASSERT1(app_ids);
  return EnumerateSubKeys(
      ConfigManager::Instance()->registry_client_state(is_machine_),
      [&](const CString& subkey) {
        if (this->IsAppUninstalled(subkey)) {
          app_ids->push_back(subkey);
        }
      });
}

HRESULT AppManager::GetOemInstalledAndEulaAcceptedApps(
    AppIdVector* app_ids) const {
  ASSERT1(app_ids);
  return EnumerateSubKeys(
      ConfigManager::Instance()->registry_client_state(is_machine_),
      [&](const CString& subkey) {
        if (this->IsAppOemInstalledAndEulaAccepted(subkey)) {
          app_ids->push_back(subkey);
        }
      });
}

// Vulnerable to a race condition with installers. We think this is acceptable.
// If there is a future requirement for greater consistency, acquire
// GetRegistryStableStateLock().
HRESULT AppManager::RunAllRegistrationUpdateHooks() const {
  const TCHAR* key(ConfigManager::Instance()->registry_clients(is_machine_));
  return EnumerateSubKeys(key,
    [&](const CString& subkey) {
      this->RunRegistrationUpdateHook(subkey);
    });
}

CString AppManager::GetClientKeyName(const GUID& app_guid) const {
  return app_registry_utils::GetAppClientsKey(is_machine_,
                                              GuidToString(app_guid));
}

CString AppManager::GetClientStateKeyName(const GUID& app_guid) const {
  return app_registry_utils::GetAppClientStateKey(is_machine_,
                                                  GuidToString(app_guid));
}

CString AppManager::GetClientStateMediumKeyName(const GUID& app_guid) const {
  ASSERT1(is_machine_);
  return app_registry_utils::GetAppClientStateMediumKey(is_machine_,
                                                        GuidToString(app_guid));
}

// Assumes the registry access lock is held.
HRESULT AppManager::OpenClientKey(const GUID& app_guid,
                                  RegKey* client_key) const {
  ASSERT1(client_key);
  return client_key->Open(GetClientKeyName(app_guid), KEY_READ);
}

// Assumes the registry access lock is held.
HRESULT AppManager::OpenClientStateKey(const GUID& app_guid,
                                       REGSAM sam_desired,
                                       RegKey* client_state_key) const {
  ASSERT1(client_state_key);
  CString key_name = GetClientStateKeyName(app_guid);
  return client_state_key->Open(key_name, sam_desired);
}

// Also creates the ClientStateMedium key for machine apps, ensuring it exists
// whenever ClientState exists.  Does not create ClientStateMedium for Omaha.
// This function is called for self-updates, so it must explicitly avoid this.
// Assumes the registry access lock is held.
HRESULT AppManager::CreateClientStateKey(const GUID& app_guid,
                                         RegKey* client_state_key) const {
  ASSERT1(client_state_key);
  // TODO(omaha3): Add GetOwner() to GLock & add this to Open() functions too.
  // ASSERT1(::GetCurrentThreadId() == registry_access_lock_.GetOwner());

  const CString key_name = GetClientStateKeyName(app_guid);
  HRESULT hr = client_state_key->Create(key_name);
  if (FAILED(hr)) {
    CORE_LOG(L3, (_T("[RegKey::Create failed][0x%08x]"), hr));
    return hr;
  }

  if (!is_machine_) {
    return S_OK;
  }

  if (::IsEqualGUID(kGoopdateGuid, app_guid)) {
    return S_OK;
  }

  const CString medium_key_name = GetClientStateMediumKeyName(app_guid);
  hr = RegKey::CreateKey(medium_key_name);
  if (FAILED(hr)) {
    CORE_LOG(L3, (_T("[RegKey::Create ClientStateMedium failed][0x%08x]"), hr));
    return hr;
  }

  return S_OK;
}

HRESULT AppManager::ReadAppDefinedAttributes(
    const CString& app_id, std::vector<StringPair>* attributes) const {
  ASSERT1(!app_id.IsEmpty());
  ASSERT1(attributes);
  ASSERT1(attributes->empty());

  CString base_key_name(
    is_machine_ ?
    ConfigManager::Instance()->machine_registry_client_state_medium() :
    ConfigManager::Instance()->registry_client_state(is_machine_));

  RegKey app_id_key;
  CString app_id_key_name = AppendRegKeyPath(base_key_name, app_id);
  HRESULT hr = app_id_key.Open(app_id_key_name, KEY_READ);
  if (FAILED(hr)) {
    if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
      hr = S_FALSE;
    }
    return hr;
  }

  hr = ReadAppDefinedAttributeValues(&app_id_key, attributes);
  if (FAILED(hr)) {
    return hr;
  }

  return ReadAppDefinedAttributeSubkeys(&app_id_key, attributes);
}

HRESULT AppManager::ReadAppDefinedAttributeValues(
    RegKey* app_id_key, std::vector<StringPair>* attributes) const {
  ASSERT1(app_id_key);
  ASSERT1(attributes);

  const int num_attributes = app_id_key->GetValueCount();

  for (int i = 0; i < num_attributes; ++i) {
    CString attribute_name;
    DWORD type(REG_SZ);

    HRESULT hr = app_id_key->GetValueNameAt(i, &attribute_name, &type);
    attribute_name.MakeLower();
    if (FAILED(hr)) {
      OPT_LOG(LE, (_T("[ReadAppDefinedAttributeValues][Failed read Attribute]")
                   _T("[%s][%#x]"), attribute_name, hr));
      continue;
    }

    if (!String_StartsWith(attribute_name, kRegValueAppDefinedPrefix, false)) {
      continue;
    }

    if (type != REG_SZ) {
      OPT_LOG(LE, (_T("[ReadAppDefinedAttributeValues][Type needs to be")
                   _T(" REG_SZ[%s][%#x]"), attribute_name, type));
      continue;
    }

    CString attribute_value;
    hr = app_id_key->GetValue(attribute_name, &attribute_value);
    if (FAILED(hr)) {
      continue;
    }

    attributes->push_back(std::make_pair(attribute_name, attribute_value));
  }

  return S_OK;
}

HRESULT AppManager::ReadAppDefinedAttributeSubkeys(
    RegKey* app_id_key, std::vector<StringPair>* attributes) const {
  ASSERT1(app_id_key);
  ASSERT1(attributes);

  const int num_subkeys = app_id_key->GetSubkeyCount();

  for (int i = 0; i < num_subkeys; ++i) {
    CString attribute_subkey_name;
    HRESULT hr = app_id_key->GetSubkeyNameAt(i, &attribute_subkey_name);
    attribute_subkey_name.MakeLower();
    if (FAILED(hr)) {
      continue;
    }

    if (!String_StartsWith(attribute_subkey_name,
                           kRegValueAppDefinedPrefix,
                           false)) {
      continue;
    }


    RegKey attribute_subkey;
    hr = attribute_subkey.Open(app_id_key->Key(),
                               attribute_subkey_name,
                               KEY_READ);
    if (FAILED(hr)) {
      continue;
    }

    CString value;
    hr = attribute_subkey.GetValue(kRegValueAppDefinedAggregate, &value);
    if (FAILED(hr)) {
      continue;
    }

    if (value.CompareNoCase(kRegValueAppDefinedAggregateSum)) {
      OPT_LOG(LE, (_T("[ReadAppDefinedAttributeSubkeys]['%s' aggregate")
                   _T(" not supported]"), value));
      continue;
    }

    const int num_values = attribute_subkey.GetValueCount();
    DWORD attribute_sum = 0;

    for (int j = 0; j < num_values; ++j) {
      CString value_name;
      DWORD type(REG_DWORD);
      hr = attribute_subkey.GetValueNameAt(j, &value_name, &type);
      if (FAILED(hr)) {
        continue;
      }

      if (type != REG_DWORD) {
        OPT_LOG(LE, (_T("[ReadAppDefinedAttributeSubkeys][Type needs to be")
                     _T(" DWORD[%s]"), value_name));
        continue;
      }

      DWORD val = 0;
      hr = attribute_subkey.GetValue(value_name, &val);
      if (FAILED(hr)) {
        continue;
      }

      attribute_sum += val;
    }


    CString attribute_value;
    SafeCStringFormat(&attribute_value, _T("%d"), attribute_sum);
    attributes->push_back(std::make_pair(attribute_subkey_name,
                                         attribute_value));
  }

  return S_OK;
}

// Reads the following values from the registry:
//  Clients key
//    pv
//    lang
//    name
//  ClientState key
//    lang (if not present in Clients)
//    ap
//    tttoken
//    cohort
//    cohorthint
//    cohortname
//    iid
//    brand
//    client
//    experiment
//    (referral is intentionally not read)
//    InstallTime (converted to diff)
//    oeminstall
//    ping_freshness
//  ClientState and ClientStateMedium key
//    eulaaccepted
//    usagestats
//  ClientState key in HKCU/HKLM/Low integrity
//    did run
//
// app_guid_ is set to the app_guid argument.
// Note: pv is not read from ClientState into app_data. It's
// presence is checked for an uninstall
// TODO(omaha3): We will need to get ClientState's pv when reporting uninstalls.
// Note: If the application is uninstalled, the Clients key may not exist.
HRESULT AppManager::ReadAppPersistentData(App* app) {
  ASSERT1(app);

  const GUID& app_guid = app->app_guid();
  const CString& app_guid_string = app->app_guid_string();

  CORE_LOG(L2, (_T("[AppManager::ReadAppPersistentData][%s]"),
                app_guid_string));

  ASSERT1(app->model()->IsLockedByCaller());

  __mutexScope(registry_access_lock_);

  const bool is_eula_accepted =
      app_registry_utils::IsAppEulaAccepted(is_machine_,
                                            app_guid_string,
                                            false);
  app->is_eula_accepted_ = is_eula_accepted ? TRISTATE_TRUE : TRISTATE_FALSE;

  bool client_key_exists = false;
  RegKey client_key;
  HRESULT hr = OpenClientKey(app_guid, &client_key);
  if (SUCCEEDED(hr)) {
    client_key_exists = true;

    CString version;
    hr = client_key.GetValue(kRegValueProductVersion, &version);
    CORE_LOG(L3, (_T("[AppManager::ReadAppPersistentData]")
                  _T("[%s][version=%s]"), app_guid_string, version));
    if (FAILED(hr)) {
      return hr;
    }

    app->current_version()->set_version(version);

    // Language and name might not be written by installer, so ignore failures.
    client_key.GetValue(kRegValueLanguage, &app->language_);
    client_key.GetValue(kRegValueAppName, &app->display_name_);
  }

  // Ensure there is a valid display name.
  if (app->display_name_.IsEmpty()) {
    StringFormatter formatter(app->app_bundle()->display_language());

    CString company_name;
    VERIFY_SUCCEEDED(formatter.LoadString(IDS_FRIENDLY_COMPANY_NAME,
                                           &company_name));

    VERIFY_SUCCEEDED(formatter.FormatMessage(&app->display_name_,
                                              IDS_DEFAULT_APP_DISPLAY_NAME,
                                              company_name));
  }

  // If ClientState registry key doesn't exist, the function could return.
  // Before opening the key, set days_since_last* and set_day_of* to -1, which
  // is the default value if reg key doesn't exist. If later we find that the
  // values are readable, new values will overwrite current ones.
  app->set_days_since_last_active_ping(-1);
  app->set_days_since_last_roll_call(-1);
  app->set_day_of_last_activity(-1);
  app->set_day_of_last_roll_call(-1);

  // The following do not rely on client_state_key, so check them before
  // possibly returning if OpenClientStateKey fails.

  // Reads the did run value.
  ApplicationUsageData app_usage(is_machine_, vista_util::IsVistaOrLater());
  app_usage.ReadDidRun(app_guid_string);

  // Sets did_run regardless of the return value of ReadDidRun above. If read
  // fails, active_state() should return ACTIVE_UNKNOWN which is intented.
  app->did_run_ = app_usage.active_state();

  // TODO(omaha3): Consider moving GetInstallTimeDiffSec() up here. Be careful
  // that the results when ClientState does not exist are desirable. See the
  // comments near that function and above set_days_since_last_active_ping call.

  RegKey client_state_key;
  hr = OpenClientStateKey(app_guid, KEY_READ, &client_state_key);
  if (FAILED(hr)) {
    // It is possible that the client state key has not yet been populated.
    // In this case just return the information that we have gathered thus far.
    // However if both keys do not exist, then we are doing something wrong.
    CORE_LOG(LW, (_T("[AppManager::ReadAppPersistentData - No ClientState]")));
    if (client_key_exists) {
      return S_OK;
    } else {
      return hr;
    }
  }

  // Read language from ClientState key if it was not found in the Clients key.
  if (app->language().IsEmpty()) {
    client_state_key.GetValue(kRegValueLanguage, &app->language_);
  }

  VERIFY_SUCCEEDED(ReadAppDefinedAttributes(GuidToString(app_guid),
                                             &app->app_defined_attributes_));

  client_state_key.GetValue(kRegValueAdditionalParams, &app->ap_);
  client_state_key.GetValue(kRegValueTTToken, &app->tt_token_);

  ReadCohort(app_guid, &app->cohort_);

  CString iid;
  client_state_key.GetValue(kRegValueInstallationId, &iid);
  GUID iid_guid;
  if (SUCCEEDED(StringToGuidSafe(iid, &iid_guid))) {
    app->iid_ = iid_guid;
  }

  client_state_key.GetValue(kRegValueBrandCode, &app->brand_code_);
  ASSERT1(app->brand_code_.GetLength() <= kBrandIdLength);
  client_state_key.GetValue(kRegValueClientId, &app->client_id_);

  // We do not need the referral_id.

  DWORD last_active_ping_sec(0);
  if (SUCCEEDED(client_state_key.GetValue(kRegValueActivePingDayStartSec,
                                          &last_active_ping_sec))) {
    int days_since_last_active_ping =
        GetNumberOfDaysSince(static_cast<int32>(last_active_ping_sec));
    app->set_days_since_last_active_ping(days_since_last_active_ping);
  }

  DWORD last_roll_call_sec(0);
  if (SUCCEEDED(client_state_key.GetValue(kRegValueRollCallDayStartSec,
                                          &last_roll_call_sec))) {
    int days_since_last_roll_call =
        GetNumberOfDaysSince(static_cast<int32>(last_roll_call_sec));
    app->set_days_since_last_roll_call(days_since_last_roll_call);
  }

  app->install_time_diff_sec_ = GetInstallTimeDiffSec(app_guid);
  // Generally GetInstallTimeDiffSec() shouldn't return kInitialInstallTimeDiff
  // here. The only exception is in the unexpected case when ClientState exists
  // without a pv.
  ASSERT1((app->install_time_diff_sec_ != kInitialInstallTimeDiff) ||
          !RegKey::HasValue(GetClientStateKeyName(app_guid),
                            kRegValueProductVersion));

  // For apps installed before day_of_install is implemented, skip sending
  // day_of_last* one more time (hence resets the values to 0). Once client
  // gets |daynum| from server's response, it can send day_of_last* in
  // subsequent pings.
  if (app->days_since_last_active_ping() != -1) {
    app->set_day_of_last_activity(0);
  }
  if (app->days_since_last_roll_call() != -1) {
    app->set_day_of_last_roll_call(0);
  }

  DWORD day_of_last_activity(0);
  if (SUCCEEDED(client_state_key.GetValue(kRegValueDayOfLastActivity,
                                          &day_of_last_activity))) {
    app->set_day_of_last_activity(day_of_last_activity);
  }

  DWORD day_of_last_roll_call(0);
  if (SUCCEEDED(client_state_key.GetValue(kRegValueDayOfLastRollCall,
                                          &day_of_last_roll_call))) {
    app->set_day_of_last_roll_call(day_of_last_roll_call);
  }

  app->day_of_install_ = GetDayOfInstall(app_guid);

  CString ping_freshness;
  if (SUCCEEDED(client_state_key.GetValue(kRegValuePingFreshness,
                                          &ping_freshness))) {
    app->ping_freshness_ = ping_freshness;
  }

  app->usage_stats_enable_ = GetAppUsageStatsEnabled(app_guid);

  return S_OK;
}

void AppManager::ReadFirstInstallAppVersion(App* app) {
  ASSERT1(app);

  CString pv;
  app_registry_utils::GetAppVersion(is_machine_, app->app_guid_string(), &pv);

  if (!pv.IsEmpty()) {
    // Because this is an overinstall, we always want the latest version served
    // back from the server, so prefix a negative sign to allow the server to
    // ignore the version for comparisons.
    pv.Insert(0, _T('-'));
    app->current_version()->set_version(pv);
  }
}

void AppManager::ReadAppInstallTimeDiff(App* app) {
  ASSERT1(app);
  app->install_time_diff_sec_ = GetInstallTimeDiffSec(app->app_guid());
}

void AppManager::ReadDayOfInstall(App* app) {
  ASSERT1(app);
  app->day_of_install_ = GetDayOfInstall(app->app_guid());
}

// Calls ReadAppPersistentData() to populate app and adds the following values
// specific to uninstalled apps:
//  ClientState key
//    pv:  set as current_version()->version
//
// Since this is an uninstalled app, values from the Clients key should not be
// populated.
HRESULT AppManager::ReadUninstalledAppPersistentData(App* app) {
  ASSERT1(app);
  ASSERT1(!IsAppRegistered(app->app_guid_string()));

  HRESULT hr = ReadAppPersistentData(app);
  if (FAILED(hr)) {
    return hr;
  }

  ASSERT1(app->current_version()->version().IsEmpty());

  RegKey client_state_key;
  hr = OpenClientStateKey(app->app_guid(), KEY_READ, &client_state_key);
  ASSERT(SUCCEEDED(hr), (_T("Uninstalled apps have a ClientState key.")));

  CString version;
  hr = client_state_key.GetValue(kRegValueProductVersion, &version);
  CORE_LOG(L3, (_T("[AppManager::ReadAppPersistentData]")
                _T("[%s][uninstalled version=%s]"),
                app->app_guid_string(), version));
  ASSERT(SUCCEEDED(hr), (_T("Uninstalled apps have a pv.")));
  app->current_version()->set_version(version);

  return S_OK;
}

// Sets the following values in the app's ClientState, to make them available to
// the installer:
//    lang
//    ap
//    brand (in SetAppBranding)
//    client (in SetAppBranding)
//    experiment
//    referral (in SetAppBranding)
//    InstallTime (in SetAppBranding; converted from diff)
//    oeminstall (if appropriate)
//    eulaaccepted (set/deleted)
//    browser
//    usagestats
//    ping freshness
// Sets eulaaccepted=0 if the app is not already registered and the app's EULA
// has not been accepted. Deletes eulaaccepted if the EULA has been accepted.
// Only call for initial or over-installs. Do not call for updates to avoid
// mistakenly replacing data, such as the application's language, and causing
// unexpected changes to the app during a silent update.
HRESULT AppManager::WritePreInstallData(const App& app) {
  CORE_LOG(L2, (_T("[AppManager::WritePreInstallData][%s]"),
                app.app_guid_string()));

  ASSERT1(app.app_bundle()->is_machine() == is_machine_);

  ASSERT1(IsRegistryStableStateLockedByCaller());
  __mutexScope(registry_access_lock_);

  RegKey client_state_key;
  HRESULT hr = CreateClientStateKey(app.app_guid(), &client_state_key);
  if (FAILED(hr)) {
    return hr;
  }

  // Initialize the ping freshness, if needed. For installs and over-installs,
  // ping freshness must be available before sending a completion ping.
  // Otherwise, the completion ping contains no ping_freshness, which makes
  // impossible to do an accurate user count in the presence of system reimage.
  if (!client_state_key.HasValue(kRegValuePingFreshness)) {
    GUID ping_freshness = GUID_NULL;
    VERIFY_SUCCEEDED(::CoCreateGuid(&ping_freshness));
    client_state_key.SetValue(kRegValuePingFreshness,
                               GuidToString(ping_freshness));
  }

  if (app.is_eula_accepted()) {
    hr = app_registry_utils::ClearAppEulaNotAccepted(is_machine_,
                                                     app.app_guid_string());
  } else {
    if (!IsAppRegistered(app.app_guid())) {
      hr = app_registry_utils::SetAppEulaNotAccepted(is_machine_,
                                                     app.app_guid_string());
    }
  }
  if (FAILED(hr)) {
    return hr;
  }

  if (!app.language().IsEmpty()) {
    VERIFY_SUCCEEDED(client_state_key.SetValue(kRegValueLanguage,
                                                app.language()));
  }

  if (app.ap().IsEmpty()) {
    VERIFY_SUCCEEDED(client_state_key.DeleteValue(kRegValueAdditionalParams));
  } else {
    VERIFY_SUCCEEDED(client_state_key.SetValue(kRegValueAdditionalParams,
                                                app.ap()));
  }

  CString state_key_path = GetClientStateKeyName(app.app_guid());
  VERIFY_SUCCEEDED(app_registry_utils::SetAppBranding(
      state_key_path,
      app.brand_code(),
      app.client_id(),
      app.referral_id(),
      app.day_of_last_response()));

  if (oem_install_utils::IsOemInstalling(is_machine_)) {
    ASSERT1(is_machine_);
    VERIFY_SUCCEEDED(client_state_key.SetValue(kRegValueOemInstall, _T("1")));
  }

  if (BROWSER_UNKNOWN == app.browser_type()) {
    VERIFY_SUCCEEDED(client_state_key.DeleteValue(kRegValueBrowser));
  } else {
    DWORD browser_type = app.browser_type();
    VERIFY_SUCCEEDED(client_state_key.SetValue(kRegValueBrowser,
                                                browser_type));
  }

  if (TRISTATE_NONE != app.usage_stats_enable()) {
    VERIFY_SUCCEEDED(app_registry_utils::SetUsageStatsEnable(
                          is_machine_,
                          app.app_guid_string(),
                          app.usage_stats_enable()));
  }

  return S_OK;
}

// All values are optional.
void AppManager::ReadInstallerResultApiValues(
    const GUID& app_guid,
    InstallerResult* installer_result,
    DWORD* installer_error,
    DWORD* installer_extra_code1,
    CString* installer_result_uistring,
    CString* installer_success_launch_cmd) {
  ASSERT1(installer_result);
  ASSERT1(installer_error);
  ASSERT1(installer_extra_code1);
  ASSERT1(installer_result_uistring);
  ASSERT1(installer_success_launch_cmd);

  __mutexScope(registry_access_lock_);

  RegKey client_state_key;
  HRESULT hr = OpenClientStateKey(app_guid, KEY_READ, &client_state_key);
  if (FAILED(hr)) {
    return;
  }

  if (SUCCEEDED(client_state_key.GetValue(
                    kRegValueInstallerResult,
                    reinterpret_cast<DWORD*>(installer_result)))) {
    CORE_LOG(L1, (_T("[InstallerResult in registry][%u]"), *installer_result));
  }
  if (*installer_result >= INSTALLER_RESULT_MAX) {
    CORE_LOG(LW, (_T("[Unsupported InstallerResult value]")));
    *installer_result = INSTALLER_RESULT_DEFAULT;
  }

  if (SUCCEEDED(client_state_key.GetValue(kRegValueInstallerError,
                                          installer_error))) {
    CORE_LOG(L1, (_T("[InstallerError in registry][%u]"), *installer_error));
  }

  if (SUCCEEDED(client_state_key.GetValue(kRegValueInstallerExtraCode1,
                                          installer_extra_code1))) {
    CORE_LOG(L1, (_T("[InstallerExtraCode1 in registry][%u]"),
        *installer_extra_code1));
  }

  if (SUCCEEDED(client_state_key.GetValue(kRegValueInstallerResultUIString,
                                          installer_result_uistring))) {
    CORE_LOG(L1, (_T("[InstallerResultUIString in registry][%s]"),
        *installer_result_uistring));
  }

  if (SUCCEEDED(client_state_key.GetValue(
                    kRegValueInstallerSuccessLaunchCmdLine,
                    installer_success_launch_cmd))) {
    CORE_LOG(L1, (_T("[InstallerSuccessLaunchCmdLine in registry][%s]"),
        *installer_success_launch_cmd));
  }

  ClearInstallerResultApiValues(app_guid);
}

void AppManager::ClearInstallerResultApiValues(const GUID& app_guid) {
  const CString client_state_key_name = GetClientStateKeyName(app_guid);
  const CString update_key_name =
      ConfigManager::Instance()->registry_update(is_machine_);

  ASSERT1(IsRegistryStableStateLockedByCaller());
  __mutexScope(registry_access_lock_);

  // Delete the old LastXXX values.  These may not exist, so don't care if they
  // fail.
  RegKey::DeleteValue(client_state_key_name,
                      kRegValueLastInstallerResult);
  RegKey::DeleteValue(client_state_key_name,
                      kRegValueLastInstallerResultUIString);
  RegKey::DeleteValue(client_state_key_name,
                      kRegValueLastInstallerError);
  RegKey::DeleteValue(client_state_key_name,
                      kRegValueLastInstallerExtraCode1);
  RegKey::DeleteValue(client_state_key_name,
                      kRegValueLastInstallerSuccessLaunchCmdLine);

  // Also delete any values from Google\Update.
  // TODO(Omaha): This is a temporary fix for bug 1539293. See TODO below.
  RegKey::DeleteValue(update_key_name,
                      kRegValueLastInstallerResult);
  RegKey::DeleteValue(update_key_name,
                      kRegValueLastInstallerResultUIString);
  RegKey::DeleteValue(update_key_name,
                      kRegValueLastInstallerError);
  RegKey::DeleteValue(update_key_name,
                      kRegValueLastInstallerExtraCode1);
  RegKey::DeleteValue(update_key_name,
                      kRegValueLastInstallerSuccessLaunchCmdLine);

  // Rename current InstallerResultXXX values to LastXXX.
  RegKey::RenameValue(client_state_key_name,
                      kRegValueInstallerResult,
                      kRegValueLastInstallerResult);
  RegKey::RenameValue(client_state_key_name,
                      kRegValueInstallerError,
                      kRegValueLastInstallerError);
  RegKey::RenameValue(client_state_key_name,
                      kRegValueInstallerExtraCode1,
                      kRegValueLastInstallerExtraCode1);
  RegKey::RenameValue(client_state_key_name,
                      kRegValueInstallerResultUIString,
                      kRegValueLastInstallerResultUIString);
  RegKey::RenameValue(client_state_key_name,
                      kRegValueInstallerSuccessLaunchCmdLine,
                      kRegValueLastInstallerSuccessLaunchCmdLine);

  // Copy over to the Google\Update key.
  // TODO(Omaha3): This is a temporary fix for bug 1539293. Once Pack V2 is
  // deprecated (Pack stops taking offline installers for new versions of
  // Omaha apps), remove this. (It might be useful to leave the CopyValue calls
  // in DEBUG builds only.)
  RegKey::CopyValue(client_state_key_name,
                    update_key_name,
                    kRegValueLastInstallerResult);
  RegKey::CopyValue(client_state_key_name,
                    update_key_name,
                    kRegValueLastInstallerError);
  RegKey::CopyValue(client_state_key_name,
                    update_key_name,
                    kRegValueLastInstallerExtraCode1);
  RegKey::CopyValue(client_state_key_name,
                    update_key_name,
                    kRegValueLastInstallerResultUIString);
  RegKey::CopyValue(client_state_key_name,
                    update_key_name,
                    kRegValueLastInstallerSuccessLaunchCmdLine);
}

// Reads the following values from Clients:
//    pv
//    lang (if present)
// name is not read. TODO(omaha3): May change if we persist name in registry.
HRESULT AppManager::ReadInstallerRegistrationValues(App* app) {
  ASSERT1(app);

  const CString& app_guid_string = app->app_guid_string();

  CORE_LOG(L2, (_T("[AppManager::ReadInstallerRegistrationValues][%s]"),
                app_guid_string));

  ASSERT1(app->model()->IsLockedByCaller());

  __mutexScope(registry_access_lock_);

  RegKey client_key;
  if (FAILED(OpenClientKey(app->app_guid(), &client_key))) {
    OPT_LOG(LE, (_T("[Installer did not create key][%s]"), app_guid_string));
    return GOOPDATEINSTALL_E_INSTALLER_DID_NOT_WRITE_CLIENTS_KEY;
  }

  CString version;
  if (FAILED(client_key.GetValue(kRegValueProductVersion, &version))) {
    OPT_LOG(LE, (_T("[Installer did not register][%s]"), app_guid_string));
    return GOOPDATEINSTALL_E_INSTALLER_DID_NOT_WRITE_CLIENTS_KEY;
  }

  if (version.IsEmpty()) {
    OPT_LOG(LE, (_T("[Installer did not write version][%s]"), app_guid_string));
    return GOOPDATEINSTALL_E_INSTALLER_DID_NOT_WRITE_CLIENTS_KEY;
  }

  app->next_version()->set_version(version);

  CString language;
  if (SUCCEEDED(client_key.GetValue(kRegValueLanguage, &language))) {
    app->language_ = language;
  }

  return S_OK;
}

// Writes tttoken and updates relevant stats.
void AppManager::PersistSuccessfulUpdateCheckResponse(
    const App& app,
    bool is_update_available) {
  CORE_LOG(L2, (_T("[AppManager::PersistSuccessfulUpdateCheckResponse]")
                _T("[%s][%d]"), app.app_guid_string(), is_update_available));
  __mutexScope(registry_access_lock_);

  VERIFY_SUCCEEDED(SetTTToken(app));

  VERIFY_SUCCEEDED(WriteCohort(app));

  const CString client_state_key = GetClientStateKeyName(app.app_guid());

  if (is_update_available) {
    if (app.error_code() == GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY) {
      // The error indicates is_update and updates are disabled by policy.
      ASSERT1(app.is_update());
      app_registry_utils::ClearUpdateAvailableStats(client_state_key);
    } else if (app.is_update()) {
      // Only record an update available event for updates.
      // We have other mechanisms, including IID, to track install success.
      UpdateUpdateAvailableStats(app.app_guid());
    }
  } else {
    app_registry_utils::ClearUpdateAvailableStats(client_state_key);
    app_registry_utils::PersistSuccessfulUpdateCheck(client_state_key);
  }
}

// Writes the following values to the ClientState key:
//    pv (should be value written by installer in Clients key)
//    lang (should be value written by installer in Clients key)
//    iid (set/deleted)
//
// Does not write the following values because they were set by
// WritePreInstallData() and would not have changed during installation unless
// modified directly by the app installer.
//    ap
//    brand
//    client
//    experiment
//    referral
//    InstallTime (converted from diff)
//    oeminstall
//    eulaaccepted
//    browser
//    usagestats
// TODO(omaha3): Maybe we should delete referral at this point. Ask Chrome.
//
// Other values, such as tttoken were set after the update check.
//
// The caller is responsible for modifying the values in app_data as
// appropriate, including:
//   * Updating values in app_data to reflect installer's values (pv and lang)
//   * Clearing iid if appropriate.
//   * Clearing the did run value. TODO(omaha3): Depends on TODO below.
void AppManager::PersistSuccessfulInstall(const App& app) {
  CORE_LOG(L2, (_T("[AppManager::PersistSuccessfulInstall][%s]"),
                app.app_guid_string()));

  ASSERT1(IsRegistryStableStateLockedByCaller());
  __mutexScope(registry_access_lock_);

  ASSERT1(!::IsEqualGUID(kGoopdateGuid, app.app_guid()));

  RegKey client_state_key;
  VERIFY_SUCCEEDED(CreateClientStateKey(app.app_guid(), &client_state_key));

  VERIFY_SUCCEEDED(client_state_key.SetValue(kRegValueProductVersion,
                                              app.next_version()->version()));

  if (!app.language().IsEmpty()) {
    VERIFY_SUCCEEDED(client_state_key.SetValue(kRegValueLanguage,
                                                app.language()));
  }

  if (::IsEqualGUID(app.iid(), GUID_NULL)) {
    VERIFY_SUCCEEDED(client_state_key.DeleteValue(kRegValueInstallationId));
  } else {
    VERIFY_SUCCEEDED(client_state_key.SetValue(
                          kRegValueInstallationId,
                          GuidToString(app.iid())));
  }

  const CString client_state_key_path = GetClientStateKeyName(app.app_guid());
  app_registry_utils::PersistSuccessfulInstall(client_state_key_path,
                                               app.is_update(),
                                               false);  // TODO(omaha3): offline
}

CString AppManager::GetCurrentStateKeyName(const CString& app_guid) const {
  const CString base_key_name(ConfigManager::Instance()->registry_client_state(
      is_machine_));
  const CString app_id_key_name = AppendRegKeyPath(base_key_name, app_guid);
  return AppendRegKeyPath(app_id_key_name, kRegSubkeyCurrentState);
}

HRESULT AppManager::ResetCurrentStateKey(const CString& app_guid) {
  return RegKey::DeleteKey(GetCurrentStateKeyName(app_guid));
}

HRESULT AppManager::WriteStateValue(const App& app, CurrentState state_value) {
  CORE_LOG(L2, (_T("[AppManager::WriteStateValue][%s]"),
                app.app_guid_string()));

  return RegKey::SetValue(GetCurrentStateKeyName(app.app_guid_string()),
                          kRegValueStateValue,
                          static_cast<DWORD>(state_value));
}

HRESULT AppManager::WriteDownloadProgress(const App& app,
                                          uint64 bytes_downloaded,
                                          uint64 bytes_total,
                                          LONG download_time_remaining_ms) {
  CORE_LOG(L2, (_T("[AppManager::WriteDownloadProgress][%s]"),
                app.app_guid_string()));

  if (bytes_total <= 0) {
    return E_INVALIDARG;
  }

  int download_progress_percentage =
      static_cast<int>(100ULL * bytes_downloaded / bytes_total);

  const CString current_state_key_name(
      GetCurrentStateKeyName(app.app_guid_string()));
  HRESULT hr = RegKey::SetValue(current_state_key_name,
                                kRegValueDownloadTimeRemainingMs,
                                static_cast<DWORD>(download_time_remaining_ms));
  if (FAILED(hr)) {
    return hr;
  }

  return RegKey::SetValue(current_state_key_name,
                          kRegValueDownloadProgressPercent,
                          static_cast<DWORD>(download_progress_percentage));
}

HRESULT AppManager::WriteInstallProgress(const App& app,
                                         LONG install_progress_percentage,
                                         LONG install_time_remaining_ms) {
  CORE_LOG(L2, (_T("[AppManager::WriteInstallProgress][%s]"),
                app.app_guid_string()));

  const CString current_state_key_name(
      GetCurrentStateKeyName(app.app_guid_string()));
  HRESULT hr = RegKey::SetValue(current_state_key_name,
                                kRegValueInstallTimeRemainingMs,
                                static_cast<DWORD>(install_time_remaining_ms));
  if (FAILED(hr)) {
    return hr;
  }

  return RegKey::SetValue(current_state_key_name,
                          kRegValueInstallProgressPercent,
                          static_cast<DWORD>(install_progress_percentage));
}

HRESULT AppManager::SynchronizeClientState(const GUID& app_guid) {
  __mutexScope(registry_access_lock_);

  RegKey client_key;
  HRESULT hr = OpenClientKey(app_guid, &client_key);
  if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
    return S_OK;
  }
  if (FAILED(hr)) {
    return hr;
  }

  RegKey client_state_key;
  hr = CreateClientStateKey(app_guid, &client_state_key);
  if (FAILED(hr)) {
    return hr;
  }

  CString version;
  client_key.GetValue(kRegValueProductVersion, &version);
  if (FAILED(hr)) {
    return hr;
  }
  hr = client_state_key.SetValue(kRegValueProductVersion, version);
  if (FAILED(hr)) {
    return hr;
  }

  CString language;
  client_key.GetValue(kRegValueLanguage, &language);
  if (!language.IsEmpty()) {
    return client_state_key.SetValue(kRegValueLanguage, language);
  }

  return S_OK;
}

// TODO(omaha3): tttoken is not currently read from the server response.
// TODO(omaha3): When implementing offline, we must make sure that the tttoken
// is not deleted by the offline response processing.
// TODO(omaha3): Having the parser write the server's token to the same member
// that is used for the value from the tag exposes this value to the COM setter.
// It would be nice to avoid that, possibly by only allowing that setter to work
// in certain states.
HRESULT AppManager::SetTTToken(const App& app) const {
  CORE_LOG(L3, (_T("[AppManager::SetTTToken][token=%s]"), app.tt_token()));

  __mutexScope(registry_access_lock_);

  RegKey client_state_key;
  HRESULT hr = CreateClientStateKey(app.app_guid(), &client_state_key);
  if (FAILED(hr)) {
    return hr;
  }

  if (app.tt_token().IsEmpty()) {
    return client_state_key.DeleteValue(kRegValueTTToken);
  } else {
    return client_state_key.SetValue(kRegValueTTToken, app.tt_token());
  }
}

HRESULT AppManager::DeleteCohortKey(const GUID& app_guid) const {
  return app_registry_utils::DeleteCohortKey(is_machine_,
                                             GuidToString(app_guid));
}

HRESULT AppManager::ReadCohort(const GUID& app_guid, Cohort * cohort) const {
  CORE_LOG(L3, (_T("[AppManager::ReadCohort][%s]"), GuidToString(app_guid)));

  ASSERT1(cohort);

  return app_registry_utils::ReadCohort(is_machine_,
                                        GuidToString(app_guid),
                                        cohort);
}

HRESULT AppManager::WriteCohort(const App& app) const {
  CORE_LOG(L3, (_T("[AppManager::WriteCohort][%s]"), app.cohort().cohort));

  __mutexScope(registry_access_lock_);

  return app_registry_utils::WriteCohort(is_machine_,
                                         app.app_guid_string(),
                                         app.cohort());
}

void AppManager::ClearOemInstalled(const AppIdVector& app_ids) {
  __mutexScope(registry_access_lock_);

  AppIdVector::const_iterator it;
  for (it = app_ids.begin(); it != app_ids.end(); ++it) {
    ASSERT1(IsAppOemInstalledAndEulaAccepted(*it));
    RegKey state_key;

    GUID app_guid = GUID_NULL;
    HRESULT hr = StringToGuidSafe(*it, &app_guid);
    if (FAILED(hr)) {
      continue;
    }

    hr = OpenClientStateKey(app_guid, KEY_ALL_ACCESS, &state_key);
    if (FAILED(hr)) {
      continue;
    }

    VERIFY_SUCCEEDED(state_key.DeleteValue(kRegValueOemInstall));

    // The current time is close to when OEM activation has happened. Treat the
    // current time as the real install time by resetting InstallTime.
    const DWORD now = Time64ToInt32(GetCurrent100NSTime());
    VERIFY_SUCCEEDED(state_key.SetValue(kRegValueInstallTimeSec, now));
    VERIFY_SUCCEEDED(app_registry_utils::SetInitialDayOfValues(
        GetClientStateKeyName(app_guid), -1));
  }
}

void AppManager::UpdateUpdateAvailableStats(const GUID& app_guid) const {
  __mutexScope(registry_access_lock_);

  RegKey state_key;
  HRESULT hr = CreateClientStateKey(app_guid, &state_key);
  if (FAILED(hr)) {
    ASSERT1(false);
    return;
  }

  DWORD update_available_count(0);
  hr = state_key.GetValue(kRegValueUpdateAvailableCount,
                          &update_available_count);
  if (FAILED(hr)) {
    update_available_count = 0;
  }
  ++update_available_count;
  VERIFY_SUCCEEDED(state_key.SetValue(kRegValueUpdateAvailableCount,
                                       update_available_count));

  DWORD64 update_available_since_time(0);
  hr = state_key.GetValue(kRegValueUpdateAvailableSince,
                          &update_available_since_time);
  if (FAILED(hr)) {
    // There is no existing value, so this must be the first update notice.
    VERIFY_SUCCEEDED(state_key.SetValue(kRegValueUpdateAvailableSince,
                                         GetCurrent100NSTime()));

    // TODO(omaha): It would be nice to report the version that we were first
    // told to update to. This is available in UpdateResponse but we do not
    // currently send it down in update responses. If we start using it, add
    // kRegValueFirstUpdateResponseVersion.
  }
}

// Returns 0 for any values that are not found.
void AppManager::ReadUpdateAvailableStats(
    const GUID& app_guid,
    DWORD* update_responses,
    DWORD64* time_since_first_response_ms) {
  ASSERT1(update_responses);
  ASSERT1(time_since_first_response_ms);
  *update_responses = 0;
  *time_since_first_response_ms = 0;

  __mutexScope(registry_access_lock_);

  RegKey state_key;
  HRESULT hr = OpenClientStateKey(app_guid, KEY_READ, &state_key);
  if (FAILED(hr)) {
    CORE_LOG(LW, (_T("[App ClientState key does not exist][%s]"),
                  GuidToString(app_guid)));
    return;
  }

  DWORD update_responses_in_reg(0);
  hr = state_key.GetValue(kRegValueUpdateAvailableCount,
                          &update_responses_in_reg);
  if (SUCCEEDED(hr)) {
    *update_responses = update_responses_in_reg;
  }

  DWORD64 update_available_since_time(0);
  hr = state_key.GetValue(kRegValueUpdateAvailableSince,
                          &update_available_since_time);
  if (SUCCEEDED(hr)) {
    const DWORD64 current_time = GetCurrent100NSTime();
    ASSERT1(update_available_since_time <= current_time);
    const DWORD64 time_since_first_response_in_100ns =
        current_time - update_available_since_time;
    *time_since_first_response_ms =
        time_since_first_response_in_100ns / kMillisecsTo100ns;
  }
}

uint32 AppManager::GetInstallTimeDiffSec(const GUID& app_guid) const {
  if (!IsAppRegistered(app_guid) && !IsAppUninstalled(app_guid)) {
    return kInitialInstallTimeDiff;
  }

  return app_registry_utils::GetInstallTimeDiffSec(is_machine_,
                                                   GuidToString(app_guid));
}

uint32 AppManager::GetDayOfInstall(const GUID& app_guid) const {
  if (!IsAppRegistered(app_guid) && !IsAppUninstalled(app_guid)) {
    return kInitialDayOfInstall;
  }

  DWORD day_of_install(0);
  if (SUCCEEDED(app_registry_utils::GetDayOfInstall(
      is_machine_, GuidToString(app_guid), &day_of_install)) &&
      day_of_install != static_cast<DWORD>(-1)) {
    return day_of_install;
  }

  // No DayOfInstall is present. This app is probably installed before
  // DayOfInstall was implemented. Do not send DayOfInstall in this case.
  return kUnknownDayOfInstall;
}

Tristate AppManager::GetAppUsageStatsEnabled(const GUID& app_guid) const {
  if (!IsAppRegistered(app_guid) && !IsAppUninstalled(app_guid)) {
    return TRISTATE_NONE;
  }

  if (app_registry_utils::AreAppUsageStatsEnabled(is_machine_,
                                                  GuidToString(app_guid))) {
    return TRISTATE_TRUE;
  }

  return TRISTATE_FALSE;
}

// Clear the Installation ID if at least one of the conditions is true:
// 1) DidRun==yes. First run is the last time we want to use the Installation
//    ID. So delete Installation ID if it is present.
// 2) kMaxLifeOfInstallationIDSec has passed since the app was installed. This
//    is to ensure that Installation ID is cleared even if DidRun is never set.
// 3) The app is Omaha. Always delete Installation ID if it is present
//    because DidRun does not apply.
HRESULT AppManager::ClearInstallationId(const App& app) const {
  ASSERT1(app.model()->IsLockedByCaller());
  __mutexScope(registry_access_lock_);

  if (::IsEqualGUID(app.iid(), GUID_NULL)) {
    return S_OK;
  }

  if ((ACTIVE_RUN == app.did_run()) ||
      (kMaxLifeOfInstallationIDSec <= app.install_time_diff_sec()) ||
      (::IsEqualGUID(kGoopdateGuid, app.app_guid()))) {
    CORE_LOG(L1, (_T("[Deleting iid for app][%s]"), app.app_guid_string()));

    RegKey client_state_key;
    HRESULT hr = CreateClientStateKey(app.app_guid(), &client_state_key);
    if (FAILED(hr)) {
      return hr;
    }

    return client_state_key.DeleteValue(kRegValueInstallationId);
  }

  return S_OK;
}

void AppManager::SetLastPingTimeMetrics(
    const App& app,
    int elapsed_days_since_datum,
    int elapsed_seconds_since_day_start) const {
  ASSERT1(elapsed_seconds_since_day_start >= 0);
  ASSERT1(elapsed_seconds_since_day_start < kMaxTimeSinceMidnightSec);
  ASSERT1(elapsed_days_since_datum >= kMinDaysSinceDatum);
  ASSERT1(elapsed_days_since_datum <= kMaxDaysSinceDatum);
  ASSERT1(app.model()->IsLockedByCaller());

  __mutexScope(registry_access_lock_);

  int now = Time64ToInt32(GetCurrent100NSTime());

  RegKey client_state_key;
  if (FAILED(CreateClientStateKey(app.app_guid(), &client_state_key))) {
    return;
  }

  // Update old-style counting metrics.
  const bool did_send_active_ping = (app.did_run() == ACTIVE_RUN &&
                                     app.days_since_last_active_ping() != 0);
  if (did_send_active_ping) {
    VERIFY_SUCCEEDED(client_state_key.SetValue(
        kRegValueActivePingDayStartSec,
        static_cast<DWORD>(now - elapsed_seconds_since_day_start)));
  }

  const bool did_send_roll_call = (app.days_since_last_roll_call() != 0);
  if (did_send_roll_call) {
    VERIFY_SUCCEEDED(client_state_key.SetValue(
        kRegValueRollCallDayStartSec,
        static_cast<DWORD>(now - elapsed_seconds_since_day_start)));
  }

  // Update new-style counting metrics.
  const bool did_send_day_of_last_activity = (app.did_run() == ACTIVE_RUN &&
                                              app.day_of_last_activity() != 0);
  if (did_send_active_ping || did_send_day_of_last_activity) {
    VERIFY_SUCCEEDED(client_state_key.SetValue(
        kRegValueDayOfLastActivity,
        static_cast<DWORD>(elapsed_days_since_datum)));
  }

  const bool did_send_day_of_roll_call = (app.day_of_last_roll_call() != 0);
  if (did_send_roll_call || did_send_day_of_roll_call) {
    VERIFY_SUCCEEDED(client_state_key.SetValue(
        kRegValueDayOfLastRollCall,
        static_cast<DWORD>(elapsed_days_since_datum)));
  }

  // Update the ping freshness value for this ping data. The purpose of the
  // ping freshness is to avoid counting duplicate ping data in the case of
  // reimaged machines. Every time the program updates the user counts, it
  // generates a new freshness value, which remains constant until the
  // user counts are sent to the server.
  GUID ping_freshness = GUID_NULL;
  VERIFY_SUCCEEDED(::CoCreateGuid(&ping_freshness));
  VERIFY_SUCCEEDED(client_state_key.SetValue(kRegValuePingFreshness,
                                              GuidToString(ping_freshness)));
}

void AppManager::UpdateDayOfInstallIfNecessary(
    const App& app, int elapsed_days_since_datum) const {
  ASSERT1(elapsed_days_since_datum >= kMinDaysSinceDatum);
  ASSERT1(elapsed_days_since_datum <= kMaxDaysSinceDatum);
  ASSERT1(app.model()->IsLockedByCaller());

  __mutexScope(registry_access_lock_);

  RegKey client_state_key;
  if (FAILED(CreateClientStateKey(app.app_guid(), &client_state_key))) {
    return;
  }

  DWORD existing_day_of_install(0);
  if (SUCCEEDED(client_state_key.GetValue(kRegValueDayOfInstall,
                                           &existing_day_of_install))) {
    // Update DayOfInstall only if its value is -1.
    if (existing_day_of_install == static_cast<DWORD>(-1)) {
      VERIFY_SUCCEEDED(client_state_key.SetValue(
          kRegValueDayOfInstall,
          static_cast<DWORD>(elapsed_days_since_datum)));
    }
  }
}

// Writes the day start time when last active ping/roll call happened to
// registry if the corresponding ping has been sent.
// Removes installation id, if did run = true or if goopdate.
// Clears did run.
// Updates ping_freshness, as a side effect of calling SetLastPingTimeMetrics.
HRESULT AppManager::PersistUpdateCheckSuccessfullySent(
    const App& app,
    int elapsed_days_since_datum,
    int elapsed_seconds_since_day_start) {
  ASSERT1(app.model()->IsLockedByCaller());

  ApplicationUsageData app_usage(app.app_bundle()->is_machine(),
                                 vista_util::IsVistaOrLater());
  VERIFY_SUCCEEDED(app_usage.ResetDidRun(app.app_guid_string()));

  SetLastPingTimeMetrics(
      app, elapsed_days_since_datum, elapsed_seconds_since_day_start);
  UpdateDayOfInstallIfNecessary(app, elapsed_days_since_datum);

  // Handle the installation id.
  VERIFY_SUCCEEDED(ClearInstallationId(app));

  return S_OK;
}

HRESULT AppManager::RemoveClientState(const GUID& app_guid) {
  CORE_LOG(L2, (_T("[AppManager::RemoveClientState][%s]"),
                GuidToString(app_guid)));
  ASSERT1(IsRegistryStableStateLockedByCaller());
  __mutexScope(registry_access_lock_);

  ASSERT1(!IsAppRegistered(app_guid));

  return app_registry_utils::RemoveClientState(is_machine_,
                                               GuidToString(app_guid));
}

}  // namespace omaha
