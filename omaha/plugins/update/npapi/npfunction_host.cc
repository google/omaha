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

#include "omaha/plugins/update/npapi/npfunction_host.h"

#include "omaha/base/debug.h"
#include "omaha/base/utils.h"
#include "omaha/plugins/update/npapi/variant_utils.h"

namespace omaha {

typedef CComObject<NpFunctionHost> CComNpFuncHost;
typedef scoped_any<CComNpFuncHost*, close_release_com, null_t> scoped_host;

HRESULT NpFunctionHost::Create(NPP npp, NPObject* npobj, IDispatch** host) {
  ASSERT1(npobj);
  ASSERT1(host);

  if (!npobj || !host) {
    return E_INVALIDARG;
  }

  // Create the host and hand off the NPObject to it.
  scoped_host comobj;
  HRESULT hr = CComNpFuncHost::CreateInstance(address(comobj));
  if (FAILED(hr)) {
    return hr;
  }
  get(comobj)->AddRef();

  comobj->npp_ = npp;
  comobj->obj_ = npobj;
  NPN_RetainObject(npobj);

  return comobj->QueryInterface(host);
}

STDMETHODIMP NpFunctionHost::GetTypeInfoCount(UINT* pctinfo) {
  if (pctinfo == NULL) {
    return E_INVALIDARG;
  }
  *pctinfo = 0;
  return S_OK;
}

STDMETHODIMP NpFunctionHost::GetTypeInfo(UINT iTInfo,
                                         LCID lcid,
                                         ITypeInfo** ppTInfo) {
  UNREFERENCED_PARAMETER(iTInfo);
  UNREFERENCED_PARAMETER(lcid);
  UNREFERENCED_PARAMETER(ppTInfo);

  return E_NOTIMPL;
}

STDMETHODIMP NpFunctionHost::GetIDsOfNames(REFIID riid,
                                           LPOLESTR* rgszNames,
                                           UINT cNames,
                                           LCID lcid,
                                           DISPID* rgDispId) {
  UNREFERENCED_PARAMETER(riid);
  UNREFERENCED_PARAMETER(rgszNames);
  UNREFERENCED_PARAMETER(cNames);
  UNREFERENCED_PARAMETER(lcid);
  UNREFERENCED_PARAMETER(rgDispId);

  return E_NOTIMPL;
}

STDMETHODIMP NpFunctionHost::Invoke(DISPID dispIdMember,
                                    REFIID riid,
                                    LCID lcid,
                                    WORD wFlags,
                                    DISPPARAMS* pDispParams,
                                    VARIANT* pVarResult,
                                    EXCEPINFO* pExcepInfo,
                                    UINT* puArgErr) {
  UNREFERENCED_PARAMETER(dispIdMember);
  UNREFERENCED_PARAMETER(riid);
  UNREFERENCED_PARAMETER(lcid);
  UNREFERENCED_PARAMETER(pExcepInfo);
  UNREFERENCED_PARAMETER(puArgErr);

  if (wFlags != DISPATCH_METHOD) {
    return DISP_E_MEMBERNOTFOUND;
  }

  uint32_t num_args = 0;
  scoped_array<NPVariant> arguments;
  if (pDispParams) {
    // Javascript doesn't officially support named args, so the current
    // implementation ignores any named args that are supplied.  However,
    // you can cast a function object to a string and it will hold the
    // argument names as used in the function definition.  Thus, if we
    // need to support named arguments in the future, we may be able to
    // get the argument names indirectly using NPN_Evaluate() and emulate.
    if (pDispParams->cNamedArgs != 0) {
      return DISP_E_NONAMEDARGS;
    }
    if (pDispParams->cArgs != 0) {
      num_args = pDispParams->cArgs;
      arguments.reset(new NPVariant[num_args]);
      for (uint32_t i = 0; i < num_args; ++i) {
        // Arguments are stored in rgvarg in reverse order.
        VariantToNPVariant(npp_,
                           pDispParams->rgvarg[num_args - 1 - i],
                           &arguments[i]);
      }
    }
  }

  NPVariant retval;
  VOID_TO_NPVARIANT(retval);

  bool result = NPN_InvokeDefault(npp_,
                                  obj_,
                                  arguments.get(),
                                  num_args,
                                  &retval);
  if (result && pVarResult) {
    NPVariantToVariant(npp_, retval, pVarResult);
  }
  NPN_ReleaseVariantValue(&retval);

  return result ? S_OK : E_FAIL;
}

void NpFunctionHost::FinalRelease() {
  ASSERT1(obj_);
  if (obj_) {
    NPN_ReleaseObject(obj_);
    obj_ = NULL;
  }
}

NpFunctionHost::NpFunctionHost() : npp_(NULL), obj_(NULL) {}

}  // namespace omaha

