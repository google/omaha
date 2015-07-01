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

#include "omaha/goopdate/app_bundle_state_ready.h"
#include "omaha/base/debug.h"
#include "omaha/base/logging.h"
#include "omaha/goopdate/app_bundle_state_busy.h"
#include "omaha/goopdate/model.h"

namespace omaha {

namespace fsm {

HRESULT AppBundleStateReady::Download(AppBundle* app_bundle) {
  CORE_LOG(L3, (_T("[AppBundleStateReady::Download][0x%p]"), app_bundle));
  ASSERT1(app_bundle);
  ASSERT1(app_bundle->model()->IsLockedByCaller());
  ASSERT1(!IsPendingNonBlockingCall(app_bundle));

  HRESULT hr = app_bundle->model()->Download(app_bundle);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[Download failed][0x%08x][0x%p]"), hr, app_bundle));
    return hr;
  }

  ChangeState(app_bundle, new AppBundleStateBusy);
  return S_OK;
}

HRESULT AppBundleStateReady::Install(AppBundle* app_bundle) {
  CORE_LOG(L3, (_T("[AppBundleStateReady::Install][0x%p]"), app_bundle));
  ASSERT1(app_bundle);
  ASSERT1(app_bundle->model()->IsLockedByCaller());
  ASSERT1(!IsPendingNonBlockingCall(app_bundle));

  HRESULT hr = app_bundle->model()->DownloadAndInstall(app_bundle);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[Install failed][0x%08x][0x%p]"), hr, app_bundle));
    return hr;
  }

  ChangeState(app_bundle, new AppBundleStateBusy);
  return S_OK;
}

// Remains in this state since there is nothing to pause.
HRESULT AppBundleStateReady::Pause(AppBundle* app_bundle) {
  CORE_LOG(L3, (_T("[AppBundleStateReady::Pause][0x%p]"), app_bundle));
  UNREFERENCED_PARAMETER(app_bundle);
  return S_OK;
}

// Remains in this state since the bundle is not paused. This might occur if
// Pause() was called while in this state.
HRESULT AppBundleStateReady::Resume(AppBundle* app_bundle) {
  CORE_LOG(L3, (_T("[AppBundleStateReady::Resume][0x%p]"), app_bundle));
  UNREFERENCED_PARAMETER(app_bundle);
  return S_OK;
}

HRESULT AppBundleStateReady::DownloadPackage(AppBundle* app_bundle,
                                             const CString& app_id,
                                             const CString& package_name) {
  CORE_LOG(L3, (_T("[AppBundleStateReady::DownloadPackage][0x%p]"),
                app_bundle));

  // TODO(omaha): There may need to be some check here that the app is
  // downloaded or installed.

  return DoDownloadPackage(app_bundle, app_id, package_name);
}

}  // namespace fsm

}  // namespace omaha
