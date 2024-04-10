// Copyright 2009-2010 Google Inc.
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

#ifndef OMAHA_CLIENT_INSTALL_PROGRESS_OBSERVER_H_
#define OMAHA_CLIENT_INSTALL_PROGRESS_OBSERVER_H_

#include <windows.h>
#include <atlstr.h>
#include <vector>

#include "omaha/base/safe_format.h"
#include "omaha/base/time.h"

namespace omaha {

// TODO(omaha3): These specify the completion UI to display not the job
// result as they did in Omaha 2.
// Some of them may not be necessary depending on how information is
// communicated to the observer. For example, should the COM API allow reboot/
// restart browser AND launchcmd?
// If we keep this enum, rename to something like CompletionTypes to make it
// clear that it is describing the desired behavior of the observer. This also
// differentiates the name from LegacyCompletionCodes.
// TODO(omaha): If the codes below change, need a conversion method to convert
// to LegacyCompletionCodes.
typedef enum {
  COMPLETION_CODE_SUCCESS = 1,
  COMPLETION_CODE_EXIT_SILENTLY,
  COMPLETION_CODE_ERROR = COMPLETION_CODE_SUCCESS + 2,
  COMPLETION_CODE_RESTART_ALL_BROWSERS,
  COMPLETION_CODE_REBOOT,
  COMPLETION_CODE_RESTART_BROWSER,
  COMPLETION_CODE_RESTART_ALL_BROWSERS_NOTICE_ONLY,
  COMPLETION_CODE_REBOOT_NOTICE_ONLY,
  COMPLETION_CODE_RESTART_BROWSER_NOTICE_ONLY,
  COMPLETION_CODE_LAUNCH_COMMAND,
  COMPLETION_CODE_EXIT_SILENTLY_ON_LAUNCH_COMMAND,
  COMPLETION_CODE_INSTALL_FINISHED_BEFORE_CANCEL,
} CompletionCodes;

inline bool IsCompletionCodeSuccess(CompletionCodes completion_code) {
  switch (completion_code) {
    case COMPLETION_CODE_SUCCESS:
    case COMPLETION_CODE_EXIT_SILENTLY:
    case COMPLETION_CODE_RESTART_ALL_BROWSERS:
    case COMPLETION_CODE_REBOOT:
    case COMPLETION_CODE_RESTART_BROWSER:
    case COMPLETION_CODE_RESTART_ALL_BROWSERS_NOTICE_ONLY:
    case COMPLETION_CODE_REBOOT_NOTICE_ONLY:
    case COMPLETION_CODE_RESTART_BROWSER_NOTICE_ONLY:
    case COMPLETION_CODE_LAUNCH_COMMAND:
    case COMPLETION_CODE_EXIT_SILENTLY_ON_LAUNCH_COMMAND:
    case COMPLETION_CODE_INSTALL_FINISHED_BEFORE_CANCEL:
      return true;

    case COMPLETION_CODE_ERROR:
    default:
      return false;
  }
}

struct AppCompletionInfo {
  CString display_name;
  CString app_id;
  CString completion_message;
  CompletionCodes completion_code;
  HRESULT error_code;
  int extra_code1;
  uint32 installer_result_code;
  bool is_canceled;
  bool is_noupdate;  // noupdate response from server
  CString post_install_launch_command_line;
  CString post_install_url;

  AppCompletionInfo() : completion_code(COMPLETION_CODE_SUCCESS),
                        error_code(S_OK),
                        extra_code1(0),
                        installer_result_code(0),
                        is_canceled(false),
                        is_noupdate(false) {}

#ifdef DEBUG
  CString ToString() const {
    CString result;
    SafeCStringFormat(&result,
        _T("[AppCompletionInfo][%s][%s][0x%x][%d][%d][%d][%d][cmd_line=%s]"),
        app_id, completion_message, error_code, extra_code1,
        installer_result_code, is_canceled, is_noupdate,
        post_install_launch_command_line);
    return result;
  }
#endif
};

struct ObserverCompletionInfo {
  CompletionCodes completion_code;
  CString completion_text;
  CString help_url;
  std::vector<AppCompletionInfo> apps_info;

  explicit ObserverCompletionInfo(CompletionCodes code)
      : completion_code(code) {}

#ifdef DEBUG
  CString ToString() const {
    CString result;
    SafeCStringFormat(&result, _T("[ObserverCompletionInfo][code=%d][text=%s]"),
                      completion_code, completion_text);
    for (size_t i = 0; i < apps_info.size(); ++i) {
      SafeCStringAppendFormat(&result, _T("[%s]"), apps_info[i].ToString());
    }
    return result;
  }
#endif
};

// TODO(omaha3): This is bundle-centric. Add support for individual app-updates
// when we have a better idea of the new bundle-supporting UI.
class InstallProgressObserver {
 public:
  virtual ~InstallProgressObserver() {}
  virtual void OnCheckingForUpdate() = 0;
  virtual void OnUpdateAvailable(const CString& app_id,
                                 const CString& app_name,
                                 const CString& version_string) = 0;
  virtual void OnWaitingToDownload(const CString& app_id,
                                   const CString& app_name) = 0;
  virtual void OnDownloading(const CString& app_id,
                             const CString& app_name,
                             int time_remaining_ms,
                             int pos) = 0;
  virtual void OnWaitingRetryDownload(const CString& app_id,
                                      const CString& app_name,
                                      time64 next_retry_time) = 0;
  virtual void OnWaitingToInstall(const CString& app_id,
                                  const CString& app_name,
                                  bool* can_start_install) = 0;
  virtual void OnInstalling(const CString& app_id,
                            const CString& app_name,
                            int time_remaining_ms,
                            int pos) = 0;
  virtual void OnPause() = 0;
  virtual void OnComplete(const ObserverCompletionInfo& observer_info) = 0;
};

// Launches the post-install launch command lines for each app in `info`.
bool LaunchCommandLines(const ObserverCompletionInfo& info, bool is_machine);

}  // namespace omaha

#endif  // OMAHA_CLIENT_INSTALL_PROGRESS_OBSERVER_H_
