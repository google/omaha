// Copyright 2009 Google Inc.
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

#include "omaha/goopdate/app_state_installing.h"
#include "omaha/base/debug.h"
#include "omaha/base/logging.h"
#include "omaha/goopdate/app_state_error.h"
#include "omaha/goopdate/app_state_install_complete.h"
#include "omaha/goopdate/model.h"

namespace omaha {

namespace fsm {

AppStateInstalling::AppStateInstalling()
    : AppState(STATE_INSTALLING) {
}

const PingEvent* AppStateInstalling::CreatePingEvent(
    App* app,
    CurrentState previous_state) const {
  ASSERT1(app);
  UNREFERENCED_PARAMETER(previous_state);

  const PingEvent::Types event_type(app->is_update() ?
      PingEvent::EVENT_UPDATE_INSTALLER_START :
      PingEvent::EVENT_INSTALL_INSTALLER_START);


  const HRESULT error_code = app->error_code();
  ASSERT1(SUCCEEDED(error_code));

  return new PingEvent(event_type, GetCompletionResult(*app), error_code, 0);
}

// The state does not change if already in the Installing state.
void AppStateInstalling::Installing(App* app) {
  CORE_LOG(L3, (_T("[AppStateInstalling::Installing][0x%p]"), app));
  ASSERT1(app);
  ASSERT(false, (_T("This might be valid when we support install progress.")));

  UNREFERENCED_PARAMETER(app);
}

void AppStateInstalling::ReportInstallerComplete(
    App* app,
    const InstallerResultInfo& result_info) {
  CORE_LOG(L3, (_T("[AppStateInstalling::ReportInstallerComplete][%p]"), app));
  ASSERT1(app);

  app->SetInstallerResult(result_info);

  ChangeState(app, result_info.type == INSTALLER_RESULT_SUCCESS ?
      static_cast<AppState*>(new AppStateInstallComplete(app)) :
      static_cast<AppState*>(new AppStateError));
}

// Cancel in installing state has no effect because currently we cannot cancel
// installers.. Override the function to avoid moving app into error state.
// The app will automatically enter the completion state when the installer
// completes.
void AppStateInstalling::Cancel(App* app) {
  CORE_LOG(L3, (_T("[AppStateInstalling::Cancel][0x%p]"), app));
  ASSERT1(app);

  UNREFERENCED_PARAMETER(app);
}

}  // namespace fsm

}  // namespace omaha
