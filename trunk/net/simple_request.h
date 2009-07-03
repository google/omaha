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
// SimpleRequest provides for http transactions using WinHttp/WinInet.
//
// TODO(omaha): the class interface is not stable yet, as a few more
// getters and setters are still needed.
//
// TODO(omaha): receiving a response into a file is not implemented yet.

#ifndef OMAHA_NET_SIMPLE_REQUEST_H__
#define OMAHA_NET_SIMPLE_REQUEST_H__

#include <atlstr.h>
#include <vector>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "omaha/common/debug.h"
#include "omaha/common/synchronized.h"
#include "omaha/net/http_client.h"
#include "omaha/net/http_request.h"
#include "omaha/net/network_config.h"

namespace omaha {

class SimpleRequest : public HttpRequestInterface {
 public:
  SimpleRequest();
  virtual ~SimpleRequest();

  virtual HRESULT Close();

  virtual HRESULT Send();

  virtual HRESULT Cancel();

  virtual std::vector<uint8> GetResponse() const;

  virtual int GetHttpStatusCode() const {
    return request_state_.get() ? request_state_->http_status_code : 0;
  }

  virtual HRESULT QueryHeadersString(uint32 info_level,
                                     const TCHAR* name,
                                     CString* value) const;

  virtual CString GetResponseHeaders() const;

  virtual CString ToString() const { return _T("WinHTTP"); }

  virtual void set_session_handle(HINTERNET session_handle) {
    session_handle_ = session_handle;
  }

  virtual void set_url(const CString& url) { url_ = url; }

  virtual void set_request_buffer(const void* buffer, size_t buffer_length) {
    request_buffer_ = buffer;
    request_buffer_length_ = buffer_length;
  }

  virtual void set_network_configuration(const Config& network_config) {
    network_config_ = network_config;
  }

  // Sets the filename to receive the response instead of the memory buffer.
  virtual void set_filename(const CString& filename) { filename_ = filename; }

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

 private:
  HRESULT DoSend();

  void LogResponseHeaders();

  // Sets proxy information for the request.
  void SetProxyInformation();

  struct TransientRequestState;
  void CloseHandles(TransientRequestState* transient_request_state);

  static uint32 ChooseProxyAuthScheme(uint32 supported_schemes);

  static void __stdcall StatusCallback(HINTERNET handle,
                                       uint32 context,
                                       uint32 status,
                                       void* info,
                                       uint32 info_len);

  // Returns true if the request is a POST request, in other words, if there
  // is a request buffer to be sent to the server.
  bool IsPostRequest() const { return request_buffer_ != NULL; }

  // Holds the transient state corresponding to a single http request. We
  // prefer to isolate the state of a request to avoid dirty state.
  struct TransientRequestState {
    TransientRequestState()
        : port(0),
          http_status_code(0),
          proxy_authentication_scheme(0),
          content_length(0),
          current_bytes(0),
          connection_handle(NULL),
          request_handle(NULL) {}
    ~TransientRequestState() {
      ASSERT1(connection_handle == NULL);
      ASSERT1(request_handle == NULL);
    }

    CString scheme;
    CString server;
    int     port;
    CString url_path;

    std::vector<uint8> response;
    int http_status_code;
    uint32 proxy_authentication_scheme;
    CString proxy;
    CString proxy_bypass;
    int content_length;
    int current_bytes;

    HINTERNET connection_handle;
    HINTERNET request_handle;
  };

  LLock lock_;
  volatile bool is_canceled_;
  HINTERNET session_handle_;  // Not owned by this class.
  CString url_;
  CString filename_;
  const void* request_buffer_;          // Contains the request body for POST.
  size_t      request_buffer_length_;   // Length of the request body.
  CString additional_headers_;
  CString user_agent_;
  Config network_config_;
  bool low_priority_;
  NetworkRequestCallback* callback_;
  scoped_ptr<HttpClient> http_client_;
  scoped_ptr<TransientRequestState> request_state_;

  DISALLOW_EVIL_CONSTRUCTORS(SimpleRequest);
};

}   // namespace omaha

#endif  // OMAHA_NET_SIMPLE_REQUEST_H__

