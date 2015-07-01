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

#include "omaha/plugins/update/npapi/urlpropbag.h"

#include "omaha/base/debug.h"
#include "omaha/base/utils.h"
#include "omaha/plugins/update/npapi/variant_utils.h"

namespace omaha {

typedef CComObject<UrlPropertyBag> CComUrlBag;
typedef scoped_any<CComUrlBag*, close_release_com, null_t> scoped_bag;

HRESULT UrlPropertyBag::Create(const TCHAR* url, IPropertyBag** pb) {
  ASSERT1(url);
  ASSERT1(pb);

  if (!url || !pb) {
    return E_INVALIDARG;
  }

  // Create the host and hand off the string to it.
  scoped_bag comobj;
  HRESULT hr = CComUrlBag::CreateInstance(address(comobj));
  if (FAILED(hr)) {
    return hr;
  }
  get(comobj)->AddRef();

  comobj->url_ = url;

  return comobj->QueryInterface(pb);
}

STDMETHODIMP UrlPropertyBag::Read(LPCOLESTR pszPropName, VARIANT* pVar,
                                  IErrorLog* pErrorLog) {
  UNREFERENCED_PARAMETER(pErrorLog);

  ASSERT1(pszPropName);
  ASSERT1(pVar);
  if (!pszPropName || !pVar) {
    return E_POINTER;
  }

  if (0 == _tcscmp(pszPropName, kUrlPropertyBag_Url)) {
    V_VT(pVar) = VT_BSTR;
    V_BSTR(pVar) = url_.AllocSysString();
    return S_OK;
  }

  return E_INVALIDARG;
}

STDMETHODIMP UrlPropertyBag::Write(LPCOLESTR pszPropName, VARIANT* pVar) {
  UNREFERENCED_PARAMETER(pszPropName);
  UNREFERENCED_PARAMETER(pVar);
  return E_FAIL;
}

UrlPropertyBag::UrlPropertyBag() {}

}  // namespace omaha

