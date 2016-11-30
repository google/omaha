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
class CupRequest;

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

  // Returns true if HTTPS was used for this transaction.
  virtual bool http_used_ssl() const = 0;

  // Returns the result of the HTTPS transaction.  (This will be S_FALSE if
  // HTTPS wasn't used.)
  virtual HRESULT http_ssl_result() const = 0;

  // Returns the last valid value of the X-Daystart header or -1 if the
  // header was not found in either request.
  // If present, this value allows resetting the values of ActivePingDayStartSec
  // and RollCallDayStartSec even if the server response could not be parsed
  // for any reason.
  virtual int http_xdaystart_header_value() const = 0;

  // Same as above but for the value of the X-Daynum header and
  // DayOfLastActivity/DayOfLastRollCall/DayOfInstall respectively.
  virtual int http_xdaynum_header_value() const = 0;

  // Returns the last valid value of the optional X-Retry-After header or -1 if
  // the header was not present in the request. Only HTTPS X-Retry-After header
  // header values are respected. Also, the header value is clamped to 24 hours.
  // The server uses the optional X-Retry-After header to indicate that the
  // current request should not be attempted again. Any response received along
  // with the X-Retry-After header should be interpreted as it would have been
  // without the X-Retry-After header. The value of the header is the number of
  // seconds to wait before trying to connect to the server again.
  virtual int retry_after_sec() const = 0;
};

// Defines a class to send and receive protocol requests, with a fall back
// from HTTPS to HTTP.
class WebServicesClient : public WebServicesClientInterface {
 public:
  explicit WebServicesClient(bool is_machine);
  virtual ~WebServicesClient();

  HRESULT  Initialize(const CString& url,
                      const HeadersVector& headers,
                      bool use_cup);

  virtual HRESULT Send(const xml::UpdateRequest* update_request,
                       xml::UpdateResponse* update_response);

  virtual HRESULT SendString(const CString* request_string,
                             xml::UpdateResponse* update_response);

  virtual void Cancel();

  virtual void set_proxy_auth_config(const ProxyAuthConfig& proxy_auth_config);

  virtual bool is_http_success() const;

  virtual int http_status_code() const;

  virtual CString http_trace() const;

  virtual bool http_used_ssl() const;

  virtual HRESULT http_ssl_result() const;

  virtual int http_xdaystart_header_value() const;

  virtual int http_xdaynum_header_value() const;

  virtual int retry_after_sec() const;

 private:
  HRESULT CreateRequest();

  // Sends a string and possibly retries the request  by falling back on http
  // if the request has failed the first time. No fall backs happens if the
  // initial url is http or if encryption is required.
  // Returns S_OK if the request is successfully sent, otherwise it returns the
  // error corresponding to the first request sent.
  HRESULT SendStringWithFallback(bool use_encryption,
                                 const CString* request_string,
                                 xml::UpdateResponse* update_response);

  // Sends a string representing a protocol message and returns a parsed
  // response. The |update_response| parameter is only modified if the
  // parsing has succeeded.
  HRESULT SendStringInternal(const CString& url,
                             const CStringA& utf8_request_string,
                             xml::UpdateResponse* update_response);

  // Captures the values of kHeaderXDaystart and kHeaderXDaynum if the fields
  // are found in the response headers.
  void CaptureCustomHeaderValues();

  // Finds the |search_name| header in the response headers (case-insensitive).
  static CString FindHttpHeaderValue(const CString& all_headers,
                                     const CString& search_name);

  int FindHttpHeaderValueInt(const CString& header_name) const;

  Lockable* volatile lock_;   // Owned by this instance.

  const bool is_machine_;

  // The url of the update server. This is usually an HTTPS url but using
  // the url override in the UpdateDev, an HTTP url can be specified for
  // testing purposes. Since the class allow falling back on an HTTP url in
  // certain cases, the actual request can go to a different url than what
  // this member contains.
  CString original_url_;

  // True if an HTTPS request has been made.
  bool used_ssl_;

  // Contains the error code of the HTTPS request, if such a request was made,
  // or S_FALSE otherwise.
  HRESULT ssl_result_;

  // If true, the request will use CUP. In general, this is the case for
  // update checks. Pings don't use CUP.
  bool use_cup_;

  // Contains the request headers to send.
  HeadersVector headers_;
  HeadersVector update_request_headers_;

  // Even if the response can't be parsed for any reason, store the values
  // for these headers anyway, if the values are present. Since it is possible
  // for two requests to be made due to fall back, the last valid values win.
  // The values are -1 when the headers are not found.
  int http_xdaystart_header_value_;
  int http_xdaynum_header_value_;

  // Stores the last valid value of the optional X-Retry-After header or -1 if
  // the header was not present in the request. Only HTTPS X-Retry-After header
  // header values are respected. Also, the header value is clamped to 24 hours.
  int retry_after_sec_;

  // Set by the client of this class, may be used by the network request if
  // proxy authentication is required later on.
  ProxyAuthConfig proxy_auth_config_;

  // Each web services request must use its own network request instance.
  scoped_ptr<NetworkRequest> network_request_;

  friend class WebServicesClientTest;
  DISALLOW_COPY_AND_ASSIGN(WebServicesClient);
};

}  // namespace omaha

#endif  // OMAHA_COMMON_WEB_SERVICES_CLIENT_H_
