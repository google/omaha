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

#include "omaha/goopdate/app_state_init.h"
#include "omaha/base/debug.h"
#include "omaha/base/logging.h"
#include "omaha/goopdate/app_manager.h"
#include "omaha/goopdate/app_state_waiting_to_check_for_update.h"
#include "omaha/goopdate/model.h"

namespace omaha {

namespace fsm {

AppStateInit::AppStateInit() : AppState(STATE_INIT) {
}

void AppStateInit::QueueUpdateCheck(App* app) {
  CORE_LOG(L3, (_T("[AppStateInit::QueueUpdateCheck][0x%p]"), app));
  ASSERT1(app);

  // Omaha should never be part of an app bundle in the install case. This is
  // an important debug check to ensure that duplicate pings are not sent.
  ASSERT1(!::IsEqualGUID(kGoopdateGuid, app->app_guid()) || app->is_update());

  app->ResetInstallProgress();
  AppManager::Instance()->ResetCurrentStateKey(app->app_guid_string());

  ChangeState(app, new AppStateWaitingToCheckForUpdate);
}

}  // namespace fsm

}  // namespace omaha
