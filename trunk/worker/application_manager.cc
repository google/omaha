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

//
// Contains the logic to encapsulate access to the application data
// stored in the registry.

#include "omaha/worker/application_manager.h"

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <vector>
#include "base/scoped_ptr.h"
#include "omaha/common/scoped_ptr_address.h"
#include "omaha/common/time.h"
#include "omaha/common/utils.h"
#include "omaha/common/vistautil.h"
#include "omaha/goopdate/config_manager.h"
#include "omaha/goopdate/const_goopdate.h"
#include "omaha/goopdate/goopdate_utils.h"
#include "omaha/worker/application_usage_data.h"
#include "omaha/worker/application_data.h"
#include "omaha/worker/job.h"

namespace omaha {

namespace {

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

// Determines if an application is registered with Google Update.
class IsAppRegisteredFunc
    : public std::unary_function<const CString&, HRESULT> {
 public:
  explicit IsAppRegisteredFunc(const CString& guid)
      : is_registered_(false),
        guid_(guid) {}

  bool is_registered() const { return is_registered_; }

  HRESULT operator() (const CString& guid) {
    if (guid.CompareNoCase(guid_) == 0) {
      is_registered_ = true;
    }
    return S_OK;
  }
 private:
  CString guid_;
  bool is_registered_;
};

// Accumulates ProductData.
class CollectProductsFunc
    : public std::unary_function<const CString&, HRESULT> {
 public:
  CollectProductsFunc(ProductDataVector* products,
                      bool is_machine,
                      bool collect_registered_products)
      : products_(products),
        is_machine_(is_machine),
        collect_registered_products_(collect_registered_products) {
    ASSERT1(products);
  }

  // Ignores errors and accumulates as many applications as possible.
  HRESULT operator() (const CString& guid) {
    AppManager app_manager(is_machine_);
    ProductData product_data;
    if (SUCCEEDED(app_manager.ReadProductDataFromStore(StringToGuid(guid),
                                                       &product_data))) {
      ASSERT(!collect_registered_products_ ||
             !product_data.app_data().is_uninstalled(),
             (_T("Should not be finding uninstalled apps while looking for ")
              _T("registered apps; may be enumerating the wrong key.")));
      if (collect_registered_products_ ||
          product_data.app_data().is_uninstalled()) {
        CORE_LOG(L3, (_T("[Found %s product][%s]"),
                      collect_registered_products_ ? _T("registered") :
                                                      _T("uninstalled"),
                      guid));
        products_->push_back(product_data);
      }
    }
    return S_OK;
  }

 private:
  bool collect_registered_products_;
  bool is_machine_;
  ProductDataVector* products_;
};

// Accumulates AppData for components of a product.
class CollectComponentsFunc
    : public std::unary_function<const CString&, HRESULT> {
 public:
  CollectComponentsFunc(ProductData* product_data,
                        bool is_machine)
      : product_data_(product_data),
        is_machine_(is_machine) {
    ASSERT1(product_data);
  }

  // Ignores errors and accumulates as many components as possible.
  HRESULT operator() (const CString& guid) {
    AppManager app_manager(is_machine_);
    AppData component_data;
    if (SUCCEEDED(app_manager.ReadAppDataFromStore(
        product_data_->app_data().app_guid(),
        StringToGuid(guid),
        &component_data))) {
      product_data_->AddComponent(component_data);
    }
    return S_OK;
  }

 private:
  ProductData* product_data_;
  bool is_machine_;
};


// Enumerates all sub keys of the key and calls the functor for each of them.
template <typename T>
HRESULT EnumerateSubKeys(const TCHAR* key_name, T* functor) {
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
      (*functor)(sub_key_name);
    }
  }

  return S_OK;
}

}  // namespace

AppManager::AppManager(bool is_machine)
    : is_machine_(is_machine) {
  CORE_LOG(L3, (_T("[AppManager::AppManager][is_machine=%d]"), is_machine));
}

bool AppManager::IsProductRegistered(const GUID& app_guid) const {
  IsAppRegisteredFunc func(GuidToString(app_guid));
  HRESULT hr = EnumerateSubKeys(
      ConfigManager::Instance()->registry_clients(is_machine_),
      &func);
  if (FAILED(hr)) {
    return false;
  }

  return func.is_registered();
}

