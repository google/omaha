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
// The main BHO COM object that IE instantiates. Creates a thread for
// registering itself with GoogleUpdate.exe. Does http requests on
// GoogleUpdate.exe's behalf.

#ifndef OMAHA_BHO_BHO_ENTRYPOINT_H__
#define OMAHA_BHO_BHO_ENTRYPOINT_H__

#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>
#include "bho/bho_dll.h"
#include "omaha/bho/resource.h"
#include "omaha/common/ATLRegMapEx.h"
#include "omaha/common/const_config.h"
#include "omaha/common/debug.h"
#include "omaha/common/logging.h"

namespace omaha {

class ATL_NO_VTABLE BhoEntrypoint
  : public CComObjectRootEx<CComObjectThreadModel>,
    public CComCoClass<BhoEntrypoint, &__uuidof(BhoEntrypointClass)>,
    public IObjectWithSiteImpl<BhoEntrypoint> {
 public:

  DECLARE_REGISTRY_RESOURCEID_EX(IDR_BHO_ENTRY)
  DECLARE_NOT_AGGREGATABLE(BhoEntrypoint)
  DECLARE_PROTECT_FINAL_CONSTRUCT()

  #pragma warning(push)
  // C4640: construction of local static object is not thread-safe
  #pragma warning(disable : 4640)
  BEGIN_REGISTRY_MAP()
    REGMAP_ENTRY(_T("PRODUCT"),           kCiProgram)
    REGMAP_ENTRY(_T("CLSID"),             __uuidof(BhoEntrypointClass))
  END_REGISTRY_MAP()
  #pragma warning(pop)

  BEGIN_COM_MAP(BhoEntrypoint)
    COM_INTERFACE_ENTRY(IObjectWithSite)
  END_COM_MAP()

  BhoEntrypoint();
  virtual ~BhoEntrypoint();

  STDMETHODIMP SetSite(IUnknown* site);
};

}  // namespace omaha

#endif  // OMAHA_BHO_BHO_ENTRYPOINT_H__

