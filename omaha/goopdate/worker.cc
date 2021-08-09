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

#include "omaha/goopdate/worker.h"
#include "omaha/goopdate/worker_internal.h"

#include <atlbase.h>
#include <atlstr.h>
#include <memory>

#include "omaha/base/app_util.h"
#include "omaha/base/const_object_names.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/file.h"
#include "omaha/base/firewall_product_detection.h"
#include "omaha/base/highres_timer-win32.h"
#include "omaha/base/logging.h"
#include "omaha/base/path.h"
#include "omaha/base/reactor.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/scoped_impersonation.h"
#include "omaha/base/system.h"
#include "omaha/base/utils.h"
#include "omaha/base/thread_pool_callback.h"
#include "omaha/base/vistautil.h"
#include "omaha/common/app_registry_utils.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/event_logger.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/common/ping.h"
#include "omaha/common/ping_event.h"
#include "omaha/common/update_request.h"
#include "omaha/common/update_response.h"
#include "omaha/common/web_services_client.h"
#include "omaha/goopdate/app_manager.h"
#include "omaha/goopdate/download_manager.h"
#include "omaha/goopdate/goopdate.h"
#include "omaha/goopdate/install_manager.h"
#include "omaha/goopdate/model.h"
#include "omaha/goopdate/offline_utils.h"
#include "omaha/goopdate/server_resource.h"
#include "omaha/goopdate/string_formatter.h"
#include "omaha/goopdate/update_request_utils.h"
#include "omaha/goopdate/update_response_utils.h"
#include "omaha/goopdate/worker_metrics.h"
#include "omaha/goopdate/worker_utils.h"

namespace omaha {

namespace internal {

void RecordUpdateAvailableUsageStats() {
  AppManager& app_manager = *AppManager::Instance();

  DWORD update_responses(0);
  DWORD64 time_since_first_response_ms(0);
  app_manager.ReadUpdateAvailableStats(kGoopdateGuid,
                                       &update_responses,
                                       &time_since_first_response_ms);
  if (update_responses) {
    metric_worker_self_update_responses = update_responses;
  }
  if (time_since_first_response_ms) {
    metric_worker_self_update_response_time_since_first_ms =
        time_since_first_response_ms;
  }

  AppIdVector registered_app_ids;
  HRESULT hr = app_manager.GetRegisteredApps(&registered_app_ids);
  if (FAILED(hr)) {
    return;
  }

  // These store information about the app with the most update responses.
  GUID max_responses_app(GUID_NULL);
  DWORD max_responses(0);
  DWORD64 max_responses_time_since_first_response_ms(0);

  for (size_t i = 0; i < registered_app_ids.size(); ++i) {
    const CString& app_id = registered_app_ids[i];
    GUID app_guid = GUID_NULL;

    if (FAILED(StringToGuidSafe(app_id, &app_guid))) {
      ASSERT(false, (_T("Invalid App ID: %s"), app_id));
      continue;
    }

    if (::IsEqualGUID(kGoopdateGuid, app_guid)) {
      continue;
    }

    update_responses = 0;
    time_since_first_response_ms = 0;
    app_manager.ReadUpdateAvailableStats(app_guid,
                                         &update_responses,
                                         &time_since_first_response_ms);

    if (max_responses < update_responses) {
      max_responses_app = app_guid;
      max_responses = update_responses;
      max_responses_time_since_first_response_ms = time_since_first_response_ms;
    }
  }

  if (max_responses) {
    metric_worker_app_max_update_responses_app_high =
        GetGuidMostSignificantUint64(max_responses_app);
    metric_worker_app_max_update_responses = max_responses;
    metric_worker_app_max_update_responses_ms_since_first =
        max_responses_time_since_first_response_ms;
  }
}

// Will block if any apps are being installed.
HRESULT AddUninstalledAppsPings(AppBundle* app_bundle) {
  CORE_LOG(L3, (_T("[AddUninstalledAppsPings]")));
  ASSERT1(app_bundle);

  AppManager& app_manager = *AppManager::Instance();

  // Ensure that no installers are running while determining uninstalled apps
  // and information about them.
  __mutexScope(app_manager.GetRegistryStableStateLock());

  AppIdVector uninstalled_app_ids;
  HRESULT hr = app_manager.GetUninstalledApps(&uninstalled_app_ids);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[GetUninstalledApps failed][0x%08x]"), hr));
    return hr;
  }

  if (!uninstalled_app_ids.size()) {
    return S_OK;
  }

