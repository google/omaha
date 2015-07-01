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

#ifndef OMAHA_PLUGINS_UPDATE_NPAPI_URLPROPBAG_H_
#define OMAHA_PLUGINS_UPDATE_NPAPI_URLPROPBAG_H_

#include <atlbase.h>
#include <atlcom.h>
#include <atlstr.h>

#include "base/basictypes.h"

namespace omaha {

const TCHAR* const kUrlPropertyBag_Url = _T("omaha-urlpropertybag-url");

class ATL_NO_VTABLE UrlPropertyBag
    : public CComObjectRootEx<CComObjectThreadModel>,
      public IPropertyBag {
 public:
  static HRESULT Create(const TCHAR* url, IPropertyBag** pb);

  BEGIN_COM_MAP(UrlPropertyBag)
    COM_INTERFACE_ENTRY(IPropertyBag)
  END_COM_MAP()

  // IPropertyBag methods.
  STDMETHOD(Read)(LPCOLESTR pszPropName, VARIANT* pVar, IErrorLog* pErrorLog);
  STDMETHOD(Write)(LPCOLESTR pszPropName, VARIANT* pVar);

 protected:
  UrlPropertyBag();
  virtual ~UrlPropertyBag() {}

 private:
  CString url_;

  DISALLOW_COPY_AND_ASSIGN(UrlPropertyBag);
};

}  // namespace omaha

#endif  // OMAHA_PLUGINS_UPDATE_NPAPI_URLPROPBAG_H_
