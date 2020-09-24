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
//
// This module registers the proxy/stubs for the interfaces in kIIDsToRegister,
// including the marshal-by-value proxy/stub for the ICurrentState
// implementation.
// Most coclasses in omaha3_idl.idl inject code into clients by using
// IStdMarshalInfo and redirect proxy lookups so that machine and user
// Omaha installations are isolated from each other.
//

#include <atlbase.h>
#include "base/basictypes.h"
#include "omaha/base/utils.h"
#include "goopdate/omaha3_idl.h"
#include "omaha/goopdate/com_proxy.h"
#include "omaha/goopdate/current_state.h"
#include "omaha/goopdate/omaha3_idl_datax.h"
#include "omaha/goopdate/policy_status_value.h"

namespace omaha {

// TODO(omaha): Try to see if a single proxy DLL can be used for both user and
// machine.

#if IS_MACHINE_HANDLER
  OBJECT_ENTRY_AUTO(__uuidof(GoogleComProxyMachineClass), ComProxy)
  OBJECT_ENTRY_AUTO(__uuidof(CurrentStateMachineClass), CurrentAppState)
  OBJECT_ENTRY_AUTO(__uuidof(PolicyStatusValueMachineClass), PolicyStatusValue)
#else
  OBJECT_ENTRY_AUTO(__uuidof(GoogleComProxyUserClass), ComProxy)
  OBJECT_ENTRY_AUTO(__uuidof(CurrentStateUserClass), CurrentAppState)
  OBJECT_ENTRY_AUTO(__uuidof(PolicyStatusValueUserClass), PolicyStatusValue)
#endif

namespace {

class GoogleUpdatePSModule
    : public CAtlDllModuleT<GoogleUpdatePSModule> {
 public:
  GoogleUpdatePSModule() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(GoogleUpdatePSModule);  // NOLINT
} _AtlModule;

}  // namespace

}  // namespace omaha

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
  if (!omaha::_AtlModule.DllMain(reason, reserved)) {
    return FALSE;
  }

  return PrxDllMain(instance, reason, reserved);
}

STDAPI DllCanUnloadNow() {
  if (omaha::_AtlModule.DllCanUnloadNow() == S_OK &&
      PrxDllCanUnloadNow() == S_OK) {
    return S_OK;
  }

  return S_FALSE;
}

STDAPI DllGetClassObject(REFCLSID clsid, REFIID iid, void** ptr) {
  HRESULT hr_atl = omaha::_AtlModule.DllGetClassObject(clsid, iid, ptr);
  if (SUCCEEDED(hr_atl)) {
    return hr_atl;
  }

  HRESULT hr_prx = PrxDllGetClassObject(clsid, iid, ptr);
  if (FAILED(hr_prx)) {
    CORE_LOG(LE, (_T("[DllGetClassObject failed][%s][%s][0x%x][0x%x]"),
        omaha::GuidToString(clsid), omaha::GuidToString(iid), hr_atl, hr_prx));
  }

  return hr_prx;
}

STDAPI DllRegisterServer() {
  HRESULT hr = omaha::_AtlModule.DllRegisterServer(false);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[DllRegisterServer failed][0x%x]"), hr));
    return hr;
  }

  return PrxDllRegisterServer();
}

STDAPI DllUnregisterServer() {
  HRESULT hr = PrxDllUnregisterServer();
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[DllUnregisterServer failed][0x%x]"), hr));
    return hr;
  }

  return omaha::_AtlModule.DllUnregisterServer(false);
}

