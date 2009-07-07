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
// The class structure is as following:
//    - NetworkRequest and the underlying NetworkRequestImpl provide fault
//      tolerant client server http transactions.
//    - HttpRequestInterface defines an interface for different mechanisms that
//      can move bytes between the client and the server. These mechanisms are
//      chained up so that the control passes from one mechanism to the next
//      until one of them is able to fulfill the request or an error is
//      generated. Currently, SimpleRequest and BitsRequest are provided.
//    - HttpClient is the c++ wrapper over winhttp-wininet.

#ifndef OMAHA_NET_NETWORK_REQUEST_IMPL_H__
#define OMAHA_NET_NETWORK_REQUEST_IMPL_H__

#include <windows.h>
#include <atlstr.h>
#include <vector>
#include "base/basictypes.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/synchronized.h"
#include "omaha/net/network_config.h"
#include "omaha/net/network_request.h"
#include "omaha/net/http_request.h"

namespace omaha {

namespace detail {

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
  HRESULT Cancel();

  void AddHeader(const TCHAR* name, const TCHAR* value);

  HRESULT QueryHeadersString(uint32 info_level,
                             const TCHAR* name,
                             CString* value);

  int http_status_code() const { return http_status_code_; }

  CString response_headers() const { return response_headers_; }

  void set_num_retries(int num_retries) { num_retries_ = num_retries; }

  void set_time_between_retries(int time_between_retries_ms) {
    time_between_retries_ms_ = time_between_retries_ms;
  }

  void set_callback(NetworkRequestCallback* callback) {
    callback_ = callback;
  }

  void set_low_priority(bool low_priority) { low_priority_ = low_priority; }

  void set_network_configuration(const Config* network_configuration) {
    if (network_configuration) {
      network_configuration_.reset(new Config);
      *network_configuration_ = *network_configuration;
    } else {
      network_configuration_.reset();
    }
  }

  CString trace() const { return trace_; }

  // Detects the available network configurations and returns the chain of
  // configurations to be used.
  void DetectNetworkConfiguration(
      std::vector<Config>* network_configurations) const;

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
                 std::vector<uint8>* response) const;

  // Sends the request using the current configuration. The request is tried for
  // each HttpRequestInterface in the fallback chain until one of them succeeds
  // or the end of the chain is reached.
  HRESULT DoSendWithConfig(int* http_status_code,
                           CString* response_headers,
                           std::vector<uint8>* response) const;

  // Sends an http request using the current HttpRequest interface over the
  // current network configuration.
  HRESULT DoSendHttpRequest(int* http_status_code,
                            CString* response_headers,
                            std::vector<uint8>* response) const;

  // Specifies the chain of HttpRequestInterface to handle the request.
  std::vector<HttpRequestInterface*> http_request_chain_;

  // Specifies the detected network configurations.
  std::vector<Config> network_configurations_;

  // Specifies the network configuration override. When set, the network
  // configurations are not auto detected.
  scoped_ptr<Config> network_configuration_;

  // Input data members.
  // The request and response buffers are owner by the caller.
  CString  url_;
  const void* request_buffer_;     // Contains the request body for POST.
  size_t   request_buffer_length_;  // Length of the request body.
  CString  filename_;              // Contains the response for downloads.
  CString  additional_headers_;    // Each header is separated by \r\n.
  int      num_retries_;
  bool     low_priority_;
  int time_between_retries_ms_;

  // Output data members.
  int      http_status_code_;
  CString  response_headers_;      // Each header is separated by \r\n.
  std::vector<uint8>* response_;   // Contains the response for Post and Get.
  HRESULT  last_network_error_;    // TODO(omaha): not implemented.

  const NetworkConfig::Session  network_session_;
  NetworkRequestCallback*       callback_;

  // The http request and the network configuration currently in use.
  mutable HttpRequestInterface* cur_http_request_;
  mutable const Config*         cur_network_config_;

  volatile LONG is_canceled_;
  scoped_event event_cancel_;

  LLock lock_;

  // Contains the trace of the request as handled by the fallback chain.
  mutable CString trace_;

  static const int kDefaultTimeBetweenRetriesMs   = 5000;     // 5 seconds.
  static const int kTimeBetweenRetriesMultiplier  = 2;

  DISALLOW_EVIL_CONSTRUCTORS(NetworkRequestImpl);
};

HRESULT PostRequest(NetworkRequest* network_request,
                    bool fallback_to_https,
                    const CString& url,
                    const CString& request_string,
                    CString* response);

HRESULT GetRequest(NetworkRequest* network_request,
                   const CString& url,
                   CString* response);

}   // namespace detail

}   // namespace omaha

#endif  // OMAHA_NET_NETWORK_REQUEST_IMPL_H__

