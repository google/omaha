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
// TODO(omaha): use NPN_SetException to return useful error information.

#include "omaha/plugins/update/npapi/dispatch_host.h"

#include "base/logging.h"
#include "base/scope_guard.h"
#include "base/scoped_ptr.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/string.h"
#include "omaha/plugins/update/npapi/variant_utils.h"

namespace omaha {

namespace {

void SetExceptionIfFailed(NPObject* object, HRESULT result) {
  if (FAILED(result)) {
    CStringA message;
    SafeCStringAFormat(&message, "0x%08x", result);
    NPN_SetException(object, message);
  }
}

}  // namespace

DispatchHost* DispatchHost::CreateInstance(NPP npp, IDispatch* dispatch) {
  ASSERT1(dispatch);
  DispatchHost* host = static_cast<DispatchHost*>(
      NPN_CreateObject(npp, &kNPClass_));
  host->dispatch_ = dispatch;
  CORE_LOG(L3, (L"[DispatchHost::DispatchHost][this=0x%p][dispatch=0x%p]",
                host, dispatch));
  return host;
}

DispatchHost::DispatchHost(NPP npp) : npp_(npp) {
}

DispatchHost::~DispatchHost() {
  CORE_LOG(L3, (L"[DispatchHost::~DispatchHost][this=0x%p][dispatch=0x%p]",
                this, dispatch_));
}

DISPID DispatchHost::GetDispatchId(NPIdentifier name) {
  NPUTF8* utf8_name = NPN_UTF8FromIdentifier(name);
  CString wide_name = Utf8ToWideChar(utf8_name, lstrlenA(utf8_name));
  NPN_MemFree(utf8_name);
  DISPID dispatch_id = DISPID_UNKNOWN;
  HRESULT hr = dispatch_.GetIDOfName(wide_name, &dispatch_id);
  if (FAILED(hr)) {
    return DISPID_UNKNOWN;
  }
  return dispatch_id;
}

// Whether or not a member should be treated as a property by NPAPI. A member is
// considered a property for NPAPI if either of the following are true:
// - The property is a getter with exactly one [out, retval] argument.
// - The property is a putter with exactly one [in] argument.
// The reason for this limitation is NPAPI does not support passing additional
// arguments when getting/setting properties. Properties that take additional
// arguments are handled as methods by NPAPI instead.
bool DispatchHost::IsProperty(DISPID dispatch_id) {
  CComPtr<ITypeInfo> type_info;
  HRESULT hr = dispatch_->GetTypeInfo(0, LOCALE_SYSTEM_DEFAULT, &type_info);
  if (FAILED(hr)) {
    ASSERT(false, (L"[IsProperty][failed=0x%08x]", hr));
    return false;
  }
  TYPEATTR* type_attr;
  hr = type_info->GetTypeAttr(&type_attr);
  if (FAILED(hr)) {
    ASSERT(false, (L"[IsProperty][failed=0x%08x]", hr));
    return false;
  }
  ON_SCOPE_EXIT_OBJ(*type_info.p, &ITypeInfo::ReleaseTypeAttr, type_attr);

  for (int i = 0; i < type_attr->cFuncs; ++i) {
    FUNCDESC* func_desc = NULL;
    hr = type_info->GetFuncDesc(i, &func_desc);
    if (FAILED(hr)) {
      ASSERT(false, (L"[IsProperty][failed=0x%08x]", hr));
      return false;
    }
    ON_SCOPE_EXIT_OBJ(*type_info.p, &ITypeInfo::ReleaseFuncDesc, func_desc);
    if (dispatch_id == func_desc->memid) {
      if (((func_desc->invkind & DISPATCH_PROPERTYGET) &&
            func_desc->cParams == 0) ||
          ((func_desc->invkind & DISPATCH_PROPERTYPUT) &&
           func_desc->cParams == 1 &&
           (func_desc->lprgelemdescParam[0].paramdesc.wParamFlags &
            PARAMFLAG_FIN))) {
        return true;
      }
    }
  }
  return false;
}

// Simple helper to adapt NPAPI method/property invocations to IDispatch::Invoke
// by wrapping/unwrapping NPVariants into VARIANTs.
HRESULT DispatchHost::InvokeHelper(DISPID dispatch_id, WORD flags,
                                   const NPVariant* args, uint32_t arg_count,
                                   NPP npp, NPVariant* result) {
  ASSERT1(args || arg_count == 0);
  ASSERT1(result);
  CORE_LOG(L3, (L"[InvokeHelper][this=0x%p][dispatch=0x%p][flags=0x%x]"
                L"[arg_count=%d]", this, dispatch_, flags, arg_count));

  // Just in case a rogue browser decides to use the return value on failure.
  VOID_TO_NPVARIANT(*result);
  scoped_array<CComVariant> dispatch_args(new CComVariant[arg_count]);

  // IDispatch::Invoke expects arguments in "reverse" order
  for (uint32_t i = 0 ; i < arg_count; ++i) {
    NPVariantToVariant(npp, args[i], &dispatch_args[arg_count - i - 1]);
  }
  DISPPARAMS dispatch_params = {};
  dispatch_params.rgvarg = dispatch_args.get();
  dispatch_params.cArgs = arg_count;
  CComVariant dispatch_result;
  HRESULT hr = dispatch_->Invoke(dispatch_id, IID_NULL, LOCALE_USER_DEFAULT,
                                 flags, &dispatch_params, &dispatch_result,
                                 NULL, NULL);
  if (FAILED(hr)) {
    CORE_LOG(L3, (L"[InvokeHelper][failed_hr=0x%p]", hr));
    return hr;
  }
  VariantToNPVariant(npp, dispatch_result, result);
  return hr;
}

NPObject* DispatchHost::Allocate(NPP npp, NPClass* class_functions) {
  UNREFERENCED_PARAMETER(class_functions);
  return new DispatchHost(npp);
}

void DispatchHost::Deallocate(NPObject* object) {
  delete static_cast<DispatchHost*>(object);
}

bool DispatchHost::HasMethod(NPObject* object, NPIdentifier name) {
  DispatchHost* host = static_cast<DispatchHost*>(object);
  DISPID dispatch_id = host->GetDispatchId(name);
  return dispatch_id != DISPID_UNKNOWN && !host->IsProperty(dispatch_id);
}

bool DispatchHost::Invoke(NPObject* object, NPIdentifier name,
                          const NPVariant* args, uint32_t arg_count,
                          NPVariant* result) {
  DispatchHost* host = static_cast<DispatchHost*>(object);
  CORE_LOG(L3, (L"[DispatchHost::Invoke][this=0x%p][dispatch=0x%p]",
                host, host->dispatch_));
  HRESULT hr = host->InvokeHelper(host->GetDispatchId(name),
                                  DISPATCH_METHOD | DISPATCH_PROPERTYGET,
                                  args,
                                  arg_count,
                                  host->npp_,
                                  result);
  SetExceptionIfFailed(object, hr);
  return SUCCEEDED(hr);
}

bool DispatchHost::InvokeDefault(NPObject* object, const NPVariant* args,
                                 uint32_t arg_count, NPVariant* result) {
  DispatchHost* host = static_cast<DispatchHost*>(object);
  CORE_LOG(L3, (L"[DispatchHost::InvokeDefault][this=0x%p][dispatch=0x%p]",
                host, host->dispatch_));
  HRESULT hr = host->InvokeHelper(DISPID_VALUE,
                                  DISPATCH_METHOD | DISPATCH_PROPERTYGET,
                                  args,
                                  arg_count,
                                  host->npp_,
                                  result);
  SetExceptionIfFailed(object, hr);
  return SUCCEEDED(hr);
}

bool DispatchHost::HasProperty(NPObject* object, NPIdentifier name) {
  DispatchHost* host = static_cast<DispatchHost*>(object);
  DISPID dispatch_id = host->GetDispatchId(name);
  return dispatch_id != DISPID_UNKNOWN && host->IsProperty(dispatch_id);
}

bool DispatchHost::GetProperty(NPObject* object, NPIdentifier name,
                               NPVariant* result) {
  DispatchHost* host = static_cast<DispatchHost*>(object);
  CORE_LOG(L3, (L"[DispatchHost::GetProperty][this=0x%p][dispatch=0x%p]",
                host, host->dispatch_));
  HRESULT hr = host->InvokeHelper(host->GetDispatchId(name),
                                  DISPATCH_PROPERTYGET,
                                  NULL,
                                  0,
                                  host->npp_,
                                  result);
  SetExceptionIfFailed(object, hr);
  return SUCCEEDED(hr);
}

bool DispatchHost::SetProperty(NPObject* object, NPIdentifier name,
                               const NPVariant* value) {
  DispatchHost* host = static_cast<DispatchHost*>(object);
  CORE_LOG(L3, (L"[DispatchHost::SetProperty][this=0x%p][dispatch=0x%p]",
                host, host->dispatch_));
  DISPID dispatch_id = host->GetDispatchId(name);
  CComVariant dispatch_arg;
  NPVariantToVariant(host->npp_, *value, &dispatch_arg);
  HRESULT hr = host->dispatch_.PutProperty(dispatch_id, &dispatch_arg);
  SetExceptionIfFailed(object, hr);
  return SUCCEEDED(hr);
}

bool DispatchHost::RemoveProperty(NPObject* object, NPIdentifier name) {
  UNREFERENCED_PARAMETER(object);
  UNREFERENCED_PARAMETER(name);
  return false;
}

bool DispatchHost::Enumerate(NPObject* object, NPIdentifier** names,
                             uint32_t* count) {
  UNREFERENCED_PARAMETER(object);
  UNREFERENCED_PARAMETER(names);
  UNREFERENCED_PARAMETER(count);
  return false;
}

bool DispatchHost::Construct(NPObject* object, const NPVariant* args,
                             uint32_t arg_count, NPVariant* result) {
  UNREFERENCED_PARAMETER(object);
  UNREFERENCED_PARAMETER(args);
  UNREFERENCED_PARAMETER(arg_count);
  UNREFERENCED_PARAMETER(result);
  return false;
}

NPClass DispatchHost::kNPClass_ = {
  NP_CLASS_STRUCT_VERSION,
  Allocate,
  Deallocate,
  NULL,
  HasMethod,
  Invoke,
  InvokeDefault,
  HasProperty,
  GetProperty,
  SetProperty,
  RemoveProperty,
  Enumerate,
  Construct,
};

}  // namespace omaha