  for (size_t i = 0; i < uninstalled_app_ids.size(); ++i) {
    CString app_id = uninstalled_app_ids[i];
    CORE_LOG(L3, (_T("[found uninstalled product][%s]"), app_id));

    // Omaha uninstall ping is sent by the Uninstall function in setup.
    if (app_id == kGoogleUpdateAppId) {
      CORE_LOG(L3, (_T("[skipping Omaha]")));
      continue;
    }

    App* app = NULL;
    hr = app_bundle->CreateUninstalledApp(app_id, &app);
    if (FAILED(hr)) {
      return hr;
    }

    PingEventPtr ping_event(
        new PingEvent(PingEvent::EVENT_UNINSTALL,
                      PingEvent::EVENT_RESULT_SUCCESS,
                      0,    // error code
                      0));  // extra code 1
    app->AddPingEvent(ping_event);

    // TODO(omaha3) It would be nice to call RemoveClientState() only after the
    // uninstall ping is sent so that the values are not deleted if the ping
    // fails. However, this would allow race conditions between installers
    // and the uninstall ping unless app_manager.GetRegistryStableStateLock() is
    // held from the ping building through the send, which is not currently
    // feasible since the ping is sent in the AppBundle destructor, and the
    // AppBundle's lifetime is controlled by the client. Improving the ping
    // architecture, such as having a ping queue managed by the Worker, may
    // enable this.
    VERIFY_SUCCEEDED(app_manager.RemoveClientState(app->app_guid()));
  }

  return S_OK;
}

HRESULT SendOemInstalledPing(bool is_machine, const CString& session_id) {
  CORE_LOG(L3, (_T("[SendOemInstalledPing]")));

  std::vector<CString> oem_installed_apps;
  HRESULT hr = AppManager::Instance()->GetOemInstalledAndEulaAcceptedApps(
      &oem_installed_apps);
  if (FAILED(hr)) {
    return hr;
  }

  PingEventPtr oem_ping_event(
      new PingEvent(PingEvent::EVENT_INSTALL_OEM_FIRST_CHECK,
                    PingEvent::EVENT_RESULT_SUCCESS,
                    0,
                    0));
  Ping ping(is_machine, session_id, CString());
  ping.LoadAppDataFromRegistry(oem_installed_apps);
  ping.BuildAppsPing(oem_ping_event);
  hr = SendReliablePing(&ping, false);
  if (FAILED(hr)) {
    CORE_LOG(L3, (_T("[SendOemInstalledPing failed][0x%08x]"), hr));
    return hr;
  }

  AppManager::Instance()->ClearOemInstalled(oem_installed_apps);

  return S_OK;
}

void SendCupFailurePing(bool is_machine,
                        const CString& session_id,
                        const CString& install_source,
                        const HRESULT extra_code) {
  Ping ping(is_machine, session_id, install_source);
  PingEventPtr ping_event(new PingEvent(PingEvent::EVENT_DEBUG,
                                        PingEvent::EVENT_RESULT_ERROR,
                                        PingEvent::DEBUG_SOURCE_CUP_FAILURE,
                                        static_cast<int>(extra_code)));
  ping.LoadOmahaDataFromRegistry();
  ping.BuildOmahaPing(NULL, NULL, ping_event);

  HRESULT hr = SendReliablePing(&ping, false);
  if (FAILED(hr)) {
    CORE_LOG(LW, (_T("[SendCupFailurePing failed][0x%x]"), hr));
  }
}

}  // namespace internal

Worker::Worker()
    : is_machine_(false),
      lock_count_(0),
      single_instance_hr_(E_FAIL) {
  CORE_LOG(L1, (_T("[Worker::Worker]")));

  reactor_.reset(new Reactor);
  shutdown_handler_.reset(new ShutdownHandler);
  model_.reset(new Model(this));
}

Worker::~Worker() {
  CORE_LOG(L1, (_T("[Worker::~Worker]")));

  ASSERT1(!lock_count_);
  // TODO(omaha3): Remove when Run() is used. See TODO in GoogleUpdate::Main().
  Stop();

  AppManager::DeleteInstance();
}

Worker* const Worker::kInvalidInstance = reinterpret_cast<Worker* const>(-1);
Worker* Worker::instance_              = NULL;

Worker& Worker::Instance() {
  // Getting the instance after the instance has been deleted is a bug in
  // the logic of the program.
  ASSERT1(instance_ != kInvalidInstance);
  if (!instance_) {
    instance_ = new Worker();
  }
  return *instance_;
}

HRESULT Worker::EnsureSingleInstance() {
  __mutexScope(model()->lock());

  CORE_LOG(L2, (_T("[Worker::EnsureSingleInstance]")));
  if (single_instance_.get()) {
    return single_instance_hr_;
  }

  NamedObjectAttributes attr;
  GetNamedObjectAttributes(kGoogleUpdate3SingleInstance, is_machine_, &attr);

  single_instance_.reset(new ProgramInstance(attr.name));
  if (!single_instance_.get()) {
    CORE_LOG(LE, (_T("[Failed to create Worker Single Instance]")));
    single_instance_hr_ = E_OUTOFMEMORY;
    return single_instance_hr_;
  }

  if (!single_instance_->EnsureSingleInstance()) {
    CORE_LOG(LW, (_T("[Another Worker instance already running]")));
    single_instance_hr_ = GOOPDATE_E_INSTANCES_RUNNING;
    return single_instance_hr_;
  }

  single_instance_hr_ = S_OK;
  return single_instance_hr_;
}

