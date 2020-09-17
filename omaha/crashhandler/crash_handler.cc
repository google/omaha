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

// The CrashHandler is a long-lived Omaha process. It runs one instance for the
// machine and one instance for each user session, including console and TS
// sessions. If the user has turned off crash reporting, this process will not
// run. For each crash the crash handler spawns a crash handler worker process
// which performs the minidump and crash analysis on the requesting process.
// The crash handler worker initially runs as the same user as its parent crash
// handler. It begins the process of generating the minidump and then, once
// all required handles have been opened performs a lockdown from inside a
// MinidumpWriteDump callback. Sandboxing is achieved by setting an untrusted
// label on the token which affectively prevents the process from opening any
// new handles. After lockdown the worker performs crash analysis and returns
// back to MinidumpWriteDump.

#include "omaha/crashhandler/crash_handler.h"

#include <atlbase.h>
#include <atlstr.h>
#include <map>

#include "omaha/base/const_object_names.h"
#include "omaha/base/debug.h"
#include "omaha/base/environment_block_modifier.h"
#include "omaha/base/error.h"
#include "omaha/base/file.h"
#include "omaha/base/logging.h"
#include "omaha/base/program_instance.h"
#include "omaha/base/reactor.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/scope_guard.h"
#include "omaha/base/shutdown_callback.h"
#include "omaha/base/shutdown_handler.h"
#include "omaha/base/string.h"
#include "omaha/base/system.h"
#include "omaha/base/time.h"
#include "omaha/base/utils.h"
#include "omaha/common/command_line_builder.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/crash_utils.h"
#include "omaha/common/exception_handler.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/common/stats_uploader.h"
#include "omaha/crashhandler/crashhandler_metrics.h"
#include "omaha/crashhandler/crash_dump_util.h"
#include "omaha/crashhandler/crash_worker.h"
#include "omaha/third_party/smartany/scoped_any.h"
#include "third_party/breakpad/src/client/windows/common/ipc_protocol.h"
#include "third_party/breakpad/src/client/windows/crash_generation/client_info.h"
#include "third_party/breakpad/src/client/windows/crash_generation/crash_generation_server.h"
#include "third_party/breakpad/src/common/windows/guid_string.h"

#ifdef _WIN64
#define EXE_ARCH 64
#else
#define EXE_ARCH 32
#endif

using google_breakpad::CustomInfoEntry;

namespace omaha {

const TCHAR* const kLaunchedForMinidump = _T("CrashHandlerLaunchedForMinidump");

CrashHandler::CrashHandler()
    : is_system_(false),
      main_thread_id_(0) {
  CORE_LOG(L1, (_T("[CrashHandler::CrashHandler][%d-bit]"), EXE_ARCH));
  stats_report::g_global_metrics.Initialize();
}

CrashHandler::~CrashHandler() {
  CORE_LOG(L1, (_T("[CrashHandler::~CrashHandler][%d-bit]"), EXE_ARCH));
  StopServer();
  stats_report::g_global_metrics.Uninitialize();
}

HRESULT CrashHandler::Main(bool is_system) {
  main_thread_id_ = ::GetCurrentThreadId();
  is_system_ = is_system;

  if (IsRunningAsMinidumpHandler()) {
    CORE_LOG(L1, (_T("[CrashHandler runs as minidump handler]")));
    return RunAsCrashHandlerWorker();
  } else {
    CORE_LOG(L1, (_T("[CrashHandler runs as singleton][%d]"), is_system));

    // Before we do anything substantial, attempt to install the in-proc crash
    // handler.
    CustomInfoMap custom_info_map;
    CString command_line_mode;
    SafeCStringFormat(&command_line_mode, _T("%d"),
                      COMMANDLINE_MODE_CRASH_HANDLER);
    custom_info_map[kCrashCustomInfoCommandLineMode] = command_line_mode;

    VERIFY_SUCCEEDED(
        OmahaExceptionHandler::Create(is_system,
                                      custom_info_map,
                                      &exception_handler_));

    // Are we allowed to monitor crashes?
    if (!ConfigManager::Instance()->CanCollectStats(is_system)) {
        CORE_LOG(L1, (_T("[CrashHandler][Stats disabled][%d-bit][%d]"),
                      EXE_ARCH, is_system));
      return S_OK;
    }

    return RunAsCrashHandler();
  }
}

// The process is launched to handle one particular crash.
HRESULT CrashHandler::RunAsCrashHandlerWorker() {
  // When crash handler fails to generate minidump, it may skip setting the
  // notification event. The notification receiver should wait on the handler
  // process as well to avoid unnecessary wait.
  scoped_event notification_event;
  scoped_handle mini_dump_handle;
  scoped_handle full_dump_handle;
  scoped_handle custom_info_handle;
  std::unique_ptr<google_breakpad::ClientInfo> client_info;
  HRESULT hr = GetCrashInfoFromEnvironmentVariables(address(notification_event),
                                                    address(mini_dump_handle),
                                                    address(full_dump_handle),
                                                    address(custom_info_handle),
                                                    &client_info);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[Failed to get crash info from environment.]")));
    return hr;
  }

