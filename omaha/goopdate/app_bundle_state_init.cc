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

#include "omaha/goopdate/app_bundle_state_init.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/user_rights.h"
#include "omaha/base/utils.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/web_services_client.h"
#include "omaha/goopdate/app_bundle_state_initialized.h"
#include "omaha/goopdate/model.h"

namespace omaha {

namespace fsm {

HRESULT AppBundleStateInit::put_altTokens(AppBundle* app_bundle,
                                          ULONG_PTR impersonation_token,
                                          ULONG_PTR primary_token,
                                          DWORD caller_proc_id) {
  CORE_LOG(L3, (_T("[AppBundleStateInit::put_altTokens][0x%p]"), app_bundle));
  ASSERT1(app_bundle);
  ASSERT1(impersonation_token);
  ASSERT1(primary_token);
  ASSERT1(caller_proc_id);
  ASSERT1(app_bundle->model()->IsLockedByCaller());

  scoped_handle caller_proc_handle(::OpenProcess(PROCESS_DUP_HANDLE,
                                                 false,
                                                 caller_proc_id));
  if (!get(caller_proc_handle)) {
    return HRESULTFromLastError();
  }

  if (app_bundle->alt_impersonation_token_.GetHandle() ||
      app_bundle->alt_primary_token_.GetHandle()) {
    return E_UNEXPECTED;
  }

  HRESULT hr = DuplicateTokenIntoCurrentProcess(
      get(caller_proc_handle),
      reinterpret_cast<HANDLE>(impersonation_token),
      &app_bundle->alt_impersonation_token_);
  if (FAILED(hr)) {
    return hr;
  }

  return DuplicateTokenIntoCurrentProcess(
             get(caller_proc_handle),
             reinterpret_cast<HANDLE>(primary_token),
             &app_bundle->alt_primary_token_);
}

HRESULT AppBundleStateInit::put_sessionId(AppBundle* app_bundle,
                                          BSTR session_id) {
  CORE_LOG(L3, (_T("[AppBundleStateInit::put_sessionId][0x%p]"), app_bundle));
  ASSERT1(app_bundle);
  ASSERT1(app_bundle->model()->IsLockedByCaller());

  if (!session_id) {
    return E_POINTER;
  }

  ASSERT1(IsGuid(session_id));

  app_bundle->session_id_ = session_id;
  return S_OK;
}

// Captures the caller's imperonation token and uses it to initialize the ping
// and WebServicesClient objects.
// It is possible that one user could call this function and another admin user
// could call subsequent methods with this impersonation token. Since it is only
// used for network access, this is okay. The primary token, which is used for
// installation, is only captured by install().
// The WebServicesClient objects are created here instead of in the constructor
// because the Initialize() function is not part of WebServicesClientInterface.
// TODO(omaha): Enforce ordering - initialize must be called before any other
// non-property functions. This code must move to AppBundleStateInit.
HRESULT AppBundleStateInit::Initialize(AppBundle* app_bundle) {
  CORE_LOG(L3, (_T("[AppBundleStateInit::Initialize][0x%p]"), app_bundle));
  ASSERT1(app_bundle);
  ASSERT1(app_bundle->model()->IsLockedByCaller());

  // Clients should have set these properties before calling this function.
  ASSERT1(!app_bundle->display_name_.IsEmpty());
  ASSERT1(!app_bundle->display_language().IsEmpty());

  // If the client hasn't set a session ID before calling this function,
  // generate a random one for them.
  if (app_bundle->session_id_.IsEmpty()) {
    GetGuid(&app_bundle->session_id_);
  }

  HRESULT hr = app_bundle->CaptureCallerImpersonationToken();
  if (FAILED(hr)) {
    return hr;
  }

  ASSERT1(!app_bundle->update_check_client_.get());
  CString update_check_url;
  VERIFY_SUCCEEDED(
      ConfigManager::Instance()->GetUpdateCheckUrl(&update_check_url));
  auto web_service_client = std::make_unique<WebServicesClient>(
      app_bundle->is_machine());
  hr = web_service_client->Initialize(update_check_url, HeadersVector(), true);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[Update check client init failed][0x%08x]"), hr));
    return hr;
  }
  app_bundle->update_check_client_.reset(web_service_client.release());

  ChangeState(app_bundle, new AppBundleStateInitialized);
  return S_OK;
}

}  // namespace fsm

}  // namespace omaha