int Worker::Lock() {
  __mutexScope(model()->lock());

  ++lock_count_;
  return _pAtlModule->Lock();
}

int Worker::Unlock() {
  __mutexScope(model()->lock());

  if (!--lock_count_) {
    single_instance_.reset();
    single_instance_hr_ = E_FAIL;
  }

  return _pAtlModule->Unlock();
}

HRESULT Worker::Initialize(bool is_machine) {
  CORE_LOG(L1, (_T("[Worker::Initialize][%d]"), is_machine));

  is_machine_ = is_machine;

  HRESULT hr = EnsureSingleInstance();
  if (FAILED(hr)) {
    return hr;
  }

  hr = AppManager::CreateInstance(is_machine_);
  if (FAILED(hr)) {
    return hr;
  }

  download_manager_.reset(new DownloadManager(is_machine_));

  hr = download_manager_->Initialize();
  if (FAILED(hr)) {
    return hr;
  }

  install_manager_.reset(new InstallManager(&model_->lock(), is_machine_));
  hr = install_manager_->Initialize();
  if (FAILED(hr)) {
    return hr;
  }

  return S_OK;
}

void Worker::Stop() {
  // Stop the concurrent objects to avoid spurious events.
  shutdown_handler_.reset();
  reactor_.reset();

  // TODO(omaha3): Remove when Run() is used. See TODO in GoogleUpdate::Main().
  // Until then this call is necessary to wait for threads to complete on
  // destruction.
  // TODO(omaha3): Is it correct that the thread pool does not wait for the
  // threads if !shutdown_event_?
  Goopdate::Instance().Stop();
}

HRESULT Worker::Run() {
  HRESULT hr = DoRun();
  Stop();

  return hr;
}

HRESULT Worker::DoRun() {
  CORE_LOG(L1, (_T("[Worker::DoRun]")));

  HRESULT hr = InitializeShutDownHandler();
  if (FAILED(hr)) {
    return hr;
  }

  return hr;
}

HRESULT Worker::InitializeShutDownHandler() {
  CORE_LOG(L3, (_T("[InitializeShutDownHandler]")));
  return shutdown_handler_->Initialize(reactor_.get(), this, is_machine_);
}

HRESULT Worker::Shutdown() {
  CORE_LOG(L2, (_T("[Worker::Shutdown]")));
  return S_OK;
}

void Worker::CollectAmbientUsageStats() {
#if 0
  // The firewall detection code has been proved to block on some
  // computers in the IWbemLocator::ConnectServer call even though a
  // timeout was specified in the call.
  CString name, version;
  HRESULT hr = firewall_detection::Detect(&name, &version);
  bool has_software_firewall = SUCCEEDED(hr) && !name.IsEmpty();
  metric_worker_has_software_firewall.Set(has_software_firewall);
#endif

  if (System::IsRunningOnBatteries()) {
    ++metric_worker_silent_update_running_on_batteries;
  }

  metric_worker_shell_version = app_util::GetVersionFromModule(NULL);

  metric_worker_is_windows_installing.Set(IsWindowsInstalling());

  bool is_uac_on(false);
  if (vista_util::IsVistaOrLater() &&
      SUCCEEDED(vista_util::IsUACOn(&is_uac_on))) {
    metric_worker_is_uac_disabled.Set(!is_uac_on);
  }
}

HRESULT Worker::CheckForUpdateAsync(AppBundle* app_bundle) {
  CORE_LOG(L3, (_T("[Worker::CheckForUpdateAsync][0x%p]"), app_bundle));

  ASSERT1(app_bundle);
  ASSERT1(model_->IsLockedByCaller());

  std::shared_ptr<AppBundle> shared_bundle(app_bundle->controlling_ptr());
  HRESULT hr = QueueDeferredFunctionCall0(shared_bundle,
                                          &Worker::CheckForUpdate);
  if (FAILED(hr)) {
    return hr;
  }

  for (size_t i = 0; i != shared_bundle->GetNumberOfApps(); ++i) {
    App* app = shared_bundle->GetApp(i);
    app->QueueUpdateCheck();
  }

  return S_OK;
}

void Worker::CheckForUpdate(std::shared_ptr<AppBundle> app_bundle) {
  CORE_LOG(L3, (_T("[Worker::CheckForUpdate][0x%p]"), app_bundle.get()));
  ASSERT1(app_bundle.get());

  bool is_check_successful = false;
  CheckForUpdateHelper(app_bundle.get(), &is_check_successful);

  app_bundle->CompleteAsyncCall();
}