  DWORD pid = client_info->pid();
  OPT_LOG(L1, (_T("[CrashHandler][Preparing dump][%d-bit][pid %d]"),
      EXE_ARCH, pid));

  std::unique_ptr<CrashAnalyzer> analyzer(new CrashAnalyzer(*client_info));
  if (!analyzer->Init()) {
    analyzer.release();
  }

  MinidumpCallbackParameter callback_parameter = {0};
  std::map<CString, CString> custom_info_map;
  callback_parameter.custom_info_map = &custom_info_map;
  callback_parameter.crash_analyzer = analyzer.get();

  // Depends on the dump type requested by the client, the dump generator could
  // also generate full dump. Get the file name so we can delete it.
  hr = GenerateMinidump(is_system_,
                        *client_info,
                        get(mini_dump_handle),
                        get(full_dump_handle),
                        &callback_parameter);
  if (FAILED(hr)) {
    return hr;
  }

  if (!client_info->PopulateCustomInfo()) {
    CORE_LOG(LE, (_T("[CrashHandler][PopulateCustomInfo failed]")));
    return E_FAIL;
  }

  hr = crash_utils::ConvertCustomClientInfoToMap(
      client_info->GetCustomInfo(), &custom_info_map);
  if (FAILED(hr)) {
    ASSERT1(custom_info_map.empty());
    return hr;
  }

  hr = crash_utils::WriteCustomInfoFile(get(custom_info_handle),
                                        custom_info_map);
  if (FAILED(hr)) {
    return hr;
  }

  VERIFY1(::SetEvent(get(notification_event)));

  CORE_LOG(L1, (_T("[CrashHandler][Dump handled][%d-bit][pid %d][issystem %d]"),
                EXE_ARCH, pid, is_system_));
  return S_OK;
}

