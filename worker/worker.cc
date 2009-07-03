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
// TODO(omaha): maybe introduce a new logging facility: WORK_LOG.

// TODO(omaha): Dig out the RefHolder in scope_guard.h so we can use const
// references instead pointers. This TODO was added for some code that no longer
// exists, but it is still a good idea.

#include "omaha/worker/worker.h"

#include <atlbase.h>
#include <atlstr.h>
#include <atlapp.h>
#include <atlsecurity.h>
#include "omaha/common/app_util.h"
#include "omaha/common/const_addresses.h"
#include "omaha/common/const_object_names.h"
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/firewall_product_detection.h"
#include "omaha/common/logging.h"
#include "omaha/common/omaha_version.h"
#include "omaha/common/reactor.h"
#include "omaha/common/scope_guard.h"
#include "omaha/common/scoped_ptr_address.h"
#include "omaha/common/shutdown_handler.h"
#include "omaha/common/sta_call.h"
#include "omaha/common/system.h"
#include "omaha/common/string.h"
#include "omaha/common/thread_pool.h"
#include "omaha/common/time.h"
#include "omaha/common/utils.h"
#include "omaha/common/vistautil.h"
#include "omaha/goopdate/config_manager.h"
#include "omaha/goopdate/const_goopdate.h"
#include "omaha/goopdate/goopdate.h"
#include "omaha/goopdate/goopdate_utils.h"
#include "omaha/goopdate/resource.h"
#include "omaha/goopdate/stats_uploader.h"
#include "omaha/goopdate/ui_displayed_event.h"
#include "omaha/net/network_config.h"
#include "omaha/net/network_request.h"
#include "omaha/setup/setup.h"
#include "omaha/worker/application_manager.h"
#include "omaha/worker/ping.h"
#include "omaha/worker/ping_utils.h"
#include "omaha/worker/worker_event_logger.h"
#include "omaha/worker/worker_job.h"
#include "omaha/worker/worker_metrics.h"
#include "omaha/worker/ui.h"

// Since the net code is linked in as a lib, force the registration code to
// be a dependency, otherwise the linker is optimizing in out.
//
// TODO(omaha): fix this clunkiness and require explicit registration of the
// http creators with the factory. Not ideal but better then linker options.
//
// Design Notes:
// Following are the mutexes that are taken by the worker
// 1. SingleUpdateWorker. Only taken by the update worker.
// 2. SingleInstallWorker. This is application specific. Only taken by the
//    install worker and for the specific application.
// 3. Before install, the install manager takes the global install lock.
// 4. A key thing to add to this code is after taking the install lock,
//    to validate that the version of the applicaion that is present in the
//    registry is the same as that we queried for. The reason to do this
//    is to ensure that there are no races between update and install workers.
// 5. Termination of the worker happens because of four reasons:
//    a. Shutdown event - Only applicable to the update worker. When this event
//       is signalled, the main thread comes out of the wait. It then tries to
//       destroy the contained thread pool, which causes a timed wait for the
//       worker thread. The worker thread is notified by setting a
//       cancelled flag on the worker.
//    b. Install completes, user closes UI - Only applicable for the
//       interactive installs. In this case the main thread comes out of
//       the message loop and deletes the thread pool. The delete happens
//       immediately, since the worker is doing nothing.
//    c. User cancels install - Only applicable in case if interactive installs.
//       The main thread sets the cancelled flag on the workerjob and comes out
//       of the message loop. It then tries to delete the thread pool, causing
//       a timed wait. The worker job queries the cancelled flag periodically
//       and quits as soon as possible.
//    d. The update worker completes - In this case we do not run on a thread
//       pool.

#pragma comment(linker, "/INCLUDE:_kRegisterWinHttp")

