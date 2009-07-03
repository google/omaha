// Copyright 2007-2009 Google Inc.
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
// DLL entry points for the Bho DLL

#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>
#include "omaha/bho/bho_entrypoint.h"
#include "omaha/common/debug.h"
#include "omaha/common/logging.h"
#include "omaha/common/utils.h"

namespace omaha {

class BhoDllModule : public CAtlDllModuleT<BhoDllModule> {
};

// Add BhoEntry to class library table so IE can CoCreate it.
OBJECT_ENTRY_AUTO(__uuidof(BhoEntrypointClass), BhoEntrypoint);

BhoDllModule _AtlModule;

}  // namespace omaha

using omaha::_AtlModule;

extern "C" BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID reserved) {
  if (reason == DLL_PROCESS_ATTACH) {
    VERIFY1(SUCCEEDED(omaha::PinModuleIntoProcess(BHO_FILENAME)));
  }

  return _AtlModule.DllMain(reason, reserved);
}

STDAPI DllCanUnloadNow() {
  // Do not allow unloading until the process terminates.
  return S_FALSE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
  CORE_LOG(L2, (_T("[DllGetClassObject]")));
  return _AtlModule.DllGetClassObject(rclsid, riid, ppv);
}

STDAPI DllRegisterServer(void) {
  // Not registering the typelib.
  return _AtlModule.DllRegisterServer(FALSE);
}


STDAPI DllUnregisterServer(void) {
  // Not unregistering the typelib.
  return _AtlModule.DllUnregisterServer(FALSE);
}