// TODO(omaha): Consider making AppManager a namespace.
void AppManager::ConvertCommandLineToProductData(const CommandLineArgs& args,
                                                 ProductDataVector* products) {
  ASSERT1(products);

  // TODO(omaha):  Need to update this to read the bundle info to build up
  // multiple AppData objects (and also to read components) and add unit test.
  for (size_t i = 0; i < args.extra.apps.size(); ++i) {
    const CommandLineAppArgs& extra_arg = args.extra.apps[i];
    const GUID& app_guid = extra_arg.app_guid;
    AppData app_data(app_guid, is_machine_);
    app_data.set_language(args.extra.language);
    app_data.set_ap(extra_arg.ap);
    app_data.set_tt_token(extra_arg.tt_token);
    app_data.set_iid(args.extra.installation_id);
    app_data.set_brand_code(args.extra.brand_code);
    app_data.set_client_id(args.extra.client_id);
    app_data.set_referral_id(args.extra.referral_id);

    // install_time_diff_sec is set based on the current state of the system.
    if (IsProductRegistered(app_guid)) {
      app_data.set_install_time_diff_sec(GetInstallTimeDiffSec(app_guid));
    } else {
      // The product is not already installed. We differentiate this from no
      // install time being present (i.e. the app was installed before
      // installtime was implemented) by setting the diff to -1 days.
      // This makes an assumption about the XML parser but works for now.
      const int kNewInstallValue = -1 * kSecondsPerDay;
      app_data.set_install_time_diff_sec(kNewInstallValue);
    }

    // Do not set is_oem_install because it is not based on the command line.
    app_data.set_is_eula_accepted(!args.is_eula_required_set);
    app_data.set_display_name(extra_arg.app_name);
    app_data.set_browser_type(args.extra.browser_type);
    app_data.set_install_source(args.install_source);
    app_data.set_usage_stats_enable(args.extra.usage_stats_enable);
    app_data.set_encoded_installer_data(extra_arg.encoded_installer_data);
    app_data.set_install_data_index(extra_arg.install_data_index);

    ProductData product_data(app_data);
    products->push_back(product_data);
  }

  ASSERT1(products->size() == args.extra.apps.size());
}


HRESULT AppManager::GetRegisteredProducts(ProductDataVector* products) const {
  ASSERT1(products);

  CollectProductsFunc func(products, is_machine_, true);
  return EnumerateSubKeys(
      ConfigManager::Instance()->registry_clients(is_machine_),
      &func);
}

HRESULT AppManager::GetUnRegisteredProducts(ProductDataVector* products) const {
  ASSERT1(products);

  CollectProductsFunc func(products, is_machine_, false);
  return EnumerateSubKeys(
      ConfigManager::Instance()->registry_client_state(is_machine_),
      &func);
}

CString AppManager::GetProductClientKeyName(const GUID& app_guid) {
  return goopdate_utils::GetAppClientsKey(is_machine_, GuidToString(app_guid));
}

CString AppManager::GetProductClientComponentsKeyName(const GUID& app_guid) {
  return AppendRegKeyPath(GetProductClientKeyName(app_guid),
                          kComponentsRegKeyName);
}

CString AppManager::GetProductClientStateComponentsKeyName(
    const GUID& app_guid) {
  return AppendRegKeyPath(GetProductClientStateKeyName(app_guid),
                          kComponentsRegKeyName);
}

CString AppManager::GetComponentClientKeyName(const GUID& parent_app_guid,
                                              const GUID& app_guid) {
  return AppendRegKeyPath(GetProductClientComponentsKeyName(parent_app_guid),
                          GuidToString(app_guid));
}

CString AppManager::GetProductClientStateKeyName(const GUID& app_guid) {
  return goopdate_utils::GetAppClientStateKey(is_machine_,
                                              GuidToString(app_guid));
}

CString AppManager::GetComponentClientStateKeyName(const GUID& parent_app_guid,
                                                   const GUID& app_guid) {
  return AppendRegKeyPath(
      GetProductClientStateComponentsKeyName(parent_app_guid),
      GuidToString(app_guid));
}

CString AppManager::GetProductClientStateMediumKeyName(const GUID& app_guid) {
  ASSERT1(is_machine_);
  return goopdate_utils::GetAppClientStateMediumKey(is_machine_,
                                                    GuidToString(app_guid));
}

CString AppManager::GetClientKeyName(const GUID& parent_app_guid,
                                     const GUID& app_guid) {
  if (::IsEqualGUID(parent_app_guid, GUID_NULL)) {
    return GetProductClientKeyName(app_guid);
  } else {
    return GetComponentClientKeyName(parent_app_guid, app_guid);
  }
}

CString AppManager::GetClientStateKeyName(const GUID& parent_app_guid,
                                          const GUID& app_guid) {
  if (::IsEqualGUID(parent_app_guid, GUID_NULL)) {
    return GetProductClientStateKeyName(app_guid);
  } else {
    return GetComponentClientStateKeyName(parent_app_guid, app_guid);
  }
}

HRESULT AppManager::OpenClientKey(const GUID& parent_app_guid,
                                  const GUID& app_guid,
                                  RegKey* client_key) {
  ASSERT1(client_key);
  return client_key->Open(GetClientKeyName(parent_app_guid, app_guid),
                          KEY_READ);
}

