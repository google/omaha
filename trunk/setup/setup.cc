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
// This class handles the installation of Google Update when it is run with the
// install or update switch. It is invoked when Google Update is launched from
// the meta-installer and as part of self-update.
//
//
// *** Documentation for OEM Installs ***
//
// A /oem install requires:
//  * Per-machine install
//  * Running as admin
//  * Running in Windows audit mode (ConfigManager::IsWindowsInstalling())
//  * Standalone installer (determined in LaunchInstalledWorker())
//
// If the first three conditions are met, this class writes the OemInstallTime
// registry value, which is used by ConfigManager::IsOemInstalling() along with
// other logic to determine whether Google Update is running in an OEM install
// environment.
// Other objects use IsOemInstalling() - not IsWindowsInstalling() - to
// determine whether to run in a disabled mode for OEM factory installations.
// For example, the core exits immediately without checking for updates or Code
// Red events, no instances ping, and persistent IDs are not saved.
// OemInstallTime is never cleared. The logic in IsOemInstalling() determines
// when the system is no longer in an OEM install mode.

#include "omaha/setup/setup.h"
#include <regstr.h>
#include <algorithm>
#include <functional>
#include <vector>
#include "omaha/common/app_util.h"
#include "omaha/common/const_cmd_line.h"
#include "omaha/common/const_object_names.h"
#include "omaha/common/constants.h"
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/file.h"
#include "omaha/common/highres_timer-win32.h"
#include "omaha/common/logging.h"
#include "omaha/common/omaha_version.h"
#include "omaha/common/path.h"
#include "omaha/common/process.h"
#include "omaha/common/reg_key.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/scope_guard.h"
#include "omaha/common/synchronized.h"
#include "omaha/common/system.h"
#include "omaha/common/time.h"
#include "omaha/common/timer.h"
#include "omaha/common/user_info.h"
#include "omaha/common/utils.h"
#include "omaha/common/vistautil.h"
#include "omaha/common/xml_utils.h"
#include "omaha/net/http_client.h"
#include "omaha/goopdate/command_line_builder.h"
#include "omaha/goopdate/config_manager.h"
#include "omaha/goopdate/const_goopdate.h"
#include "omaha/goopdate/event_logger.h"
#include "omaha/goopdate/goopdate_utils.h"
#include "omaha/goopdate/stats_uploader.h"
#include "omaha/goopdate/ui_displayed_event.h"
#include "omaha/setup/setup_files.h"
#include "omaha/setup/setup_google_update.h"
#include "omaha/setup/setup_metrics.h"
#include "omaha/setup/setup_service.h"

