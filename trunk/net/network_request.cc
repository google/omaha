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

#include "omaha/net/network_request.h"
#include "omaha/net/network_request_impl.h"

namespace omaha {

NetworkRequest::NetworkRequest(const NetworkConfig::Session& network_session) {
  impl_.reset(new detail::NetworkRequestImpl(network_session));
}

NetworkRequest::~NetworkRequest() {
}

HRESULT NetworkRequest::Close() {
  return impl_->Close();
}

void NetworkRequest::AddHttpRequest(HttpRequestInterface* http_request) {
  return impl_->AddHttpRequest(http_request);
}

HRESULT NetworkRequest::Post(const CString& url,
                             const void* buffer,
                             size_t length,
                             std::vector<uint8>* response) {
  return impl_->Post(url, buffer, length, response);
}

HRESULT NetworkRequest::Get(const CString& url, std::vector<uint8>* response) {
  return impl_->Get(url, response);
}

HRESULT NetworkRequest::DownloadFile(const CString& url,
                                     const CString& filename) {
  return impl_->DownloadFile(url, filename);
}

HRESULT NetworkRequest::Pause() {
  return impl_->Pause();
}

HRESULT NetworkRequest::Cancel() {
  return impl_->Cancel();
}

void NetworkRequest::AddHeader(const TCHAR* name, const TCHAR* value) {
  return impl_->AddHeader(name, value);
}

int NetworkRequest::http_status_code() const {
  return impl_->http_status_code();
}

void NetworkRequest::set_num_retries(int num_retries) {
  return impl_->set_num_retries(num_retries);
}

void NetworkRequest::set_time_between_retries(int time_between_retries_ms) {
  return impl_->set_time_between_retries(time_between_retries_ms);
}

void NetworkRequest::set_callback(NetworkRequestCallback* callback) {
  return impl_->set_callback(callback);
}

CString NetworkRequest::response_headers() const {
  return impl_->response_headers();
}

CString NetworkRequest::trace() const {
  return impl_->trace();
}

HRESULT NetworkRequest::QueryHeadersString(uint32 info_level,
                                           const TCHAR* name,
                                           CString* value) {
  return impl_->QueryHeadersString(info_level, name, value);
}

void NetworkRequest::set_low_priority(bool low_priority) {
  return impl_->set_low_priority(low_priority);
}

void NetworkRequest::set_network_configuration(
    const Config* network_configuration) {
  return impl_->set_network_configuration(network_configuration);
}

HRESULT PostRequest(NetworkRequest* network_request,
                    bool fallback_to_https,
                    const CString& url,
                    const CString& request_string,
                    CString* response) {
  return detail::PostRequest(network_request,
                             fallback_to_https,
                             url,
                             request_string,
                             response);
}

HRESULT GetRequest(NetworkRequest* network_request,
                   const CString& url,
                   CString* response) {
  return detail::GetRequest(network_request, url, response);
}

}  // namespace omaha

