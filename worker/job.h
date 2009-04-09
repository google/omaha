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
// job.h : Contains the JobList and the Job class.
// The Job goes through a number of states as represented by the following
// state diagram for updates and installs.
//
//                  START
//                    |
//               DOWNLOAD STARTED ----------|
//                    |                     |-> COMPLETED(ERROR)
//              DOWNLOAD COMPLETED ---------|
//                    |                     |
//              INSTALL STARTED    ---------|
//                    |
//          COMPLETED(SUCCESS|RESTART BROWSER)
//

#ifndef OMAHA_WORKER_JOB_H__
#define OMAHA_WORKER_JOB_H__

#include <windows.h>
#include <atlstr.h>
#include <vector>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "goopdate/google_update_idl.h"
#include "omaha/common/browser_utils.h"
#include "omaha/common/constants.h"
#include "omaha/goopdate/update_response.h"
#include "omaha/net/network_request.h"
#include "omaha/worker/application_data.h"
#include "omaha/worker/application_manager.h"

namespace omaha {

class DownloadManager;
class InstallManager;
class JobObserver;
class Ping;
class UpdateResponse;

enum JobCompletionStatus {
  COMPLETION_SUCCESS = 0,
  COMPLETION_SUCCESS_REBOOT_REQUIRED,
  COMPLETION_ERROR,
  COMPLETION_INSTALLER_ERROR_MSI,
  COMPLETION_INSTALLER_ERROR_SYSTEM,
  COMPLETION_INSTALLER_ERROR_OTHER,
  COMPLETION_CANCELLED,
};

// Represents the text and the error code of an error message.
struct CompletionInfo {
  CompletionInfo()
      : status(COMPLETION_SUCCESS),
        error_code(S_OK) {}
  CompletionInfo(JobCompletionStatus stat,
                 DWORD error,
                 CString txt)
      : status(stat),
        error_code(error),
        text(txt) {}

  CString ToString() const;

  JobCompletionStatus status;  // Job status on completion.
  DWORD error_code;
  CString text;                // Success or failure text.
};

bool IsCompletionStatusSuccess(JobCompletionStatus status);
bool IsCompletionStatusInstallerError(JobCompletionStatus status);
inline bool IsCompletionSuccess(CompletionInfo info) {
  return IsCompletionStatusSuccess(info.status);
}
inline bool IsCompletionInstallerError(CompletionInfo info) {
  return IsCompletionStatusInstallerError(info.status);
}


// These are reported in failure pings. If changing existing values, let the
// team know and maybe change the kJobStateExtraCodeMask.
enum JobState {
  JOBSTATE_START = 1,               // Initial state.
  JOBSTATE_DOWNLOADSTARTED,         // Started the download of the installer.
  JOBSTATE_DOWNLOADCOMPLETED,       // Completed the download from the server.
  JOBSTATE_INSTALLERSTARTED,        // Installer run.
  JOBSTATE_COMPLETED                // Job completed.
};

// The job contains information about install or update jobs.
class Job : public NetworkRequestCallback {
 public:
  // Differentiates JobState values from other extra_code1 values.
  static const int kJobStateExtraCodeMask = 0x10000000;

  Job(bool is_update, Ping* ping);
  virtual ~Job();

  // Transition the job into an completed state.
  virtual void NotifyCompleted(const CompletionInfo& info);

  // NetworkRequestCallback implementation.
  virtual void OnProgress(int bytes,
                          int bytes_total,
                          int status,
                          const TCHAR* status_text);

  void RestartBrowsers();
  void LaunchBrowser(const CString& url);

  // If the installer has set a custom command line in the installer results
  // registry, we run the command after a successful interactive install.
  HRESULT LaunchCmdLine();

  virtual HRESULT Download(DownloadManager* manager);
  virtual HRESULT Install();



  // Getters and Setters for all the data members.

  const AppData& app_data() const { return app_data_; }
  void set_app_data(const AppData& app_data);

