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
// Not used at the moment. The idea is to use an external process, such as
// the web browser in cases where GoogleUpdate.exe is not able to connect
// directly for some reason.
//
// TODO(Omaha) - Better algorithm for finding IBrowserHttpRequest2 objects:
// We currently use shared memory. We have the high integrity process
// read from shared memory that the low integrity process writes to. We assume
// a common executable, IEXPLORE.exe, which is limiting, because in the future
// we could be embedded in multiple processes.

// Another choice for detection will be more efficient overall. But it will
// entail some work: each IBrowserRequest will register itself with the ROT.
// It does this through a medium-integrity GoogleUpdate.exe. The high integrity
// process will then impersonate the explorer token, and then CoCreate
// GoogleUpdate.exe at medium integrity. Then the medium integrity GoogleUpdate
// can return an appropriate IBrowserRequest interface from the local ROT.
//
// This scheme is flexible enough that it can work regardless of which
// process the IBrowserRequest object(s) reside in. We could have a Firefox
// plugin, a Chrome-resident object, an object residing inside Google Talk, etc.

#include "omaha/net/browser_request.h"
#include <atlbase.h>
#include <atlcom.h>
#include "omaha/common/logging.h"
#include "omaha/common/system.h"
#include "omaha/common/vista_utils.h"
#include "omaha/goopdate/google_update_proxy.h"

namespace omaha {

BrowserRequest::BrowserRequest() {
  NET_LOG(L3, (_T("[BrowserRequest::BrowserRequest]")));
  user_agent_.Format(_T("%s;iexplore"), NetworkConfig::GetUserAgent());

#if 0
  if (!GetAvailableBrowserObjects()) {
    NET_LOG(LW, (_T("[BrowserRequest: No Browser Objects Found.]")));
  }
#endif
}

bool BrowserRequest::GetAvailableBrowserObjects() {
  std::vector<uint32> pids;
  HRESULT hr = vista::GetProcessPidsForActiveUserOrSession(kIExplore, &pids);
  if (FAILED(hr)) {
    NET_LOG(LE, (_T("[GetProcessPidsForActiveUserOrSession fail][0x%x]"), hr));
    return false;
  }

  bool is_system = false;
  hr = IsSystemProcess(&is_system);
  if (FAILED(hr)) {
    NET_LOG(LE, (_T("[IsSystemProcess failed][0x%x]"), hr));
    return false;
  }

  CString shared_memory_prefix;
  DWORD active_session = System::GetActiveSessionId();
  if (is_system && active_session != System::GetCurrentSessionId()) {
    // The Session\\ syntax references a local object in a different session.
    shared_memory_prefix.Format(_T("Session\\%d\\"), active_session);
  }
  shared_memory_prefix += kBrowserHttpRequestShareName;

  std::vector<uint32>::const_iterator iter = pids.begin();
  for (; iter != pids.end(); ++iter) {
    uint32 pid = *iter;
    CString shared_memory_name;
    shared_memory_name = shared_memory_prefix;
    shared_memory_name.AppendFormat(_T("%d"), pid);
    SharedMemoryAttributes default_attributes(shared_memory_name,
                                              CSecurityDesc());
    SharedMemoryProxy<IBrowserHttpRequest2, FakeGLock> proxy_read(
                                                           true,
                                                           &default_attributes);
    CComPtr<IBrowserHttpRequest2> browser_http_request;
    HRESULT hr = proxy_read.GetObject(&browser_http_request);
    if (FAILED(hr) || !browser_http_request) {
      NET_LOG(LW, (_T("[GetObject failed][%d][%d][0x%x]"),
                   pid, browser_http_request, hr));
      // Keep looking for more IBrowserHttpRequest2 objects.
      continue;
    }

    objects_.push_back(browser_http_request);
  }

  return !objects_.empty();
}

HRESULT BrowserRequest::SendRequest(BSTR url,
                                    BSTR post_data,
                                    BSTR request_headers,
                                    VARIANT response_headers_needed,
                                    CComVariant* response_headers,
                                    DWORD* response_code,
                                    BSTR* cache_filename_bstr) {
  NET_LOG(L3, (_T("[BrowserRequest::SendRequest]")));
  if (objects_.empty()) {
    NET_LOG(LE, (_T("[SendRequest: No Browser Objects available.]")));
    return E_FAIL;
  }

  BrowserObjects::const_iterator i = objects_.begin();
  HRESULT hr = E_UNEXPECTED;
  CComBSTR cache_filename;
  for (; i != objects_.end(); ++i) {
    CComPtr<IBrowserHttpRequest2> browser_object(*i);
    response_headers->Clear();
    *response_code = 0;
    cache_filename.Empty();
    hr = browser_object->Send(url,
                              post_data,
                              request_headers,
                              response_headers_needed,
                              response_headers,
                              response_code,
                              &cache_filename);
    NET_LOG(L3, (_T("[BrowserRequest::SendRequest][0x%x][%d][%s]"),
                 hr, *response_code, cache_filename));

    if (!*response_code) {
      continue;
    }

    *cache_filename_bstr = cache_filename.Detach();
    return S_OK;
  }

  return FAILED(hr) ? hr : E_UNEXPECTED;
}

}   // namespace omaha

