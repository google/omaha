// Copyright 2013 Google Inc.
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
// CupEcdsaRequest provides CUP-ECDSA capabilities for a generic
// HttpRequestInterface.

#ifndef OMAHA_NET_CUP_ECDSA_REQUEST_H__
#define OMAHA_NET_CUP_ECDSA_REQUEST_H__

#include <windows.h>
#include <atlstr.h>
#include <vector>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "omaha/net/http_request.h"

namespace omaha {

namespace internal {

// The implementation uses a pimpl pattern to encapsulate CUP-ECDSA's
// protocol implementation details from the calling code.
class CupEcdsaRequestImpl;

}   // namespace internal

class CupEcdsaRequest : public HttpRequestInterface {
 public:
  // Decorates an HttpRequestInterface to provide CUP-ECDSA functionality.
  // It takes ownership of the object provided as parameter.
  explicit CupEcdsaRequest(HttpRequestInterface* http_request);

  virtual ~CupEcdsaRequest();

  virtual HRESULT Close();

  virtual HRESULT Send();

  virtual HRESULT Cancel();

  virtual HRESULT Pause();

  virtual HRESULT Resume();

  virtual std::vector<uint8> GetResponse() const;

  virtual HRESULT QueryHeadersString(uint32 info_level,
                                     const TCHAR* name,
                                     CString* value) const;
  virtual CString GetResponseHeaders() const;

  virtual int GetHttpStatusCode() const;

  virtual CString ToString() const;

  virtual void set_session_handle(HINTERNET session_handle);

  virtual void set_url(const CString& url);

  virtual void set_request_buffer(const void* buffer, size_t buffer_length);

  virtual void set_proxy_configuration(const ProxyConfig& proxy_config);

  // Sets the filename to receive the response instead of the memory buffer.
  virtual void set_filename(const CString& filename);

  virtual void set_low_priority(bool low_priority);

  virtual void set_callback(NetworkRequestCallback* callback);

  virtual void set_additional_headers(const CString& additional_headers);

  virtual CString user_agent() const;

  virtual void set_user_agent(const CString& user_agent);

  virtual void set_proxy_auth_config(const ProxyAuthConfig& proxy_auth_config);

  virtual bool download_metrics(DownloadMetrics* download_metrics) const;

 private:
  friend class CupEcdsaRequestTest;

  scoped_ptr<internal::CupEcdsaRequestImpl> impl_;
  DISALLOW_COPY_AND_ASSIGN(CupEcdsaRequest);
};

}   // namespace omaha

#endif  // OMAHA_NET_CUP_ECDSA_REQUEST_H__