namespace omaha {

namespace {

uint64 GetGuidMostSignificantUint64(const GUID& guid) {
  return (static_cast<uint64>(guid.Data1) << 32) +
         (static_cast<uint64>(guid.Data2) << 16) +
         static_cast<uint64>(guid.Data3);
}

class WorkItem : public UserWorkItem {
 public:
  WorkItem(WorkerJob* worker_job, bool delete_after_run)
      : worker_job_(worker_job),
        delete_after_run_(delete_after_run) {
    ASSERT1(worker_job);
  }
 private:
  void DoProcess() {
    worker_job_->DoProcess();
    if (delete_after_run_) {
      delete worker_job_;
      worker_job_ = NULL;
    }
  }

  WorkerJob* worker_job_;
  bool delete_after_run_;
  DISALLOW_EVIL_CONSTRUCTORS(WorkItem);
};

class ErrorWndEvents : public ProgressWndEvents {
 public:
  virtual void DoPause() {}
  virtual void DoResume() {}
  virtual void DoClose() {}
  virtual void DoRestartBrowsers() {}
  virtual void DoReboot() {}
  virtual void DoLaunchBrowser(const CString& url) {
    VERIFY1(SUCCEEDED(goopdate_utils::LaunchBrowser(BROWSER_DEFAULT, url)));
  }
};

}  // namespace

namespace internal {

void RecordUpdateAvailableUsageStats(bool is_machine) {
  AppManager app_manager(is_machine);

  DWORD update_responses(0);
  DWORD64 time_since_first_response_ms(0);
  app_manager.ReadUpdateAvailableStats(GUID_NULL,
                                       kGoopdateGuid,
                                       &update_responses,
                                       &time_since_first_response_ms);
  if (update_responses) {
    metric_worker_self_update_responses = update_responses;
  }
  if (time_since_first_response_ms) {
    metric_worker_self_update_response_time_since_first_ms =
        time_since_first_response_ms;
  }

  ProductDataVector products;
  HRESULT hr = app_manager.GetRegisteredProducts(&products);
  if (FAILED(hr)) {
    ASSERT1(false);
    return;
  }

  // These store information about the app with the most update responses.
  GUID max_responses_product(GUID_NULL);
  DWORD max_responses(0);
  DWORD64 max_responses_time_since_first_response_ms(0);

  for (size_t i = 0; i < products.size(); ++i) {
    const ProductData& product_data = products[i];
    const GUID& product_guid = product_data.app_data().app_guid();

    if (::IsEqualGUID(kGoopdateGuid, product_guid)) {
      continue;
    }

    DWORD update_responses(0);
    DWORD64 time_since_first_response_ms(0);
    app_manager.ReadUpdateAvailableStats(GUID_NULL,
                                         product_guid,
                                         &update_responses,
                                         &time_since_first_response_ms);

    if (max_responses < update_responses) {
      max_responses_product = product_guid;
      max_responses = update_responses;
      max_responses_time_since_first_response_ms = time_since_first_response_ms;
    }
  }

  if (max_responses) {
    metric_worker_app_max_update_responses_app_high =
        GetGuidMostSignificantUint64(max_responses_product);
    metric_worker_app_max_update_responses = max_responses;
    metric_worker_app_max_update_responses_ms_since_first =
        max_responses_time_since_first_response_ms;
  }
}

void SendSelfUpdateFailurePing(bool is_machine) {
  DWORD self_update_error_code(0);
  DWORD self_update_extra_code1(0);
  CString self_update_version;

  if (!Setup::ReadAndClearUpdateErrorInfo(is_machine,
                                          &self_update_error_code,
                                          &self_update_extra_code1,
                                          &self_update_version)) {
    return;
  }

  Ping ping;
  HRESULT hr = ping_utils::SendGoopdatePing(
      is_machine,
      CommandLineExtraArgs(),
      PingEvent::EVENT_SETUP_UPDATE_FAILURE,
      self_update_error_code,
      self_update_extra_code1,
      self_update_version,
      &ping);
  if (FAILED(hr)) {
    CORE_LOG(LW, (_T("[SendGoopdatePing failed][0x%08x]"), hr));
    // TODO(omaha): Consider writing the values back with
    // Setup::PersistUpdateErrorInfo(so we can try pinging later. This would be
    // useful if the user happens to be offline when update worker runs.
  }
}

HRESULT SendFinalUninstallPingForApps(bool is_machine) {
  CORE_LOG(L2, (_T("[SendFinalUninstallPingForApps]")));
  scoped_ptr<Request> uninstall_ping;
  HRESULT hr = BuildUninstallPing(is_machine, address(uninstall_ping));
  if (SUCCEEDED(hr) && uninstall_ping->get_request_count()) {
    Ping ping;
    hr = ping.SendPing(uninstall_ping.get());
  }
  return hr;
}

}  // namespace internal

Worker::Worker(bool is_machine)
    : is_machine_(is_machine),
      is_local_system_(false),
      has_uninstalled_(false) {
  CORE_LOG(L1, (_T("[Worker::Worker]")));
}

Worker::~Worker() {
  CORE_LOG(L1, (_T("[Worker::~Worker]")));

  // Ensure the threads are cleaned up regardless of the entry point or flow.
  StopWorker(NULL);

  CollectAmbientUsageStats();
}

// This method could be called multiple times, and thus needs to be idempotent.
// worker_job_error_code can be NULL.
void Worker::StopWorker(HRESULT* worker_job_error_code) {
  // Stop the concurrent objects to avoid spurious events.
  shutdown_handler_.reset();
  reactor_.reset();

  if (worker_job_.get()) {
    worker_job_->Cancel();
  }

  // The thread pool destructor waits for any remaining jobs to complete.
  thread_pool_.reset();

  if (worker_job_.get() && worker_job_error_code) {
    *worker_job_error_code = worker_job_->error_code();
  }

  // Uninstall should happen as late as possible. Since uninstall may occur
  // after this method, destroy all objects now.
  worker_job_.reset();
}

// This method must not depend on Omaha being registered because that may not
// occur until DoRun.
// Assumes is_machine_ is set correctly.
HRESULT Worker::Main(Goopdate* goopdate) {
  ASSERT1(goopdate);
  args_ = goopdate->args();
  cmd_line_ = goopdate->cmd_line();
  is_local_system_ = goopdate->is_local_system();

  HRESULT hr = DoRun();
  // If this is an interactive instance, UI should have been shown either by
  // WorkerJob or an error.
  ASSERT1(UIDisplayedEventManager::HasUIBeenDisplayed(is_machine_) ||
          (args_.mode != COMMANDLINE_MODE_IG &&
           args_.mode != COMMANDLINE_MODE_HANDOFF_INSTALL) ||
          args_.is_silent_set);

  // Stop WorkerJob and thread pool so we can get the final error code.
  HRESULT worker_job_error_code(S_OK);
  StopWorker(&worker_job_error_code);

  // Get the error code from WorkerJob thread to return to the caller.
  if (SUCCEEDED(hr)) {
    hr = worker_job_error_code;
  }

  if (FAILED(hr) && args_.mode == COMMANDLINE_MODE_IG) {
    MaybeUninstallGoogleUpdate();
  }

  return hr;
}

// This method must not depend on any Omaha "registration" because Google Update
// may not be completely installed yet.
HRESULT Worker::DoRun() {
  OPT_LOG(L1, (_T("[Worker::DoRun]")));

  HRESULT hr = InitializeThreadPool();
  if (FAILED(hr)) {
    return hr;
  }

  // The /install case does not really use the Worker and needs to be moved
  // outside. This will be done as part of unifying Setup.
  if (COMMANDLINE_MODE_INSTALL == args_.mode) {
    return DoInstallGoogleUpdateAndApp();
  }

  if ((COMMANDLINE_MODE_IG == args_.mode ||
       COMMANDLINE_MODE_HANDOFF_INSTALL == args_.mode) &&
      !args_.is_silent_set) {
    HRESULT hr = InitializeUI();
    if (FAILED(hr)) {
      CString error_text;
      error_text.FormatMessage(IDS_INSTALL_FAILED, hr);
      DisplayErrorInMessageBox(error_text);
      return hr;
    }

    if (!args_.is_offline_set) {
      CString caption;
      caption.FormatMessage(IDS_WINDOW_TITLE, GetPrimaryJobInfo().app_name);
      CString message;
      message.LoadString(IDS_PROXY_PROMPT_MESSAGE);
      const uint32 kProxyMaxPrompts = 1;
      NetworkConfig::Instance().ConfigureProxyAuth(caption,
                                                   message,
                                                   *progress_wnd_,
                                                   kProxyMaxPrompts);
    }
  }

  worker_job_.reset(WorkerJobFactory::CreateWorkerJob(is_machine_,
                                                      args_,
                                                      job_observer_.get()));

  // TODO(omaha): It would be nice if we could move all the differences
  // between installs and updates into the WorkerJobStrategy and just call
  // StartWorkerJob at this point.
#pragma warning(push)
// C4061: enumerator 'xxx' in switch of enum 'yyy' is not explicitly handled by
// a case label.
#pragma warning(disable : 4061)
  switch (args_.mode) {
    case COMMANDLINE_MODE_IG:
    case COMMANDLINE_MODE_HANDOFF_INSTALL:
      hr = DoInstall();
      break;
    case COMMANDLINE_MODE_UA:
      hr = DoUpdateApps();
      OPT_LOG(L2, (_T("[Update worker finished]")));
      break;
    default:
      ASSERT(false, (_T("Nothing to do in the worker.")));
      hr = E_UNEXPECTED;
  }
#pragma warning(pop)

  return hr;
}

bool Worker::EnsureSingleAppInstaller(const CString& guid) {
  // We allow only one instance of interactive install per application.
  CString mutex_name;
  mutex_name.Format(kSingleInstallWorker, guid);
  single_install_worker_.reset(new ProgramInstance(mutex_name));
  return !single_install_worker_->EnsureSingleInstance();
}

bool Worker::EnsureSingleUpdateWorker() {
  // We allow only one instance of the update worker per user.
  NamedObjectAttributes single_update_worker_attr;
  GetNamedObjectAttributes(kSingleupdateWorker,
                           is_machine_,
                           &single_update_worker_attr);

  single_update_worker_.reset(new ProgramInstance(
      single_update_worker_attr.name));
  return !single_update_worker_->EnsureSingleInstance();
}

HRESULT Worker::InitializeUI() {
  OPT_LOG(L1, (_T("[InitializeUI]")));
  ASSERT1((COMMANDLINE_MODE_IG == args_.mode ||
           COMMANDLINE_MODE_HANDOFF_INSTALL == args_.mode ||
           COMMANDLINE_MODE_INSTALL == args_.mode) &&
          !args_.is_silent_set);

  progress_wnd_.reset(new ProgressWnd(&message_loop_, NULL));

  progress_wnd_->set_is_machine(is_machine_);
  progress_wnd_->set_language(args_.extra.language);
  ASSERT1(!args_.extra.apps.empty());
  progress_wnd_->set_product_name(GetPrimaryJobInfo().app_name);
  progress_wnd_->set_product_guid(GetPrimaryJobInfo().app_guid);

  JobObserverCallMethodDecorator* decorator =
      new JobObserverCallMethodDecorator(progress_wnd_.get());
  job_observer_.reset(decorator);
  HRESULT hr = decorator->Initialize();
  if (FAILED(hr)) {
    OPT_LOG(L1, (_T("JobObserver initialize failed][0x%08x]"), hr));
    return hr;
  }

  return S_OK;
}

HRESULT Worker::DoInstall() {
  OPT_LOG(L1, (_T("[DoInstall]")));

  const bool is_already_installing =
      EnsureSingleAppInstaller(GuidToString(GetPrimaryJobInfo().app_guid));

  if (is_already_installing) {
    OPT_LOG(L1, (_T("[Another Install of this application is running in the ")
                 _T("same session. Exiting.]")));
    ++metric_worker_another_install_in_progress;
    HRESULT hr = GOOPDATE_E_APP_BEING_INSTALLED;
    CString error_text;
    error_text.FormatMessage(IDS_APPLICATION_ALREADY_INSTALLING,
                             GetPrimaryJobInfo().app_name);
    DisplayError(error_text, hr);
    return hr;
  }

  HRESULT hr = StartWorkerJob();
  if (FAILED(hr)) {
    CString error_text;
    error_text.FormatMessage(IDS_INSTALL_FAILED, hr);

    DisplayError(error_text, hr);
    return hr;
  }

  return S_OK;
}

HRESULT Worker::InitializeShutDownHandler(ShutdownCallback* callback) {
  CORE_LOG(L3, (_T("[InitializeShutDownHandler]")));
  ASSERT1(callback);

  reactor_.reset(new Reactor);
  shutdown_handler_.reset(new ShutdownHandler);
  return shutdown_handler_->Initialize(reactor_.get(),
                                       callback,
                                       is_machine_);
}

HRESULT Worker::DoUpdateApps() {
  OPT_LOG(L1, (_T("[DoUpdateApps]")));

  WriteUpdateAppsWorkerStartEvent(is_machine_);

  if (EnsureSingleUpdateWorker()) {
    OPT_LOG(L1, (_T("[Another worker is already running. Exiting.]")));
    ++metric_worker_another_update_in_progress;
    return GOOPDATE_E_WORKER_ALREADY_RUNNING;
  }

  HRESULT hr = InitializeShutDownHandler(this);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[InitializeShutDownHandler failed][0x%08x]"), hr));
    return hr;
  }