HRESULT CrashHandler::RunAsCrashHandler() {
  // Ensure that there's no other crash handler running.
  NamedObjectAttributes single_CrashHandler_attr;
  GetNamedObjectAttributes(GetCrashHandlerInstanceName(),
                           is_system_,
                           &single_CrashHandler_attr);
  ProgramInstance instance(single_CrashHandler_attr.name);
  bool is_already_running = !instance.EnsureSingleInstance();
  if (is_already_running) {
    OPT_LOG(L1, (_T("[CrashHandler][Instance is already running][%d-bit][%d]"),
                EXE_ARCH, is_system_));
    return S_OK;
  }

  // Create the minidump storage directory if it doesn't already exist.
  HRESULT hr = crash_utils::InitializeCrashDir(is_system_, &crash_dir_);
  if (FAILED(hr)) {
    OPT_LOG(LW, (_T("[CrashHandler][Failed to init crash dir][0x%08x]"), hr));
    return hr;
  }

  if (FAILED(hr)) {
    OPT_LOG(LW,
        (_T("[CrashHandler][Failed to initialize worker desktop][0x%08x]"),
        hr));
  }

  // Start the crash handler.
  hr = StartServer();
  if (FAILED(hr)) {
    OPT_LOG(LW, (_T("[CrashHandler][Failed to start Breakpad][0x%08x]"), hr));
    return hr;
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

  // Start processing messages and events from the system.
  return RunUntilShutdown();
}

// Creates a child process with equivalent privilege as the crashed process
// owner to generate and upload minidump.
HRESULT CrashHandler::CreateCrashHandlerProcess(
    HANDLE notification_event,
    HANDLE mini_dump_handle,
    HANDLE full_dump_handle,
    HANDLE custom_info_handle,
    const google_breakpad::ClientInfo& client_info,
    PROCESS_INFORMATION* pi) {
  HRESULT hr = S_OK;
  CString crash_handler_path;

  // The command is same as current main module.
  DWORD len = ::GetModuleFileName(NULL,
                                  CStrBuf(crash_handler_path, MAX_PATH),
                                  MAX_PATH);
  if (len == 0) {
    hr = HRESULTFromLastError();
    CORE_LOG(LE, (_T("[Failed to get crash handler path.[0x%x]"), hr));
    return hr;
  }

  EnvironmentBlockModifier eb_mod;
  // Set a flag in the environment variable so the child process knows it is
  // created for a particular minidump.
  eb_mod.SetVar(kLaunchedForMinidump, _T("True"));

  // Pass crash process information via environment variables.
  hr = SetCrashInfoToEnvironmentBlock(&eb_mod,
                                      notification_event,
                                      mini_dump_handle,
                                      full_dump_handle,
                                      custom_info_handle,
                                      client_info);
  if (FAILED(hr)) {
    return hr;
  }

  std::vector<TCHAR> env_block;
  eb_mod.Create(_T(""), &env_block);

  // TODO(omaha): Need to handle the case: process creation fails when crash
  // handler is running as elevated user mode but the crashed process runs by
  // the same user but un-elevated. Also need to set ACLs for the crash
  // handler pipe in this case.
  hr = System::StartProcessWithEnvironment(
      NULL,
      CStrBuf(crash_handler_path, MAX_PATH),
      &env_block.front(),
      true,
      pi);
  CORE_LOG(L1, (_T("[StartProcessAsUserWithEnvironment.][command=%s][0x%x]"),
                crash_handler_path, hr));
  return hr;
}

CString CrashHandler::GetCrashHandlerInstanceName() {
  CString instance = kCrashHandlerSingleInstance;
#ifdef _WIN64
  instance.Append(kObjectName64Suffix);
#endif
  return instance;
}

HRESULT CrashHandler::StartServer() {
  CORE_LOG(L1, (_T("[CrashHandler::StartServer][%d-bit][is_system %d]"),
               EXE_ARCH, is_system_));
  ++metric_crash_start_server_total;

  std::wstring dump_path(crash_dir_);

  CString pipe_name;
  HRESULT hr = crash_utils::GetCrashPipeName(&pipe_name);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[GetCrashPipeName() failed][0x%08x]"), hr));
    return hr;
  }
  UTIL_LOG(L6, (_T("[listening on crash pipe][%s]"), pipe_name));

  // https://bit.ly/3fygY37
  // The ACLs in the default security descriptor for a named pipe grant full
  // control to the LocalSystem account, administrators, and the creator owner.
  // They also grant read access to members of the Everyone group and the
  // anonymous account.
  crash_server_.reset(new google_breakpad::CrashGenerationServer(
      std::wstring(pipe_name),
      NULL,
      BreakpadClientConnected, NULL,
      BreakpadClientCrashed, this,
      BreakpadClientDisconnected, NULL,
      BreakpadClientUpload, this,
      false,  // Dumps are generated in a new sandboxed process
      &dump_path));
  if (!crash_server_.get() || !crash_server_->Start()) {
    CORE_LOG(LE, (_T("[CrashServer::Start failed]")));
    crash_server_.reset();
    return GOOPDATE_E_CRASH_START_SERVER_FAILED;
  }

  ++metric_crash_start_server_succeeded;
  return S_OK;
}

void CrashHandler::StopServer() {
  CORE_LOG(L1, (_T("[CrashHandler::StopServer][%d-bit][is_system %d]"),
               EXE_ARCH, is_system_));
  crash_server_.reset();
}

