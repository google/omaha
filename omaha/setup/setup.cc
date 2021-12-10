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

#include "omaha/setup/setup.h"
#include <regstr.h>
#include <atlpath.h>
#include <algorithm>
#include <vector>
#include "omaha/base/const_object_names.h"
#include "omaha/base/constants.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/highres_timer-win32.h"
#include "omaha/base/logging.h"
#include "omaha/base/omaha_version.h"
#include "omaha/base/process.h"
#include "omaha/base/reg_key.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/scope_guard.h"
#include "omaha/base/synchronized.h"
#include "omaha/base/system.h"
#include "omaha/base/timer.h"
#include "omaha/base/user_info.h"
#include "omaha/base/utils.h"
#include "omaha/base/vistautil.h"
#include "omaha/common/app_registry_utils.h"
#include "omaha/common/command_line.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/const_cmd_line.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/common/event_logger.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/common/ping.h"
#include "omaha/common/scheduled_task_utils.h"
#include "omaha/common/stats_uploader.h"
#include "omaha/setup/setup_files.h"
#include "omaha/setup/setup_google_update.h"
#include "omaha/setup/setup_metrics.h"
#include "omaha/setup/setup_service.h"

namespace omaha {

namespace {

const TCHAR* const kUninstallEventDescriptionFormat = _T("%s uninstall");

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
    case COMMANDLINE_MODE_CORE:
      ++metric_setup_process_wait_failed_core;
      break;
    case COMMANDLINE_MODE_REPORTCRASH:
      ++metric_setup_process_wait_failed_report;
      break;
    case COMMANDLINE_MODE_UPDATE:
      ++metric_setup_process_wait_failed_update;
      break;
    case COMMANDLINE_MODE_HANDOFF_INSTALL:
      ++metric_setup_process_wait_failed_handoff;
      break;
    case COMMANDLINE_MODE_UA:
      ++metric_setup_process_wait_failed_ua;
      break;
    case COMMANDLINE_MODE_CODE_RED_CHECK:
      ++metric_setup_process_wait_failed_cr;
      break;
    case COMMANDLINE_MODE_NOARGS:
    case COMMANDLINE_MODE_SERVICE:

    case COMMANDLINE_MODE_REGSERVER:
    case COMMANDLINE_MODE_UNREGSERVER:
    case COMMANDLINE_MODE_CRASH:
    case COMMANDLINE_MODE_INSTALL:
    case COMMANDLINE_MODE_RECOVER:
    case COMMANDLINE_MODE_COMSERVER:
    case COMMANDLINE_MODE_REGISTER_PRODUCT:
    case COMMANDLINE_MODE_UNREGISTER_PRODUCT:
    case COMMANDLINE_MODE_SERVICE_REGISTER:
    case COMMANDLINE_MODE_SERVICE_UNREGISTER:
    case COMMANDLINE_MODE_CRASH_HANDLER:
    case COMMANDLINE_MODE_COMBROKER:
    case COMMANDLINE_MODE_ONDEMAND:
    case COMMANDLINE_MODE_MEDIUM_SERVICE:
    case COMMANDLINE_MODE_UNINSTALL:
    case COMMANDLINE_MODE_PING:
    case COMMANDLINE_MODE_HEALTH_CHECK:
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
                                      kOmahaShellFileName,
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

void WriteGoogleUpdateUninstallEvent(bool is_machine) {
  CString description;
  SafeCStringFormat(&description, kUninstallEventDescriptionFormat, kAppName);
  GoogleUpdateLogEvent uninstall_event(EVENTLOG_INFORMATION_TYPE,
                                       kUninstallEventId,
                                       is_machine);
  uninstall_event.set_event_desc(description);
  uninstall_event.WriteEvent();
}

// TODO(omaha3): Enable. Will we still need this method?
// Returns true if the mode can be determined and pid represents a "/c" process.
bool IsCoreProcess(uint32 pid) {
  CommandLineMode mode(COMMANDLINE_MODE_UNKNOWN);
  return SUCCEEDED(GetProcessModeFromPid(pid, &mode)) &&
         COMMANDLINE_MODE_CORE == mode;
}

}  // namespace

bool Setup::did_uninstall_ = false;

Setup::Setup(bool is_machine)
    : is_machine_(is_machine),
      is_self_update_(false),
      extra_code1_(S_OK) {
  SETUP_LOG(L2, (_T("[Setup::Setup]")));
}

Setup::~Setup() {
  SETUP_LOG(L2, (_T("[Setup::~Setup]")));
}

// Protects all setup-related operations with:
// * Setup Lock - Prevents other instances from installing Google Update for
// this machine/user at the same time.
// * Shutdown Event - Tells existing other instances and any instances that may
// start during Setup to exit.
// Setup-related operations do not include installation of the app.
HRESULT Setup::Install(RuntimeMode runtime_mode) {
  SETUP_LOG(L3,
      (_T("[Admin=%d, NEAdmin=%d, Update3Svc=%d, MedSvc=%d, Machine=%d"),
       vista_util::IsUserAdmin(),
       vista_util::IsUserNonElevatedAdmin(),
       SetupUpdate3Service::IsServiceInstalled(),
       SetupUpdateMediumService::IsServiceInstalled(),
       is_machine_));

  ASSERT1(!IsElevationRequired());

  // Start the setup timer.
  metrics_timer_.reset(new HighresTimer);

  GLock setup_lock;

  if (!InitSetupLock(is_machine_, &setup_lock)) {
    SETUP_LOG(L2, (_T("[Setup::InitSetupLock failed]")));
    extra_code1_ = kVersion12;
    return GOOPDATE_E_SETUP_LOCK_INIT_FAILED;
  }

  HighresTimer lock_metrics_timer;

  if (!setup_lock.Lock(kSetupLockWaitMs)) {
    OPT_LOG(LE, (_T("[Failed to acquire setup lock]")));
    return HandleLockFailed(kVersion12);
  }
  metric_setup_lock_acquire_ms.AddSample(lock_metrics_timer.GetElapsedMs());
  SETUP_LOG(L1, (_T("[Setup Locks acquired]")));

  HRESULT hr = DoProtectedInstall(runtime_mode);
  if (FAILED(hr)) {
    SETUP_LOG(LE, (_T("[Setup::DoProtectedInstall failed][0x%08x]"), hr));
  }

  SETUP_LOG(L1, (_T("[Releasing Setup Lock]")));
  return hr;
}

// TODO(omaha3): Eliminate lock_version if we do not fix http://b/1076207.
// Sets appropriate metrics and extra_code1_ value. It then tries to determine
// the scenario that caused this failure and returns an appropriate error.
// The detected processes may not actually be in conflict with this one, but are
// more than likely the cause of the lock failure.
HRESULT Setup::HandleLockFailed(int lock_version) {
  ++metric_setup_locks_failed;

  switch (lock_version) {
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

  SafeCStringFormat(&switch_to_include, _T("/%s"), kCmdLineUpdate);
  HRESULT hr = GetPidsWithArgsForAllUsers(switch_to_include, &matching_pids);
  if (FAILED(hr)) {
    ASSERT1(false);
    return GOOPDATE_E_FAILED_TO_GET_LOCK;
  }
  if (!matching_pids.empty()) {
    return GOOPDATE_E_FAILED_TO_GET_LOCK_UPDATE_PROCESS_RUNNING;
  }

  SafeCStringFormat(&switch_to_include, _T("/%s"), kCmdLineUninstall);
  hr = GetPidsWithArgsForAllUsers(switch_to_include, &matching_pids);
  if (FAILED(hr)) {
    ASSERT1(false);
    return GOOPDATE_E_FAILED_TO_GET_LOCK;
  }
  if (!matching_pids.empty()) {
    return GOOPDATE_E_FAILED_TO_GET_LOCK_UNINSTALL_PROCESS_RUNNING;
  }

  SafeCStringFormat(&switch_to_include, _T("/%s"), kCmdLineInstall);
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
  int exe_index = current_cmd_line.Find(kOmahaShellFileName);
  if (-1 == exe_index) {
    ASSERT(false, (_T("Unable to find %s in %s"),
                   kOmahaShellFileName, current_cmd_line));
    return GOOPDATE_E_FAILED_TO_GET_LOCK;
  }
  int args_start = exe_index + static_cast<int>(_tcslen(kOmahaShellFileName));
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
HRESULT Setup::DoProtectedInstall(RuntimeMode runtime_mode) {
  SETUP_LOG(L2, (_T("[Setup::DoProtectedInstall]")));

  SetupFiles setup_files(is_machine_);

  HRESULT hr = setup_files.Init();
  if (FAILED(hr)) {
    SETUP_LOG(LE, (_T("[SetupFiles::Init failed][0x%08x]"), hr));
    return hr;
  }

  if (ShouldInstall()) {
    ++metric_setup_do_self_install_total;

    // TODO(omaha3): IMPORTANT: Try to avoid losing users due to firewall
    // blocking caused by changing the constant shell. Try a simple ping using
    // the new shell, and if it fails take one of the following actions:
    //  1) Keep the old shell (if possible).
    //  2) Fail the self-update. Leave the user on this version. Would need to
    //     figure out a way to avoid updating the user every 5 hours.

    hr = DoProtectedGoogleUpdateInstall(&setup_files);
    if (FAILED(hr)) {
      SETUP_LOG(LE, (_T("[DoProtectedGoogleUpdateInstall fail][0x%08x]"), hr));
      // Do not return until rolling back and releasing the events.
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
      return hr;
    }

    ++metric_setup_do_self_install_succeeded;
  }

  // We set or reset the Runtime mode here, regardless of whether Omaha was
  // already installed or we just installed it. This is to allow for turning
  // on/off the Runtime mode independent of Omaha installation.
  SetRuntimeMode(runtime_mode);

  return S_OK;
}

// Assumes that the shell is the correct version for the existing Omaha version.
bool Setup::ShouldInstall() {
  SETUP_LOG(L2, (_T("[Setup::ShouldInstall]")));

  // TODO(omaha3): Figure out a different way to record these stats.
  bool is_install = true;

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

  // If running from the official install directory for this type of install
  // (user/machine), it is most likely a OneClick install. Do not install self.
  if (goopdate_utils::IsRunningFromOfficialGoopdateDir(is_machine_)) {
    ++metric_setup_should_install_false_oc;
    return false;
  }

  if (is_install) {
    ++metric_setup_subsequent_install_total;
  }

  bool should_install(false);

  ULONGLONG cv = VersionFromString(existing_version);
  if (cv >= my_version) {
    should_install = ShouldOverinstall();
    if (should_install) {
      ++metric_setup_should_overinstall_true;
    } else {
      ++metric_setup_should_overinstall_false;
    }
  } else {
    ++metric_setup_should_install_true_newer;
    should_install = true;
  }

  if (is_install && should_install) {
    ++metric_setup_subsequent_install_should_install_true;
  }

  OPT_LOG(L1, (_T("[machine = %d][existing version = %s][should_install = %d]"),
               is_machine_, existing_version, should_install));

  return should_install;
}

bool Setup::ShouldOverinstall() {
  SETUP_LOG(L2, (_T("[Setup::ShouldOverinstall]")));

  CommandLineBuilder builder(COMMANDLINE_MODE_HEALTH_CHECK);
  CString cmd_line = builder.GetCommandLineArgs();
  scoped_process process;
  HRESULT hr = goopdate_utils::StartGoogleUpdateWithArgs(is_machine_,
                                                         StartMode::kBackground,
                                                         cmd_line,
                                                         address(process));
  if (FAILED(hr)) {
    SETUP_LOG(LE, (_T("[HealthCheck failed to start][%#x]"), hr));
    return true;
  }

  const DWORD result(::WaitForSingleObject(get(process), INFINITE));
  DWORD exit_code(0);
  const bool should_overinstall =
      result != WAIT_OBJECT_0 ||
      !::GetExitCodeProcess(get(process), &exit_code) ||
      FAILED(exit_code);
  if (should_overinstall) {
    SETUP_LOG(LE, (_T("[HealthCheck failed][%d][%#x]"), result, exit_code));
  }

  return should_overinstall;
}

HRESULT Setup::DoProtectedGoogleUpdateInstall(SetupFiles* setup_files) {
  ASSERT1(setup_files);
  SETUP_LOG(L2, (_T("[Setup::DoProtectedGoogleUpdateInstall]")));

  HRESULT hr = StopGoogleUpdateAndWait(GetForceKillWaitTimeMs());
  if (FAILED(hr)) {
    SETUP_LOG(LE, (_T("[StopGoogleUpdateAndWait failed][0x%08x]"), hr));
    if (E_ACCESSDENIED == hr) {
      return GOOPDATE_E_ACCESSDENIED_STOP_PROCESSES;
    }
    return hr;
  }

// TODO(omaha3): Enable. Prefer to move out of Setup if possible.
#if 0
  VERIFY_SUCCEEDED(ResetMetrics(is_machine_));
#endif

  hr = RegKey::GetValue(
             ConfigManager::Instance()->registry_clients_goopdate(is_machine_),
             kRegValueProductVersion,
             &saved_version_);
  if (FAILED(hr)) {
    SETUP_LOG(L3, (_T("[failed to get existing Omaha version][0x%08x]"), hr));
    // Continue as this is expected for first installs.
  }

  hr = setup_files->Install();
  if (FAILED(hr)) {
    SETUP_LOG(LE, (_T("[SetupFiles::Install failed][0x%08x]"), hr));
    extra_code1_ =
        setup_files->extra_code1() | PingEvent::kSetupFilesExtraCodeMask;
    return hr;
  }

  hr = SetupGoogleUpdate();
  if (FAILED(hr)) {
    SETUP_LOG(LE, (_T("[SetupGoogleUpdate failed][0x%08x]"), hr));
    return hr;
  }

  // TODO(omaha3): Maybe move out of Setup.
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

    VERIFY_SUCCEEDED(RegKey::SetValue(
        ConfigManager::Instance()->registry_clients_goopdate(is_machine_),
        kRegValueProductVersion,
        saved_version_));
  }

  // TODO(omaha3): Rollback SetupGoogleUpdate.
  VERIFY_SUCCEEDED(setup_files->RollBack());
}

// Assumes the caller is ensuring this is the only running instance of setup.
// The original process holds the lock while it waits for this one to complete.
HRESULT Setup::SetupGoogleUpdate() {
  SETUP_LOG(L2, (_T("[Setup::SetupGoogleUpdate]")));

  HighresTimer phase2_metrics_timer;

  omaha::SetupGoogleUpdate setup_google_update(is_machine_, is_self_update_);

  HRESULT hr = setup_google_update.FinishInstall();
  if (FAILED(hr)) {
    extra_code1_ = setup_google_update.extra_code1();
    SETUP_LOG(LE, (_T("[FinishInstall failed][0x%x][0x%x]"), hr, extra_code1_));
    return hr;
  }

  // Release the shutdown event so that we can start the core if necessary, and
  // we do not interfere with other app installs that may be waiting on the
  // Setup Lock.
  ASSERT1(shutdown_event_);
  ReleaseShutdownEvents();

  if (!scheduled_task_utils::IsUATaskHealthy(is_machine_)) {
    HRESULT start_hr = StartCore();
    if (FAILED(start_hr)) {
      SETUP_LOG(LW, (_T("[StartCore failed][0x%x]"), start_hr));
    }
  }

  // Setup is now complete.

  metric_setup_phase2_ms.AddSample(phase2_metrics_timer.GetElapsedMs());
  return S_OK;
}

// Stops all user/machine instances including the service, unregisters using
// SetupGoogleUpdate, then deletes  the files using SetupFiles.
// Does not wait for the processes to exit, except the service.
// Protects all operations with the setup lock. If MSI is found busy, Omaha
// won't uninstall.
HRESULT Setup::Uninstall(bool send_uninstall_ping) {
  OPT_LOG(L1, (_T("[Setup::Uninstall]")));
  ASSERT1(!IsElevationRequired());

  // Try to get the global setup lock; if the lock is taken, do not block
  // waiting to uninstall; just return.
  GLock setup_lock;
  VERIFY1(InitSetupLock(is_machine_, &setup_lock));
  if (!setup_lock.Lock(0)) {
    OPT_LOG(LE, (_T("[Failed to acquire setup lock]")));
    return E_FAIL;
  }

  return DoProtectedUninstall(send_uninstall_ping);
}

// Aggregates metrics regardless of whether uninstall is allowed.
// Foces reporting of the metrics if uninstall is allowed.
// Assumes that the current process holds the Setup Lock.
HRESULT Setup::DoProtectedUninstall(bool send_uninstall_ping) {
  const bool can_uninstall = CanUninstallGoogleUpdate();
  OPT_LOG(L1, (_T("[CanUninstallGoogleUpdate returned %d]"), can_uninstall));

  HRESULT hr = S_OK;
  if (!can_uninstall) {
    hr = GOOPDATE_E_CANT_UNINSTALL;
  } else {
    hr = StopGoogleUpdateAndWait(GetForceKillWaitTimeMs());
    if (FAILED(hr)) {
      // If there are any clients that don't listen to the shutdown event,
      // such as the current Update3Web workers, we'll need to wait until
      // they can be shut down.
      // TODO(omaha3): We might want to add a count metric for this case,
      // and maybe go through with the uninstall anyways after several tries.
      SETUP_LOG(L1, (_T("[StopGoogleUpdateAndWait returned 0x%08x]"), hr));
      hr = GOOPDATE_E_CANT_UNINSTALL;
    }
  }

  if (FAILED(hr)) {
    VERIFY_SUCCEEDED(AggregateMetrics(is_machine_));
    return hr;
  }
  hr = AggregateAndReportMetrics(is_machine_, true);
  ASSERT1(SUCCEEDED(hr) || GOOPDATE_E_CANNOT_USE_NETWORK == hr);

  bool can_use_network = ConfigManager::Instance()->CanUseNetwork(is_machine_);
  if (can_use_network && send_uninstall_ping) {
    SendUninstallPing();
  }

  // Write the event in the event log before uninstalling the program since
  // the event contains version and language information, which are removed
  // during the uninstall.
  WriteGoogleUpdateUninstallEvent(is_machine_);

  omaha::SetupGoogleUpdate setup_google_update(is_machine_, is_self_update_);
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

  HRESULT hr = SignalShutdownEvent();
  if (FAILED(hr)) {
    SETUP_LOG(LE, (_T("[SignalShutdownEvent failed][0x%08x]"), hr));
    return hr;
  }

  return S_OK;
}

int Setup::GetForceKillWaitTimeMs() const {
  return is_self_update_ ? kSetupUpdateShutdownWaitMs :
                           kSetupInstallShutdownWaitMs;
}

HRESULT Setup::StopGoogleUpdateAndWait(int wait_time_before_kill_ms) {
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

  hr = WaitForOtherInstancesToExit(pids, wait_time_before_kill_ms);
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

void Setup::ReleaseShutdownEvents() {
  if (!shutdown_event_) {
    return;
  }

  VERIFY1(::ResetEvent(get(shutdown_event_)));
  reset(shutdown_event_);
}

// Because this waiting can occur before a UI is generated, we do not want to
// wait too long.
// If a process fails to stop, its mode is stored in extra_code1_.
// Does not return until all opened handles have been closed.
// TODO(omaha): Add a parameter to specify the amount of time to wait to this
// method and StopGoogleUpdateAndWait after we unify Setup and always have a UI.
HRESULT Setup::WaitForOtherInstancesToExit(const Pids& pids,
                                           int wait_time_before_kill_ms) {
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

    HighresTimer metrics_timer;
    DWORD res = WaitForAllObjects(handles.size(),
                                  &handles.front(),
                                  wait_time_before_kill_ms);
    metric_setup_process_wait_ms.AddSample(metrics_timer.GetElapsedMs());

    SETUP_LOG(L2, (_T("[::WaitForMultipleObjects returned]")));
    ASSERT1(WAIT_OBJECT_0 == res || WAIT_TIMEOUT == res);
    if (WAIT_FAILED == res) {
      const DWORD error = ::GetLastError();
      SETUP_LOG(LE, (_T("[::WaitForMultipleObjects failed][%u]"), error));
      hr = HRESULT_FROM_WIN32(error);
    } else if (WAIT_OBJECT_0 != res) {
      OPT_LOG(LEVEL_ERROR, (_T("[Other ") MAIN_EXE_BASE_NAME _T(".exe instances failed to ")
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
  } else {
    for (size_t i = 0; i < handles.size(); ++i) {
      if (!::TerminateProcess(handles[i], UINT_MAX)) {
        const uint32 pid = Process::GetProcessIdFromHandle(handles[i]);
        const DWORD error = ::GetLastError();
        SETUP_LOG(LW, (_T("[::TerminateProcess failed][%u][%u]"), pid, error));
      }
    }

    const int kTerminateWaitMs = 500;
    DWORD res = WaitForAllObjects(handles.size(),
                                  &handles.front(),
                                  kTerminateWaitMs);
    if (res != WAIT_OBJECT_0) {
      const DWORD error = ::GetLastError();
      SETUP_LOG(LW, (_T("[Wait failed][%u][%u]"), res, error));
    }
  }

  // Close the handles.
  for (size_t i = 0; i < handles.size(); ++i) {
    if (!::CloseHandle(handles[i])) {
      hr = HRESULTFromLastError();
      SETUP_LOG(LEVEL_WARNING, (_T("[CloseHandle failed][0x%08x]"), hr));
    }
  }

  return S_OK;
}

// Wait for all instances of Omaha running as the current user - or as any user
// in the case of machine installs - except "/install" or "/registerproduct"
// instances, which should be blocked by the Setup Lock, which we are holding.
HRESULT Setup::GetPidsToWaitFor(Pids* pids) const {
  ASSERT1(pids);

  HRESULT hr = GetPidsToWaitForUsingCommandLine(pids);
  if (FAILED(hr)) {
    return hr;
  }

  ASSERT1(pids->end() == std::find(pids->begin(), pids->end(),
                                   ::GetCurrentProcessId()));
  SETUP_LOG(L3, (_T("[found %d total processes to wait for]"), pids->size()));

  return S_OK;
}

// Finds processes to wait for based on the command line.
// Differences between Omaha 2 and this code:
//  * User's processes running outside AppData are not caught. This should only
//    be /install or /registerproduct.
//  * In the user case, "machine" processes running as the user are EXcluded
//    based on path instead of mode and "needsadmin=true". This additionally
//    excludes oneclick cross-installs (u-to-m).
//  * In the machine case, "machine" processes running as the user are INcluded
//    based on path instead of mode and "needsadmin=true".
//  * /pi: m-to-u running in PF as user with needsadmin=false: now INcluded.
//    This is a good idea, since we do not want to delete the in-use file.
//    /pi: u-to-m running in appdata as user with needsadmin=true: now
//    EXcluded.
HRESULT Setup::GetPidsToWaitForUsingCommandLine(Pids* pids) const {
  CORE_LOG(L3, (_T("[Setup::GetPidsToWaitForUsingCommandLine]")));

  ASSERT1(pids);

  // Get processes running as the current user in the case of user, and all
  // users as well as SYSTEM in the case of machine, except those with
  // * "/install" - must be excluded because may be waiting for the Setup Lock.
  // * "/registerproduct" - same as for /install.
  std::vector<CString> command_lines;
  CString switch_to_exclude;
  SafeCStringFormat(&switch_to_exclude, _T(" /%s "), kCmdLineInstall);
  command_lines.push_back(switch_to_exclude);
  SafeCStringFormat(&switch_to_exclude, _T(" /%s "), kCmdLineRegisterProduct);
  command_lines.push_back(switch_to_exclude);

  CString user_sid;
  DWORD flags = EXCLUDE_CURRENT_PROCESS |
                EXCLUDE_PROCESS_COMMAND_LINE_CONTAINING_STRING;

  if (!is_machine_) {
    // Search only the same sid as the current user.
    flags |= INCLUDE_ONLY_PROCESS_OWNED_BY_USER;

    HRESULT hr = user_info::GetProcessUser(NULL, NULL, &user_sid);
    if (FAILED(hr)) {
      CORE_LOG(LE, (_T("[GetProcessUser failed][0x%x]"), hr));
      return hr;
    }
  }

  std::vector<uint32> google_update_process_ids;
  HRESULT hr = Process::FindProcesses(flags,
                                      kOmahaShellFileName,
                                      true,
                                      user_sid,
                                      command_lines,
                                      &google_update_process_ids);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T(" [FindProcesses failed][0x%08x]"), hr));
    return hr;
  }

  // Also finds all crash handler processes.
  std::vector<uint32> crash_handler_process_ids;
  const TCHAR* kCrashHandlerFileNames[] = {
    kCrashHandlerFileName,
    kCrashHandler64FileName,
  };
  for (size_t i = 0; i < arraysize(kCrashHandlerFileNames); ++i) {
    std::vector<uint32> matching_pids;
    hr = Process::FindProcesses(0,
                                kCrashHandlerFileNames[i],
                                true,
                                user_sid,
                                std::vector<CString>(),
                                &matching_pids);
    if (SUCCEEDED(hr)) {
      crash_handler_process_ids.insert(crash_handler_process_ids.end(),
                                       matching_pids.begin(),
                                       matching_pids.end());
    }
  }

  std::vector<uint32> candidate_process_ids;
  candidate_process_ids.insert(candidate_process_ids.end(),
                               google_update_process_ids.begin(),
                               google_update_process_ids.end());
  candidate_process_ids.insert(candidate_process_ids.end(),
                               crash_handler_process_ids.begin(),
                               crash_handler_process_ids.end());

  const ConfigManager* cm = ConfigManager::Instance();
  CString official_path(is_machine_ ?
                        cm->GetMachineGoopdateInstallDirNoCreate() :
                        cm->GetUserGoopdateInstallDirNoCreate());
  ASSERT1(!official_path.IsEmpty());

  // Only include processes running under the official path.
  Pids pids_to_wait_for;
  for (size_t i = 0; i < candidate_process_ids.size(); ++i) {
    CString cmd_line;
    const uint32 process_id = candidate_process_ids[i];
    if (SUCCEEDED(Process::GetCommandLine(process_id, &cmd_line))) {
      cmd_line.MakeLower();

      CString exe_path;
      if (SUCCEEDED(GetExePathFromCommandLine(cmd_line, &exe_path)) &&
          String_StrNCmp(official_path, exe_path, official_path.GetLength(),
                         true) == 0) {
        CORE_LOG(L4, (_T(" [Including pid][%u][%s]"), process_id, cmd_line));
        pids_to_wait_for.push_back(process_id);
      }
    }
  }

  pids->swap(pids_to_wait_for);
  return S_OK;
}

// On Windows Vista, an admin must be elevated in order to install a machine app
// without elevating. On Vista, IsUserAdmin returns false unless the user is
// elevated.
bool Setup::IsElevationRequired() const {
  return is_machine_ && !vista_util::IsUserAdmin();
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
  HRESULT service_hr = SetupUpdate3Service::StartService();
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
  HRESULT task_hr = scheduled_task_utils::StartGoopdateTaskCore(true);
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
// Do not call this method until the shutdown event has been released or the
// process may immediately exit.
HRESULT Setup::StartUserCoreProcess(const CString& core_cmd_line) const {
  HRESULT hr = System::StartCommandLine(core_cmd_line);
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
  SafeCStringFormat(&switch_to_include, _T("/%s"), kCmdLineCore);
  command_lines.push_back(switch_to_include);

  DWORD flags = INCLUDE_ONLY_PROCESS_OWNED_BY_USER |
                EXCLUDE_CURRENT_PROCESS |
                INCLUDE_PROCESS_COMMAND_LINE_CONTAINING_STRING;
  hr = Process::FindProcesses(flags,
                              kOmahaShellFileName,
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
      [](Pids::value_type pid) { return !IsCoreProcess(pid); });
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

  ASSERT1(terminated_processes.size() <= MAXIMUM_WAIT_OBJECTS);

  const int kCoreTerminateWaitMs = 500;
  DWORD res = ::WaitForMultipleObjects(
      static_cast<DWORD>(terminated_processes.size()),
      &terminated_processes.front(),
      true,  // wait for all
      kCoreTerminateWaitMs);
  SETUP_LOG(L2, (_T("[::WaitForMultipleObjects returned]")));
  ASSERT1(WAIT_OBJECT_0 == res || WAIT_TIMEOUT == res);
  if (WAIT_FAILED == res) {
    const DWORD error = ::GetLastError();
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
// Uses the service or the scheduled task for machine, and the Run key value for
// user.
HRESULT Setup::StartCore() const {
  SETUP_LOG(L2, (_T("[Attempting to start core]")));

  if (is_machine_) {
    HRESULT hr = StartMachineCoreProcess();
    if (FAILED(hr)) {
      SETUP_LOG(LW, (_T("[StartMachineCoreProcess failed][0x%08x]"), hr));
      return hr;
    }

    return hr;
  }

  CString installed_run_cmd_line;
  HRESULT hr = RegKey::GetValue(USER_KEY REGSTR_PATH_RUN, kRunValueName,
                                &installed_run_cmd_line);
  if (FAILED(hr)) {
    SETUP_LOG(LW, (_T("[Failed to get Run val][%s][0x%x]"), kRunValueName, hr));
    return hr;
  }

  hr = StartUserCoreProcess(installed_run_cmd_line);
  if (FAILED(hr)) {
    SETUP_LOG(LW, (_T("[StartUserCoreProcess failed][%s][0x%x]"),
                   installed_run_cmd_line, hr));
    return hr;
  }

  return S_OK;
}

// Returns Local System's SID for machine installs and the user's SID otherwise.
HRESULT Setup::GetAppropriateSid(CString* sid) const {
  ASSERT1(sid);
  if (is_machine_) {
    *sid = kLocalSystemSid;
  } else {
    HRESULT hr = user_info::GetProcessUser(NULL, NULL, sid);
    if (FAILED(hr)) {
      SETUP_LOG(LEVEL_ERROR, (_T("[GetProcessUser failed][0x%08x]"), hr));
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

// Assumes that the Setup Lock is held.
// This method is based on the assumption that if another install, which could
// be modifying the number of clients, is in progress, that it either:
//  (a) Has the Setup Lock, which is not possible because this process has it.
//  (b) Has started an install worker.
// TODO(omaha3): This is flawed: http://b/2764048.
bool Setup::CanUninstallGoogleUpdate() const {
  CORE_LOG(L2, (_T("[Setup::CanUninstallGoogleUpdate]")));
  if (goopdate_utils::IsAppInstallWorkerRunning(is_machine_)) {
    CORE_LOG(L2, (_T("[Found install workers. Not uninstalling]")));
    return false;
  }

  RuntimeMode runtime_mode = GetRuntimeMode();
  if (runtime_mode == RUNTIME_MODE_PERSIST) {
    OPT_LOG(L1, (_T("[RUNTIME_MODE_PERSIST is set. Not uninstalling.]")));
    return false;
  } else if (runtime_mode == RUNTIME_MODE_TRUE) {
    // If the RUNTIME_MODE_TRUE flag is set, that implies that someone has
    // installed us with the runtime=true flag, expecting that they can
    // use our API later from another process.  If 24 hours have passed
    // since that initial Omaha install, we clear the flag but still
    // return false for this check.  (That way, if a machine has been
    // suspended in mid-install, they still have a grace period until
    // the next /ua to get something installed.)
    CORE_LOG(L3, (_T("[RUNTIME_MODE_TRUE is set. Not uninstalling.]")));
    if (ConfigManager::Instance()->Is24HoursSinceLastUpdate(is_machine_)) {
      CORE_LOG(L4, (_T("[24 hours elapsed; clearing RUNTIME_MODE_TRUE.]")));
      SetRuntimeMode(RUNTIME_MODE_FALSE);
    }
    return false;
  }
  size_t num_clients(0);
  if (SUCCEEDED(app_registry_utils::GetNumClients(is_machine_, &num_clients)) &&
      num_clients >= 2) {
    CORE_LOG(L3, (_T("[Found products. Not uninstalling]")));
    return false;
  }

  return true;
}

RuntimeMode Setup::GetRuntimeMode() const {
  const TCHAR* key = ConfigManager::Instance()->registry_update(is_machine_);
  if (!RegKey::HasValue(key, kRegValueRuntimeMode)) {
    return RUNTIME_MODE_NOT_SET;
  }
  DWORD runtime_mode = static_cast<DWORD>(RUNTIME_MODE_NOT_SET);
  if (FAILED(RegKey::GetValue(key, kRegValueRuntimeMode, &runtime_mode))) {
    return RUNTIME_MODE_NOT_SET;
  }

  if (runtime_mode != RUNTIME_MODE_TRUE &&
      runtime_mode != RUNTIME_MODE_PERSIST) {
    return RUNTIME_MODE_NOT_SET;
  }

  return static_cast<RuntimeMode>(runtime_mode);
}

HRESULT Setup::SetRuntimeMode(RuntimeMode runtime_mode) const {
  if (runtime_mode == RUNTIME_MODE_NOT_SET) {
    return S_FALSE;
  }

  const TCHAR* key = ConfigManager::Instance()->registry_update(is_machine_);
  if (runtime_mode == RUNTIME_MODE_TRUE ||
      runtime_mode == RUNTIME_MODE_PERSIST) {
    return RegKey::SetValue(key,
                            kRegValueRuntimeMode,
                            static_cast<DWORD>(runtime_mode));
  } else {
    return RegKey::DeleteValue(key, kRegValueRuntimeMode);
  }
}

HRESULT Setup::SendUninstallPing() {
  CORE_LOG(L3, (_T("[SendUninstallPing]")));

  const bool is_eula_accepted =
      app_registry_utils::IsAppEulaAccepted(is_machine_,
                                            kGoogleUpdateAppId,
                                            false);

  if (!is_eula_accepted) {
    CORE_LOG(LE, (_T("[SendUninstallPing - eula not accepted]")));
    return E_FAIL;
  }

  PingEventPtr uninstall_ping_event(
      new PingEvent(PingEvent::EVENT_UNINSTALL,
                    PingEvent::EVENT_RESULT_SUCCESS,
                    0,
                    0));

  // Generate a session ID for uninstall pings.  NOTE: The assumption here is
  // that if /uninstall was launched by /ua, we have no updates to check for,
  // so we assume that /ua won't use its own session ID.  If /ua does any
  // network activity on a no-clients case, we will have to start passing the
  // session ID from /ua to /uninstall in the future.
  CString session_id;
  VERIFY_SUCCEEDED(GetGuid(&session_id));

  // Send uninstall ping for uninstalled apps.
  HRESULT hr = S_OK;
  std::vector<CString> uninstalled_apps;
  if (SUCCEEDED(app_registry_utils::GetUninstalledApps(is_machine_,
                                                       &uninstalled_apps))) {
    Ping apps_uninstall_ping(is_machine_, session_id, kInstallSource_Uninstall);
    apps_uninstall_ping.LoadAppDataFromRegistry(uninstalled_apps);
    apps_uninstall_ping.BuildAppsPing(uninstall_ping_event);

    hr = SendReliablePing(&apps_uninstall_ping, false);
    if (FAILED(hr)) {
      CORE_LOG(LE, (_T("[SendUninstallPing: failed to send app uninstall ping]")
                    _T("[0x%08x]"), hr));
    }
  }

  // Send uninstall ping for Omaha.
  const CString current_omaha_version(GetVersionString());
  const CString next_omaha_version;     // Empty, in the uninstall case.
  Ping omaha_uninstall_ping(is_machine_, session_id, kInstallSource_Uninstall);
  omaha_uninstall_ping.LoadOmahaDataFromRegistry();
  omaha_uninstall_ping.BuildOmahaPing(current_omaha_version,
                                      next_omaha_version,
                                      uninstall_ping_event);
  hr = SendReliablePing(&omaha_uninstall_ping, false);
  if (SUCCEEDED(hr)) {
    // Clears the registry after ping is sent successfully.
    uninstalled_apps.clear();
    app_registry_utils::GetUninstalledApps(is_machine_, &uninstalled_apps);
    app_registry_utils::RemoveClientStateForApps(is_machine_, uninstalled_apps);
  } else {
    CORE_LOG(LE, (_T("[SendUninstallPing: failed to send Omaha uninstall ping]")
                  _T("[0x%08x]"), hr));
  }

  return hr;
}


}  // namespace omaha