namespace omaha {

namespace {

const int kVersion10 = 10;
const int kVersion11 = 11;
const int kVersion11MachineLock = 111;
const int kVersion12 = 12;

void GetShutdownEventAttributes(bool is_machine, NamedObjectAttributes* attr) {
  ASSERT1(attr);
  GetNamedObjectAttributes(kShutdownEvent, is_machine, attr);
}

// Returns the process's mode based on its command line.
HRESULT GetProcessModeFromPid(uint32 pid, CommandLineMode* mode) {
  ASSERT1(mode);

  CString cmd_line;
  HRESULT hr = Process::GetCommandLine(pid, &cmd_line);
  if (FAILED(hr)) {
    SETUP_LOG(LE, (_T("[GetCommandLine failed][%u][0x%08x]"), pid, hr));
    return hr;
  }

  CommandLineArgs args;
  hr = ParseCommandLine(cmd_line, &args);
  if (FAILED(hr)) {
    SETUP_LOG(LE, (_T("[ParseCommandLine failed][%u][%s][0x%08x]"),
                   pid, cmd_line, hr));
    return hr;
  }

  OPT_LOG(L2, (_T("[Process %u][cmd line %s][mode %u]"),
               pid, cmd_line, args.mode));
  *mode = args.mode;
  return S_OK;
}

void IncrementProcessWaitFailCount(CommandLineMode mode) {
  switch (mode) {
    case COMMANDLINE_MODE_UNKNOWN:
      ++metric_setup_process_wait_failed_unknown;
      break;
    case COMMANDLINE_MODE_NOARGS:  // Legacy install
    case COMMANDLINE_MODE_LEGACYUI:
    case COMMANDLINE_MODE_LEGACY_MANIFEST_HANDOFF:
      ++metric_setup_process_wait_failed_legacy;
      break;
    case COMMANDLINE_MODE_CORE:
      ++metric_setup_process_wait_failed_core;
      break;
    case COMMANDLINE_MODE_REPORTCRASH:
      ++metric_setup_process_wait_failed_report;
      break;
    case COMMANDLINE_MODE_UPDATE:
      ++metric_setup_process_wait_failed_update;
      break;
    case COMMANDLINE_MODE_IG:
      ++metric_setup_process_wait_failed_ig;
      break;
    case COMMANDLINE_MODE_HANDOFF_INSTALL:
      ++metric_setup_process_wait_failed_handoff;
      break;
    case COMMANDLINE_MODE_UG:
      ++metric_setup_process_wait_failed_ug;
      break;
    case COMMANDLINE_MODE_UA:
      ++metric_setup_process_wait_failed_ua;
      break;
    case COMMANDLINE_MODE_CODE_RED_CHECK:
      ++metric_setup_process_wait_failed_cr;
      break;
    case COMMANDLINE_MODE_CRASH_HANDLER:
    case COMMANDLINE_MODE_SERVICE:
    case COMMANDLINE_MODE_SERVICE_REGISTER:
    case COMMANDLINE_MODE_SERVICE_UNREGISTER:
    case COMMANDLINE_MODE_REGSERVER:
    case COMMANDLINE_MODE_UNREGSERVER:
    case COMMANDLINE_MODE_NETDIAGS:
    case COMMANDLINE_MODE_CRASH:
    case COMMANDLINE_MODE_INSTALL:
    case COMMANDLINE_MODE_RECOVER:
    case COMMANDLINE_MODE_WEBPLUGIN:
    case COMMANDLINE_MODE_COMSERVER:
    case COMMANDLINE_MODE_REGISTER_PRODUCT:
    case COMMANDLINE_MODE_UNREGISTER_PRODUCT:
    default:
      ++metric_setup_process_wait_failed_other;
      break;
  }
}

// Returns the pids of all other GoogleUpdate.exe processes with the specified
// argument string. Checks processes for all users that it has privileges to
// access.
HRESULT GetPidsWithArgsForAllUsers(const CString& args,
                                   std::vector<uint32>* pids) {
  ASSERT1(pids);

  std::vector<CString> command_line;
  command_line.push_back(args);

  DWORD flags = EXCLUDE_CURRENT_PROCESS |
                INCLUDE_PROCESS_COMMAND_LINE_CONTAINING_STRING;
  HRESULT hr = Process::FindProcesses(flags,
                                      kGoopdateFileName,
                                      true,
                                      CString(),
                                      command_line,
                                      pids);
  if (FAILED(hr)) {
    SETUP_LOG(LE, (_T("[FindProcesses failed][0x%08x]"), hr));
    return hr;
  }

  return S_OK;
}

const TCHAR* const kUninstallEventDesc = _T("Google Update uninstall");

void WriteGoogleUpdateUninstallEvent(bool is_machine) {
  GoogleUpdateLogEvent uninstall_event(EVENTLOG_INFORMATION_TYPE,
                                       kUninstallEventId,
                                       is_machine);
  uninstall_event.set_event_desc(kUninstallEventDesc);
  uninstall_event.WriteEvent();
}

// Returns true if the mode can be determined and pid represents a "/c" process.
bool IsCoreProcess(uint32 pid) {
  CommandLineMode mode(COMMANDLINE_MODE_UNKNOWN);
  return SUCCEEDED(GetProcessModeFromPid(pid, &mode)) &&
         COMMANDLINE_MODE_CORE == mode;
}

}  // namespace

bool Setup::did_uninstall_ = false;

Setup::Setup(bool is_machine, const CommandLineArgs* args)
    : is_machine_(is_machine),
      mode_(MODE_UNKNOWN),
      args_(args),
      extra_code1_(S_OK),
      launched_offline_worker_(false) {
  SETUP_LOG(L2, (_T("[Setup::Setup]")));
}

Setup::~Setup() {
  SETUP_LOG(L2, (_T("[Setup::~Setup]")));
}

// Handles the elevation case, then calls DoInstall to do the real work if
// elevation is not required.
// If elevation is required, the method returns when the elevated instance
// exits because there is nothing more to do in this Omaha instance.
HRESULT Setup::Install(const CString& cmd_line) {
  SETUP_LOG(L2, (_T("[Setup::Install][%s]"), cmd_line));
  ASSERT1(!cmd_line.IsEmpty());

  mode_ = MODE_INSTALL;

  if (args_->is_oem_set) {
    HRESULT hr = SetOemInstallState();
    if (FAILED(hr)) {
      return hr;
    }
  } else if (ConfigManager::Instance()->IsWindowsInstalling()) {
    ASSERT(false, (_T("[In OEM Audit Mode but not doing OEM install]")));
    return GOOPDATE_E_NON_OEM_INSTALL_IN_AUDIT_MODE;
  }

  HRESULT hr = SetEulaRequiredState();
  if (FAILED(hr)) {
    return hr;
  }

  ++metric_setup_install_total;
  if (args_->is_install_elevated) {
    ++metric_setup_uac_succeeded;
  }

  // If we ever support NEEDS_ADMIN_PREFERS variants, handle them here.
  // We will also need to tell the installer how to install.

  if (IsElevationRequired()) {
    hr = ElevateAndWait(cmd_line);
    if (FAILED(hr)) {
      SETUP_LOG(LE, (_T("[Setup::ElevateAndWait failed][%s][0x%08x]"),
                    cmd_line, hr));
    }
    return hr;
  }

  if (vista_util::IsVistaOrLater() &&
      !is_machine_ &&
      vista_util::IsUserAdmin()) {
    ++metric_setup_user_app_admin;
  }

  hr = DoInstall();

  if (FAILED(hr)) {
    return hr;
  }

  ++metric_setup_install_succeeded;
  return S_OK;
}

HRESULT Setup::InstallSelfSilently() {
  SETUP_LOG(L2, (_T("[Setup::InstallSelfSilently]")));
  ASSERT1(!IsElevationRequired());
  ASSERT1(!args_->extra_args_str.IsEmpty());

  // TODO(omaha): add metrics.
  mode_ = MODE_SELF_INSTALL;

  HRESULT hr = SetEulaRequiredState();
  if (FAILED(hr)) {
    return hr;
  }

  hr = DoInstall();
  if (FAILED(hr)) {
    return hr;
  }

  // TODO(omaha): add metrics.
  return S_OK;
}

HRESULT Setup::UpdateSelfSilently() {
  SETUP_LOG(L2, (_T("[Setup::UpdateSelfSilently]")));
  ASSERT1(!IsElevationRequired());
  ASSERT1(args_->extra_args_str.IsEmpty());

  ++metric_setup_update_self_total;
  mode_ = MODE_SELF_UPDATE;

  HRESULT hr = DoInstall();
  if (FAILED(hr)) {
    PersistUpdateErrorInfo(is_machine_, hr, extra_code1_, GetVersionString());
    return hr;
  }

  ++metric_setup_update_self_succeeded;
  return S_OK;
}

HRESULT Setup::RepairSilently() {
  SETUP_LOG(L2, (_T("[Setup::RepairSilently]")));
  ASSERT1(!IsElevationRequired());
  ASSERT1(args_->extra_args_str.IsEmpty());

  mode_ = MODE_REPAIR;

  HRESULT hr = DoInstall();
  if (FAILED(hr)) {
    // Use the update failed ping for repairs too.
    PersistUpdateErrorInfo(is_machine_, hr, extra_code1_, GetVersionString());
    return hr;
  }

  return S_OK;
}

// Protects all setup-related operations with:
// * Setup Lock - Prevents other instances from installing Google Update for
// this machine/user at the same time.
// * Shutdown Event - Tells existing other instances and any instances that may
// start during Setup to exit.
// Setup-related operations do not include installation of the app.
// Also tries to acquire the locks for Omaha 1.0 and 1.1, which are lock10 and
// lock11, respectively.
// Until we secure the current setup lock (bug 1076207), we return an error if
// we are unable to acquire the legacy locks.
// Note: Our own lock is susceptible to DOS until we fix bug 1076207.
// Gets the legacy locks first because we wait a lot longer for them during
// updates and would not want to hold the setup lock all that time.
// Legacy Setup Locks are not released until the app is installed.
HRESULT Setup::DoInstall() {
  ASSERT1(MODE_UNKNOWN != mode_);
  ASSERT1(!IsElevationRequired());

  // Validate that key OS components are installed.
  if (!HasXmlParser()) {
    return GOOPDATE_E_RUNNING_INFERIOR_MSXML;
  }

  // Start the setup timer.
  metrics_timer_.reset(new HighresTimer);

  GLock lock10, lock11_user, lock11_machine, setup_lock;

  // Initialize all locks.
  if (!InitLegacySetupLocks(&lock10, &lock11_user, &lock11_machine)) {
    SETUP_LOG(L2, (_T("[Setup::InitLegacySetupLocks failed]")));
    return GOOPDATE_E_SETUP_LOCK_INIT_FAILED;
  }
  if (!InitSetupLock(is_machine_, &setup_lock)) {
    SETUP_LOG(L2, (_T("[Setup::InitSetupLock failed]")));
    extra_code1_ = kVersion12;
    return GOOPDATE_E_SETUP_LOCK_INIT_FAILED;
  }

  scoped_process handoff_process;

  // Attempt to acquire all locks. Acquire them in a new scope so they are
  // automatically released by ScopeGuards when we no longer need protection.
  {
    HighresTimer lock_metrics_timer;

    // Try to acquire the 1.0 lock to prevent 1.0 installers from running, but
    // do not fail if not acquired because this prevents simultaneous user and
    // machine installs because both used the same name. See bug 1145609.
    // For simplicity, hold this lock until the function exits.
    if (!lock10.Lock(kSetupLockWaitMs)) {
      OPT_LOG(LW, (_T("[Failed to acquire 1.0 setup lock]")));
    }

    if (!lock11_user.Lock(kSetupLockWaitMs)) {
      OPT_LOG(LE, (_T("[Failed to acquire 1.1 user setup lock]")));
      return HandleLockFailed(kVersion11);
    }
    ON_SCOPE_EXIT_OBJ(lock11_user, &GLock::Unlock);

    if (is_machine_) {
      if (!lock11_machine.Lock(kSetupLockWaitMs)) {
        OPT_LOG(LE, (_T("[Failed to acquire 1.1 machine setup lock]")));
        return HandleLockFailed(kVersion11MachineLock);
      }
    }
    ScopeGuard lock11_machine_guard = MakeObjGuard(lock11_machine,
                                                   &GLock::Unlock);
    if (!is_machine_) {
      lock11_machine_guard.Dismiss();
    }

    if (!setup_lock.Lock(kSetupLockWaitMs)) {
      OPT_LOG(LE, (_T("[Failed to acquire setup lock]")));
      return HandleLockFailed(kVersion12);
    }
    ON_SCOPE_EXIT_OBJ(setup_lock, &GLock::Unlock);

    metric_setup_lock_acquire_ms.AddSample(lock_metrics_timer.GetElapsedMs());
    SETUP_LOG(L1, (_T("[Setup Locks acquired]")));

    // Do the installation.
    HRESULT hr = DoProtectedInstall(address(handoff_process));
    if (FAILED(hr)) {
      SETUP_LOG(LE, (_T("[Setup::DoProtectedInstall failed][0x%08x]"), hr));
      SETUP_LOG(L1, (_T("[Releasing Setup Lock]")));
      return hr;
    }

    SETUP_LOG(L1, (_T("[Releasing Setup Lock]")));
  }  // End lock protection.

  if (handoff_process) {
    ASSERT1(MODE_INSTALL == mode_);

    // TODO(omaha): Why do we exit once the UI event is signaled? It is not
    // related to the locks like waiting for /ig and it complicates the code.
    HRESULT hr = WaitForHandoffWorker(get(handoff_process));

    if (FAILED(hr)) {
      if (!ShouldWaitForWorkerProcess()) {
        // Did not wait for the second process to exit. The error may not be the
        // exit code of the worker process. Report that the handoff failed.
        OPT_LOG(LE, (_T("[Wait for Google Update hand off failed][0x%08x]"),
                     hr));
        extra_code1_ = hr;
        return GOOPDATE_E_HANDOFF_FAILED;
      } else {
        return hr;
      }
    }

    ++metric_setup_handoff_only_succeeded;
  }

  return S_OK;
}

// Sets appropriate metrics and extra_code1_ value. It then tries to determine
// the scenario that caused this failure and returns an appropriate error.
// The detected processes may not actually be in conflict with this one, but are
// more than likely the cause of the lock failure.
HRESULT Setup::HandleLockFailed(int lock_version) {
  ++metric_setup_locks_failed;

  switch (lock_version) {
    case kVersion10:
      ASSERT1(false);
      extra_code1_ = kVersion10;
      break;
    case kVersion11:
      extra_code1_ = kVersion11;
      ++metric_setup_lock11_failed;
      break;
    case kVersion11MachineLock:
      extra_code1_ = kVersion11MachineLock;
      ++metric_setup_lock11_failed;
      break;
    case kVersion12:
      extra_code1_ = kVersion12;
      ++metric_setup_lock12_failed;
      break;
    default:
      ASSERT1(false);
      extra_code1_ = -1;
      break;
  }

  Pids matching_pids;
  CString switch_to_include;

  switch_to_include.Format(_T("/%s"), kCmdLineUpdate);
  HRESULT hr = GetPidsWithArgsForAllUsers(switch_to_include, &matching_pids);
  if (FAILED(hr)) {
    ASSERT1(false);
    return GOOPDATE_E_FAILED_TO_GET_LOCK;
  }
  if (!matching_pids.empty()) {
    return GOOPDATE_E_FAILED_TO_GET_LOCK_UPDATE_PROCESS_RUNNING;
  }

  switch_to_include.Format(_T("/%s"), kCmdLineInstall);
  hr = GetPidsWithArgsForAllUsers(switch_to_include, &matching_pids);
  if (FAILED(hr)) {
    ASSERT1(false);
    return GOOPDATE_E_FAILED_TO_GET_LOCK;
  }
  if (matching_pids.empty()) {
    return GOOPDATE_E_FAILED_TO_GET_LOCK;
  }

  // Another /install process was found. Determine if it has the same cmd line.
  const TCHAR* this_cmd_line = ::GetCommandLine();
  if (!this_cmd_line) {
    ASSERT1(false);
    return GOOPDATE_E_FAILED_TO_GET_LOCK;
  }
  const CString current_cmd_line(this_cmd_line);
  if (current_cmd_line.IsEmpty()) {
    ASSERT1(false);
    return GOOPDATE_E_FAILED_TO_GET_LOCK;
  }

  // Strip the directory path, which may vary, and executable name.
  int exe_index = current_cmd_line.Find(kGoopdateFileName);
  if (-1 == exe_index) {
    ASSERT(false, (_T("Unable to find %s in %s"),
                   kGoopdateFileName, current_cmd_line));
    return GOOPDATE_E_FAILED_TO_GET_LOCK;
  }
  int args_start = exe_index + _tcslen(kGoopdateFileName);
  // Support enclosed paths; increment past closing double quote.
  if (_T('"') == current_cmd_line.GetAt(args_start)) {
    ++args_start;
  }
  const int args_length = current_cmd_line.GetLength() - args_start;
  CString current_args = current_cmd_line.Right(args_length);
  current_args.Trim();

  for (size_t i = 0; i < matching_pids.size(); ++i) {
    CString matching_pid_cmd_line;
    if (FAILED(Process::GetCommandLine(matching_pids[i],
                                       &matching_pid_cmd_line))) {
      continue;
    }

    if (-1 != matching_pid_cmd_line.Find(current_args)) {
      // Assume that this is a match and not a subset.
      return GOOPDATE_E_FAILED_TO_GET_LOCK_MATCHING_INSTALL_PROCESS_RUNNING;
    }
  }

  return GOOPDATE_E_FAILED_TO_GET_LOCK_NONMATCHING_INSTALL_PROCESS_RUNNING;
}

// Assumes the necessary locks have been acquired.
HRESULT Setup::DoProtectedInstall(HANDLE* handoff_process) {
  SETUP_LOG(L2, (_T("[Setup::DoProtectedInstall]")));
  ASSERT1(handoff_process);
  ASSERT1(MODE_UNKNOWN != mode_);

  SetupFiles setup_files(is_machine_);

  HRESULT hr = setup_files.Init();
  if (FAILED(hr)) {
    SETUP_LOG(LE, (_T("[SetupFiles::Init failed][0x%08x]"), hr));
    return hr;
  }

  if (ShouldInstall(&setup_files)) {
    ++metric_setup_do_self_install_total;
    HRESULT hr = DoProtectedGoogleUpdateInstall(&setup_files);
    if (FAILED(hr)) {
      SETUP_LOG(LE, (_T("[DoProtectedGoogleUpdateInstall fail][0x%08x]"), hr));
      // Do not return until rolling back, releasing the events and restarting
      // the core.
    }

    if (FAILED(hr)) {
      RollBack(&setup_files);
    }

    // We need to hold the shutdown events until phase 2 is complete.
    // Phase 2 will release the current version's event. Here we release all
    // shutdown events, including the one that should have already been released
    // to be safe (i.e. in case phase 2 crashes).
    // This will happen before the Setup Lock is released, preventing any races.
    ReleaseShutdownEvents();

    if (FAILED(hr)) {
      // We may have shutdown an existing core. Start it again if possible.
      // First kill running cores to prevent the stated instance from exiting
      // because it cannot acquire the single program instance.
      OPT_LOG(L2, (_T("[Attempting to restart existing core]")));

      VERIFY1(SUCCEEDED(TerminateCoreProcesses()));

      HRESULT start_hr = StartCore();
      if (FAILED(start_hr)) {
        SETUP_LOG(LW, (_T("[StartCore failed][0x%08x]"), start_hr));
      }

      return hr;
    }

    ++metric_setup_do_self_install_succeeded;
    return S_OK;
  } else if (MODE_INSTALL == mode_) {
    // No setup was required. Launch the worker to install the app.
    // Since we are not doing any setup in the worker, return without waiting
    // so that we can release the Setup Lock.
    ++metric_setup_handoff_only_total;

    hr = LaunchInstalledWorker(false,   // Do not run setup phase 2.
                               handoff_process);
    if (SUCCEEDED(hr)) {
      metric_setup_handoff_ms.AddSample(metrics_timer_->GetElapsedMs());
    } else {
      OPT_LOG(LE, (_T("[Failed to launch installed instance][0x%08x]"), hr));
      // TODO(omaha): Consider checking for GoogleUpdate.exe in version
      // directory in file not found case and copying it to shell location then
      // relaunching.
    }

    // Start the core in case one is not already running. If one is already
    // running, this one will exit quickly.
    HRESULT start_hr = StartCore();
    if (FAILED(start_hr)) {
      SETUP_LOG(LW, (_T("[StartCore failed][0x%08x]"), start_hr));
    }

    return hr;
  } else {
    // Do not launch the worker because there is no app to install.
    OPT_LOG(L1, (_T("[Not installing Google Update or an app]")));

    // Start the core in case one is not already running. If one is already
    // running, this one will exit quickly.
    return StartCore();
  }
}

// Assumes that the shell is the correct version for the existing Omaha version.
bool Setup::ShouldInstall(SetupFiles* setup_files) {
  SETUP_LOG(L2, (_T("[Setup::ShouldInstall]")));
  ASSERT1(setup_files);

  ++metric_setup_should_install_total;

  ULONGLONG my_version = GetVersion();

  const ConfigManager* cm = ConfigManager::Instance();
  CString existing_version;
  HRESULT hr = RegKey::GetValue(cm->registry_clients_goopdate(is_machine_),
                                kRegValueProductVersion,
                                &existing_version);
  if (FAILED(hr)) {
    OPT_LOG(L2, (_T("[fresh install]")));
    ++metric_setup_should_install_true_fresh_install;
    return true;
  }

  OPT_LOG(L2, (_T("[Existing version: %s][Running version: %s]"),
               existing_version, GetVersionString()));
  UpdateCoreNotRunningMetric(existing_version);

  // If running from the official install directory for this type of install
  // (user/machine), it is most likely a OneClick install. Do not install self.
  if (goopdate_utils::IsRunningFromOfficialGoopdateDir(is_machine_)) {
    ++metric_setup_should_install_false_oc;
    return false;
  }

  if (MODE_INSTALL == mode_) {
    ++metric_setup_subsequent_install_total;
  }

  bool should_install(false);

  ULONGLONG cv = VersionFromString(existing_version);
  if (cv > my_version) {
    SETUP_LOG(L2, (_T("[not installing, newer version exists]")));
    ++metric_setup_should_install_false_older;
    should_install = false;
  } else if (cv < my_version) {
    SETUP_LOG(L2, (_T("[installing with local build]")));
    ++metric_setup_should_install_true_newer;
    should_install = true;
  } else {
    // Same version.
    should_install = ShouldOverinstallSameVersion(setup_files);
    if (should_install) {
      ++metric_setup_should_install_true_same;
    } else {
      ++metric_setup_should_install_false_same;
    }
  }

  if (MODE_INSTALL == mode_ && should_install) {
    ++metric_setup_subsequent_install_should_install_true;
  }

  OPT_LOG(L1, (_T("[machine = %d][existing version = %s][should_install = %d]"),
               is_machine_, existing_version, should_install));

  return should_install;
}

// Checks the following:
//  * OverInstall override.
//  * The "installed" version in the registry equals this version.
//    If not, this version was not fully installed even though "pv" says it is.
//  * Files are properly installed.
bool Setup::ShouldOverinstallSameVersion(SetupFiles* setup_files) {
  SETUP_LOG(L2, (_T("[Setup::ShouldOverinstallSameVersion]")));
  ASSERT1(setup_files);

  const ConfigManager* cm = ConfigManager::Instance();

  bool should_over_install = cm->CanOverInstall();
  SETUP_LOG(L1, (_T("[should over install = %d]"), should_over_install));
  if (should_over_install) {
    SETUP_LOG(L2, (_T("[overinstalling with local build]")));
    return true;
  }

  CString installed_version;
  HRESULT hr = RegKey::GetValue(cm->registry_update(is_machine_),
                                kRegValueInstalledVersion,
                                &installed_version);
  if (FAILED(hr) || GetVersionString() != installed_version) {
    SETUP_LOG(L1, (_T("[installed version missing or did not match][%s]"),
                   installed_version));
    ++metric_setup_should_install_true_same_completion_missing;
    return true;
  }

  if (setup_files->ShouldOverinstallSameVersion()) {
    SETUP_LOG(L1, (_T("[files need over-install]")));
    return true;
  }

  // TODO(omaha): Verify the current installation is complete and correct.
  // For example, in Omaha 1, we would always set the run key to the version
  // being installed. Now that code is in SetupGoogleUpdate, and it does not get
  // called.

  return false;
}


void Setup::UpdateCoreNotRunningMetric(const CString& existing_version) {
  if (!goopdate_utils::IsGoogleUpdate2OrLater(existing_version)) {
    return;
  }

  Pids found_core_pids;
  HRESULT hr = FindCoreProcesses(&found_core_pids);
  if (FAILED(hr)) {
    ASSERT(false, (_T("[FindCoreProcesses failed][0x%08x]"), hr));
    return;
  }

  if (found_core_pids.empty()) {
    ++metric_setup_installed_core_not_running;
  }
}

HRESULT Setup::DoProtectedGoogleUpdateInstall(SetupFiles* setup_files) {
  ASSERT1(setup_files);
  SETUP_LOG(L2, (_T("[Setup::DoProtectedGoogleUpdateInstall]")));

  HRESULT hr = StopGoogleUpdateAndWait();
  if (FAILED(hr)) {
    SETUP_LOG(LE, (_T("[StopGoogleUpdateAndWait failed][0x%08x]"), hr));
    if (E_ACCESSDENIED == hr) {
      return GOOPDATE_E_ACCESSDENIED_STOP_PROCESSES;
    }
    return hr;
  }

  VERIFY1(SUCCEEDED(ResetMetrics(is_machine_)));

  hr = setup_files->Install();
  if (FAILED(hr)) {
    SETUP_LOG(LE, (_T("[SetupFiles::Install failed][0x%08x]"), hr));
    extra_code1_ = setup_files->extra_code1();
    return hr;
  }
  ASSERT1(!setup_files->extra_code1());

  scoped_event setup_complete_event;
  VERIFY1(SUCCEEDED(goopdate_utils::CreateUniqueEventInEnvironment(
      kSetupCompleteEventEnvironmentVariableName,
      is_machine_,
      address(setup_complete_event))));
  // Continue on failure. We just will not be able to release the lock early.

  hr = goopdate_utils::GetVerFromRegistry(is_machine_,
                                          kGoogleUpdateAppId,
                                          &saved_version_);
  if (FAILED(hr)) {
    SETUP_LOG(L3, (_T("[GetVerFromRegistry failed][0x%08x]"), hr));
    // Continue as this is expected for first installs.
  }

  // Set the version so the constant shell will know which version to use.
  hr = RegKey::SetValue(
      ConfigManager::Instance()->registry_clients_goopdate(is_machine_),
      kRegValueProductVersion,
      GetVersionString());
  if (FAILED(hr)) {
    OPT_LOG(LE, (_T("[Failed to set version in registry][0x%08x]"), hr));
    if (E_ACCESSDENIED == hr) {
      return GOOPDATE_E_ACCESSDENIED_SETUP_REG_ACCESS;
    }
    return hr;
  }

  // Start the worker to run setup phase 2 and install the app.
  scoped_process worker_process;
  hr = LaunchInstalledWorker(true,  // Run setup phase 2.
                             address(worker_process));
  if (FAILED(hr)) {
    OPT_LOG(LE, (_T("[Failed to launch installed instance][0x%08x]"), hr));
    return hr;
  }

  // Wait for setup to complete to ensure the Setup Lock is held though the
  // end of setup.
  OPT_LOG(L1, (_T("[Waiting for setup to complete]")));
  uint32 exit_code(0);
  hr = WaitForProcessExitOrEvent(get(worker_process),
                                 get(setup_complete_event),
                                 &exit_code);
  if (FAILED(hr)) {
    OPT_LOG(LE, (_T("[Failed waiting for setup to complete][0x%08x]"), hr));
    return hr;
  }
  if (exit_code) {
    OPT_LOG(LE, (_T("[Setup exited with error][0x%08x]"), exit_code));
    ASSERT1(FAILED(exit_code));

    return exit_code;
  }

  metric_setup_install_google_update_total_ms.AddSample(
      metrics_timer_->GetElapsedMs());
  return S_OK;
}

void Setup::RollBack(SetupFiles* setup_files) {
  OPT_LOG(L1, (_T("[Roll back]")));
  ASSERT1(setup_files);

  // Restore the saved version.
  if (!saved_version_.IsEmpty()) {
    SETUP_LOG(L1, (_T("[Rolling back version to %s]"), saved_version_));
    ++metric_setup_rollback_version;

    VERIFY1(SUCCEEDED(RegKey::SetValue(
        ConfigManager::Instance()->registry_clients_goopdate(is_machine_),
        kRegValueProductVersion,
        saved_version_)));
  }

  VERIFY1(SUCCEEDED(setup_files->RollBack()));
}

// Assumes the caller is ensuring this is the only running instance of setup.
// The original process holds the lock while it waits for this one to complete.
HRESULT Setup::SetupGoogleUpdate() {
  SETUP_LOG(L2, (_T("[Setup::SetupGoogleUpdate]")));
  ASSERT1(!IsElevationRequired());
  mode_ = MODE_PHASE2;

  HighresTimer phase2_metrics_timer;

  omaha::SetupGoogleUpdate setup_google_update(is_machine_, args_);

  HRESULT hr = setup_google_update.FinishInstall();
  if (FAILED(hr)) {
    SETUP_LOG(LE, (_T("[FinishInstall failed][0x%08x]"), hr));

    // We shutdown the existing core. Start a core - which one depends on where
    // we failed - again if possible.
    // There is a chance we have messed up the launch mechanisms and the core
    // will not even start on reboot.
    VERIFY1(SUCCEEDED(StartCore()));

    return hr;
  }

  // Release the shutdown event, so we can start the core and do not interfere
  // with other app installs that may be waiting on the Setup Lock.
  NamedObjectAttributes event_attr;
  GetShutdownEventAttributes(is_machine_, &event_attr);
  scoped_event shutdown_event(::OpenEvent(EVENT_MODIFY_STATE,
                                          false,
                                          event_attr.name));
  if (shutdown_event) {
    VERIFY1(::ResetEvent(get(shutdown_event)));
  } else {
    SETUP_LOG(LW, (_T("[::OpenEvent failed][%s][%u]"),
                  event_attr.name, ::GetLastError()));
  }

  if (is_machine_) {
    hr = StartMachineCoreProcess();
    if (FAILED(hr)) {
      SETUP_LOG(LE, (_T("[StartMachineCoreProcess failed][0x%08x]"), hr));
      return hr;
    }
  } else {
    CString core_cmd_line(setup_google_update.BuildCoreProcessCommandLine());
    hr = StartUserCoreProcess(core_cmd_line);
    if (FAILED(hr)) {
      SETUP_LOG(LE, (_T("[StartUserCoreProcess failed][0x%08x]"), hr));
      return hr;
    }
  }

  // Setup is now complete.
  SetSetupCompleteEvent();

  metric_setup_phase2_ms.AddSample(phase2_metrics_timer.GetElapsedMs());
  return S_OK;
}

// Stops all user/machine instances including the service, unregisters using
// SetupGoogleUpdate, then deletes  the files using SetupFiles.
// Does not wait for the processes to exit, except the service.
// Protects all operations with the setup lock. If MSI is found busy, Omaha
// won't uninstall.
HRESULT Setup::Uninstall() {
  OPT_LOG(L1, (_T("[Setup::Uninstall]")));
  ASSERT1(!IsElevationRequired());
  mode_ = MODE_UNINSTALL;

  // Try to get the global setup lock; if the lock is taken, do not block
  // waiting to uninstall; just return.
  GLock setup_lock;
  VERIFY1(InitSetupLock(is_machine_, &setup_lock));
  if (!setup_lock.Lock(0)) {
    OPT_LOG(LE, (_T("[Failed to acquire setup lock]")));
    return E_FAIL;
  }

  return DoProtectedUninstall();
}

// Aggregates metrics regardless of whether uninstall is allowed.
// Foces reporting of the metrics if uninstall is allowed.
// Assumes that the current process holds the Setup Lock.
HRESULT Setup::DoProtectedUninstall() {
  const bool can_uninstall = CanUninstallGoogleUpdate();
  OPT_LOG(L1, (_T("[CanUninstallGoogleUpdate returned %d]"), can_uninstall));
  ASSERT1(!IsElevationRequired());

  if (can_uninstall) {
    HRESULT hr = AggregateAndReportMetrics(is_machine_, true);
    VERIFY1(SUCCEEDED(hr) || GOOPDATE_E_CANNOT_USE_NETWORK == hr);
  } else {
    VERIFY1(SUCCEEDED(AggregateMetrics(is_machine_)));
  }

  if (!can_uninstall) {
    return GOOPDATE_E_CANT_UNINSTALL;
  }

  if (is_machine_) {
    HRESULT hr = SetupService::StopService();
    if (FAILED(hr)) {
      SETUP_LOG(LW, (_T("[SetupService::StopService failed][0x%08x]"), hr));
      ASSERT1(HRESULT_FROM_WIN32(ERROR_SERVICE_DOES_NOT_EXIST) == hr);
    }
  }
  VERIFY1(SUCCEEDED(SignalShutdownEvent()));

  // TODO(omaha): Consider waiting for the Omaha processes to exit. This would
  // need to exclude any processes, such as /ua /uninstall, that can uninstall.

  // Write the event in the event log before uninstalling the program since
  // the event contains version and language information, which are removed
  // during the uninstall.
  WriteGoogleUpdateUninstallEvent(is_machine_);

  omaha::SetupGoogleUpdate setup_google_update(is_machine_, args_);
  setup_google_update.Uninstall();

  SetupFiles setup_files(is_machine_);
  setup_files.Uninstall();

  OPT_LOG(L1, (_T("[Uninstall complete]")));
  did_uninstall_ = true;
  return S_OK;
}

// Should only be called after the point where Uninstall would have been called.
// Works correctly in the case where the Setup Lock is not held but an app is
// being installed because it does not check the number of registered apps.
// Either Omaha is installed or has been cleaned up.
// Installed means Clients, ClientState, etc. sub keys exist.
// Cleaned up may mean the Update key does not exist or some values, such as
// mid and uid exist, but there are no subkeys.
// The Update key should never exist without any values.
// Does not take the Setup Lock because it is just a dbg check. It is possible
// for the value of did_uninstall_ to be changed in another thread or for
// another process to install or uninstall Google Update while this is running.
void Setup::CheckInstallStateConsistency(bool is_machine) {
  UNREFERENCED_PARAMETER(is_machine);
#if DEBUG
  CString key_name = ConfigManager::Instance()->registry_update(is_machine);
  if (!RegKey::HasKey(key_name)) {
    // Either this instance called uninstall or it is the non-elevated machine
    // instance on Vista and later. Both cannot be true in the same instance.
    ASSERT1(did_uninstall_ != (is_machine && !vista_util::IsUserAdmin()));
    return;
  }

  RegKey update_key;
  ASSERT1(SUCCEEDED(update_key.Open(key_name, KEY_READ)));

  ASSERT1(0 != update_key.GetValueCount());

  if (did_uninstall_) {
    ASSERT1(0 == update_key.GetSubkeyCount());
    ASSERT1(!update_key.HasValue(kRegValueInstalledVersion));
    ASSERT1(!update_key.HasValue(kRegValueInstalledPath));
  } else {
    ASSERT1(update_key.HasSubkey(_T("Clients")));
    ASSERT1(update_key.HasSubkey(_T("ClientState")));
    ASSERT1(update_key.HasSubkey(_T("network")));
    ASSERT1(update_key.HasValue(kRegValueInstalledVersion));
    ASSERT1(update_key.HasValue(kRegValueInstalledPath));

    CString installed_version;
    ASSERT1(SUCCEEDED(update_key.GetValue(kRegValueInstalledVersion,
                                          &installed_version)));
    const CString state_key_name =
      ConfigManager::Instance()->registry_client_state_goopdate(is_machine);
    CString pv;
    ASSERT1(SUCCEEDED(RegKey::GetValue(state_key_name,
                                       kRegValueProductVersion,
                                       &pv)));
    ASSERT1(installed_version == pv);
  }
#endif
}

// Stops both legacy and current instances.
// Holds the shutdown events so that other instances do not start running.
// The caller is responsible for releasing the events.
// Because this waiting occurs before a UI is generated, we do not want to wait
// too long.
HRESULT Setup::StopGoogleUpdate() {
  OPT_LOG(L1, (_T("[Stopping other instances]")));

  // In the machine install case, we should first stop the service.
  // This means that we should not get the service as a process to wait
  // on when the machine goopdate tries to enumerate the processes.
  if (is_machine_) {
    OPT_LOG(L1, (_T("[Stopping service]")));
    HRESULT hr = SetupService::StopService();
    if (FAILED(hr)) {
      OPT_LOG(LE, (_T("[StopService failed][0x%08x]"), hr));
    }
  }

  HRESULT hr = SignalLegacyShutdownEvents();
  if (FAILED(hr)) {
    SETUP_LOG(LE, (_T("[SignalLegacyShutdownEvents failed][0x%08x]"), hr));
    return hr;
  }

  hr = SignalShutdownEvent();
  if (FAILED(hr)) {
    SETUP_LOG(LE, (_T("[SignalShutdownEvent failed][0x%08x]"), hr));
    return hr;
  }

  return S_OK;
}

HRESULT Setup::StopGoogleUpdateAndWait() {
  HRESULT hr = StopGoogleUpdate();
  if (FAILED(hr)) {
    SETUP_LOG(LE, (_T("[StopGoogleUpdate failed][0x%08x]"), hr));
    return hr;
  }

  Pids pids;
  hr = GetPidsToWaitFor(&pids);
  if (FAILED(hr)) {
    SETUP_LOG(LEVEL_ERROR, (_T("[GetPidsToWaitFor failed][0x%08x]"), hr));
    return hr;
  }

  hr = WaitForOtherInstancesToExit(pids);
  if (FAILED(hr)) {
    SETUP_LOG(LE, (_T("[WaitForOtherInstancesToExit failed][0x%08x]"), hr));
    return hr;
  }

  return S_OK;
}

// Signals >= 1.2.x processes to exit.
HRESULT Setup::SignalShutdownEvent() {
  SETUP_LOG(L1, (_T("[Setup::SignalShutdownEvent]")));
  NamedObjectAttributes event_attr;
  GetShutdownEventAttributes(is_machine_, &event_attr);

  if (!shutdown_event_) {
    HRESULT hr = goopdate_utils::CreateEvent(&event_attr,
                                             address(shutdown_event_));
    if (FAILED(hr)) {
      SETUP_LOG(LE, (_T("[CreateEvent current failed][0x%08x]"), hr));
      return hr;
    }
  }

  VERIFY1(::SetEvent(get(shutdown_event_)));

  return S_OK;
}

// Signals the quiet mode events for 1.0.x (pre-i18n) and 1.1.x (i18n)
// processes, causing them to exit.
HRESULT Setup::SignalLegacyShutdownEvents() {
  SETUP_LOG(L1, (_T("[Setup::SignalLegacyShutdownEvents]")));

  // Signal 1.0.x (pre-i18n) processes.
  CString sid;
  HRESULT hr = GetAppropriateSid(&sid);
  if (FAILED(hr)) {
    return hr;
  }

  CString legacy_1_0_shutdown_event_name;
  legacy_1_0_shutdown_event_name.Format(kEventLegacyQuietModeName, sid);

  hr = CreateLegacyEvent(legacy_1_0_shutdown_event_name,
                         address(legacy_1_0_shutdown_event_));
  if (FAILED(hr)) {
    SETUP_LOG(LE, (_T("[CreateLegacyEvent legacy quiet mode failed][0x%08x]"),
                  hr));
    return hr;
  }


  // Signal 1.1.x (i18n) processes.
  // 1.1 used the same event GUID in a different way.
  CString legacy_1_1_shutdown_event_name(is_machine_ ? kOmaha11GlobalPrefix :
                                                       kOmaha11LocalPrefix);
  legacy_1_1_shutdown_event_name.Append(kShutdownEvent);

  hr = CreateLegacyEvent(legacy_1_1_shutdown_event_name,
                         address(legacy_1_1_shutdown_event_));
  if (FAILED(hr)) {
    SETUP_LOG(LE, (_T("[CreateLegacyEvent quiet mode failed][0x%08x]"), hr));
    return hr;
  }

  VERIFY1(::SetEvent(get(legacy_1_0_shutdown_event_)));
  VERIFY1(::SetEvent(get(legacy_1_1_shutdown_event_)));

  return S_OK;
}

// The caller is responsible for reseting the event and closing the handle.
HRESULT Setup::CreateLegacyEvent(const CString& event_name,
                                 HANDLE* event_handle) const {
  ASSERT1(event_handle);
  ASSERT1(!event_name.IsEmpty());
  CSecurityAttributes sa;
  if (is_machine_) {
    // Grant access to administrators and system. This allows an admin
    // instance to open and modify the event even if the system created it.
    GetAdminDaclSecurityAttributes(&sa, GENERIC_ALL);
  }
  *event_handle = ::CreateEvent(&sa,
                                true,   // manual reset
                                false,  // not signaled
                                event_name);

  if (!*event_handle) {
    DWORD error = ::GetLastError();
    SETUP_LOG(LEVEL_ERROR, (_T("[::CreateEvent failed][%u]"), error));
    return HRESULT_FROM_WIN32(error);
  }

  return S_OK;
}

void Setup::ReleaseShutdownEvents() {
  VERIFY1(::ResetEvent(get(shutdown_event_)));
  reset(shutdown_event_);
  VERIFY1(::ResetEvent(get(legacy_1_0_shutdown_event_)));
  reset(legacy_1_0_shutdown_event_);
  VERIFY1(::ResetEvent(get(legacy_1_1_shutdown_event_)));
  reset(legacy_1_1_shutdown_event_);
}

void Setup::SetSetupCompleteEvent() const {
  scoped_event complete_event;
  HRESULT hr = goopdate_utils::OpenUniqueEventFromEnvironment(
      kSetupCompleteEventEnvironmentVariableName,
      is_machine_,
      address(complete_event));

  if (FAILED(hr)) {
    // We just will not be able to release the lock early.
    return;
  }

  ASSERT1(complete_event);
  VERIFY1(::SetEvent(get(complete_event)));
}

// Because this waiting can occur before a UI is generated, we do not want to
// wait too long.
// If a process fails to stop, its mode is stored in extra_code1_.
// Does not return until all opened handles have been closed.
// TODO(omaha): Add a parameter to specify the amount of time to wait to this
// method and StopGoogleUpdateAndWait after we unify Setup and always have a UI.
HRESULT Setup::WaitForOtherInstancesToExit(const Pids& pids) {
  OPT_LOG(L1, (_T("[Waiting for other instances to exit]")));

  // Wait for all the processes to exit.
  std::vector<HANDLE> handles;
  for (size_t i = 0; i < pids.size(); ++i) {
    SETUP_LOG(L2, (_T("[Waiting for process][%u]"), pids[i]));

    DWORD desired_access =
        PROCESS_QUERY_INFORMATION | SYNCHRONIZE | PROCESS_TERMINATE;
    scoped_handle handle(::OpenProcess(desired_access,
                                       FALSE,
                                       pids[i]));
    if (!handle) {
      HRESULT hr = HRESULTFromLastError();
      SETUP_LOG(LE, (_T("[::OpenProcess failed][%u][0x%08x]"), pids[i], hr));
      continue;
    }

    handles.push_back(release(handle));
  }

  HRESULT hr = S_OK;
  if (!handles.empty()) {
    SETUP_LOG(L2, (_T("[Calling ::WaitForMultipleObjects]")));

    // In the /install case, there is no UI so we cannot wait too long.
    // In other cases, we are silent or some other app is displaying a UI.
    const int shutdown_wait_ms = IsInteractiveInstall() ?
                                 kSetupShutdownWaitMsInteractiveNoUi :
                                 kSetupShutdownWaitMsSilent;
    HighresTimer metrics_timer;
    DWORD res = ::WaitForMultipleObjects(handles.size(),
                                         &handles.front(),
                                         true,  // wait for all
                                         shutdown_wait_ms);
    metric_setup_process_wait_ms.AddSample(metrics_timer.GetElapsedMs());

    SETUP_LOG(L2, (_T("[::WaitForMultipleObjects returned]")));
    ASSERT1(WAIT_OBJECT_0 == res || WAIT_TIMEOUT == res);
    if (WAIT_FAILED == res) {
      DWORD error = ::GetLastError();
      SETUP_LOG(LE, (_T("[::WaitForMultipleObjects failed][%u]"), error));
      hr = HRESULT_FROM_WIN32(error);
    } else if (WAIT_OBJECT_0 != res) {
      OPT_LOG(LEVEL_ERROR, (_T("[Other GoogleUpdate.exe instances failed to ")
                            _T("shutdown in time][%u]"), res));

      extra_code1_ = COMMANDLINE_MODE_UNKNOWN;

      SETUP_LOG(L2, (_T("[Listing processes that did not exit. This may be ")
                    _T("incomplete if processes exited after ")
                    _T("WaitForMultipleObjects returned.]")));
      for (size_t i = 0; i < handles.size(); ++i) {
        if (WAIT_TIMEOUT == ::WaitForSingleObject(handles[i], 0)) {
          uint32 pid = Process::GetProcessIdFromHandle(handles[i]);
          if (!pid) {
            SETUP_LOG(LW, (_T(" [Process::GetProcessIdFromHandle failed][%u]"),
                          ::GetLastError()));
            SETUP_LOG(L2, (_T(" [Process did not exit][unknown]")));
            continue;
          }
          SETUP_LOG(L2, (_T(" [Process did not exit][%u]"), pid));

          CommandLineMode mode(COMMANDLINE_MODE_UNKNOWN);
          if (SUCCEEDED(GetProcessModeFromPid(pid, &mode))) {
            extra_code1_ = mode;
          }
          IncrementProcessWaitFailCount(mode);
        }
      }

      ++metric_setup_process_wait_failed;
      hr = GOOPDATE_E_INSTANCES_RUNNING;
    }
  }
  if (SUCCEEDED(hr)) {
    SETUP_LOG(L3, (_T("[Wait for all processes to exit succeeded]")));
  }

  // Close the handles.
  for (size_t i = 0; i < handles.size(); ++i) {
    if (!::CloseHandle(handles[i])) {
      HRESULT hr = HRESULTFromLastError();
      SETUP_LOG(LEVEL_WARNING, (_T("[CloseHandle failed][0x%08x]"), hr));
    }
  }

  return hr;
}

// Wait for all instances of Omaha running as the current user - or SYSTEM for
// machine installs - except "/install" instances, which should be blocked by
// the Setup Lock, which we are holding.
// For machine installs, also wait for instances running with ("/handoff" OR
// "/ig") AND "needsadmin=True" running as any user. These instances are
// installing a machine app as a non-SYSTEM user and may also cause conflicts.
HRESULT Setup::GetPidsToWaitFor(Pids* pids) const {
  ASSERT1(pids);

  HRESULT hr = GetPidsToWaitForUsingCommandLine(pids);
  if (FAILED(hr)) {
    return hr;
  }
  SETUP_LOG(L3, (_T("[found %d processes using cmd line]"), pids->size()));

  // Remove any copies of the current PID from the list. This can happen when
  // doing self-updates.
  const uint32 current_pid = ::GetCurrentProcessId();
  Pids::iterator it = std::remove(pids->begin(), pids->end(), current_pid);
  if (pids->end() != it) {
    SETUP_LOG(L2, (_T("[removing current PID from list of PIDs]")));
  }
  pids->erase(it, pids->end());

  SETUP_LOG(L3, (_T("[found %d total processes to wait for]"), pids->size()));

  return S_OK;
}

// Finds legacy processes to wait for based on the command line.
HRESULT Setup::GetPidsToWaitForUsingCommandLine(Pids* pids) const {
  ASSERT1(pids);

  CString user_sid;
  HRESULT hr = GetAppropriateSid(&user_sid);
  if (FAILED(hr)) {
    return hr;
  }

  // Get all processes running as the current user/SYSTEM except those with
  // * "/install" - must be excluded because may be waiting for the Setup Lock.
  // * "/registerproduct" - same as for /install.
  // * "/installelevated" OR "/ui" - legacy switches used only for machine
  //   installs but ran as user and could cause false positives. For machine
  //   installs, we look for these running as any user in a separate search.
  std::vector<CString> command_lines;
  CString switch_to_exclude;
  switch_to_exclude.Format(_T("/%s"), kCmdLineInstall);
  command_lines.push_back(switch_to_exclude);
  switch_to_exclude.Format(_T("/%s"), kCmdLineRegisterProduct);
  command_lines.push_back(switch_to_exclude);
  switch_to_exclude.Format(_T("/%s"), kCmdLineLegacyVistaInstall);
  command_lines.push_back(switch_to_exclude);
  switch_to_exclude.Format(_T("/%s"), kCmdLineLegacyUi);
  command_lines.push_back(switch_to_exclude);

  DWORD flags = INCLUDE_ONLY_PROCESS_OWNED_BY_USER |
                EXCLUDE_CURRENT_PROCESS |
                EXCLUDE_PROCESS_COMMAND_LINE_CONTAINING_STRING;
  Pids found_pids;
  hr = Process::FindProcesses(flags,
                              kGoopdateFileName,
                              true,
                              user_sid,
                              command_lines,
                              &found_pids);
  if (FAILED(hr)) {
    SETUP_LOG(LE, (_T("[FindProcesses failed][0x%08x]"), hr));
    return hr;
  }
  for (size_t i = 0; i < found_pids.size(); ++i) {
    pids->push_back(found_pids[i]);
  }

  // Get all processes running as any user with:
  // * "/handoff" or "/ig" and"needsadmin=True"
  // * "-Embedding" and running from machine install location.
  std::vector<uint32> machine_install_worker_pids;
  hr = goopdate_utils::GetInstallWorkerProcesses(true,
                                                 &machine_install_worker_pids);
  if (FAILED(hr)) {
    SETUP_LOG(LE, (_T("[GetInstallWorkerProcesses failed][0x%08x]"), hr));
    return hr;
  }

  if (is_machine_) {
    // Add all machine install worker pids to the list of pids to wait for.
    for (size_t i = 0; i < machine_install_worker_pids.size(); ++i) {
      pids->push_back(machine_install_worker_pids[i]);
    }
  } else {
    // Remove machine install worker pids from the list of pids to wait for.
    for (size_t i = 0; i < machine_install_worker_pids.size(); ++i) {
      std::vector<uint32>::iterator iter = find(pids->begin(),
                                                pids->end(),
                                                machine_install_worker_pids[i]);
      if (pids->end() != iter) {
        pids->erase(iter);
      }
    }
  }

  if (is_machine_) {
    // Find legacy machine install processes that run as user. Check all users.
    std::vector<CString> legacy_machine_command_lines;
    CString switch_to_include;
    switch_to_include.Format(_T("/%s"), kCmdLineLegacyVistaInstall);
    command_lines.push_back(switch_to_include);
    switch_to_include.Format(_T("/%s"), kCmdLineLegacyUi);
    command_lines.push_back(switch_to_include);

    DWORD flags = EXCLUDE_CURRENT_PROCESS |
                  INCLUDE_PROCESS_COMMAND_LINE_CONTAINING_STRING;
    Pids found_legacy_machine_pids;
    hr = Process::FindProcesses(flags,
                                kGoopdateFileName,
                                true,
                                user_sid,
                                legacy_machine_command_lines,
                                &found_legacy_machine_pids);
    if (FAILED(hr)) {
      SETUP_LOG(LE, (_T("[FindProcesses failed][0x%08x]"), hr));
      return hr;
    }
    for (size_t i = 0; i < found_legacy_machine_pids.size(); ++i) {
      // Because we excluded these processes in the first search, we should not
      // get any duplicates.
      ASSERT1(found_pids.end() ==
              std::find(found_pids.begin(),
                        found_pids.end(),
                        found_legacy_machine_pids[i]));
      pids->push_back(found_legacy_machine_pids[i]);
    }
  }

  return S_OK;
}

// On Windows Vista, an admin must be elevated in order to install a machine app
// without elevating. On Vista, IsUserAdmin returns false unless the user is
// elevated.
bool Setup::IsElevationRequired() const {
  return is_machine_ && !vista_util::IsUserAdmin();
}

// The behavior depends on the OS:
//  1. OS >= Vista : Try to elevate - causes a UAC dialog.
//  2. OS < Vista  : Fail with a message box.
// We should be here only in case of initial machine installs when the user is
// not an elevated admin.
HRESULT Setup::ElevateAndWait(const CString& cmd_line) {
  OPT_LOG(L1, (_T("[Elevating][%s]"), cmd_line));
  ASSERT1(is_machine_);
  ASSERT1(!vista_util::IsUserAdmin());
  ASSERT1(MODE_INSTALL == mode_);

  if (!IsInteractiveInstall()) {
    return GOOPDATE_E_SILENT_INSTALL_NEEDS_ELEVATION;
  }

  if (args_->is_install_elevated) {
    // This can happen if UAC is disabled. See http://b/1187784.
    SETUP_LOG(LE, (_T("[Install elevated process requires elevation]")));
    return GOOPDATE_E_INSTALL_ELEVATED_PROCESS_NEEDS_ELEVATION;
  }

  if (!vista_util::IsVistaOrLater()) {
    // TODO(omaha): We could consider to ask for credentials here.
    // This TODO existed in Omaha 1. How would we even do this?
    SETUP_LOG(LE, (_T("[Non Admin trying to install admin app]")));
    ++metric_setup_machine_app_non_admin;
    return GOOPDATE_E_NONADMIN_INSTALL_ADMIN_APP;
  }

  CString cmd_line_elevated(GetCmdLineTail(cmd_line));
  cmd_line_elevated.AppendFormat(_T(" /%s"), kCmdLineInstallElevated);

  HRESULT hr = goopdate_utils::StartElevatedSelfWithArgsAndWait(
      cmd_line_elevated);
  if (FAILED(hr)) {
    OPT_LOG(LE, (_T("[Starting elevated GoogleUpdate.exe failed][%s][0x%08x]"),
                 cmd_line, hr));

    extra_code1_ = hr;
    if (vista_util::IsUserNonElevatedAdmin()) {
      return GOOPDATE_E_ELEVATION_FAILED_ADMIN;
    } else {
      return GOOPDATE_E_ELEVATION_FAILED_NON_ADMIN;
    }
  }

  // TODO(omaha): we might have to look at the exit_code from above.
  // Do not know what we should do in case of an error.

  return S_OK;
}

HRESULT Setup::CopyOfflineFilesForGuid(const CString& app_guid,
                                       const CString& offline_dir) {
  SETUP_LOG(L3, (_T("[Setup::CopyOfflineFilesForGuid][%s][%s]"),
                 app_guid, offline_dir));

  CPath setup_temp_dir(app_util::GetCurrentModuleDirectory());

  // Copy offline manifest into "Google\Update\Offline\{guid}.gup".
  CString manifest_filename = app_guid + _T(".gup");
  CString source_manifest_path = ConcatenatePath(setup_temp_dir,
                                                 manifest_filename);
  if (!File::Exists(source_manifest_path)) {
    return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
  }
  if (!File::IsDirectory(offline_dir)) {
    VERIFY1(SUCCEEDED(CreateDir(offline_dir, NULL)));
  }
  CString dest_manifest_path = ConcatenatePath(offline_dir,
                                               manifest_filename);
  HRESULT hr = File::Copy(source_manifest_path, dest_manifest_path, true);
  if (FAILED(hr)) {
    SETUP_LOG(L4, (_T("[File copy failed][%s][%s][0x%x]"),
                   source_manifest_path, dest_manifest_path, hr));
    return hr;
  }

  CString pattern;
  // Find the installer file. "Installer.exe.{guid}". Only one file per guid is
  // supported. Store "Installer.exe" in the directory
  // "Google\Update\Offline\{guid}".
  pattern.Format(_T("*.%s"), app_guid);
  std::vector<CString> files;
  hr = FindFiles(setup_temp_dir, pattern, &files);
  if (FAILED(hr)) {
    SETUP_LOG(L4, (_T("[FindFiles failed][0x%x]"), hr));
    return hr;
  }
  if (files.empty()) {
    SETUP_LOG(L4, (_T("[FindFiles found no files]")));
    return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
  }

  ASSERT1(files.size() == 1);
  ASSERT1(files[0].GetLength() > app_guid.GetLength());

  CString file_path = ConcatenatePath(setup_temp_dir, files[0]);
  CString renamed_file_name = files[0].Left(files[0].GetLength() -
                                            app_guid.GetLength() - 1);
  CString offline_app_dir = ConcatenatePath(offline_dir, app_guid);
  CString new_file_path = ConcatenatePath(offline_app_dir, renamed_file_name);
  SETUP_LOG(L4, (_T("[new_file_path][%s]"), new_file_path));

  if (File::IsDirectory(offline_app_dir)) {
    VERIFY1(SUCCEEDED(DeleteDirectoryFiles(offline_app_dir)));
  } else {
    hr = CreateDir(offline_app_dir, NULL);
    if (FAILED(hr)) {
      SETUP_LOG(L3, (_T("[CreateDir failed][%s]"), offline_app_dir));
      return hr;
    }
  }

  return File::Copy(file_path, new_file_path, false);
}

bool Setup::CopyOfflineFiles(const CString& offline_dir) {
  SETUP_LOG(L3, (_T("[Setup::CopyOfflineFiles][%s]"), offline_dir));

  ASSERT1(!args_->extra.apps.empty());
  if (args_->extra.apps.empty()) {
    return false;
  }

  for (size_t i = 0; i < args_->extra.apps.size(); ++i) {
    const GUID& app_guid = args_->extra.apps[i].app_guid;
    HRESULT hr = CopyOfflineFilesForGuid(GuidToString(app_guid), offline_dir);
    if (FAILED(hr)) {
      SETUP_LOG(L3, (_T("[CopyOfflineFilesForGuid failed][0x%x]"), hr));
      return false;
    }
  }

  return true;
}

// The installed worker is in the final location and can access the network to
// provide stats and error information.
// worker_pi can be NULL.
// TODO(omaha): Extract the command line building and unit test it.
// Starts the file that was just installed or the registered version of Google
// Update depending on whether Google Update is being installed
// (do_setup_phase_2).
// If do_setup_phase_2 is true, assumes the new version has been set in the
// registry.
// SelfInstall uses UG with the /machine override because UG silently completes
// setup without installing an app and SelfInstall may not be running as
// LocalSystem.
// Detects whether this is an offline install and stages the files
// appropriately.
// Reports errors when when offline files are not present for scenarios that
// require offline installs.
HRESULT Setup::LaunchInstalledWorker(bool do_setup_phase_2, HANDLE* process) {
  SETUP_LOG(L2, (_T("[Setup::LaunchInstalledWorker]")));

  bool is_offline = false;
  CommandLineMode cmd_line_mode = COMMANDLINE_MODE_UNKNOWN;
  if (MODE_INSTALL == mode_) {
    // If offline binaries are found the program enters the offline mode.
    is_offline = CopyOfflineFiles(is_machine_ ?
        ConfigManager::Instance()->GetMachineSecureOfflineStorageDir() :
        ConfigManager::Instance()->GetUserOfflineStorageDir());
    cmd_line_mode = do_setup_phase_2 ? COMMANDLINE_MODE_IG :
                                       COMMANDLINE_MODE_HANDOFF_INSTALL;
  } else {
    ASSERT1(!args_->is_oem_set);
    cmd_line_mode = COMMANDLINE_MODE_UG;
  }

  CommandLineBuilder builder(cmd_line_mode);
  if (is_offline) {
    builder.set_is_offline_set(is_offline);
    // If installsource is present on the command line, it will override this.
    builder.set_install_source(kCmdLineInstallSource_Offline);
  } else if (args_->is_oem_set) {
    return GOOPDATE_E_OEM_WITH_ONLINE_INSTALLER;
  } else if (args_->is_eula_required_set) {
    return GOOPDATE_E_EULA_REQURED_WITH_ONLINE_INSTALLER;
  }

  switch (mode_) {
    case MODE_INSTALL:
      builder.set_is_silent_set(args_->is_silent_set);
      builder.set_is_eula_required_set(args_->is_eula_required_set);
      ASSERT1(!args_->extra_args_str.IsEmpty());
      builder.set_extra_args(args_->extra_args_str);
      builder.set_app_args(args_->app_args_str);
      if (!args_->install_source.IsEmpty()) {
        builder.set_install_source(args_->install_source);
      }
      break;
    case MODE_SELF_UPDATE:
      ASSERT1(do_setup_phase_2);
      ASSERT1(args_->extra_args_str.IsEmpty());
      break;
    case MODE_SELF_INSTALL:
      ASSERT1(do_setup_phase_2);
      ASSERT1(!args_->extra_args_str.IsEmpty());
      builder.set_is_machine_set(is_machine_);
      break;
    case MODE_REPAIR:
      ASSERT1(do_setup_phase_2);
      ASSERT1(args_->extra_args_str.IsEmpty());
      builder.set_is_machine_set(is_machine_);
      break;
    case MODE_UNKNOWN:
    case MODE_PHASE2:
    case MODE_UNINSTALL:
    default:
      ASSERT1(false);
      break;
  }

  CString cmd_line = builder.GetCommandLineArgs();

  HRESULT hr = goopdate_utils::StartGoogleUpdateWithArgs(is_machine_,
                                                         cmd_line,
                                                         process);
  if (FAILED(hr)) {
    if (do_setup_phase_2) {
      OPT_LOG(LE, (_T("[Starting Google Update failed][%s][0x%08x]"),
                   cmd_line, hr));
      ASSERT1(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) != hr);
      return hr;
    } else {
      OPT_LOG(LE, (_T("[Google Update hand off failed][%s][0x%08x]"),
                   cmd_line, hr));
      extra_code1_ = hr;
      return GOOPDATE_E_HANDOFF_FAILED;
    }
  }

