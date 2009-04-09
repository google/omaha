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
// run.

#include "omaha/core/crash_handler.h"
#include "omaha/common/const_object_names.h"
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/logging.h"
#include "omaha/common/reactor.h"
#include "omaha/common/shutdown_handler.h"
#include "omaha/common/utils.h"
#include "omaha/goopdate/config_manager.h"
#include "omaha/goopdate/crash.h"
#include "omaha/goopdate/program_instance.h"

namespace omaha {

CrashHandler::CrashHandler()
    : is_system_(false),
      main_thread_id_(0) {
  CORE_LOG(L1, (_T("[CrashHandler::CrashHandler]")));
}

CrashHandler::~CrashHandler() {
  CORE_LOG(L1, (_T("[CrashHandler::~CrashHandler]")));
  Crash::StopServer();
}

HRESULT CrashHandler::Main(bool is_system) {
  if (!ConfigManager::Instance()->CanCollectStats(is_system)) {
    return S_OK;
  }

  main_thread_id_ = ::GetCurrentThreadId();
  is_system_ = is_system;

  NamedObjectAttributes single_CrashHandler_attr;
  GetNamedObjectAttributes(kCrashHandlerSingleInstance,
                           is_system,
                           &single_CrashHandler_attr);
  ProgramInstance instance(single_CrashHandler_attr.name);
  bool is_already_running = !instance.EnsureSingleInstance();
  if (is_already_running) {
    OPT_LOG(L1, (_T("[another CrashHandler instance is already running]")));
    return S_OK;
  }

  // Start the crash handler.
  HRESULT hr = Crash::StartServer();
  if (FAILED(hr)) {
    OPT_LOG(LW, (_T("[Failed to start crash handler][0x%08x]"), hr));
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
  return DoRun();
}

// Signals the CrashHandler to shutdown. The shutdown method is called by a
// thread running in the thread pool. It posts a WM_QUIT to the main thread,
// which causes it to break out of the message loop. If the message can't be
// posted, it terminates the process unconditionally.
HRESULT CrashHandler::Shutdown() {
  OPT_LOG(L1, (_T("[CrashHandler::Shutdown]")));
  ASSERT1(::GetCurrentThreadId() != main_thread_id_);
  if (::PostThreadMessage(main_thread_id_, WM_QUIT, 0, 0)) {
    return S_OK;
  }

  ASSERT(false, (_T("Failed to post WM_QUIT")));
  uint32 exit_code = static_cast<uint32>(E_ABORT);
  VERIFY1(::TerminateProcess(::GetCurrentProcess(), exit_code));
  return S_OK;
}

HRESULT CrashHandler::DoRun() {
  OPT_LOG(L1, (_T("[CrashHandler::DoRun]")));

  // Trim the process working set to minimum. It does not need a more complex
  // algorithm for now. Likely the working set will increase slightly over time
  // as the CrashHandler is handling events.
  VERIFY1(::SetProcessWorkingSetSize(::GetCurrentProcess(),
                                     static_cast<uint32>(-1),
                                     static_cast<uint32>(-1)));
  return DoHandleEvents();
}

HRESULT CrashHandler::DoHandleEvents() {
  CORE_LOG(L1, (_T("[CrashHandler::DoHandleEvents]")));
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

}  // namespace omaha

