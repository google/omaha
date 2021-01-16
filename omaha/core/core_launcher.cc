// Copyright 2019 Google Inc.
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

#include "omaha/core/core_launcher.h"

#include <cstdint>

#include "omaha/base/constants.h"
#include "omaha/base/debug.h"
#include "omaha/base/logging.h"
#include "omaha/base/reg_key.h"
#include "omaha/common/command_line_builder.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/third_party/smartany/scoped_any.h"

namespace omaha {

namespace {

time64 GetLastCoreRunTimeMs(bool is_machine) {
  const wchar_t* reg_update_key(is_machine ? MACHINE_REG_UPDATE
                                           : USER_REG_UPDATE);
  time64 last_core_run_time = 0;
  if (SUCCEEDED(RegKey::GetValue(reg_update_key, kRegValueLastCoreRun,
                                 &last_core_run_time))) {
    return last_core_run_time;
  }

  return 0;
}

time64 GetTimeSinceLastCoreRunMs(bool is_machine) {
  const time64 now = GetCurrentMsTime();
  const time64 last_core_run = GetLastCoreRunTimeMs(is_machine);

  if (now < last_core_run) {
    return UINT64_MAX;
  }

  return now - last_core_run;
}

HRESULT UpdateLastCoreRunTime(bool is_machine) {
  const time64 now = GetCurrentMsTime();
  const wchar_t* reg_update_key(is_machine ? MACHINE_REG_UPDATE
                                           : USER_REG_UPDATE);
  return RegKey::SetValue(reg_update_key, kRegValueLastCoreRun, now);
}

}  // namespace

bool ShouldRunCore(bool is_system) {
  const time64 time_difference_ms = GetTimeSinceLastCoreRunMs(is_system);

  const bool result = time_difference_ms >= kMaxWaitBetweenCoreRunsMs;
  CORE_LOG(L3, (L"[ShouldRunCore][%d][%llu]", result, time_difference_ms));
  return result;
}

HRESULT StartCoreIfNeeded(bool is_system) {
  CORE_LOG(L3, (L"[Core::StartCoreIfNeeded][%d]", is_system));

  if (!ShouldRunCore(is_system)) {
    return S_OK;
  }

  CommandLineBuilder builder(omaha::COMMANDLINE_MODE_CORE);
  CString cmd_line(builder.GetCommandLineArgs());
  scoped_process process;
  HRESULT hr = goopdate_utils::StartGoogleUpdateWithArgs(is_system,
                                                         StartMode::kBackground,
                                                         cmd_line,
                                                         address(process));
  if (FAILED(hr)) {
    CORE_LOG(LE, (L"[Unable to start Core][%#x]", hr));
    return hr;
  }

  VERIFY_SUCCEEDED(UpdateLastCoreRunTime(is_system));
  return S_OK;
}

}  // namespace omaha
