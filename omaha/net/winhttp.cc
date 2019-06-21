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
//
// Internet-access utility functions via winhttp

#include "omaha/net/winhttp.h"

#include <memory>
#include <vector>

#include "base/basictypes.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/string.h"
#include "omaha/base/synchronized.h"
#include "omaha/base/utils.h"
#include "omaha/net/winhttp_vtable.h"

namespace omaha {

class WinHttp : public HttpClient {
 public:
  static HttpClient* Create() { return new WinHttp; }

  virtual HRESULT Initialize();
  virtual HRESULT AddRequestHeaders(HINTERNET request,
                                    const TCHAR* headers,
                                    int length,
                                    uint32 modifiers);
  virtual HRESULT CheckPlatform();
  virtual HRESULT Close(HINTERNET handle);
  virtual HRESULT Connect(HINTERNET session_handle,
                          const TCHAR* server,
                          int port,
                          HINTERNET* connection_handle);
  virtual HRESULT CrackUrl(const TCHAR* url,
                           uint32 flags,
                           CString* scheme,
                           CString* server,
                           int* port,
                           CString* url_path,
                           CString* extra_info);
  virtual HRESULT CreateUrl(const TCHAR* scheme,
                            const TCHAR* server,
                            int port,
                            const TCHAR* url_path,
                            const TCHAR* extra_info,
                            uint32 flags,
                            CString* url);
  virtual HRESULT DetectAutoProxyConfigUrl(uint32 flags,
                                           CString* auto_config_url);
  virtual HRESULT GetDefaultProxyConfiguration(ProxyInfo* proxy_info);
  virtual HRESULT GetIEProxyConfiguration(
                      CurrentUserIEProxyConfig* proxy_info);
  virtual HRESULT GetProxyForUrl(HINTERNET session_handle,
                                 const TCHAR* url,
                                 const AutoProxyOptions* auto_proxy_options,
                                 ProxyInfo* proxy_info);
  virtual HRESULT Open(const TCHAR* user_agent,
                       uint32 access_type,
                       const TCHAR* proxy_name,
                       const TCHAR* proxy_bypass,
                       DWORD flags,
                       HINTERNET* session_handle);
  virtual HRESULT OpenRequest(HINTERNET connection_handle,
                              const TCHAR* verb,
                              const TCHAR* uri,
                              const TCHAR* version,
                              const TCHAR* referrer,
                              const TCHAR** accept_types,
                              uint32 flags,
                              HINTERNET* request_handle);
  virtual HRESULT QueryAuthSchemes(HINTERNET request_handle,
                                   uint32* supported_schemes,
                                   uint32* first_scheme,
                                   uint32* auth_target);
  virtual HRESULT QueryDataAvailable(HINTERNET request_handle,
                                     DWORD* num_bytes);
  virtual HRESULT QueryHeaders(HINTERNET request_handle,
                               uint32 info_level,
                               const TCHAR* name,
                               void* buffer,
                               DWORD* buffer_length,
                               DWORD* index);
  virtual HRESULT QueryOption(HINTERNET handle,
                              uint32 option,
                              void* buffer,
                              DWORD* buffer_length);
  virtual HRESULT ReadData(HINTERNET request_handle,
                           void* buffer,
                           DWORD buffer_length,
                           DWORD* bytes_read);
  virtual HRESULT ReceiveResponse(HINTERNET request_handle);
  virtual HRESULT SendRequest(HINTERNET request_handle,
                              const TCHAR* headers,
                              DWORD headers_length,
                              const void* optional_data,
                              DWORD optional_data_length,
                              DWORD content_length,
                              DWORD_PTR context);
  virtual HRESULT SetCredentials(HINTERNET request_handle,
                                 uint32 auth_targets,
                                 uint32 auth_scheme,
                                 const TCHAR* user_name,
                                 const TCHAR* password);
  virtual HRESULT SetDefaultProxyConfiguration(const ProxyInfo& proxy_info);
  virtual HRESULT SetOption(HINTERNET handle,
                            uint32 option,
                            const void* buffer,
                            DWORD buffer_length);
  virtual StatusCallback SetStatusCallback(HINTERNET handle,
                                           StatusCallback callback,
                                           uint32 flags);
  virtual HRESULT SetTimeouts(HINTERNET handle,
                              int resolve_timeout_ms,
                              int connect_timeout_ms,
                              int send_timeout_ms,
                              int receive_timeout_ms);
  virtual HRESULT WriteData(HINTERNET request_handle,
                            const void* buffer,
                            DWORD bytes_to_write,
                            DWORD* bytes_written);

