// Copyright 2009-2010 Google Inc.
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

#include "omaha/goopdate/app_state.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/goopdate/app_state_error.h"
#include "omaha/goopdate/model.h"
#include "omaha/goopdate/server_resource.h"
#include "omaha/goopdate/string_formatter.h"

namespace omaha {

namespace fsm {

const PingEvent* AppState::CreatePingEvent(App* app,
                                           CurrentState previous_state) const {
  UNREFERENCED_PARAMETER(app);
  UNREFERENCED_PARAMETER(previous_state);
  return NULL;
}

void AppState::QueueUpdateCheck(App* app) {
  HandleInvalidStateTransition(app, _T(__FUNCTION__));
}

void AppState::PreUpdateCheck(App* app, xml::UpdateRequest* update_request) {
  UNREFERENCED_PARAMETER(update_request);
  HandleInvalidStateTransition(app, _T(__FUNCTION__));
}

void AppState::PostUpdateCheck(App* app,
                               HRESULT result,
                               xml::UpdateResponse* update_response) {
  UNREFERENCED_PARAMETER(result);
  UNREFERENCED_PARAMETER(update_response);
  HandleInvalidStateTransition(app, _T(__FUNCTION__));
}

void AppState::QueueDownload(App* app) {
  HandleInvalidStateTransition(app, _T(__FUNCTION__));
}

void AppState::QueueDownloadOrInstall(App* app) {
  HandleInvalidStateTransition(app, _T(__FUNCTION__));
}

void AppState::Download(App* app, DownloadManagerInterface* download_manager) {
  ASSERT1(download_manager);
  UNREFERENCED_PARAMETER(download_manager);
  // Must acquire the lock here because app does not acquire it before calling
  // this method.
  __mutexScope(app->model()->lock());
  HandleInvalidStateTransition(app, _T(__FUNCTION__));
}

void AppState::Downloading(App* app) {
  HandleInvalidStateTransition(app, _T(__FUNCTION__));
}

void AppState::DownloadComplete(App* app) {
  HandleInvalidStateTransition(app, _T(__FUNCTION__));
}

void AppState::MarkReadyToInstall(App* app) {
  HandleInvalidStateTransition(app, _T(__FUNCTION__));
}

void AppState::QueueInstall(App* app) {
  HandleInvalidStateTransition(app, _T(__FUNCTION__));
}

void AppState::Install(App* app, InstallManagerInterface* install_manager) {
  ASSERT1(install_manager);
  UNREFERENCED_PARAMETER(install_manager);
  // Must acquire the lock here because app does not acquire it before calling
  // this method.
  __mutexScope(app->model()->lock());
  HandleInvalidStateTransition(app, _T(__FUNCTION__));
}

void AppState::Installing(App* app) {
  HandleInvalidStateTransition(app, _T(__FUNCTION__));
}

void AppState::ReportInstallerComplete(App* app,
                                       const InstallerResultInfo& result_info) {
  UNREFERENCED_PARAMETER(result_info);
  HandleInvalidStateTransition(app, _T(__FUNCTION__));
}

void AppState::Pause(App* app) {
  HandleInvalidStateTransition(app, _T(__FUNCTION__));
}

// TODO(omaha3): If Cancel is not valid during certain states, override this in
// those states and decide what should happen. Consider Worker::StopAsync().
void AppState::Cancel(App* app) {
  ASSERT1(app);
  ASSERT1(app->model()->IsLockedByCaller());
  CORE_LOG(L3, (_T("[AppState::Cancel][0x%p]"), app));

  const HRESULT hr = GOOPDATE_E_CANCELLED;
  CString message;

  StringFormatter formatter(app->app_bundle()->display_language());
  VERIFY_SUCCEEDED(formatter.LoadString(IDS_CANCELED, &message));
  app->SetError(ErrorContext(hr), message);
  app->set_state_cancelled(state());
  app->SetCurrentTimeAs(App::TIME_CANCELLED);
  ChangeState(app, new AppStateError);
}

void AppState::Error(App* app,
                     const ErrorContext& error_context,
                     const CString& message) {
  ASSERT1(app);
  ASSERT1(app->model()->IsLockedByCaller());
  CORE_LOG(LE, (_T("[AppState::Error][0x%p][0x%08x][%s]"),
      app, error_context.error_code, message));

  app->SetError(error_context, message);
  ChangeState(app, new AppStateError);
}

void AppState::ChangeState(App* app, AppState* app_state) {
  ASSERT1(app);
  ASSERT1(app_state);
  ASSERT1(app->model()->IsLockedByCaller());
  CORE_LOG(L3, (_T("[AppState::ChangeState][0x%p][%d]"),
                app, app_state->state()));

  app->ChangeState(app_state);
}

// Avoid infinite recursion by calling the base class's Error() method.
void AppState::HandleInvalidStateTransition(App* app,
                                            const TCHAR* function_name) {
  UNREFERENCED_PARAMETER(function_name);
  ASSERT(false, (_T("Invalid state transition: %s called while in state %u."),
                 function_name, state_));
  const HRESULT hr = GOOPDATE_E_INVALID_STATE_TRANSITION;
  StringFormatter formatter(app->app_bundle()->display_language());
  CString message;
  VERIFY_SUCCEEDED(formatter.LoadString(IDS_INSTALL_FAILED, &message));
  AppState::Error(app, ErrorContext(hr, state_), message);
}

PingEvent::Results AppState::GetCompletionResult(const App& app) {
  ASSERT1(app.model()->IsLockedByCaller());
  return app.completion_result_;
}

void AppState::HandleGroupPolicyError(App* app, HRESULT code) {
  ASSERT1(app);
  ASSERT1(code == GOOPDATE_E_APP_INSTALL_DISABLED_BY_POLICY ||
          code == GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY ||
          code == GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY_MANUAL);
  OPT_LOG(LW, (_T("[App Update disabled][%s]"), app->app_guid_string()));
  app->LogTextAppendFormat(_T("Status=%s-disabled"),
                           app->is_update() ? _T("update") : _T("install"));

  StringFormatter formatter(app->app_bundle()->display_language());
  CString error_message;
  VERIFY_SUCCEEDED(formatter.LoadString(
                        IDS_APP_INSTALL_DISABLED_BY_GROUP_POLICY,
                        &error_message));
  Error(app, ErrorContext(code), error_message);
}

}  // namespace fsm

}  // namespace omaha
