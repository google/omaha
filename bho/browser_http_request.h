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
// Browser Http Request class. Created by BhoEntrypoint. Creates a thread that
// registers with GoogleUpdate.exe, and waits for calls into the Send() method.

#ifndef OMAHA_BHO_BROWSER_HTTP_REQUEST_H__
#define OMAHA_BHO_BROWSER_HTTP_REQUEST_H__

#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>
#include "base/scoped_ptr.h"
#include "goopdate/google_update_idl.h"
#include "omaha/common/debug.h"
#include "omaha/common/logging.h"
#include "omaha/common/synchronized.h"
#include "omaha/common/utils.h"
#include "omaha/goopdate/google_update_proxy.h"

namespace omaha {

typedef omaha::SharedMemoryProxy<IBrowserHttpRequest2,
                                 FakeGLock> SharedBrowserRequestObj;

class ATL_NO_VTABLE BrowserHttpRequest
  : public CComObjectRootEx<CComObjectThreadModel>,
    public IBrowserHttpRequest2 {
 public:
  BrowserHttpRequest() {}
  virtual ~BrowserHttpRequest() {
    CORE_LOG(L2, (_T("[BrowserHttpRequest::~BrowserHttpRequest]")));
  }

  BEGIN_COM_MAP(BrowserHttpRequest)
    COM_INTERFACE_ENTRY(IBrowserHttpRequest2)
  END_COM_MAP()

  DECLARE_PROTECT_FINAL_CONSTRUCT()

  STDMETHOD(Send)(BSTR url,
                  BSTR post_data,
                  BSTR request_headers,
                  VARIANT response_headers_needed,
                  VARIANT* response_headers,
                  DWORD* response_code,
                  BSTR* cache_filename);

  static HRESULT Init();

 private:
  static DWORD WINAPI ProxyThreadProc(void* parameter);
  static HRESULT CreateAndRegister();
  static HRESULT RegisterInSharedMemory(IBrowserHttpRequest2* http_request);

  static CComPtr<IBrowserHttpRequest2> instance_;
  static LLock lock_;
  static scoped_thread request_thread_;

  // The browser http request shared with other processes.
  static SharedBrowserRequestObj* shared_obj_;
};

}  // namespace omaha

#endif  // OMAHA_BHO_BROWSER_HTTP_REQUEST_H__