 private:
  WinHttp();

  bool is_initialized_;

  static WinHttpVTable winhttp_;
  static LLock lock_;

  DISALLOW_COPY_AND_ASSIGN(WinHttp);
};

WinHttpVTable WinHttp::winhttp_;
LLock         WinHttp::lock_;

namespace internal {

HttpClient* WinHttpClientCreator() {
  return WinHttp::Create();
}

}  // namespace internal

// TODO(omaha): remove after the implementation is complete.
// 4100: unreferenced formal parameter
#pragma warning(disable : 4100)

WinHttp::WinHttp() : is_initialized_(false) {
}

HRESULT WinHttp::Initialize() {
  __mutexScope(WinHttp::lock_);
  if (is_initialized_) {
    return S_OK;
  }
  if (!winhttp_.Load()) {
    HRESULT hr = HRESULTFromLastError();
    NET_LOG(LEVEL_ERROR, (_T("[failed to load winhttp][0x%08x]"), hr));
    return hr;
  }
  is_initialized_ = true;
  return S_OK;
}

HRESULT WinHttp::Open(const TCHAR* user_agent,
                      uint32 access_type,
                      const TCHAR* proxy_name,
                      const TCHAR* proxy_bypass,
                      DWORD flags,
                      HINTERNET* session_handle) {
  *session_handle = winhttp_.WinHttpOpen(user_agent,
                                         access_type,
                                         proxy_name,
                                         proxy_bypass,
                                         flags);
  return *session_handle ? S_OK : HRESULTFromLastError();
}

HRESULT WinHttp::Close(HINTERNET handle) {
  ASSERT1(handle);
  return winhttp_.WinHttpCloseHandle(handle) ? S_OK : HRESULTFromLastError();
}

HRESULT WinHttp::Connect(HINTERNET session_handle,
                         const TCHAR* server,
                         int port,
                         HINTERNET* connection_handle) {
  ASSERT1(server);
  ASSERT1(port <= INTERNET_MAX_PORT_NUMBER_VALUE);

  // There has to be a valid session, otherwise it is possible that WinHTTP is
  // not properly initialized and the behavior of the WinHttpConnect is
  // undefined (it could crash in some cases).
  ASSERT1(session_handle);

  *connection_handle = winhttp_.WinHttpConnect(session_handle,
                                               server,
                                               static_cast<INTERNET_PORT>(port),
                                               0);
  return *connection_handle ? S_OK : HRESULTFromLastError();
}

HRESULT WinHttp::OpenRequest(HINTERNET connection_handle,
                             const TCHAR* verb,
                             const TCHAR* uri,
                             const TCHAR* version,
                             const TCHAR* referrer,
                             const TCHAR** accept_types,
                             uint32 flags,
                             HINTERNET* request_handle) {
  *request_handle = winhttp_.WinHttpOpenRequest(connection_handle,
                                                verb,
                                                uri,
                                                version,
                                                referrer,
                                                accept_types,
                                                flags);
  return *request_handle ? S_OK : HRESULTFromLastError();
}

HRESULT WinHttp::SendRequest(HINTERNET request_handle,
                             const TCHAR* headers,
                             DWORD headers_length,
                             const void* optional_data,
                             DWORD optional_data_length,
                             DWORD content_length,
                             DWORD_PTR context) {
  bool res = !!winhttp_.WinHttpSendRequest(
                            request_handle,
                            headers,
                            headers_length,
                            const_cast<void*>(optional_data),
                            optional_data_length,
                            content_length,
                            context);
  return res ? S_OK : HRESULTFromLastError();
}

HRESULT WinHttp::ReceiveResponse(HINTERNET request_handle) {
  bool res = !!winhttp_.WinHttpReceiveResponse(request_handle, NULL);
  return res ? S_OK : HRESULTFromLastError();
}

HRESULT WinHttp::QueryDataAvailable(HINTERNET request_handle,
                                    DWORD* num_bytes) {
  bool res = !!winhttp_.WinHttpQueryDataAvailable(request_handle, num_bytes);
  return res ? S_OK : HRESULTFromLastError();
}

HRESULT WinHttp::SetTimeouts(HINTERNET handle,
                             int resolve_timeout_ms,
                             int connect_timeout_ms,
                             int send_timeout_ms,
                             int receive_timeout_ms) {
  bool res = !!winhttp_.WinHttpSetTimeouts(handle,
                                           resolve_timeout_ms,
                                           connect_timeout_ms,
                                           send_timeout_ms,
                                           receive_timeout_ms);
  return res ? S_OK : HRESULTFromLastError();
}

HRESULT WinHttp::ReadData(HINTERNET request_handle,
                          void* buffer,
                          DWORD buffer_length,
                          DWORD* bytes_read) {
  ASSERT1(buffer);

  bool res = !!winhttp_.WinHttpReadData(request_handle,
                                        buffer,
                                        buffer_length,
                                        bytes_read);
  return res ? S_OK : HRESULTFromLastError();
}

HRESULT WinHttp::WriteData(HINTERNET request_handle,
                           const void* buffer,
                           DWORD bytes_to_write,
                           DWORD* bytes_written) {
  ASSERT1(buffer);

  bool res = !!winhttp_.WinHttpWriteData(request_handle,
                                         buffer,
                                         bytes_to_write,
                                         bytes_written);
  return res ? S_OK : HRESULTFromLastError();
}

HRESULT WinHttp::SetCredentials(HINTERNET request_handle,
                                uint32 auth_targets,
                                uint32 auth_scheme,
                                const TCHAR* user_name,
                                const TCHAR* password) {
  bool res = !!winhttp_.WinHttpSetCredentials(request_handle,
                                              auth_targets,
                                              auth_scheme,
                                              user_name,
                                              password,
                                              NULL);
  return res ? S_OK : HRESULTFromLastError();
}

HRESULT WinHttp::DetectAutoProxyConfigUrl(uint32 flags,
                                          CString* auto_config_url) {
  ASSERT1(auto_config_url);
  TCHAR* url = NULL;
  bool res = !!winhttp_.WinHttpDetectAutoProxyConfigUrl(flags, &url);
  *auto_config_url = url;
  HRESULT hr = res ? S_OK : HRESULTFromLastError();
  if (url) {
    VERIFY1(!::GlobalFree(url));
  }
  return hr;
}

HRESULT WinHttp::GetDefaultProxyConfiguration(ProxyInfo* proxy_info) {
  ASSERT1(proxy_info);

  WINHTTP_PROXY_INFO pi = {0};
  bool res = !!winhttp_.WinHttpGetDefaultProxyConfiguration(&pi);

  proxy_info->access_type  = pi.dwAccessType;
  proxy_info->proxy        = pi.lpszProxy;
  proxy_info->proxy_bypass = pi.lpszProxyBypass;

  return res ? S_OK : HRESULTFromLastError();
}

HRESULT WinHttp::SetDefaultProxyConfiguration(const ProxyInfo& proxy_info) {
  WINHTTP_PROXY_INFO pi = {0};
  pi.dwAccessType    = proxy_info.access_type;
  pi.lpszProxy       = const_cast<TCHAR*>(proxy_info.proxy);
  pi.lpszProxyBypass = const_cast<TCHAR*>(proxy_info.proxy_bypass);

  bool res = !!winhttp_.WinHttpSetDefaultProxyConfiguration(&pi);
  return res ? S_OK : HRESULTFromLastError();
}

HRESULT WinHttp::GetIEProxyConfiguration(CurrentUserIEProxyConfig* proxy_info) {
  ASSERT1(proxy_info);

  WINHTTP_CURRENT_USER_IE_PROXY_CONFIG pi = {0};
  bool res = !!winhttp_.WinHttpGetIEProxyConfigForCurrentUser(&pi);

  proxy_info->auto_detect     = !!pi.fAutoDetect;
  proxy_info->auto_config_url = pi.lpszAutoConfigUrl;
  proxy_info->proxy           = pi.lpszProxy;
  proxy_info->proxy_bypass    = pi.lpszProxyBypass;

  return res ? S_OK : HRESULTFromLastError();
}

HRESULT WinHttp::GetProxyForUrl(HINTERNET session_handle,
                                const TCHAR* url,
                                const AutoProxyOptions* auto_proxy_options,
                                ProxyInfo* proxy_info) {
  ASSERT1(auto_proxy_options);
  ASSERT1(proxy_info);

  WINHTTP_AUTOPROXY_OPTIONS apo = {0};
  apo.dwFlags                = auto_proxy_options->flags;
  apo.dwAutoDetectFlags      = auto_proxy_options->auto_detect_flags;
  apo.lpszAutoConfigUrl      = auto_proxy_options->auto_config_url;
  apo.lpvReserved            = NULL;
  apo.dwReserved             = 0;
  apo.fAutoLogonIfChallenged = auto_proxy_options->auto_logon_if_challenged;

  WINHTTP_PROXY_INFO pi = {0};

  ASSERT1(session_handle);
  bool res = !!winhttp_.WinHttpGetProxyForUrl(session_handle, url, &apo, &pi);

  proxy_info->access_type  = pi.dwAccessType;
  proxy_info->proxy        = pi.lpszProxy;
  proxy_info->proxy_bypass = pi.lpszProxyBypass;

  return res ? S_OK : HRESULTFromLastError();
}

HRESULT WinHttp::QueryAuthSchemes(HINTERNET request_handle,
                                  uint32* supported_schemes,
                                  uint32* first_scheme,
                                  uint32* auth_target) {
  ASSERT1(supported_schemes);
  ASSERT1(first_scheme);
  ASSERT1(auth_target);

  DWORD ss = 0;
  DWORD fs = 0;
  DWORD at = 0;

  ASSERT1(request_handle);
  bool res = !!winhttp_.WinHttpQueryAuthSchemes(request_handle, &ss, &fs, &at);

  *supported_schemes = ss;
  *first_scheme      = fs;
  *auth_target       = at;

  return res ? S_OK : HRESULTFromLastError();
}

HRESULT WinHttp::AddRequestHeaders(HINTERNET request_handle,
                                   const TCHAR* headers,
                                   int length,
                                   uint32 modifiers) {
  ASSERT1(headers);
  bool res = !!winhttp_.WinHttpAddRequestHeaders(request_handle,
                                                 headers,
                                                 length,
                                                 modifiers);
  return res ? S_OK : HRESULTFromLastError();
}

HRESULT WinHttp::CrackUrl(const TCHAR* url,
                          uint32 flags,
                          CString* scheme,
                          CString* server,
                          int* port,
                          CString* url_path,
                          CString* extra_info) {
  ASSERT1(url);
  DWORD url_length = static_cast<DWORD>(_tcslen(url));
  URL_COMPONENTS url_comp = {0};
  url_comp.dwStructSize   = sizeof(url_comp);
  if (scheme) {
    url_comp.lpszScheme     = scheme->GetBuffer(INTERNET_MAX_SCHEME_LENGTH);
    url_comp.dwSchemeLength = INTERNET_MAX_SCHEME_LENGTH;
  }
  if (server) {
    url_comp.lpszHostName    = server->GetBuffer(INTERNET_MAX_HOST_NAME_LENGTH);
    url_comp.dwHostNameLength = INTERNET_MAX_HOST_NAME_LENGTH;
  }
  if (url_path) {
    url_comp.lpszUrlPath     = url_path->GetBuffer(INTERNET_MAX_PATH_LENGTH);
    url_comp.dwUrlPathLength = INTERNET_MAX_PATH_LENGTH;
  }
  if (extra_info) {
    // There is no constant for the extra info max length.
    url_comp.lpszExtraInfo    = extra_info->GetBuffer(INTERNET_MAX_PATH_LENGTH);
    url_comp.dwExtraInfoLength = INTERNET_MAX_PATH_LENGTH;
  }
  bool res = !!winhttp_.WinHttpCrackUrl(url, url_length, flags, &url_comp);
  if (scheme) {
    scheme->ReleaseBuffer(url_comp.dwSchemeLength);
  }
  if (server) {
    server->ReleaseBuffer(url_comp.dwHostNameLength);
  }
  if (port) {
    *port = url_comp.nPort;
  }
  if (url_path) {
    url_path->ReleaseBuffer(url_comp.dwUrlPathLength);
  }
  if (extra_info) {
    extra_info->ReleaseBuffer(url_comp.dwExtraInfoLength);
  }
  return res ? S_OK : HRESULTFromLastError();
}

HRESULT WinHttp::CreateUrl(const TCHAR* scheme,
                           const TCHAR* server,
                           int port,
                           const TCHAR* url_path,
                           const TCHAR* extra_info,
                           uint32 flags,
                           CString* url) {
  ASSERT1(url);
  ASSERT1(port <= INTERNET_MAX_PORT_NUMBER_VALUE);

  URL_COMPONENTS url_comp = {0};
  url_comp.dwStructSize   = sizeof(url_comp);
  if (scheme) {
    url_comp.lpszScheme        = const_cast<TCHAR*>(scheme);
    url_comp.dwSchemeLength    = static_cast<DWORD>(_tcslen(scheme));
  }
  if (server) {
    url_comp.lpszHostName      = const_cast<TCHAR*>(server);
    url_comp.dwHostNameLength  = static_cast<DWORD>(_tcslen(server));
  }
  if (port) {
    url_comp.nPort             = static_cast<INTERNET_PORT>(port);
  }
  if (url_path) {
    url_comp.lpszUrlPath       = const_cast<TCHAR*>(url_path);
    url_comp.dwUrlPathLength   = static_cast<DWORD>(_tcslen(url_path));
  }
  if (extra_info) {
    url_comp.lpszExtraInfo     = const_cast<TCHAR*>(extra_info);
    url_comp.dwExtraInfoLength = static_cast<DWORD>(_tcslen(extra_info));
  }

  DWORD url_length = INTERNET_MAX_URL_LENGTH;
  bool res = !!winhttp_.WinHttpCreateUrl(
                            &url_comp,
                            flags,
                            url->GetBuffer(url_length),
                            &url_length);
  if (!res) {
    return HRESULTFromLastError();
  }
  ASSERT1(url_length);
  url->ReleaseBuffer(url_length);
  return S_OK;
}

HttpClient::StatusCallback WinHttp::SetStatusCallback(HINTERNET handle,
                                                      StatusCallback callback,
                                                      uint32 flags) {
  WINHTTP_STATUS_CALLBACK winhttp_status_callback =
      winhttp_.WinHttpSetStatusCallback(
          handle,
          reinterpret_cast<WINHTTP_STATUS_CALLBACK>(callback),
          flags,
          NULL);
  return reinterpret_cast<HttpClient::StatusCallback>(winhttp_status_callback);
}

HRESULT WinHttp::CheckPlatform() {
  return !!winhttp_.WinHttpCheckPlatform() ? S_OK : HRESULTFromLastError();
}

HRESULT WinHttp::QueryHeaders(HINTERNET request_handle,
                              uint32 info_level,
                              const TCHAR* name,
                              void* buffer,
                              DWORD* buffer_length,
                              DWORD* index) {
  ASSERT1(buffer_length);
  bool res = !!winhttp_.WinHttpQueryHeaders(request_handle,
                                            info_level,
                                            name,
                                            buffer,
                                            buffer_length,
                                            index);
  return res ? S_OK : HRESULTFromLastError();
}

HRESULT WinHttp::QueryOption(HINTERNET handle,
                             uint32 option,
                             void* buffer,
                             DWORD* buffer_length) {
  ASSERT1(buffer_length);
  bool res = !!winhttp_.WinHttpQueryOption(handle, option,
                                           buffer, buffer_length);
  return res ? S_OK : HRESULTFromLastError();
}

HRESULT WinHttp::SetOption(HINTERNET handle,
                           uint32 option,
                           const void* buffer,
                           DWORD buffer_length) {
  ASSERT1(buffer);
  ASSERT1(buffer_length);
  bool res = !!winhttp_.WinHttpSetOption(handle,
                                         option,
                                         const_cast<void*>(buffer),
                                         buffer_length);
  return res ? S_OK : HRESULTFromLastError();
}

}  // namespace omaha

