// Copyright 2009-2010 Google Inc.
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

#ifndef OMAHA_COMMON_WEB_SERVICES_CLIENT_H_
#define OMAHA_COMMON_WEB_SERVICES_CLIENT_H_

#include <windows.h>
#include <atlstr.h>
#include <utility>
#include <vector>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "omaha/net/proxy_auth.h"

namespace omaha {

namespace xml {

class UpdateRequest;
class UpdateResponse;

}  // namespace xml

struct Lockable;
class NetworkRequest;

typedef std::vector<std::pair<CString, CString> > HeadersVector;

class WebServicesClientInterface {
 public:
  virtual ~WebServicesClientInterface() {}

  virtual HRESULT Send(const xml::UpdateRequest* update_request,
                       xml::UpdateResponse* update_response) = 0;

  virtual HRESULT SendString(const CString* request_buffer,
                             xml::UpdateResponse* response_buffer) = 0;

  virtual void Cancel() = 0;

  virtual void set_proxy_auth_config(const ProxyAuthConfig& config) = 0;

  // Returns true if the http transaction completed with 200 OK.
  virtual bool is_http_success() const = 0;

  // Returns the last http request status code.
  virtual int http_status_code() const = 0;

  // Returns http request trace (for logging).
  virtual CString http_trace() const = 0;
};

// Defines a class to send and receive protocol requests.
class WebServicesClient : public WebServicesClientInterface {
 public:
  explicit WebServicesClient(bool is_machine);
  virtual ~WebServicesClient();

  HRESULT  Initialize(const CString& url,
                      const HeadersVector& headers,
                      bool use_cup);

  virtual HRESULT Send(const xml::UpdateRequest* update_request,
                       xml::UpdateResponse* update_response);

  virtual HRESULT SendString(const CString* request_buffer,
                             xml::UpdateResponse* update_response);

  virtual void Cancel();

  virtual void set_proxy_auth_config(const ProxyAuthConfig& proxy_auth_config);

  virtual bool is_http_success() const;

  virtual int http_status_code() const;

  virtual CString http_trace() const;

 private:
  HRESULT SendStringPreserveProtocol(bool need_preserve_https,
                                     const CString* request_buffer,
                                     xml::UpdateResponse* update_response);

  const Lockable& lock() const;

  CString url() const;

  NetworkRequest* network_request();

  mutable Lockable* volatile lock_;   // Owned by this instance.

  const bool is_machine_;
  CString url_;

  scoped_ptr<NetworkRequest> network_request_;

  friend class WebServicesClientTest;
  DISALLOW_EVIL_CONSTRUCTORS(WebServicesClient);
};

}  // namespace omaha

#endif  // OMAHA_COMMON_WEB_SERVICES_CLIENT_H_