HRESULT AppManager::OpenClientStateKey(const GUID& parent_app_guid,
                                       const GUID& app_guid,
                                       REGSAM sam_desired,
                                       RegKey* client_state_key) {
  ASSERT1(client_state_key);
  CString key_name = GetClientStateKeyName(parent_app_guid, app_guid);
  return client_state_key->Open(key_name, sam_desired);
}

// Also creates the ClientStateMedium key for machine apps, ensuring it exists
// whenever ClientState exists.  Does not create ClientStateMedium for Omaha.
// This method is called for self-updates, so it must explicitly avoid this.
HRESULT AppManager::CreateClientStateKey(const GUID& parent_app_guid,
                                         const GUID& app_guid,
                                         RegKey* client_state_key) {
  ASSERT(::IsEqualGUID(parent_app_guid, GUID_NULL),
         (_T("Legacy components not supported; ClientStateMedium ignores.")));

  ASSERT1(client_state_key);
  const CString key_name = GetClientStateKeyName(parent_app_guid, app_guid);
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

  const CString medium_key_name = GetProductClientStateMediumKeyName(app_guid);
  hr = RegKey::CreateKey(medium_key_name);
  if (FAILED(hr)) {
    CORE_LOG(L3, (_T("[RegKey::Create ClientStateMedium failed][0x%08x]"), hr));
    return hr;
  }

  return S_OK;
}

// Reads the following values from the registry:
// Clients Key
//   product version
//   language
// Client State Key
//   previous product version.
//   last checked
//   ap
//   client id
//   iid
// Clients key in HKCU/HKLM/Low integrity
//   did run
// Note: If the application is uninstalled, the clients key may not exist.
HRESULT AppManager::ReadProductDataFromStore(const GUID& app_guid,
                                             ProductData* product_data) {
  ASSERT1(product_data);

  AppData app_data;
  HRESULT hr = ReadAppDataFromStore(GUID_NULL, app_guid, &app_data);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[AppManager::ReadAppDataFromStore failed][0x%08x]"), hr));
    return hr;
  }

  ProductData product(app_data);

  // Read components for this product from the registry.
  CollectComponentsFunc func(&product, is_machine_);
  EnumerateSubKeys(GetProductClientComponentsKeyName(app_data.app_guid()),
                   &func);

  *product_data = product;
  return S_OK;
}

