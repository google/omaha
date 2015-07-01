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

#include "omaha/goopdate/app_state_download_complete.h"
#include "omaha/base/debug.h"
#include "omaha/base/logging.h"
#include "omaha/goopdate/app_state_ready_to_install.h"
#include "omaha/goopdate/model.h"

namespace omaha {

namespace fsm {

AppStateDownloadComplete::AppStateDownloadComplete()
    : AppState(STATE_DOWNLOAD_COMPLETE) {
}

const PingEvent* AppStateDownloadComplete::CreatePingEvent(
    App* app,
    CurrentState previous_state) const {
  ASSERT1(app);
  UNREFERENCED_PARAMETER(previous_state);

  const PingEvent::Types event_type(app->is_update() ?
      PingEvent::EVENT_UPDATE_DOWNLOAD_FINISH :
      PingEvent::EVENT_INSTALL_DOWNLOAD_FINISH);

  const HRESULT error_code = app->error_code();
  ASSERT1(SUCCEEDED(error_code));

  return new PingEvent(event_type, GetCompletionResult(*app), error_code, 0);
}

// TODO(omaha3): When extraction and differential updates are supported, this
// method will need to be copied/moved/changed to handle branches in the flow.
void AppStateDownloadComplete::MarkReadyToInstall(App* app) {
  CORE_LOG(L3,
           (_T("[AppStateDownloadComplete::MarkReadyToInstall][0x%p]"), app));
  ASSERT1(app);

  ChangeState(app, new AppStateReadyToInstall);
}

}  // namespace fsm

}  // namespace omaha
