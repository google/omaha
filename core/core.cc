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

// Core is the long-lived Omaha process. It runs one instance for the
// machine and one instance for each user session, including console and TS
// sessions.
// If the same user is logged in multiple times, only one core process will
// be running.

#include "omaha/core/core.h"
#include <atlsecurity.h>
#include <algorithm>
#include <map>
#include <string>
#include <vector>
#include "omaha/common/app_util.h"
#include "omaha/common/const_object_names.h"
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/logging.h"
#include "omaha/common/module_utils.h"
#include "omaha/common/path.h"
#include "omaha/common/reactor.h"
#include "omaha/common/reg_key.h"
#include "omaha/common/shutdown_handler.h"
#include "omaha/common/system.h"
#include "omaha/common/time.h"
#include "omaha/common/user_info.h"
#include "omaha/common/utils.h"
#include "omaha/common/vistautil.h"
#include "omaha/core/core_metrics.h"
#include "omaha/core/legacy_manifest_handler.h"
#include "omaha/core/scheduler.h"
#include "omaha/core/system_monitor.h"
#include "omaha/goopdate/command_line_builder.h"
#include "omaha/goopdate/config_manager.h"
#include "omaha/goopdate/crash.h"
#include "omaha/goopdate/program_instance.h"
#include "omaha/goopdate/goopdate_utils.h"
#include "omaha/goopdate/stats_uploader.h"

namespace omaha {

Core::Core()
    : is_system_(false),
      is_crash_handler_enabled_(false),
      main_thread_id_(0) {
  CORE_LOG(L1, (_T("[Core::Core]")));
}

Core::~Core() {
  CORE_LOG(L1, (_T("[Core::~Core]")));
  scheduler_.reset(NULL);
  system_monitor_.reset(NULL);
}

// We always return S_OK, because the core can be invoked from the system
// scheduler, and the scheduler does not work well if the process returns
// an error. We do not depend on the return values from the Core elsewhere.
HRESULT Core::Main(bool is_system, bool is_crash_handler_enabled) {
  HRESULT hr = DoMain(is_system, is_crash_handler_enabled);
  if (FAILED(hr)) {
    OPT_LOG(LW, (_T("[Core::DoMain failed][0x%x]"), hr));
  }

  return S_OK;
}

HRESULT Core::DoMain(bool is_system, bool is_crash_handler_enabled) {
  main_thread_id_ = ::GetCurrentThreadId();
  is_system_ = is_system;
  is_crash_handler_enabled_ = is_crash_handler_enabled;

  if (ConfigManager::Instance()->IsOemInstalling(is_system_)) {
    // Exit immediately while an OEM is installing Windows. This prevents cores
    // or update workers from being started by the Scheduled Task or other means
    // before the system is sealed.
    OPT_LOG(L1, (_T("[Exiting because an OEM is installing Windows]")));
    ASSERT1(is_system_);
    return S_OK;
  }

  // Do a code red check as soon as possible.
  StartCodeRed();

  CORE_LOG(L2, (_T("[IsGoogler %d]"), ConfigManager::Instance()->IsGoogler()));

  NamedObjectAttributes single_core_attr;
  GetNamedObjectAttributes(kCoreSingleInstance, is_system, &single_core_attr);
  ProgramInstance instance(single_core_attr.name);
  bool is_already_running = !instance.EnsureSingleInstance();
  if (is_already_running) {
    OPT_LOG(L1, (_T("[another core instance is already running]")));
    return S_OK;
  }

  // TODO(omaha): the user Omaha core should run at medium integrity level and
  // it should deelevate itself if it does not, see bug 1549842.

  // Clean up the initial install directory and ignore the errors.
  HRESULT hr = CleanUpInitialManifestDirectory();
  if (FAILED(hr)) {
    CORE_LOG(LW, (_T("[CleanUpInitialManifestDirectory failed][0x%08x]"), hr));
  }

  // TODO(Omaha): Delay starting update worker when run at startup.
  StartUpdateWorker();

  // Start the crash handler if necessary.
  if (is_crash_handler_enabled_) {
    HRESULT hr = StartCrashHandler();
    if (FAILED(hr)) {
      OPT_LOG(LW, (_T("[Failed to start crash handler][0x%08x]"), hr));
    }
  }

  // The scheduled task will start the Update Worker at intervals. If the task
  // is not installed, then Omaha uses the built-in scheduler hosted by the core
  // and it keeps the core running. In addition, for the machine GoogleUpdate,
  // if the service is not installed, then Omaha uses the elevator interface
  // hosted by the core, and this keeps the core running.
  if (goopdate_utils::IsInstalledGoopdateTaskUA(is_system_)) {
    if (!is_system_) {
      return S_OK;
    }

    if (goopdate_utils::IsServiceInstalled()) {
      return S_OK;
    }
    ++metric_core_run_service_missing;
  } else {
    ++metric_core_run_scheduled_task_missing;
  }

  // Force the main thread to create a message queue so any future WM_QUIT
  // message posted by the ShutdownHandler will be received. If the main
  // thread does not have a message queue, the message can be lost.
  MSG msg = {0};
  ::PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);

