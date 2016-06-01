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

#include "omaha/common/web_services_client.h"
#include <atlstr.h>
#include "omaha/base/const_addresses.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/synchronized.h"
#include "omaha/base/utils.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/update_request.h"
#include "omaha/common/update_response.h"
#include "omaha/net/cup_ecdsa_request.h"
#include "omaha/net/net_utils.h"
#include "omaha/net/network_config.h"
#include "omaha/net/network_request.h"
#include "omaha/net/simple_request.h"

namespace omaha {

WebServicesClient::WebServicesClient(bool is_machine)
    : lock_(NULL),
      is_machine_(is_machine),
      used_ssl_(false),
      ssl_result_(S_FALSE),
      use_cup_(false),
      http_xdaystart_header_value_(-1),
      http_xdaynum_header_value_(-1),
      retry_after_sec_(-1) {
}

WebServicesClient::~WebServicesClient() {
  CORE_LOG(L3, (_T("[WebServicesClient::~WebServicesClient]")));

  delete lock_;
  omaha::interlocked_exchange_pointer(&lock_, static_cast<Lockable*>(NULL));
}

HRESULT WebServicesClient::Initialize(const CString& url,
                                      const HeadersVector& headers,
                                      bool use_cup) {
  CORE_LOG(L3, (_T("[WebServicesClient::Initialize][%s][%d]"), url, use_cup));

  omaha::interlocked_exchange_pointer(&lock_,
                                      static_cast<Lockable*>(new LLock));
  __mutexScope(lock_);

  original_url_ = url;
  headers_ = headers;
  use_cup_ = use_cup;

  return S_OK;
}

HRESULT WebServicesClient::CreateRequest() {
  __mutexScope(lock_);

  network_request_.reset();

  NetworkConfig* network_config = NULL;
  NetworkConfigManager& network_manager = NetworkConfigManager::Instance();
  HRESULT hr = network_manager.GetUserNetworkConfig(&network_config);
  if (FAILED(hr)) {
    return hr;
  }

  network_request_.reset(new NetworkRequest(network_config->session()));

  for (size_t i = 0; i < headers_.size(); ++i) {
    network_request_->AddHeader(headers_[i].first, headers_[i].second);
  }

  if (use_cup_) {
    network_request_->AddHttpRequest(new CupEcdsaRequest(new SimpleRequest));
  } else {
    network_request_->AddHttpRequest(new SimpleRequest);
  }

  network_request_->set_num_retries(1);
  network_request_->set_proxy_auth_config(proxy_auth_config_);

  return S_OK;
}

HRESULT WebServicesClient::Send(const xml::UpdateRequest* update_request,
                                xml::UpdateResponse* update_response) {
  CORE_LOG(L3, (_T("[WebServicesClient::Send]")));
  ASSERT1(update_request);
  ASSERT1(update_response);

  if (!ConfigManager::Instance()->CanUseNetwork(is_machine_)) {
    CORE_LOG(LE, (_T("[WebServicesClient::Send][network use prohibited]")));
    return GOOPDATE_E_CANNOT_USE_NETWORK;
  }

  CString request_string;
  HRESULT hr = update_request->Serialize(&request_string);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[Serialize failed][0x%x]"), hr));
    return hr;
  }

  ASSERT1(!request_string.IsEmpty());

  // Use encrypted transport when the request includes a tt_token.
  const bool use_encryption = update_request->has_tt_token();

  return SendStringWithFallback(use_encryption,
                                &request_string,
                                update_response);
}

HRESULT WebServicesClient::SendString(const CString* request_string,
                                      xml::UpdateResponse* update_response) {
  CORE_LOG(L3, (_T("[WebServicesClient::SendString]")));
  ASSERT1(request_string);
  ASSERT1(update_response);

  return SendStringWithFallback(false, request_string, update_response);
}

HRESULT WebServicesClient::SendStringWithFallback(
    bool use_encryption,
    const CString* request_string,
    xml::UpdateResponse* update_response) {
  CORE_LOG(L3, (_T("[WebServicesClient::SendStringWithFallback]")));

  ASSERT1(request_string);
  ASSERT1(update_response);

  const CStringA utf8_request_string(WideToUtf8(*request_string));
  CORE_LOG(L3, (_T("[sending web services request as UTF-8][%S]"),
      utf8_request_string));

  HRESULT hr = SendStringInternal(original_url_,
                                  utf8_request_string,
                                  update_response);
  if (IsHttpsUrl(original_url_)) {
    used_ssl_ = true;
    ssl_result_ = hr;
  }

  CORE_LOG(L3, (_T("[first request returned 0x%x]"), hr));

  if (SUCCEEDED(hr)) {
    return hr;
  }

  if (retry_after_sec_ > 0) {
    CORE_LOG(L3, (_T("[retry after was received, don't fallback to http]")));
    return hr;
  }
  if (hr == GOOPDATE_E_CANCELLED) {
    CORE_LOG(L3, (_T("[the request was canceled, don't fallback to http]")));
    return hr;
  }
  if (IsHttpUrl(original_url_)) {
    CORE_LOG(L3, (_T("[http request already failed, don't fallback to http]")));
    return hr;
  }
  if (use_encryption) {
    CORE_LOG(L3, (_T("[encryption required, don't fallback to http]")));
    return hr;
  }

  CORE_LOG(L3, (_T("[fallback to the http url]")));
  HRESULT hr_fallback = SendStringInternal(MakeHttpUrl(original_url_),
                                           utf8_request_string,
                                           update_response);
  if (SUCCEEDED(hr_fallback)) {
    return S_OK;
  }

  // Return the error of the first request when the fallback has failed too.
  CORE_LOG(L3, (_T("[fallback to the http url returned 0x%x]"), hr_fallback));
  return hr;
}

