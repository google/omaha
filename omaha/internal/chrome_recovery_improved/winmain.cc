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
// Probe that gets delivered to machines via Chrome's recovery component.

#include <windows.h>

#include <memory>

#include "omaha/base/const_object_names.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/omaha_version.h"
#include "base/program_instance.h"
#include "omaha/base/utils.h"
#include "omaha/common/command_line.h"
#include "omaha/common/crash_utils.h"
#include "omaha/common/exception_handler.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/internal/chrome_recovery_improved/command_line.h"
#include "omaha/internal/chrome_recovery_improved/recovery.h"
#include "omaha/net/network_config.h"
#include "omaha/third_party/smartany/scoped_any.h"

namespace {

// Called by operator new or operator new[] when they cannot satisfy a request
// for additional storage.
void OutOfMemoryHandler() {
  ::RaiseException(EXCEPTION_ACCESS_VIOLATION,
                   EXCEPTION_NONCONTINUABLE,
                   0,
                   NULL);
}

}  // namespace

namespace omaha {

int ChromeRecoveryImprovedMain() {
  OPT_LOG(L3, (_T("[ChromeRecoveryMain]")));

  // Initialize the command line for this process.
  CommandLine::Init(0, NULL);
  const CommandLine* cl = CommandLine::ForCurrentProcess();
  ASSERT1(cl);
  OPT_LOG(L3, (_T("[command line][%s]"), cl->GetCommandLineString().c_str()));

  const bool is_machine = cl->HasSwitch(_T("system"));
  const CString app_guid = cl->GetSwitchValue(_T("appguid")).c_str();
  const CString browser_version = cl->GetSwitchValue(
      _T("browser-version")).c_str();
  const CString session_id = cl->GetSwitchValue(_T("sessionid")).c_str();

  CustomInfoMap custom_info_map;
  CString command_line_mode;
  SafeCStringFormat(&command_line_mode, _T("%d"), COMMANDLINE_MODE_RECOVER);
  custom_info_map[kCrashCustomInfoCommandLineMode] = command_line_mode;

  std::unique_ptr<OmahaExceptionHandler> crash_handler;
  VERIFY_SUCCEEDED(OmahaExceptionHandler::Create(is_machine,
                                                  custom_info_map,
                                                  &crash_handler));
  NamedObjectAttributes attrs;
  GetNamedObjectAttributes(kRecoveryProbeSingleInstance, is_machine, &attrs);
  ProgramInstance instance(attrs.name);
  const bool is_already_running = !instance.EnsureSingleInstance();
  if (is_already_running) {
    OPT_LOG(L1, (_T("[Another recovery probe is already running]")));
    return GOOPDATE_E_PROBE_ALREADY_RUNNING;
  }

  // Initialize the network.
  NetworkConfigManager::set_is_machine(is_machine);
  NetworkConfigManager::Instance();

  return omaha::ChromeRecoveryImproved(is_machine,
                                       app_guid,
                                       browser_version,
                                       session_id).Repair();
}

}  // namespace omaha

int WINAPI _tWinMain(HINSTANCE instance, HINSTANCE, LPTSTR, int) {
  omaha::EnableSecureDllLoading();

  // Install an error-handling mechanism which gets called when new operator
  // fails to allocate memory.
  VERIFY1(set_new_handler(&OutOfMemoryHandler) == 0);

  omaha::InitializeShellVersion();
  omaha::InitializeVersionFromModule(instance);

  return omaha::ChromeRecoveryImprovedMain();
}