  launched_offline_worker_ = is_offline;

  return S_OK;
}

// Start the machine core process using one of the launch mechanisms.
// We know that at least one of the service and scheduled task were installed
// because otherwise we would have exited fatally.
// If the service was not installed, starting it will just fail silently and we
// will start the scheduled task.
// do not call this method until the shutdown event has been released or the
// process may immediately exit.
// TODO(omaha): Provide service_hr and task_hr failures in a ping.
HRESULT Setup::StartMachineCoreProcess() const {
  SETUP_LOG(L3, (_T("[Setup::StartMachineCoreProcess]")));

  HighresTimer metrics_timer;

  // Start the service.
  ++metric_setup_start_service_total;
  HRESULT service_hr = SetupService::StartService();
  if (SUCCEEDED(service_hr)) {
    metric_setup_start_service_ms.AddSample(metrics_timer.GetElapsedMs());
    OPT_LOG(L1, (_T("[Service started]")));
    ++metric_setup_start_service_succeeded;
    return S_OK;
  }
  metric_setup_start_service_failed_ms.AddSample(metrics_timer.GetElapsedMs());
  OPT_LOG(LEVEL_ERROR, (_T("[Start service failed][0x%08x]"), service_hr));
  metric_setup_start_service_error = service_hr;

  // TODO(omaha): We should only skip this block when /install /silent fails
  // and there are no other apps installed. Guarantee this somehow.
  ++metric_setup_start_task_total;
  const ULONGLONG start_task_start_ms = metrics_timer.GetElapsedMs();
  HRESULT task_hr = goopdate_utils::StartGoopdateTaskCore(true);
  if (SUCCEEDED(task_hr)) {
    const ULONGLONG start_task_end_ms = metrics_timer.GetElapsedMs();
    ASSERT1(start_task_end_ms >= start_task_start_ms);
    metric_setup_start_task_ms.AddSample(
        start_task_end_ms - start_task_start_ms);
    OPT_LOG(L1, (_T("[run scheduled task succeeded]")));
    ++metric_setup_start_task_succeeded;
    return S_OK;
  }
  OPT_LOG(LE, (_T("[Start scheduled task failed][0x%08x]"), task_hr));
  metric_setup_start_task_error = task_hr;

  return service_hr;
}

