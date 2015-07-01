// Copyright 2010 Google Inc.
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

#include "omaha/goopdate/app_bundle_state_paused.h"
#include "omaha/base/debug.h"
#include "omaha/base/logging.h"
#include "omaha/goopdate/app_bundle_state_busy.h"
#include "omaha/goopdate/app_bundle_state_ready.h"
#include "omaha/goopdate/model.h"

namespace omaha {

namespace fsm {

// Remains in this state since the bundle is already paused.
HRESULT AppBundleStatePaused::Pause(AppBundle* app_bundle) {
  CORE_LOG(L3, (_T("[AppBundleStatePaused::Pause][0x%p]"), app_bundle));
  UNREFERENCED_PARAMETER(app_bundle);
  return S_OK;
}

HRESULT AppBundleStatePaused::Resume(AppBundle* app_bundle) {
  CORE_LOG(L3, (_T("[AppBundleStatePaused::Resume][0x%p]"), app_bundle));
  ASSERT1(app_bundle);
  ASSERT1(app_bundle->model()->IsLockedByCaller());

  HRESULT hr = app_bundle->model()->Resume(app_bundle);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[Resume failed][0x%08x][0x%p]"), hr, app_bundle));
    return hr;
  }

  if (is_async_call_complete_) {
    ChangeState(app_bundle, new AppBundleStateReady);
  } else {
    ChangeState(app_bundle, new AppBundleStateBusy);
  }
  return S_OK;
}

HRESULT AppBundleStatePaused::CompleteAsyncCall(AppBundle* app_bundle) {
  CORE_LOG(L3, (_T("[AppBundleStatePaused::CompleteAsyncCall][0x%p]"),
                app_bundle));
  ASSERT1(app_bundle);
  ASSERT1(app_bundle->model()->IsLockedByCaller());
  ASSERT1(IsPendingNonBlockingCall(app_bundle));
  UNREFERENCED_PARAMETER(app_bundle);
  is_async_call_complete_ = true;
  return S_OK;
}

}  // namespace fsm

}  // namespace omaha
