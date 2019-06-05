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

#ifndef OMAHA_NET_NETWORK_REQUEST_IMPL_H_
#define OMAHA_NET_NETWORK_REQUEST_IMPL_H_

#include <windows.h>
#include <atlstr.h>
#include <vector>

#include "base/basictypes.h"
#include "omaha/base/synchronized.h"
#include "omaha/net/network_config.h"
#include "omaha/net/network_request.h"
#include "omaha/net/http_request.h"
#include "omaha/third_party/smartany/scoped_any.h"

namespace omaha {

namespace internal {

// The class structure is as following:
//    - NetworkRequest and the underlying NetworkRequestImpl provide fault
//      tolerant client server http transactions.
//    - HttpRequestInterface defines an interface for different mechanisms that
//      can move bytes between the client and the server. These mechanisms are
//      chained up so that the control passes from one mechanism to the next
//      until one of them is able to fulfill the request or an error is
//      generated. Currently, SimpleRequest and BitsRequest are provided.
//    - HttpClient is the c++ wrapper over winhttp-wininet.
class NetworkRequestImpl {
 public:
  explicit NetworkRequestImpl(const NetworkConfig::Session& network_session);
  ~NetworkRequestImpl();

  HRESULT Close();

  void AddHttpRequest(HttpRequestInterface* http_request);

  HRESULT Post(const CString& url,
               const void* buffer,
               size_t length,
               std::vector<uint8>* response);
  HRESULT Get(const CString& url, std::vector<uint8>* response);
  HRESULT DownloadFile(const CString& url, const CString& filename);

  HRESULT Pause();
  HRESULT Resume();
  HRESULT Cancel();

  void AddHeader(const TCHAR* name, const TCHAR* value);

  HRESULT QueryHeadersString(uint32 info_level,
                             const TCHAR* name,
                             CString* value);

  int http_status_code() const { return http_status_code_; }

  CString response_headers() const { return response_headers_; }

  void set_proxy_auth_config(const ProxyAuthConfig& proxy_auth_config) {
    proxy_auth_config_ = proxy_auth_config;
  }

  void set_num_retries(int num_retries) { num_retries_ = num_retries; }

  void set_time_between_retries(int time_between_retries_ms) {
    initial_retry_delay_ms_ = time_between_retries_ms;
  }

  void set_retry_delay_jitter(int jitter_ms) {
    retry_delay_jitter_ms_ = jitter_ms;
  }

  void set_callback(NetworkRequestCallback* callback) {
    callback_ = callback;
  }

  void set_low_priority(bool low_priority) { low_priority_ = low_priority; }

  void set_proxy_configuration(const ProxyConfig* proxy_configuration) {
    if (proxy_configuration) {
      proxy_configuration_.reset(new ProxyConfig);
      *proxy_configuration_ = *proxy_configuration;
    } else {
      proxy_configuration_.reset();
    }
  }

  CString trace() const { return trace_; }

  std::vector<DownloadMetrics> download_metrics() const {
    return download_metrics_;
  }

  // Detects the available proxy configurations and returns the chain of
  // configurations to be used.
  void DetectProxyConfiguration(
      std::vector<ProxyConfig>* proxy_configurations) const;

 private:
  // Resets the state of the output data members.
  void Reset();

  // Sends the request with a retry policy. This is the only function that
  // modifies the state of the output data members: the status code, the
  // response headers, and the response. When errors are encountered, the
  // output data members contain the values corresponding to first network
  // configuration and first HttpRequest instance in the fallback chain.
  HRESULT DoSendWithRetries();

  // Sends a single request and receives the response.
  HRESULT DoSend(int* http_status_code,
                 CString* response_headers,
                 std::vector<uint8>* response);

  // Sends the request using the current configuration. The request is tried for
  // each HttpRequestInterface in the fallback chain until one of them succeeds
  // or the end of the chain is reached.
  HRESULT DoSendWithConfig(int* http_status_code,
                           CString* response_headers,
                           std::vector<uint8>* response);