HRESULT CrashHandler::StartCrashUploader(
    const CString& crash_filename,
    const CString& custom_info_filename) {
  // Normally, we only aggregate metrics at process exit. Since the crash
  // handler is long-running, however, we hardly ever exit. This call will do
  // additional aggregation, so that we can report metrics in a timely manner.
  ON_SCOPE_EXIT(AggregateMetrics, is_system_);

  // Count the number of crashes requested by applications.
  ++metric_oop_crashes_requested;

  ASSERT1(!crash_filename.IsEmpty());
  if (crash_filename.IsEmpty()) {
    OPT_LOG(L1, (_T("[No crash file]")));
    ++metric_oop_crashes_crash_filename_empty;
    return E_UNEXPECTED;
  }

  // Start a sender process to handle the crash.
  HRESULT hr = crash_utils::StartCrashReporter(false,  // is_interactive
                                               is_system_,
                                               crash_filename,
                                               &custom_info_filename);
  if (FAILED(hr)) {
    OPT_LOG(LE, (_T("[StartCrashReporter failed][0x%08x]"), hr));
    ++metric_oop_crashes_startsenderwithcommandline_failed;
    return hr;
  }

  return S_OK;
}

void CrashHandler::BreakpadClientConnected(
    void* context,
    const google_breakpad::ClientInfo* client_info) {
  ASSERT1(!context);
  ASSERT1(client_info);

  UNREFERENCED_PARAMETER(context);
  UNREFERENCED_PARAMETER(client_info);
  CORE_LOG(L1, (_T("[CrashHandler][Client connected][%d-bit][pid %d]"),
               EXE_ARCH, client_info->pid()));
}

void CrashHandler::BreakpadClientCrashed(
    void* context,
    const google_breakpad::ClientInfo* client_info,
    const std::wstring* dump_path) {
  ASSERT1(context);
  ASSERT1(client_info);
  CString crash_filename(dump_path ? dump_path->c_str() : NULL);
  CORE_LOG(L1, (_T("[CrashHandler][Client crashed][%d-bit][pid %d]"),
               EXE_ARCH, client_info->pid()));

  CrashHandler* handler = reinterpret_cast<CrashHandler*>(context);
  handler->CleanStaleCrashes();

  CString dump_file_path = handler->GenerateDumpFilePath();

  CString full_dump_file_path(_T(""));
  bool full_dump = (client_info->dump_type() & MiniDumpWithFullMemory) != 0;
  if (full_dump) {
    // strip .dmp extension.
    full_dump_file_path = dump_file_path.Left(dump_file_path.GetLength() - 4);
    full_dump_file_path += _T("-full.dmp");
  }

  SECURITY_ATTRIBUTES security_attributes = {0};
  security_attributes.nLength = sizeof(security_attributes);
  security_attributes.bInheritHandle = true;
  scoped_handle mini_dump_handle(::CreateFile(dump_file_path,
                                              GENERIC_WRITE,
                                              0,
                                              &security_attributes,
                                              CREATE_NEW,
                                              FILE_ATTRIBUTE_NORMAL,
                                              NULL));
  scoped_handle full_dump_handle;
  if (full_dump) {
    reset(full_dump_handle, ::CreateFile(full_dump_file_path,
                                         GENERIC_WRITE,
                                         0,
                                         &security_attributes,
                                         CREATE_NEW,
                                         FILE_ATTRIBUTE_NORMAL,
                                         NULL));
  }

  scoped_handle custom_info_handle;
  HRESULT hr = OpenCustomMapFile(dump_file_path, address(custom_info_handle));
  if (FAILED(hr)) {
    // Log if this call fails, but continue handling the crash otherwise.
    OPT_LOG(LE, (_T("[OpenCustomInfoFile failed][0x%08x]"), hr));
  }

  // Launch a separate process with the privilege of the crash client to
  // generate and upload the crash.
  CrashHandlerResult result =
      handler->CreateCrashHandlingProcessAndWait(*client_info,
                                                 get(mini_dump_handle),
                                                 get(full_dump_handle),
                                                 get(custom_info_handle));
  reset(mini_dump_handle);
  reset(custom_info_handle);
  reset(full_dump_handle);
  if (result == CRASH_HANDLER_ERROR) {
    OPT_LOG(LE, (_T("[CrashHandler minidump generation failed]")));
    return;
  }
  std::map<CString, CString> custom_info_map;
  hr = crash_utils::ConvertCustomClientInfoToMap(
      client_info->GetCustomInfo(), &custom_info_map);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[ConvertCustomClientInfoToMap failed][0x%x]"), hr));
    ++metric_oop_crashes_convertcustomclientinfotomap_failed;
    ASSERT1(custom_info_map.empty());
    return;
  }
  bool defer_upload = crash_utils::IsUploadDeferralRequested(custom_info_map);
  if (defer_upload) {
    // The dump had been generated and upload deferral was requested.
    handler->saved_crashes_[client_info->crash_id()] = CString(dump_file_path);
    OPT_LOG(LE, (_T("[CrashHandler][Upload deferred][Crash ID %d]"),
                client_info->crash_id()));
    ++metric_oop_crashes_deferred;
    return;
  } else {
    CString custom_info_filename;
    crash_utils::GetCustomInfoFilePath(dump_file_path, &custom_info_filename);
    hr = handler->StartCrashUploader(dump_file_path,
                                             custom_info_filename);
    if (FAILED(hr)) {
      OPT_LOG(LE, (_T("[StartCrashUploader() failed][0x%08x]"), hr));
    }
  }

  if (full_dump)
    ::DeleteFile(full_dump_file_path);

  OPT_LOG(L1, (_T("[CrashHandler][Dump handled][%d-bit][is_system %d]"),
              EXE_ARCH, handler->is_system_));
  return;
}

