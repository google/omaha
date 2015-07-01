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

#ifndef OMAHA_NET_NETWORK_REQUEST_H_
#define OMAHA_NET_NETWORK_REQUEST_H_

#include <windows.h>
#include <winhttp.h>
#include <atlstr.h>
#include <vector>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "omaha/base/string.h"
#include "omaha/base/time.h"
#include "omaha/common/ping_event_download_metrics.h"
#include "omaha/net/network_config.h"

namespace omaha {

namespace internal {

class NetworkRequestImpl;

}   // namespace internal

class NetworkRequestCallback {
 public:
  virtual ~NetworkRequestCallback() {}

  // Notifies download begins. This gives the callee a chance to initialize its
  // download state, such as resetting the download progress.
  virtual void OnRequestBegin() = 0;

  // Indicates the progress of the NetworkRequest.
  //
  // bytes - Current number of bytes transferred, relative to the expected
  //         maximum indicated by the bytes_total.
  // bytes_total - The expected total number of bytes to transfer. This is
  //               usually the content length value or 0 if the content length
  //               is not available.
  // status - WinHttp status codes regarding the progress of the request.
  // status_text - Additional information, when available.
  virtual void OnProgress(int bytes, int bytes_total,
                          int status, const TCHAR* status_text) = 0;

  virtual void OnRequestRetryScheduled(time64 next_retry_time) = 0;
};

class  HttpRequestInterface;

// NetworkRequest is the main interface to the net module. The semantics of
// the interface is defined as transferring bytes from a url, with an optional
// request body, to a destination specified as a memory buffer or a file.
// NetworkRequest encapsulates the retry logic for a request, the fall back to
// different network configurations, and ultimately the fallback to different
// network mechanisms. The calls are blocking calls. One instance of the class
// is responsible for one request only.
// The client of NetworkRequest is responsible for configuring the network
// fallbacks. This gives the caller a lot of flexibility over the fallbacks but
// it requires more work to properly set the fallback chain.
class NetworkRequest {
 public:
  explicit NetworkRequest(const NetworkConfig::Session& network_session);
  ~NetworkRequest();

  // Cancels the request. Cancel can be called from a different thread.
  // Calling Cancel makes the control return to the caller of Post or Get
  // methods. The object can't be reused once it is canceled.
  HRESULT Cancel();

  // Closes the request and releases the request resources, such as handles or
  // interface pointers. Unlike Cancel, the request object can be reused
  // after closing.
  HRESULT Close();

  // Adds an instance of HttpRequestInterface at the end of the fallback
  // chain of http requests. This class takes ownership of the http_request
  // object.
  void AddHttpRequest(HttpRequestInterface* http_request);

  // Post, Get, and DownloadFile return S_OK if the request completed
  // successfully and the response is available to the caller. For errors,
  // the status code can be checked.
  //
  // Posts a buffer to a url.
  HRESULT Post(const CString& url,
               const void* buffer,
               size_t length,
               std::vector<uint8>* response);

  // Posts an UTF-8 string to a url.
  HRESULT PostUtf8String(const CString& url,
                         const CStringA& request,
                         std::vector<uint8>* response) {
    const uint8* buffer = reinterpret_cast<const uint8*>(request.GetString());
    size_t length = request.GetLength();
    return Post(url, buffer, length, response);
  }

  // Posts a Unicode string to a url.
  HRESULT PostString(const CString& url,
                     const CString& request,
                     std::vector<uint8>* response) {
    return PostUtf8String(url, WideToUtf8(request), response);
  }

  // Gets a url.
  HRESULT Get(const CString& url, std::vector<uint8>* response);

  // Downloads a url to a file.
  HRESULT DownloadFile(const CString& url, const CString& filename);

  // Enables a separate thread to temporarily stops the network downloading.
  // The downloading thread will be blocked inside NetworkRequest::Post/Get
  // infinitely by an event until Resume/Cancel/Close is called.
  HRESULT Pause();

  // Enables a separate thread to resume current network request if it is in
  // pause state.
  HRESULT Resume();

  // Adds a request header. The header with the same name is only added once.
  // The method is only good enough for what Omaha needs and it is not good
  // for general purpose header manipulation, which is quite sophisticated.
  void AddHeader(const TCHAR* name, const TCHAR* value);

  // Queries a response header. This is the companion for the AddHeader
  // method above.
  HRESULT QueryHeadersString(uint32 info_level,
                             const TCHAR* name,
                             CString* value);

  // Returns the http status code if available.
  int http_status_code() const;

  // Returns the response headers, separated by \r\n.
  CString response_headers() const;

  // Returns a trace of the request for logging purposes.
  CString trace() const;

  // Returns the download metrics corresponding to a download request.
  std::vector<DownloadMetrics> download_metrics() const;

  void set_proxy_auth_config(const ProxyAuthConfig& proxy_auth_config);

  // Sets the number of retries for the request. The retry mechanism uses
  // exponential backoff to decrease the rate of the retries.
  void set_num_retries(int num_retries);

  // Sets the initial time, in milliseconds, between retries.  This will be
  // the base amount for the exponential delay growth.  (Default value is set
  // in network_request_impl.h -- currently, it is 5 seconds.)
  void set_time_between_retries(int time_between_retries_ms);

  // Sets a +/- jitter, in milliseconds, to be applied to each retry delay.
  // Set to 0 (or any negative value) to disable delay jittering.  (Default
  // value is set in network_request_impl.h -- currently, it is 3 seconds.)
  void set_retry_delay_jitter(int jitter_ms);

  // Sets an external observer for the request. The ownership of the callback
  // remains with the caller. The current implementation provides callback
  // notification for DownloadFile only.
  void set_callback(NetworkRequestCallback* callback);

  // Sets the priority of the request. Currently, only BITS requests support
  // prioritization of requests.
  void set_low_priority(bool low_priority);

  // Overrides detecting the network configuration and uses the configuration
  // specified. If parameter is NULL, it defaults to detecting the configuration
  // automatically.
  void set_proxy_configuration(const ProxyConfig* proxy_configuration);

 private:
  // Uses pimpl idiom to minimize dependencies on implementation details.
  scoped_ptr<internal::NetworkRequestImpl> impl_;
  DISALLOW_EVIL_CONSTRUCTORS(NetworkRequest);
};

}   // namespace omaha

#endif  // OMAHA_NET_NETWORK_REQUEST_H_

