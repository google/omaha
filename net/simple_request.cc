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

// The implementation does not allow concurrent calls on the same object but
// it allows calling SimpleRequest::Cancel in order to stop an ongoing request.
// The transient state of the request is maintained by request_state_.
// The state is created by SimpleRequest::Send and it is destroyed by
// SimpleRequest::Close. The only concurrent access to the object state can
// happened during calling SimpleRequest::Cancel. Cancel closes the
// connection and the request handles. This makes any of the WinHttp calls
// on these handles fail and SimpleRequest::Send return to the caller.

#include "omaha/net/simple_request.h"

#include <atlconv.h>
#include <climits>
#include <vector>
#include "omaha/common/const_addresses.h"
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/logging.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/scope_guard.h"
#include "omaha/common/string.h"
#include "omaha/net/http_client.h"
#include "omaha/net/network_config.h"
#include "omaha/net/network_request.h"
#include "omaha/net/proxy_auth.h"

namespace omaha {

SimpleRequest::SimpleRequest()
    : request_buffer_(NULL),
      request_buffer_length_(0),
      is_canceled_(false),
      session_handle_(NULL),
      low_priority_(false),
      callback_(NULL) {
  user_agent_.Format(_T("%s;winhttp"), NetworkConfig::GetUserAgent());
  http_client_.reset(CreateHttpClient());
}

SimpleRequest::~SimpleRequest() {
  Close();
  callback_ = NULL;
}

HRESULT SimpleRequest::Close() {
  NET_LOG(L3, (_T("[SimpleRequest::Close]")));
  __mutexBlock(lock_) {
    CloseHandles(request_state_.get());
    request_state_.reset();
  }
  return S_OK;
}

HRESULT SimpleRequest::Cancel() {
  NET_LOG(L3, (_T("[SimpleRequest::Cancel]")));
  __mutexBlock(lock_) {
    is_canceled_ = true;
    CloseHandles(request_state_.get());
  }
  return S_OK;
}

HRESULT SimpleRequest::Send() {
  NET_LOG(L3, (_T("[SimpleRequest::Send][%s]"), url_));

  ASSERT1(!url_.IsEmpty());
  if (!session_handle_) {
    // Winhttp could not be loaded.
    NET_LOG(LW, (_T("[SimpleRequest: session_handle_ is NULL.]")));
    // TODO(omaha): This makes an assumption that only WinHttp is
    // supported by the network code.
    return OMAHA_NET_E_WINHTTP_NOT_AVAILABLE;
  }

  __mutexBlock(lock_) {
    CloseHandles(request_state_.get());
    request_state_.reset(new TransientRequestState);
  }

  HRESULT hr = DoSend();
  int status_code(GetHttpStatusCode());
  if (hr == HRESULT_FROM_WIN32(ERROR_WINHTTP_OPERATION_CANCELLED) ||
      is_canceled_) {
    hr = OMAHA_NET_E_REQUEST_CANCELLED;
  }
  NET_LOG(L3, (_T("[SimpleRequest::Send][0x%08x][%d]"), hr, status_code));
  return hr;
}

HRESULT SimpleRequest::DoSend() {
  // First chance to see if it is canceled.
  if (is_canceled_) {
    return OMAHA_NET_E_REQUEST_CANCELLED;
  }

  HRESULT hr = http_client_->Initialize();
  if (FAILED(hr)) {
    return hr;
  }

  hr = http_client_->CrackUrl(url_,
                              ICU_DECODE,
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

  hr = http_client_->Connect(session_handle_,
                             request_state_->server,
                             request_state_->port,
                             &request_state_->connection_handle);
  if (FAILED(hr)) {
    return hr;
  }

  // TODO(omaha): figure out the accept types.
  //              figure out more flags.
  DWORD flags = WINHTTP_FLAG_REFRESH;
  bool is_https = false;
  if (request_state_->scheme == kHttpsProtoScheme) {
    is_https = true;
    flags |= WINHTTP_FLAG_SECURE;
  }
  const TCHAR* verb = IsPostRequest() ? _T("POST") : _T("GET");
  hr = http_client_->OpenRequest(request_state_->connection_handle,
                                 verb, request_state_->url_path,
                                 NULL, WINHTTP_NO_REFERER,
                                 WINHTTP_DEFAULT_ACCEPT_TYPES, flags,
                                 &request_state_->request_handle);
  if (FAILED(hr)) {
    return hr;
  }

  // Disable redirects for POST requests.
  if (IsPostRequest()) {
    VERIFY1(SUCCEEDED(http_client_->SetOptionInt(request_state_->request_handle,
                                                 WINHTTP_OPTION_DISABLE_FEATURE,
                                                 WINHTTP_DISABLE_REDIRECTS)));
  }

  additional_headers_.AppendFormat(_T("User-Agent: %s\r\n"), user_agent_);
  uint32 header_flags = WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE;
  hr = http_client_->AddRequestHeaders(request_state_->request_handle,
                                       additional_headers_,
                                       -1,
                                       header_flags);
  if (FAILED(hr)) {
    return hr;
  }

  // If the WPAD detection fails, allow the request to go direct connection.
  SetProxyInformation();

  // The purpose of the status callback is informational only.
  HttpClient::StatusCallback old_callback =
      http_client_->SetStatusCallback(request_state_->request_handle,
                                      &SimpleRequest::StatusCallback,
                                      WINHTTP_CALLBACK_FLAG_ALL_NOTIFICATIONS);
  // No previous callback should be there for this handle or the callback
  // could not be installed, for example, when the request handle has been
  // canceled already.
  const HttpClient::StatusCallback kInvalidStatusCallback =
      reinterpret_cast<HttpClient::StatusCallback>(
          WINHTTP_INVALID_STATUS_CALLBACK);
  ASSERT1(old_callback == NULL ||
          old_callback == kInvalidStatusCallback);

  // Up to this point nothing has been sent over the wire.
  // Second chance to see if it is canceled.
  if (is_canceled_) {
    return OMAHA_NET_E_REQUEST_CANCELLED;
  }

  int proxy_retry_count = 0;
  int max_proxy_retries = 1;
  CString username;
  CString password;

  bool done = false;
  while (!done) {
    uint32& request_scheme = request_state_->proxy_authentication_scheme;
    if (request_scheme) {
      NET_LOG(L3, (_T("[SR::DoSend][auth_scheme][%d]"), request_scheme));
      http_client_->SetCredentials(request_state_->request_handle,
                                   WINHTTP_AUTH_TARGET_PROXY,
                                   request_scheme,
                                   username, password);
    }

    size_t bytes_to_send = request_buffer_length_;
    hr = http_client_->SendRequest(request_state_->request_handle,
                                   NULL,
                                   0,
                                   request_buffer_,
                                   bytes_to_send,
                                   bytes_to_send);
    if (FAILED(hr)) {
      return hr;
    }
    NET_LOG(L3, (_T("[SimpleRequest::DoSend][request sent]")));

    hr = http_client_->ReceiveResponse(request_state_->request_handle);
#if DEBUG
    LogResponseHeaders();
#endif
    if (hr == ERROR_WINHTTP_RESEND_REQUEST) {
      // Resend the request if needed, likely because the authentication
      // scheme requires many transactions on the same handle.
      continue;
    } else if (FAILED(hr)) {
      return hr;
    }

    hr = http_client_->QueryHeadersInt(request_state_->request_handle,
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
          hr = http_client_->QueryAuthSchemes(request_state_->request_handle,
                                              &supported_schemes,
                                              &first_scheme,
                                              &auth_target);
          if (FAILED(hr)) {
            return hr;
          }
          ASSERT1(auth_target == WINHTTP_AUTH_TARGET_PROXY);
          request_scheme = ChooseProxyAuthScheme(supported_schemes);
          ASSERT1(request_scheme);
          NET_LOG(L3, (_T("[SR::DoSend][Auth scheme][%d]"), request_scheme));
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
        NetworkConfig& network_config = NetworkConfig::Instance();
        if (!network_config.GetProxyCredentials(true,
                                                false,
                                                request_state_->proxy,
                                                is_https,
                                                &username,
                                                &password,
                                                &auth_scheme)) {
          NET_LOG(LE, (_T("[SimpleRequest::DoSend][GetProxyCreds failed]")));
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
        // for future use within this process.
        if (!username.IsEmpty()) {
          VERIFY1(SUCCEEDED(NetworkConfig::Instance().SetProxyAuthScheme(
              request_state_->proxy, is_https, request_scheme)));
        }
        done = true;
        break;
    }
  }

  // In the case of a "204 No Content" response, WinHttp blocks when
  // querying or reading the available data. According to the RFC,
  // the 204 response must not include a message-body, and thus is always
  // terminated by the first empty line after the header fields.
  // It appears WinHttp does not internally handles the 204 response.If this,
  // condition is not handled here explicitly, WinHttp will timeout when
  // waiting for the data instead of returning right away.
  if (request_state_->http_status_code == HTTP_STATUS_NO_CONTENT) {
    return S_OK;
  }

  http_client_->QueryHeadersInt(request_state_->request_handle,
                                WINHTTP_QUERY_CONTENT_LENGTH,
                                WINHTTP_HEADER_NAME_BY_INDEX,
                                &request_state_->content_length,
                                WINHTTP_NO_HEADER_INDEX);

  // Read the remaining bytes of the body. If we have a file to save the
  // response into, create the file.
  // TODO(omaha): we should attempt to cleanup the file only if we
  // created it in the first place.
  ScopeGuard delete_file_guard = MakeGuard(::DeleteFile, filename_);
  scoped_hfile file_handle;
  if (!filename_.IsEmpty()) {
    reset(file_handle, ::CreateFile(filename_, GENERIC_WRITE, 0, NULL,
                                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                                    NULL));
    if (!file_handle) {
      return HRESULTFromLastError();
    }
  }

  const bool is_http_success =
      request_state_->http_status_code == HTTP_STATUS_OK ||
      request_state_->http_status_code == HTTP_STATUS_PARTIAL_CONTENT;

  std::vector<uint8> buffer;
  do  {
    DWORD bytes_available(0);
    http_client_->QueryDataAvailable(request_state_->request_handle,
                                     &bytes_available);
    buffer.resize(1 + bytes_available);
    hr = http_client_->ReadData(request_state_->request_handle,
                                &buffer.front(),
                                buffer.size(),
                                &bytes_available);
    if (FAILED(hr)) {
      return hr;
    }

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

    buffer.resize(bytes_available);
    if (!buffer.empty()) {
      if (!filename_.IsEmpty()) {
        DWORD num_bytes(0);
        if (!::WriteFile(get(file_handle),
                         reinterpret_cast<const char*>(&buffer.front()),
                         buffer.size(), &num_bytes, NULL)) {
          return HRESULTFromLastError();
        }
        ASSERT1(num_bytes == buffer.size());
      } else {
        request_state_->response.insert(request_state_->response.end(),
                                        buffer.begin(),
                                        buffer.end());
      }
    }
  } while (!buffer.empty());

  NET_LOG(L3, (_T("[bytes downloaded %d]"), request_state_->current_bytes));
  if (file_handle) {
    // All bytes must be written to the file in the file download case.
    ASSERT1(::SetFilePointer(get(file_handle), 0, NULL, FILE_CURRENT) ==
            static_cast<DWORD>(request_state_->current_bytes));
  }

  delete_file_guard.Dismiss();
  return S_OK;
}

std::vector<uint8> SimpleRequest::GetResponse() const {
  return request_state_.get() ? request_state_->response :
                                std::vector<uint8>();
}

HRESULT SimpleRequest::QueryHeadersString(uint32 info_level,
                                          const TCHAR* name,
                                          CString* value) const {
  // Name can be null when the info_level specifies the header to query.
  ASSERT1(value);
  if (!http_client_.get() ||
      !request_state_.get() ||
      !request_state_->request_handle) {
    return E_UNEXPECTED;
  }

  return http_client_->QueryHeadersString(request_state_->request_handle,
                                          info_level,
                                          name,
                                          value,
                                          WINHTTP_NO_HEADER_INDEX);
}

CString SimpleRequest::GetResponseHeaders() const {
  CString response_headers;
  if (http_client_.get() &&
      request_state_.get() &&
      request_state_->request_handle) {
    CString response_headers;
    if (SUCCEEDED(http_client_->QueryHeadersString(
        request_state_->request_handle,
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

void SimpleRequest::SetProxyInformation() {
  bool uses_proxy = false;
  CString proxy, proxy_bypass;
  int access_type = NetworkConfig::GetAccessType(network_config_);
  if (access_type == WINHTTP_ACCESS_TYPE_AUTO_DETECT) {
    HttpClient::ProxyInfo proxy_info = {0};
    HRESULT hr = NetworkConfig::Instance().GetProxyForUrl(
        url_,
        network_config_.auto_config_url,
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
  } else if (access_type == WINHTTP_ACCESS_TYPE_NAMED_PROXY) {
    uses_proxy = true;
    proxy = network_config_.proxy;
    proxy_bypass = network_config_.proxy_bypass;
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
    VERIFY1(SUCCEEDED(http_client_->SetOption(request_state_->request_handle,
                                              WINHTTP_OPTION_PROXY,
                                              &proxy_info,
                                              sizeof(proxy_info))));
  }
}

void SimpleRequest::LogResponseHeaders() {
  CString response_headers;
  http_client_->QueryHeadersString(request_state_->request_handle,
                                   WINHTTP_QUERY_RAW_HEADERS_CRLF,
                                   WINHTTP_HEADER_NAME_BY_INDEX,
                                   &response_headers,
                                   WINHTTP_NO_HEADER_INDEX);
  NET_LOG(L3, (_T("[response headers...]\r\n%s"), response_headers));
}

void __stdcall SimpleRequest::StatusCallback(HINTERNET handle,
                                             uint32 context,
                                             uint32 status,
                                             void* info,
                                             size_t info_len) {
  UNREFERENCED_PARAMETER(context);

  CString status_string;
  CString info_string;
  switch (status) {
    case WINHTTP_CALLBACK_STATUS_RESOLVING_NAME:
      status_string = _T("resolving");
      info_string.SetString(static_cast<TCHAR*>(info), info_len);  // host name
      break;
    case WINHTTP_CALLBACK_STATUS_NAME_RESOLVED:
      status_string = _T("resolved");
      info_string.SetString(static_cast<TCHAR*>(info), info_len);  // host ip
      break;
    case WINHTTP_CALLBACK_STATUS_CONNECTING_TO_SERVER:
      status_string = _T("connecting");
      info_string.SetString(static_cast<TCHAR*>(info), info_len);  // host ip
      break;
    case WINHTTP_CALLBACK_STATUS_CONNECTED_TO_SERVER:
      status_string = _T("connected");
      info_string.SetString(static_cast<TCHAR*>(info), info_len);  // host ip
      break;
    case WINHTTP_CALLBACK_STATUS_SENDING_REQUEST:
      status_string = _T("sending");
      break;
    case WINHTTP_CALLBACK_STATUS_REQUEST_SENT:
      status_string = _T("sent");
      break;
    case WINHTTP_CALLBACK_STATUS_RECEIVING_RESPONSE:
      status_string = _T("receiving");
      break;
    case WINHTTP_CALLBACK_STATUS_RESPONSE_RECEIVED:
      status_string = _T("received");
      break;
    case WINHTTP_CALLBACK_STATUS_CLOSING_CONNECTION:
      status_string = _T("closing");
      break;
    case WINHTTP_CALLBACK_STATUS_CONNECTION_CLOSED:
      status_string = _T("closed");
      break;
    case WINHTTP_CALLBACK_STATUS_REDIRECT:
      status_string = _T("redirect");
      info_string.SetString(static_cast<TCHAR*>(info), info_len);  // url
      break;
    default:
      break;
  }
  CString log_line;
  log_line.AppendFormat(_T("[HttpClient::StatusCallback][0x%08x]"), handle);
  if (!status_string.IsEmpty()) {
    log_line.AppendFormat(_T("[%s]"), status_string);
  } else {
    log_line.AppendFormat(_T("[0x%08x]"), status);
  }
  if (!info_string.IsEmpty()) {
    log_line.AppendFormat(_T("[%s]"), info_string);
  }
  NET_LOG(L3, (_T("%s"), log_line));
}

void SimpleRequest::CloseHandles(TransientRequestState* request_state) {
  if (request_state) {
    if (request_state->request_handle) {
      http_client_->Close(request_state->request_handle);
      request_state->request_handle = NULL;
    }
    if (request_state->connection_handle) {
      http_client_->Close(request_state->connection_handle);
      request_state->connection_handle = NULL;
    }
  }
}

}  // namespace omaha

