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

#include "omaha/common/update3_utils.h"
#include <atlsafe.h>
#include "goopdate/omaha3_idl.h"
#include "omaha/base/const_object_names.h"
#include "omaha/base/debug.h"
#include "omaha/base/logging.h"
#include "omaha/base/system.h"
#include "omaha/base/vistautil.h"
#include "omaha/goopdate/google_update3.h"

namespace omaha {

namespace update3_utils {

namespace {

bool UseInProcCOMServer() {
  DWORD value = 0;
  if (SUCCEEDED(RegKey::GetValue(MACHINE_REG_UPDATE_DEV,
                                 kRegValueUseInProcCOMServer,
                                 &value))) {
    return value != 0;
  } else {
    return false;
  }
}

template <typename Update3COMClassT>
HRESULT CreateGoogleUpdate3LocalClass(IGoogleUpdate3** server) {
  CORE_LOG(L3, (_T("[CreateGoogleUpdate3LocalClass]")));
  ASSERT1(server);

  typedef CComObject<Update3COMClassT> Update3;
  scoped_ptr<Update3> update3;
  HRESULT hr = Update3::CreateInstance(address(update3));
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[Update3 creation failed][0x%x]"), hr));
    return hr;
  }

  hr = update3->QueryInterface(server);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[Update3 QueryInterface failed][0x%x]"), hr));
    return hr;
  }

  update3.release();

  return S_OK;
}

}  // namespace

HRESULT SetProxyBlanketAllowImpersonate(IUnknown* server) {
  ASSERT1(server);

  HRESULT hr = ::CoSetProxyBlanket(server,
                                   RPC_C_AUTHN_DEFAULT,
                                   RPC_C_AUTHZ_DEFAULT,
                                   COLE_DEFAULT_PRINCIPAL,
                                   RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
                                   RPC_C_IMP_LEVEL_IMPERSONATE,
                                   NULL,
                                   EOAC_DYNAMIC_CLOAKING);

  // E_NOINTERFACE indicates an in-proc intra-apartment call.
  if (FAILED(hr) && hr != E_NOINTERFACE) {
    CORE_LOG(LE, (_T("[::CoSetProxyBlanket failed][0x%x]"), hr));
    return hr;
  }

  return S_OK;
}

HRESULT CreateGoogleUpdate3Class(bool is_machine, IGoogleUpdate3** server) {
  CORE_LOG(L3, (_T("[CreateGoogleUpdate3Class][%d]"), is_machine));
  ASSERT1(server);

  CComPtr<IGoogleUpdate3> com_server;
  HRESULT hr = is_machine ? CreateGoogleUpdate3MachineClass(&com_server) :
                            CreateGoogleUpdate3UserClass(&com_server);
  if (FAILED(hr)) {
    return hr;
  }

  hr = SetProxyBlanketAllowImpersonate(com_server);
  if (FAILED(hr)) {
    return hr;
  }

  *server = com_server.Detach();
  return S_OK;
}

// Tries to CoCreate the service CLSID first. If that fails, tries to create the
// server in-proc. Finally, sets a security blanket on the interface to allow
// the server to impersonate the client.
HRESULT CreateGoogleUpdate3MachineClass(IGoogleUpdate3** machine_server) {
  ASSERT1(machine_server);
  ASSERT1(vista_util::IsUserAdmin());

  if (UseInProcCOMServer()) {
    return
        CreateGoogleUpdate3LocalClass<Update3COMClassService>(machine_server);
  }

  CComPtr<IGoogleUpdate3> server;
  HRESULT hr = server.CoCreateInstance(__uuidof(GoogleUpdate3ServiceClass));

  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[CoCreate GoogleUpdate3ServiceClass failed][0x%x]"), hr));

    hr = CreateGoogleUpdate3LocalClass<Update3COMClassService>(&server);
    if (hr == GOOPDATE_E_INSTANCES_RUNNING) {
      CORE_LOG(L3, (_T("[Retry CoCreate GoogleUpdate3ServiceClass]")));
      hr = server.CoCreateInstance(__uuidof(GoogleUpdate3ServiceClass));
    }

    if (FAILED(hr)) {
      CORE_LOG(LE, (_T("[Create GoogleUpdate3MachineClass failed][0x%x]"), hr));
      return hr;
    }
  }

  ASSERT1(server);
  *machine_server = server.Detach();
  return S_OK;
}

