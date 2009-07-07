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

#include "omaha/worker/job.h"

#include <windows.h>
#include <winhttp.h>
#include <vector>
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/file.h"
#include "omaha/common/logging.h"
#include "omaha/common/path.h"
#include "omaha/common/sta_call.h"
#include "omaha/common/time.h"
#include "omaha/common/utils.h"
#include "omaha/common/vistautil.h"
#include "omaha/goopdate/command_line.h"
#include "omaha/goopdate/goopdate_utils.h"
#include "omaha/goopdate/resource.h"
#include "omaha/goopdate/update_response_data.h"
#include "omaha/worker/application_data.h"
#include "omaha/worker/application_manager.h"
#include "omaha/worker/download_manager.h"
#include "omaha/worker/install_manager.h"
#include "omaha/worker/job_observer.h"
#include "omaha/worker/ping.h"
#include "omaha/worker/ping_utils.h"
#include "omaha/worker/worker_event_logger.h"
#include "omaha/worker/worker_metrics.h"

namespace omaha {

// The caller retains ownership of ping. Delete this object before deleting
// the Ping.
Job::Job(bool is_update, Ping* ping)
    : extra_code1_(0),
      job_state_(JOBSTATE_START),
      job_observer_(NULL),
      user_explorer_pid_(0),
      bytes_downloaded_(0),
      bytes_total_(0),
      ping_(ping),
      is_update_(is_update),
      is_offline_(false),
      is_background_(false),
      is_update_check_only_(false),
      did_launch_cmd_fail_(false) {
  CORE_LOG(L1, (_T("[Job::Job]")));
}

Job::~Job() {
  CORE_LOG(L1, (_T("[Job::~Job][%s]"), GuidToString(app_data_.app_guid())));
}

void Job::ChangeState(JobState state, bool send_ping) {
  CORE_LOG(L1, (_T("[Job::ChangeState moving job from %d to %d.]"),
                job_state_, state));
  const JobState previous_state = job_state_;
  job_state_ = state;

  // Ignore failures.
  if (send_ping && job_state_ != JOBSTATE_START) {
    SendStateChangePing(previous_state);
  }
  VERIFY1(SUCCEEDED(NotifyUI()));
}

void Job::NotifyDownloadStarted() {
  ASSERT1(job_state_ == JOBSTATE_START);
  ChangeState(JOBSTATE_DOWNLOADSTARTED, true);
}

void Job::NotifyDownloadComplete() {
  ASSERT1(job_state_ == JOBSTATE_DOWNLOADSTARTED);
  ChangeState(JOBSTATE_DOWNLOADCOMPLETED, true);
}

void Job::NotifyInstallStarted() {
  ChangeState(JOBSTATE_INSTALLERSTARTED, true);
}

void Job::NotifyCompleted(const CompletionInfo& info) {
  info_ = info;

  if (!is_update_check_only_) {
    WriteJobCompletedEvent(app_data().is_machine_app(), *this);
  }

  bool is_successful_self_update =
      is_update_ &&
      IsCompletionSuccess(info_) &&
      ::IsEqualGUID(kGoopdateGuid, app_data().app_guid());
  if (is_successful_self_update) {
    CORE_LOG(L2, (_T("[self-update successfully invoked; not sending ping]")));
  }

  // Always send a ping unless:
  // - the state is successful self-update completed, or
  // - this is an update check only job.
  // The exception exists because in the first case we do not know the actual
  // outcome of the job. The ping will be sent by the installer when setup
  // completes the self-update.
  // In the second case, update check only jobs do not actually complete.
  // They are used as containers for the update check response and they
  // are not downloaded nor installed.
  bool send_ping = !(is_successful_self_update || is_update_check_only_);
  ChangeState(JOBSTATE_COMPLETED, send_ping);
}

void Job::OnProgress(int bytes, int bytes_total,
                     int status, const TCHAR* status_text) {
  UNREFERENCED_PARAMETER(status);
  UNREFERENCED_PARAMETER(status_text);
  ASSERT1(status == WINHTTP_CALLBACK_STATUS_READ_COMPLETE ||
          status == WINHTTP_CALLBACK_STATUS_CONNECTING_TO_SERVER);
  ASSERT1(job_state_ == JOBSTATE_DOWNLOADSTARTED);
  bytes_downloaded_ = bytes;
  bytes_total_ = bytes_total;
  VERIFY1(SUCCEEDED(NotifyUI()));
}

HRESULT Job::NotifyUI() {
  if (!job_observer_) {
    return S_OK;
  }

  switch (job_state_) {
    case JOBSTATE_DOWNLOADSTARTED: {
      int pos = 0;
      if (bytes_total_) {
        pos = static_cast<int>(
            (static_cast<double>(bytes_downloaded_) / bytes_total_) * 100);
      }
      job_observer_->OnDownloading(0, pos);
      break;
    }
    case JOBSTATE_DOWNLOADCOMPLETED:
      job_observer_->OnWaitingToInstall();
      break;
    case JOBSTATE_INSTALLERSTARTED:
      job_observer_->OnInstalling();
      break;
    case JOBSTATE_COMPLETED:
      return DoCompleteJob();
    case JOBSTATE_START:
    default:
      ASSERT1(false);
      return E_UNEXPECTED;
  }

  return S_OK;
}

HRESULT Job::ShouldRestartBrowser() const {
  return !update_response_data_.success_url().IsEmpty();
}

HRESULT Job::DeleteJobDownloadDirectory() const {
  CORE_LOG(L3, (_T("[DeleteJobDownloadDirectory]")));
  ASSERT1(IsCompletionSuccess(info_));

  // Do no delete the downloaded file if the installer is omaha.
  if (::IsEqualGUID(app_data_.app_guid(), kGoopdateGuid)) {
    CORE_LOG(L3, (_T("[Not deleting goopdate self update.]")));
    return S_OK;
  }

  // In case of ondemand updates checks, it is possible that we do not have
  // a downloaded file.
  if (download_file_name_.IsEmpty() &&
      is_update_check_only_) {
    return S_OK;
  }

  ASSERT1(!download_file_name_.IsEmpty());
  ASSERT1(!File::IsDirectory(download_file_name_));
  CString path = GetDirectoryFromPath(download_file_name_);
  HRESULT hr = DeleteDirectory(path);
  if (FAILED(hr)) {
    CORE_LOG(L3, (_T("[Download file directory delete failed.][%s][0x%08x]"),
                  path, hr));
  }
  return hr;
}

// The SUCCESS_ACTION_EXIT_SILENTLY_ON_LAUNCH_CMD case assumes that
// the launch command was run and did_launch_cmd_fail_ set correctly if
// launch_cmd_line_ is not empty.
CompletionCodes Job::CompletionStatusToCompletionCode(
    JobCompletionStatus status) const {
  CompletionCodes code(COMPLETION_CODE_ERROR);
  switch (status) {
    case COMPLETION_SUCCESS: {
      if ((update_response_data_.success_action() ==
           SUCCESS_ACTION_EXIT_SILENTLY) ||
          (update_response_data_.success_action() ==
           SUCCESS_ACTION_EXIT_SILENTLY_ON_LAUNCH_CMD &&
           !launch_cmd_line_.IsEmpty())) {
        // If launch failed, display the success UI anyway.
        code = did_launch_cmd_fail_ ? COMPLETION_CODE_SUCCESS :
                                      COMPLETION_CODE_SUCCESS_CLOSE_UI;
      } else if (ShouldRestartBrowser()) {
        if (app_data_.browser_type() == BROWSER_UNKNOWN ||
            app_data_.browser_type() == BROWSER_DEFAULT ||
            app_data_.browser_type() > BROWSER_MAX) {
          code = update_response_data_.terminate_all_browsers() ?
                     COMPLETION_CODE_RESTART_ALL_BROWSERS_NOTICE_ONLY :
                     COMPLETION_CODE_RESTART_BROWSER_NOTICE_ONLY;
        } else {
          code = update_response_data_.terminate_all_browsers() ?
                     COMPLETION_CODE_RESTART_ALL_BROWSERS :
                     COMPLETION_CODE_RESTART_BROWSER;
        }
      } else {
        code = COMPLETION_CODE_SUCCESS;
      }
      break;
    }
    case COMPLETION_SUCCESS_REBOOT_REQUIRED:
      code = COMPLETION_CODE_REBOOT_NOTICE_ONLY;
      break;
    case COMPLETION_ERROR:
    case COMPLETION_INSTALLER_ERROR_MSI:
    case COMPLETION_INSTALLER_ERROR_SYSTEM:
    case COMPLETION_INSTALLER_ERROR_OTHER:
      code = COMPLETION_CODE_ERROR;
      break;
    case COMPLETION_CANCELLED:
      code = COMPLETION_CODE_ERROR;
      break;
    default:
      ASSERT1(false);
  }

  return code;
}

HRESULT Job::DoCompleteJob() {
  CompletionCodes completion_code =
      CompletionStatusToCompletionCode(info_.status);

  if (IsCompletionSuccess(info_)) {
    VERIFY1(SUCCEEDED(DeleteJobDownloadDirectory()));
  }

  OPT_LOG(L1, (_T("[job completed]")
               _T("[status %d][completion code %d][error 0x%08x][text \"%s\"]"),
               info_.status, completion_code, info_.error_code, info_.text));

  if (job_observer_) {
    job_observer_->OnComplete(completion_code,
                              info_.text,
                              info_.error_code);
  }

  return S_OK;
}

HRESULT Job::SendStateChangePing(JobState previous_state) {
  Request request(app_data().is_machine_app());

  // TODO(omaha): Will need to update this to determine if this is a product or
  // component level job and create the ProductAppData appropriately.

  AppRequestData app_request_data(app_data());

  PingEvent::Types event_type = JobStateToEventType();
  PingEvent::Results event_result =
      ping_utils::CompletionStatusToPingEventResult(info().status);
  // TODO(omaha): Remove this value when circular log buffer is implemented.
  // If extra_code1 is not already used and there is an error, specify the state
  // in which the error occurred in extra_code1.
  const int extra_code1 = (!extra_code1_ && info().error_code) ?
                              kJobStateExtraCodeMask | previous_state :
                              extra_code1_;
  PingEvent ping_event(event_type,
                       event_result,
                       info().error_code,
                       extra_code1,
                       app_data().previous_version());
  app_request_data.AddPingEvent(ping_event);

  AppRequest app_request(app_request_data);
  request.AddAppRequest(app_request);

  // Clear the extra code for the next state/ping.
  extra_code1_ = 0;

  if (event_result == COMPLETION_CANCELLED) {
    // This ping is sending the last ping after the cancel it cannot
    // be canceled.
    Ping cancel_ping;
    return cancel_ping.SendPing(&request);
  } else {
    ASSERT1(ping_);
    return ping_->SendPing(&request);
  }
}

PingEvent::Types Job::JobStateToEventType() const {
  PingEvent::Types type = PingEvent::EVENT_UNKNOWN;
  switch (job_state_) {
    case JOBSTATE_START:
      break;
    case JOBSTATE_DOWNLOADSTARTED:
      type = is_update_ ? PingEvent::EVENT_UPDATE_DOWNLOAD_START :
                          PingEvent::EVENT_INSTALL_DOWNLOAD_START;
      break;
    case JOBSTATE_DOWNLOADCOMPLETED:
      type = is_update_ ? PingEvent::EVENT_UPDATE_DOWNLOAD_FINISH :
                          PingEvent::EVENT_INSTALL_DOWNLOAD_FINISH;
      break;
    case JOBSTATE_INSTALLERSTARTED:
      type = is_update_ ? PingEvent::EVENT_UPDATE_INSTALLER_START :
                          PingEvent::EVENT_INSTALL_INSTALLER_START;
      break;
    case JOBSTATE_COMPLETED:
      type = is_update_ ? PingEvent::EVENT_UPDATE_COMPLETE :
                          PingEvent::EVENT_INSTALL_COMPLETE;
      break;
    default:
      break;
  }
  ASSERT1(PingEvent::EVENT_UNKNOWN != type);
  return type;
}

void Job::RestartBrowsers() {
  // CompletionStatusToCompletionCode does not set a restart completion code
  // if the browser type is not a specific browser, so this method should never
  // be called in those cases.
  ASSERT1(app_data_.browser_type() != BROWSER_UNKNOWN &&
          app_data_.browser_type() != BROWSER_DEFAULT &&
          app_data_.browser_type() < BROWSER_MAX);

  CORE_LOG(L3, (_T("[Job::RestartBrowsers]")));
  TerminateBrowserResult browser_res;
  TerminateBrowserResult default_res;
  if (update_response_data_.terminate_all_browsers()) {
    goopdate_utils::TerminateAllBrowsers(app_data_.browser_type(),
                                         &browser_res,
                                         &default_res);
  } else {
    goopdate_utils::TerminateBrowserProcesses(app_data_.browser_type(),
                                              &browser_res,
                                              &default_res);
  }

  BrowserType default_type = BROWSER_UNKNOWN;
  HRESULT hr = GetDefaultBrowserType(&default_type);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[GetDefaultBrowserType failed][0x%08x]"), hr));
    return;
  }

