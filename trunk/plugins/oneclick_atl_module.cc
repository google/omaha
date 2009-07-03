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
// The ATL module definition and module instance.

#include "omaha/common/reg_key.h"
#include "omaha/common/scope_guard.h"
#include "omaha/common/vistautil.h"
#include "omaha/goopdate/goopdate_utils.h"
#include "plugins/oneclick_idl.h"

namespace omaha {

class OneClickModule : public CAtlDllModuleT<OneClickModule> {
 public :
  OneClickModule() {}
  DECLARE_LIBID(LIBID_OneClickLib)
};

extern "C" {
  OneClickModule _AtlModule;
}

}  // namespace omaha.

HRESULT RegisterOrUnregisterDll(bool is_register) {
  HRESULT hr = is_register ? omaha::_AtlModule.DllRegisterServer(false) :
                             omaha::_AtlModule.DllUnregisterServer(false);
  ASSERT(SUCCEEDED(hr), (_T("[RegisterOrUnregisterDll failed][%d][0x%08x]"),
                         is_register, hr));
  return hr;
}

// The standard COM entry points below are exported using a .def file.
STDAPI DllCanUnloadNow() {
  return omaha::_AtlModule.DllCanUnloadNow();
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv) {
  return omaha::_AtlModule.DllGetClassObject(rclsid, riid, ppv);
}

STDAPI DllRegisterServer() {
  return omaha::goopdate_utils::RegisterOrUnregisterModule(
      true,
      &RegisterOrUnregisterDll);
}

STDAPI DllUnregisterServer() {
  return omaha::goopdate_utils::RegisterOrUnregisterModule(
      false,
      &RegisterOrUnregisterDll);
}

