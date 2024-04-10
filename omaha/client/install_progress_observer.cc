// Copyright 2024 Google Inc.
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

#include "omaha/client/install_progress_observer.h"

#include "omaha/base/debug.h"
#include "omaha/base/logging.h"
#include "omaha/common/goopdate_utils.h"

namespace omaha {

namespace {

HRESULT LaunchCommandLine(const AppCompletionInfo& app_info, bool is_machine) {
  CORE_LOG(L3, (_T("[LaunchCommandLine][%s]"),
                app_info.post_install_launch_command_line));
  if (app_info.post_install_launch_command_line.IsEmpty()) {
    return S_OK;
  }

  if (app_info.completion_code != COMPLETION_CODE_LAUNCH_COMMAND &&
      app_info.completion_code !=
          COMPLETION_CODE_EXIT_SILENTLY_ON_LAUNCH_COMMAND) {
    CORE_LOG(LW, (_T("Launch command line [%s] is not empty but completion ")
                  _T("code [%d] doesn't require a launch"),
                  app_info.post_install_launch_command_line.GetString(),
                  app_info.completion_code));
    return S_OK;
  }

  ASSERT1(SUCCEEDED(app_info.error_code));
  ASSERT1(!app_info.is_noupdate);

  HRESULT hr = goopdate_utils::LaunchCmdLine(
      is_machine, app_info.post_install_launch_command_line, NULL, NULL);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[goopdate_utils::LaunchCommandLine failed][0x%x]"), hr));
    return hr;
  }

  return S_OK;
}

}  // namespace

bool LaunchCommandLines(const ObserverCompletionInfo& info, bool is_machine) {
  bool  result = true;

  CORE_LOG(L3, (_T("[LaunchCommandLines]")));
  for (size_t i = 0; i < info.apps_info.size(); ++i) {
    const AppCompletionInfo& app_info = info.apps_info[i];
    if (FAILED(app_info.error_code)) {
      continue;
    }
    result &= SUCCEEDED(LaunchCommandLine(app_info, is_machine));
    VERIFY1(result);
  }

  return result;
}

}  // namespace omaha