// *is_check_successful is true if the network request was successful and the
// response was successfully parsed. The elements' status need not be success,
// but an invalid response, such as HTML from a proxy, should result in false.
// TODO(omaha): Unit test this by mocking update_check_client.
void Worker::CheckForUpdateHelper(AppBundle* app_bundle,
                                  bool* is_check_successful) {
  ASSERT1(app_bundle);
  ASSERT1(is_check_successful);
  *is_check_successful = false;

  if (ConfigManager::Instance()->CanUseNetwork(is_machine_)) {
    VERIFY_SUCCEEDED(internal::SendOemInstalledPing(
        is_machine_, app_bundle->session_id()));
  }

  scoped_impersonation impersonate_user(app_bundle->impersonation_token());
  HRESULT hr = impersonate_user.result();
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[Impersonation failed][0x%08x]"), hr));
    return;
  }

  std::unique_ptr<xml::UpdateRequest> update_request(
      xml::UpdateRequest::Create(is_machine_,
                                 app_bundle->session_id(),
                                 app_bundle->install_source(),
                                 app_bundle->origin_url()));

  std::unique_ptr<xml::UpdateResponse> update_response(
      xml::UpdateResponse::Create());

  CallAsSelfAndImpersonate2(this,
                            &Worker::DoPreUpdateCheck,
                            app_bundle,
                            update_request.get());

  // This is a blocking call on the network.
  hr = DoUpdateCheck(app_bundle,
                     update_request.get(),
                     update_response.get());
  if (FAILED(hr)) {
    CORE_LOG(LW, (_T("[DoUpdateCheck failed][0x%08x]"), hr));
  }
  *is_check_successful = SUCCEEDED(hr);

  CallAsSelfAndImpersonate3(this,
                            &Worker::DoPostUpdateCheck,
                            app_bundle,
                            hr,
                            update_response.get());
  if (IsCupError(hr)) {
    CORE_LOG(L3, (_T("[CUP failed][%#08x]"), hr));
    // Only send the CUP debug ping when there is no "retry after" in effect.
    const int retry_after_sec(
        app_bundle->update_check_client()->retry_after_sec());
    if (retry_after_sec <= 0) {
      internal::SendCupFailurePing(is_machine_,
                                   app_bundle->session_id(),
                                   app_bundle->install_source(),
                                   hr);
    }
  }

  CString event_description;
  SafeCStringFormat(&event_description, _T("Update check. Status = 0x%08x"),
                    hr);
  CString event_text;
  CString url;
  VERIFY_SUCCEEDED(ConfigManager::Instance()->GetUpdateCheckUrl(&url));
  SafeCStringFormat(&event_text, _T("url=%s\n%s"),
                    url,
                    app_bundle->FetchAndResetLogText());

  WriteEventLog(EVENTLOG_INFORMATION_TYPE,
                kUpdateEventId,
                event_description,
                event_text);
}

HRESULT Worker::DownloadAsync(AppBundle* app_bundle) {
  CORE_LOG(L3, (_T("[Worker::DownloadAsync][0x%p]"), app_bundle));

  ASSERT1(app_bundle);
  ASSERT1(model_->IsLockedByCaller());

  std::shared_ptr<AppBundle> shared_bundle(app_bundle->controlling_ptr());
  HRESULT hr = QueueDeferredFunctionCall0(shared_bundle, &Worker::Download);
  if (FAILED(hr)) {
    return hr;
  }

  for (size_t i = 0; i != shared_bundle->GetNumberOfApps(); ++i) {
    App* app = shared_bundle->GetApp(i);
    app->QueueDownload();
  }

  return S_OK;
}

void Worker::Download(std::shared_ptr<AppBundle> app_bundle) {
  CORE_LOG(L3, (_T("[Worker::Download][0x%p]"), app_bundle.get()));
  ASSERT1(app_bundle.get());

  scoped_impersonation impersonate_user(app_bundle->impersonation_token());
  HRESULT hr = impersonate_user.result();
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[Impersonation failed][0x%08x]"), hr));
    return;
  }

  const size_t num_apps = app_bundle->GetNumberOfApps();

  for (size_t i = 0; i != num_apps; ++i) {
    App* app = app_bundle->GetApp(i);

    ASSERT1(app->state() == STATE_WAITING_TO_DOWNLOAD ||
            app->state() == STATE_NO_UPDATE ||
            app->state() == STATE_ERROR);

    // This is a blocking call on the network.
    app->Download(download_manager_.get());

    ASSERT1(app->state() == STATE_READY_TO_INSTALL ||
            app->state() == STATE_NO_UPDATE ||
            app->state() == STATE_ERROR);
  }

  WriteEventLog(EVENTLOG_INFORMATION_TYPE,
                kDownloadEventId,
                _T("Bundle download"),
                app_bundle->FetchAndResetLogText());
  app_bundle->CompleteAsyncCall();
}

HRESULT Worker::DownloadAndInstallAsync(AppBundle* app_bundle) {
  CORE_LOG(L3, (_T("[Worker::DownloadAndInstallAsync][0x%p]"), app_bundle));

  ASSERT1(app_bundle);
  ASSERT1(model_->IsLockedByCaller());

  std::shared_ptr<AppBundle> shared_bundle(app_bundle->controlling_ptr());
  HRESULT hr = QueueDeferredFunctionCall0(shared_bundle,
                                          &Worker::DownloadAndInstall);
  if (FAILED(hr)) {
    return hr;
  }

  for (size_t i = 0; i != shared_bundle->GetNumberOfApps(); ++i) {
    App* app = shared_bundle->GetApp(i);
    // Queue the download or install depending on the current state. The correct
    // one must be queued so that all apps are moved into waiting now and remain
    // there until the download or install starts for that app.
    app->QueueDownloadOrInstall();
  }

  return S_OK;
}

