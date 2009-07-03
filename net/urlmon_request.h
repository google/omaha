// Copyright 2008-2009 Google Inc.
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
// UrlmonRequest sends http transactions through urlmon.dll

#ifndef OMAHA_NET_URLMON_REQUEST_H__
#define OMAHA_NET_URLMON_REQUEST_H__

#include <windows.h>
#include <atlstr.h>
#include <atlsafe.h>
#include <map>
#include <vector>
#include "base/basictypes.h"
#include "goopdate/google_update_idl.h"
#include "omaha/common/debug.h"
#include "omaha/net/http_request.h"

namespace omaha {

class UrlmonRequest : public HttpRequestInterface {
 public:
  UrlmonRequest();
  virtual ~UrlmonRequest();

  // HttpRequestInterface methods.
  virtual HRESULT Close();

  virtual HRESULT Send();

  virtual HRESULT Cancel();

  virtual std::vector<uint8> GetResponse() const {
    return response_body_;
  }

  virtual int GetHttpStatusCode() const {
    return http_status_code_;
  }

  virtual HRESULT QueryHeadersString(uint32 info_level,
                                     const TCHAR* name,
                                     CString* value) const {
    // TODO(Omaha) - support for a custom name string.
    name;

    ASSERT1(value);
    value->Empty();
    std::map<DWORD, CString>::const_iterator cur =
        response_headers_map_.find(info_level);
    if (cur != response_headers_map_.end()) {
      *value = cur->second;
    }
    return !value->IsEmpty() ? S_OK : E_FAIL;
  }

  virtual CString GetResponseHeaders() const {
    return raw_response_headers_;
  }

  virtual CString ToString() const { return _T("urlmon"); }

  // No effect for this class.
  virtual void set_session_handle(HINTERNET) {}

  virtual void set_url(const CString& url) { url_ = url; }

  virtual void set_request_buffer(const void* buffer, size_t buffer_length) {
    request_buffer_ = buffer;
    request_buffer_length_ = buffer_length;
  }

  virtual void set_network_configuration(const Config& network_config) {
    network_config;
  }

  // Sets the filename to receive the response instead of the memory buffer.
  virtual void set_filename(const CString& filename) { filename_ = filename; }

  virtual void set_low_priority(bool low_priority) {
    low_priority;
  }

  virtual void set_callback(NetworkRequestCallback* callback) {
    // TODO(Omaha) - Provide events.
    callback;
  }

  virtual void set_additional_headers(const CString& additional_headers) {
    additional_headers_ = additional_headers;
  }

  virtual CString user_agent() const { return user_agent_; }

  virtual void set_user_agent(const CString& user_agent) {
    user_agent_ = user_agent;
  }

  virtual HRESULT SendRequest(BSTR url,
                              BSTR post_data,
                              BSTR request_headers,
                              VARIANT response_headers_needed,
                              CComVariant* response_headers,
                              DWORD* response_code,
                              BSTR* cache_filename);

 protected:
  CString user_agent_;

 private:
  HRESULT ProcessResponseHeaders(const CComVariant& headers,
                                 const CComSafeArray<DWORD>& headers_needed);
  HRESULT ProcessResponseFile(const CComBSTR& cache_filename);
  bool CreateBrowserHttpRequest();

  CComBSTR url_;
  CString filename_;
  const void* request_buffer_;          // Contains the request body for POST.
  size_t      request_buffer_length_;   // Length of the request body.
  CString additional_headers_;

  volatile LONG is_cancelled_;
  DWORD http_status_code_;
  std::vector<uint8> response_body_;
  std::map<DWORD, CString> response_headers_map_;
  CString raw_response_headers_;

  DISALLOW_EVIL_CONSTRUCTORS(UrlmonRequest);
};

}  // namespace omaha

#endif  // OMAHA_NET_URLMON_REQUEST_H__