HRESULT AppManager::ReadAppDataFromStore(const GUID& parent_app_guid,
                                         const GUID& app_guid,
                                         AppData* app_data) {
  ASSERT1(app_data);
  AppData temp_data(app_guid, is_machine_);
  temp_data.set_parent_app_guid(parent_app_guid);

  bool client_key_exists = false;
  RegKey client_key;
  HRESULT hr = OpenClientKey(parent_app_guid, app_guid, &client_key);
  if (SUCCEEDED(hr)) {
    CString version;
    hr = client_key.GetValue(kRegValueProductVersion, &version);
    temp_data.set_version(version);
    CORE_LOG(L3, (_T("[AppManager::ReadAppDataFromStore]")
                  _T("[parent_app_guid=%s]")
                  _T("[app_guid=%s]")
                  _T("[version=%s]"),
                  GuidToString(parent_app_guid),
                  GuidToString(app_guid),
                  version));
    if (FAILED(hr)) {
      return hr;
    }

    // Language might not be written by an installer, so ignore failures.
    CString language;
    client_key.GetValue(kRegValueLanguage, &language);
    temp_data.set_language(language);
    client_key_exists = true;
  }

  // If ClientState registry key doesn't exist, the function could return.
  // Before opening the key, set days_since_last* to -1, which is the
  // default value if reg key doesn't exist. If later we find that the values
  // are readable, new values will overwrite current ones.
  temp_data.set_days_since_last_active_ping(-1);
  temp_data.set_days_since_last_roll_call(-1);

  RegKey client_state_key;
  hr = OpenClientStateKey(parent_app_guid,
                          app_guid,
                          KEY_READ,
                          &client_state_key);
  if (FAILED(hr)) {
    // It is possible that the client state key has not yet been populated.
    // In this case just return the information that we have gathered thus far.
    // However if both keys dont exist, then we are doing something wrong.
    CORE_LOG(LW, (_T("[AppManager::ReadAppDataFromStore - No ClientState]")));
    if (client_key_exists) {
      *app_data = temp_data;
      return S_OK;
    } else {
      return hr;
    }
  }

  // The value is not essential for omaha's operation, so ignore errors.
  CString previous_version;
  HRESULT previous_version_hr =
      client_state_key.GetValue(kRegValueProductVersion, &previous_version);
  temp_data.set_previous_version(previous_version);

  // An app is uninstalled if the Client key exists and the pv value in the
  // ClientState key does not exist.
  // Omaha may create the app's ClientState key and write values from the
  // metainstaller tag before running the installer, which creates the Client
  // key. Requiring pv in ClientState avoids mistakenly determining that the
  // Omaha-created key indicates an uninstall.
  bool app_is_uninstalled =
      !client_key_exists &&
      HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) != previous_version_hr;
  temp_data.set_is_uninstalled(app_is_uninstalled);

  CString ap;
  client_state_key.GetValue(kRegValueAdditionalParams, &ap);
  temp_data.set_ap(ap);

  CString tt_token;
  client_state_key.GetValue(kRegValueTTToken, &tt_token);
  temp_data.set_tt_token(tt_token);

  CString iid;
  client_state_key.GetValue(kRegValueInstallationId, &iid);
  temp_data.set_iid(StringToGuid(iid));

  CString brand_code;
  client_state_key.GetValue(kRegValueBrandCode, &brand_code);
  ASSERT1(brand_code.GetLength() <= kBrandIdLength);
  temp_data.set_brand_code(brand_code);

  CString client_id;
  client_state_key.GetValue(kRegValueClientId, &client_id);
  temp_data.set_client_id(client_id);

  DWORD last_active_ping_sec(0);
  if (SUCCEEDED(client_state_key.GetValue(kRegValueActivePingDayStartSec,
                                          &last_active_ping_sec))) {
    int days_since_last_active_ping =
        GetNumberOfDaysSince(static_cast<int32>(last_active_ping_sec));
    temp_data.set_days_since_last_active_ping(days_since_last_active_ping);
  }

  DWORD last_roll_call_sec(0);
  if (SUCCEEDED(client_state_key.GetValue(kRegValueRollCallDayStartSec,
                                          &last_roll_call_sec))) {
    int days_since_last_roll_call =
        GetNumberOfDaysSince(static_cast<int32>(last_roll_call_sec));
    temp_data.set_days_since_last_roll_call(days_since_last_roll_call);
  }

  // We do not need the referral_id.

  ASSERT(::IsEqualGUID(parent_app_guid, GUID_NULL),
         (_T("Legacy components not supported.")));
  temp_data.set_install_time_diff_sec(GetInstallTimeDiffSec(app_guid));

  temp_data.set_is_oem_install(client_state_key.HasValue(kRegValueOemInstall));

  bool is_eula_accepted = true;
  DWORD eula_accepted = 0;

  ASSERT(::IsEqualGUID(parent_app_guid, GUID_NULL),
         (_T("Legacy components not supported; IsAppEulaAccepted ignores.")));
  temp_data.set_is_eula_accepted(
      goopdate_utils::IsAppEulaAccepted(is_machine_,
                                        GuidToString(app_guid),
                                        false));

  if (temp_data.language().IsEmpty()) {
    // Read the language from the client state key if we did not find
    // it in the client key.
    CString language;
    client_state_key.GetValue(kRegValueLanguage, &language);
    temp_data.set_language(language);
  }

  // Read the did run value.
  // TODO(omaha): Try to move this logic into the application_usage_data class.
  ApplicationUsageData app_usage(is_machine_, vista_util::IsVistaOrLater());
  hr = app_usage.ReadDidRun(GuidToString(app_guid));
  if (FAILED(hr)) {
    CORE_LOG(LEVEL_WARNING, (_T("[ReadDidRun failed][0x%08x]"), hr));
  }
  AppData::ActiveStates active = AppData::ACTIVE_NOTRUN;
  if (app_usage.exists()) {
    active = app_usage.did_run() ? AppData::ACTIVE_RUN :
                                   AppData::ACTIVE_NOTRUN;
  } else {
    active = AppData::ACTIVE_UNKNOWN;
  }
  temp_data.set_did_run(active);

  *app_data = temp_data;
  return S_OK;
}