HRESULT WebServicesClient::SendStringInternal(
    const CString& actual_url,
    const CStringA& utf8_request_string,
    xml::UpdateResponse* update_response) {
  CORE_LOG(L3, (_T("[actual_url is %s]"), actual_url));

  // Each attempt to send a request is using its own network client.
  HRESULT hr = CreateRequest();
  if (FAILED(hr)) {
    return hr;
  }

  std::vector<uint8> response_buffer;
  hr = network_request_->PostUtf8String(actual_url,
                                        utf8_request_string,
                                        &response_buffer);
  CORE_LOG(L3, (_T("[the request returned 0x%x]"), hr));
  const CString response_string(Utf8BufferToWideChar(response_buffer));
  CORE_LOG(L3, (_T("[response received][%s]"), response_string));

  // Save the values of the custom headers if the values are found.
  CaptureCustomHeaderValues();

  // The value of the X-Retry-After header is only trusted when the response is
  // over https.
  if (IsHttpsUrl(actual_url)) {
    retry_after_sec_ =
        std::min(FindHttpHeaderValueInt(kHeaderXRetryAfter), kSecondsPerDay);
    CORE_LOG(L3, (_T("[retry_after_sec_][%d]"), retry_after_sec_));
  }

  if (FAILED(hr)) {
    CORE_LOG(L3, (_T("[PostUtf8String failed][0x%x]"), hr));
    return hr;
  }

  // The web services server is expected to reply with 200 OK if the
  // transaction has been successful.
  ASSERT1(is_http_success());
  hr = update_response->Deserialize(response_buffer);
  if (FAILED(hr)) {
    CORE_LOG(L3, (_T("[Deserialize failed][0x%x]"), hr));
    // If we received a 200 response that doesn't successfully parse, one
    // possibility is that we were redirected or DNS-poisoned by a captive
    // portal, and the body is actually an HTML login/eula page. Check the
    // response body; if it looks like HTML, return OMAHA_NET_E_CAPTIVEPORTAL.
    // Otherwise, return the actual error from the XML parser, and assume that
    // we've been corrupted in-flight.
    //
    // If CUP is used, this case will be detected at the network layer, and the
    // call to PostUtf8String call will return OMAHA_NET_E_CAPTIVEPORTAL.
    if (NULL == stristrW(response_string, L"<response") &&
        NULL != stristrW(response_string, L"<html")) {
      CORE_LOG(LE, (_T("[HTML body detected - possibly a captive portal]")));
      hr = OMAHA_NET_E_CAPTIVEPORTAL;
    }

    return hr;
  }

  return S_OK;
}

void WebServicesClient::CaptureCustomHeaderValues() {
  const int day_start = FindHttpHeaderValueInt(kHeaderXDaystart);
  if (day_start != -1) {
    http_xdaystart_header_value_ = day_start;
  }
  const int day_num = FindHttpHeaderValueInt(kHeaderXDaynum);
  if (day_num != -1) {
    http_xdaynum_header_value_ = day_num;
  }
}

void WebServicesClient::Cancel() {
  CORE_LOG(L3, (_T("[WebServicesClient::Cancel]")));
  if (network_request_.get()) {
    network_request_->Cancel();
  }
}

void WebServicesClient::set_proxy_auth_config(const ProxyAuthConfig& config) {
  __mutexScope(lock_);
  proxy_auth_config_ = config;
}

bool WebServicesClient::is_http_success() const {
  return network_request_.get() &&
         network_request_->http_status_code() == HTTP_STATUS_OK;
}

int WebServicesClient::http_status_code() const {
  return network_request_.get() ? network_request_->http_status_code() : 0;
}

CString WebServicesClient::http_trace() const {
  return network_request_.get() ? network_request_->trace() : CString();
}

bool WebServicesClient::http_used_ssl() const {
  __mutexScope(lock_);
  return used_ssl_;
}

HRESULT WebServicesClient::http_ssl_result() const {
  __mutexScope(lock_);
  return ssl_result_;
}

int WebServicesClient::FindHttpHeaderValueInt(const CString& header) const {
  if (!network_request_.get()) {
    return -1;
  }

  CString all_headers = network_request_->response_headers();
  if (!all_headers.IsEmpty()) {
    CString value = FindHttpHeaderValue(all_headers, header);
    if (!value.IsEmpty()) {
      return String_StringToInt(value);
    }
  }

  return -1;
}

int WebServicesClient::http_xdaystart_header_value() const {
  __mutexScope(lock_);
  return http_xdaystart_header_value_;
}

int WebServicesClient::http_xdaynum_header_value() const {
  __mutexScope(lock_);
  return http_xdaynum_header_value_;
}

int WebServicesClient::retry_after_sec() const {
  __mutexScope(lock_);
  return retry_after_sec_;
}

// static
CString WebServicesClient::FindHttpHeaderValue(const CString& all_headers,
                                               const CString& search_name) {
  ASSERT1(!search_name.IsEmpty());

  typedef std::vector<CString>::const_iterator CStrVecIter;

  std::vector<CString> headers;
  TextToLines(all_headers, _T("\r\n"), &headers);
  for (CStrVecIter it = headers.begin(); it != headers.end(); ++it) {
    UTIL_LOG(L6, (_T("[WebServicesClient::FindHttpHeaderValue][%s]"), *it));
    CString name, value;
    if (ParseNameValuePair(*it, _T(':'), &name, &value)) {
      if (name.Trim().CompareNoCase(search_name) == 0) {
        UTIL_LOG(L6, (_T("[WebServicesClient::FindHttpHeaderValue][found]")));
        return value.Trim();
      }
    }
  }

  return _T("");
}

}  // namespace omaha
