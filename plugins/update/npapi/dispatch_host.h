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
//
// DispatchHost hosts an IDispatch object inside a NPObject to allow scripting
// of COM objects from a NPAPI environment. Types are automatically marshalled
// between NPVariant and VARIANT using the functions in variant_utils.h.
// Limitations:
// - IDispatch methods/properties may only take arguments of type VT_VOID,
//   VT_NULL, VT_BOOL, VT_I4, VT_R8, and VT_BSTR
// - Multiple out parameters are not supported.
// - IDispatch methods/properties may only return a value of type VT_EMPTY,
//   VT_VOID, VT_NULL, VT_BOOL, VT_I4, VT_UI4, VT_R8, VT_BSTR, and VT_DISPATCH
// - A method and property a property that takes additional arguments may not
//   have the same identifier--the method will not be callable through
//   DispatchHost.

#ifndef OMAHA_PLUGINS_UPDATE_NPAPI_DISPATCH_HOST_H_
#define OMAHA_PLUGINS_UPDATE_NPAPI_DISPATCH_HOST_H_

#include <atlbase.h>
#include <atlcom.h>

#include "base/basictypes.h"
#include "third_party/npapi/bindings/nphostapi.h"

namespace omaha {

class DispatchHostTest;

class DispatchHost : public NPObject {
 public:
  static DispatchHost* CreateInstance(NPP npp, IDispatch* dispatch);

 private:
  explicit DispatchHost(NPP npp);
  ~DispatchHost();

  DISPID GetDispatchId(NPIdentifier name);
  bool IsProperty(DISPID dispatch_id);
  HRESULT InvokeHelper(DISPID dispatch_id, WORD flags, const NPVariant* args,
                       uint32_t arg_count, NPP npp, NPVariant* result);

  static NPObject* Allocate(NPP npp, NPClass *class_functions);
  static void Deallocate(NPObject* object);
  static bool HasMethod(NPObject* object, NPIdentifier name);
  static bool Invoke(NPObject* object, NPIdentifier name, const NPVariant* args,
                     uint32_t arg_count, NPVariant* result);
  static bool InvokeDefault(NPObject* object, const NPVariant* args,
                            uint32_t arg_count, NPVariant* result);
  static bool HasProperty(NPObject* object, NPIdentifier name);
  static bool GetProperty(NPObject* object, NPIdentifier name,
                          NPVariant* result);
  static bool SetProperty(NPObject* object, NPIdentifier name,
                          const NPVariant* value);
  static bool RemoveProperty(NPObject* object, NPIdentifier name);
  static bool Enumerate(NPObject* object, NPIdentifier** names,
                        uint32_t* count);
  static bool Construct(NPObject* object, const NPVariant* args,
                        uint32_t arg_count, NPVariant* result);

  NPP npp_;
  // The hosted dispatch object.
  CComPtr<IDispatch> dispatch_;

  // The NPObject vtable.
  static NPClass kNPClass_;

  friend class DispatchHostTest;

  DISALLOW_COPY_AND_ASSIGN(DispatchHost);
};

}  // namespace omaha

#endif  // OMAHA_PLUGINS_UPDATE_NPAPI_DISPATCH_HOST_H_
