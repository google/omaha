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

#include "omaha/goopdate/app_bundle_state_busy.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/goopdate/app_bundle_state_paused.h"
#include "omaha/goopdate/app_bundle_state_ready.h"
#include "omaha/goopdate/app_bundle_state_stopped.h"
#include "omaha/goopdate/model.h"

namespace omaha {

namespace fsm {

HRESULT AppBundleStateBusy::Pause(AppBundle* app_bundle) {
  CORE_LOG(L3, (_T("[AppBundleStateBusy::Pause][0x%p]"), app_bundle));
  ASSERT1(app_bundle);
  ASSERT1(app_bundle->model()->IsLockedByCaller());
  ASSERT1(IsPendingNonBlockingCall(app_bundle));

  HRESULT hr = app_bundle->model()->Pause(app_bundle);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[Pause failed][0x%08x][0x%p]"), hr, app_bundle));
    return hr;
  }

  ChangeState(app_bundle, new AppBundleStatePaused);
  return S_OK;
}

HRESULT AppBundleStateBusy::Stop(AppBundle* app_bundle) {
  CORE_LOG(L3, (_T("[AppBundleStateBusy::Stop][0x%p]"), app_bundle));
  ASSERT1(app_bundle);
  ASSERT1(app_bundle->model()->IsLockedByCaller());
  ASSERT1(IsPendingNonBlockingCall(app_bundle));

  HRESULT hr = app_bundle->model()->Stop(app_bundle);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[Stop failed][0x%08x][0x%p]"), hr, app_bundle));
    return hr;
  }

  // Handling Stop is non-blocking. The worker completes the pending bundle
  // calls while the bundle remains in the stopped state.
  ChangeState(app_bundle, new AppBundleStateStopped);
  return S_OK;
}

HRESULT AppBundleStateBusy::CompleteAsyncCall(AppBundle* app_bundle) {
  CORE_LOG(L3, (_T("[AppBundleStateBusy::CompleteAsyncCall][0x%p]"),
                app_bundle));
  ASSERT1(IsPendingNonBlockingCall(app_bundle));
  ChangeState(app_bundle, new AppBundleStateReady);
  return S_OK;
}

HRESULT AppBundleStateBusy::Download(AppBundle* app_bundle) {
  UNREFERENCED_PARAMETER(app_bundle);
  ASSERT1(IsPendingNonBlockingCall(app_bundle));
  return GOOPDATE_E_NON_BLOCKING_CALL_PENDING;
}

HRESULT AppBundleStateBusy::Install(AppBundle* app_bundle) {
  UNREFERENCED_PARAMETER(app_bundle);
  ASSERT1(IsPendingNonBlockingCall(app_bundle));
  return GOOPDATE_E_NON_BLOCKING_CALL_PENDING;
}

HRESULT AppBundleStateBusy::DownloadPackage(AppBundle* app_bundle,
                                            const CString& app_id,
                                            const CString& package_name) {
  UNREFERENCED_PARAMETER(app_bundle);
  UNREFERENCED_PARAMETER(app_id);
  UNREFERENCED_PARAMETER(package_name);
  ASSERT1(IsPendingNonBlockingCall(app_bundle));
  return GOOPDATE_E_NON_BLOCKING_CALL_PENDING;
}

bool AppBundleStateBusy::IsBusy() const {
  return true;
}

}  // namespace fsm

}  // namespace omaha