// Tries to CoCreate the LocalServer CLSID first. If that fails, tries to create
// the server in-proc.
HRESULT CreateGoogleUpdate3UserClass(IGoogleUpdate3** user_server) {
  ASSERT1(user_server);

  if (UseInProcCOMServer()) {
    return CreateGoogleUpdate3LocalClass<Update3COMClassUser>(user_server);
  }

  CComPtr<IGoogleUpdate3> server;
  HRESULT hr = server.CoCreateInstance(__uuidof(GoogleUpdate3UserClass));
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[CoCreate GoogleUpdate3UserClass failed][0x%x]"), hr));

    // The primary reason for the LocalServer activation failing on Vista/Win7
    // is that COM does not look at HKCU registration when the code is running
    // elevated. We fall back to an in-proc mode. The in-proc mode is limited to
    // one install at a time, so we use it only as a backup mechanism.
    OPT_LOG(LE, (_T("[IsElevatedWithEnableLUAOn][%d]"),
                 vista_util::IsElevatedWithEnableLUAOn()));
    hr = CreateGoogleUpdate3LocalClass<Update3COMClassUser>(&server);
    if (FAILED(hr)) {
      return hr;
    }
  }

  ASSERT1(server);
  *user_server = server.Detach();
  return S_OK;
}

HRESULT CreateAppBundle(IGoogleUpdate3* server, IAppBundle** app_bundle) {
  ASSERT1(server);
  ASSERT1(app_bundle);

  CComPtr<IDispatch> idispatch;
  HRESULT hr = server->createAppBundle(&idispatch);
  if (FAILED(hr)) {
    return hr;
  }

  return idispatch.QueryInterface(app_bundle);
}

HRESULT CreateApp(BSTR app_id, IAppBundle* app_bundle, IApp** app) {
  ASSERT1(app_id);
  ASSERT1(app_bundle);
  ASSERT1(app);

  CComPtr<IDispatch> idispatch;
  HRESULT hr = app_bundle->createApp(app_id, &idispatch);
  if (FAILED(hr)) {
    return hr;
  }

  return idispatch.QueryInterface(app);
}

HRESULT CreateInstalledApp(BSTR app_id, IAppBundle* app_bundle, IApp** app) {
  ASSERT1(app_id);
  ASSERT1(app_bundle);
  ASSERT1(app);

  CComPtr<IDispatch> idispatch;
  HRESULT hr = app_bundle->createInstalledApp(app_id, &idispatch);
  if (FAILED(hr)) {
    return hr;
  }

  return idispatch.QueryInterface(app);
}

HRESULT CreateAllInstalledApps(IAppBundle* app_bundle) {
  ASSERT1(app_bundle);

  return app_bundle->createAllInstalledApps();
}

HRESULT GetApp(IAppBundle* app_bundle, long index, IApp** app) {  // NOLINT
  ASSERT1(app_bundle);
  ASSERT1(index >= 0);
  ASSERT1(app);

  CComPtr<IDispatch> app_idispatch;
  HRESULT hr = app_bundle->get_Item(index, &app_idispatch);
  if (FAILED(hr)) {
    return hr;
  }
  ASSERT1(app_idispatch);

  return app_idispatch.QueryInterface(app);
}

HRESULT GetAppCommand(IApp* app, BSTR command_id, IAppCommand2** app_command) {
  ASSERT1(app);
  ASSERT1(command_id);
  ASSERT1(app_command);

  *app_command = NULL;

  CComPtr<IDispatch> idispatch;
  HRESULT hr = app->get_command(command_id, &idispatch);
  if (FAILED(hr)) {
    return hr;
  }
  if (hr == S_FALSE) {
    return S_FALSE;
  }

  ASSERT1(idispatch);

  return idispatch.QueryInterface(app_command);
}

HRESULT GetCurrentAppVersion(IApp* app, IAppVersion** app_version) {
  ASSERT1(app);
  ASSERT1(app_version);

  CComPtr<IDispatch> idispatch;
  HRESULT hr = app->get_currentVersion(&idispatch);
  if (FAILED(hr)) {
    return hr;
  }

  return idispatch.QueryInterface(app_version);
}

HRESULT GetNextAppVersion(IApp* app, IAppVersion** app_version) {
  ASSERT1(app);
  ASSERT1(app_version);

  CComPtr<IDispatch> idispatch;
  HRESULT hr = app->get_nextVersion(&idispatch);
  if (FAILED(hr)) {
    return hr;
  }

  return idispatch.QueryInterface(app_version);
}

HRESULT GetAppCurrentState(IApp* app,
                           CurrentState* current_state,
                           ICurrentState** icurrent_state) {
  ASSERT1(app);
  ASSERT1(current_state);
  ASSERT1(icurrent_state);

  CComPtr<IDispatch> idispatch;
  HRESULT hr = app->get_currentState(&idispatch);
  if (FAILED(hr)) {
    return hr;
  }

  hr = idispatch.QueryInterface(icurrent_state);
  if (FAILED(hr)) {
    return hr;
  }

  LONG state = 0;
  hr = (*icurrent_state)->get_stateValue(&state);
  if (FAILED(hr)) {
    return hr;
  }

  *current_state = static_cast<CurrentState>(state);
  return S_OK;
}

}  // namespace update3_utils

}  // namespace omaha