  reactor_.reset(new Reactor);
  shutdown_handler_.reset(new ShutdownHandler);
  hr = shutdown_handler_->Initialize(reactor_.get(), this, is_system_);
  if (FAILED(hr)) {
    return hr;
  }

  if (!is_system_) {
    // We watch the legacy manifest install directory only if we are the
    // user core. The omaha1 -> omaha2 machine hand off occurs using the
    // /UI cmd line switch.
    VERIFY1(SUCCEEDED(InitializeManifestDirectoryWatcher()));
  }

  scheduler_.reset(new Scheduler(*this));
  hr = scheduler_->Initialize();
  if (FAILED(hr)) {
    return hr;
  }

  system_monitor_.reset(new SystemMonitor(is_system_));
  VERIFY1(SUCCEEDED(system_monitor_->Initialize(true)));
  system_monitor_->set_observer(this);

  if (is_system_) {
     VERIFY(SUCCEEDED(RegisterCoreProxy()),
            (_T("The core may have been started when the registered version ")
             _T("of Google Update does not exist or one is not registered.")));
  }

  // Start processing messages and events from the system.
  hr = DoRun();

  if (is_system) {
    UnregisterCoreProxy();
  }

  return hr;
}

// Signals the core to shutdown. The shutdown method is called by a thread
// running in the thread pool. It posts a WM_QUIT to the main thread, which
// causes it to break out of the message loop. If the message can't be posted,
// it terminates the process unconditionally.
HRESULT Core::Shutdown() {
  OPT_LOG(L1, (_T("[Google Update is shutting down...]")));
  ASSERT1(::GetCurrentThreadId() != main_thread_id_);
  if (::PostThreadMessage(main_thread_id_, WM_QUIT, 0, 0)) {
    return S_OK;
  }

  ASSERT(false, (_T("Failed to post WM_QUIT")));
  uint32 exit_code = static_cast<uint32>(E_ABORT);
  VERIFY1(::TerminateProcess(::GetCurrentProcess(), exit_code));
  return S_OK;
}

void Core::LastCheckedDeleted() {
  OPT_LOG(L1, (_T("[Core::LastCheckedDeleted]")));
  VERIFY1(SUCCEEDED(StartUpdateWorker()));
}

void Core::NoRegisteredClients() {
  OPT_LOG(L1, (_T("[Core::NoRegisteredClients]")));
  VERIFY1(SUCCEEDED(StartUpdateWorker()));
}

HRESULT Core::DoRun() {
  OPT_LOG(L1, (_T("[Core::DoRun]")));

  // Trim the process working set to minimum. It does not need a more complex
  // algorithm for now. Likely the working set will increase slightly over time
  // as the core is handling events.
  VERIFY1(::SetProcessWorkingSetSize(::GetCurrentProcess(),
                                     static_cast<uint32>(-1),
                                     static_cast<uint32>(-1)));
  return DoHandleEvents();
}

HRESULT Core::DoHandleEvents() {
  CORE_LOG(L1, (_T("[Core::DoHandleEvents]")));
  MSG msg = {0};
  int result = 0;
  while ((result = ::GetMessage(&msg, 0, 0, 0)) != 0) {
    ::DispatchMessage(&msg);
    if (result == -1) {
      break;
    }
  }
  CORE_LOG(L3, (_T("[GetMessage returned %d]"), result));
  return (result != -1) ? S_OK : HRESULTFromLastError();
}