  CString download_file_name() const { return download_file_name_; }
  void set_download_file_name(const CString& download_file_name) {
    download_file_name_ = download_file_name;
  }

  CString launch_cmd_line() const { return launch_cmd_line_; }
  void set_launch_cmd_line(const CString& launch_cmd_line) {
    launch_cmd_line_ = launch_cmd_line;
  }

  uint32 user_explorer_pid() const { return user_explorer_pid_; }
  void set_user_explorer_pid(uint32 explorer_pid) {
    user_explorer_pid_ = explorer_pid;
  }

  const CompletionInfo& info() const { return info_; }
  void set_extra_code1(int extra_code1) {
    extra_code1_ = extra_code1;
  }
  JobState job_state() const { return job_state_; }
  int bytes_downloaded() const { return bytes_downloaded_; }
  int bytes_total() const { return bytes_total_; }
  void set_job_observer(JobObserver* job_observer) {
    job_observer_ = job_observer;
  }
  bool is_machine_app() const { return app_data_.is_machine_app(); }

  void set_update_response_data(const UpdateResponseData& response_data) {
    update_response_data_ = response_data;
  }

  const UpdateResponseData& response_data() const {
    return update_response_data_;
  }

  bool is_update() const {
    return is_update_;
  }
  bool is_background() const {
    return is_background_;
  }
  void set_is_background(bool is_background) {
    is_background_ = is_background;
  }
  void set_is_update_check_only(bool is_update_check_only) {
    is_update_check_only_ = is_update_check_only;
  }

  HRESULT GetInstallerData(CString* installer_data) const;

 private:
  HRESULT ShouldRestartBrowser() const;
  CompletionCodes CompletionStatusToCompletionCode(
      JobCompletionStatus status) const;
  HRESULT DoCompleteJob();
  HRESULT SendStateChangePing(JobState previous_state);
  PingEvent::Types JobStateToEventType() const;
  void ChangeState(JobState state, bool send_ping);
  HRESULT NotifyUI();
  void NotifyDownloadStarted();
  void NotifyDownloadComplete();
  void NotifyInstallStarted();
  HRESULT DoInstall(CompletionInfo* completion_info, AppData* new_app_data);
  HRESULT UpdateJob();
  HRESULT UpdateRegistry(AppData* new_app_data);
  HRESULT DeleteJobDownloadDirectory() const;

  AppData app_data_;
  CString download_file_name_;   // The name of the downloaded file.
  CString launch_cmd_line_;      // Custom command to run after successful
                                 // install.
  CompletionInfo info_;          // The completion info.
  int extra_code1_;              // Extra info to send in pings.
                                 // Cleared after each ping.
  JobState job_state_;           // The state of the job.
  JobObserver* job_observer_;    // Pointer to an observer for the job
                                 // events.

  // Other miscellaneous information.
  uint32 user_explorer_pid_;     // Identifies user that initiated job.

  UpdateResponseData update_response_data_;

  // Represent the job information during download.
  int bytes_downloaded_;         // The number of bytes downloaded.
  int bytes_total_;              // The download file size in bytes.
  Ping* ping_;                   // The Ping object used to send pings.

  // True if the job is any kind of update and false if it is an install.
  bool is_update_;

  // True if the job is running silently in the background but not if running
  // silently at the request of another process.
  bool is_background_;

  // True if the job is intended to do an update check only, for instance, in
  // an on demand check for updates case.
  bool is_update_check_only_;

  // True if the launch command failed to launch. Only set if the launch was
  // attempted and failed. Does not reflect successful execution or exit code.
  bool did_launch_cmd_fail_;

  friend class InstallManagerTest;
  friend class JobTest;

  DISALLOW_EVIL_CONSTRUCTORS(Job);
};

typedef std::vector<Job*> Jobs;

}  // namespace omaha

#endif  // OMAHA_WORKER_JOB_H__