  internal::RecordUpdateAvailableUsageStats(is_machine_);

  // A tentative uninstall check is done here. There are stronger checks,
  // protected by locks, which are done by Setup.
  size_t num_clients(0);
  const bool is_uninstall =
      FAILED(goopdate_utils::GetNumClients(is_machine_, &num_clients)) ||
      num_clients <= 1;
  CORE_LOG(L4, (_T("[Worker::DoUpdateApps][%u]"), num_clients));

  if (is_uninstall) {
    // Attempt a conditional uninstall and always return S_OK to avoid
    // executing error handling code in the case of an actual uninstall.
    // Do not attempt to uninstall if MSI is busy to avoid spurious uninstalls.
    // See http://b/1436223. The call to WaitForMSIExecute blocks with a
    // timeout. It is better to block here than block while holding the setup
    // lock.
    hr = WaitForMSIExecute(kWaitForMSIExecuteMs);
    CORE_LOG(L2, (_T("[WaitForMSIExecute returned 0x%08x]"), hr));
    if (SUCCEEDED(hr)) {
      // Destroy all the objects before uninstalling.
      StopWorker(NULL);
      MaybeUninstallGoogleUpdate();
    }
    return S_OK;
  }

  hr = StartWorkerJob();

  internal::SendSelfUpdateFailurePing(is_machine_);