// Sets brand code, client ID, usage stats, browser, ap, and Omaha language in
// the app's ClientState. Also, sets the installed by OEM flag if appropriate.
// Sets eulaaccepted=0 if the app is not already registered and the app's EULA
// has not been accepted. Deletes eulaaccepted if the EULA has been accepted.
// Only call for initial or over-installs. Do not call for updates to avoid
// mistakenly replacing data, such as the application's language, and causing
// unexpected changes to the app during a silent update.
HRESULT AppManager::WritePreInstallData(const AppData& app_data) {
  CORE_LOG(L3, (_T("[AppManager::WritePreInstallData]")));

  RegKey client_state_key;
  HRESULT hr = CreateClientStateKey(app_data.parent_app_guid(),
                                    app_data.app_guid(),
                                    &client_state_key);
  if (FAILED(hr)) {
    return hr;
  }

  if (app_data.is_eula_accepted()) {
    hr = goopdate_utils::ClearAppEulaNotAccepted(
             is_machine_,
             GuidToString(app_data.app_guid()));
  } else {
    if (!IsProductRegistered(app_data.app_guid())) {
      hr = goopdate_utils::SetAppEulaNotAccepted(
               is_machine_,
               GuidToString(app_data.app_guid()));
    }
  }
  if (FAILED(hr)) {
    return hr;
  }

  CString state_key_path = GetClientStateKeyName(app_data.parent_app_guid(),
                                                 app_data.app_guid());
  VERIFY1(SUCCEEDED(goopdate_utils::SetAppBranding(state_key_path,
                                                   app_data.brand_code(),
                                                   app_data.client_id(),
                                                   app_data.referral_id())));

  ASSERT(::IsEqualGUID(app_data.parent_app_guid(), GUID_NULL),
         (_T("Legacy components not supported; SetUsageStatsEnable ignores.")));
  if (TRISTATE_NONE != app_data.usage_stats_enable()) {
    VERIFY1(SUCCEEDED(goopdate_utils::SetUsageStatsEnable(
                          is_machine_,
                          GuidToString(app_data.app_guid()),
                          app_data.usage_stats_enable())));
  }

  if (BROWSER_UNKNOWN == app_data.browser_type()) {
    VERIFY1(SUCCEEDED(client_state_key.DeleteValue(kRegValueBrowser)));
  } else {
    DWORD browser_type = app_data.browser_type();
    VERIFY1(SUCCEEDED(client_state_key.SetValue(kRegValueBrowser,
                                                browser_type)));
  }

  if (app_data.ap().IsEmpty()) {
    VERIFY1(SUCCEEDED(
        client_state_key.DeleteValue(kRegValueAdditionalParams)));
  } else {
    VERIFY1(SUCCEEDED(client_state_key.SetValue(kRegValueAdditionalParams,
                                                app_data.ap())));
  }

  if (!app_data.language().IsEmpty()) {
    VERIFY1(SUCCEEDED(client_state_key.SetValue(kRegValueLanguage,
                                                app_data.language())));
  }

  if (ConfigManager::Instance()->IsOemInstalling(is_machine_)) {
    ASSERT1(is_machine_);
    VERIFY1(SUCCEEDED(client_state_key.SetValue(kRegValueOemInstall, _T("1"))));
  }

  return S_OK;
}

// Sets installation ID in the app's ClientState.
// Copies version and language from clients to client state reg key.
HRESULT AppManager::InitializeApplicationState(AppData* app_data) {
  CORE_LOG(L3, (_T("[AppManager::InitializeApplicationState]")));
  ASSERT1(app_data);
  RegKey client_state_key;
  HRESULT hr = CreateClientStateKey(app_data->parent_app_guid(),
                                    app_data->app_guid(),
                                    &client_state_key);
  if (FAILED(hr)) {
    return hr;
  }

  RegKey client_key;
  hr = OpenClientKey(app_data->parent_app_guid(),
                     app_data->app_guid(),
                     &client_key);
  if (FAILED(hr)) {
    return hr;
  }

  if (!::IsEqualGUID(app_data->iid(), GUID_NULL)) {
    VERIFY1(SUCCEEDED(client_state_key.SetValue(
                          kRegValueInstallationId,
                          GuidToString(app_data->iid()))));
  }

  return CopyVersionAndLanguageToClientState(app_data,
                                             client_state_key,
                                             client_key);
}

// Copies language and version from clients into client state reg key.
// Removes installation id, if did run = true or if goopdate.
// Clears did run.
// Also updates the last active ping and roll call time in registry
// if the corresponding ping was sent to server.
HRESULT AppManager::UpdateApplicationState(int time_since_midnight_sec,
                                           AppData* app_data) {
  ASSERT1(app_data);
  RegKey client_state_key;
  HRESULT hr = CreateClientStateKey(app_data->parent_app_guid(),
                                    app_data->app_guid(),
                                    &client_state_key);
  if (FAILED(hr)) {
    return hr;
  }

  RegKey client_key;
  hr = OpenClientKey(app_data->parent_app_guid(),
                     app_data->app_guid(),
                     &client_key);
  if (FAILED(hr)) {
    return hr;
  }

  hr = CopyVersionAndLanguageToClientState(app_data,
                                           client_state_key,
                                           client_key);
  if (FAILED(hr)) {
    return hr;
  }

  // Handle the installation id.
  hr = ClearInstallationId(app_data, client_state_key);
  if (FAILED(hr)) {
    return hr;
  }

  SetLastPingDayStartTime(time_since_midnight_sec,
                          app_data,
                          client_state_key);

  // Reset did_run after updating the days_since_last_active_ping since that
  // need previous did_run status to determine whether an active ping has been
  // sent.
  ResetDidRun(app_data);

  return S_OK;
}

