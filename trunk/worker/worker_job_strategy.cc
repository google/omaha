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

#include "omaha/worker/worker_job_strategy.h"
#include <functional>
#include <algorithm>
#include "omaha/common/const_cmd_line.h"
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/logging.h"
#include "omaha/common/scoped_ptr_address.h"
#include "omaha/common/scope_guard.h"
#include "omaha/goopdate/command_line.h"
#include "omaha/goopdate/config_manager.h"
#include "omaha/goopdate/goopdate_helper.h"
#include "omaha/goopdate/goopdate_utils.h"
#include "omaha/goopdate/stats_uploader.h"
#include "omaha/setup/setup.h"
#include "omaha/worker/application_manager.h"
#include "omaha/worker/application_data.h"
#include "omaha/worker/job_observer.h"
#include "omaha/worker/ping.h"
#include "omaha/worker/ping_utils.h"
#include "omaha/worker/worker_job.h"
#include "omaha/worker/worker_metrics.h"

namespace omaha {

namespace {

// Google Update should never be not accepted since its EULA state is kept in
// the Update key.
// Uses RefHolder because std::not1 uses a reference to the parameter type,
// which results in an illegal reference to a reference if the parameter is
// just a const reference.
bool IsEulaAccepted(const RefHolder<ProductData> product_holder) {
  const ProductData& product = product_holder;
  const bool is_accepted = product.app_data().is_eula_accepted();
  ASSERT1(is_accepted ||
          !::IsEqualGUID(kGoopdateGuid, product.app_data().app_guid()));
  return is_accepted;
}

bool IsInstallDisabled(const ProductData& product) {
  return !ConfigManager::Instance()->CanInstallApp(
      product.app_data().app_guid());
}

bool IsUpdateDisabled(const ProductData& product, bool is_manual) {
  return !ConfigManager::Instance()->CanUpdateApp(
      product.app_data().app_guid(),
      is_manual);
}

// Removes apps that are not allowed to be updated because the app's EULA has
// not been accepted.
// Returns true if any apps were removed.
bool RemoveAppsIfUpdateDisallowedByEula(ProductDataVector* products) {
  ASSERT1(products);

  bool were_apps_removed = false;

  const ProductDataVector::iterator new_end = std::remove_if(
      products->begin(),
      products->end(),
      std::not1(std::ptr_fun(IsEulaAccepted)));
  if (new_end != products->end()) {
    const size_t previous_size = products->size();
    products->erase(new_end, products->end());
    were_apps_removed = true;
    metric_worker_apps_not_updated_eula = previous_size - products->size();
  }

  return were_apps_removed;
}

// Removes apps that are not allowed to be installed because Group Policy has
// disabled this.
// Returns true if any apps were removed.
bool RemoveAppsDisallowedByInstallGroupPolicy(
    ProductDataVector* products,
    CString* first_disallowed_app_name) {
  ASSERT1(products);
  ASSERT1(first_disallowed_app_name);
  ASSERT1(first_disallowed_app_name->IsEmpty());

  bool were_apps_removed = false;

  const ProductDataVector::iterator new_end = std::remove_if(
      products->begin(),
      products->end(),
      IsInstallDisabled);
  if (new_end != products->end()) {
    const size_t previous_size = products->size();
    *first_disallowed_app_name = new_end->app_data().display_name();

    products->erase(new_end, products->end());

    were_apps_removed = true;
    metric_worker_apps_not_installed_group_policy =
        previous_size - products->size();
  }

  return were_apps_removed;
}

// Disables updates for apps  for which this type of update is disallowed by
// Group Policy.
// Returns true if updates were disabled for any app.
bool DisableUpdateForAppsDisallowedByUpdateGroupPolicy(
    bool is_manual,
    ProductDataVector* products,
    CString* first_disallowed_app_name) {
  ASSERT1(products);
  ASSERT1(first_disallowed_app_name);
  ASSERT1(first_disallowed_app_name->IsEmpty());

  bool were_apps_disabled = false;

  for (ProductDataVector::iterator iter = products->begin();
       iter != products->end();
       ++iter) {
    if (!IsUpdateDisabled(*iter, is_manual)) {
      continue;
    }

    if (first_disallowed_app_name->IsEmpty()) {
      *first_disallowed_app_name = iter->app_data().display_name();
    }

    AppData modified_app_data = iter->app_data();
    modified_app_data.set_is_update_disabled(true);
    iter->set_app_data(modified_app_data);

    were_apps_disabled = true;
    metric_worker_apps_not_updated_group_policy++;
  }

  return were_apps_disabled;
}

class OnlineStrategy : public NetworkStrategy {
 public:
  explicit OnlineStrategy(WorkerJobStrategy* worker_job_strategy)
      : NetworkStrategy(worker_job_strategy) {}
  HRESULT DoUpdateCheck(const ProductDataVector& products) {
    return worker_job()->DoUpdateCheck(products);
  }
  HRESULT DownloadJobs() {
    return worker_job()->DownloadJobs();
  }
 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(OnlineStrategy);
};

class OfflineStrategy : public NetworkStrategy {
 public:
  explicit OfflineStrategy(WorkerJobStrategy* worker_job_strategy)
      : NetworkStrategy(worker_job_strategy) {}
  HRESULT DoUpdateCheck(const ProductDataVector& products) {
    return worker_job()->CreateOfflineJobs(products);
  }
  HRESULT DownloadJobs() {
    return S_OK;
  }
 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(OfflineStrategy);
};

}  // namespace

WorkerJobStrategy::WorkerJobStrategy(bool is_machine,
                                     const CString& lang)
    : is_machine_(is_machine),
      language_(lang),
      worker_job_(NULL) {
}

HRESULT WorkerJobStrategy::ProcessApps() {
  HRESULT hr = network_strategy()->DownloadJobs();
  if (FAILED(hr)) {
    return hr;
  }

  return worker_job()->InstallJobs();
}

HRESULT WorkerJobStrategy::DoUpdateCheck(const ProductDataVector& products) {
  return network_strategy()->DoUpdateCheck(products);
}

HRESULT WorkerJobStrategy::PostInstall() {
  return S_OK;
}

HRESULT UpdateAppsStrategy::PreUpdateCheck(ProductDataVector* products) {
  ASSERT1(products);
  ASSERT1(worker_job());

  HRESULT hr = PingUninstalledProducts();
  if (FAILED(hr)) {
    CORE_LOG(LW, (_T("[PingUninstalledApps failed][0x%08x]"), hr));
  }

  // Fill in products from the registered apps database.
  AppManager app_manager(is_machine());
  if (!app_manager.ShouldCheckForUpdates()) {
    OPT_LOG(L1, (_T("[Update check not needed at this time]")));
    return S_OK;
  }

  hr = app_manager.GetRegisteredProducts(products);

  if (FAILED(hr) || args_.install_source.IsEmpty()) {
    return hr;
  }

  for (size_t i = 0; i < products->size(); ++i) {
    AppData app_data = (*products)[i].app_data();
    app_data.set_install_source(args_.install_source);
    (*products)[i].set_app_data(app_data);
  }

#ifdef _DEBUG
  for (size_t i = 0; i < products->size(); ++i) {
    const GUID& app_guid = (*products)[i].app_data().app_guid();
    const CString client_state_key_path =
        goopdate_utils::GetAppClientStateKey(is_machine(),
                                             GuidToString(app_guid));
    ASSERT(RegKey::HasKey(client_state_key_path),
           (_T("[App Clients key does not have matching ClientState key][%s]"),
            GuidToString(app_guid)));
  }
#endif

  return S_OK;
}

// Not being able to update one or more apps is not an error as long as there is
// at least one app to update. This should always be the case because Omaha can
// always be updated.
HRESULT UpdateAppsStrategy::RemoveDisallowedApps(ProductDataVector* products) {
  RemoveAppsIfUpdateDisallowedByEula(products);
  if (products->empty()) {
    return GOOPDATE_E_APP_UPDATE_DISABLED_EULA_NOT_ACCEPTED;
  }

  DisableUpdateForAppsDisallowedByUpdateGroupPolicy(
      false,
      products,
      &first_disallowed_app_name_);
  if (products->empty()) {
    return GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY;
  }

  return S_OK;
}

HRESULT UpdateAppsStrategy::DoUpdateCheck(const ProductDataVector& products) {
  HRESULT hr = SendInstalledByOemPing(products);
  if (FAILED(hr)) {
    CORE_LOG(LW, (_T("[SendInstalledByOemPing failed][0x%08x]"), hr));
  }

  return WorkerJobStrategy::DoUpdateCheck(products);
}

// Updates the LastChecked value; called only after successful update checks.
HRESULT UpdateAppsStrategy::PostUpdateCheck() {
  AppManager app_manager(is_machine());
  HRESULT hr = app_manager.UpdateLastChecked();

  if (FAILED(hr)) {
    CORE_LOG(LW, (_T("[UpdateLastChecked failed][0x%08x]"), hr));
  }
  return S_OK;
}

HRESULT UpdateAppsStrategy::PingUninstalledProducts() const {
  CORE_LOG(L2, (_T("[UpdateAppsStrategy::PingUninstalledProducts]")));
  ASSERT1(worker_job()->ping());

  scoped_ptr<Request> uninstall_ping;
  HRESULT hr = BuildUninstallPing(is_machine(), address(uninstall_ping));
  if (FAILED(hr)) {
    return hr;
  }

  if (!uninstall_ping->get_request_count()) {
    return S_OK;
  }

  if (worker_job()->is_canceled()) {
    return GOOPDATE_E_WORKER_CANCELLED;
  }

  CORE_LOG(L2, (_T("[Sending uninstall ping]")));
  return worker_job()->ping()->SendPing(uninstall_ping.get());
}

HRESULT UpdateAppsStrategy::SendInstalledByOemPing(
    const ProductDataVector& products) const {
  CORE_LOG(L2, (_T("[UpdateAppsStrategy::SendInstalledByOemPing()]")));
  ASSERT1(!products.empty());

  Request request(is_machine());

  for (size_t i = 0; i < products.size(); ++i) {
    const ProductData& product_data = products[i];
    const AppData& app_data = product_data.app_data();
    if (app_data.is_oem_install()) {
      ASSERT1(!::IsEqualGUID(kGoopdateGuid, app_data.app_guid()));
      AppRequestData app_request_data(app_data);
      PingEvent ping_event(PingEvent::EVENT_INSTALL_OEM_FIRST_CHECK,
                           PingEvent::EVENT_RESULT_SUCCESS,
                           0,  // error code
                           0,  // extra code 1
                           app_data.previous_version());
      app_request_data.AddPingEvent(ping_event);
      AppRequest app_request(app_request_data);
      request.AddAppRequest(app_request);
    }
  }

  if (!request.get_request_count()) {
    return S_OK;
  }

  if (worker_job()->is_canceled()) {
    return GOOPDATE_E_WORKER_CANCELLED;
  }

  HRESULT hr = worker_job()->ping()->SendPing(&request);
  if (FAILED(hr)) {
    return hr;
  }

  AppManager app_manager(is_machine());
  for (size_t i = 0; i < products.size(); ++i) {
    const ProductData& product_data = products[i];
    const AppData& app_data = product_data.app_data();
    if (app_data.is_oem_install()) {
      app_manager.ClearOemInstalled(app_data.parent_app_guid(),
                                    app_data.app_guid());
    }
  }

  return S_OK;
}

InstallAppsStrategy::InstallAppsStrategy(bool is_machine,
                                         const CommandLineArgs& args)
    : WorkerJobStrategy(is_machine, args.extra.language),
      args_(args) {
}

HRESULT InstallAppsStrategy::PreUpdateCheck(ProductDataVector* products) {
  ASSERT1(products);
  ASSERT1(worker_job());
  AppManager app_manager(is_machine());
  app_manager.ConvertCommandLineToProductData(args_, products);

  return S_OK;
}

HRESULT InstallAppsStrategy::RemoveDisallowedApps(ProductDataVector* products) {
  if (RemoveAppsDisallowedByInstallGroupPolicy(products,
                                               &first_disallowed_app_name_)) {
    return GOOPDATE_E_APP_INSTALL_DISABLED_BY_POLICY;
  }

  return S_OK;
}

HRESULT InstallAppsStrategy::PostUpdateCheck() {
  return S_OK;
}

HRESULT InstallAppsStrategy::PostInstall() {
  // TODO(omaha): Maybe move to UI class.
  if (args_.is_silent_set) {
    return S_OK;
  }

  const Jobs& jobs = worker_job()->jobs();
  if (jobs.empty()) {
    return S_OK;
  }

  Job* primary_job = jobs[0];

  // This is a temporary workaround for Earth-Chrome bundles until we implement
  // better bundle support. Chrome is installed last and is thus the
  // "primary app", but we want to launch Earth so specify it as primary here.
  // Earth has two GUIDs that must be supported.
  // Using StringToGuid below ensure there is no mismatch in upper/lower case.
  // Note that Chrome's onsuccess value is still used to determine whether to
  // close the UI.
  const TCHAR* const kChromeGuid = _T("{8A69D345-D564-463C-AFF1-A69D9E530F96}");
  const TCHAR* const kEarthMachineGuid =
      _T("{74AF07D8-FB8F-4D51-8AC7-927721D56EBB}");
  const TCHAR* const kEarthUserGuid =
      _T("{0A52903D-0FBF-439A-93E4-CB609A2F63DB}");

  if (jobs.size() == 2 &&
      jobs[0]->app_data().app_guid() == StringToGuid(kChromeGuid) &&
      (jobs[1]->app_data().app_guid() == StringToGuid(kEarthMachineGuid) ||
       jobs[1]->app_data().app_guid() == StringToGuid(kEarthUserGuid))) {
    primary_job = jobs[1];
  }

  ASSERT1(primary_job);
  VERIFY1(SUCCEEDED(primary_job->LaunchCmdLine()));

  return S_OK;
}

bool InstallAppsStrategy::ShouldLaunchBrowserOnUpdateCheckError() const {
  return !args_.is_silent_set;
}

HRESULT InstallGoopdateAndAppsStrategy::PreUpdateCheck(
    ProductDataVector* products) {
  ASSERT1(products);
  HRESULT hr = InstallAppsStrategy::PreUpdateCheck(products);
  if (FAILED(hr)) {
    return hr;
  }

  hr = FinishGoogleUpdateInstall(args_,
                                 is_machine(),
                                 false,
                                 worker_job()->ping(),
                                 job_observer_);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[FinishGoogleUpdateInstall failed][0x%08x]"), hr));
    return hr;
  }
  return S_OK;
}