// Start the user core process directly.
// do not call this method until the shutdown event has been released or the
// process may immediately exit.
HRESULT Setup::StartUserCoreProcess(const CString& core_cmd_line) const {
  HRESULT hr = System::ShellExecuteCommandLine(core_cmd_line, NULL, NULL);
  if (FAILED(hr)) {
    SETUP_LOG(LE, (_T("[Could not start Google Update Core][0x%08x]"), hr));
    return hr;
  }

  return S_OK;
}

HRESULT Setup::FindCoreProcesses(Pids* found_core_pids) const {
  SETUP_LOG(L3, (_T("[Setup::FindCoreProcesses]")));
  ASSERT1(found_core_pids);

  CString user_sid;
  HRESULT hr = GetAppropriateSid(&user_sid);
  if (FAILED(hr)) {
    return hr;
  }

  std::vector<CString> command_lines;
  CString switch_to_include;
  switch_to_include.Format(_T("/%s"), kCmdLineCore);
  command_lines.push_back(switch_to_include);

  DWORD flags = INCLUDE_ONLY_PROCESS_OWNED_BY_USER |
                EXCLUDE_CURRENT_PROCESS |
                INCLUDE_PROCESS_COMMAND_LINE_CONTAINING_STRING;
  hr = Process::FindProcesses(flags,
                              kGoopdateFileName,
                              true,
                              user_sid,
                              command_lines,
                              found_core_pids);
  if (FAILED(hr)) {
    SETUP_LOG(LE, (_T("[FindProcesses failed][0x%08x]"), hr));
    return hr;
  }

  // Remove PIDs where the command line is not actually the "/c" switch and is
  // some other command line, such as "/cr".
  const Pids::iterator new_end = std::remove_if(
      found_core_pids->begin(),
      found_core_pids->end(),
      std::not1(std::ptr_fun(IsCoreProcess)));
  if (new_end != found_core_pids->end()) {
    found_core_pids->erase(new_end, found_core_pids->end());
  }

  SETUP_LOG(L2, (_T("[Core processes found][%u]"), found_core_pids->size()));
  return S_OK;
}

