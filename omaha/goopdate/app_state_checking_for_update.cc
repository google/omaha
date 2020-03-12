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

#include "omaha/goopdate/app_state_checking_for_update.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/vistautil.h"
#include "omaha/common/lang.h"
#include "omaha/common/update_response.h"
#include "omaha/goopdate/app_manager.h"
#include "omaha/goopdate/app_state_no_update.h"
#include "omaha/goopdate/app_state_update_available.h"
#include "omaha/goopdate/model.h"
#include "omaha/goopdate/server_resource.h"
#include "omaha/goopdate/string_formatter.h"
#include "omaha/goopdate/update_response_utils.h"
#include "omaha/goopdate/worker_metrics.h"
#include "omaha/goopdate/worker_utils.h"

namespace omaha {

namespace fsm {

xml::UpdateResponseResult GetUpdateResponseResult(
    const App* app,
    const xml::UpdateResponse* update_response) {
  ASSERT1(app);
  ASSERT1(update_response);

  const CString app_name = app->display_name();
  const CString language = app->app_bundle()->display_language();

  xml::UpdateResponseResult update_response_result =
      update_response_utils::CheckSystemRequirements(update_response, language);
  if (FAILED(update_response_result.first)) {
    return update_response_result;
  }

  update_response_result =
      update_response_utils::GetResult(update_response,
                                       app->app_guid_string(),
                                       app_name,
                                       language);

  const bool is_omaha   = !!::IsEqualGUID(kGoopdateGuid, app->app_guid());
  const bool has_update = update_response_result.first == S_OK &&
                          app->is_update();

  // Defer the update if the app is not Omaha, it has an update available, and
  // an Omaha update is available at the same time.
  if (!is_omaha  &&
      has_update &&
      update_response_utils::IsOmahaUpdateAvailable(update_response)) {
    StringFormatter formatter(language);
    CString text;
    VERIFY_SUCCEEDED(formatter.LoadString(IDS_NO_UPDATE_RESPONSE, &text));
    update_response_result = std::make_pair(GOOPDATE_E_UPDATE_DEFERRED, text);
  }

  return update_response_result;
}

AppStateCheckingForUpdate::AppStateCheckingForUpdate()
    : AppState(STATE_CHECKING_FOR_UPDATE),
      update_response_(NULL) {
}

// TODO(omaha3): Consider passing in an xml::response::App instead of a raw
// xml::UpdateResponse to this method.
void AppStateCheckingForUpdate::PostUpdateCheck(
    App* app,
    HRESULT update_check_result,
    xml::UpdateResponse* update_response) {
  CORE_LOG(L3, (_T("[AppStateCheckingForUpdate::PostUpdateCheck][0x%p]"), app));

  ASSERT1(app);
  ASSERT1(update_response);

  ASSERT1(app->model()->IsLockedByCaller());

  app->SetCurrentTimeAs(App::TIME_UPDATE_CHECK_COMPLETE);

  update_response_ = update_response;

  const CString language = app->app_bundle()->display_language();

  if (FAILED(update_check_result)) {
    PersistUpdateCheckValuesOnFailure(app);

    // TODO(omaha3): There is no guarantee that this is a actually network
    // error. In Omaha 2, this was called much closer to the send. Making most
    // errors, such as processing errors, app errors helps, but it could still
    // be a parsing or other error.
    CString error_message;
    worker_utils::FormatMessageForNetworkError(update_check_result,
                                               language,
                                               &error_message);

    Error(app, ErrorContext(update_check_result), error_message);
    return;
  }

  app->set_day_of_last_response(update_response_->GetElapsedDaysSinceDatum());
  PersistUpdateCheckSuccessfullySent(*app);

  const xml::UpdateResponseResult update_response_result(
      GetUpdateResponseResult(app, update_response));

  const HRESULT& code    = update_response_result.first;
  const CString& message = update_response_result.second;

  if (SUCCEEDED(code)) {
    HandleUpdateAvailable(app, code, message);
  } else if (code == GOOPDATE_E_UPDATE_DEFERRED) {
    HandleUpdateDeferred(app, code, message);
  } else if (code == GOOPDATE_E_NO_UPDATE_RESPONSE) {
    HandleNoUpdate(app, code, message);
  } else {
    HandleErrorResponse(app, code, message);
  }
}

void AppStateCheckingForUpdate::HandleUpdateAvailable(App* app,
                                                      HRESULT code,
                                                      const CString& message) {
  CORE_LOG(L3, (_T("[HandleUpdateAvailable][0x%p]"), app));

  ASSERT1(app);
  ASSERT1(SUCCEEDED(code));

  UNREFERENCED_PARAMETER(code);
  UNREFERENCED_PARAMETER(message);

  app->set_has_update_available(true);

  HRESULT hr = update_response_utils::BuildApp(update_response_, code, app);
  if (FAILED(hr)) {
    // Most of the errors that might actually be seen are likely to be due to
    // response issues. Therefore, display a message about the server.
    const CString language = app->app_bundle()->display_language();
    StringFormatter formatter(language);
    CString error_message;
    VERIFY_SUCCEEDED(formatter.LoadString(IDS_UNKNOWN_APPLICATION,
                                           &error_message));
    Error(app, ErrorContext(hr), error_message);
    return;
  }

  const TCHAR* action = app->is_update() ? _T("update") : _T("install");
  app->LogTextAppendFormat(_T("Status=%s"), action);

  // Record the update available response regardless of how it is handled.
  AppManager::Instance()->PersistSuccessfulUpdateCheckResponse(*app, true);

  if (app->is_update()) {
    if (::IsEqualGUID(kGoopdateGuid, app->app_guid())) {
      ++metric_worker_self_updates_available;
    } else {
      ++metric_worker_app_updates_available;
    }
  }

  ChangeState(app, new AppStateUpdateAvailable);
}

void AppStateCheckingForUpdate::HandleUpdateDeferred(App* app,
                                                     HRESULT code,
                                                     const CString& message) {
  CORE_LOG(L3, (_T("[HandleUpdateDeferred][0x%p]"), app));

  ASSERT1(app);
  ASSERT1(code == GOOPDATE_E_UPDATE_DEFERRED);

  ASSERT1(app->is_update());

  app->SetNoUpdate(ErrorContext(code), message);
  ChangeState(app, new AppStateNoUpdate);
}

void AppStateCheckingForUpdate::HandleNoUpdate(App* app,
                                               HRESULT code,
                                               const CString& message) {
  CORE_LOG(L3, (_T("[HandleNoUpdate][0x%p]"), app));
  ASSERT1(app);
  ASSERT1(code == GOOPDATE_E_NO_UPDATE_RESPONSE);

  app->LogTextAppendFormat(_T("Status=no-update"));

  // For installs, no update is handled as an error.
  if (!app->is_update()) {
    Error(app, ErrorContext(code), message);
    return;
  }

  VERIFY_SUCCEEDED(update_response_utils::BuildApp(update_response_,
                                                    code,
                                                    app));
  AppManager::Instance()->PersistSuccessfulUpdateCheckResponse(*app, false);

  app->SetNoUpdate(ErrorContext(S_OK), message);
  ChangeState(app, new AppStateNoUpdate);
}

void AppStateCheckingForUpdate::HandleErrorResponse(App* app,
                                                    HRESULT code,
                                                    const CString& message) {
  CORE_LOG(L3, (_T("[HandleErrorResponse][0x%p]"), app));

  ASSERT1(app);
  ASSERT1(FAILED(code));

  CString log_status;
  switch (code) {
    case GOOPDATE_E_NO_SERVER_RESPONSE:
      log_status = _T("no-response-received");
      break;
    case GOOPDATE_E_RESTRICTED_SERVER_RESPONSE:
      log_status = _T("restricted");
      break;
    case GOOPDATE_E_UNKNOWN_APP_SERVER_RESPONSE:
    case GOOPDATE_E_OS_NOT_SUPPORTED:
    case GOOPDATE_E_INTERNAL_ERROR_SERVER_RESPONSE:
    case GOOPDATE_E_SERVER_RESPONSE_NO_HASH:
    case GOOPDATE_E_SERVER_RESPONSE_UNSUPPORTED_PROTOCOL:
    case GOOPDATE_E_UNKNOWN_SERVER_RESPONSE:
    default:
      log_status = _T("error");
      break;
  }

  app->LogTextAppendFormat(_T("Status=%s, Code=0x%08x"), log_status, code);

  Error(app, ErrorContext(code), message);
}

// This function will be called when the update check fails. In this case,
// Omaha looks into the response headers for relevant values. |daynum| or
// |daystart|will be -1 if the corresponding header value is absent. Client
// should not persist if either value is -1.
void AppStateCheckingForUpdate::PersistUpdateCheckValuesOnFailure(App* app) {
  ASSERT1(app);

  const int daynum =
      app->app_bundle()->update_check_client()->http_xdaynum_header_value();
  const int daystart =
      app->app_bundle()->update_check_client()->http_xdaystart_header_value();

  if (daystart == -1 || daynum == -1) {
    return;
  }

  // Registry writes to HKLM need admin.
  ASSERT1(!app->app_bundle()->is_machine() || vista_util::IsUserAdmin());
  ASSERT1(daystart >= 0);
  ASSERT1(daynum >= kMinDaysSinceDatum);
  ASSERT1(daynum <= kMaxDaysSinceDatum);

  CORE_LOG(LE, (_T("[PersistHeadersOnFailure][%d][%d]"), daynum, daystart));

  AppManager& app_manager = *AppManager::Instance();
  app->set_day_of_last_response(daynum);
  VERIFY_SUCCEEDED(app_manager.PersistUpdateCheckSuccessfullySent(
      *app, daynum, daystart));
}

void AppStateCheckingForUpdate::PersistUpdateCheckSuccessfullySent(
    const App& app) {
  // For offline install, we didn't send actual pings to server and the response
  // is loaded from XML manifest file, so no need to update metrics for ping
  // sent event.
  if (app.app_bundle()->is_offline_install()) {
    return;
  }

  AppManager& app_manager = *AppManager::Instance();
  VERIFY_SUCCEEDED(app_manager.PersistUpdateCheckSuccessfullySent(
      app,
      update_response_->GetElapsedDaysSinceDatum(),
      update_response_->GetElapsedSecondsSinceDayStart()));

  // Here we assume that some of the members in app object
  // (days_since_last_active_ping_, days_since_last_roll_call_,
  //  day_of_last_roll_call_, day_of_last_activity_, iid_, did_run_)
  // will not be used after the update check so there is no need to update them.
}

}  // namespace fsm

}  // namespace omaha
