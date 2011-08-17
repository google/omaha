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

#include "omaha/net/network_request_impl.h"
#include <limits.h>
#include <atlsecurity.h>
#include <algorithm>
#include <cctype>
#include <functional>
#include <vector>
#include "base/basictypes.h"
#include "omaha/base/const_addresses.h"
#include "omaha/base/constants.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/scoped_any.h"
#include "omaha/base/string.h"
#include "omaha/base/time.h"
#include "omaha/net/http_client.h"
#include "omaha/net/net_utils.h"
#include "omaha/net/network_config.h"

namespace omaha {

namespace detail {

// Returns the user sid corresponding to the token. This function is only used
// for logging purposes.
CString GetTokenUser(HANDLE token) {
  CAccessToken access_token;
  access_token.Attach(token);
  CSid sid;
  access_token.GetUser(&sid);
  access_token.Detach();
  return sid.Sid();
}

// Logs bytes from the beginning of the file. Non-printable bytes are
// replaced with '.'.
void LogFileBytes(const CString& filename, size_t num_bytes) {
  scoped_hfile file(::CreateFile(filename,
                                 GENERIC_READ,
                                 FILE_SHARE_READ,
                                 NULL,
                                 OPEN_EXISTING,
                                 FILE_ATTRIBUTE_NORMAL,
                                 NULL));
  std::vector<char> bytes(num_bytes);
  DWORD bytes_read(0);
  if (file) {
    ::ReadFile(get(file), &bytes.front(), bytes.size(), &bytes_read, NULL);
  }
  bytes.resize(bytes_read);
  replace_if(bytes.begin(), bytes.end(), std::not1(std::ptr_fun(isprint)), '.');
  bytes.push_back('\0');
  NET_LOG(L3, (_T("[file bytes: %hs]"), &bytes.front()));
}

NetworkRequestImpl::NetworkRequestImpl(
    const NetworkConfig::Session& network_session)
      : cur_http_request_(NULL),
        cur_proxy_config_(NULL),
        cur_retry_count_(0),
        last_hr_(S_OK),
        last_http_status_code_(0),
        http_status_code_(0),
        proxy_auth_config_(NULL, CString()),
        num_retries_(0),
        low_priority_(false),
        time_between_retries_ms_(kDefaultTimeBetweenRetriesMs),
        callback_(NULL),
        request_buffer_(NULL),
        request_buffer_length_(0),
        response_(NULL),
        network_session_(network_session),
        is_canceled_(false),
        preserve_protocol_(false) {
  // NetworkConfig::Initialize must be called before using NetworkRequest.
  // If Winhttp cannot be loaded, this handle will be NULL.
  if (!network_session.session_handle) {
    NET_LOG(LW, (_T("[NetworkRequestImpl: session_handle is NULL.]")));
  }

  // Create a manual reset event to wait on during retry periods.
  // The event is signaled by NetworkRequestImpl::Cancel, to break out of
  // the retry loop and return control to the caller.
  reset(event_cancel_, ::CreateEvent(NULL, true, false, NULL));
  ASSERT1(event_cancel_);

  const CString mid(NetworkConfig::GetMID());
  if (!mid.IsEmpty()) {
    AddHeader(kHeaderXMID, mid);
  }
}

NetworkRequestImpl::~NetworkRequestImpl() {
  for (size_t i = 0; i != http_request_chain_.size(); ++i) {
    delete http_request_chain_[i];
  }
}

void NetworkRequestImpl::Reset() {
  if (response_) {
    response_->clear();
  }
  response_headers_.Empty();
  http_status_code_      = 0;
  cur_http_request_      = NULL;
  cur_proxy_config_      = NULL;
  cur_retry_count_       = 0;
  last_hr_               = S_OK;
  last_http_status_code_ = 0;
}

HRESULT NetworkRequestImpl::Close() {
  HRESULT hr = S_OK;
  for (size_t i = 0; i != http_request_chain_.size(); ++i) {
    hr = http_request_chain_[i]->Close();
  }
  return hr;
}

HRESULT NetworkRequestImpl::Cancel() {
  NET_LOG(L3, (_T("[NetworkRequestImpl::Cancel]")));
  HRESULT hr = S_OK;
  ::InterlockedExchange(&is_canceled_, true);
  if (event_cancel_) {
    hr = ::SetEvent(get(event_cancel_)) ? S_OK : HRESULTFromLastError();
  }
  for (size_t i = 0; i != http_request_chain_.size(); ++i) {
    hr = http_request_chain_[i]->Cancel();
  }
  return hr;
}

void NetworkRequestImpl::AddHttpRequest(HttpRequestInterface* http_request) {
  ASSERT1(http_request);
  http_request_chain_.push_back(http_request);
}

HRESULT NetworkRequestImpl::Post(const CString& url,
                                 const void* buffer,
                                 size_t length,
                                 std::vector<uint8>* response) {
  ASSERT1(response);
  url_ = url;
  request_buffer_ = buffer;
  request_buffer_length_ = length;
  response_ = response;
  return DoSendWithRetries();
}

HRESULT NetworkRequestImpl::Get(const CString& url,
                                std::vector<uint8>* response) {
  ASSERT1(response);
  url_ = url;
  request_buffer_ = NULL;
  request_buffer_length_ = 0;
  response_ = response;
  return DoSendWithRetries();
}

HRESULT NetworkRequestImpl::DownloadFile(const CString& url,
                                         const CString& filename) {
  url_ = url;
  filename_ = filename;
  request_buffer_ = NULL;
  request_buffer_length_ = 0;
  response_ = NULL;
  return DoSendWithRetries();
}

HRESULT NetworkRequestImpl::Pause() {
  NET_LOG(L3, (_T("[NetworkRequestImpl::Pause]")));
  HRESULT hr = S_OK;

  for (size_t i = 0; i != http_request_chain_.size(); ++i) {
    HRESULT hr2 = http_request_chain_[i]->Pause();

    // Only overwrite hr if it doesn't have useful error information.
    if (SUCCEEDED(hr)) {
      hr = hr2;
    }
  }
  return hr;
}

HRESULT NetworkRequestImpl::Resume() {
  NET_LOG(L3, (_T("[NetworkRequestImpl::Pause]")));
  HRESULT hr = S_OK;

  for (size_t i = 0; i != http_request_chain_.size(); ++i) {
    HRESULT hr2 = http_request_chain_[i]->Resume();

    // Only overwrite hr if it doesn't have useful error information.
    if (SUCCEEDED(hr)) {
      hr = hr2;
    }
  }
  return hr;
}

HRESULT NetworkRequestImpl::DoSendWithRetries() {
  ASSERT1(num_retries_ >= 0);
  ASSERT1(response_ || !filename_.IsEmpty());

  Reset();

  int http_status_code(0);
  CString response_headers;
  std::vector<uint8> response;

  SafeCStringAppendFormat(&trace_, _T("Url=%s\r\n"), url_);

  HRESULT hr = S_OK;
  int wait_interval_ms = time_between_retries_ms_;
  for (cur_retry_count_ = 0;
       cur_retry_count_ < 1 + num_retries_;
       ++cur_retry_count_) {
    if (IsHandleSignaled(get(event_cancel_))) {
      ASSERT1(is_canceled_);

      // There is no state to be set when the request is canceled.
      return GOOPDATE_E_CANCELLED;
    }

    // Wait before retrying if there are retries to be done.
    if (cur_retry_count_ > 0) {
      if (callback_) {
        const time64 next_retry_time = GetCurrent100NSTime() +
                                       wait_interval_ms * kMillisecsTo100ns;
        callback_->OnRequestRetryScheduled(next_retry_time);
      }

      NET_LOG(L3, (_T("[wait %d ms]"), wait_interval_ms));
      VERIFY1(::WaitForSingleObject(get(event_cancel_),
                                    wait_interval_ms) != WAIT_FAILED);

      if (callback_) {
        callback_->OnRequestBegin();
      }

      // Compute the next wait interval and check for multiplication overflow.
      if (wait_interval_ms > INT_MAX / kTimeBetweenRetriesMultiplier) {
        ASSERT1(false);
        hr = HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
        break;
      }
      wait_interval_ms *= kTimeBetweenRetriesMultiplier;
    }

    DetectProxyConfiguration(&proxy_configurations_);
    ASSERT1(!proxy_configurations_.empty());
    OPT_LOG(L2, (_T("[detected configurations][\r\n%s]"),
                 NetworkConfig::ToString(proxy_configurations_)));

    hr = DoSend(&http_status_code, &response_headers, &response);
    HttpClient::StatusCodeClass status_code_class =
        HttpClient::GetStatusCodeClass(http_status_code);
    if (SUCCEEDED(hr) ||
        hr == GOOPDATE_E_CANCELLED ||
        status_code_class == HttpClient::STATUS_CODE_CLIENT_ERROR) {
      break;
    }
  }

  // Update the object state with the local values.
  http_status_code_ = http_status_code;
  response_headers_ = response_headers;
  if (response_) {
    response_->swap(response);
  }

  // Avoid returning generic errors from the network stack.
  ASSERT1(hr != E_FAIL);
  return hr;
}

HRESULT NetworkRequestImpl::DoSend(int* http_status_code,
                                   CString* response_headers,
                                   std::vector<uint8>* response) const {
  ASSERT1(http_status_code);
  ASSERT1(response_headers);
  ASSERT1(response);

  OPT_LOG(L3, (_T("[Send][url=%s][request=%s][filename=%s]"),
          url_,
          BufferToPrintableString(request_buffer_, request_buffer_length_),
          filename_));

  // Cache the values corresponding to the error encountered by the first
  // configuration, which is the preferred configuration.
  HRESULT error_hr = S_OK;
  int     error_http_status_code = 0;
  CString error_response_headers;
  std::vector<uint8> error_response;

  // Tries out all the available configurations until one of them succeeds.
  // TODO(omaha): remember the last good configuration and prefer that for
  // future requests.
  HRESULT hr = S_OK;
  ASSERT1(!proxy_configurations_.empty());
  for (size_t i = 0; i != proxy_configurations_.size(); ++i) {
    cur_proxy_config_ = &proxy_configurations_[i];
    hr = DoSendWithConfig(http_status_code, response_headers, response);
    if (i == 0 && FAILED(hr)) {
      error_hr = hr;
      error_http_status_code = *http_status_code;
      error_response_headers = *response_headers;
      error_response.swap(*response);
    }

    if (SUCCEEDED(hr) ||
        hr == GOOPDATE_E_CANCELLED ||
        hr == CI_E_BITS_DISABLED ||
        *http_status_code == HTTP_STATUS_NOT_FOUND) {
      break;
    }
  }

  // There are only four possible outcomes: success, cancel, BITS disabled
  // and an error other than these.
  HRESULT result = S_OK;
  if (SUCCEEDED(hr) ||
      hr == GOOPDATE_E_CANCELLED ||
      hr == CI_E_BITS_DISABLED) {
    result = hr;
  } else {
    // In case of errors, log the error and the response returned by the first
    // network configuration.
    result = error_hr;
    *http_status_code = error_http_status_code;
    *response_headers = error_response_headers;
    response->swap(error_response);
  }

  OPT_LOG(L3, (_T("[Send response received][result 0x%x][status code %d][%s]"),
      result, *http_status_code, VectorToPrintableString(*response)));

#ifdef DEBUG
  if (!filename_.IsEmpty()) {
    const size_t kNumBytes = 8196;    // 8 kb.
    LogFileBytes(filename_, kNumBytes);
  }
#endif

  return result;
}

HRESULT NetworkRequestImpl::DoSendWithConfig(
    int* http_status_code,
    CString* response_headers,
    std::vector<uint8>* response) const {
  ASSERT1(response_headers);
  ASSERT1(response);
  ASSERT1(http_status_code);

  bool    first_error_from_http_request_saved = false;
  HRESULT error_hr = S_OK;
  int     error_http_status_code = 0;
  CString error_response_headers;
  std::vector<uint8> error_response;

  ASSERT1(cur_proxy_config_);

  CString msg;
  SafeCStringFormat(&msg, _T("Trying config: %s"),
                    NetworkConfig::ToString(*cur_proxy_config_));
  NET_LOG(L3, (_T("[%s]"), msg));
  SafeCStringAppendFormat(&trace_, _T("%s.\r\n"), msg);

  HRESULT hr = S_OK;
  ASSERT1(!http_request_chain_.empty());
  for (size_t i = 0; i != http_request_chain_.size(); ++i) {
    cur_http_request_ = http_request_chain_[i];

    CString msg;
    SafeCStringFormat(&msg, _T("trying %s"), cur_http_request_->ToString());
    NET_LOG(L3, (_T("[%s]"), msg));
    SafeCStringAppendFormat(&trace_, _T("%s.\r\n"), msg);

    hr = DoSendHttpRequest(http_status_code, response_headers, response);

    SafeCStringFormat(&msg,
                      _T("Send request returned 0x%08x. Http status code %d"),
                      hr, *http_status_code);
    NET_LOG(L3, (_T("[%s]"), msg));
    SafeCStringAppendFormat(&trace_, _T("%s.\r\n"), msg);

    if (!first_error_from_http_request_saved && FAILED(hr) &&
        hr != CI_E_BITS_DISABLED) {
      error_hr = hr;
      error_http_status_code = cur_http_request_->GetHttpStatusCode();
      error_response_headers = cur_http_request_->GetResponseHeaders();
      cur_http_request_->GetResponse().swap(error_response);
      first_error_from_http_request_saved = true;
    }

    // The chain traversal stops when the request is successful or
    // it is canceled, or the status code is 404.
    // In the case of 404 response, all HttpRequests are likely to return
    // the same 404 response.
    if (SUCCEEDED(hr) ||
        hr == GOOPDATE_E_CANCELLED ||
        *http_status_code == HTTP_STATUS_NOT_FOUND) {
      break;
    }
  }

  // There are only three possible outcomes: success, cancel, and an error
  // other than cancel.
  HRESULT result = S_OK;
  if (SUCCEEDED(hr) || hr == GOOPDATE_E_CANCELLED) {
    result = hr;
  } else {
    ASSERT1(first_error_from_http_request_saved);
    if (first_error_from_http_request_saved) {
      // In case of errors, log the error and the response returned by the first
      // active http request object.
      result = error_hr;
      *http_status_code = error_http_status_code;
      *response_headers = error_response_headers;
      response->swap(error_response);
    } else {
      ASSERT1(false);   // BITS is the onlay channel and is disabled.
      NET_LOG(LE, (_T("Possiblly no viable network channel is available.")));
      result = E_UNEXPECTED;
    }
  }

  if (SUCCEEDED(hr)) {
    NetworkConfig::SaveProxyConfig(*cur_proxy_config_);
  }
  return result;
}

HRESULT NetworkRequestImpl::DoSendHttpRequest(
    int* http_status_code,
    CString* response_headers,
    std::vector<uint8>* response) const {
  ASSERT1(response_headers);
  ASSERT1(response);
  ASSERT1(http_status_code);

  ASSERT1(cur_http_request_);

  // Set common HttpRequestInterface properties.
  cur_http_request_->set_session_handle(network_session_.session_handle);
  cur_http_request_->set_request_buffer(request_buffer_,
                                        request_buffer_length_);
  cur_http_request_->set_url(url_);
  cur_http_request_->set_filename(filename_);
  cur_http_request_->set_low_priority(low_priority_);
  cur_http_request_->set_callback(callback_);
  cur_http_request_->set_additional_headers(BuildPerRequestHeaders());
  cur_http_request_->set_proxy_configuration(*cur_proxy_config_);
  cur_http_request_->set_preserve_protocol(preserve_protocol_);
  cur_http_request_->set_proxy_auth_config(proxy_auth_config_);

  if (IsHandleSignaled(get(event_cancel_))) {
    return GOOPDATE_E_CANCELLED;
  }

  if (callback_) {
    callback_->OnRequestBegin();
  }

  // The algorithm is very rough meaning it does not look at the error
  // returned by the Send and it blindly retries the call. For some errors
  // it may not make sense to retry at all, for example, let's say the
  // error is ERROR_DISK_FULL.
  NET_LOG(L3, (_T("[%s]"), url_));
  last_hr_ = cur_http_request_->Send();
  NET_LOG(L3, (_T("[HttpRequestInterface::Send returned 0x%08x]"), last_hr_));

  if (last_hr_ == GOOPDATE_E_CANCELLED) {
    return last_hr_;
  }

  last_http_status_code_ = cur_http_request_->GetHttpStatusCode();

  *http_status_code = cur_http_request_->GetHttpStatusCode();
  *response_headers = cur_http_request_->GetResponseHeaders();
  cur_http_request_->GetResponse().swap(*response);

  // Check if the computer is connected to the network.
  if (FAILED(last_hr_)) {
    last_hr_ = IsMachineConnectedToNetwork() ? last_hr_ : GOOPDATE_E_NO_NETWORK;
    return last_hr_;
  }

  // Status code must be available if the http request is successful. This
  // is the contract that http requests objects in the fallback chain must
  // implement.
  ASSERT1(SUCCEEDED(last_hr_) && *http_status_code);
  ASSERT1(HTTP_STATUS_FIRST <= *http_status_code &&
          *http_status_code <= HTTP_STATUS_LAST);

  switch (*http_status_code) {
    case HTTP_STATUS_OK:                // 200
    case HTTP_STATUS_NO_CONTENT:        // 204
    case HTTP_STATUS_PARTIAL_CONTENT:   // 206
    case HTTP_STATUS_NOT_MODIFIED:      // 304
      last_hr_ = S_OK;
      break;

    default:
      last_hr_ = HRESULTFromHttpStatusCode(*http_status_code);
      break;
  }
  return last_hr_;
}

CString NetworkRequestImpl::BuildPerRequestHeaders() const {
  CString headers(additional_headers_);

  const CString& user_agent(cur_http_request_->user_agent());
  if (!user_agent.IsEmpty()) {
    SafeCStringAppendFormat(&headers, _T("%s: %s\r\n"),
                                      kHeaderUserAgent, user_agent);
  }

  SafeCStringAppendFormat(&headers, _T("%s: 0x%x\r\n"),
                                    kHeaderXLastHR, last_hr_);
  SafeCStringAppendFormat(&headers,
                          _T("%s: %d\r\n"),
                          kHeaderXLastHTTPStatusCode, last_http_status_code_);

  SafeCStringAppendFormat(&headers, _T("%s: %d\r\n"),
                                    kHeaderXRetryCount, cur_retry_count_);

  return headers;
}

void NetworkRequestImpl::AddHeader(const TCHAR* name, const TCHAR* value) {
  ASSERT1(name && *name);
  ASSERT1(value && *value);
  if (_tcsicmp(additional_headers_, name) == 0) {
    return;
  }
  // Documentation specifies each header must be terminated by \r\n.
  SafeCStringAppendFormat(&additional_headers_, _T("%s: %s\r\n"), name, value);
}

HRESULT NetworkRequestImpl::QueryHeadersString(uint32 info_level,
                                               const TCHAR* name,
                                               CString* value) {
  // Name can be null when the info_level specifies the header to query.
  ASSERT1(value);
  if (!cur_http_request_) {
    return E_UNEXPECTED;
  }
  return cur_http_request_->QueryHeadersString(info_level, name, value);
}

void NetworkRequestImpl::DetectProxyConfiguration(
    std::vector<ProxyConfig>* proxy_configurations) const {
  ASSERT1(proxy_configurations);

  proxy_configurations->clear();

  // Use this object's configuration override if one is set.
  if (proxy_configuration_.get()) {
    proxy_configurations->push_back(*proxy_configuration_);
    return;
  }

  // Use the global configuration override if the network config has one.
  NetworkConfig* network_config = NULL;
  NetworkConfigManager& network_manager = NetworkConfigManager::Instance();
  HRESULT hr = network_manager.GetUserNetworkConfig(&network_config);
  if (SUCCEEDED(hr)) {
    ProxyConfig config;
    if (SUCCEEDED(network_config->GetConfigurationOverride(&config))) {
      proxy_configurations->push_back(config);
      return;
    }

    // Detect the configurations if no configuration override is specified.
    hr = network_config->Detect();
    if (SUCCEEDED(hr)) {
      network_config->GetConfigurations().swap(*proxy_configurations);
    } else {
      NET_LOG(LW, (_T("[failed to detect net config][0x%08x]"), hr));
    }

    network_config->AppendLastKnownGoodProxyConfig(proxy_configurations);
  } else {
    NET_LOG(LW, (_T("[failed to get network config instance][0x%08x]"), hr));
  }

  NetworkConfig::AppendStaticProxyConfigs(proxy_configurations);
  NetworkConfig::SortProxies(proxy_configurations);

  // Some of the configurations might occur multiple times. To avoid retrying
  // the same configuration over, try to remove duplicates while preserving
  // the order of existing configurations.
  NetworkConfig::RemoveDuplicates(proxy_configurations);
  ASSERT1(!proxy_configurations->empty());
}

HRESULT PostRequest(NetworkRequest* network_request,
                    bool fallback_to_https,
                    const CString& url,
                    const CString& request_string,
                    std::vector<uint8>* response) {
  ASSERT1(network_request);
  ASSERT1(response);

  const CStringA utf8_request_string(WideToUtf8(request_string));
  HRESULT hr = network_request->PostUtf8String(url,
                                               utf8_request_string,
                                               response);
  bool is_canceled(hr == GOOPDATE_E_CANCELLED);
  if (FAILED(hr) && !is_canceled && fallback_to_https) {
    // Replace http with https and resend the request.
    if (String_StartsWith(url, kHttpProto, true)) {
      CString https_url = url.Mid(_tcslen(kHttpProto));
      https_url.Insert(0, kHttpsProto);

      NET_LOG(L3, (_T("[network request fallback to %s]"), https_url));
      response->clear();
      if (SUCCEEDED(network_request->PostUtf8String(https_url,
                                                    utf8_request_string,
                                                    response))) {
        hr = S_OK;
      }
    }
  }
  return hr;
}

// TODO(omaha): Eliminate this function if no longer a need for it.  It's only
// used in NetDiags, and its value has been eliminated since we switched to all
// network functions returning uint8 buffers.
HRESULT GetRequest(NetworkRequest* network_request,
                   const CString& url,
                   std::vector<uint8>* response) {
  ASSERT1(network_request);
  ASSERT1(response);

  return network_request->Get(url, response);
}

}   // namespace detail

}   // namespace omaha