  return hr;
}

HRESULT Worker::DoInstallGoogleUpdateAndApp() {
  OPT_LOG(L1, (_T("[DoInstallGoogleUpdateAndApp]")));

  // For machine installs, do not use the UI displayed event in the non-elevated
  // instance.
  if (!is_machine_ || vista_util::IsUserAdmin()) {
    VERIFY1(SUCCEEDED(UIDisplayedEventManager::CreateEvent(is_machine_)));
  }

  Setup setup(is_machine_, &args_);
  HRESULT hr = setup.Install(cmd_line_);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[Setup::Install failed][0x%08x]"), hr));
    VERIFY1(SUCCEEDED(HandleSetupError(hr, setup.extra_code1())));
    return hr;
  }

  return S_OK;
}

HRESULT Worker::InitializeThreadPool() {
  CORE_LOG(L3, (_T("[Worker::InitializeThreadPool]")));
  thread_pool_.reset(new ThreadPool);
  return thread_pool_->Initialize(kThreadPoolShutdownDelayMs);
}

HRESULT Worker::StartWorkerJob() {
  CORE_LOG(L2, (_T("[Worker::StartWorker]")));
  if (args_.is_silent_set || COMMANDLINE_MODE_UA == args_.mode) {
    return worker_job_->DoProcess();
  }

  HRESULT hr = QueueWorkerJob(worker_job_.get(), false);
  if (FAILED(hr)) {
    return hr;
  }

  message_loop_.Run();
  return S_OK;
}

