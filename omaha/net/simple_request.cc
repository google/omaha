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

// The implementation does not allow concurrent calls on the same object but
// it allows calling SimpleRequest::Cancel in order to stop an ongoing request.
// The transient state of the request is maintained by request_state_.
// The state is created by SimpleRequest::Send and it is destroyed by
// SimpleRequest::Close. The only concurrent access to the object state can
// happen during calling SimpleRequest::Cancel(), Pause() and Resume(). Cancel
// closes the connection and the request handles. This makes any of the WinHttp
// calls on these handles fail and SimpleRequest::Send return to the caller.
// Pause() also closes the handles but SimpleRequest automatically reopens
// them when Resume() is called. During resume stage, SimpleRequest sends a
// range request to continue download. During these actions, the caller is still
// blocked.

#include "omaha/net/simple_request.h"
#include <atlconv.h>
#include <intsafe.h>
#include <climits>
#include <memory>
#include <vector>
#include "omaha/base/const_addresses.h"
#include "omaha/base/constants.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/scope_guard.h"
#include "omaha/base/string.h"
#include "omaha/common/ping_event_download_metrics.h"
#include "omaha/net/network_config.h"
#include "omaha/net/network_request.h"
#include "omaha/net/proxy_auth.h"
#include "omaha/net/winhttp_adapter.h"
#include "omaha/third_party/smartany/scoped_any.h"