HRESULT AppManager::WriteTTToken(const AppData& app_data,
                                 const UpdateResponseData& response_data) {
  CORE_LOG(L3, (_T("[WriteTTToken][app_data token=%s][response_data token=%s]"),
                app_data.tt_token(), response_data.tt_token()));
  RegKey client_state_key;
  HRESULT hr = CreateClientStateKey(app_data.parent_app_guid(),
                                    app_data.app_guid(),
                                    &client_state_key);
  if (FAILED(hr)) {
    return hr;
  }

  if (response_data.tt_token().IsEmpty()) {
    VERIFY1(SUCCEEDED(client_state_key.DeleteValue(kRegValueTTToken)));
  } else {
    VERIFY1(SUCCEEDED(client_state_key.SetValue(kRegValueTTToken,
                                                response_data.tt_token())));
  }

  return S_OK;
}

// The registry reads and writes are not thread safe, but there should not be
// other threads or processes calling this at the same time.
void AppManager::UpdateUpdateAvailableStats(const GUID& parent_app_guid,
                                            const GUID& app_guid) {
  RegKey state_key;
  HRESULT hr = CreateClientStateKey(parent_app_guid, app_guid, &state_key);
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
  VERIFY1(SUCCEEDED(state_key.SetValue(kRegValueUpdateAvailableCount,
                                       update_available_count)));

  DWORD64 update_available_since_time(0);
  hr = state_key.GetValue(kRegValueUpdateAvailableSince,
                          &update_available_since_time);
  if (FAILED(hr)) {
    // There is no existing value, so this must be the first update notice.
    VERIFY1(SUCCEEDED(state_key.SetValue(kRegValueUpdateAvailableSince,
                                         GetCurrent100NSTime())));

    // TODO(omaha): It would be nice to report the version that we were first
    // told to update to. This is available in UpdateResponse but we do not
    // currently send it down in update responses. If we start using it, add
    // kRegValueFirstUpdateResponseVersion.
  }
}

void AppManager::ClearUpdateAvailableStats(const GUID& parent_app_guid,
                                           const GUID& app_guid) {
  RegKey state_key;
  HRESULT hr =
      OpenClientStateKey(parent_app_guid, app_guid, KEY_ALL_ACCESS, &state_key);
  if (FAILED(hr)) {
    return;
  }

  VERIFY1(SUCCEEDED(state_key.DeleteValue(kRegValueUpdateAvailableCount)));
  VERIFY1(SUCCEEDED(state_key.DeleteValue(kRegValueUpdateAvailableSince)));
}

void AppManager::ClearOemInstalled(const GUID& parent_app_guid,
                                   const GUID& app_guid) {
  RegKey state_key;
  HRESULT hr =
      OpenClientStateKey(parent_app_guid, app_guid, KEY_ALL_ACCESS, &state_key);
  ASSERT1(SUCCEEDED(hr));
  if (FAILED(hr)) {
    return;
  }

  VERIFY1(SUCCEEDED(state_key.DeleteValue(kRegValueOemInstall)));
}

