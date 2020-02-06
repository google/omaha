// Copyright 2018 Google Inc.
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

#ifndef OMAHA_INTERNAL_CHROME_RECOVERY_IMPROVED_RECOVERY_H_
#define OMAHA_INTERNAL_CHROME_RECOVERY_IMPROVED_RECOVERY_H_

#include <windows.h>
#include <atlstr.h>

#include "base/basictypes.h"

namespace omaha {

enum RecoveryResultCode {
  RECOVERY_SUCCEEDED,
  RECOVERY_NO_RECOVERY,
  RECOVERY_NEEDS_ADMIN,
};

class ChromeRecoveryImproved {
 public:
  ChromeRecoveryImproved(bool is_machine,
                         const CString& browser_guid,
                         const CString& browser_version,
                         const CString& session_id);

  RecoveryResultCode Repair();

 private:
  // Cleans up the machine by resetting Chrome and Omaha install state for
  // upcoming recovery with best effort. This is to reduce the possibility of
  // existing (broken) Omaha or Chrome from affecting a new install.
  void ResetInstallState();
  void ResetChromeInstallState();
  void ResetOmahaInstallState();

  // Synthesizes Clients and ClientState entries for Chrome.
  HRESULT RestoreChromeRegEntries();
  HRESULT BuildChromeClientKey();
  HRESULT BuildChromeClientStateKey();

  // Sets a long-lived experiment label on Omaha, for Omaha and Chrome browser.
  void SetExperimentLabel(const CString& label);
  HRESULT SetLabelOnApp(const CString& label_value,
                        const CString& app_id);

  // Installs Omaha in runtime mode.
  HRESULT InstallOmaha();

  // Sends a result ping.
  HRESULT SendResultPing(int result_code,
                         int error_code,
                         int extra_code);

  // Triggers an update check with Omaha.
  HRESULT TriggerUpdateCheck();

  HRESULT DoRepair();

  // Returns an integer representing the cohort encoded as YYYYWW where WW is
  // the week the year.
  static int GetRunCohort();

  static HRESULT LaunchProcess(const CString& exe_path,
                               const CString& args,
                               HANDLE* process_out);
  static HRESULT WaitAndGetExitCode(HANDLE process, DWORD* exit_code);

  const bool is_machine_;
  const CString browser_guid_;
  const CString browser_version_;
  const CString session_id_;

  // The error code returned by the Omaha metainstaller.
  DWORD omaha_install_error_code_;

  static const TCHAR kChromeName[];
  static const TCHAR kLongLabelName[];
  static const TCHAR kExtraLabelName[];

  static const int kPingResult_NoRecovery;
  static const int kPingResult_RecoverySuccessful;

  DISALLOW_COPY_AND_ASSIGN(ChromeRecoveryImproved);
};

}  // namespace omaha

#endif  // OMAHA_INTERNAL_CHROME_RECOVERY_IMPROVED_RECOVERY_H_
