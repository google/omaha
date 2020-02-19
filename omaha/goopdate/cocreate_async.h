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

#ifndef OMAHA_GOOPDATE_COCREATE_ASYNC_H_
#define OMAHA_GOOPDATE_COCREATE_ASYNC_H_

#include <atlbase.h>
#include <oaidl.h>
#include <oleauto.h>
#include "goopdate/omaha3_idl.h"
#include "omaha/base/atlregmapex.h"
#include "omaha/base/synchronized.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/goopdate/com_proxy.h"
#include "omaha/goopdate/non_localized_resource.h"

namespace omaha {

class ATL_NO_VTABLE CoCreateAsync
  : public CComObjectRootEx<CComObjectThreadModel>,
    public CComCoClass<CoCreateAsync, &__uuidof(CoCreateAsyncClass)>,
    public ICoCreateAsync,
    public StdMarshalInfo {
 public:
  CoCreateAsync();

  STDMETHOD(createOmahaMachineServerAsync)(BSTR origin_url,
                                           BOOL create_elevated,
                                           ICoCreateAsyncStatus** status);

  DECLARE_NOT_AGGREGATABLE(CoCreateAsync);
  DECLARE_REGISTRY_RESOURCEID_EX(IDR_LOCAL_SERVER_RGS)

  BEGIN_REGISTRY_MAP()
    REGMAP_ENTRY(_T("HKROOT"),       goopdate_utils::GetHKRoot())
    REGMAP_MODULE2(_T("MODULE"),     kOmahaBrokerFileName)
    REGMAP_ENTRY(_T("VERSION"),      _T("1.0"))
    REGMAP_ENTRY(_T("PROGID"),       kProgIDCoCreateAsync)
    REGMAP_ENTRY(_T("DESCRIPTION"),  _T("CoCreateAsync"))
    REGMAP_UUID(_T("CLSID"),         GetObjectCLSID())
  END_REGISTRY_MAP()

  BEGIN_COM_MAP(CoCreateAsync)
    COM_INTERFACE_ENTRY(ICoCreateAsync)
    COM_INTERFACE_ENTRY(IStdMarshalInfo)
  END_COM_MAP()

 protected:
  virtual ~CoCreateAsync() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(CoCreateAsync);
};

class ATL_NO_VTABLE CoCreateAsyncStatus
  : public CComObjectRootEx<CComObjectThreadModel>,
    public IDispatchImpl<ICoCreateAsyncStatus,
                         &__uuidof(ICoCreateAsyncStatus),
                         &CAtlModule::m_libid,
                         kMajorTypeLibVersion,
                         kMinorTypeLibVersion> {
 public:
  CoCreateAsyncStatus();
  HRESULT CreateOmahaMachineServerAsync(BSTR origin_url, BOOL create_elevated);

  // ICoCreateAsyncStatus.
  STDMETHOD(get_isDone)(VARIANT_BOOL* is_done);
  STDMETHOD(get_completionHResult)(LONG* hr);
  STDMETHOD(get_createdInstance)(IDispatch** instance);

  BEGIN_COM_MAP(CoCreateAsyncStatus)
    COM_INTERFACE_ENTRY(ICoCreateAsyncStatus)
    COM_INTERFACE_ENTRY(IDispatch)
  END_COM_MAP()

 protected:
  virtual ~CoCreateAsyncStatus() {}

 private:
  void CreateOmahaMachineServer(const CString origin_url, BOOL create_elevated);
  void SetCreateInstanceResults(const HRESULT& hr,
                                const CComPtr<IDispatch>& ptr);

  bool is_done_;
  HRESULT hr_;
  CComPtr<IDispatch> ptr_;

  DISALLOW_COPY_AND_ASSIGN(CoCreateAsyncStatus);
};

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_COCREATE_ASYNC_H_