void Worker::DownloadAndInstall(std::shared_ptr<AppBundle> app_bundle) {
  CORE_LOG(L3, (_T("[Worker::DownloadAndInstall][0x%p]"), app_bundle.get()));
  ASSERT1(app_bundle.get());

  // If any applications have been uninstalled and /ua hasn't run yet, queue
  // their uninstall pings now, and clear out their ClientState.  This ensures
  // that they'll have a clean slate in the case of a uninstall+reinstall.
  // However, continue with the download/install if it fails.
  HRESULT hr = internal::AddUninstalledAppsPings(app_bundle.get());
  if (FAILED(hr)) {
    CORE_LOG(LW, (_T("[AddUninstalledAppsPings failed][0x%08x]"), hr));
  }

  DownloadAndInstallHelper(app_bundle.get());

  app_bundle->CompleteAsyncCall();
}

void Worker::DownloadAndInstallHelper(AppBundle* app_bundle) {
  ASSERT1(app_bundle);

  scoped_impersonation impersonate_user(app_bundle->impersonation_token());
  HRESULT hr = impersonate_user.result();
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[Impersonation failed][0x%08x]"), hr));
    return;
  }

  const size_t num_apps = app_bundle->GetNumberOfApps();

  for (size_t i = 0; i != num_apps; ++i) {
    App* app = app_bundle->GetApp(i);

    ASSERT1(app->state() == STATE_WAITING_TO_DOWNLOAD ||
            app->state() == STATE_WAITING_TO_INSTALL ||
            app->state() == STATE_NO_UPDATE ||
            app->state() == STATE_ERROR);

    // Download the app if it has not already been downloaded.
    // This is a blocking call on the network.
    app->Download(download_manager_.get());

    ASSERT1(app->state() == STATE_READY_TO_INSTALL ||    // Downloaded above.
            app->state() == STATE_WAITING_TO_INSTALL ||  // Downloaded earlier.
            app->state() == STATE_NO_UPDATE ||
            app->state() == STATE_ERROR);

    app->QueueInstall();

    // This is a blocking call on the app installer.
    CallAsSelfAndImpersonate1(
        app,
        &App::Install,
        install_manager_.get());

    ASSERT1(app->state() == STATE_INSTALL_COMPLETE ||
            app->state() == STATE_NO_UPDATE ||
            app->state() == STATE_ERROR);
  }

  WriteEventLog(EVENTLOG_INFORMATION_TYPE,
                kUpdateEventId,
                _T("Application update/install"),
                app_bundle->FetchAndResetLogText());
}


HRESULT Worker::UpdateAllAppsAsync(AppBundle* app_bundle) {
  CORE_LOG(L3, (_T("[Worker::UpdateAllAppsAsync][0x%p]"), app_bundle));

  ASSERT1(app_bundle);
  ASSERT1(model_->IsLockedByCaller());

  std::shared_ptr<AppBundle> shared_bundle(app_bundle->controlling_ptr());
  HRESULT hr = QueueDeferredFunctionCall0(shared_bundle,
                                          &Worker::UpdateAllApps);
  if (FAILED(hr)) {
    return hr;
  }

  for (size_t i = 0; i != shared_bundle->GetNumberOfApps(); ++i) {
    App* app = shared_bundle->GetApp(i);
    app->QueueUpdateCheck();
  }

  return S_OK;
}

// Runs through all steps regardless of error. App state machine handles errors.
void Worker::UpdateAllApps(std::shared_ptr<AppBundle> app_bundle) {
  CORE_LOG(L3, (_T("[Worker::UpdateAllApps][0x%p]"), app_bundle.get()));
  ASSERT1(app_bundle.get());

  bool is_check_successful = false;
  CheckForUpdateHelper(app_bundle.get(), &is_check_successful);

  if (is_check_successful) {
    HRESULT hr = goopdate_utils::UpdateLastChecked(is_machine_);
    ASSERT(SUCCEEDED(hr), (_T("UpdateLastChecked failed with 0x%08x"), hr));
  }

  for (size_t i = 0; i != app_bundle->GetNumberOfApps(); ++i) {
    App* app = app_bundle->GetApp(i);
    app->QueueDownloadOrInstall();
  }

  HRESULT hr = internal::AddUninstalledAppsPings(app_bundle.get());
  if (FAILED(hr)) {
    CORE_LOG(LW, (_T("[AddUninstalledAppsPings failed][0x%08x]"), hr));
  }

  DownloadAndInstallHelper(app_bundle.get());

  internal::RecordUpdateAvailableUsageStats();
  CollectAmbientUsageStats();

  app_bundle->CompleteAsyncCall();
}