// Does not try to terminate legacy processes.
// Waits up to 500 ms for the terminated core processes to exit.
HRESULT Setup::TerminateCoreProcesses() const {
  SETUP_LOG(L2, (_T("[Setup::TerminateCoreProcesses]")));
  Pids found_core_pids;
  HRESULT hr = FindCoreProcesses(&found_core_pids);
  if (FAILED(hr)) {
    SETUP_LOG(LE, (_T("[FindCoreProcesses failed][0x%08x]"), hr));
    return hr;
  }

  std::vector<HANDLE> terminated_processes;
  for (size_t i = 0; i < found_core_pids.size(); ++i) {
    uint32 pid = found_core_pids[i];

    SETUP_LOG(L2, (_T("[Terminating core process][%u]"), pid));

    HANDLE process(::OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, FALSE, pid));
    if (!process) {
      SETUP_LOG(LW, (_T("[::OpenProcess failed][%u][%u]"),
                     pid, ::GetLastError()));
      continue;
    }
    terminated_processes.push_back(process);

    if (!::TerminateProcess(process, static_cast<uint32>(-2))) {
      SETUP_LOG(LW, (_T("[::TerminateProcess failed][%u][%u]"),
                     pid, ::GetLastError()));
    }
  }

  if (terminated_processes.empty()) {
    return S_OK;
  }
  // Do not return until the handles have been closed.

  const int kCoreTerminateWaitMs = 500;
  DWORD res = ::WaitForMultipleObjects(terminated_processes.size(),
                                       &terminated_processes.front(),
                                       true,  // wait for all
                                       kCoreTerminateWaitMs);
  SETUP_LOG(L2, (_T("[::WaitForMultipleObjects returned]")));
  ASSERT1(WAIT_OBJECT_0 == res || WAIT_TIMEOUT == res);
  if (WAIT_FAILED == res) {
    DWORD error = ::GetLastError();
    SETUP_LOG(LE, (_T("[::WaitForMultipleObjects failed][%u]"), error));
    hr = HRESULT_FROM_WIN32(error);
  } else {
    hr = HRESULT_FROM_WIN32(res);
  }

  for (size_t i = 0; i < terminated_processes.size(); ++i) {
    VERIFY1(::CloseHandle(terminated_processes[i]));
  }

  return hr;
}