HRESULT Worker::QueueWorkerJob(WorkerJob* worker_job, bool delete_after_run) {
  CORE_LOG(L2, (_T("[Worker::QueueWorkerJob]")));
  ASSERT1(thread_pool_.get());

  scoped_ptr<WorkItem> work_item;
  work_item.reset(new WorkItem(worker_job, delete_after_run));
  HRESULT hr = thread_pool_->QueueUserWorkItem(work_item.get(),
                                               WT_EXECUTELONGFUNCTION);
  if (FAILED(hr)) {
    return hr;
  }

  work_item.release();
  return S_OK;
}

// Displays an error in the Google Update UI if not silent then sends a ping if
// allowed If the UI fails, uses a system message box as a fallback.
HRESULT Worker::HandleSetupError(HRESULT error, int extra_code1) {
  ASSERT1(FAILED(error));

  CString error_text;
  ASSERT1(!args_.extra.apps.empty());
  switch (error) {
    case GOOPDATE_E_INSTALL_ELEVATED_PROCESS_NEEDS_ELEVATION:
    case GOOPDATE_E_NONADMIN_INSTALL_ADMIN_APP:
      error_text.FormatMessage(IDS_NEED_ADMIN_TO_INSTALL,
                               GetPrimaryJobInfo().app_name);
      break;
    case GOOPDATE_E_ELEVATION_FAILED_ADMIN:
    case GOOPDATE_E_ELEVATION_FAILED_NON_ADMIN:
      error_text.FormatMessage(IDS_ELEVATION_FAILED,
                               GetPrimaryJobInfo().app_name);
      break;
    case GOOPDATE_E_FAILED_TO_GET_LOCK:
    case GOOPDATE_E_FAILED_TO_GET_LOCK_MATCHING_INSTALL_PROCESS_RUNNING:
    case GOOPDATE_E_FAILED_TO_GET_LOCK_NONMATCHING_INSTALL_PROCESS_RUNNING:
    case GOOPDATE_E_FAILED_TO_GET_LOCK_UPDATE_PROCESS_RUNNING:
      error_text.FormatMessage(IDS_APPLICATION_INSTALLING_GOOGLE_UPDATE,
                               GetPrimaryJobInfo().app_name);
      break;
    case GOOPDATE_E_INSTANCES_RUNNING:
      error_text.FormatMessage(IDS_INSTANCES_RUNNING_AFTER_SHUTDOWN,
                               GetPrimaryJobInfo().app_name);
      break;
    case GOOPDATE_E_RUNNING_INFERIOR_MSXML:
      error_text.FormatMessage(IDS_WINDOWS_IS_NOT_UP_TO_DATE,
                               GetPrimaryJobInfo().app_name);
      break;
    case GOOPDATE_E_HANDOFF_FAILED:
      error_text.FormatMessage(IDS_HANDOFF_FAILED,
                               GetPrimaryJobInfo().app_name);
      break;
    default:
      error_text.FormatMessage(IDS_SETUP_FAILED, error);
      break;
  }

  OPT_LOG(LE, (_T("[Failed to install Google Update][0x%08x][%s]"),
               error, error_text));

  if (UIDisplayedEventManager::HasUIBeenDisplayed(is_machine_)) {
    // Do not display another UI, launch the web page, or ping.
    CORE_LOG(L2, (_T("[second instance has UI; not displaying error UI]")));
    return S_OK;
  }

  HRESULT hr = S_OK;
  if (!args_.is_silent_set) {
    hr = InitializeUI();
    if (SUCCEEDED(hr)) {
      DisplayError(error_text, error);
    } else {
      DisplayErrorInMessageBox(error_text);
    }
  }

  // Do not ping. Cannot rely on the ping code to not send the ping because
  // Setup may have failed before it could write eulaccepted=0 to the registry.
  if (args().is_eula_required_set) {
    return hr;
  }

  if (args().is_oem_set) {
    // Do not ping.
    // Cannot rely on the ping code to not send the ping because the OEM Mode
    // value is not set after Setup returns.
    return hr;
  }

  // This ping may cause a firewall prompt, but we are willing to cause a prompt
  // in failure cases.
  Request request(is_machine_);
  Ping ping;
  HRESULT hr_ping = ping_utils::SendGoopdatePing(
      is_machine_,
      args().extra,
      PingEvent::EVENT_SETUP_INSTALL_FAILURE,
      error,
      extra_code1,
      NULL,
      &ping);
  if (FAILED(hr_ping)) {
    CORE_LOG(LW, (_T("[SendGoopdatePing failed][0x%08x]"), hr_ping));
  }

  return hr;
}

