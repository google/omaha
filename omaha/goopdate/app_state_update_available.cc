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

#include "omaha/goopdate/app_state_update_available.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/goopdate/app_state_waiting_to_download.h"
#include "omaha/goopdate/model.h"
#include "omaha/goopdate/server_resource.h"
#include "omaha/goopdate/string_formatter.h"

namespace omaha {

namespace fsm {

AppStateUpdateAvailable::AppStateUpdateAvailable()
    : AppState(STATE_UPDATE_AVAILABLE) {
}

void AppStateUpdateAvailable::QueueDownload(App* app) {
  CORE_LOG(L3, (_T("[AppStateUpdateAvailable::QueueDownload][0x%p]"), app));
  ASSERT1(app);

  HRESULT policy_hr = app->CheckGroupPolicy();
  if (FAILED(policy_hr)) {
    HandleGroupPolicyError(app, policy_hr);
    return;
  }

  app->SetCurrentTimeAs(App::TIME_UPDATE_AVAILABLE);
  ChangeState(app, new AppStateWaitingToDownload);
}

void AppStateUpdateAvailable::QueueDownloadOrInstall(App* app) {
  CORE_LOG(L3, (_T("[AppStateUpdateAvailable::QueueDownloadOrInstall][0x%p]"),
                app));
  ASSERT1(app);

  QueueDownload(app);
}

void AppStateUpdateAvailable::HandleGroupPolicyError(App* app, HRESULT code) {
  ASSERT1(app);
  ASSERT1(code == GOOPDATE_E_APP_INSTALL_DISABLED_BY_POLICY ||
          code == GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY);
  OPT_LOG(LW, (_T("[Update available for disabled app][%s]"),
                app->app_guid_string()));
  app->LogTextAppendFormat(_T("Status=%s-disabled"),
                           app->is_update() ? _T("update") : _T("install"));

  StringFormatter formatter(app->app_bundle()->display_language());
  CString error_message;
  VERIFY1(SUCCEEDED(formatter.LoadString(
                        IDS_APP_INSTALL_DISABLED_BY_GROUP_POLICY,
                        &error_message)));
  Error(app, ErrorContext(code), error_message);
}

}  // namespace fsm

}  // namespace omaha
