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

#include "omaha/goopdate/app_state_waiting_to_install.h"
#include "omaha/base/debug.h"
#include "omaha/base/file.h"
#include "omaha/base/logging.h"
#include "omaha/base/path.h"
#include "omaha/base/utils.h"
#include "omaha/goopdate/app_state_installing.h"
#include "omaha/goopdate/install_manager.h"
#include "omaha/goopdate/model.h"
#include "omaha/goopdate/server_resource.h"
#include "omaha/goopdate/string_formatter.h"

namespace omaha {

namespace fsm {

AppStateWaitingToInstall::AppStateWaitingToInstall()
    : AppState(STATE_WAITING_TO_INSTALL) {
}

void AppStateWaitingToInstall::Download(
    App* app,
    DownloadManagerInterface* download_manager) {
  CORE_LOG(L3, (_T("[AppStateWaitingToInstall::Download][0x%p]"), app));
  ASSERT1(app);
  ASSERT1(download_manager);
  UNREFERENCED_PARAMETER(app);
  UNREFERENCED_PARAMETER(download_manager);
}

void AppStateWaitingToInstall::QueueInstall(App* app) {
  CORE_LOG(L3, (_T("[AppStateWaitingToInstall::QueueInstall][%p]"), app));
  ASSERT1(app);
  UNREFERENCED_PARAMETER(app);
}

// Copies app packages and runs the installer. The packages are cleaned up
// by the InstallManager constructor, when its instance will be created by
// the next COM server that starts up.
void AppStateWaitingToInstall::Install(
    App* app,
    InstallManagerInterface* install_manager) {
  CORE_LOG(L3, (_T("[AppStateWaitingToInstall::Install][0x%p]"), app));
  ASSERT1(app);
  ASSERT1(install_manager);

  // For large files, verifying the hash of the file may take a several
  // seconds of CPU time. The execution of the CopyAppVersionPackages function
  // occurs under the model lock. That means that the controller can't respond
  // to state queries initiate by the COM client, which means that state
  // changes can't be observed while the hash verification occurs. While the
  // hash of the file is checked, the UI appears to be stuck in a state, which
  // may be different than this state. Calling ::Sleep here is a work around
  // to increase the likelihood that a client polling for state changes detects
  // this transition, and the UI displays a relevant text for the user.
  constexpr int kWaitBeforeInstallMs = 500;
  ::Sleep(kWaitBeforeInstallMs);

  CString guid;
  HRESULT hr(GetGuid(&guid));
  if (SUCCEEDED(hr)) {
    const CString installer_dir(
        ConcatenatePath(install_manager->install_working_dir(), guid));
    hr = CopyAppVersionPackages(app->next_version(), installer_dir);
    if (SUCCEEDED(hr)) {
      install_manager->InstallApp(app, installer_dir);
    }
  }

  if (FAILED(hr)) {
    StringFormatter formatter(app->app_bundle()->display_language());
    CString message;
    VERIFY_SUCCEEDED(formatter.LoadString(IDS_INSTALL_FAILED, &message));
    Error(app, ErrorContext(hr), message);
  }
}

void AppStateWaitingToInstall::Installing(App* app) {
  CORE_LOG(L3, (_T("[AppStateWaitingToInstall::Installing][%p]"), app));
  ASSERT1(app);

  ChangeState(app, new AppStateInstalling);
}

}  // namespace fsm

}  // namespace omaha