HRESULT Worker::Shutdown() {
  CORE_LOG(L2, (_T("[Worker::Shutdown]")));
  ASSERT1(args_.mode == COMMANDLINE_MODE_UA);
  if (worker_job_.get()) {
    worker_job_->Cancel();
  }
  return S_OK;
}

HRESULT Worker::DoOnDemand(const WCHAR* guid,
                           const CString& lang,
                           IJobObserver* observer,
                           bool is_update_check_only) {
  CORE_LOG(L3, (_T("[Worker::DoOnDemand][%d][%s][%d][%d]"),
                is_machine_, guid, observer, is_update_check_only));
  ASSERT1(guid);
  ASSERT1(observer);

  if (!is_update_check_only && is_machine_ && !vista_util::IsUserAdmin()) {
    ASSERT(false, (_T("Need to be elevated for machine application.")));
    return HRESULT_FROM_WIN32(ERROR_ELEVATION_REQUIRED);
  }

  // Create a fresh WorkerJob for each OnDemand request that comes in.
  scoped_ptr<WorkerJob> worker_job;
  HRESULT hr = WorkerJobFactory::CreateOnDemandWorkerJob(
      is_machine_,
      is_update_check_only,
      lang,
      StringToGuid(guid),
      observer,
      shutdown_callback_.get(),
      address(worker_job));
  if (FAILED(hr)) {
    return hr;
  }

  hr = QueueWorkerJob(worker_job.get(), true);
  if (FAILED(hr)) {
    CORE_LOG(LW, (_T("[QueueWorkerJob failed][0x%08x]"), hr));
    return hr;
  }
  worker_job.release();

  return S_OK;
}

