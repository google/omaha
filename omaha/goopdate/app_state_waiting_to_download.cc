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

#include "omaha/goopdate/app_state_waiting_to_download.h"
#include "omaha/base/debug.h"
#include "omaha/base/logging.h"
#include "omaha/goopdate/app_state_download_complete.h"
#include "omaha/goopdate/app_state_downloading.h"
#include "omaha/goopdate/download_manager.h"
#include "omaha/goopdate/model.h"

namespace omaha {

namespace fsm {

AppStateWaitingToDownload::AppStateWaitingToDownload()
    : AppState(STATE_WAITING_TO_DOWNLOAD) {
}

const PingEvent* AppStateWaitingToDownload::CreatePingEvent(
    App* app,
    CurrentState previous_state) const {
  ASSERT1(app);
  UNREFERENCED_PARAMETER(previous_state);

  // Omaha 3 reports this ping later than Omaha 2 because the COM server does
  // not know the client's intent when doing the update check.
  const PingEvent::Types event_type(app->is_update() ?
      PingEvent::EVENT_UPDATE_APPLICATION_BEGIN :
      PingEvent::EVENT_INSTALL_APPLICATION_BEGIN);

  const HRESULT error_code = app->error_code();
  ASSERT1(SUCCEEDED(error_code));

  return new PingEvent(event_type, GetCompletionResult(*app), error_code, 0);
}

void AppStateWaitingToDownload::Download(
    App* app,
    DownloadManagerInterface* download_manager) {
  CORE_LOG(L3, (_T("[AppStateWaitingToDownload::Download][0x%p]"), app));
  ASSERT1(app);
  ASSERT1(download_manager);

  // This is a blocking call on the network.
  HRESULT hr = download_manager->DownloadApp(app);

  app->LogTextAppendFormat(_T("Download result=0x%08x"), hr);
}

void AppStateWaitingToDownload::Downloading(App* app) {
  CORE_LOG(L3, (_T("[AppStateWaitingToDownload::Downloading][%p]"), app));
  ASSERT1(app);

  ChangeState(app, new AppStateDownloading);
}

void AppStateWaitingToDownload::DownloadComplete(App* app) {
  CORE_LOG(L3, (_T("[AppStateWaitingToDownload::DownloadComplete][%p]"), app));
  CORE_LOG(L3, (_T("[Did not download anything - likely because all packages ")
                _T("were cached - or OnProgress callback was never called.]")));
  ASSERT1(app);

  ChangeState(app, new AppStateDownloadComplete);
}

}  // namespace fsm

}  // namespace omaha

