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

#include "omaha/internal/chrome_recovery_improved/recovery.h"

#include "omaha/base/app_util.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/file.h"
#include "omaha/base/logging.h"
#include "omaha/base/reg_key.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/system.h"
#include "omaha/base/time.h"
#include "omaha/common/app_registry_utils.h"
#include "omaha/common/command_line_builder.h"
#include "omaha/common/const_cmd_line.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/common/const_group_policy.h"
#include "omaha/common/experiment_labels.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/common/ping.h"
#include "omaha/third_party/smartany/scoped_any.h"

namespace omaha {

const TCHAR ChromeRecoveryImproved::kChromeName[] = _T("Google Chrome");
const TCHAR ChromeRecoveryImproved::kLongLabelName[] = _T("chromerec3");
const TCHAR ChromeRecoveryImproved::kExtraLabelName[] = _T("chromrec3extra");

const int ChromeRecoveryImproved::kPingResult_NoRecovery = 0;
const int ChromeRecoveryImproved::kPingResult_RecoverySuccessful = 2;

ChromeRecoveryImproved::ChromeRecoveryImproved(bool is_machine,
                                               const CString& browser_guid,
                                               const CString& browser_version,
                                               const CString& session_id)
    : is_machine_(is_machine),
      browser_guid_(browser_guid),
      browser_version_(browser_version),
      session_id_(session_id),
      omaha_install_error_code_(0) {}

RecoveryResultCode ChromeRecoveryImproved::Repair() {
  const HRESULT hr = DoRepair();

  CString experiment_label;
  SafeCStringFormat(&experiment_label,
                    _T("%6d%s"),
                    GetRunCohort(),
                    SUCCEEDED(hr) ? _T("R") : _T("F"));
  const int result_code = SUCCEEDED(hr) ?
      kPingResult_RecoverySuccessful : kPingResult_NoRecovery;
  const int error_code = static_cast<int>(hr);
  const int extra_code = static_cast<int>(omaha_install_error_code_);

  SetExperimentLabel(experiment_label);
  SendResultPing(result_code, error_code, extra_code);

  return SUCCEEDED(hr) ? RECOVERY_SUCCEEDED : RECOVERY_NO_RECOVERY;
}

HRESULT ChromeRecoveryImproved::DoRepair() {
  ResetInstallState();

  HRESULT hr = RestoreChromeRegEntries();
  if (FAILED(hr)) {
    OPT_LOG(L1, (_T("[RestoreChromeRegEntries failed][%#x]"), hr));
    return hr;
  }

  hr = InstallOmaha();
  if (FAILED(hr)) {
    OPT_LOG(L1, (_T("[InstallOmaha failed][%#x]"), hr));
    return hr;
  }


  VERIFY_SUCCEEDED(TriggerUpdateCheck());

  return S_OK;
}

void ChromeRecoveryImproved::ResetInstallState() {
  ResetChromeInstallState();
  ResetOmahaInstallState();
}

void ChromeRecoveryImproved::ResetChromeInstallState() {
  // Nothing to do for now.
}

void ChromeRecoveryImproved::ResetOmahaInstallState() {
  // Omaha installer checks 'pv' value to see whether Omaha already exists,
  // and skips installation if existing version is higher.
  // Delete 'pv' in case the broken existing Omaha blocks new install.
  const CString state_key =
      app_registry_utils::GetAppClientsKey(is_machine_, GOOPDATE_APP_ID);
  RegKey::DeleteValue(state_key, kRegValueProductVersion);
}

HRESULT ChromeRecoveryImproved::RestoreChromeRegEntries() {
  OPT_LOG(L1, (_T("[RestoreChromeRegEntries][%d][%s][%s]"),
               is_machine_, browser_guid_, browser_version_));

  // Create a minimal Clients / ClientState.  The Chrome installer
  // recreates the remaining state when it runs at some point in the future.
  HRESULT hr = BuildChromeClientKey();
  if (FAILED(hr)) {
    return hr;
  }

  hr = BuildChromeClientStateKey();
  if (FAILED(hr)) {
    return hr;
  }

  return S_OK;
}

HRESULT ChromeRecoveryImproved::BuildChromeClientKey() {
  const CString kClientsKey =
      app_registry_utils::GetAppClientsKey(is_machine_, browser_guid_);

  RegKey key;
  HRESULT hr = key.Create(kClientsKey);
  if (FAILED(hr)) {
    return hr;
  }

  hr = key.SetValue(kRegValueProductVersion, browser_version_);
  if (FAILED(hr)) {
    return hr;
  }

  key.SetValue(kRegValueAppName, kChromeName);

  return S_OK;
}

HRESULT ChromeRecoveryImproved::BuildChromeClientStateKey() {
  const CString kClientStateKey =
      app_registry_utils::GetAppClientStateKey(is_machine_, browser_guid_);

  RegKey key;
  HRESULT hr = key.Create(kClientStateKey);
  if (FAILED(hr)) {
    return hr;
  }

  key.SetValue(kRegValueAdditionalParams, _T(""));
  key.SetValue(kRegValueBrandCode, kDefaultGoogleUpdateBrandCode);

  return S_OK;
}

HRESULT ChromeRecoveryImproved::InstallOmaha() {
  CPath metainstaller_path(app_util::GetCurrentModuleDirectory());
  VERIFY1(metainstaller_path.Append(kOmahaMetainstallerFileName));
  if (!File::Exists(metainstaller_path)) {
    OPT_LOG(LE, (_T("[InstallOmaha][couldn't find metainstaller]")));
    return GOOPDATE_E_METAINSTALLER_NOT_FOUND;
  }

  // Generate arguments for a silent runtime install.
  CommandLineBuilder builder(COMMANDLINE_MODE_INSTALL);
  builder.set_is_silent_set(true);
  builder.set_install_source(kCmdLineInstallSource_ChromeRecovery);
  CString extra_args;
  SafeCStringFormat(&extra_args,
                    _T("runtime=true&needsadmin=%s"),
                    is_machine_ ? _T("true") : _T("false"));
  builder.set_extra_args(extra_args);
  const CString cmd_line_args = builder.GetCommandLineArgs();

  // Start the process and wait for it to exit.
  scoped_process install_process;
  HRESULT hr = LaunchProcess(metainstaller_path,
                            cmd_line_args,
                            address(install_process));
  if (FAILED(hr)) {
    OPT_LOG(LE, (_T("[InstallOmaha][couldn't start MI][%s][%s][%#x]"),
                 metainstaller_path, cmd_line_args, hr));
    return hr;
  }

  OPT_LOG(L1, (_T("[InstallOmaha][launched MI, waiting for exit code]")));

  DWORD error_code = 0;
  hr = WaitAndGetExitCode(get(install_process), &error_code);
  OPT_LOG(L1, (_T("[InstallOmaha][finished][%#x][%#x]"), hr, error_code));

  omaha_install_error_code_ = error_code;

  return SUCCEEDED(hr) ? error_code : hr;
}

HRESULT ChromeRecoveryImproved::SetLabelOnApp(const CString& label_value,
                                              const CString& app_id) {
  const time64 k90Days100ns = static_cast<time64>(kDaysTo100ns) * 90;
  const time64 now = GetCurrent100NSTime();

  CString label(ExperimentLabels::CreateLabel(
      kLongLabelName, label_value, now + k90Days100ns));
  return ExperimentLabels::WriteRegistry(is_machine_, app_id, label);
}

HRESULT ChromeRecoveryImproved::TriggerUpdateCheck() {
  OPT_LOG(L1, (_T("[TriggerUpdateCheck][is_machine: %d]"), is_machine_));

  // TODO(sorin): consider deleting LastChecked.

  CommandLineBuilder builder(COMMANDLINE_MODE_UA);
  builder.set_is_machine_set(is_machine_);
  builder.set_install_source(kCmdLineInstallSource_ChromeRecovery);
  const CString cmd_line_args = builder.GetCommandLineArgs();

  HRESULT hr = goopdate_utils::StartGoogleUpdateWithArgs(is_machine_,
                                                         StartMode::kBackground,
                                                         cmd_line_args,
                                                         NULL);
  if (FAILED(hr)) {
    OPT_LOG(LE, (_T("[couldn't start UA][%s][%#x]"), cmd_line_args, hr));
  }

  return hr;
}


HRESULT ChromeRecoveryImproved::SendResultPing(
                       int result_code,
                       int error_code,
                       int extra_code) {
  OPT_LOG(L1, (_T("[SendResultPing][%d][%d/%d/%d]"),
               is_machine_, result_code, error_code, extra_code));

  CString omaha_pv;
  app_registry_utils::GetAppVersion(is_machine_, kGoogleUpdateAppId, &omaha_pv);

  Ping ping(is_machine_, session_id_, kCmdLineInstallSource_ChromeRecovery);
  PingEventPtr ping_event(
      new PingEvent(PingEvent::EVENT_CHROME_RECOVERY_COMPONENT,
                    static_cast<PingEvent::Results>(result_code),
                    error_code,
                    extra_code));
  ping.LoadOmahaDataFromRegistry();

  ping.BuildOmahaPing(omaha_pv, omaha_pv, ping_event);
  return SendReliablePing(&ping, false);
}

int ChromeRecoveryImproved::GetRunCohort() {
  const time64 kNow = GetCurrent100NSTime();

  SYSTEMTIME st = Time64ToSystemTime(kNow);
  const int year = static_cast<int>(st.wYear);

  // Compute the ordinal week count (typically 1-52; will be 53 for 12/31 on
  // a leap year).  On failure, use zero for a week.
  //
  // This algorithm does not need to be particularly precise wrt the
  // Gregorian calendar, nor locale-specific.  It just needs to operate the
  // same on all machines for the purpose of getting cohort groups.
  int week = 0;
  if (st.wYear != 0) {
    st.wMonth = 1;
    st.wDay = 1;
    st.wHour = 0;
    st.wMinute = 0;
    st.wSecond = 0;
    st.wMilliseconds = 0;
    time64 year_start = SystemTimeToTime64(&st);
    if (year_start != 0) {
      week = static_cast<int>(((kNow - year_start) / kDaysTo100ns) / 7) + 1;
    }
  }

  return 100 * year + week;
}

HRESULT ChromeRecoveryImproved::LaunchProcess(const CString& exe_path,
                                              const CString& args,
                                              HANDLE* process_out) {
  PROCESS_INFORMATION pi = {0};

  HRESULT hr = System::StartProcessWithArgsAndInfo(exe_path, args, &pi);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[LaunchProcess][StartProcessWithArgsAndInfo][%#x]"),
                  hr));
    return hr;
  }

  ASSERT1(pi.hProcess);
  VERIFY1(::CloseHandle(pi.hThread));

  *process_out = pi.hProcess;
  return hr;
}

HRESULT ChromeRecoveryImproved::WaitAndGetExitCode(HANDLE process,
                                                   DWORD* exit_code) {
  ASSERT1(exit_code);

  if (::WaitForSingleObject(process, INFINITE) == WAIT_FAILED) {
    const DWORD error = ::GetLastError();
    CORE_LOG(LE, (_T("[WaitAndGetExitCode][WaitForSingleObject failed][%u]"),
                  error));
    return HRESULT_FROM_WIN32(error);
  }

  if (!::GetExitCodeProcess(process, exit_code)) {
    const DWORD error = ::GetLastError();
    CORE_LOG(LE, (_T("[WaitAndGetExitCode][GetExitCodeProcess failed][%u]"),
                  error));
    return HRESULT_FROM_WIN32(error);
  }

  return S_OK;
}

void ChromeRecoveryImproved::SetExperimentLabel(const CString& label) {
  OPT_LOG(L1, (_T("[SetExperimentLabel][%s]"), label));
  VERIFY_SUCCEEDED(SetLabelOnApp(label, kGoogleUpdateAppId));
  VERIFY_SUCCEEDED(SetLabelOnApp(label, browser_guid_));
}

}  // namespace omaha
