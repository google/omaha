// Copyright 2007-2010 Google Inc.
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
// HttpRequestInterface defines an interface for http transactions, with an
// optional number of retries, going over a specified network configuration.
//
// TODO(omaha): the class interface is not stable yet, as a few more
// getters and setters are still needed.
//
// TODO(omaha): provide a way to query how many bytes have been downloaded
// when the Send() returns so that the code doesn't have to rely on a final
// callback to update the progress information.

#ifndef OMAHA_NET_HTTP_REQUEST_H__
#define OMAHA_NET_HTTP_REQUEST_H__

#include <atlstr.h>
#include <vector>
#include "base/basictypes.h"
#include "omaha/net/network_config.h"

namespace omaha {

class NetworkRequestCallback;
struct DownloadMetrics;

class HttpRequestInterface {
 public:
  virtual ~HttpRequestInterface() {}

  virtual HRESULT Close() = 0;

  // Sends a fault-tolerant http request. Returns S_OK if the http transaction
  // went through and the correct response is available.
  virtual HRESULT Send() = 0;

  virtual HRESULT Cancel() = 0;

  virtual HRESULT Pause() = 0;

  virtual HRESULT Resume() = 0;

  virtual std::vector<uint8> GetResponse() const = 0;

  virtual int GetHttpStatusCode() const = 0;

  virtual HRESULT QueryHeadersString(uint32 info_level,
                                     const TCHAR* name,
                                     CString* value) const = 0;

  virtual CString GetResponseHeaders() const = 0;

  virtual CString ToString() const = 0;

  virtual void set_session_handle(HINTERNET session_handle) = 0;

  virtual void set_url(const CString& url) = 0;

  virtual void set_request_buffer(const void* buffer,
                                  size_t buffer_length) = 0;

  virtual void set_proxy_configuration(const ProxyConfig& proxy_config) = 0;

  // Sets the filename to receive the response instead of the memory buffer.
  virtual void set_filename(const CString& filename) = 0;

  virtual void set_low_priority(bool low_priority) = 0;

  virtual void set_callback(NetworkRequestCallback* callback) = 0;

  virtual void set_additional_headers(const CString& additional_headers) = 0;

  // Gets the user agent for this http request. The default user agent has
  // the following format: Google Update/a.b.c.d;req1;req2 where a.b.c.d is
  // the version of the client code and req1, req2,... are appended by
  // different http requests. For example:
  //    User-Agent: Google Update/1.2.15.0;winhttp;cup
  // indicates a WinHTTP+CUP request.
  virtual CString user_agent() const = 0;

  virtual void set_user_agent(const CString& user_agent) = 0;

  virtual void set_proxy_auth_config(const ProxyAuthConfig& config) = 0;

  // Returns true if download metrics are available for this HTTP request and
  // copies the metrics in the |download_metrics| function parameter.
  // Download metrics are available after the Send() call has returned and
  // they are meaningful for download requests only. Download requests are the
  // requests where the response goes to a file.
  virtual bool download_metrics(DownloadMetrics* download_metrics) const = 0;
};

}   // namespace omaha

#endif  // OMAHA_NET_HTTP_REQUEST_H__