// Returns 0 for any values that are not found.
void AppManager::ReadUpdateAvailableStats(
    const GUID& parent_app_guid,
    const GUID& app_guid,
    DWORD* update_responses,
    DWORD64* time_since_first_response_ms) {
  ASSERT1(update_responses);
  ASSERT1(time_since_first_response_ms);
  *update_responses = 0;
  *time_since_first_response_ms = 0;

  RegKey state_key;
  HRESULT hr = OpenClientStateKey(parent_app_guid,
                                  app_guid,
                                  KEY_READ,
                                  &state_key);
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

// The update success and update check success times are not updated for
// installs even if it is an over-install. At this point, we do not know for
// sure that it was an online update.
void AppManager::RecordSuccessfulInstall(const GUID& parent_app_guid,
                                         const GUID& app_guid,
                                         bool is_update,
                                         bool is_offline) {
  ASSERT1(!is_update || !is_offline);

  ClearUpdateAvailableStats(parent_app_guid, app_guid);

  if (!is_offline) {
    // Assumes that all updates are online.
    RecordSuccessfulUpdateCheck(parent_app_guid, app_guid);
  }

  if (is_update) {
    RegKey state_key;
    HRESULT hr = CreateClientStateKey(parent_app_guid, app_guid, &state_key);
    if (FAILED(hr)) {
      ASSERT1(false);
      return;
    }

    const DWORD now = Time64ToInt32(GetCurrent100NSTime());
    VERIFY1(SUCCEEDED(state_key.SetValue(kRegValueLastUpdateTimeSec, now)));
  }
}

void AppManager::RecordSuccessfulUpdateCheck(const GUID& parent_app_guid,
                                             const GUID& app_guid) {
  RegKey state_key;
  HRESULT hr = CreateClientStateKey(parent_app_guid, app_guid, &state_key);
  if (FAILED(hr)) {
    ASSERT1(false);
    return;
  }

  const DWORD now = Time64ToInt32(GetCurrent100NSTime());
  VERIFY1(SUCCEEDED(state_key.SetValue(kRegValueLastSuccessfulCheckSec, now)));
}

// Assumes the app is registered and has a ClientState. The new install case
// should be handled differently.
uint32 AppManager::GetInstallTimeDiffSec(const GUID& app_guid) {
  RegKey client_state_key;
  HRESULT hr = OpenClientStateKey(GUID_NULL,
                                  app_guid,
                                  KEY_READ,
                                  &client_state_key);
  ASSERT(SUCCEEDED(hr), (_T("Assumes app is registered.")));
  if (FAILED(hr)) {
    return 0;
  }

  DWORD install_time(0);
  DWORD install_time_diff_sec(0);
  if (SUCCEEDED(client_state_key.GetValue(kRegValueInstallTimeSec,
                                          &install_time))) {
    const uint32 now = Time64ToInt32(GetCurrent100NSTime());
    if (0 != install_time && now >= install_time) {
      install_time_diff_sec = now - install_time;
      // TODO(omaha3): Restore this assert. In Omaha 2, this method gets called
      // as part of installation verification and Job::UpdateJob(), so the value
      // can be 0. This will not be the case in Omaha 3.
      // ASSERT1(install_time_diff_sec != 0);
    }
  }

  return install_time_diff_sec;
}

// Clear the Installation ID if at least one of the conditions is true:
// 1) DidRun==yes. First run is the last time we want to use the Installation
//    ID. So delete Installation ID if it is present.
// 2) kMaxLifeOfInstallationIDSec has passed since the app was installed. This
//    is to ensure that Installation ID is cleared even if DidRun is never set.
// 3) The app is Omaha. Always delete Installation ID if it is present
//    because DidRun does not apply.
HRESULT AppManager::ClearInstallationId(AppData* app_data,
                                        const RegKey& client_state_key) {
  ASSERT1(app_data);
  if (::IsEqualGUID(app_data->iid(), GUID_NULL)) {
    return S_OK;
  }

  if ((AppData::ACTIVE_RUN == app_data->did_run()) ||
      (kMaxLifeOfInstallationIDSec <= app_data->install_time_diff_sec()) ||
      (::IsEqualGUID(kGoopdateGuid, app_data->app_guid()))) {
    CORE_LOG(L1, (_T("[Deleting iid for app][%s]"),
                  GuidToString(app_data->app_guid())));
    // Relies on installation_id not empty to indicate state_key is valid.
    VERIFY1(S_OK == client_state_key.DeleteValue(kRegValueInstallationId));
    app_data->set_iid(GUID_NULL);
  }

  return S_OK;
}

void AppManager::ResetDidRun(AppData* app_data) {
  ApplicationUsageData app_usage(app_data->is_machine_app(),
                                 vista_util::IsVistaOrLater());
  VERIFY1(SUCCEEDED(app_usage.ResetDidRun(GuidToString(app_data->app_guid()))));
  app_data->set_did_run(app_usage.exists() ? AppData::ACTIVE_NOTRUN :
                                             AppData::ACTIVE_UNKNOWN);
}

// Write the day start time when last active ping/roll call happened to
// registry.
void AppManager::SetLastPingDayStartTime(int time_since_midnight_sec,
                                         AppData* app_data,
                                         const RegKey& client_state_key) {
  ASSERT1(time_since_midnight_sec >= 0);
  ASSERT1(time_since_midnight_sec < kMaxTimeSinceMidnightSec);

  int now = Time64ToInt32(GetCurrent100NSTime());

  bool did_send_active_ping = (app_data->did_run() == AppData::ACTIVE_RUN &&
                               app_data->days_since_last_active_ping() != 0);
  if (did_send_active_ping) {
    VERIFY1(SUCCEEDED(client_state_key.SetValue(
                          kRegValueActivePingDayStartSec,
                          static_cast<DWORD>(now - time_since_midnight_sec))));
    app_data->set_days_since_last_active_ping(0);
  }

  bool did_send_roll_call = (app_data->days_since_last_roll_call() != 0);
  if (did_send_roll_call) {
    VERIFY1(SUCCEEDED(client_state_key.SetValue(
                          kRegValueRollCallDayStartSec,
                          static_cast<DWORD>(now - time_since_midnight_sec))));
    app_data->set_days_since_last_roll_call(0);
  }
}

// Only replaces the language in ClientState and app_data if the language in
// Clients is not empty.
HRESULT AppManager::CopyVersionAndLanguageToClientState(
    AppData* app_data,
    const RegKey& client_state_key,
    const RegKey& client_key) {
  // TODO(omaha):  need to handle components too.
  ASSERT1(app_data);
  // Read the version and language from the client key.
  CString version;
  HRESULT hr = client_key.GetValue(kRegValueProductVersion, &version);
  if (FAILED(hr)) {
    return hr;
  }

  CString language;
  client_key.GetValue(kRegValueLanguage, &language);

  // Write the version and language in the client state key.
  hr = client_state_key.SetValue(kRegValueProductVersion, version);
  if (FAILED(hr)) {
    return hr;
  }

  app_data->set_version(version);
  app_data->set_previous_version(version);

  if (!language.IsEmpty()) {
    VERIFY1(SUCCEEDED(client_state_key.SetValue(kRegValueLanguage, language)));
    app_data->set_language(language);
  }

  return S_OK;
}

HRESULT AppManager::RemoveClientState(const AppData& app_data) {
  ASSERT(::IsEqualGUID(app_data.parent_app_guid(), GUID_NULL),
         (_T("Legacy components not supported; ClientStateMedium ignores.")));

  ASSERT1(app_data.is_uninstalled());
  const CString state_key = GetClientStateKeyName(app_data.parent_app_guid(),
                                                  app_data.app_guid());
  HRESULT state_hr = RegKey::DeleteKey(state_key, true);

  if (!is_machine_) {
    return state_hr;
  }

  const CString state_medium_key =
      GetProductClientStateMediumKeyName(app_data.app_guid());
  HRESULT state_medium_hr = RegKey::DeleteKey(state_medium_key, true);

  return FAILED(state_hr) ? state_hr : state_medium_hr;
}

// Returns true if the absolute difference between time moments is greater than
// the interval between update checks.
// Deals with clocks rolling backwards, in scenarios where the clock indicates
// some time in the future, for example next year, last_checked_ is updated to
// reflect that time, and then the clock is adjusted back to present.
bool AppManager::ShouldCheckForUpdates() const {
  ConfigManager* cm = ConfigManager::Instance();
  bool is_period_overridden = false;
  const int update_interval = cm->GetLastCheckPeriodSec(&is_period_overridden);
  if (0 == update_interval) {
    ASSERT1(is_period_overridden);
    OPT_LOG(L1, (_T("[ShouldCheckForUpdates returned 0][checks disabled]")));
    return false;
  }

  const int time_difference = cm->GetTimeSinceLastCheckedSec(is_machine_);

  const bool result = time_difference >= update_interval ? true : false;
  CORE_LOG(L3, (_T("[ShouldCheckForUpdates returned %d][%u]"),
                result, is_period_overridden));
  return result;
}

HRESULT AppManager::UpdateLastChecked() {
  // Set the last check value to the current value.
  DWORD now = Time64ToInt32(GetCurrent100NSTime());
  HRESULT hr = ConfigManager::Instance()->SetLastCheckedTime(is_machine_, now);
  CORE_LOG(L3, (_T("[AppManager::UpdateLastChecked][now %d]"), now));
  if (FAILED(hr)) {
    CORE_LOG(LW, (_T("[UpdateLastChecked returned 0x%08x]"), hr));
    return hr;
  }
  return S_OK;
}

HRESULT AppManager::ReadProductDataFromUserOrMachineStore(
    const GUID& guid,
    ProductData* product_data) {
  ASSERT1(product_data);
  AppManager app_manager_user(false);
  HRESULT hr = app_manager_user.ReadProductDataFromStore(guid, product_data);
  if (SUCCEEDED(hr)) {
    return hr;
  }

  AppManager app_manager_machine(true);
  return app_manager_machine.ReadProductDataFromStore(guid, product_data);
}

// Writes 0.0.0.1 to pv. This value avoids any special cases, such as initial
// install rules, for 0.0.0.0, while being unlikely to be higher than the
// product's actual current version.
HRESULT AppManager::RegisterProduct(const GUID& product_guid,
                                    const CString& product_name) {
  const TCHAR* const kRegisterProductVersion = _T("0.0.0.1");

  RegKey client_key;
  HRESULT hr = client_key.Create(GetClientKeyName(GUID_NULL, product_guid));
  if (FAILED(hr)) {
    return hr;
  }

  hr = client_key.SetValue(kRegValueProductVersion, kRegisterProductVersion);
  if (FAILED(hr)) {
    return hr;
  }

  // AppName is not a required parameter since it's only used for being able to
  // easily tell what application is there when reading the registry.
  VERIFY1(SUCCEEDED(client_key.SetValue(kRegValueAppName, product_name)));

  return S_OK;
}

HRESULT AppManager::UnregisterProduct(const GUID& product_guid) {
  return RegKey::DeleteKey(GetClientKeyName(GUID_NULL, product_guid), true);
}

}  // namespace omaha.