// Tries to start the core using existing launch methods if present.
// Uses the the service registry value, scheduled task, and Run key value names
// for this version; if the ones in the installed version are are different,
// this method will not be able to start the core.
HRESULT Setup::StartCore() const {
  SETUP_LOG(L2, (_T("[Attempting to start existing core]")));

  if (is_machine_) {
    HRESULT hr = StartMachineCoreProcess();
    if (FAILED(hr)) {
      SETUP_LOG(LW, (_T("[StartMachineCoreProcess failed][0x%08x]"), hr));
      return hr;
    }
  } else {
    // Read the Run key entry to determine how to run the version that we
    // believe is installed, which may not be this instance's version or even
    // the the version in the registry if it has been over-written by us.
    CString run_key_path = AppendRegKeyPath(USER_KEY_NAME, REGSTR_PATH_RUN);
    RegKey key;
    HRESULT hr = key.Open(run_key_path);
    if (FAILED(hr)) {
      SETUP_LOG(LW, (_T("[Failed to open Run key][%s][0x%08x]"),
                     run_key_path, hr));
      return hr;
    }

    CString installed_run_cmd_line;
    hr = key.GetValue(kRunValueName, &installed_run_cmd_line);
    if (FAILED(hr)) {
      SETUP_LOG(LW, (_T("[Failed to get Run value][%s][0x%08x]"),
                     kRunValueName, hr));
      return hr;
    }

    hr = StartUserCoreProcess(installed_run_cmd_line);
    if (FAILED(hr)) {
      SETUP_LOG(LW, (_T("[StartUserCoreProcess failed][%s][0x%08x]"),
                     installed_run_cmd_line, hr));
      return hr;
    }
  }

  return S_OK;
}