  BrowserType browser_type = BROWSER_UNKNOWN;
  if (!goopdate_utils::GetBrowserToRestart(app_data_.browser_type(),
                                           default_type,
                                           browser_res,
                                           default_res,
                                           &browser_type)) {
    CORE_LOG(LE, (_T("[GetBrowserToRestart returned false. Not launching.]")));
    return;
  }

  ASSERT1(BROWSER_UNKNOWN != browser_type);
  VERIFY1(SUCCEEDED(goopdate_utils::LaunchBrowser(
      browser_type,
      update_response_data_.success_url())));
}

void Job::LaunchBrowser(const CString& url) {
  CORE_LOG(L3, (_T("[Job::LaunchBrowser]")));
  BrowserType browser_type =
      app_data_.browser_type() == BROWSER_UNKNOWN ?
                                  BROWSER_DEFAULT :
                                  app_data_.browser_type();
  VERIFY1(SUCCEEDED(goopdate_utils::LaunchBrowser(browser_type, url)));
}

void Job::set_app_data(const AppData& app_data) {
  app_data_ = app_data;
}

// On Vista with UAC on for an interactive install, LaunchCmdLine will run the
// command line at medium integrity even if the installer was running at high
// integrity.
HRESULT Job::LaunchCmdLine() {
  CORE_LOG(L3, (_T("[Job::LaunchCmdLine][%s]"), launch_cmd_line_));
  ASSERT1(!is_update_);

  if (launch_cmd_line_.IsEmpty()) {
    return S_OK;
  }

  // InstallerSuccessLaunchCmdLine should not be set if the install failed.
  ASSERT1(IsCompletionSuccess(info_));

  HRESULT hr = goopdate_utils::LaunchCmdLine(launch_cmd_line_);
  if (FAILED(hr)) {
    OPT_LOG(LE, (_T("[goopdate_utils::LaunchCmdLine failed][0x%x]"), hr));
    did_launch_cmd_fail_ = true;
    return hr;
  }

  return S_OK;
}