HRESULT Core::StartUpdateWorker() const {
  // The uninstall check is tentative. There are stronger checks, protected
  // by locks, which are done by the worker process.
  size_t num_clients(0);
  const bool is_uninstall =
      FAILED(goopdate_utils::GetNumClients(is_system_, &num_clients)) ||
      num_clients <= 1;

  CORE_LOG(L2, (_T("[Core::StartUpdateWorker][%u]"), num_clients));

  CString exe_path = goopdate_utils::BuildGoogleUpdateExePath(is_system_);
  CommandLineBuilder builder(COMMANDLINE_MODE_UA);
  builder.set_is_uninstall_set(is_uninstall);
  CString cmd_line = builder.GetCommandLineArgs();
  HRESULT hr = System::StartProcessWithArgs(exe_path, cmd_line);
  if (SUCCEEDED(hr)) {
    ++metric_core_worker_succeeded;
  } else {
    CORE_LOG(LE, (_T("[can't start update worker][0x%08x]"), hr));
  }
  ++metric_core_worker_total;
  return hr;
}

HRESULT Core::StartCodeRed() const {
  if (RegKey::HasValue(MACHINE_REG_UPDATE_DEV, kRegValueNoCodeRedCheck)) {
    CORE_LOG(LW, (_T("[Code Red is disabled for this system]")));
    return E_ABORT;
  }

  CORE_LOG(L2, (_T("[Core::StartCodeRed]")));

  CString exe_path = goopdate_utils::BuildGoogleUpdateExePath(is_system_);
  CommandLineBuilder builder(COMMANDLINE_MODE_CODE_RED_CHECK);
  CString cmd_line = builder.GetCommandLineArgs();
  HRESULT hr = System::StartProcessWithArgs(exe_path, cmd_line);
  if (SUCCEEDED(hr)) {
    ++metric_core_cr_succeeded;
  } else {
    CORE_LOG(LE, (_T("[can't start Code Red worker][0x%08x]"), hr));
  }
  ++metric_core_cr_total;
  return hr;
}

HRESULT Core::StartCrashHandler() const {
  CORE_LOG(L2, (_T("[Core::StartCrashHandler]")));

  CString exe_path = goopdate_utils::BuildGoogleUpdateServicesPath(is_system_);
  CommandLineBuilder builder(COMMANDLINE_MODE_CRASH_HANDLER);
  CString cmd_line = builder.GetCommandLineArgs();
  HRESULT hr = System::StartProcessWithArgs(exe_path, cmd_line);
  if (SUCCEEDED(hr)) {
    ++metric_core_start_crash_handler_succeeded;
  } else {
    CORE_LOG(LE, (_T("[can't start Crash Handler][0x%08x]"), hr));
  }
  ++metric_core_start_crash_handler_total;
  return hr;
}

HRESULT Core::InitializeManifestDirectoryWatcher() {
  // We watch the legacy manifest install directory only if we are the
  // user core. The omaha1 -> omaha2 machine hand off occurs using the
  // /UI cmd line switch.
  legacy_manifest_handler_.reset(new LegacyManifestHandler());
  return legacy_manifest_handler_->Initialize(this);
}

HRESULT Core::StartInstallWorker() {
  // Get all the manifests that are present in the handoff directory.
  CString manifest_dir =
      ConfigManager::Instance()->GetUserInitialManifestStorageDir();
  std::vector<CString> manifests;
  HRESULT hr = File::GetWildcards(manifest_dir, _T("*.gup"), &manifests);
  if (FAILED(hr)) {
    return hr;
  }

  std::vector<CString>::iterator it;
  for (it = manifests.begin(); it != manifests.end(); ++it) {
    // Launch the worker using /UIUser manifest_file.
    CString filename = *it;
    EnclosePath(&filename);

    CommandLineBuilder builder(COMMANDLINE_MODE_LEGACY_MANIFEST_HANDOFF);
    builder.set_legacy_manifest_path(filename);
    CString cmd_line = builder.GetCommandLineArgs();
    HRESULT hr_ret = goopdate_utils::StartGoogleUpdateWithArgs(is_system_,
                                                               cmd_line,
                                                               NULL);
    if (FAILED(hr_ret)) {
      OPT_LOG(LE, (_T("[StartGoogleUpdateWithArgs failed][0x%08x]"), hr_ret));
      hr = hr_ret;
    }
  }

  return hr;
}

