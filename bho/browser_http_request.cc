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

#include "omaha/bho/browser_http_request.h"
#include <atlsecurity.h>
#include <atlstr.h>
#include "omaha/common/error.h"
#include "omaha/common/scoped_any.h"
#include "omaha/net/bind_status_callback.h"

namespace omaha {

CComPtr<IBrowserHttpRequest2> BrowserHttpRequest::instance_;
LLock BrowserHttpRequest::lock_;
scoped_thread BrowserHttpRequest::request_thread_;
SharedBrowserRequestObj* BrowserHttpRequest::shared_obj_ = NULL;

// Use Urlmon to download.
STDMETHODIMP BrowserHttpRequest::Send(BSTR url,
                                      BSTR post_data,
                                      BSTR request_headers,
                                      VARIANT response_headers_needed,
                                      VARIANT* response_headers,
                                      DWORD* response_code,
                                      BSTR* cache_filename) {
  CORE_LOG(L2, (_T("[BrowserHttpRequest::Send][url \"%s\"]"), url));
  return BindStatusCallback::CreateAndSend(url,
                                           post_data,
                                           request_headers,
                                           response_headers_needed,
                                           response_headers,
                                           response_code,
                                           cache_filename);
}

// Creates and register static instance of the BrowserHttpRequest, if it does
// not exist.
HRESULT BrowserHttpRequest::Init() {
  CORE_LOG(L2, (_T("[BrowserHttpRequest::Register]")));

  __mutexBlock(lock_) {
    if (!request_thread_) {
      // Start thread that accepts incoming COM requests from GoogleUpdate.
      DWORD thread_id = 0;
      reset(request_thread_,
            ::CreateThread(NULL, 0, ProxyThreadProc, NULL, 0, &thread_id));
      if (!request_thread_) {
        HRESULT hr = HRESULTFromLastError();
        CORE_LOG(LE, (_T("::CreateThread failed 0x%x"), hr));
        return hr;
      }
      CORE_LOG(L2, (_T("[Init][request_thread_ id=%d]"), thread_id));
    }
  }

  return S_OK;
}

DWORD WINAPI BrowserHttpRequest::ProxyThreadProc(LPVOID /* parameter */) {
  CORE_LOG(L2, (_T("[BrowserHttpRequest::ProxyThreadProc]")));

  scoped_co_init init_com_apt;
  ASSERT1(SUCCEEDED(init_com_apt.hresult()));

  if (FAILED(init_com_apt.hresult()) || FAILED(CreateAndRegister())) {
    __mutexBlock(lock_) {
      // Try to recreate the request thread later.
      reset(request_thread_);
    }
    return 1;
  }

  // Message Loop
  MSG msg = {0};
  while (::GetMessage(&msg, NULL, 0, 0)) {
    if (WM_QUIT == msg.message)
      break;
    ::TranslateMessage(&msg);
    ::DispatchMessage(&msg);
  }

  // Since nobody posts a WM_QUIT message for this thread, shared_obj_ is
  // leaked. This is ok, because the DLL is not unloaded until the process
  // terminates, since we pin it. At that point, not releasing the shared_obj_
  // is inconsequential. The OS will release the file mapping associated with
  // the shared memory.
  return 0;
}

HRESULT BrowserHttpRequest::CreateAndRegister() {
  CORE_LOG(L2, (_T("[BrowserHttpRequest::CreateAndRegister]")));
  if (instance_ != NULL) {
    // Already exists. Exit early.
    return S_OK;
  }

  CComObject<BrowserHttpRequest>* http_request = NULL;
  HRESULT hr = CComObject<BrowserHttpRequest>::CreateInstance(&http_request);
  CORE_LOG(L2, (_T("[BrowserHttpRequest CreateInstance returned 0x%x]"), hr));
  if (FAILED(hr)) {
    return hr;
  }

  instance_ = http_request;
  hr = RegisterInSharedMemory(instance_);
  CORE_LOG(L2, (_T("[RegisterInSharedMemory returned 0x%x]"), hr));
  if (FAILED(hr)) {
    instance_ = NULL;
  }

  return hr;
}

// Insert the IBrowserHttpRequest2 interface into shared memory.
HRESULT BrowserHttpRequest::RegisterInSharedMemory(
            IBrowserHttpRequest2* http_request) {
  ASSERT1(http_request);

  // There is one browser helper per browser process. Each helper registers
  // itself into a separate shared memory suffixed with the PID of the process.
  // This is to keep the registration of each object distinct.
  // For example,"Local\\IBrowserRequest2_2229".
  // * GoogleUpdate.exe running as SYSTEM will choose the browser helper objects
  // registered and running in the currently active session.
  // * GoogleUpdate.exe running as the  user, either elevated or not, will
  // choose browser helpers registered in the same session and running as that
  // user.
  CString shared_memory_name(_T("Local\\"));
  shared_memory_name.AppendFormat(_T("%s%d"),
                                  omaha::kBrowserHttpRequestShareName,
                                  ::GetCurrentProcessId());
  omaha::SharedMemoryAttributes attr(shared_memory_name, CSecurityDesc());
  shared_obj_ = new SharedBrowserRequestObj(false, &attr);
  ASSERT1(shared_obj_ != NULL);
  return shared_obj_->RegisterObject(http_request);
}

}  // namespace omaha