HRESULT Job::Download(DownloadManager* dl_manager) {
  CORE_LOG(L2, (_T("[Job::DownloadJob][%s]"),
                GuidToString(app_data_.app_guid())));
  ASSERT1(dl_manager);

  NotifyDownloadStarted();
  HRESULT hr = dl_manager->DownloadFile(this);
  if (FAILED(hr)) {
    CORE_LOG(LW, (_T("[Job::DownloadJob DownloadFile failed][0x%08x][%s]"),
                  hr, GuidToString(app_data_.app_guid())));
    CompletionInfo info = dl_manager->error_info();
    if (!IsCompletionSuccess(info)) {
      NotifyCompleted(info);
    }
    return hr;
  }

  NotifyDownloadComplete();
  return S_OK;
}

HRESULT Job::Install() {
  CORE_LOG(L2, (_T("[Job::InstallJob]")));
  NotifyInstallStarted();

  CompletionInfo completion_info;
  AppData new_app_data;
  HRESULT hr = DoInstall(&completion_info, &new_app_data);
  // Do not return until after NotifyCompleted() has been called.

  NotifyCompleted(completion_info);
  app_data_ = new_app_data;

  return hr;
}

HRESULT Job::DoInstall(CompletionInfo* completion_info,
                       AppData* new_app_data) {
  CORE_LOG(L3, (_T("[Job::DoInstall]")));
  ASSERT1(completion_info);
  ASSERT1(new_app_data);

  if (!is_update_) {
    AppManager app_manager(app_data_.is_machine_app());
    HRESULT hr = app_manager.WritePreInstallData(app_data_);
    if (FAILED(hr)) {
      CORE_LOG(LE, (_T("[AppManager::WritePreInstallData failed][0x%08x][%s]"),
                    hr, GuidToString(app_data_.app_guid())));
      completion_info->status = COMPLETION_ERROR;
      completion_info->error_code = hr;
      completion_info->text.FormatMessage(IDS_INSTALL_FAILED, hr);
      return hr;
    }
  }

  InstallManager install_manager(app_data_.is_machine_app());
  AppManager app_manager(app_data_.is_machine_app());

  HRESULT hr = install_manager.InstallJob(this);
  *completion_info = install_manager.error_info();
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[InstallManager::InstallJobs failed][0x%08x][%s]"),
                  hr, GuidToString(app_data_.app_guid())));

    // If we failed the install job and the product wasn't registered, it's safe
    // to delete the ClientState key.  We need to remove it because it contains
    // data like "ap", browsertype, language, etc. that need to be cleaned up in
    // case user tries to install again in the future.
    if (!is_update_ &&
        !app_manager.IsProductRegistered(app_data_.app_guid())) {
      // Need to set IsUninstalled to true or else we'll assert in
      // RemoveClientState().
      app_data_.set_is_uninstalled(true);
      app_manager.RemoveClientState(app_data_);
    }

    return hr;
  }

  hr = UpdateJob();
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[UpdateJobAndRegistry failed][0x%08x][%s]"),
                  hr, GuidToString(app_data_.app_guid())));
    completion_info->status = COMPLETION_ERROR;
    completion_info->error_code = hr;
    completion_info->text.FormatMessage(IDS_INSTALL_FAILED, hr);
    return hr;
  }

  hr = UpdateRegistry(new_app_data);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[UpdateJobAndRegistry failed][0x%08x][%s]"),
                  hr, GuidToString(app_data_.app_guid())));
    completion_info->status = COMPLETION_ERROR;
    completion_info->error_code = hr;
    completion_info->text.FormatMessage(IDS_INSTALL_FAILED, hr);
    return hr;
  }

  // We do not know whether Goopdate has succeeded because its installer has
  // not completed.
  if (!::IsEqualGUID(kGoopdateGuid, app_data_.app_guid())) {
    app_manager.RecordSuccessfulInstall(app_data_.parent_app_guid(),
                                        app_data_.app_guid(),
                                        is_update_,
                                        is_offline_);
  }

  return S_OK;
}

