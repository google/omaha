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

#include <atlbase.h>
#include "base/basictypes.h"
#include "omaha/base/omaha_version.h"
#include "goopdate/omaha3_idl.h"
#include "omaha/plugins/update/activex/update3web_control.h"
#include "omaha/plugins/update/activex/oneclick_control.h"
#include "plugins/update/activex/update_control_idl.h"

namespace omaha {

OBJECT_ENTRY_AUTO(__uuidof(GoogleUpdateOneClickControlCoClass), OneClickControl)
OBJECT_ENTRY_AUTO(__uuidof(GoogleUpdate3WebControlCoClass), Update3WebControl)

namespace {

class GoogleUpdateControlModule
    : public CAtlDllModuleT<GoogleUpdateControlModule> {
 public:
  GoogleUpdateControlModule() {}

  DECLARE_LIBID(LIBID_GoogleUpdateControlLib);

 private:
  DISALLOW_COPY_AND_ASSIGN(GoogleUpdateControlModule);  // NOLINT
} _AtlModule;

}  // namespace

}  // namespace omaha

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
  switch (reason) {
    case DLL_PROCESS_ATTACH:
      omaha::InitializeVersionFromModule(instance);
      break;
    default:
      break;
  }

  return omaha::_AtlModule.DllMain(reason, reserved);
}

STDAPI DllCanUnloadNow() {
  return omaha::_AtlModule.DllCanUnloadNow();
}

STDAPI DllGetClassObject(REFCLSID clsid, REFIID iid, void** ppv) {
  return omaha::_AtlModule.DllGetClassObject(clsid, iid, ppv);
}

STDAPI DllRegisterServer() {
  return omaha::_AtlModule.DllRegisterServer(false);
}

STDAPI DllUnregisterServer() {
  return omaha::_AtlModule.DllUnregisterServer(false);
}
