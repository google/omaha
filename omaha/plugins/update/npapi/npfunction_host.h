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
// NpFunctionHost hosts an NPObject inside an IDispatch interface to allow
// invoking NPAPI functions from a COM environment.  Types are automatically
// marshalled between NPVariant and VARIANT using the functions in
// variant_utils.h.  (For the reverse -- providing an NPObject interface to
// a COM object implementing IDispatch -- see DispatchHost.)
//
// Note that this currently only supports functions; this does not currently
// support objects.  NPN_Enumerate() only provides method/property names,
// not return types or argument counts/types, which makes it impossible to
// properly implement IDispatch::GetTypeInfo().  (However, we can implement
// GetIDsOfNames() if we need it in the future.)

#ifndef OMAHA_PLUGINS_UPDATE_NPAPI_NPFUNCTION_HOST_H_
#define OMAHA_PLUGINS_UPDATE_NPAPI_NPFUNCTION_HOST_H_

#include <atlbase.h>
#include <atlcom.h>

#include "base/basictypes.h"
#include "third_party/npapi/bindings/nphostapi.h"

namespace omaha {

class NpFunctionHostTest;

class ATL_NO_VTABLE NpFunctionHost
    : public CComObjectRootEx<CComObjectThreadModel>,
      public IDispatch {
 public:
  static HRESULT Create(NPP npp, NPObject* npobj, IDispatch** host);

  BEGIN_COM_MAP(NpFunctionHost)
    COM_INTERFACE_ENTRY(IDispatch)
  END_COM_MAP()

  // IDispatch methods.
  STDMETHOD(GetTypeInfoCount)(UINT* pctinfo);
  STDMETHOD(GetTypeInfo)(UINT iTInfo,
                         LCID lcid,
                         ITypeInfo** ppTInfo);
  STDMETHOD(GetIDsOfNames)(REFIID riid,
                           LPOLESTR* rgszNames,
                           UINT cNames,
                           LCID lcid,
                           DISPID* rgDispId);
  STDMETHOD(Invoke)(DISPID dispIdMember,
                    REFIID riid,
                    LCID lcid,
                    WORD wFlags,
                    DISPPARAMS* pDispParams,
                    VARIANT* pVarResult,
                    EXCEPINFO* pExcepInfo,
                    UINT* puArgErr);

  // CComObjectRootEx overrides.
  void FinalRelease();

 protected:
  NpFunctionHost();
  virtual ~NpFunctionHost() {}

 private:
  NPP npp_;
  NPObject* obj_;

  DISALLOW_COPY_AND_ASSIGN(NpFunctionHost);
};

}  // namespace omaha

#endif  // OMAHA_PLUGINS_UPDATE_NPAPI_NPFUNCTION_HOST_H_