// If the process exits without signaling the event, its exit code is returned.
// Waits for the process to exit (and ignore the event) unless this is an
// interactive install.
// If event is NULL, waits for the process to exit.
HRESULT Setup::WaitForProcessExitOrEvent(HANDLE process,
                                         HANDLE event,
                                         uint32* exit_code) const {
  SETUP_LOG(L3, (_T("[Setup::WaitForProcessExitOrEvent]")));
  ASSERT1(process);
  ASSERT1(exit_code);
  *exit_code = 0;

  const bool include_event_in_wait = event && !ShouldWaitForWorkerProcess();
  HANDLE handles[] = {process, event};

  const int num_handles = arraysize(handles) - (include_event_in_wait ? 0 : 1);
  const int kProcessSignaled = WAIT_OBJECT_0;
  const int kEventSignaled = WAIT_OBJECT_0 + 1;

  int res = ::WaitForMultipleObjects(num_handles,
                                     handles,
                                     false,  // wait for any one
                                     INFINITE);
  ASSERT1(kProcessSignaled == res ||
          (kEventSignaled == res) && include_event_in_wait);
  if (WAIT_FAILED == res) {
    DWORD error = ::GetLastError();
    SETUP_LOG(LE, (_T("[::WaitForMultipleObjects failed][%u]"), error));
    return HRESULT_FROM_WIN32(error);
  }

  // If the process exited, get the exit code.
  if (kProcessSignaled == res) {
    DWORD local_exit_code = 0;
    if (::GetExitCodeProcess(process, &local_exit_code)) {
      SETUP_LOG(L2, (_T("[process exited][PID %u][exit code 0x%08x]"),
                    Process::GetProcessIdFromHandle(process), local_exit_code));
      *exit_code = local_exit_code;
    } else {
      DWORD error = ::GetLastError();
      SETUP_LOG(LE, (_T("[::GetExitCodeProcess failed][%u]"), error));
      return HRESULT_FROM_WIN32(error);
    }
  } else {
    ASSERT1(kEventSignaled == res);
    ASSERT1(2 == num_handles);
    SETUP_LOG(L2, (_T("[event received before process exited]")));
  }

  return S_OK;
}