  // Sends an http request using the current HttpRequest interface over the
  // current network configuration.
  HRESULT DoSendHttpRequest(int* http_status_code,
                            CString* response_headers,
                            std::vector<uint8>* response);

  // Returns true if we should continue to retry a network request, false if
  // we should bail out early.
  bool CanRetryRequest();

  // Delays for a specified interval, notifying any specified callbacks at the
  // beginning and end of the wait.
  HRESULT DoWaitBetweenRetries();

  // Adjusts the delay until the next retry, given the current retry status
  // and the result of the previous network attempt.
  void ComputeNextRetryDelay(HttpClient::StatusCodeClass previous_result);

  // Builds headers for the current HttpRequest and network configuration.
  CString BuildPerRequestHeaders() const;

  // Specifies the chain of HttpRequestInterface to handle the request.
  std::vector<HttpRequestInterface*> http_request_chain_;

  // Specifies the detected proxy configurations.
  std::vector<ProxyConfig> proxy_configurations_;

  // Specifies the proxy configuration override. When set, the proxy
  // configurations are not auto detected.
  std::unique_ptr<ProxyConfig> proxy_configuration_;

  // Input data members.
  // The request and response buffers are owner by the caller.
  CString  url_;
  const void* request_buffer_;     // Contains the request body for POST.
  size_t   request_buffer_length_;  // Length of the request body.
  CString  filename_;              // Contains the response for downloads.
  CString  additional_headers_;    // Headers common to all requests.
                                   // Each header is separated by \r\n.
  ProxyAuthConfig proxy_auth_config_;
  int      num_retries_;
  bool     low_priority_;
  int      initial_retry_delay_ms_;
  int      retry_delay_jitter_ms_;

  // Output data members.
  int      http_status_code_;
  CString  response_headers_;      // Each header is separated by \r\n.
  std::vector<uint8>* response_;   // Contains the response for Post and Get.

  const NetworkConfig::Session  network_session_;
  NetworkRequestCallback*       callback_;

  // The http request and the network configuration currently in use.
  HttpRequestInterface* cur_http_request_;
  const ProxyConfig*    cur_proxy_config_;

  // The HRESULT and HTTP status code updated by the prior
  // DoSendHttpRequest() call.
  HRESULT  last_hr_;
  int      last_http_status_code_;

  // Stores the last valid value of the optional X-Retry-After header or -1 if
  // the header was not present in the request. Only HTTPS X-Retry-After header
  // values are respected.
  // If the server sends a positive value in seconds for this header, the
  // request will stop processing fallbacks and will return from the
  // NetworkRequestImpl::DoSendWithRetries() call immediately.
  int retry_after_seconds_;

  // The current retry count and delay between retries, defined by the outermost
  // DoSendWithRetries() call.
  int cur_retry_count_;
  int cur_retry_delay_ms_;

  // Count of DoSendHttpRequest() calls.
  int http_attempts_;

  volatile LONG is_canceled_;
  scoped_event event_cancel_;

  LLock lock_;

  // Contains the trace of the request as handled by the fallback chain.
  CString trace_;

  std::vector<DownloadMetrics> download_metrics_;

  static const int kDefaultTimeBetweenRetriesMs      = 5000;    // 5 seconds.
  static const int kServerErrMinTimeBetweenRetriesMs = 20000;   // 20 seconds.
  static const int kMaxTimeBetweenRetriesMs          = 100000;  // 100 seconds.
  static const int kTimeBetweenRetriesMultiplier     = 2;
  static const int kDefaultRetryTimeJitterMs         = 3000;    // +- 3 seconds.

  DISALLOW_COPY_AND_ASSIGN(NetworkRequestImpl);
};


}   // namespace internal

}   // namespace omaha

#endif  // OMAHA_NET_NETWORK_REQUEST_IMPL_H_