// Update the version and the language in the job using the values written by
// the installer.
HRESULT Job::UpdateJob() {
  CORE_LOG(L2, (_T("[Job::UpdateJob]")));
  AppManager app_manager(app_data_.is_machine_app());
  AppData data;
  HRESULT hr = app_manager.ReadAppDataFromStore(app_data_.parent_app_guid(),
                                                app_data_.app_guid(),
                                                &data);
  ASSERT1(SUCCEEDED(hr));
  if (SUCCEEDED(hr)) {
    app_data_.set_version(data.version());
    app_data_.set_language(data.language());
  } else {
    CORE_LOG(LW, (_T("[ReadApplicationData failed][0x%08x][%s]"),
                  hr, GuidToString(app_data_.app_guid())));
    // Continue without the data from the registry.
  }

  return S_OK;
}

// Update the registry with the information from the job.
HRESULT Job::UpdateRegistry(AppData* new_app_data) {
  CORE_LOG(L2, (_T("[Job::UpdateRegistry]")));
  ASSERT1(new_app_data);

  *new_app_data = app_data_;
  AppManager app_manager(app_data_.is_machine_app());
  // Update the client registry information and the client state with the
  // information in this job.
  HRESULT hr = app_manager.InitializeApplicationState(new_app_data);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[WriteAppParamsToRegistry failed][0x%08x][%s]"),
                  hr, GuidToString(new_app_data->app_guid())));
    return hr;
  }

  return S_OK;
}