HRESULT Worker::DownloadPackageAsync(Package* package) {
  ASSERT1(package);
  ASSERT1(model_->IsLockedByCaller());

  std::shared_ptr<AppBundle> shared_bundle =
      package->app_version()->app()->app_bundle()->controlling_ptr();
  CORE_LOG(L3, (_T("[Worker::DownloadPackageAsync][0x%p][0x%p]"),
      shared_bundle.get(), package));

  HRESULT hr = QueueDeferredFunctionCall1<Package*>(shared_bundle,
                                                    package,
                                                    &Worker::DownloadPackage);
  if (FAILED(hr)) {
    return hr;
  }

  App* app = package->app_version()->app();
  app->QueueDownload();

  return S_OK;
}

void Worker::DownloadPackage(std::shared_ptr<AppBundle> app_bundle,
                             Package* package) {
  CORE_LOG(L3, (_T("[Worker::DownloadPackage][0x%p][0x%p]"),
      app_bundle.get(), package));
  ASSERT1(app_bundle.get());
  ASSERT1(package);

  scoped_impersonation impersonate_user(app_bundle->impersonation_token());
  HRESULT hr = impersonate_user.result();
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[Impersonation failed][0x%08x]"), hr));
    return;
  }

  UNREFERENCED_PARAMETER(package);

  // TODO(omaha): implement downloading a package.

  // TODO(omaha): The event log function should be modified depends on
  // how the download is implemented.
  //   * whether HRESULT is needed.
  //   * whether the app_bundle has the log text.
  //   * additional information needed?
  CString event_text;
  SafeCStringFormat(&event_text, _T("Package: %s\n%s"),
                    package->filename(),
                    app_bundle->FetchAndResetLogText());

  WriteEventLog(EVENTLOG_INFORMATION_TYPE,
                kDownloadEventId,
                _T("Package download"),
                event_text);
  app_bundle->CompleteAsyncCall();
}

// TODO(omaha3): Implement this and enforce the postcondition that the
// bundle object is not busy before the function returns.
HRESULT Worker::Stop(AppBundle* app_bundle) {
  CORE_LOG(L3, (_T("[Worker::Stop][0x%p]"), app_bundle));

  ASSERT1(app_bundle);
  ASSERT1(model_->IsLockedByCaller());

  // Cancels update check client but not the ping client since we need to send
  // cancellation ping.
  WebServicesClientInterface* update_check_client(
      app_bundle->update_check_client());
  if (update_check_client) {
    update_check_client->Cancel();
  }

  // TODO(omaha3): What do we do with active installs? We can at least cancel
  // the InstallManager/InstallerWrapper if it has not started.

  // Cancel any apps that might have pending operations.
  for (size_t i = 0; i != app_bundle->GetNumberOfApps(); ++i) {
    App* app = app_bundle->GetApp(i);
    download_manager_->Cancel(app);
    app->Cancel();
  }

  return S_OK;
}

HRESULT Worker::Pause(AppBundle* app_bundle) {
  CORE_LOG(L3, (_T("[Worker::Pause][0x%p]"), app_bundle));
  ASSERT1(app_bundle);
  ASSERT1(model_->IsLockedByCaller());
  UNREFERENCED_PARAMETER(app_bundle);

  return E_NOTIMPL;
}

HRESULT Worker::Resume(AppBundle* app_bundle) {
  CORE_LOG(L3, (_T("[Worker::Resume][0x%p]"), app_bundle));
  ASSERT1(app_bundle);
  ASSERT1(model_->IsLockedByCaller());
  UNREFERENCED_PARAMETER(app_bundle);

  return E_NOTIMPL;
}

HRESULT Worker::GetPackage(const Package* package, const CString& dir) {
  CORE_LOG(L3, (_T("[Worker::GetPackage]")));
  ASSERT1(model_->IsLockedByCaller());
  return download_manager_->GetPackage(package, dir);
}

bool Worker::IsPackageAvailable(const Package* package) const {
  CORE_LOG(L3, (_T("[Worker::IsPackageAvailable]")));
  ASSERT1(model_->IsLockedByCaller());
  return download_manager_->IsPackageAvailable(package);
}

