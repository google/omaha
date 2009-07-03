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
// IBindStatusCallback interface. Support for HTTP POST.

#ifndef OMAHA_NET_BIND_STATUS_CALLBACK_H__
#define OMAHA_NET_BIND_STATUS_CALLBACK_H__

#include <windows.h>
#include <urlmon.h>
#include <atlbase.h>
#include <atlcom.h>
#include <atlsafe.h>
#include <atlstr.h>
#include "omaha/common/scoped_any.h"

namespace omaha {

class ATL_NO_VTABLE BindStatusCallback
    : public CComObjectRootEx<CComObjectThreadModel>,
      public IBindStatusCallback,
      public IHttpNegotiate {
 public:
  static HRESULT CreateAndSend(BSTR url,
                               BSTR post_data,
                               BSTR request_headers,
                               VARIANT response_headers_needed,
                               VARIANT* response_headers,
                               DWORD* response_code,
                               BSTR* cache_filename);

  // C4505: unreferenced IUnknown local functions have been removed
  #pragma warning(disable : 4505)
  BEGIN_COM_MAP(BindStatusCallback)
    COM_INTERFACE_ENTRY(IBindStatusCallback)
    COM_INTERFACE_ENTRY(IHttpNegotiate)
  END_COM_MAP()

  // IBindStatusCallback methods.
  STDMETHODIMP OnStartBinding(DWORD reserved, IBinding* binding);
  STDMETHODIMP GetPriority(LONG* priority);
  STDMETHODIMP OnLowResource(DWORD reserved);
  STDMETHODIMP OnProgress(ULONG, ULONG, ULONG, LPCWSTR);
  STDMETHODIMP OnStopBinding(HRESULT, LPCWSTR);
  STDMETHODIMP GetBindInfo(DWORD* bindf_flags, BINDINFO* bind_info);
  STDMETHODIMP OnDataAvailable(DWORD, DWORD, FORMATETC*, STGMEDIUM*);
  STDMETHODIMP OnObjectAvailable(REFIID, IUnknown*);

  // IHttpNegotiate methods
  STDMETHODIMP BeginningTransaction(LPCWSTR url,
                                    LPCWSTR request_headers,
                                    DWORD reserved,
                                    LPWSTR* additional_headers);
  STDMETHODIMP OnResponse(DWORD response_code,
                          LPCWSTR response_headers,
                          LPCWSTR request_headers,
                          LPWSTR* additional_request_headers);

 protected:
  BindStatusCallback();
  virtual ~BindStatusCallback() {}

 private:
  HRESULT Init(BSTR post_data,
               BSTR request_headers,
               VARIANT response_headers_needed,
               VARIANT* response_headers,
               DWORD* response_code);

 private:
  BINDVERB              http_verb_;
  scoped_hglobal        post_data_;
  DWORD                 post_data_byte_count_;
  CString               request_headers_;
  CComSafeArray<DWORD>  response_headers_needed_;
  VARIANT*              response_headers_;
  DWORD*                response_code_;
  CComPtr<IBinding>     binding_;
};

}  // namespace omaha

#endif  // OMAHA_NET_BIND_STATUS_CALLBACK_H__