HRESULT OnDemandUpdateStrategy::InitializeDecorator(
    IJobObserver* observer,
    WorkerComWrapperShutdownCallBack* call_back) {
  CORE_LOG(L2, (_T("[OnDemandUpdateStrategy::InitializeDecorator]")));
  HRESULT hr =
      CComObject<JobObserverCOMDecorator>::CreateInstance(&job_observer_com_);
  ASSERT(SUCCEEDED(hr),
      (_T("[JobObserverCOMDecorator CreateInstance returned 0x%x]"), hr));
  if (FAILED(hr)) {
    return hr;
  }

  job_observer_com_scoped_holder_ = job_observer_com_;
  job_observer_com_->Initialize(observer, call_back);
  return S_OK;
}

HRESULT OnDemandUpdateStrategy::Init(
    GUID guid,
    IJobObserver* observer,
    WorkerComWrapperShutdownCallBack* call_back) {
  CORE_LOG(L3, (_T("[OnDemandUpdateStrategy::Init][%s]"), GuidToString(guid)));
  AppManager app_manager(is_machine());
  ProductData product_data;

  HRESULT hr = app_manager.ReadProductDataFromStore(guid, &product_data);
  if (FAILED(hr)) {
    ASSERT1(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) == hr);
    return GOOPDATE_E_APP_NOT_REGISTERED;
  }
  if (product_data.app_data().is_uninstalled()) {
    return GOOPDATE_E_APP_UNINSTALLED;
  }

  AppData app_data = product_data.app_data();
  app_data.set_install_source(is_update_check_only_ ?
                              kCmdLineInstallSource_OnDemandCheckForUpdate :
                              kCmdLineInstallSource_OnDemandUpdate);
  app_data.set_previous_version(app_data.version());
  product_data.set_app_data(app_data);

  products_.push_back(product_data);

  hr = InitializeDecorator(observer, call_back);
  if (FAILED(hr)) {
    return hr;
  }

  return S_OK;
}

