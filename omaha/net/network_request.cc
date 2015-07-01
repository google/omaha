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

#include "omaha/net/network_request.h"
#include "omaha/net/network_request_impl.h"

namespace omaha {

NetworkRequest::NetworkRequest(const NetworkConfig::Session& network_session) {
  impl_.reset(new internal::NetworkRequestImpl(network_session));
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

HRESULT NetworkRequest::Resume() {
  return impl_->Resume();
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

void NetworkRequest::set_proxy_auth_config(const ProxyAuthConfig& config) {
  return impl_->set_proxy_auth_config(config);
}

void NetworkRequest::set_num_retries(int num_retries) {
  return impl_->set_num_retries(num_retries);
}

void NetworkRequest::set_time_between_retries(int time_between_retries_ms) {
  return impl_->set_time_between_retries(time_between_retries_ms);
}

void NetworkRequest::set_retry_delay_jitter(int jitter_ms) {
  return impl_->set_retry_delay_jitter(jitter_ms);
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

std::vector<DownloadMetrics> NetworkRequest::download_metrics() const {
  return impl_->download_metrics();
}

HRESULT NetworkRequest::QueryHeadersString(uint32 info_level,
                                           const TCHAR* name,
                                           CString* value) {
  return impl_->QueryHeadersString(info_level, name, value);
}

void NetworkRequest::set_low_priority(bool low_priority) {
  return impl_->set_low_priority(low_priority);
}

void NetworkRequest::set_proxy_configuration(
    const ProxyConfig* proxy_configuration) {
  return impl_->set_proxy_configuration(proxy_configuration);
}

}  // namespace omaha
