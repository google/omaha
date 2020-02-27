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
// SimpleRequest provides for http transactions using WinHttp/WinInet.
//
// TODO(omaha): the class interface is not stable yet, as a few more
// getters and setters are still needed.
//
// TODO(omaha): receiving a response into a file is not implemented yet.

#ifndef OMAHA_NET_SIMPLE_REQUEST_H_
#define OMAHA_NET_SIMPLE_REQUEST_H_

#include <atlstr.h>
#include <memory>
#include <vector>

#include "base/basictypes.h"
#include "omaha/base/debug.h"
#include "omaha/base/synchronized.h"
#include "omaha/net/http_request.h"
#include "omaha/net/network_config.h"
#include "omaha/third_party/smartany/scoped_any.h"

namespace omaha {

class WinHttpAdapter;
struct DownloadMetrics;

class SimpleRequest : public HttpRequestInterface {
 public:
  SimpleRequest();
  virtual ~SimpleRequest();

  virtual HRESULT Close();

  virtual HRESULT Send();

  virtual HRESULT Cancel();

  virtual HRESULT Pause();

  virtual HRESULT Resume();

  virtual std::vector<uint8> GetResponse() const;

  virtual int GetHttpStatusCode() const {
    return request_state_.get() ? request_state_->http_status_code : 0;
  }

  virtual HRESULT QueryHeadersString(uint32 info_level,
                                     const TCHAR* name,
                                     CString* value) const;

  virtual CString GetResponseHeaders() const;

  virtual CString ToString() const { return _T("winhttp"); }

  virtual void set_session_handle(HINTERNET session_handle) {
    session_handle_ = session_handle;
  }

  virtual void set_url(const CString& url);

  virtual void set_request_buffer(const void* buffer, size_t buffer_length) {
    request_buffer_ = buffer;
    request_buffer_length_ = buffer_length;
  }

  virtual void set_proxy_configuration(const ProxyConfig& proxy_config) {
    proxy_config_ = proxy_config;
  }

  // Sets the filename to receive the response instead of the memory buffer.
  virtual void set_filename(const CString& filename);

  virtual void set_low_priority(bool low_priority) {
    low_priority_ = low_priority;
  }

  virtual void set_callback(NetworkRequestCallback* callback) {
    callback_ = callback;
  }

  virtual void set_additional_headers(const CString& additional_headers) {
    additional_headers_ = additional_headers;
  }

  virtual CString user_agent() const { return user_agent_; }

  virtual void set_user_agent(const CString& user_agent) {
    user_agent_ = user_agent;
  }

  virtual void set_proxy_auth_config(const ProxyAuthConfig& proxy_auth_config) {
    proxy_auth_config_ = proxy_auth_config;
  }

  virtual bool download_metrics(DownloadMetrics* download_metrics) const;

 private:
  HRESULT DoSend();
  HRESULT OpenDestinationFile(HANDLE* file_handle);
  HRESULT PrepareRequest(HANDLE* file_handle);
  HRESULT Connect();
  HRESULT SendRequest();
  HRESULT ReceiveData(HANDLE file_handle);
  HRESULT RequestData(HANDLE file_handle);
  bool IsResumeNeeded() const;
  bool IsPauseSupported() const;

  void LogResponseHeaders();

  // Attempts to set proxy information for the request.
  HRESULT SetProxyInformation();

  struct TransientRequestState;
  void CloseHandles();

  static uint32 ChooseProxyAuthScheme(uint32 supported_schemes);

  // Returns true if the request is a POST request, in other words, if there
  // is a request buffer to be sent to the server.
  bool IsPostRequest() const { return request_buffer_ != NULL; }

  // When in pause state, caller will be blocked until Resume() is called.
  // Returns immediately otherwise.
  void WaitForResumeEvent();

  DownloadMetrics MakeDownloadMetrics(HRESULT hr) const;

  // Holds the transient state corresponding to a single http request. We
  // prefer to isolate the state of a request to avoid dirty state.
  struct TransientRequestState {
    TransientRequestState();
    ~TransientRequestState();

    CString scheme;
    CString server;
    int     port;
    CString url_path;
    bool    is_https;

    std::vector<uint8> response;
    int http_status_code;
    uint32 proxy_authentication_scheme;
    CString proxy;
    CString proxy_bypass;
    int content_length;
    int current_bytes;
    uint64 request_begin_ms;
    uint64 request_end_ms;
    std::unique_ptr<DownloadMetrics> download_metrics;
  };

  LLock lock_;
  volatile bool is_canceled_;
  volatile bool is_closed_;
  volatile bool pause_happened_;
  LLock ready_to_pause_lock_;
  HINTERNET session_handle_;  // Not owned by this class.
  CString url_;
  CString filename_;
  const void* request_buffer_;          // Contains the request body for POST.
  size_t      request_buffer_length_;   // Length of the request body.
  CString additional_headers_;
  CString user_agent_;
  ProxyAuthConfig proxy_auth_config_;
  ProxyConfig proxy_config_;
  bool low_priority_;
  NetworkRequestCallback* callback_;
  std::unique_ptr<WinHttpAdapter> winhttp_adapter_;
  std::unique_ptr<TransientRequestState> request_state_;
  scoped_event event_resume_;
  bool download_completed_;
  int resend_count_;

  DISALLOW_COPY_AND_ASSIGN(SimpleRequest);
};

}   // namespace omaha

#endif  // OMAHA_NET_SIMPLE_REQUEST_H_