CrashHandler::CrashHandlerResult
    CrashHandler::CreateCrashHandlingProcessAndWait(
        const google_breakpad::ClientInfo& client_info,
        HANDLE mini_dump_handle,
        HANDLE full_dump_handle,
        HANDLE custom_info_handle) {
  // Create an event so the child process can notify that minidump is generated.
  HRESULT hr = S_OK;
  SECURITY_ATTRIBUTES security_attributes = {0};
  security_attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
  security_attributes.bInheritHandle = true;
  scoped_event notification_event(
      ::CreateEvent(&security_attributes, true, false, NULL));

  PROCESS_INFORMATION pi = {0};
  hr = CreateCrashHandlerProcess(get(notification_event),
                                 mini_dump_handle,
                                 full_dump_handle,
                                 custom_info_handle,
                                 client_info,
                                 &pi);
  if FAILED(hr) {
    CORE_LOG(LE, (_T("[Failed to create process for crash handling][0x%08X]"),
                  hr));
    return CRASH_HANDLER_ERROR;
  }

  const DWORD kWaitForCrashGenerationTimeoutMs = 5 * 60 * 1000;   // 5 minutes
  HANDLE handles_to_wait[] = { get(notification_event), pi.hProcess };
  const DWORD wait_result = ::WaitForMultipleObjects(
      arraysize(handles_to_wait),
      handles_to_wait,
      FALSE,
      kWaitForCrashGenerationTimeoutMs);
  switch (wait_result) {
    case WAIT_OBJECT_0:
      OPT_LOG(L1, (_T("[Child process signals that minidump is created.]")));
      return CRASH_HANDLER_SUCCESS;
    case WAIT_OBJECT_0 + 1: {
      DWORD exit_code = 0;
      CORE_LOG(LE, (_T("[Child process terminated before event signals.")));
      if (::GetExitCodeProcess(pi.hProcess, &exit_code)) {
        CORE_LOG(L1, (_T("[Child process exit code: %d]"), exit_code));
      }
      break;
    }
    case WAIT_TIMEOUT:
    case WAIT_FAILED:
    default: {
      CORE_LOG(LE, (_T("[Unexpected wait result.][%d]"), wait_result));
      break;
    }
  }
  return CRASH_HANDLER_ERROR;
}

void CrashHandler::BreakpadClientDisconnected(
    void* context,
    const google_breakpad::ClientInfo* client_info) {
  ASSERT1(!context);
  ASSERT1(client_info);

  UNREFERENCED_PARAMETER(context);
  UNREFERENCED_PARAMETER(client_info);
  CORE_LOG(L1, (_T("[CrashHandler][Client exited][%d-bit][%d]"),
               EXE_ARCH, client_info->pid()));
}