HRESULT Worker::CacheOfflinePackages(AppBundle* app_bundle) {
  CORE_LOG(L3, (_T("[Worker::CacheOfflinePackages]")));
  ASSERT1(app_bundle);

  for (size_t i = 0; i != app_bundle->GetNumberOfApps(); ++i) {
    App* app = app_bundle->GetApp(i);
    AppVersion* app_version = app->working_version();
    const size_t num_packages = app_version->GetNumberOfPackages();

    for (size_t j = 0; j < num_packages; ++j) {
      Package* package(app_version->GetPackage(j));
      if (download_manager_->IsPackageAvailable(package)) {
        continue;
      }

      CString offline_app_dir = ConcatenatePath(app_bundle->offline_dir(),
                                                app->app_guid_string());
      CString offline_package_path = ConcatenatePath(offline_app_dir,
                                                     package->filename());
      if (!File::Exists(offline_package_path)) {
        HRESULT hr = offline_utils::FindV2OfflinePackagePath(
            offline_app_dir, &offline_package_path);
        if (FAILED(hr)) {
          CORE_LOG(LE, (_T("[FindOfflinePackagePath failed][0x%x]"), hr));
          return hr;
        }
      }

      File offline_package_file;
      HRESULT hr = offline_package_file.OpenShareMode(offline_package_path,
                                                      false,
                                                      false,
                                                      FILE_SHARE_READ);
      if (FAILED(hr)) {
        return hr;
      }

      hr = download_manager_->CachePackage(package,
                                           &offline_package_file,
                                           &offline_package_path);
      if (FAILED(hr)) {
        CORE_LOG(LE, (_T("[CachePackage failed][%s][%s][0x%x][%Iu]"),
                      app->app_guid_string(), offline_package_path, hr, j));
        return hr;
      }
    }
  }

  return S_OK;
}

HRESULT Worker::PurgeAppLowerVersions(const CString& app_id,
                                      const CString& version) {
  return download_manager_->PurgeAppLowerVersions(app_id, version);
}

// metric_worker_apps_not_*ed_group_policy are integers, not a counter, so they
// should be set to a value, not incremented. Otherwise the same app could be
// counted twice if the same COM server instance was used for multiple bundles
// of the same app(s). For this reason and to avoid overwriting valid counts
// with 0, the number of disabled apps is accumulated then the appropriate
// metric is set only if the count is non-zero.
void Worker::DoPreUpdateCheck(AppBundle* app_bundle,
                              xml::UpdateRequest* update_request) {
  ASSERT1(app_bundle);
  ASSERT1(update_request);

  for (size_t i = 0; i != app_bundle->GetNumberOfApps(); ++i) {
    App* app = app_bundle->GetApp(i);
    app->PreUpdateCheck(update_request);
  }

  size_t num_disabled_apps = 0;
  const std::vector<xml::request::App>& apps = update_request->request().apps;

  // apps.size is 0 if update check was cancelled. Its size can also be less
  // than the number of apps in app_bundle if EULA is not accepted for some
  // apps.
  ASSERT1(apps.size() <= app_bundle->GetNumberOfApps());

  for (size_t i = 0; i != apps.size(); ++i) {
    ASSERT1(apps[i].update_check.is_valid);
    if (apps[i].update_check.is_update_disabled) {
      ++num_disabled_apps;
    }
  }

  if (num_disabled_apps) {
    // Assumes that all apps are either updates or installs.
    if (app_bundle->GetNumberOfApps() && app_bundle->GetApp(0)->is_update()) {
      metric_worker_apps_not_updated_group_policy = num_disabled_apps;
    } else {
      metric_worker_apps_not_installed_group_policy = num_disabled_apps;
    }
  }
}

HRESULT Worker::DoUpdateCheck(AppBundle* app_bundle,
                              const xml::UpdateRequest* update_request,
                              xml::UpdateResponse* update_response) {
  ASSERT1(app_bundle);
  ASSERT1(update_request);
  ASSERT1(update_response);

  ASSERT1(app_bundle->GetNumberOfApps() > 0);

  // The UpdateRequest could be empty if manual updates or installs are disabled
  // by Group Policy.
  if (update_request->IsEmpty()) {
    CORE_LOG(L3, (_T("[DoUpdateCheck][No apps for update check]")));
    return S_OK;
  }

  if (app_bundle->is_offline_install()) {
    return offline_utils::ParseOfflineManifest(
        app_bundle->GetApp(0)->app_guid_string(), app_bundle->offline_dir(),
        update_response);
  }

  if (!ConfigManager::Instance()->CanUseNetwork(is_machine_)) {
    CORE_LOG(L1, (_T("[Update check failed because network use prohibited]")));
    return GOOPDATE_E_CANNOT_USE_NETWORK;
  }

  const bool is_update = app_bundle->is_auto_update();

  if (is_update) {
    ++metric_worker_update_check_total;
  }

  HighresTimer update_check_timer;

  // This is a blocking call on the network.
  const bool is_foreground = app_bundle->priority() == INSTALL_PRIORITY_HIGH;
  HRESULT hr = app_bundle->update_check_client()->Send(is_foreground,
                                                       update_request,
                                                       update_response);

  CORE_LOG(L3, (_T("[Update check HTTP trace][%s]"),
      app_bundle->update_check_client()->http_trace()));

  if (FAILED(hr)) {
    metric_updatecheck_failed_ms.AddSample(update_check_timer.GetElapsedMs());

    CORE_LOG(LE, (_T("[Send failed][0x%08x]"), hr));
    worker_utils::AddHttpRequestDataToEventLog(
        hr,
        app_bundle->update_check_client()->http_ssl_result(),
        app_bundle->update_check_client()->http_status_code(),
        app_bundle->update_check_client()->http_trace(),
        is_machine_);

    // TODO(omaha3): Omaha 2 would launch a web browser here for installs by
    // calling goopdate_utils::LaunchBrowser(). Browser launch needs to be in
    // the client but it currently has no way of knowing that it was a network
    // error. Even here, we don't know that the it wasn't a parsing, etc. error.
    return hr;
  }

  metric_updatecheck_succeeded_ms.AddSample(update_check_timer.GetElapsedMs());

  if (is_update) {
    ++metric_worker_update_check_succeeded;
  }

  return S_OK;
}