JobObserver* OnDemandUpdateStrategy::GetJobObserver() const {
  return job_observer_com_;
}

HRESULT OnDemandUpdateStrategy::PreUpdateCheck(ProductDataVector* products) {
  ASSERT1(products);
  ASSERT1(worker_job());
  ASSERT1(!products_.empty());
  *products = products_;
  return S_OK;
}

HRESULT OnDemandUpdateStrategy::RemoveDisallowedApps(
    ProductDataVector* products) {
  if (RemoveAppsIfUpdateDisallowedByEula(products)) {
    return GOOPDATE_E_APP_UPDATE_DISABLED_EULA_NOT_ACCEPTED;
  }
  if (DisableUpdateForAppsDisallowedByUpdateGroupPolicy(
          true,
          products,
          &first_disallowed_app_name_)) {
    return GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY;
  }

  return S_OK;
}

HRESULT OnDemandUpdateStrategy::PostUpdateCheck() {
  return S_OK;
}

HRESULT OnDemandUpdateStrategy::ProcessApps() {
  HRESULT hr = S_OK;
  if (!is_update_check_only_ && !worker_job()->jobs().empty()) {
    // Only call process apps if we have been asked to perform
    // an update(and not just an update check), and an update is
    // available.
    return WorkerJobStrategy::ProcessApps();
  } else {
    const TCHAR* const text = _T("Update Check Completed");
    worker_job()->NotifyCompleted(COMPLETION_SUCCESS, S_OK, text, products_);
    worker_job()->CompleteAllNonCompletedJobs(COMPLETION_SUCCESS, S_OK, text);
  }

  return hr;
}

