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

#include "omaha/goopdate/app_state_error.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/goopdate/model.h"
#include "omaha/goopdate/ping_event_cancel.h"

namespace omaha {

namespace fsm {

AppStateError::AppStateError() : AppState(STATE_ERROR) {
}

const PingEvent* AppStateError::CreatePingEvent(
    App* app,
    CurrentState previous_state) const {
  ASSERT1(app);

  ASSERT1(FAILED(app->error_code()));

  const PingEvent::Types event_type(app->is_update() ?
      PingEvent::EVENT_UPDATE_COMPLETE :
      PingEvent::EVENT_INSTALL_COMPLETE);

  const PingEvent::Results result = GetCompletionResult(*app);

  // Installer errors are reported in the ping in the case where the
  // installer ran and failed. Otherwise, Omaha errors are reported.
  //
  // App extra codes are reported if set, otherwise the state of the state
  // machine which caused the transition to the error state is encoded and
  // reported.
  const bool is_installer_error =
      result == PingEvent::EVENT_RESULT_INSTALLER_ERROR_MSI    ||
      result == PingEvent::EVENT_RESULT_INSTALLER_ERROR_OTHER  ||
      result == PingEvent::EVENT_RESULT_INSTALLER_ERROR_SYSTEM;

  HRESULT error_code = S_OK;
  int extra_code1    = 0;

  if (is_installer_error) {
    error_code  = static_cast<HRESULT>(app->installer_result_code());
    extra_code1 = app->installer_result_extra_code1();
  } else {
    const int app_extra_code1 = app->error_context().extra_code1;
    error_code = app->error_code();
    extra_code1 = app_extra_code1 ? app_extra_code1 :
                          (PingEvent::kAppStateExtraCodeMask | previous_state);
  }

  // TODO(omaha): remove special case after the experiment is complete.
  if (error_code == GOOPDATEDOWNLOAD_E_CACHING_FAILED ||
      error_code == GOOPDATEINSTALL_E_INSTALLER_FAILED_START) {
    extra_code1 = error_extra_code1();
  }

  // The error completion ping is sent whenever the application ended up in
  // the error state:
  // * in the install case always, since any install error is final.
  // * in the update case only when an update has been available.
  //
  // In the update case, it is possible that the code errors out before it
  // discovers that an update is available. Therefore, there is a window of
  // uncertainty where the client did not get far enough to know if it was
  // told by the server to update or not.
  const bool can_ping = app->is_install() || app->has_update_available();
  if (!can_ping) {
    return NULL;
  }

  if (result == PingEvent::EVENT_RESULT_CANCELLED) {
    return new PingEventCancel(event_type,
                               result,
                               error_code,
                               extra_code1,
                               app->is_bundled(),
                               app->state_cancelled(),
                               app->GetTimeSinceUpdateAvailable(),
                               app->GetTimeSinceDownloadStart());
  }

  return new PingEvent(event_type,
                       result,
                       error_code,
                       extra_code1,
                       app->source_url_index(),
                       app->GetUpdateCheckTimeMs(),
                       app->GetDownloadTimeMs(),
                       app->num_bytes_downloaded(),
                       app->GetPackagesTotalSize(),
                       app->GetInstallTimeMs());
}

void AppStateError::DownloadComplete(App* app) {
  CORE_LOG(L3, (_T("[AppStateError::DownloadComplete][0x%p]"), app));
  UNREFERENCED_PARAMETER(app);
}

void AppStateError::MarkReadyToInstall(App* app) {
  CORE_LOG(L3, (_T("[AppStateError::MarkReadyToInstall][0x%p]"), app));
  UNREFERENCED_PARAMETER(app);
}

void AppStateError::PreUpdateCheck(App* app,
                                   xml::UpdateRequest* update_request) {
  CORE_LOG(L3, (_T("[AppStateError::PreUpdateCheck][%p]"), app));
  ASSERT1(app);
  UNREFERENCED_PARAMETER(app);
  UNREFERENCED_PARAMETER(update_request);
}

void AppStateError::PostUpdateCheck(App* app,
                               HRESULT result,
                               xml::UpdateResponse* update_response) {
  CORE_LOG(L3, (_T("[AppStateError::PostUpdateCheck][%p]"), app));
  ASSERT1(app);
  UNREFERENCED_PARAMETER(app);
  UNREFERENCED_PARAMETER(result);
  UNREFERENCED_PARAMETER(update_response);
}

void AppStateError::QueueDownload(App* app) {
  CORE_LOG(L3, (_T("[AppStateError::QueueDownload][%p]"), app));
  ASSERT1(app);
  UNREFERENCED_PARAMETER(app);
}

void AppStateError::QueueDownloadOrInstall(App* app) {
  CORE_LOG(L3, (_T("[AppStateError::QueueDownloadOrInstall][%p]"), app));
  ASSERT1(app);
  UNREFERENCED_PARAMETER(app);
}

void AppStateError::Download(
    App* app,
    DownloadManagerInterface* download_manager) {
  CORE_LOG(L3, (_T("[AppStateError::Download][0x%p]"), app));
  ASSERT1(app);
  ASSERT1(download_manager);
  UNREFERENCED_PARAMETER(app);
  UNREFERENCED_PARAMETER(download_manager);
}

void AppStateError::QueueInstall(App* app) {
  CORE_LOG(L3, (_T("[AppStateError::QueueInstall][%p]"), app));
  ASSERT1(app);
  UNREFERENCED_PARAMETER(app);
}

void AppStateError::Install(
    App* app,
    InstallManagerInterface* install_manager) {
  CORE_LOG(L3, (_T("[AppStateError::Install][0x%p]"), app));
  ASSERT1(app);
  ASSERT1(install_manager);
  UNREFERENCED_PARAMETER(app);
  UNREFERENCED_PARAMETER(install_manager);
}

void AppStateError::Cancel(App* app) {
  CORE_LOG(L3, (_T("[AppStateError::Cancel][0x%p]"), app));
  ASSERT1(app);
  UNREFERENCED_PARAMETER(app);
}

void AppStateError::Error(App* app,
                          const ErrorContext& error_context,
                          const CString& message) {
  ASSERT1(app);
  UNREFERENCED_PARAMETER(app);
  UNREFERENCED_PARAMETER(error_context);
  UNREFERENCED_PARAMETER(message);
  CORE_LOG(L3, (_T("[app is already in the Error state]")
      _T("[0x%p][app error=0x%x][this error=0x%x][%s]"),
      app, app->error_code(), error_context.error_code, message));
}

}  // namespace fsm

}  // namespace omaha