void Worker::CollectAmbientUsageStats() {
  if (args_.mode != COMMANDLINE_MODE_UA) {
    return;
  }

  CString name, version;
  HRESULT hr = firewall_detection::Detect(&name, &version);
  bool has_software_firewall = SUCCEEDED(hr) && !name.IsEmpty();
  metric_worker_has_software_firewall.Set(has_software_firewall);

  if (System::IsRunningOnBatteries()) {
    ++metric_worker_silent_update_running_on_batteries;
  }

  metric_worker_shell_version = app_util::GetVersionFromModule(NULL);

  metric_worker_is_windows_installing.Set(IsWindowsInstalling());
  metric_worker_is_uac_disabled.Set(vista_util::IsUACDisabled());
  metric_worker_is_clickonce_disabled.Set(IsClickOnceDisabled());
}

// Uninstall is a tricky use case. Uninstall can primarily happen in three cases
// and there are two mechanisms to uninstall. The cases in which Omaha
// uninstalls are:
// 1. The last registered application uninstalls. Omaha monitors the
// client keys and it will trigger an immediate uninstall in this case.
// 2. The core starts an update worker, if there are no registered
// applications, the update worker will do the uninstall.
// 3. An error, including user cancel, happens during Omaha or app installation
// and there are no registered applications.
// The uninstall is implemented in terms of the following mechanisms:
// * An update worker launched with "/ua /uninstalled" by the core, in the
// first two cases above.
// * A direct uninstall, in the case of errors or user cancellations, in the
// last case above.
//
// Omaha can uninstall only if there are no install workers running and no
// registered applications. This check is done under the setup lock protection.
// In addition, the uninstall worker takes the update worker lock. Acquiring
// this lock is important since the silent installers can modify the
// registration of apps and trigger uninstalls workers. Therefore, both
// setup lock and the update worker locks are needed.
//
// In the direct uninstall case there is a small race condition, since there is
// no other single lock that can be acquired to prevent changes to the
// application registration. The code looks for install workers but the test is
// racy if not protected by locks.
void Worker::MaybeUninstallGoogleUpdate() {
  CORE_LOG(L1, (_T("[Worker::MaybeUninstallGoogleUpdate]")));
  ASSERT1(args_.mode == COMMANDLINE_MODE_UA ||
          args_.mode == COMMANDLINE_MODE_IG);
  internal::SendFinalUninstallPingForApps(is_machine_);

  Setup setup(is_machine_, &args_);
  has_uninstalled_ = !!SUCCEEDED(setup.Uninstall());
}