HRESULT Job::GetInstallerData(CString* installer_data) const {
  ASSERT1(installer_data);
  installer_data->Empty();

  if (!app_data().encoded_installer_data().IsEmpty()) {
    CString decoded_installer_data;
    HRESULT hr = Utf8UrlEncodedStringToWideString(
                     app_data().encoded_installer_data(),
                     &decoded_installer_data);
    ASSERT(SUCCEEDED(hr), (_T("[Utf8UrlEncodedStringToWideString][0x%x]"), hr));

    if (FAILED(hr) || CString(decoded_installer_data).Trim().IsEmpty()) {
      return GOOPDATE_E_INVALID_INSTALLER_DATA_IN_APPARGS;
    }
    *installer_data = decoded_installer_data;
    return S_OK;
  }

  if (app_data().install_data_index().IsEmpty()) {
    return S_OK;
  }

  CString data(response_data().GetInstallData(app_data().install_data_index()));
  if (CString(data).Trim().IsEmpty()) {
    return GOOPDATE_E_INVALID_INSTALL_DATA_INDEX;
  }

  *installer_data = data;
  return S_OK;
}

bool IsCompletionStatusSuccess(JobCompletionStatus status) {
  switch (status) {
    case COMPLETION_SUCCESS:
    case COMPLETION_SUCCESS_REBOOT_REQUIRED:
      return true;
    case COMPLETION_ERROR:
    case COMPLETION_INSTALLER_ERROR_MSI:
    case COMPLETION_INSTALLER_ERROR_SYSTEM:
    case COMPLETION_INSTALLER_ERROR_OTHER:
    case COMPLETION_CANCELLED:
    default:
      return false;
  }
}

