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

#include "omaha/goopdate/app_state_ready_to_install.h"
#include "omaha/base/debug.h"
#include "omaha/base/logging.h"
#include "omaha/goopdate/app_state_waiting_to_install.h"

namespace omaha {

namespace fsm {

AppStateReadyToInstall::AppStateReadyToInstall()
    : AppState(STATE_READY_TO_INSTALL) {
}

void AppStateReadyToInstall::QueueDownloadOrInstall(App* app) {
  CORE_LOG(L3, (_T("[AppStateReadyToInstall::QueueDownloadOrInstall][0x%p]"),
                app));
  ASSERT1(app);

  QueueInstall(app);
}

void AppStateReadyToInstall::QueueInstall(App* app) {
  CORE_LOG(L3, (_T("[AppStateReadyToInstall::QueueInstall][%p]"), app));
  ASSERT1(app);

  ChangeState(app, new AppStateWaitingToInstall);
}

}  // namespace fsm

}  // namespace omaha