// job_observer can be NULL.
// TODO(omaha): Remove job_observer parameter after unifying Setup.
WorkerJobStrategy* WorkerJobStrategyFactory::CreateInstallStrategy(
    bool is_machine,
    const CommandLineArgs& args,
    JobObserver* job_observer) {
  WorkerJobStrategy* strategy(NULL);

#pragma warning(push)
// C4061: enumerator 'xxx' in switch of enum 'yyy' is not explicitly handled by
// a case label.
#pragma warning(disable : 4061)
  switch (args.mode) {
    // TODO(omaha): Remove Install mode after unifying Setup. It is only here
    // to support error reporting from DisplaySetupError().
    case COMMANDLINE_MODE_INSTALL:
    case COMMANDLINE_MODE_IG:
      strategy = new InstallGoopdateAndAppsStrategy(is_machine,
                                                    args,
                                                    job_observer);
      break;
    case COMMANDLINE_MODE_HANDOFF_INSTALL:
      strategy = new InstallAppsStrategy(is_machine, args);
      break;
    default:
      ASSERT1(false);
  }
#pragma warning(pop)

  NetworkStrategy* network_strategy(NULL);
  if (args.is_offline_set) {
    network_strategy = new OfflineStrategy(strategy);
  } else {
    network_strategy = new OnlineStrategy(strategy);
  }

  strategy->set_network_strategy(network_strategy);

  return strategy;
}

WorkerJobStrategy* WorkerJobStrategyFactory::CreateUpdateAppsStrategy(
    bool is_machine, const CommandLineArgs& args) {
  WorkerJobStrategy* strategy = new UpdateAppsStrategy(is_machine, args);
  strategy->set_network_strategy(new OnlineStrategy(strategy));
  return strategy;
}

OnDemandUpdateStrategy* WorkerJobStrategyFactory::CreateOnDemandStrategy(
    bool is_update_check_only,
    const CString& lang,
    bool is_machine) {
  OnDemandUpdateStrategy* strategy =
      new OnDemandUpdateStrategy(is_update_check_only, lang, is_machine);
  strategy->set_network_strategy(new OnlineStrategy(strategy));
  return strategy;
}

}  // namespace omaha