bool IsCompletionStatusInstallerError(JobCompletionStatus status) {
  switch (status) {
    case COMPLETION_INSTALLER_ERROR_MSI:
    case COMPLETION_INSTALLER_ERROR_SYSTEM:
    case COMPLETION_INSTALLER_ERROR_OTHER:
      return true;
    case COMPLETION_SUCCESS:
    case COMPLETION_SUCCESS_REBOOT_REQUIRED:
    case COMPLETION_ERROR:
    case COMPLETION_CANCELLED:
    default:
      return false;
  }
}

CString CompletionInfo::ToString() const {
  CString error_code_string = FormatErrorCode(error_code);

  // Get the format string for the status. Must have %s placeholder for code.
  CString status_format;
  switch (status) {
    case COMPLETION_SUCCESS:
      status_format = _T("Installer succeeded with code %s.");
      break;
    case COMPLETION_SUCCESS_REBOOT_REQUIRED:
      status_format = _T("Installer succeeded but reboot required. Code %s.");
      break;
    case COMPLETION_ERROR:
      status_format = _T("Omaha failed with error code %s.");
      break;
    case COMPLETION_INSTALLER_ERROR_MSI:
      status_format = _T("Installer failed with MSI error %s.");
      break;
    case COMPLETION_INSTALLER_ERROR_SYSTEM:
      status_format = _T("Installer failed with system error %s.");
      break;
    case COMPLETION_INSTALLER_ERROR_OTHER:
      status_format = _T("Installer failed with error code %s.");
      break;
    case COMPLETION_CANCELLED:
      status_format = _T("Operation canceled. Code %s.");
      break;
    default:
      status_format = _T("Unknown status. Code %s.");
      ASSERT1(false);
  }

  CString output;
  output.Format(status_format, error_code_string);

  if (!text.IsEmpty()) {
    output.Append(_T(" ") + text);
  }

  return output;
}

}  // namespace omaha