HRESULT Core::CleanUpInitialManifestDirectory() {
  CORE_LOG(L2, (_T("[CleanUpInitialManifestDirectory]")));

  const CString dir =
      ConfigManager::Instance()->GetUserInitialManifestStorageDir();
  if (dir.IsEmpty()) {
    return GOOPDATE_E_CORE_INTERNAL_ERROR;
  }
  return DeleteDirectoryFiles(dir);
}

void Core::AggregateMetrics() const {
  CORE_LOG(L2, (_T("[aggregate core metrics]")));
  CollectMetrics();
  VERIFY1(SUCCEEDED(omaha::AggregateMetrics(is_system_)));
}

// Collects: working set, peak working set, handle count, process uptime,
// user disk free space on the current drive, process kernel time, and process
// user time.
void Core::CollectMetrics() const {
  uint64 working_set(0), peak_working_set(0);
  VERIFY1(SUCCEEDED(System::GetProcessMemoryStatistics(&working_set,
                                                       &peak_working_set,
                                                       NULL,
                                                       NULL)));
  metric_core_working_set      = working_set;
  metric_core_peak_working_set = peak_working_set;

  metric_core_handle_count = System::GetProcessHandleCount();

  FILETIME now = {0};
  FILETIME creation_time = {0};
  FILETIME exit_time = {0};
  FILETIME kernel_time = {0};
  FILETIME user_time = {0};

  ::GetSystemTimeAsFileTime(&now);

  VERIFY1(::GetProcessTimes(::GetCurrentProcess(),
                            &creation_time,
                            &exit_time,
                            &kernel_time,
                            &user_time));

  ASSERT1(FileTimeToInt64(now) >= FileTimeToInt64(creation_time));
  uint64 uptime_100ns = FileTimeToInt64(now) - FileTimeToInt64(creation_time);

  metric_core_uptime_ms      = uptime_100ns / kMillisecsTo100ns;
  metric_core_kernel_time_ms = FileTimeToInt64(kernel_time) / kMillisecsTo100ns;
  metric_core_user_time_ms   = FileTimeToInt64(user_time) / kMillisecsTo100ns;

  uint64 free_bytes_current_user(0);
  uint64 total_bytes_current_user(0);
  uint64 free_bytes_all_users(0);

  CString directory_name(app_util::GetCurrentModuleDirectory());
  VERIFY1(SUCCEEDED(System::GetDiskStatistics(directory_name,
                                              &free_bytes_current_user,
                                              &total_bytes_current_user,
                                              &free_bytes_all_users)));
  metric_core_disk_space_available = free_bytes_current_user;
}

HRESULT Core::RegisterCoreProxy() {
  CComObjectNoLock<GoogleUpdateCore>* google_update_core =
      new CComObjectNoLock<GoogleUpdateCore>;

  CDacl dacl;
  dacl.AddAllowedAce(Sids::System(), GENERIC_ALL);
  dacl.AddAllowedAce(Sids::Users(), GENERIC_READ);
  CSecurityDesc sd;
  sd.SetDacl(dacl);
  sd.MakeAbsolute();

  SharedMemoryAttributes attr(kGoogleUpdateCoreSharedMemoryName, sd);
  google_update_core_proxy_.reset(new GoogleUpdateCoreProxy(false, &attr));

#if DEBUG
  // The core interface should be registered only once.
  CComPtr<IGoogleUpdateCore> core_interface;
  HRESULT hr = google_update_core_proxy_->GetObject(&core_interface);
  ASSERT1(FAILED(hr) || !core_interface);
#endif

  return google_update_core_proxy_->RegisterObject(google_update_core);
}

void Core::UnregisterCoreProxy() {
  if (google_update_core_proxy_.get()) {
    google_update_core_proxy_->RevokeObject();
  }
}

}  // namespace omaha

