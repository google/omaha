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

#include "omaha/goopdate/app_state_install_complete.h"

#include "omaha/common/config_manager.h"
#include "omaha/common/ping_event.h"
#include "omaha/base/logging.h"
#include "omaha/goopdate/model.h"
#include "omaha/goopdate/worker.h"

namespace omaha {

namespace fsm {

AppStateInstallComplete::AppStateInstallComplete(App* app)
    : AppState(STATE_INSTALL_COMPLETE) {
  ASSERT1(app);

  // Start the crash handler if an installer has indicated that it requires OOP
  // crash handling.
  bool is_machine = app->app_bundle()->is_machine();
  if (ConfigManager::Instance()->CanCollectStats(is_machine) &&
      (!is_machine || user_info::IsRunningAsSystem())) {
    goopdate_utils::StartCrashHandler(is_machine);
  }

  VERIFY_SUCCEEDED(app->model()->PurgeAppLowerVersions(
      app->app_guid_string(), app->next_version()->version()));
}

// Omaha installs and updates are two-step processes. Omaha is handled as a
// special case in both installs and updates.
//
// In the install case, Omaha itself is installed by the /install process, which
// is responsible for pinging. Omaha is never part of the bundle in this case.
//
// In update case, the Omaha update is run as a /update process first.
// The install manager does not wait for that process to complete and, if the
// launch of it was successful, it transitions the Omaha app into the install
// complete state. No ping should be sent in this case. Next, the update process
// runs, finishes the update of Omaha, and then it sends the
// EVENT_UPDATE_COMPLETE ping.
const PingEvent* AppStateInstallComplete::CreatePingEvent(
    App* app,
    CurrentState previous_state) const {
  ASSERT1(app);
  UNREFERENCED_PARAMETER(previous_state);

  const PingEvent::Types event_type(app->is_update() ?
      PingEvent::EVENT_UPDATE_COMPLETE :
      PingEvent::EVENT_INSTALL_COMPLETE);

  const bool is_omaha = !!::IsEqualGUID(kGoopdateGuid, app->app_guid());

  const bool can_ping = !is_omaha;

  const HRESULT error_code(app->error_code());
  ASSERT1(SUCCEEDED(error_code));

  return can_ping ? new PingEvent(event_type,
                                  GetCompletionResult(*app),
                                  error_code,
                                  app->installer_result_extra_code1(),
                                  app->source_url_index(),
                                  app->GetUpdateCheckTimeMs(),
                                  app->GetDownloadTimeMs(),
                                  app->num_bytes_downloaded(),
                                  app->GetPackagesTotalSize(),
                                  app->GetInstallTimeMs()) : NULL;
}

// Canceling while in a terminal state has no effect.
void AppStateInstallComplete::Cancel(App* app) {
  CORE_LOG(L3, (_T("[AppStateInstallComplete::Cancel][0x%p]"), app));
  UNREFERENCED_PARAMETER(app);
}

// Terminal states should not transition to error.
void AppStateInstallComplete::Error(App* app,
                                    const ErrorContext& error_context,
                                    const CString& message) {
  UNREFERENCED_PARAMETER(error_context);
  UNREFERENCED_PARAMETER(message);
  HandleInvalidStateTransition(app, _T(__FUNCTION__));
}

}  // namespace fsm

}  // namespace omaha