const CommandLineAppArgs& Worker::GetPrimaryJobInfo() const {
  ASSERT1(!args_.extra.apps.empty());
  return args_.extra.apps[0];
}

void Worker::DisplayError(const CString& error_text, HRESULT error) {
  ASSERT1(!UIDisplayedEventManager::HasUIBeenDisplayed(is_machine_));

#pragma warning(push)
// C4061: enumerator 'xxx' in switch of enum 'yyy' is not explicitly handled by
// a case label.
#pragma warning(disable : 4061)
  switch (args_.mode) {
    case COMMANDLINE_MODE_INSTALL:
    case COMMANDLINE_MODE_HANDOFF_INSTALL:
    case COMMANDLINE_MODE_IG:
      if (args_.is_silent_set) {
        return;
      }
      break;
    case COMMANDLINE_MODE_UA:
    case COMMANDLINE_MODE_UNKNOWN:  // OnDemand
      return;
      break;
    default:
      ASSERT1(false);
      return;
  }
#pragma warning(pop)

  ASSERT1(job_observer_.get());
  ErrorWndEvents error_wnd_events;

  // error_wnd_events must not be destroyed until CMessageLoop::Run() returns.
  job_observer_->SetEventSink(&error_wnd_events);
  job_observer_->OnShow();

  job_observer_->OnComplete(COMPLETION_CODE_ERROR, error_text, error);

  message_loop_.Run();
}

void Worker::DisplayErrorInMessageBox(const CString& error_text) {
  ASSERT1(args_.mode == COMMANDLINE_MODE_INSTALL ||
          args_.mode == COMMANDLINE_MODE_IG ||
          args_.mode == COMMANDLINE_MODE_HANDOFF_INSTALL);

  if (args_.is_silent_set) {
    return;
  }

  CString primary_app_name;
  ASSERT1(!args_.extra.apps.empty());
  if (!args_.extra.apps.empty()) {
    primary_app_name = GetPrimaryJobInfo().app_name;
  }

  goopdate_utils::DisplayErrorInMessageBox(error_text, primary_app_name);
}

}  // namespace omaha