// Requires that the UI displayed event and
// kUiDisplayedEventEnvironmentVariableName environment variable exist.
HRESULT Setup::WaitForHandoffWorker(HANDLE process) const {
  HANDLE ui_displayed_event(INVALID_HANDLE_VALUE);
  HRESULT hr = UIDisplayedEventManager::GetEvent(is_machine_,
                                                 &ui_displayed_event);
  if (FAILED(hr)) {
    // The event was created in this process, so this should always succeed.
    ASSERT(false, (_T("OpenUniqueEventFromEnvironment failed][0x%08x]"), hr));
    // Only wait for the process to exit.
  }

  uint32 exit_code(0);
  hr = WaitForProcessExitOrEvent(process, ui_displayed_event, &exit_code);
  if (FAILED(hr)) {
    SETUP_LOG(LE, (_T("[WaitForProcessExitOrEvent failed][0x%08x]"), hr));
    return hr;
  }
  if (exit_code) {
    OPT_LOG(LE, (_T("[Handoff exited with error][0x%08x]"), exit_code));
    ASSERT1(FAILED(exit_code));
    return exit_code;
  }

  if (ui_displayed_event &&
      WAIT_OBJECT_0 == ::WaitForSingleObject(ui_displayed_event, 0)) {
    metric_setup_handoff_ui_ms.AddSample(metrics_timer_->GetElapsedMs());
  }

  return S_OK;
}

// Returns Local System's SID for machine installs and the user's SID otherwise.
HRESULT Setup::GetAppropriateSid(CString* sid) const {
  ASSERT1(sid);
  if (is_machine_) {
    *sid = kLocalSystemSid;
  } else {
    HRESULT hr = user_info::GetCurrentUser(NULL, NULL, sid);
    if (FAILED(hr)) {
      SETUP_LOG(LEVEL_ERROR, (_T("[GetCurrentUser failed][0x%08x]"), hr));
      return hr;
    }
  }

  return S_OK;
}

bool Setup::InitSetupLock(bool is_machine, GLock* setup_lock) {
  ASSERT1(setup_lock);
  NamedObjectAttributes setup_lock_attr;
  GetNamedObjectAttributes(kSetupMutex, is_machine, &setup_lock_attr);
  return setup_lock->InitializeWithSecAttr(setup_lock_attr.name,
                                           &setup_lock_attr.sa);
}

bool Setup::InitLegacySetupLocks(GLock* lock10,
                                 GLock* lock11_user,
                                 GLock* lock11_machine) {
  ASSERT1(lock10);
  ASSERT1(lock11_user);
  ASSERT1(lock11_machine);

  // Omaha 1.0 ensures "there is only one instance of goopdate that is trying to
  // install at a time." Thus, the lock is the same for machine and user.
  CString mutex10_name(kOmaha10GlobalPrefix);
  mutex10_name.Append(kSetupMutex);
  bool is_initialized = lock10->Initialize(mutex10_name);
  if (!is_initialized) {
    extra_code1_ = kVersion10;
    return false;
  }

  // Omaha 1.1. ensures "there is only one instance of goopdate per machine or
  // per user that is trying to install at a time." It allowed installs by
  // different users to be concurrent by using the Global and Local namespaces.
  // The machine name was only used when running as Local System, so machine
  // instances need to look for both the user and machine lock to prevent
  // conflicts with initial installs running as the user.
  CString lock11_user_name(kOmaha11LocalPrefix);
  lock11_user_name.Append(kSetupMutex);
  is_initialized = lock11_user->Initialize(lock11_user_name);
  if (!is_initialized) {
    extra_code1_ = kVersion11;
    return false;
  }

  if (is_machine_) {
    CString lock11_machine_name(kOmaha11GlobalPrefix);
    lock11_machine_name.Append(kSetupMutex);
    extra_code1_ = kVersion11MachineLock;
    return lock11_machine->Initialize(lock11_machine_name);
  } else {
    return true;
  }
}

void Setup::PersistUpdateErrorInfo(bool is_machine,
                                   HRESULT error,
                                   int extra_code1,
                                   const CString& version) {
  const TCHAR* update_key_name =
      ConfigManager::Instance()->registry_update(is_machine);
  VERIFY1(SUCCEEDED(RegKey::SetValue(update_key_name,
                                     kRegValueSelfUpdateErrorCode,
                                     static_cast<DWORD>(error))));
  VERIFY1(SUCCEEDED(RegKey::SetValue(update_key_name,
                                     kRegValueSelfUpdateExtraCode1,
                                     static_cast<DWORD>(extra_code1))));
  VERIFY1(SUCCEEDED(RegKey::SetValue(update_key_name,
                                     kRegValueSelfUpdateVersion,
                                     version)));
}

// Returns false if the values cannot be deleted to avoid skewing the log data
// with a single user pinging repeatedly with the same data.
bool Setup::ReadAndClearUpdateErrorInfo(bool is_machine,
                                        DWORD* error_code,
                                        DWORD* extra_code1,
                                        CString* version) {
  ASSERT1(error_code);
  ASSERT1(extra_code1);
  ASSERT1(version);

  const TCHAR* update_key_name =
      ConfigManager::Instance()->registry_update(is_machine);
  RegKey update_key;
  HRESULT hr = update_key.Open(update_key_name);
  if (FAILED(hr)) {
    ASSERT1(false);
    return false;
  }

  if (!update_key.HasValue(kRegValueSelfUpdateErrorCode)) {
    ASSERT1(!update_key.HasValue(kRegValueSelfUpdateExtraCode1));
    return false;
  }

  VERIFY1(SUCCEEDED(update_key.GetValue(kRegValueSelfUpdateErrorCode,
                                        error_code)));
  ASSERT1(FAILED(*error_code));

  VERIFY1(SUCCEEDED(update_key.GetValue(kRegValueSelfUpdateExtraCode1,
                                        extra_code1)));

  VERIFY1(SUCCEEDED(update_key.GetValue(kRegValueSelfUpdateVersion, version)));

  if (FAILED(update_key.DeleteValue(kRegValueSelfUpdateErrorCode)) ||
      FAILED(update_key.DeleteValue(kRegValueSelfUpdateExtraCode1)) ||
      FAILED(update_key.DeleteValue(kRegValueSelfUpdateVersion))) {
    ASSERT1(false);
    return false;
  }

  return true;
}

bool Setup::HasXmlParser() {
  CComPtr<IXMLDOMDocument> my_xmldoc;
  HRESULT hr = CoCreateSafeDOMDocument(&my_xmldoc);
  const bool ret = SUCCEEDED(hr);
  CORE_LOG(L3, (_T("[Setup::HasXmlParser returned %d][0x%08x]"), ret, hr));
  return ret;
}

// Assumes that the Setup Lock is held.
// This method is based on the assumption that if another install, which could
// be modifying the number of clients, is in progress, that it either:
//  (a) Has the Setup Lock, which is not possible because this process has it.
//  (b) Has started an install worker.
bool Setup::CanUninstallGoogleUpdate() const {
  CORE_LOG(L2, (_T("[Setup::CanUninstallGoogleUpdate]")));
  if (goopdate_utils::IsAppInstallWorkerRunning(is_machine_)) {
    CORE_LOG(L2, (_T("[Found install workers. Not uninstalling]")));
    return false;
  }
  size_t num_clients(0);
  if (SUCCEEDED(goopdate_utils::GetNumClients(is_machine_, &num_clients)) &&
      num_clients >= 2) {
    CORE_LOG(L3, (_T("[Found products. Not uninstalling]")));
    return false;
  }
  return true;
}

bool Setup::IsInteractiveInstall() const {
  return (MODE_INSTALL == mode_) && !args_->is_silent_set;
}

// The result of this method is only valid after the worker has been launched.
// The /install instance should wait for worker process (/ig or /handoff) if:
//  * Running silently: The exit code is only indication of install result.
//  * Offline install: The main reason we exit early normally is to release the
//    Setup Locks and allow downloads for multiple app installs to occur
//    simultaneously. Since offline installers do not download, this is less of
//    a concern. Also, Pack launches the offline installer interactively if the
//    user chooses to retry after a failure. See http://b/1543716.
// Long term, we will refactor the locking and always wait for the worker.
bool Setup::ShouldWaitForWorkerProcess() const {
  return !IsInteractiveInstall() || launched_offline_worker_;
}

HRESULT Setup::SetOemInstallState() {
  ASSERT1(MODE_INSTALL == mode_);
  ASSERT1(args_->is_oem_set);

  if (!is_machine_ ||
      IsElevationRequired() ||
      !ConfigManager::Instance()->IsWindowsInstalling()) {
    return GOOPDATE_E_OEM_NOT_MACHINE_AND_PRIVILEGED_AND_AUDIT_MODE;
  }

  const DWORD now = Time64ToInt32(GetCurrent100NSTime());
  OPT_LOG(L1, (_T("[Beginning OEM install][%u]"), now));
  HRESULT hr = RegKey::SetValue(
      ConfigManager::Instance()->machine_registry_update(),
      kRegValueOemInstallTimeSec,
      now);
  if (FAILED(hr)) {
    return hr;
  }
  ASSERT1(ConfigManager::Instance()->IsOemInstalling(is_machine_));

  return S_OK;
}

// Failing to set the state fails installation because this would prevent
// updates or allow updates that should not be.
HRESULT Setup::SetEulaRequiredState() {
  ASSERT1(MODE_INSTALL == mode_ ||
          MODE_SELF_INSTALL == mode_ ||
          MODE_REPAIR == mode_);
  ASSERT1(MODE_INSTALL == mode_ || !args_->is_eula_required_set);

  if (IsElevationRequired()) {
    ASSERT1(is_machine_ && !args_->is_install_elevated);
    return S_OK;
  }

  const bool eula_accepted = !args_->is_eula_required_set;
  HRESULT hr = eula_accepted ? SetEulaAccepted(is_machine_) :
                               SetEulaNotAccepted(is_machine_);
  if (FAILED(hr)) {
    SETUP_LOG(LE, (_T("[set EULA accepted state failed][accepted=%d][0x%08x]"),
                   eula_accepted, hr));
    return hr;
  }

  return S_OK;
}

HRESULT Setup::SetEulaAccepted(bool is_machine) {
  SETUP_LOG(L4, (_T("[SetEulaAccepted][%d]"), is_machine));
  const TCHAR* update_key_name =
      ConfigManager::Instance()->registry_update(is_machine);
  return RegKey::HasKey(update_key_name) ?
      RegKey::DeleteValue(update_key_name, kRegValueOmahaEulaAccepted) :
      S_OK;
}

// Does not write the registry if Google Update is already installed as
// determined by the presence of  2 or more registered apps. In those cases, we
// assume the existing EULA state is correct and do not want to disable updates
// for an existing installation.
// Assumes it is called with appropriate synchronization protection such that it
// can reliably check the number of registered clients.
HRESULT Setup::SetEulaNotAccepted(bool is_machine) {
  SETUP_LOG(L4, (_T("[SetEulaNotAccepted][%d]"), is_machine));

  size_t num_clients(0);
  if (SUCCEEDED(goopdate_utils::GetNumClients(is_machine, &num_clients)) &&
      num_clients >= 2) {
    SETUP_LOG(L4, (_T(" [Apps registered. Not setting eulaaccepted=0.]")));
    return S_OK;
  }

  const ConfigManager* cm = ConfigManager::Instance();
  return RegKey::SetValue(cm->registry_update(is_machine),
                          kRegValueOmahaEulaAccepted,
                          static_cast<DWORD>(0));
}

}  // namespace omaha
