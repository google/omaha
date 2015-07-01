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
#include "omaha/net/cup_request.h"
#include "omaha/net/network_config.h"
#include "omaha/net/network_request.h"
#include "omaha/net/simple_request.h"

namespace omaha {

WebServicesClient::WebServicesClient(bool is_machine)
    : lock_(NULL),
      is_machine_(is_machine) {
}

WebServicesClient::~WebServicesClient() {
  CORE_LOG(L3, (_T("[WebServicesClient::~WebServicesClient]")));

  delete &lock();
  omaha::interlocked_exchange_pointer(&lock_, static_cast<Lockable*>(NULL));
}

HRESULT  WebServicesClient::Initialize(const CString& url,
                                       const HeadersVector& headers,
                                       bool use_cup) {
  CORE_LOG(L3, (_T("[WebServicesClient::Initialize][%s][%d]"), url, use_cup));

  omaha::interlocked_exchange_pointer(&lock_,
                                      static_cast<Lockable*>(new LLock));
  __mutexScope(lock());

  url_ = url;

  NetworkConfig* network_config = NULL;
  NetworkConfigManager& network_manager = NetworkConfigManager::Instance();
  HRESULT hr = network_manager.GetUserNetworkConfig(&network_config);
  if (FAILED(hr)) {
    return hr;
  }

  const NetworkConfig::Session& session(network_config->session());

  network_request_.reset(new NetworkRequest(session));

  for (size_t i = 0; i < headers.size(); ++i) {
    network_request_->AddHeader(headers[i].first, headers[i].second);
  }

  if (use_cup) {
    network_request_->AddHttpRequest(new CupRequest(new SimpleRequest));
  }
  network_request_->AddHttpRequest(new SimpleRequest);
  network_request_->set_num_retries(1);

  return S_OK;
}

const Lockable& WebServicesClient::lock() const {
  return *omaha::interlocked_exchange_pointer(&lock_, lock_);
}

CString WebServicesClient::url() const {
  __mutexScope(lock());
  return url_;
}

NetworkRequest* WebServicesClient::network_request() {
  __mutexScope(lock());
  return network_request_.get();
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
    CORE_LOG(LE, (_T("[Serialize failed][0x%08x]"), hr));
    return hr;
  }

  ASSERT1(!request_string.IsEmpty());

  // For security reasons, if there's tt_token in the request, we must
  // set_preserve_protocol in network request to prevent it from replacing
  // https with http scheme.
  const bool need_preserve_https = update_request->has_tt_token();

  return SendStringPreserveProtocol(need_preserve_https, &request_string,
                                    update_response);
}

HRESULT WebServicesClient::SendString(const CString* request_string,
                                      xml::UpdateResponse* update_response) {
  CORE_LOG(L3, (_T("[WebServicesClient::SendString]")));
  ASSERT1(request_string);
  ASSERT1(update_response);

  return SendStringPreserveProtocol(false, request_string, update_response);
}

HRESULT WebServicesClient::SendStringPreserveProtocol(
    bool need_preserve_https,
    const CString* request_string,
    xml::UpdateResponse* update_response) {
  CORE_LOG(L3, (_T("[WebServicesClient::SendStringPreserveProtocol]")));
  ASSERT1(request_string);
  ASSERT1(update_response);

  CORE_LOG(L3, (_T("[sending web services request][%s]"), *request_string));

  std::vector<uint8> response_buffer;

  CString request_url = url();
  ASSERT1(!need_preserve_https ||
      String_StartsWith(request_url, kHttpsProtoScheme, true));
  network_request_->set_preserve_protocol(need_preserve_https);
  HRESULT hr = PostRequest(network_request_.get(), true,
                           request_url, *request_string, &response_buffer);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[PostString failed][0x%08x]"), hr));
    return hr;
  }

  // The web services server is expected to reply with 200 OK if the
  // transaction has been successful.
  ASSERT1(is_http_success());
  if (!is_http_success()) {
    CORE_LOG(LE, (_T("[PostString returned success on a failed transaction]")));
    return E_FAIL;
  }

  CString response_string = Utf8BufferToWideChar(response_buffer);
  CORE_LOG(L3, (_T("[received web services response][%s]"), response_string));

  hr = update_response->Deserialize(response_buffer);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[UpdateResponse::Deserialize failed][0x%08x]"), hr));
    return hr;
  }

  return S_OK;
}

void WebServicesClient::Cancel() {
  CORE_LOG(L3, (_T("[WebServicesClient::Cancel]")));
  NetworkRequest* network_request(network_request());
  if (network_request) {
    network_request->Cancel();
  }
}

void WebServicesClient::set_proxy_auth_config(const ProxyAuthConfig& config) {
  ASSERT1(network_request());
  network_request()->set_proxy_auth_config(config);
}

bool WebServicesClient::is_http_success() const {
  return network_request_->http_status_code() == HTTP_STATUS_OK;
}

int WebServicesClient::http_status_code() const {
  return network_request_->http_status_code();
}

CString WebServicesClient::http_trace() const {
  return network_request_->trace();
}

}  // namespace omaha