void CrashHandler::CleanStaleCrashes() {
  std::map<DWORD, CString>::iterator it = saved_crashes_.begin();
  while (it != saved_crashes_.end()) {
    CString filename = it->second;
    FILETIME creation_time = {0};
    if (FAILED(File::GetFileTime(filename, &creation_time, NULL, NULL))) {
      saved_crashes_.erase(it++);
      continue;
    }
    time64 now = GetCurrent100NSTime();
    time64 time_diff = now - FileTimeToTime64(creation_time);
    if (time_diff >= kDaysTo100ns) {
      // If more than a day has passed since the crash was processed
      // we no longer allow deferred upload and clean up the files.
      CString custom_data_file;
      crash_utils::GetCustomInfoFilePath(filename, &custom_data_file);
      ::DeleteFile(custom_data_file);
      ::DeleteFile(filename);
      saved_crashes_.erase(it++);
      OPT_LOG(L1, (_T
          ("[CrashHandler][Deleted Stale Crash][filename %s][custom data %s]"),
          filename, custom_data_file));
    } else {
      ++it;
    }
  }
}

void CrashHandler::BreakpadClientUpload(void* context, const DWORD crash_id) {
  ASSERT1(context);
  ASSERT1(crash_id);

  CrashHandler* handler = reinterpret_cast<CrashHandler*>(context);
  std::map<DWORD, CString>::iterator it =
      handler->saved_crashes_.find(crash_id);
  if (it == handler->saved_crashes_.end()) {
    CORE_LOG(L1, (_T("[CrashHandler][Deferred upload failed for ID][%d]"),
                 crash_id));
    return;
  }

  CString crash_filename = it->second;
  CString custom_info_filename;
  crash_utils::GetCustomInfoFilePath(crash_filename, &custom_info_filename);

  HRESULT hr = handler->StartCrashUploader(crash_filename,
                                           custom_info_filename);
  if (FAILED(hr)) {
    OPT_LOG(LE, (_T("[StartCrashUploader() failed][0x%08x]"), hr));
  }
}

HRESULT CrashHandler::RunUntilShutdown() {
  OPT_LOG(L1, (_T("[CrashHandler::RunUntilShutdown]")));

  // Trim the process working set to minimum. It does not need a more complex
  // algorithm for now. Likely the working set will increase slightly over time
  // as the CrashHandler is handling events.
  VERIFY_SUCCEEDED(System::EmptyProcessWorkingSet());

  // Pump messages.  If the shutdown event is set, a WM_QUIT will be posted to
  // this thread (from a thread pool thread running Shutdown(), below) to exit.
  MSG msg = {0};
  int result = 0;
  while ((result = ::GetMessage(&msg, 0, 0, 0)) != 0) {
    ::DispatchMessage(&msg);
    if (result == -1) {
      break;
    }
  }
  CORE_LOG(L3, (_T("[CrashHandler][GetMessage returned %d]"), result));
  return (result != -1) ? S_OK : HRESULTFromLastError();
}

HRESULT CrashHandler::Shutdown() {
  OPT_LOG(L1, (_T("[CrashHandler::Shutdown]")));
  ASSERT1(::GetCurrentThreadId() != main_thread_id_);
  if (::PostThreadMessage(main_thread_id_, WM_QUIT, 0, 0)) {
    return S_OK;
  }

  ASSERT(false, (_T("Crash handler failed to post WM_QUIT to itself")));

  // If, for some reason, we can't post the WM_QUIT message in the wild, resort
  // to forcibly terminating our own process.
  uint32 exit_code = static_cast<uint32>(E_ABORT);
  VERIFY1(::TerminateProcess(::GetCurrentProcess(), exit_code));
  return S_OK;
}


CString CrashHandler::GenerateDumpFilePath() {
  UUID id = {0};
  ::UuidCreate(&id);
  return CString(crash_dir_ + _T("\\") +
      google_breakpad::GUIDString::GUIDToWString(&id).c_str() + _T(".dmp"));
}

}  // namespace omaha
