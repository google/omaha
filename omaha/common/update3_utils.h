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

// Utilities for IGoogleUpdate3 and related interfaces.

#ifndef OMAHA_COMMON_UPDATE3_UTILS_H_
#define OMAHA_COMMON_UPDATE3_UTILS_H_

#include <windows.h>
#include <atlsafe.h>

#include "goopdate/omaha3_idl.h"
#include "omaha/base/debug.h"
#include "omaha/base/logging.h"

namespace omaha {

namespace update3_utils {

// Helper methods.
HRESULT SetProxyBlanketAllowImpersonate(IUnknown* server);

// This method sets Cloaking and allows for servers to impersonate the caller.
// Meant to be used primarily by clients that run in processes that may call
// ::CoInitializeSecurity with incompatible flags, for example IE10.
// Without this call, Cloaking and Handler Marshaling that the Update3
// interfaces depend on may not work.
//
// TODO(omaha3): Perhaps all CoCreates of Update3 objects can use this method.
// This could eliminate other calls to SetProxyBlanketAllowImpersonate()
// elsewhere.
template <typename T>
HRESULT CoCreateWithProxyBlanket(REFCLSID rclsid, T* t) {
  CComPtr<IClassFactory> cf;
  HRESULT hr = ::CoGetClassObject(rclsid, CLSCTX_ALL, NULL, IID_PPV_ARGS(&cf));
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[::CoGetClassObject failed][0x%x]"), hr));
    return hr;
  }

  hr = SetProxyBlanketAllowImpersonate(cf);
  if (FAILED(hr)) {
    return hr;
  }

  hr = cf->CreateInstance(NULL, IID_PPV_ARGS(t));
  ASSERT(SUCCEEDED(hr), (_T("[cf->CreateInstance failed][0x%x]"), hr));
  return hr;
}

// Create methods.
HRESULT CreateGoogleUpdate3Class(bool is_machine, IGoogleUpdate3** server);
HRESULT CreateGoogleUpdate3MachineClass(IGoogleUpdate3** machine_server);
HRESULT CreateGoogleUpdate3UserClass(IGoogleUpdate3** user_server);
HRESULT CreateAppBundle(IGoogleUpdate3* server, IAppBundle** app_bundle);
HRESULT CreateApp(BSTR app_id, IAppBundle* app_bundle, IApp** app);
HRESULT CreateInstalledApp(BSTR app_id, IAppBundle* app_bundle, IApp** app);
HRESULT CreateAllInstalledApps(IAppBundle* app_bundle);

// Get methods.
HRESULT GetApp(IAppBundle* app_bundle, long index, IApp** app);  // NOLINT
HRESULT GetAppCommand(IApp* app, BSTR command_id, IAppCommand2** app_command);
HRESULT GetCurrentAppVersion(IApp* app, IAppVersion** app_version);
HRESULT GetNextAppVersion(IApp* app, IAppVersion** app_version);

// TODO(omaha): consider removing current_state parameter since it is
// returned in the icurrent_state object as well.
HRESULT GetAppCurrentState(IApp* app,
                           CurrentState* current_state,
                           ICurrentState** icurrent_state);

}  // namespace update3_utils

}  // namespace omaha

#endif  // OMAHA_COMMON_UPDATE3_UTILS_H_