namespace omaha {

namespace {

// How many times should we retry when we get ERROR_WINHTTP_RESEND_REQUEST.
constexpr const int kMaxResendAttempts = 3;

}  // namespace

SimpleRequest::TransientRequestState::TransientRequestState()
    : port(0),
      is_https(false),
      http_status_code(0),
      proxy_authentication_scheme(0),
      content_length(0),
      current_bytes(0),
      request_begin_ms(0),
      request_end_ms(0) {
}

SimpleRequest::TransientRequestState::~TransientRequestState() {
}

SimpleRequest::SimpleRequest()
    : is_canceled_(false),
      is_closed_(false),
      pause_happened_(false),
      session_handle_(NULL),
      request_buffer_(NULL),
      request_buffer_length_(0),
      proxy_auth_config_(NULL, CString()),
      low_priority_(false),
      callback_(NULL),
      download_completed_(false),
      resend_count_(0) {
  SafeCStringFormat(&user_agent_, _T("%s;winhttp"),
                    NetworkConfig::GetUserAgent());

  // Create a manual reset event to wait on during network transfer.
  // The event is signaled by default meaning the network transferring is
  // enabled. Pause() resets this event to stop network activity. Resume()
  // does the opposite as Pause(). Close() and Cancel() also set the event.
  // This unlocks the thread if it is in paused state and returns control
  // to the caller.
  reset(event_resume_, ::CreateEvent(NULL, true, true, NULL));
  ASSERT1(valid(event_resume_));
}

// TODO(omaha): we should attempt to cleanup the file only if we
// created it in the first place.
SimpleRequest::~SimpleRequest() {
  Close();
  callback_ = NULL;

  // If download failed, try to clean up the target file.
  if (!download_completed_ && !filename_.IsEmpty()) {
    if (!::DeleteFile(filename_) && ::GetLastError() != ERROR_FILE_NOT_FOUND) {
      NET_LOG(LW, (_T("[SimpleRequest][Failed to delete file: %s][0x%08x]."),
                   filename_.GetString(), HRESULTFromLastError()));
    }
  }
}

void SimpleRequest::set_url(const CString& url) {
  __mutexScope(lock_);
  if (url_ != url) {
    url_ = url;
    CloseHandles();
    request_state_.reset();
  }
}

void SimpleRequest::set_filename(const CString& filename) {
  __mutexScope(lock_);
  if (filename_ != filename) {
    filename_ = filename;
    CloseHandles();
    request_state_.reset();
  }
}

HRESULT SimpleRequest::Close() {
  NET_LOG(L3, (_T("[SimpleRequest::Close]")));

  __mutexScope(lock_);
  is_closed_ = true;
  CloseHandles();
  request_state_.reset();
  winhttp_adapter_.reset();

  // Resume the downloading thread if it is blocked. It is still fine if the
  // event is set since the operation is like no-op in that case.
  return Resume();
}

HRESULT SimpleRequest::Cancel() {
  NET_LOG(L3, (_T("[SimpleRequest::Cancel]")));

  __mutexScope(lock_);
  is_canceled_ = true;
  CloseHandles();

  // Resume the downloading thread if it is blocked. It is still fine if the
  // event is set since the operation is like no-op in that case.
  return Resume();
}

void SimpleRequest::CloseHandles() {
  if (winhttp_adapter_.get()) {
    winhttp_adapter_->CloseHandles();
  }
}

bool SimpleRequest::IsResumeNeeded() const {
  __mutexScope(lock_);
  if (!IsPauseSupported() || is_canceled_ || is_closed_) {
    return false;
  }

  return pause_happened_;
}

// Pause is not supported currently.
bool SimpleRequest::IsPauseSupported() const {
  return false;
}

HRESULT SimpleRequest::Pause() {
  NET_LOG(L3, (_T("[SimpleRequest::Pause]")));

  if (!IsPauseSupported()) {
    return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
  }

  __mutexScope(ready_to_pause_lock_);
  __mutexScope(lock_);

  pause_happened_ = true;
  CloseHandles();
  return ::ResetEvent(get(event_resume_)) ? S_OK : HRESULTFromLastError();
}

HRESULT SimpleRequest::Resume() {
  NET_LOG(L3, (_T("[SimpleRequest::Resume]")));

  __mutexScope(lock_);
  HRESULT hr = S_OK;
  if (IsPauseSupported()) {
    hr = ::SetEvent(get(event_resume_)) ? S_OK : HRESULTFromLastError();
  }
  return hr;
}

void SimpleRequest::WaitForResumeEvent() {
  if (!event_resume_) {
    return;
  }

  // Reset pause_happened_ state to indicate that pause has not yet happened
  // during new resume stage.
  __mutexBlock(lock_) {
    pause_happened_ = false;
  }

  VERIFY1(::WaitForSingleObject(get(event_resume_), INFINITE) != WAIT_FAILED);
}

HRESULT SimpleRequest::Send() {
  NET_LOG(L3, (_T("[SimpleRequest::Send][%s]"), url_));

  ASSERT1(!url_.IsEmpty());
  if (!session_handle_) {
    NET_LOG(LE, (_T("[SimpleRequest: session_handle_ is NULL]")));
    return OMAHA_NET_E_WINHTTP_NOT_AVAILABLE;
  }

  HRESULT hr = S_OK;

  __mutexBlock(ready_to_pause_lock_) {
    __mutexBlock(lock_) {
      winhttp_adapter_.reset(new WinHttpAdapter());
      hr = winhttp_adapter_->Initialize();
      if (FAILED(hr)) {
        return hr;
      }

      if (!IsPauseSupported() || request_state_ == NULL) {
        request_state_.reset(new TransientRequestState);
      } else {
        // Discard all previous download states except content_length and
        // current_bytes for resume purpose. These two states will be validated
        // against the previously (partially) downloaded file when reopens the
        // target file.
        auto request_state = std::make_unique<TransientRequestState>();
        request_state->content_length = request_state_->content_length;
        request_state->current_bytes = request_state_->current_bytes;

        request_state_.swap(request_state);
      }
    }
  }

  request_state_->request_begin_ms = GetCurrentMsTime();
  hr = DoSend();
  request_state_->request_end_ms = GetCurrentMsTime();

  request_state_->download_metrics.reset(
      new DownloadMetrics(MakeDownloadMetrics(hr)));

  NET_LOG(L3, (_T("[SimpleRequest::Send][0x%x][%d]"), hr, GetHttpStatusCode()));
  return hr;
}

HRESULT SimpleRequest::DoSend() {
  ASSERT1(request_state_.get());

  HRESULT hr = S_OK;

  for (bool first_time = true, cancelled = false;
       !cancelled;
       first_time = false) {
    scoped_hfile file_handle;

    __mutexBlock(ready_to_pause_lock_) {
      if (!first_time) {
        if (IsResumeNeeded()) {
          NET_LOG(L3, (_T("[SimpleRequest::Send paused.]")));
          WaitForResumeEvent();
          NET_LOG(L3, (_T("[SimpleRequest::Send resumed.]")));
        } else {
          if (is_canceled_) {
            hr = GOOPDATE_E_CANCELLED;
          }

          // Macro __mutexBlock has a hidden loop built-in and thus one break
          // is not enough to exit the loop. Uses a flag instead.
          cancelled = true;
        }
      }

      if (!cancelled) {
        hr = PrepareRequest(address(file_handle));
      }
    }

    if (SUCCEEDED(hr) && !cancelled) {
      ASSERT1(request_state_->current_bytes <= request_state_->content_length);

      // Only requests data if there is more data to download.
      if (request_state_->content_length == 0 ||
          request_state_->current_bytes < request_state_->content_length) {
        hr = RequestData(get(file_handle));
      }
    }
  }

  return hr;
}

HRESULT SimpleRequest::Connect() {
  // Crack the url without decoding already escaped characters.
  HRESULT hr = winhttp_adapter_->CrackUrl(url_,
                                          0,
                                          &request_state_->scheme,
                                          &request_state_->server,
                                          &request_state_->port,
                                          &request_state_->url_path,
                                          NULL);
  if (FAILED(hr)) {
    return hr;
  }
  ASSERT1(!request_state_->scheme.CompareNoCase(kHttpProtoScheme) ||
          !request_state_->scheme.CompareNoCase(kHttpsProtoScheme));

  hr = winhttp_adapter_->Connect(session_handle_,
                                 request_state_->server,
                                 request_state_->port);
  if (FAILED(hr)) {
    return hr;
  }

  // TODO(omaha): figure out the accept types.
  //              figure out more flags.
  request_state_->is_https = false;
  DWORD flags = WINHTTP_FLAG_REFRESH;
  if (request_state_->scheme == kHttpsProtoScheme) {
    request_state_->is_https = true;
    flags |= WINHTTP_FLAG_SECURE;
  }
  const TCHAR* verb = IsPostRequest() ? _T("POST") : _T("GET");
  hr = winhttp_adapter_->OpenRequest(verb, request_state_->url_path,
                                     NULL, WINHTTP_NO_REFERER,
                                     WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
  if (FAILED(hr)) {
    return hr;
  }

  // Disable redirects for POST requests.
  if (IsPostRequest()) {
    VERIFY_SUCCEEDED(
        winhttp_adapter_->SetRequestOptionInt(WINHTTP_OPTION_DISABLE_FEATURE,
                                              WINHTTP_DISABLE_REDIRECTS));
  }

  CString additional_headers = additional_headers_;

  // If the target has been partially downloaded, send a range request to resume
  // download, instead of starting from scratch again.
  if (request_state_->current_bytes != 0 &&
      request_state_->current_bytes != request_state_->content_length) {
    ASSERT1(request_state_->current_bytes < request_state_->content_length);
    SafeCStringAppendFormat(&additional_headers, _T("Range: bytes=%d-\r\n"),
                            request_state_->current_bytes);
  }
  if (!additional_headers.IsEmpty()) {
    uint32 header_flags = WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE;
    hr = winhttp_adapter_->AddRequestHeaders(additional_headers,
                                             -1,
                                             header_flags);
    if (FAILED(hr)) {
      return hr;
    }
  }

  hr = SetProxyInformation();
  if (FAILED(hr)) {
    return hr;
  }

  return S_OK;
}

HRESULT SimpleRequest::OpenDestinationFile(HANDLE* file_handle) {
  ASSERT1(!filename_.IsEmpty());
  ASSERT1(file_handle);

  DWORD create_disposition = request_state_->content_length == 0 ?
                             CREATE_ALWAYS : OPEN_ALWAYS;

  scoped_hfile file(::CreateFile(filename_, GENERIC_WRITE, 0, NULL,
                                 create_disposition, FILE_ATTRIBUTE_NORMAL,
                                 NULL));

  if (!file) {
    return HRESULTFromLastError();
  }

  if (request_state_->content_length != 0) {
    DWORD raw_file_size = ::GetFileSize(get(file), NULL);
    if (INVALID_FILE_SIZE == raw_file_size || raw_file_size > INT_MAX) {
      return E_FAIL;
    }
    int file_size = static_cast<int>(raw_file_size);

    // Local file size should not be greater than remote file size and file
    // size must match the number of bytes we previously downloaded. If not,
    // reset the local file.
    bool need_reset_file = file_size > request_state_->content_length;
    need_reset_file |= file_size != request_state_->current_bytes;

    if (need_reset_file) {
      // Need to download from byte 0. Reopen the file with truncation.
      request_state_->current_bytes = 0;
      reset(file, ::CreateFile(filename_, GENERIC_WRITE, 0, NULL,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL));

      if (!file) {
        return HRESULTFromLastError();
      }
    } else {
      LARGE_INTEGER start_pos;
      start_pos.LowPart = static_cast<DWORD>(request_state_->current_bytes);
      start_pos.HighPart = 0;
      if (!::SetFilePointerEx(get(file), start_pos, NULL, FILE_BEGIN)) {
        return HRESULTFromLastError();
      }
    }
  } else {
    // Always start from byte 0 if we don't know remote file size.
    request_state_->current_bytes = 0;
  }

  *file_handle = release(file);
  return S_OK;
}

HRESULT SimpleRequest::SendRequest() {
  int proxy_retry_count = 0;
  int max_proxy_retries = 1;
  CString username;
  CString password;
  HRESULT hr = S_OK;

  if (request_buffer_length_ > DWORD_MAX) {
    return E_FAIL;
  }

  bool done = false;
  while (!done) {
    uint32& request_scheme = request_state_->proxy_authentication_scheme;
    if (request_scheme) {
      NET_LOG(L3, (_T("[SimpleRequest::SendRequest][auth_scheme][%d]"),
          request_scheme));
      winhttp_adapter_->SetCredentials(WINHTTP_AUTH_TARGET_PROXY,
                                       request_scheme,
                                       username, password);

      CString headers;
      SafeCStringFormat(&headers, _T("%s: %d\r\n"),
                        kHeaderXProxyRetryCount, proxy_retry_count);
      if (!username.IsEmpty()) {
        SafeCStringAppendFormat(&headers, _T("%s: 1\r\n"),
                                kHeaderXProxyManualAuth);
      }
      uint32 flags = WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE;
      VERIFY_SUCCEEDED(winhttp_adapter_->AddRequestHeaders(headers,
                                                            -1,
                                                            flags));
    }

    const DWORD bytes_to_send = static_cast<DWORD>(request_buffer_length_);
    hr = winhttp_adapter_->SendRequest(NULL,
                                       0,
                                       request_buffer_,
                                       bytes_to_send,
                                       bytes_to_send);
    if (FAILED(hr)) {
      return hr;
    }
    NET_LOG(L3,
        (_T("[SimpleRequest::SendRequest][request sent][server: %s][IP: %s]"),
         winhttp_adapter_->server_name(),
         winhttp_adapter_->server_ip()));

    hr = winhttp_adapter_->ReceiveResponse();
#if DEBUG
    LogResponseHeaders();
#endif
    if (hr == HRESULT_FROM_WIN32(ERROR_WINHTTP_RESEND_REQUEST)) {
      // Resend the request if needed, likely because the authentication
      // scheme requires many transactions on the same handle.

      // Avoid infinite resend loop.
      if (++resend_count_ >= kMaxResendAttempts)
        return hr;

      continue;
    } else if (FAILED(hr)) {
      return hr;
    }

    resend_count_ = 0;

    hr = winhttp_adapter_->QueryRequestHeadersInt(
        WINHTTP_QUERY_STATUS_CODE,
        NULL,
        &request_state_->http_status_code,
        NULL);
    if (FAILED(hr)) {
      return hr;
    }
    if (request_state_->http_status_code < HTTP_STATUS_FIRST ||
        request_state_->http_status_code > HTTP_STATUS_LAST) {
      return E_FAIL;
    }

    NetworkConfigManager& network_manager = NetworkConfigManager::Instance();
    switch (request_state_->http_status_code) {
      case HTTP_STATUS_DENIED:
        // 401 responses are not supported. Omaha does not have to authenticate
        // to our backend.
        done = true;
        break;

      case HTTP_STATUS_PROXY_AUTH_REQ: {
        NET_LOG(L2, (_T("[http proxy requires authentication]")));
        ++proxy_retry_count;
        if (proxy_retry_count > max_proxy_retries) {
          // If we get multiple 407s in a row then we are done. It does not make
          // sense to retry further.
          done = true;
          break;
        }
        if (!request_scheme) {
          uint32 supported_schemes(0), first_scheme(0), auth_target(0);
          hr = winhttp_adapter_->QueryAuthSchemes(&supported_schemes,
                                                  &first_scheme,
                                                  &auth_target);
          if (FAILED(hr)) {
            return hr;
          }
          ASSERT1(auth_target == WINHTTP_AUTH_TARGET_PROXY);
          request_scheme = ChooseProxyAuthScheme(supported_schemes);
          ASSERT1(request_scheme);
          NET_LOG(L3, (_T("[SimpleRequest::SendRequest][Auth scheme][%d]"),
              request_scheme));
          if (request_scheme == WINHTTP_AUTH_SCHEME_NEGOTIATE ||
              request_scheme == WINHTTP_AUTH_SCHEME_NTLM) {
            // Increases the retry count. Tries to do an autologon at first, and
            // if that fails, will call GetProxyCredentials below.
            ++max_proxy_retries;
            break;
          }
        }

        uint32 auth_scheme = UNKNOWN_AUTH_SCHEME;
        // May prompt the user for credentials, or get cached credentials.
        NetworkConfig* network_config = NULL;
        hr = network_manager.GetUserNetworkConfig(&network_config);
        if (FAILED(hr)) {
          return hr;
        }
        if (!network_config->GetProxyCredentials(true,
                                                 false,
                                                 request_state_->proxy,
                                                 proxy_auth_config_,
                                                 request_state_->is_https,
                                                 &username,
                                                 &password,
                                                 &auth_scheme)) {
          NET_LOG(LE,
                  (_T("[SimpleRequest::SendRequest][GetProxyCreds failed]")));
          done = true;
          break;
        }
        if (auth_scheme != UNKNOWN_AUTH_SCHEME) {
          // Uses the known scheme that was successful previously.
          request_scheme = auth_scheme;
        }
        break;
      }

      default:
        // We got some kind of response. If we have a valid username, we
        // record the auth scheme with the NetworkConfig, so it can be cached
        // for future use within this process for current user..
        if (!username.IsEmpty()) {
          NetworkConfig* network_config = NULL;
          hr = network_manager.GetUserNetworkConfig(&network_config);
          if (SUCCEEDED(hr)) {
            VERIFY_SUCCEEDED(network_config->SetProxyAuthScheme(
                request_state_->proxy, request_state_->is_https,
                request_scheme));
          }
        }
        done = true;
        break;
    }
  }

  return hr;
}

HRESULT SimpleRequest::ReceiveData(HANDLE file_handle) {
  ASSERT1(file_handle != INVALID_HANDLE_VALUE || filename_.IsEmpty());

  HRESULT hr = S_OK;

  // In the case of a "204 No Content" response, WinHttp blocks when
  // querying or reading the available data. According to the RFC,
  // the 204 response must not include a message-body, and thus is always
  // terminated by the first empty line after the header fields.
  // It appears WinHttp does not internally handles the 204 response. If this,
  // condition is not handled here explicitly, WinHttp will timeout when
  // waiting for the data instead of returning right away.
  if (request_state_->http_status_code == HTTP_STATUS_NO_CONTENT) {
    return S_OK;
  }

  int content_length = 0;
  winhttp_adapter_->QueryRequestHeadersInt(WINHTTP_QUERY_CONTENT_LENGTH,
                                           WINHTTP_HEADER_NAME_BY_INDEX,
                                           &content_length,
                                           WINHTTP_NO_HEADER_INDEX);
  if (request_state_->content_length == 0) {
    request_state_->content_length = content_length;
    request_state_->current_bytes = 0;
  }

  const bool is_http_success =
      request_state_->http_status_code == HTTP_STATUS_OK ||
      request_state_->http_status_code == HTTP_STATUS_PARTIAL_CONTENT;

  std::vector<uint8> buffer;
  do  {
    DWORD bytes_available(0);
    winhttp_adapter_->QueryDataAvailable(&bytes_available);
    buffer.resize(1 + bytes_available);
    hr = winhttp_adapter_->ReadData(&buffer.front(),
                                    static_cast<DWORD>(buffer.size()),
                                    &bytes_available);
    if (FAILED(hr)) {
      return hr;
    }

    buffer.resize(bytes_available);
    if (!buffer.empty()) {
      if (!filename_.IsEmpty()) {
        DWORD num_bytes(0);
        if (!::WriteFile(file_handle,
                         reinterpret_cast<const char*>(&buffer.front()),
                         static_cast<DWORD>(buffer.size()),
                         &num_bytes,
                         NULL)) {
          return HRESULTFromLastError();
        }
        ASSERT1(num_bytes == buffer.size());
      } else {
        request_state_->response.insert(request_state_->response.end(),
                                        buffer.begin(),
                                        buffer.end());
      }
    }

    // Update current_bytes after those bytes are serialized in case we
    // pause before current_bytes is updated, we can throw away the last
    // batch of bytes received and resume.
    request_state_->current_bytes += bytes_available;
    if (request_state_->content_length) {
      ASSERT1(request_state_->current_bytes <= request_state_->content_length);
    }

    // The callback is called only for 200 or 206 http codes.
    if (callback_ && request_state_->content_length && is_http_success) {
      callback_->OnProgress(request_state_->current_bytes,
                            request_state_->content_length,
                            WINHTTP_CALLBACK_STATUS_READ_COMPLETE,
                            NULL);
    }
  } while (!buffer.empty());

  NET_LOG(L3, (_T("[bytes downloaded %d]"), request_state_->current_bytes));
  if (file_handle != INVALID_HANDLE_VALUE) {
    // All bytes must be written to the file in the file download case.
    ASSERT1(::SetFilePointer(file_handle, 0, NULL, FILE_CURRENT) ==
            static_cast<DWORD>(request_state_->current_bytes));
  }

  if (request_state_->content_length &&
      request_state_->content_length != request_state_->current_bytes) {
    return HRESULT_FROM_WIN32(ERROR_WINHTTP_CONNECTION_ERROR);
  }

  download_completed_ = true;
  return hr;
}

HRESULT SimpleRequest::PrepareRequest(HANDLE* file_handle) {
  // Read the remaining bytes of the body. If we have a file to save the
  // response into, create the file.
  HRESULT hr = S_OK;
  if (!filename_.IsEmpty()) {
    hr = OpenDestinationFile(file_handle);
    if (FAILED(hr)) {
      return hr;
    }
  } else {
    // Always restarts if downloading to memory.
    request_state_->current_bytes = 0;
    request_state_->response.clear();
  }

  return Connect();
}

HRESULT SimpleRequest::RequestData(HANDLE file_handle) {
  HRESULT hr = SendRequest();
  if (FAILED(hr)) {
    // WININET_E_DECODING_FAILED is equivalent to
    // HRESULT_FROM_WIN32(ERROR_WINHTTP_SECURE_FAILURE) as well as
    // HRESULT_FROM_WIN32(ERROR_INTERNET_DECODING_FAILED). Distinguish the two.
    if (hr == WININET_E_DECODING_FAILED) {
      HRESULT secure_status_hr =
          winhttp_adapter_->GetErrorFromSecureStatusFlag();

      if (FAILED(secure_status_hr)) {
        OPT_LOG(LE, (L"[SimpleRequest::RequestData]"
                     L"[Changing hresult from: 0x%8x to: 0x%8x]",
                     hr, secure_status_hr));
        hr = secure_status_hr;
      }
    }

    return hr;
  }

  return ReceiveData(file_handle);
}

std::vector<uint8> SimpleRequest::GetResponse() const {
  return request_state_.get() ? request_state_->response :
                                std::vector<uint8>();
}

HRESULT SimpleRequest::QueryHeadersString(uint32 info_level,
                                          const TCHAR* name,
                                          CString* value) const {
  // Name can be null when the info_level specifies the header to query.
  if (winhttp_adapter_.get()) {
    return winhttp_adapter_->QueryRequestHeadersString(info_level,
                                                       name,
                                                       value,
                                                       WINHTTP_NO_HEADER_INDEX);
  } else {
    return E_UNEXPECTED;
  }
}

CString SimpleRequest::GetResponseHeaders() const {
  if (winhttp_adapter_.get()) {
    CString response_headers;
    if (SUCCEEDED(winhttp_adapter_->QueryRequestHeadersString(
        WINHTTP_QUERY_RAW_HEADERS_CRLF,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &response_headers,
        WINHTTP_NO_HEADER_INDEX))) {
      return response_headers;
    }
  }
  return CString();
}

uint32 SimpleRequest::ChooseProxyAuthScheme(uint32 supported_schemes) {
  // It is the server's responsibility only to accept
  // authentication schemes that provide a sufficient level
  // of security to protect the server's resources.
  //
  // The client is also obligated only to use an authentication
  // scheme that adequately protects its username and password.
  //
  // TODO(omaha): remove Basic authentication because Basic authentication
  // exposes the client's username and password to anyone monitoring
  // the connection. This option is here for Fiddler testing purposes.

  uint32 auth_schemes[] = {
    WINHTTP_AUTH_SCHEME_NEGOTIATE,
    WINHTTP_AUTH_SCHEME_NTLM,
    WINHTTP_AUTH_SCHEME_DIGEST,
    WINHTTP_AUTH_SCHEME_BASIC,
  };
  for (int i = 0; i < arraysize(auth_schemes); ++i) {
    if (supported_schemes & auth_schemes[i]) {
      return auth_schemes[i];
    }
  }

  return 0;
}

HRESULT SimpleRequest::SetProxyInformation() {
  bool uses_proxy = false;
  HRESULT hr = S_FALSE;
  CString proxy, proxy_bypass;

  const int access_type = NetworkConfig::GetAccessType(proxy_config_);
  if (access_type == WINHTTP_ACCESS_TYPE_AUTO_DETECT) {
    HttpClient::ProxyInfo proxy_info = {0};
    NetworkConfig* network_config = NULL;
    NetworkConfigManager& network_manager = NetworkConfigManager::Instance();
    hr = network_manager.GetUserNetworkConfig(&network_config);
    if (SUCCEEDED(hr)) {
      hr = network_config->GetProxyForUrl(
          url_,
          proxy_config_.auto_detect,
          proxy_config_.auto_config_url,
          &proxy_info);
      if (SUCCEEDED(hr)) {
        // The result of proxy auto-detection could be that either a proxy is
        // found, or direct connection is allowed for the specified url.
        ASSERT(proxy_info.access_type == WINHTTP_ACCESS_TYPE_NAMED_PROXY ||
               proxy_info.access_type == WINHTTP_ACCESS_TYPE_NO_PROXY,
               (_T("[Unexpected access_type][%d]"), proxy_info.access_type));

        uses_proxy = proxy_info.access_type == WINHTTP_ACCESS_TYPE_NAMED_PROXY;

        proxy = proxy_info.proxy;
        proxy_bypass = proxy_info.proxy_bypass;

        ::GlobalFree(const_cast<wchar_t*>(proxy_info.proxy));
        ::GlobalFree(const_cast<wchar_t*>(proxy_info.proxy_bypass));
      } else {
        ASSERT1(!uses_proxy);
        NET_LOG(LW, (_T("[GetProxyForUrl failed][0x%08x]"), hr));
      }
    } else {
      NET_LOG(LW, (_T("[GetUserNetworkConfig failed][0x%08x]"), hr));
    }
  } else if (access_type == WINHTTP_ACCESS_TYPE_NAMED_PROXY) {
    uses_proxy = true;
    proxy = proxy_config_.proxy;
    proxy_bypass = proxy_config_.proxy_bypass;
  }

  // If a proxy is going to be used, modify the state of the object and
  // set the proxy information on the request handle.
  if (uses_proxy) {
    ASSERT1(!proxy.IsEmpty());

    request_state_->proxy        = proxy;
    request_state_->proxy_bypass = proxy_bypass;

    HttpClient::ProxyInfo proxy_info = {0};
    proxy_info.access_type = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
    proxy_info.proxy = request_state_->proxy;
    proxy_info.proxy_bypass = request_state_->proxy_bypass;

    NET_LOG(L3, (_T("[using proxy %s]"), proxy_info.proxy));
    hr = winhttp_adapter_->SetRequestOption(WINHTTP_OPTION_PROXY,
                                            &proxy_info,
                                            sizeof(proxy_info));
    if (FAILED(hr)) {
      NET_LOG(LW, (_T("[SetRequestOption failed][0x%08x]"), hr));
    }
    return hr;
  }

  NET_LOG(L3, (_T("[using direct]")));
  return S_OK;
}

void SimpleRequest::LogResponseHeaders() {
  CString response_headers;
  winhttp_adapter_->QueryRequestHeadersString(WINHTTP_QUERY_RAW_HEADERS_CRLF,
                                              WINHTTP_HEADER_NAME_BY_INDEX,
                                              &response_headers,
                                              WINHTTP_NO_HEADER_INDEX);
  NET_LOG(L3, (_T("[response headers...]\r\n%s"), response_headers));
}

DownloadMetrics SimpleRequest::MakeDownloadMetrics(HRESULT hr) const {
  ASSERT1(request_state_.get());

  // The error reported by the metrics is:
  //  * 0 when the status code is 200
  //  * an HRESULT derived from the status code, if a status code is available
  //  * an HRESULT for any other error that could have happened.
  const int status_code = request_state_->http_status_code;
  const int error = status_code == HTTP_STATUS_OK ?
      0 : (SUCCEEDED(hr) ? HRESULTFromHttpStatusCode(status_code) : hr);

  DownloadMetrics download_metrics;
  download_metrics.url = url_;
  download_metrics.downloader = DownloadMetrics::kWinHttp;
  download_metrics.error = error;
  download_metrics.downloaded_bytes = request_state_->current_bytes;
  download_metrics.total_bytes = request_state_->content_length;
  download_metrics.download_time_ms =
      request_state_->request_end_ms - request_state_->request_begin_ms;
  return download_metrics;
}

bool SimpleRequest::download_metrics(DownloadMetrics* dm) const {
  ASSERT1(dm);
  if (request_state_.get() && request_state_->download_metrics.get()) {
    *dm = *(request_state_->download_metrics);
    return true;
  } else {
    return false;
  }
}

}  // namespace omaha
