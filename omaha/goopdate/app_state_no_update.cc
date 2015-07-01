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

#include "omaha/goopdate/app_state_no_update.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/goopdate/model.h"

namespace omaha {

namespace fsm {

AppStateNoUpdate::AppStateNoUpdate() : AppState(STATE_NO_UPDATE) {
}

const PingEvent* AppStateNoUpdate::CreatePingEvent(
    App* app,
    CurrentState previous_state) const {
  ASSERT1(app);
  UNREFERENCED_PARAMETER(previous_state);

  // This state corresponds to the update case only. 'No updates' scenario in
  // the installed case should be handled as errors by the state machine.
  ASSERT1(app->is_update());

  const PingEvent::Results completion_result = GetCompletionResult(*app);
  ASSERT1(completion_result == PingEvent::EVENT_RESULT_SUCCESS ||
          completion_result == PingEvent::EVENT_RESULT_UPDATE_DEFERRED);

  // Creates a ping for deferred updates only.
  if (completion_result == PingEvent::EVENT_RESULT_UPDATE_DEFERRED) {
    // Omaha updates should never be deferred.
    ASSERT1(!::IsEqualGUID(kGoopdateGuid, app->app_guid()));

    const PingEvent::Types event_type(PingEvent::EVENT_UPDATE_COMPLETE);

    const HRESULT error_code(app->error_code());
    ASSERT1(error_code == GOOPDATE_E_UPDATE_DEFERRED);

    return new PingEvent(event_type, completion_result, error_code, 0);
  }

  return NULL;
}

void AppStateNoUpdate::QueueDownload(App* app) {
  CORE_LOG(L4, (_T("[AppStateNoUpdate::QueueDownload][%p]"), app));
  ASSERT1(app);
  UNREFERENCED_PARAMETER(app);
}

void AppStateNoUpdate::QueueDownloadOrInstall(App* app) {
  CORE_LOG(L4, (_T("[AppStateNoUpdate::QueueDownloadOrInstall][%p]"), app));
  ASSERT1(app);
  UNREFERENCED_PARAMETER(app);
}

void AppStateNoUpdate::Download(
    App* app,
    DownloadManagerInterface* download_manager) {
  CORE_LOG(L4, (_T("[AppStateNoUpdate::Download][0x%p]"), app));
  ASSERT1(app);
  ASSERT1(download_manager);
  UNREFERENCED_PARAMETER(app);
  UNREFERENCED_PARAMETER(download_manager);
}

void AppStateNoUpdate::QueueInstall(App* app) {
  CORE_LOG(L4, (_T("[AppStateNoUpdate::QueueInstall][%p]"), app));
  ASSERT1(app);
  UNREFERENCED_PARAMETER(app);
}

void AppStateNoUpdate::Install(
    App* app,
    InstallManagerInterface* install_manager) {
  CORE_LOG(L4, (_T("[AppStateNoUpdate::Install][0x%p]"), app));
  ASSERT1(app);
  ASSERT1(install_manager);
  UNREFERENCED_PARAMETER(app);
  UNREFERENCED_PARAMETER(install_manager);
}

// Canceling while in a terminal state has no effect.
void AppStateNoUpdate::Cancel(App* app) {
  CORE_LOG(L4, (_T("[AppStateNoUpdate::Cancel][0x%p]"), app));
  ASSERT1(app);
  UNREFERENCED_PARAMETER(app);
}

// Terminal states should not transition to error.
void AppStateNoUpdate::Error(App* app,
                             const ErrorContext& error_context,
                             const CString& message) {
  ASSERT1(app);
  UNREFERENCED_PARAMETER(error_context);
  UNREFERENCED_PARAMETER(message);
  HandleInvalidStateTransition(app, _T(__FUNCTION__));
}

}  // namespace fsm

}  // namespace omaha
