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

#ifndef OMAHA_PLUGINS_UPDATE_NPAPI_TESTING_DISPATCH_HOST_TEST_INTERFACE_H_
#define OMAHA_PLUGINS_UPDATE_NPAPI_TESTING_DISPATCH_HOST_TEST_INTERFACE_H_

#include <atlbase.h>
#include <atlcom.h>
#include <atlstr.h>

#include "base/basictypes.h"
#include "omaha/base/app_util.h"
#include "omaha/plugins/update/npapi/testing/resource.h"
#include "plugins/update/npapi/testing/dispatch_host_test_idl.h"

namespace omaha {

// This class allows for using IDispatchImpl with a specific TypeLib, in a
// module that has multiple TYPELIB resources. Ordinarily, IDispatchImpl will
// only load the first TYPELIB resource in the module, which does not work for
// omaha_unittest.exe. Hence the need for this class. This class loads the TLB
// with the specified resource id, and uses that for subsequent IDispatch
// requests.
// TODO(omaha3): Perhaps move this class into a generic utility header.

template <class T, const IID* piid = &__uuidof(T), const int tlb_res_id = 1>
class ATL_NO_VTABLE IDispatchImplResId : public IDispatchImpl<T, piid> {
 public:
  IDispatchImplResId() {
    CComPtr<ITypeLib> type_lib;
    CString tlb_path;

    // Format the path as "ModulePath\\ResourceId". Specifying a ResourceId
    // allows overriding the default behavior of LoadTypeLib to load the first
    // TYPELIB resource from the module.
    tlb_path.Format(_T("%s\\%d"), app_util::GetCurrentModuleName(), tlb_res_id);

    HRESULT hr = LoadTypeLib(tlb_path, &type_lib);
    if (FAILED(hr)) {
      return;
    }

    CComPtr<ITypeInfo> type_info;
    hr = type_lib->GetTypeInfoOfGuid(*piid, &type_info);
    if (FAILED(hr)) {
      return;
    }

    CComPtr<ITypeInfo2> type_info2;
    if (SUCCEEDED(type_info->QueryInterface(&type_info2))) {
      type_info = type_info2;
    }

    // Override the ITypeInfo in the CComTypeInfoHolder, which will be used in
    // subsequent calls to the IDispatch methods.
    _tih.m_pInfo = type_info.Detach();
  }

  virtual ~IDispatchImplResId() {}
};

class ATL_NO_VTABLE DispatchHostTestInterface
    : public CComObjectRootEx<CComObjectThreadModel>,
      public IDispatchImplResId<IDispatchHostTestInterface,
                                &__uuidof(IDispatchHostTestInterface),
                                IDR_DISPATCH_HOST_TEST_TLB> {
 public:
  DispatchHostTestInterface() {}
  virtual ~DispatchHostTestInterface() {}

  DECLARE_NOT_AGGREGATABLE(DispatchHostTestInterface);

  BEGIN_COM_MAP(DispatchHostTestInterface)
    COM_INTERFACE_ENTRY(IDispatch)
  END_COM_MAP()

  // IDispatchHostTestInterface methods.
  STDMETHOD(Random)(INT* x);

  STDMETHOD(get_Property)(INT* x);
  STDMETHOD(put_Property)(INT x);

  STDMETHOD(get_ReadOnlyProperty)(INT* x);
  STDMETHOD(put_WriteOnlyProperty)(INT x);

  STDMETHOD(AddAsMethod)(INT a, INT b, INT* c);
  STDMETHOD(get_AddAsProperty)(INT a, INT b, INT* c);

  STDMETHOD(DidYouMeanRecursion)(IDispatch** me);

 private:
  DISALLOW_COPY_AND_ASSIGN(DispatchHostTestInterface);
};

class ATL_NO_VTABLE DispatchHostTestInterface2
    : public CComObjectRootEx<CComObjectThreadModel>,
      public IDispatchImplResId<IDispatchHostTestInterface2,
                                &__uuidof(IDispatchHostTestInterface2),
                                IDR_DISPATCH_HOST_TEST_TLB> {
 public:
  DispatchHostTestInterface2() {}
  virtual ~DispatchHostTestInterface2() {}

  DECLARE_NOT_AGGREGATABLE(DispatchHostTestInterface2);

  BEGIN_COM_MAP(DispatchHostTestInterface)
    COM_INTERFACE_ENTRY(IDispatch)
  END_COM_MAP()

  // IDispatchHostTestInterface2 methods.
  STDMETHOD(get_Get)(INT index, INT* x);

 private:
  DISALLOW_COPY_AND_ASSIGN(DispatchHostTestInterface2);
};

}  // namespace omaha

#endif  // OMAHA_PLUGINS_UPDATE_NPAPI_TESTING_DISPATCH_HOST_TEST_INTERFACE_H_