void Worker::DoPostUpdateCheck(AppBundle* app_bundle,
                               HRESULT update_check_result,
                               xml::UpdateResponse* update_response) {
  ASSERT1(app_bundle);
  ASSERT1(update_response);

  VERIFY_SUCCEEDED(update_response_utils::ApplyExperimentLabelDeltas(
      is_machine_,
      update_response));

  PersistRetryAfter(app_bundle->update_check_client()->retry_after_sec());

  for (size_t i = 0; i != app_bundle->GetNumberOfApps(); ++i) {
    App* app = app_bundle->GetApp(i);
    app->PostUpdateCheck(update_check_result, update_response);

    ASSERT(app->state() == STATE_UPDATE_AVAILABLE ||
           app->state() == STATE_NO_UPDATE ||
           app->state() == STATE_ERROR,
           (_T("App %Iu state is %u"), i, app->state()));
  }

  if (app_bundle->is_offline_install()) {
    VERIFY_SUCCEEDED(CacheOfflinePackages(app_bundle));
    VERIFY_SUCCEEDED(DeleteDirectory(app_bundle->offline_dir()));
  }
}

void Worker::PersistRetryAfter(int retry_after_sec) const {
  CORE_LOG(L6, (_T("[Worker::PersistRetryAfter][%d]"), retry_after_sec));

  // Registry writes to HKLM need admin.
  ASSERT1(!is_machine_ || vista_util::IsUserAdmin());

  if (retry_after_sec <= 0) {
    return;
  }

  ASSERT1(retry_after_sec <= kSecondsPerDay);
  const uint32 now_sec = Time64ToInt32(GetCurrent100NSTime());
  DWORD retry_after_time_sec = now_sec + retry_after_sec;
  ConfigManager::Instance()->SetRetryAfterTime(is_machine_,
                                               retry_after_time_sec);
}

// Creates a thread pool work item for deferred execution of deferred_function.
// The thread pool owns this callback object.
HRESULT Worker::QueueDeferredFunctionCall0(
    std::shared_ptr<AppBundle> app_bundle,
    void (Worker::*deferred_function)(std::shared_ptr<AppBundle>)) {
  ASSERT1(app_bundle.get());
  ASSERT1(deferred_function);

  using Callback = ThreadPoolCallBack1<Worker, std::shared_ptr<AppBundle>>;
  auto callback = std::make_unique<Callback>(this,
                                             deferred_function,
                                             app_bundle);
  UserWorkItem* user_work_item = callback.get();
  HRESULT hr = Goopdate::Instance().QueueUserWorkItem(std::move(callback),
                                                      COINIT_MULTITHREADED,
                                                      WT_EXECUTELONGFUNCTION);
  if (FAILED(hr)) {
    return hr;
  }

  // This object is owned by the thread pool but the |app_bundle| maintains
  // a dependency on it for debugging purposes.
  app_bundle->set_user_work_item(user_work_item);
  return S_OK;
}

// Creates a thread pool work item for deferred execution of deferred_function.
// The thread pool owns this callback object.
template <typename P1>
HRESULT Worker::QueueDeferredFunctionCall1(
    std::shared_ptr<AppBundle> app_bundle,
    P1 p1,
    void (Worker::*deferred_function)(std::shared_ptr<AppBundle>, P1)) {
  ASSERT1(app_bundle.get());
  ASSERT1(deferred_function);

  using Callback = ThreadPoolCallBack2<Worker, std::shared_ptr<AppBundle>, P1>;
  auto callback = std::make_unique<Callback>(this,
                                             deferred_function,
                                             app_bundle,
                                             p1);
  UserWorkItem* user_work_item = callback.get();
  HRESULT hr = Goopdate::Instance().QueueUserWorkItem(std::move(callback),
                                                      COINIT_MULTITHREADED,
                                                      WT_EXECUTELONGFUNCTION);
  if (FAILED(hr)) {
    return hr;
  }

  // This object is owned by the thread pool but the |app_bundle| maintains
  // a dependency on it for debugging purposes.
  app_bundle->set_user_work_item(user_work_item);
  return S_OK;
}

void Worker::WriteEventLog(int event_type,
                           int event_id,
                           const CString& event_description,
                           const CString& event_text) {
  GoogleUpdateLogEvent log_event(event_type, event_id, is_machine_);
  log_event.set_event_desc(event_description);
  log_event.set_event_text(event_text);
  log_event.WriteEvent();
}

}  // namespace omaha
