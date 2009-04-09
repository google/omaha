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

#include "omaha/net/network_request_impl.h"
#include <limits.h>
#include <atlsecurity.h>
#include <algorithm>
#include <cctype>
#include <functional>
#include <vector>
#include "base/basictypes.h"
#include "omaha/common/const_addresses.h"
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/logging.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/scoped_impersonation.h"
#include "omaha/common/string.h"
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
        cur_network_config_(NULL),
        last_network_error_(S_OK),
        http_status_code_(0),
        num_retries_(0),
        low_priority_(false),
        time_between_retries_ms_(kDefaultTimeBetweenRetriesMs),
        callback_(NULL),
        request_buffer_(NULL),
        request_buffer_length_(0),
        response_(NULL),
        network_session_(network_session),
        is_canceled_(false) {
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
  http_status_code_   = 0;
  last_network_error_ = S_OK;
  cur_http_request_   = NULL;
  cur_network_config_ = NULL;
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
  return E_NOTIMPL;
}

HRESULT NetworkRequestImpl::DoSendWithRetries() {
  ASSERT1(num_retries_ >= 0);
  ASSERT1(response_ || !filename_.IsEmpty());

  // Impersonate the user if a valid impersonation token is presented.
  // Continue unimpersonated if the impersonation fails.
  HANDLE impersonation_token = network_session_.impersonation_token;
  scoped_impersonation impersonate_user(impersonation_token);
  if (impersonation_token) {
    DWORD result = impersonate_user.result();
    ASSERT(result == ERROR_SUCCESS, (_T("impersonation failed %d"), result));
    NET_LOG(L3, (_T("[impersonating %s]"), GetTokenUser(impersonation_token)));
  }

  Reset();

  int http_status_code(0);
  CString response_headers;
  std::vector<uint8> response;

  trace_.AppendFormat(_T("Url=%s\r\n"), url_);

  HRESULT hr = S_OK;
  int wait_interval_ms = time_between_retries_ms_;
  for (int i = 0; i < 1 + num_retries_; ++i) {
    if (IsHandleSignaled(get(event_cancel_))) {
      ASSERT1(is_canceled_);

      // There is no state to be set when the request is canceled.
      return OMAHA_NET_E_REQUEST_CANCELLED;
    }

    // Wait before retrying if there are retries to be done.
    if (i > 0) {
      NET_LOG(L3, (_T("[wait %d ms]"), wait_interval_ms));
      VERIFY1(::WaitForSingleObject(get(event_cancel_),
                                    wait_interval_ms) != WAIT_FAILED);

      // Compute the next wait interval and check for multiplication overflow.
      if (wait_interval_ms > INT_MAX / kTimeBetweenRetriesMultiplier) {
        ASSERT1(false);
        hr = HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
        break;
      }
      wait_interval_ms *= kTimeBetweenRetriesMultiplier;
    }

    DetectNetworkConfiguration(&network_configurations_);
    ASSERT1(!network_configurations_.empty());
    OPT_LOG(L2, (_T("[detected configurations][\r\n%s]"),
                 NetworkConfig::ToString(network_configurations_)));

    hr = DoSend(&http_status_code, &response_headers, &response);
    HttpClient::StatusCodeClass status_code_class =
        HttpClient::GetStatusCodeClass(http_status_code);
    if (SUCCEEDED(hr) ||
        hr == OMAHA_NET_E_REQUEST_CANCELLED ||
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
  ASSERT1(!network_configurations_.empty());
  for (size_t i = 0; i != network_configurations_.size(); ++i) {
    cur_network_config_ = &network_configurations_[i];
    hr = DoSendWithConfig(http_status_code, response_headers, response);
    if (i == 0 && FAILED(hr)) {
      error_hr = hr;
      error_http_status_code = *http_status_code;
      error_response_headers = *response_headers;
      error_response.swap(*response);
    }

    if (SUCCEEDED(hr) ||
        hr == OMAHA_NET_E_REQUEST_CANCELLED ||
        *http_status_code == HTTP_STATUS_NOT_FOUND) {
      break;
    }
  }

  // There are only three possible outcomes: success, cancel, and an error
  // other than cancel.
  HRESULT result = S_OK;
  if (SUCCEEDED(hr) || hr == OMAHA_NET_E_REQUEST_CANCELLED) {
    result = hr;
  } else {
    // In case of errors, log the error and the response returned by the first
    // network configuration.
    result = error_hr;
    *http_status_code = error_http_status_code;
    *response_headers = error_response_headers;
    response->swap(error_response);
  }

  OPT_LOG(L3, (_T("[Send response received][%s][0x%08x]"),
               VectorToPrintableString(*response)));

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

  HRESULT error_hr = S_OK;
  int     error_http_status_code = 0;
  CString error_response_headers;
  std::vector<uint8> error_response;

  ASSERT1(cur_network_config_);

  CString msg;
  msg.Format(_T("Trying config: %s"),
             NetworkConfig::ToString(*cur_network_config_));
  NET_LOG(L3, (_T("[msg %s]"), msg));
  trace_.AppendFormat(_T("%s.\r\n"), msg);

  HRESULT hr = S_OK;
  ASSERT1(!http_request_chain_.empty());
  for (size_t i = 0; i != http_request_chain_.size(); ++i) {
    cur_http_request_ = http_request_chain_[i];

    CString msg;
    msg.Format(_T("trying %s"), cur_http_request_->ToString());
    NET_LOG(L3, (_T("[%s]"), msg));
    trace_.AppendFormat(_T("%s.\r\n"), msg);

    hr = DoSendHttpRequest(http_status_code, response_headers, response);

    msg.Format(_T("Send request returned 0x%08x. Http status code %d"),
               hr, *http_status_code);
    NET_LOG(L3, (_T("[%s]"), msg));
    trace_.AppendFormat(_T("%s.\r\n"), msg);

    if (i == 0 && FAILED(hr)) {
      error_hr = hr;
      error_http_status_code = cur_http_request_->GetHttpStatusCode();
      error_response_headers = cur_http_request_->GetResponseHeaders();
      cur_http_request_->GetResponse().swap(error_response);
    }

    // The chain traversal stops when the request is successful or
    // it is canceled, or the status code is 404.
    // In the case of 404 response, all HttpRequests are likely to return
    // the same 404 response.
    if (SUCCEEDED(hr) ||
        hr == OMAHA_NET_E_REQUEST_CANCELLED ||
        *http_status_code == HTTP_STATUS_NOT_FOUND) {
      break;
    }
  }

  // There are only three possible outcomes: success, cancel, and an error
  // other than cancel.
  HRESULT result = S_OK;
  if (SUCCEEDED(hr) || hr == OMAHA_NET_E_REQUEST_CANCELLED) {
    result = hr;
  } else {
    // In case of errors, log the error and the response returned by the first
    // http request object.
    result = error_hr;
    *http_status_code = error_http_status_code;
    *response_headers = error_response_headers;
    response->swap(error_response);
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
  cur_http_request_->set_additional_headers(additional_headers_);
  cur_http_request_->set_network_configuration(*cur_network_config_);

  if (IsHandleSignaled(get(event_cancel_))) {
    return OMAHA_NET_E_REQUEST_CANCELLED;
  }

  // The algorithm is very rough meaning it does not look at the error
  // returned by the Send and it blindly retries the call. For some errors
  // it may not make sense to retry at all, for example, let's say the
  // error is ERROR_DISK_FULL.
  NET_LOG(L3, (_T("[%s]"), url_));
  HRESULT hr = cur_http_request_->Send();
  NET_LOG(L3, (_T("[HttpRequestInterface::Send returned 0x%08x]"), hr));

  if (hr == OMAHA_NET_E_REQUEST_CANCELLED) {
    return hr;
  }

  *http_status_code = cur_http_request_->GetHttpStatusCode();
  *response_headers = cur_http_request_->GetResponseHeaders();
  cur_http_request_->GetResponse().swap(*response);

  // Check if the computer is connected to the network.
  if (FAILED(hr)) {
    hr = IsMachineConnectedToNetwork() ? hr : GOOPDATE_E_NO_NETWORK;
    return hr;
  }

  // Status code must be available if the http request is successful. This
  // is the contract that http requests objects in the fallback chain must
  // implement.
  ASSERT1(SUCCEEDED(hr) && *http_status_code);
  ASSERT1(HTTP_STATUS_FIRST <= *http_status_code &&
          *http_status_code <= HTTP_STATUS_LAST);

  switch (*http_status_code) {
    case HTTP_STATUS_OK:                // 200
    case HTTP_STATUS_NO_CONTENT:        // 204
    case HTTP_STATUS_PARTIAL_CONTENT:   // 206
    case HTTP_STATUS_NOT_MODIFIED:      // 304
      hr = S_OK;
      break;

    default:
      hr = HRESULTFromHttpStatusCode(*http_status_code);
      break;
  }
  return hr;
}

void NetworkRequestImpl::AddHeader(const TCHAR* name, const TCHAR* value) {
  ASSERT1(name && *name);
  ASSERT1(value && *value);
  if (_tcsicmp(additional_headers_, name) == 0) {
    return;
  }
  // Documentation specifies each header must be terminated by \r\n.
  additional_headers_.AppendFormat(_T("%s: %s\r\n"), name, value);
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

void NetworkRequestImpl::DetectNetworkConfiguration(
    std::vector<Config>* network_configurations) const {
  ASSERT1(network_configurations);

  network_configurations->clear();

  // Use this object's configuration override if one is set.
  if (network_configuration_.get()) {
    network_configurations->push_back(*network_configuration_);
    return;
  }

  // Use the global configuration override if the network config has one.
  NetworkConfig& network_config(NetworkConfig::Instance());
  Config config;
  if (SUCCEEDED(network_config.GetConfigurationOverride(&config))) {
    network_configurations->push_back(config);
    return;
  }

  // Detect the configurations if no configuration override is specified.
  HRESULT hr = network_config.Detect();
  if (SUCCEEDED(hr)) {
    network_config.GetConfigurations().swap(*network_configurations);
  } else {
    NET_LOG(LW, (_T("[failed to detect net config][0x%08x]"), hr));
  }

  config = Config();
  config.source = _T("auto");

  // Always try WPAD. Might be important to try this first in the case of
  // corporate network, where direct connection might not be available at all.
  config.auto_detect = true;
  network_configurations->push_back(config);

  // Default to direct connection as a last resort.
  network_configurations->push_back(config);

  // Some of the configurations might occur multiple times. To avoid retrying
  // the same configuration over, try to remove duplicates while preserving
  // the order of existing configurations.
  NetworkConfig::RemoveDuplicates(network_configurations);
  ASSERT1(!network_configurations->empty());
}

HRESULT PostRequest(NetworkRequest* network_request,
                    bool fallback_to_https,
                    const CString& url,
                    const CString& request_string,
                    CString* response) {
  ASSERT1(network_request);
  ASSERT1(response);

  const CStringA utf8_request_string(WideToUtf8(request_string));
  std::vector<uint8> response_buffer;
  HRESULT hr = network_request->PostUtf8String(url,
                                               utf8_request_string,
                                               &response_buffer);
  bool is_canceled(hr == OMAHA_NET_E_REQUEST_CANCELLED);
  if (FAILED(hr) && !is_canceled && fallback_to_https) {
    // Replace http with https and resend the request.
    if (String_StartsWith(url, kHttpProtoScheme, true)) {
      CString https_url = url.Mid(_tcslen(kHttpProtoScheme));
      https_url.Insert(0, kHttpsProtoScheme);

      NET_LOG(L3, (_T("[network request fallback to %s]"), https_url));
      response_buffer.clear();
      if (SUCCEEDED(network_request->PostUtf8String(https_url,
                                                    utf8_request_string,
                                                    &response_buffer))) {
        hr = S_OK;
      }
    }
  }
  if (!response_buffer.empty()) {
    *response = Utf8BufferToWideChar(response_buffer);
  }
  return hr;
}

HRESULT GetRequest(NetworkRequest* network_request,
                   const CString& url,
                   CString* response) {
  ASSERT1(network_request);
  ASSERT1(response);

  std::vector<uint8> response_buffer;
  HRESULT hr = network_request->Get(url, &response_buffer);
  if (!response_buffer.empty()) {
    *response = Utf8BufferToWideChar(response_buffer);
  }
  return hr;
}

}   // namespace detail

}   // namespace omaha

