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

#include "omaha/goopdate/app_bundle_state.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/common/web_services_client.h"
#include "omaha/goopdate/app_bundle_state_busy.h"
#include "omaha/goopdate/model.h"

namespace omaha {

namespace fsm {

HRESULT AppBundleState::put_altTokens(AppBundle* app_bundle,
                                      ULONG_PTR impersonation_token,
                                      ULONG_PTR primary_token,
                                      DWORD caller_proc_id) {
  UNREFERENCED_PARAMETER(impersonation_token);
  UNREFERENCED_PARAMETER(primary_token);
  UNREFERENCED_PARAMETER(caller_proc_id);
  return HandleInvalidStateTransition(app_bundle, _T(__FUNCTION__));
}

HRESULT AppBundleState::put_sessionId(AppBundle* app_bundle, BSTR session_id) {
  UNREFERENCED_PARAMETER(session_id);
  return HandleInvalidStateTransition(app_bundle, _T(__FUNCTION__));
}

HRESULT AppBundleState::Initialize(AppBundle* app_bundle) {
  return HandleInvalidStateTransition(app_bundle, _T(__FUNCTION__));
}

HRESULT AppBundleState::CreateApp(AppBundle* app_bundle,
                                  const CString& app_id,
                                  App** app) {
  UNREFERENCED_PARAMETER(app_id);
  UNREFERENCED_PARAMETER(app);
  return HandleInvalidStateTransition(app_bundle, _T(__FUNCTION__));
}

HRESULT AppBundleState::CreateInstalledApp(AppBundle* app_bundle,
                                           const CString& app_id,
                                           App** app) {
  UNREFERENCED_PARAMETER(app_id);
  UNREFERENCED_PARAMETER(app);
  return HandleInvalidStateTransition(app_bundle, _T(__FUNCTION__));
}

HRESULT AppBundleState::CreateAllInstalledApps(AppBundle* app_bundle) {
  return HandleInvalidStateTransition(app_bundle, _T(__FUNCTION__));
}

HRESULT AppBundleState::CheckForUpdate(AppBundle* app_bundle) {
  return HandleInvalidStateTransition(app_bundle, _T(__FUNCTION__));
}

HRESULT AppBundleState::Download(AppBundle* app_bundle) {
  return HandleInvalidStateTransition(app_bundle, _T(__FUNCTION__));
}

HRESULT AppBundleState::Install(AppBundle* app_bundle) {
  return HandleInvalidStateTransition(app_bundle, _T(__FUNCTION__));
}

HRESULT AppBundleState::UpdateAllApps(AppBundle* app_bundle) {
  return HandleInvalidStateTransition(app_bundle, _T(__FUNCTION__));
}

HRESULT AppBundleState::Stop(AppBundle* app_bundle) {
  return HandleInvalidStateTransition(app_bundle, _T(__FUNCTION__));
}

HRESULT AppBundleState::Pause(AppBundle* app_bundle) {
  return HandleInvalidStateTransition(app_bundle, _T(__FUNCTION__));
}

HRESULT AppBundleState::Resume(AppBundle* app_bundle) {
  return HandleInvalidStateTransition(app_bundle, _T(__FUNCTION__));
}

HRESULT AppBundleState::DownloadPackage(AppBundle* app_bundle,
                                        const CString& app_id,
                                        const CString& package_name) {
  UNREFERENCED_PARAMETER(app_id);
  UNREFERENCED_PARAMETER(package_name);
  return HandleInvalidStateTransition(app_bundle, _T(__FUNCTION__));
}

HRESULT AppBundleState::CompleteAsyncCall(AppBundle* app_bundle) {
  return HandleInvalidStateTransition(app_bundle, _T(__FUNCTION__));
}

bool AppBundleState::IsBusy() const {
  return false;
}

void AppBundleState::AddAppToBundle(AppBundle* app_bundle, App* app) {
  ASSERT1(app_bundle);
  ASSERT1(app_bundle->model()->IsLockedByCaller());
  app_bundle->apps_.push_back(app);
}

bool AppBundleState::IsPendingNonBlockingCall(AppBundle* app_bundle) {
  ASSERT1(app_bundle);
  return app_bundle->is_pending_non_blocking_call();
}

HRESULT AppBundleState::DoDownloadPackage(AppBundle* app_bundle,
                                          const CString& app_id,
                                          const CString& package_name) {
  CORE_LOG(L3, (_T("[AppBundleState::DoDownloadPackage][0x%p]"), app_bundle));
  ASSERT1(app_bundle);
  ASSERT1(app_bundle->model()->IsLockedByCaller());
  ASSERT1(!IsPendingNonBlockingCall(app_bundle));

  GUID app_guid = {0};
  HRESULT hr = StringToGuidSafe(app_id, &app_guid);
  if (FAILED(hr)) {
    return hr;
  }

  App* app = NULL;
  for (size_t i = 0; i != app_bundle->GetNumberOfApps(); ++i) {
    App* candidate_app = app_bundle->GetApp(i);
    if (::IsEqualGUID(candidate_app->app_guid(), app_guid)) {
      app = candidate_app;
      break;
    }
  }

  if (!app) {
    return E_INVALIDARG;
  }

  // Only packages of installed applications can be downloaded.
  Package* package = NULL;
  AppVersion* version = app->current_version();
  for (size_t i = 0; i != version->GetNumberOfPackages(); ++i) {
    if (version->GetPackage(i)->filename() == package_name) {
      package = version->GetPackage(i);
      break;
    }
  }

  if (!package) {
    return E_INVALIDARG;
  }

  hr = app_bundle->model()->DownloadPackage(package);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[DownloadPackage failed][0x%x][0x%p]"), hr, app_bundle));
    return hr;
  }

  ChangeState(app_bundle, new AppBundleStateBusy);
  return S_OK;
}

void AppBundleState::ChangeState(AppBundle* app_bundle, AppBundleState* state) {
  ASSERT1(app_bundle);
  ASSERT1(state);
  ASSERT1(app_bundle->model()->IsLockedByCaller());
  CORE_LOG(L3, (_T("[AppBundleState::ChangeState][0x%p][from: %u][to: %u]"),
                app_bundle, state_, state->state_));

  app_bundle->ChangeState(state);
}

HRESULT AppBundleState::HandleInvalidStateTransition(
    AppBundle* app_bundle,
    const TCHAR* function_name) {
  UNREFERENCED_PARAMETER(app_bundle);
  UNREFERENCED_PARAMETER(function_name);
  ASSERT1(app_bundle);
  ASSERT1(app_bundle->model()->IsLockedByCaller());
  CORE_LOG(LE, (_T("[Invalid state transition][%s called while in %u]"),
                function_name, state_));
  return GOOPDATE_E_CALL_UNEXPECTED;
}

}  // namespace fsm

}  // namespace omaha
