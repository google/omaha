// Copyright 2004-2009 Google Inc.
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

#include <windows.h>
#include <wininet.h>
#include <atlstr.h>
#include <vector>                     // NOLINT

#include "omaha/net/http_client.h"

#include "base/scoped_ptr.h"
#include "omaha/common/atl_regexp.h"
#include "omaha/common/debug.h"
#include "omaha/common/logging.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/string.h"
#include "omaha/common/synchronized.h"
#include "omaha/common/utils.h"

namespace omaha {

class WinInet : public HttpClient {
 public:
  static HttpClient* Create() { return new WinInet; }

  virtual HRESULT Initialize();

  virtual HRESULT Open(const TCHAR* user_agent,
                         uint32 access_type,
                         const TCHAR* proxy_name,
                         const TCHAR* proxy_bypass);

  virtual HRESULT Close();

  virtual HRESULT Connect(const TCHAR* server, int port);

  virtual HRESULT OpenRequest(const TCHAR* verb,
                              const TCHAR* uri,
                              const TCHAR* version,
                              const TCHAR* referrer,
                              const TCHAR** accept_types,
                              uint32 flags);

  virtual HRESULT SendRequest(const TCHAR* headers,
                              int headers_length,
                              const void* optional_data,
                              size_t optional_data_length,
                              size_t content_length);

  virtual HRESULT SetTimeouts(int resolve_timeout_ms,
                              int connect_timeout_ms,
                              int send_timeout_ms,
                              int receive_timeout_ms);

  virtual HRESULT ReceiveResponse();

  virtual HRESULT QueryDataAvailable(int* num_bytes);

  virtual HRESULT ReadData(std::vector<uint8>* data);

  virtual HRESULT WriteData(const std::vector<uint8>& data, int* bytes_written);

  virtual HRESULT SetCredentials(uint32 auth_targets,
                                 uint32 auth_scheme,
                                 const TCHAR* user_name,
                                 const TCHAR* password);

  virtual HRESULT DetectAutoProxyConfigUrl(uint32 flags,
                                           CString* auto_config_url);

  virtual HRESULT GetDefaultProxyConfiguration(ProxyInfo* proxy_info);

  virtual HRESULT SetDefaultProxyConfiguration(const ProxyInfo& proxy_info);

  virtual HRESULT GetIEProxyConfiguration(CurrentUserIEProxyConfig* proxy_info);

  virtual HRESULT GetProxyForUrl(const TCHAR* url,
                                 const AutoProxyOptions* auto_proxy_options,
                                 ProxyInfo* pProxyInfo);

  virtual HRESULT QueryAuthSchemes(uint32* supported_schemes,
                                          uint32* first_scheme,
                                          uint32* auth_target);

  virtual HRESULT AddRequestHeaders(const TCHAR* headers,
                                    int length,
                                    uint32 modifiers);

  virtual HRESULT QueryHeaders(uint32 info_level,
                               const TCHAR* name,
                               CString* value,
                               int* index);
  virtual HRESULT QueryHeaders(uint32 info_level,
                               const TCHAR* name,
                               int* value,
                               int* index);

  virtual HRESULT QueryOption(bool is_session_option,
                              uint32 option,
                              std::vector<uint8>* val);

  virtual HRESULT QueryOptionString(bool is_session_option,
                                    uint32 option,
                                    CString* value);

  virtual HRESULT QueryOptionInt(bool is_session_option,
                                 uint32 option,
                                 int* value);

  virtual HRESULT SetOption(bool is_session_option,
                            uint32 option,
                            const void* buffer,
                            size_t buffer_length);

  virtual HRESULT SetOption(bool is_session_option,
                            uint32 option,
                            const std::vector<uint8>* val);

  virtual HRESULT SetOptionString(bool is_session_option,
                                  uint32 option,
                                  const TCHAR* value);

  virtual HRESULT SetOptionInt(bool is_session_option,
                               uint32 option,
                               int value);

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


  typedef void (*StatusCallback)(uint32 context,
                                 int status,
                                 void* status_information,
                                 size_t status_info_length);
  virtual StatusCallback SetStatusCallback(StatusCallback callback,
                                           uint32 flags);
 private:
  WinInet();
  DISALLOW_EVIL_CONSTRUCTORS(WinInet);
};


// TODO(omaha): remove after the implementation is complete.
// 4100: unreferenced formal parameter
#pragma warning(disable : 4100)

WinInet::WinInet() {
}

HRESULT WinInet::Initialize() {
  return E_NOTIMPL;
}

HRESULT WinInet::Open(const TCHAR* user_agent,
                      uint32 access_type,
                      const TCHAR* proxy_name,
                      const TCHAR* proxy_bypass) {
  return E_NOTIMPL;
}

HRESULT WinInet::Close() {
  return E_NOTIMPL;
}

HRESULT WinInet::Connect(const TCHAR* server, int port) {
  return E_NOTIMPL;
}

HRESULT WinInet::OpenRequest(const TCHAR* verb,
                             const TCHAR* uri,
                             const TCHAR* version,
                             const TCHAR* referrer,
                             const TCHAR** accept_types,
                             uint32 flags) {
  return E_NOTIMPL;
}

HRESULT WinInet::SendRequest(const TCHAR* headers,
                             int headers_length,
                             const void* optional_data,
                             size_t optional_data_length,
                             size_t content_length) {
  return E_NOTIMPL;
}

HRESULT WinInet::SetTimeouts(int resolve_timeout_ms,
                             int connect_timeout_ms,
                             int send_timeout_ms,
                             int receive_timeout_ms) {
  return E_NOTIMPL;
}

HRESULT WinInet::ReceiveResponse() {
  return E_NOTIMPL;
}

HRESULT WinInet::QueryDataAvailable(int* num_bytes) {
  return E_NOTIMPL;
}

HRESULT WinInet::ReadData(std::vector<uint8>* data) {
  return E_NOTIMPL;
}

HRESULT WinInet::WriteData(const std::vector<uint8>& data, int* bytes_written) {
  return E_NOTIMPL;
}

HRESULT WinInet::SetCredentials(uint32 auth_targets,
                                uint32 auth_scheme,
                                const TCHAR* user_name,
                                const TCHAR* password) {
  return E_NOTIMPL;
}

HRESULT WinInet::DetectAutoProxyConfigUrl(uint32 flags,
                                          CString* auto_config_url) {
  return E_NOTIMPL;
}

HRESULT WinInet::GetDefaultProxyConfiguration(ProxyInfo* proxy_info) {
  return E_NOTIMPL;
}

HRESULT WinInet::SetDefaultProxyConfiguration(const ProxyInfo& proxy_info) {
  return E_NOTIMPL;
}

HRESULT WinInet::GetIEProxyConfiguration(CurrentUserIEProxyConfig* proxy_info) {
  return E_NOTIMPL;
}

HRESULT WinInet::GetProxyForUrl(const TCHAR* url,
                                const AutoProxyOptions* auto_proxy_options,
                                ProxyInfo* pProxyInfo) {
  return E_NOTIMPL;
}

HRESULT WinInet::QueryAuthSchemes(uint32* supported_schemes,
                                  uint32* first_scheme,
                                  uint32* auth_target) {
  return E_NOTIMPL;
}

HRESULT WinInet::AddRequestHeaders(const TCHAR* headers,
                                   int length,
                                   uint32 modifiers) {
  return E_NOTIMPL;
}

HRESULT WinInet::QueryHeaders(uint32 info_level,
                              const TCHAR* name,
                              CString* value,
                              int* index) {
  return E_NOTIMPL;
}

HRESULT WinInet::QueryHeaders(uint32 info_level,
                              const TCHAR* name,
                              int* value,
                              int* index) {
  return E_NOTIMPL;
}


HRESULT WinInet::QueryOption(bool is_session_option,
                             uint32 option,
                             std::vector<uint8>* val) {
  return E_NOTIMPL;
}

HRESULT WinInet::QueryOptionString(bool is_session_option,
                                   uint32 option,
                                   CString* value) {
  return E_NOTIMPL;
}

HRESULT WinInet::QueryOptionInt(bool is_session_option,
                                uint32 option,
                                int* value) {
  return E_NOTIMPL;
}

HRESULT WinInet::SetOption(bool is_session_option,
                           uint32 option,
                           const void* buffer,
                           size_t buffer_length) {
  return E_NOTIMPL;
}

HRESULT WinInet::SetOption(bool is_session_option,
                           uint32 option,
                           const std::vector<uint8>* val) {
  return E_NOTIMPL;
}

HRESULT WinInet::SetOptionString(bool is_session_option,
                                 uint32 option,
                                 const TCHAR* value) {
  return E_NOTIMPL;
}

HRESULT WinInet::SetOptionInt(bool is_session_option,
                              uint32 option,
                              int value) {
  return E_NOTIMPL;
}

HRESULT WinInet::CrackUrl(const TCHAR* url,
                          uint32 flags,
                          CString* scheme,
                          CString* server,
                          int* port,
                          CString* url_path,
                          CString* extra_info) {
  return E_NOTIMPL;
}

HRESULT WinInet::CreateUrl(const TCHAR* scheme,
                           const TCHAR* server,
                           int port,
                           const TCHAR* url_path,
                           const TCHAR* extra_info,
                           uint32 flags,
                           CString* url) {
  return E_NOTIMPL;
}

HttpClient::StatusCallback WinInet::SetStatusCallback(
                               HttpClient::StatusCallback callback,
                               uint32 flags) {
  return NULL;
}

extern "C" const bool kRegisterWinInet =
    HttpClient::GetFactory().Register(HttpClient::WININET, &WinInet::Create);


}  // namespace omaha

#if 0
// Constants
const uint32 kInternetRequestCachingDefaultFlags = INTERNET_FLAG_NO_UI |
                                                   INTERNET_FLAG_NO_COOKIES;
const uint32 kInternetRequestNoCacheDefaultFlags =
                 INTERNET_FLAG_NO_UI |
                 INTERNET_FLAG_NO_COOKIES |
                 INTERNET_FLAG_NO_CACHE_WRITE |
                 INTERNET_FLAG_PRAGMA_NOCACHE |
                 INTERNET_FLAG_RELOAD;
const TCHAR kProxyAuthenticateHeader[] = _T("Proxy-Authenticate:");
const TCHAR kNegotiateAuthScheme[] = _T("Negotiate");
const TCHAR kNTLMAuthScheme[] = _T("NTLM");
const TCHAR kDigestAuthScheme[] = _T("Digest");
const TCHAR kBasicAuthScheme[] = _T("Basic");

// Provides dynamic loading of WinInet.dll
//
// Delay loading wininet.dll would be nice but would (tediously) involve
// modifying the mk_file for all DLLs/EXEs that use any of the functions that
// use InetGet.  Which is all of them (since it is used in debug.cpp).  So
// instead, we do it the old-fashioned way.
//
// N.B.: When WinInetVTable has been Load()ed, the wininet.dll is loaded -
// which adds significantly to the working set of your process.  So beware of
// loading the WinInetVTable and leaving it loaded, esp. if you have a static
// instance of WinInetVTable.  And on the other hand, the load/unload operation
// is expensive (slow) so you want to reuse your Load()ed WinInetVTable if
// you're going to be needing it frequently.  Note that that cost is in the
// loading/unloading of the DLL, thus 'nested' uses (which only need to 'snap'
// the links) are cheap.
class WinInetVTable {
 public:
  // Functions are simply reflected through data members holding function
  // pointers, taking advantage of C-syntax (you can call indirectly through a
  // function pointer without a '*').  No error checking is done: call through
  // only when Loaded() == true.
  BOOL      HttpAddRequestHeaders(HINTERNET, const TCHAR*, DWORD, DWORD);
  BOOL      HttpEndRequest(HINTERNET, LPINTERNET_BUFFERS, DWORD, DWORD_PTR);
  HINTERNET HttpOpenRequest(HINTERNET,
                            const TCHAR*,
                            const TCHAR*,
                            const TCHAR*,
                            const TCHAR*,
                            const TCHAR**,
                            DWORD,
                            DWORD_PTR);
  BOOL      HttpQueryInfo(HINTERNET, DWORD, LPVOID, LPDWORD, LPDWORD);
  BOOL      HttpSendRequestEx(HINTERNET,
                              LPINTERNET_BUFFERS,
                              LPINTERNET_BUFFERS,
                              DWORD,
                              DWORD_PTR);
  BOOL      HttpSendRequest(HINTERNET, const TCHAR*, DWORD, LPVOID, DWORD);
  BOOL      InternetCloseHandle(HINTERNET);
  HINTERNET InternetConnect(HINTERNET,
                            const TCHAR*,
                            INTERNET_PORT,
                            const TCHAR*,
                            const TCHAR*,
                            DWORD,
                            DWORD,
                            DWORD);
  BOOL      InternetGetConnectedStateEx(LPDWORD, char*, DWORD, DWORD);
  HINTERNET InternetOpen(const TCHAR*,
                         DWORD,
                         const TCHAR*,
                         const TCHAR*,
                         DWORD);
  HINTERNET InternetOpenUrl(HINTERNET,
                            const TCHAR*,
                            const TCHAR*,
                            DWORD,
                            DWORD,
                            DWORD_PTR);
  BOOL      InternetQueryDataAvailable(HINTERNET, LPDWORD, DWORD, DWORD);
  BOOL      InternetReadFile(HINTERNET, LPVOID, DWORD, LPDWORD);
  HINTERNET InternetReadFileEx(HINTERNET, LPINTERNET_BUFFERS, DWORD, DWORD_PTR);
  BOOL      InternetGetCookie(const TCHAR*, const TCHAR*, TCHAR*, LPDWORD);
  BOOL      InternetSetCookie(const TCHAR*, const TCHAR*, const TCHAR*);
  BOOL      InternetAutodial(DWORD, HWND);
  BOOL      InternetAutodialHangup(DWORD);
  BOOL      InternetQueryOption(HINTERNET hInternet,
                                DWORD dwOption,
                                LPVOID lpBuffer,
                                LPDWORD lpdwBufferLength);
  BOOL      InternetSetOption(HINTERNET hInternet,
                              DWORD dwOption,
                              LPVOID lpBuffer,
                              DWORD dwBufferLength);
  BOOL      InternetCheckConnection(const TCHAR*,
                                    DWORD, DWORD);
  BOOL      InternetCrackUrl(const TCHAR*,
                             DWORD,
                             DWORD,
                             LPURL_COMPONENTS);
  INTERNET_STATUS_CALLBACK InternetSetStatusCallback(HINTERNET, INTERNET_STATUS_CALLBACK);

  WinInetVTable();
  ~WinInetVTable();

  bool Load();    // Load library, snap links
  void Unload();  // Unload library, clear links

  bool Loaded() { return NULL != library_; }

 protected:
  BOOL      (CALLBACK *HttpAddRequestHeaders_pointer)(HINTERNET, const TCHAR*, DWORD, DWORD);
  BOOL      (CALLBACK *HttpEndRequest_pointer)(HINTERNET, LPINTERNET_BUFFERS, DWORD, DWORD_PTR);
  HINTERNET (CALLBACK *HttpOpenRequest_pointer)(HINTERNET, const TCHAR*, const TCHAR*, const TCHAR*, const TCHAR*, const TCHAR**, DWORD, DWORD_PTR);
  BOOL      (CALLBACK *HttpQueryInfo_pointer)(HINTERNET, DWORD, LPVOID, LPDWORD, LPDWORD);
  BOOL      (CALLBACK *HttpSendRequestEx_pointer)(HINTERNET, LPINTERNET_BUFFERS, LPINTERNET_BUFFERS, DWORD, DWORD_PTR);
  BOOL      (CALLBACK *HttpSendRequest_pointer)(HINTERNET, const TCHAR*, DWORD, LPVOID, DWORD);
  BOOL      (CALLBACK *InternetCloseHandle_pointer)(HINTERNET);
  HINTERNET (CALLBACK *InternetConnect_pointer)(HINTERNET, const TCHAR*, INTERNET_PORT, const TCHAR*, const TCHAR*, DWORD, DWORD, DWORD);
  BOOL      (CALLBACK *InternetGetConnectedStateEx_pointer)(LPDWORD, char*, DWORD, DWORD);
  HINTERNET (CALLBACK *InternetOpen_pointer)(const TCHAR*, DWORD, const TCHAR*, const TCHAR*, DWORD);
  HINTERNET (CALLBACK *InternetOpenUrl_pointer)(HINTERNET, const TCHAR*, const TCHAR*, DWORD, DWORD, DWORD_PTR);
  BOOL      (CALLBACK *InternetQueryDataAvailable_pointer)(HINTERNET, LPDWORD, DWORD, DWORD);
  BOOL      (CALLBACK *InternetReadFile_pointer)(HINTERNET, LPVOID, DWORD, LPDWORD);
  HINTERNET (CALLBACK *InternetReadFileEx_pointer)(HINTERNET, LPINTERNET_BUFFERS, DWORD, DWORD_PTR);
  BOOL      (CALLBACK *InternetGetCookie_pointer)(const TCHAR*, const TCHAR*, TCHAR*, LPDWORD);
  BOOL      (CALLBACK *InternetSetCookie_pointer)(const TCHAR*, const TCHAR*, const TCHAR*);
  BOOL      (CALLBACK *InternetAutodial_pointer)(DWORD, HWND);
  BOOL      (CALLBACK *InternetAutodialHangup_pointer)(DWORD);
  BOOL      (CALLBACK *InternetQueryOption_pointer)(HINTERNET hInternet, DWORD dwOption, LPVOID lpBuffer, LPDWORD lpdwBufferLength);
  BOOL      (CALLBACK *InternetSetOption_pointer)(HINTERNET hInternet, DWORD dwOption, LPVOID lpBuffer, DWORD dwBufferLength);
  BOOL      (CALLBACK *InternetCheckConnection_pointer)(const TCHAR*, DWORD, DWORD);
  BOOL      (CALLBACK *InternetCrackUrl_pointer)(const TCHAR*, DWORD, DWORD, LPURL_COMPONENTS);
  INTERNET_STATUS_CALLBACK (CALLBACK *InternetSetStatusCallback_pointer)(HINTERNET, INTERNET_STATUS_CALLBACK);

  HINSTANCE library_;

  void Clear();

  // Simple wrapper around GetProcAddress()
  template <typename T>
  bool GPA(const char* function_name, T& function_pointer);   // NO LINT

 private:
  DISALLOW_EVIL_CONSTRUCTORS(WinInetVTable);
};


class InetHttpClient : public HttpClient {
 public:
  static InetHttpClient* Create();
  virtual ~InetHttpClient();
  virtual HRESULT Abort();
  virtual uint32 GetStatus();
  virtual HRESULT GetResponseHeader(const TCHAR* name, CString* value);
  virtual HRESULT GetAllResponseHeaders(CString* response_headers);
  virtual HRESULT GetResponseText(CString* response_text, uint32 max_size);
  virtual HRESULT GetRawResponseBody(std::vector<byte>* response_body,
                                     uint32 max_size);
  virtual HRESULT SaveRawResponseBody(const TCHAR* response_filename,
                                      uint32 max_size);
  virtual HRESULT QueryProxyAuthScheme(AuthScheme* auth_scheme);
  virtual HRESULT GetAutoProxyForUrl(const TCHAR* url, CString* proxy);
  virtual HRESULT GetDefaultProxyConfiguration(AccessType* access_type,
                                               CString* proxy,
                                               CString* proxy_bypass);
 protected:
  virtual HRESULT InternalCrackUrl(const CString& url,
                                   bool to_escape,
                                   URL_COMPONENTS* url_components);
  virtual HRESULT SendRequest(const TCHAR* server,
                              uint32 port,
                              const TCHAR* path);
  virtual bool CheckConnection();
 private:
  InetHttpClient();
  HRESULT Init();
  void Close();
  HRESULT SetTimeouts();
  HRESULT SetHttpProxyCredentials(const TCHAR* username, const TCHAR* password);
  HRESULT SkipSSLCertErrorIfNeeded();
  HRESULT InternalSend(const TCHAR* server,
                       uint32 port,
                       const TCHAR* path,
                       bool use_proxy);
  HRESULT InternalGetRawResponseBody(std::vector<byte>* response_body,
                                     const TCHAR* response_filename,
                                     uint32 max_size);
  static void CALLBACK StatusCallback(HINTERNET handle,
                                      DWORD_PTR context,
                                      DWORD status,
                                      void* info,
                                      DWORD info_len);

  WinInetVTable wininet_;
  HINTERNET session_handle_;
  HINTERNET connection_handle_;
  HINTERNET request_handle_;
  uint32 status_;
  bool is_initialized_;
  bool is_aborted_;
  LLock lock_;

  // Parses the header "Proxy-Authenticate:"
  static AtlRE proxy_auth_header_regex_;

  DISALLOW_EVIL_CONSTRUCTORS(InetHttpClient);
};

AtlRE InetHttpClient::proxy_auth_header_regex_(
    _T("\\n*Proxy\\-Authenticate\\:\\b*{\\w}\\n*"), false);

HttpClient* CreateInetHttpClient() {
  return InetHttpClient::Create();
}

InetHttpClient* InetHttpClient::Create() {
  scoped_ptr<InetHttpClient> http_client(new InetHttpClient());
  if (http_client.get()) {
    if (SUCCEEDED(http_client->Init())) {
      return http_client.release();
    }
  }
  return NULL;
}

InetHttpClient::InetHttpClient()
    : session_handle_(NULL),
      connection_handle_(NULL),
      request_handle_(NULL),
      status_(0),
      is_initialized_(false),
      is_aborted_(false) {
}

InetHttpClient::~InetHttpClient() {
  Close();
  wininet_.Unload();
}

HRESULT InetHttpClient::Abort() {
  UTIL_LOG(L6, (_T("[InetHttpClient::Abort]")));

  is_aborted_ = true;
  Close();
  return S_OK;
}

void InetHttpClient::Close() {
  // Closing the handles will unblock all WinInet calls unless a call is
  // waiting for DNS lookup, or in other words, when the state is where
  // between INTERNET_STATUS_RESOLVING_NAME and INTERNET_STATUS_NAME_RESOLVED.
  // In that case, closing the handles does not have any effect.
  // This is also the case even we use async WinInet calls.

  __mutexScope(lock_);

  if (request_handle_) {
    wininet_.InternetCloseHandle(request_handle_);
    request_handle_ = NULL;
  }

  if (connection_handle_) {
    wininet_.InternetCloseHandle(connection_handle_);
    connection_handle_ = NULL;
  }

  if (session_handle_) {
    wininet_.InternetCloseHandle(session_handle_);
    session_handle_ = NULL;
  }
}

HRESULT InetHttpClient::Init() {
  if (is_initialized_) {
    return E_UNEXPECTED;
  }

  is_initialized_ = true;

  if (!wininet_.Load()) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LEVEL_ERROR, (_T("[Failed to load wininet][0x%08x]"), hr));
    return hr;
  }

  return S_OK;
}

HRESULT InetHttpClient::SetTimeouts() {
  ASSERT1(session_handle_);

  uint32 connect_timeout_ms = connect_timeout_ms_;
  uint32 send_timeout_ms = send_timeout_ms_;
  uint32 receive_timeout_ms = receive_timeout_ms_;
#if DEBUG
  ConfigProfile::GetValue(kServerSettings,
                          kConfigConnectTimeoutMs,
                          &connect_timeout_ms);
  ConfigProfile::GetValue(kServerSettings,
                          kConfigSendTimeoutMs,
                          &send_timeout_ms);
  ConfigProfile::GetValue(kServerSettings,
                          kConfigReceiveTimeoutMs,
                          &receive_timeout_ms);
#endif
  if (connect_timeout_ms || send_timeout_ms || receive_timeout_ms) {
    if (!wininet_.InternetSetOption(session_handle_,
                                    INTERNET_OPTION_CONNECT_TIMEOUT,
                                    &connect_timeout_ms,
                                    sizeof(connect_timeout_ms))) {
      UTIL_LOG(LEVEL_ERROR, (_T("[Failed to set connection timeout]")
                             _T("[0x%08x]"), HRESULTFromLastError()));
    }
    if (!wininet_.InternetSetOption(session_handle_,
                                    INTERNET_OPTION_SEND_TIMEOUT,
                                    &send_timeout_ms,
                                    sizeof(send_timeout_ms))) {
      UTIL_LOG(LEVEL_ERROR, (_T("[Failed to set send timeout]")
                             _T("[0x%08x]"), HRESULTFromLastError()));
    }
    if (!wininet_.InternetSetOption(session_handle_,
                                    INTERNET_OPTION_RECEIVE_TIMEOUT,
                                    &receive_timeout_ms,
                                    sizeof(receive_timeout_ms))) {
      UTIL_LOG(LEVEL_ERROR, (_T("[Failed to set receive timeout]")
                             _T("[0x%08x]"), HRESULTFromLastError()));
    }
  }
  return S_OK;
}

HRESULT InetHttpClient::SetHttpProxyCredentials(const TCHAR* username,
                                                const TCHAR* password) {
  ASSERT1(connection_handle_);

  if (username && *username && password && *password) {
    if (!wininet_.InternetSetOption(connection_handle_,
                                    INTERNET_OPTION_PROXY_USERNAME,
                                    const_cast<TCHAR*>(username),
                                    _tcslen(username))) {
      UTIL_LOG(LEVEL_ERROR, (_T("[Failed to set proxy authentication username]")
                             _T("[0x%08x]"), HRESULTFromLastError()));
    }
    if (!wininet_.InternetSetOption(connection_handle_,
                                    INTERNET_OPTION_PROXY_PASSWORD,
                                    const_cast<TCHAR*>(password),
                                    _tcslen(password))) {
      UTIL_LOG(LEVEL_ERROR, (_T("[Failed to set proxy authentication password]")
                             _T("[0x%08x]"), HRESULTFromLastError()));
    }
  }
  return S_OK;
}

HRESULT InetHttpClient::SkipSSLCertErrorIfNeeded() {
#if DEBUG
  ASSERT1(request_handle_);

  bool ignore_certificate_errors = false;
  ConfigProfile::GetValue(kServerSettings,
                          kConfigSecurityIgnoreCertificateErrors,
                          &ignore_certificate_errors);
  if (ignore_certificate_errors) {
    UTIL_LOG(L6, (_T("[InetHttpClient::SkipSSLCertErrorIfNeeded]")
                  _T("[ignore certificate error]")));
    DWORD flags = 0;
    DWORD flags_len = sizeof(flags);
    if (wininet_.InternetQueryOption(request_handle_,
                                     INTERNET_OPTION_SECURITY_FLAGS,
                                     &flags,
                                     &flags_len)) {
      flags |= SECURITY_FLAG_IGNORE_UNKNOWN_CA |
               SECURITY_FLAG_IGNORE_CERT_CN_INVALID;
      if (!wininet_.InternetSetOption(request_handle_,
                                      INTERNET_OPTION_SECURITY_FLAGS,
                                      &flags,
                                      sizeof(flags))) {
        UTIL_LOG(LEVEL_ERROR, (_T("[Failed to set ignore SSL cert error]")
                               _T("[0x%08x]"), HRESULTFromLastError()));
      }
    } else {
      UTIL_LOG(LEVEL_ERROR, (_T("[InternetQueryOption failed]")
                             _T("[0x%08x]"), HRESULTFromLastError()));
    }
  }
#endif

  return S_OK;
}

HRESULT InetHttpClient::SendRequest(const TCHAR* server,
                                    uint32 port,
                                    const TCHAR* path) {
  ASSERT1(server && *server);
  ASSERT1(path && *path);

  UTIL_LOG(L6, (_T("[InetHttpClient::SendRequest]")
                _T("[%s:%u%s]"), server, port, path));

  // When we are using WinInet http client, we will encouter auto-dial if the
  // user sets "Always dial my default connection" in Internet Explorer.
  // To fix this problem, we will check whether there is an active Internet
  // connection first.
  if (!IsMachineConnected()) {
    return HRESULT_FROM_WIN32(ERROR_INTERNET_CANNOT_CONNECT);
  }

  HRESULT hr = S_OK;

  for (uint32 i = 0; i < 1 + retries_; ++i) {
    if (i > 0) {
      ::Sleep(retry_delay_ms_);
      UTIL_LOG(L6, (_T("[InetHttpClient::SendRequest - retry %u]"), i));
    }

    hr = InternalSend(server, port, path, true);
    if (retry_without_proxy_ && !proxy_server_.IsEmpty() &&
        (FAILED(hr) || (status_ != HTTP_STATUS_OK &&
                        status_ != HTTP_STATUS_NOT_MODIFIED))) {
      UTIL_LOG(L6, (_T("[failed with proxy]")
                    _T("[%s][%u][0x%08x]"), proxy_server_, hr));
      hr = InternalSend(server, port, path, false);
      if (SUCCEEDED(hr)) {
        UTIL_LOG(L6, (_T("[switch to direct connection]")));
        GetProxyConfig()->SwitchAutoProxyToDirectConnection();
      }
    }
    if (SUCCEEDED(hr)) {
      break;
    }
  }

  if (FAILED(hr)) {
    // Convert hresults to be of the CI_E format.
    // If you need to be aware of another error code, then
    // add another CI_E_ error to this type of switch statment
    // in every HttpClient::Send method.
    //
    // __HRESULT_FROM_WIN32 will always be a macro where as
    // HRESULT_FROM_WIN32 can be an inline function. Using HRESULT_FROM_WIN32
    // can break the switch statement.
    //
    // TODO(omaha):  refactor weird switch.
    switch (hr) {
      case __HRESULT_FROM_WIN32(ERROR_INTERNET_SEC_CERT_DATE_INVALID):
        hr = CI_E_HTTPS_CERT_FAILURE;
        break;
    }
  }

  return hr;
}

HRESULT InetHttpClient::InternalSend(const TCHAR* server,
                                     uint32 port,
                                     const TCHAR* path,
                                     bool use_proxy) {
  ASSERT1(server && *server);
  ASSERT1(path && *path);

  UTIL_LOG(L6, (_T("[InetHttpClient::InternalSend]")
                _T("[%s:%u%s][%u]"), server, port, path, use_proxy));

  HRESULT hr = S_OK;

  // Close all handles from previous request.
  Close();

  // Open the Internet session.
  if (!use_proxy ||  proxy_server_.IsEmpty()) {
    session_handle_ = wininet_.InternetOpen(NULL,
                                            INTERNET_OPEN_TYPE_DIRECT,
                                            NULL,
                                            NULL,
                                            0);
  } else {
    session_handle_ = wininet_.InternetOpen(NULL,
                                            INTERNET_OPEN_TYPE_PROXY,
                                            proxy_server_,
                                            NULL,
                                            0);
  }
  if (!session_handle_) {
    hr = HRESULTFromLastError();
    UTIL_LOG(LEVEL_ERROR, (_T("[InternetOpen failed][0x%08x]"), hr));
    return hr;
  }

  if (is_aborted_) {
    return E_ABORT;
  }

  // Set timeouts.
  SetTimeouts();

  if (is_aborted_) {
    return E_ABORT;
  }

  // Connect to the server
  connection_handle_ = wininet_.InternetConnect(
                                    session_handle_,
                                    server,
                                    static_cast<INTERNET_PORT>(port),
                                    NULL,
                                    NULL,
                                    INTERNET_SERVICE_HTTP,
                                    0,
                                    NULL);
  if (!connection_handle_) {
    hr = HRESULTFromLastError();
    UTIL_LOG(LEVEL_ERROR, (_T("[InternetConnect failed]")
                           _T("[%s:%u%s][0x%08x]"), server, port, path, hr));
    return hr;
  }

  if (is_aborted_) {
    return E_ABORT;
  }

  // Set proxy credentials.
  if (use_proxy) {
    SetHttpProxyCredentials(proxy_auth_username_, proxy_auth_password_);
  }

  if (is_aborted_) {
    return E_ABORT;
  }

  // Prepare the flags.
  DWORD flags = no_cache_ ? kInternetRequestNoCacheDefaultFlags :
                            kInternetRequestCachingDefaultFlags;
  if (port == kDefaultSslProxyPort) {
    flags |= INTERNET_FLAG_SECURE;
  }

  // Send the request. A context is required otherwise the callback won't happen
  // even if the WinInet is used in synchronous mode.
  DWORD_PTR context = reinterpret_cast<DWORD_PTR>(this);
  if (post_data_.empty()) {
    // For GET.
    request_handle_ = wininet_.HttpOpenRequest(connection_handle_,
                                               kHttpGetMethod,
                                               path,
                                               NULL,
                                               NULL,
                                               NULL,
                                               flags,
                                               context);
    if (!request_handle_) {
      hr = HRESULTFromLastError();
      UTIL_LOG(LEVEL_ERROR, (_T("[HttpOpenRequest failed to open GET request]")
                             _T("[%s:%u%s][0x%08x]"), server, port, path, hr));
      return hr;
    }

    INTERNET_STATUS_CALLBACK old_callback =
        wininet_.InternetSetStatusCallback(request_handle_, &StatusCallback);
    ASSERT1(old_callback == NULL);
    if (old_callback != INTERNET_INVALID_STATUS_CALLBACK) {
      UTIL_LOG(L6, (_T("[InternetSetStatusCallback succeeded]")
                    _T("[0x%08x]"), request_handle_));
    } else {
      hr = HRESULTFromLastError();
      UTIL_LOG(LEVEL_WARNING, (_T("[InternetSetStatusCallback failed]")
                               _T("[%s:%u%s][0x%08x]"),
                               server, port, path, hr));
    }

    if (is_aborted_) {
      return E_ABORT;
    }

    if (port == kDefaultSslProxyPort) {
      SkipSSLCertErrorIfNeeded();
    }

    if (is_aborted_) {
      return E_ABORT;
    }

    if (!wininet_.HttpSendRequest(request_handle_,
                                  additional_headers_.GetString(),
                                  additional_headers_.GetLength(),
                                  NULL,
                                  0)) {
      hr = HRESULTFromLastError();
      UTIL_LOG(LEVEL_ERROR, (_T("[HttpSendRequest failed to send GET request]")
                             _T("[%s:%u%s][0x%08x]"), server, port, path, hr));

      // Occasionally setting the option to skip SSL certificate error before
      // sending the request does not work. We have to set it again after
      // failed to send the request.
      if (port == kDefaultSslProxyPort) {
        SkipSSLCertErrorIfNeeded();
        if (!wininet_.HttpSendRequest(request_handle_,
                                      additional_headers_.GetString(),
                                      additional_headers_.GetLength(),
                                      NULL,
                                      0)) {
          hr = HRESULTFromLastError();
          UTIL_LOG(LEVEL_ERROR, (_T("[HttpSendRequest failed to resend GET]")
                                 _T("[%s:%u%s][0x%08x]"),
                                 server, port, path, hr));
        } else {
          hr = S_OK;
        }
      }

      if (FAILED(hr)) {
        return hr;
      }
    }
  } else {
    // For post
    request_handle_ = wininet_.HttpOpenRequest(connection_handle_,
                                               kHttpPostMethod,
                                               path,
                                               NULL,
                                               NULL,
                                               NULL,
                                               flags,
                                               context);
    if (!request_handle_) {
      hr = HRESULTFromLastError();
      UTIL_LOG(LEVEL_ERROR, (_T("[HttpOpenRequest failed to open POST request]")
                             _T("[%s:%u%s][0x%08x]"), server, port, path, hr));
      return hr;
    }

    INTERNET_STATUS_CALLBACK old_callback =
        wininet_.InternetSetStatusCallback(request_handle_, StatusCallback);
    ASSERT1(old_callback == NULL);
    if (old_callback != INTERNET_INVALID_STATUS_CALLBACK) {
      UTIL_LOG(L6, (_T("[InternetSetStatusCallback succeeded]")
                    _T("[0x%08x]"), request_handle_));
    } else {
      hr = HRESULTFromLastError();
      UTIL_LOG(LEVEL_WARNING, (_T("[InternetSetStatusCallback failed]")
                               _T("[%s:%u%s][0x%08x]"),
                               server, port, path, hr));
    }

    if (is_aborted_) {
      return E_ABORT;
    }

    SkipSSLCertErrorIfNeeded();

    if (is_aborted_) {
      return E_ABORT;
    }

    if (!wininet_.HttpSendRequest(request_handle_,
                                  additional_headers_.GetString(),
                                  additional_headers_.GetLength(),
                                  &post_data_.front(),
                                  post_data_.size())) {
      hr = HRESULTFromLastError();
      UTIL_LOG(LEVEL_ERROR, (_T("[HttpSendRequest failed to send POST request]")
                             _T("[%s:%u%s][0x%08x]"), server, port, path, hr));
      return hr;
    }
  }

  if (is_aborted_) {
    return E_ABORT;
  }

  // Get the response status.
  DWORD value = 0;
  DWORD value_size = sizeof(value);
  if (!wininet_.HttpQueryInfo(request_handle_,
                              HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                              &value,
                              &value_size,
                              0)) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LEVEL_ERROR, (_T("[HttpQueryInfo failed to get status]")
                           _T("[%s:%u%s][0x%08x]"), server, port, path, hr));
    return hr;
  }

  status_ = static_cast<uint32>(value);
  return S_OK;
}

uint32 InetHttpClient::GetStatus() {
  return status_;
}

HRESULT InetHttpClient::GetResponseHeader(const TCHAR* name, CString* value) {
  ASSERT1(name && *name);
  ASSERT1(value);

  if (!status_) {
    return E_FAIL;
  }

  HRESULT hr = S_OK;

  value->SetString(name);
  TCHAR* buf = value->GetBuffer();
  DWORD size = value->GetLength() * sizeof(TCHAR);
  if (wininet_.HttpQueryInfo(request_handle_,
                             HTTP_QUERY_CUSTOM,
                             buf,
                             &size,
                             0)) {
    value->ReleaseBuffer(size / sizeof(TCHAR));
  } else {
    DWORD error = ::GetLastError();
    if (error == ERROR_INSUFFICIENT_BUFFER) {
      buf = value->GetBufferSetLength(size / sizeof(TCHAR));
      if (!wininet_.HttpQueryInfo(request_handle_,
                                  HTTP_QUERY_CUSTOM,
                                  buf,
                                  &size,
                                  0)) {
        hr = HRESULTFromLastError();
        UTIL_LOG(LEVEL_ERROR, (_T("[HttpQueryInfo failed][0x%08x]"), hr));
      }
      value->ReleaseBuffer(size / sizeof(TCHAR));
    } else {
      hr = HRESULT_FROM_WIN32(error);
      UTIL_LOG(LEVEL_ERROR, (_T("[HttpQueryInfo failed][0x%08x]"), hr));
    }
  }

  return hr;
}

HRESULT InetHttpClient::GetAllResponseHeaders(CString* response_headers) {
  ASSERT1(response_headers);
  ASSERT1(request_handle_);

  if (!status_) {
    return E_FAIL;
  }

  HRESULT hr = S_OK;

  DWORD size = 0;
  if (!wininet_.HttpQueryInfo(request_handle_,
                              HTTP_QUERY_RAW_HEADERS_CRLF,
                              NULL,
                              &size,
                              0)) {
    DWORD error = ::GetLastError();
    if (error == ERROR_INSUFFICIENT_BUFFER) {
      TCHAR* buf = response_headers->GetBufferSetLength(size / sizeof(TCHAR));
      if (!wininet_.HttpQueryInfo(request_handle_,
                                  HTTP_QUERY_RAW_HEADERS_CRLF,
                                  buf,
                                  &size,
                                  0)) {
        hr = HRESULTFromLastError();
        UTIL_LOG(LEVEL_ERROR, (_T("[HttpQueryInfo failed][0x%08x]"), hr));
      }
      response_headers->ReleaseBuffer(size / sizeof(TCHAR));
    } else {
      hr = HRESULT_FROM_WIN32(error);
      UTIL_LOG(LEVEL_ERROR, (_T("[HttpQueryInfo failed][0x%08x]"), hr));
    }
  }

  return hr;
}

HRESULT InetHttpClient::GetResponseText(CString* response_text,
                                        uint32 max_size) {
  ASSERT1(response_text);
  ASSERT1(request_handle_);

  if (!status_) {
    return E_FAIL;
  }

  const int kBufferSize = 1024;
  char buffer[kBufferSize] = {0};

  CStringA ansi_response_text;
  uint32 size = 0;
  while (true) {
    if (is_aborted_) {
      return E_ABORT;
    }

    DWORD bytes_read = 0;
    if (wininet_.InternetReadFile(request_handle_,
                                  buffer,
                                  kBufferSize,
                                  &bytes_read) && bytes_read) {
      size += bytes_read;
      if (max_size && size > max_size) {
        return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
      }
      ansi_response_text.Append(buffer, bytes_read);
    } else {
      break;
    }
  }

  response_text->SetString(Utf8ToWideChar(ansi_response_text.GetString(),
                                          ansi_response_text.GetLength()));

  return S_OK;
}

HRESULT InetHttpClient::GetRawResponseBody(std::vector<byte>* response_body,
                                           uint32 max_size) {
  return InternalGetRawResponseBody(response_body, NULL, max_size);
}

HRESULT InetHttpClient::SaveRawResponseBody(const TCHAR* response_filename,
                                            uint32 max_size) {
  return InternalGetRawResponseBody(NULL, response_filename, max_size);
}

HRESULT InetHttpClient::InternalGetRawResponseBody(
                            std::vector<byte>* response_body,
                            const TCHAR* response_filename,
                            uint32 max_size) {
  ASSERT((response_body != NULL) ^ (response_filename != NULL), (_T("")));
  ASSERT1(request_handle_);

  if (!status_) {
    return E_FAIL;
  }

  HRESULT hr = S_OK;
  scoped_hfile file;

  if (response_body) {
    response_body->clear();
  } else {
    reset(file, ::CreateFile(response_filename,
                             FILE_WRITE_DATA,
                             0,
                             NULL,
                             CREATE_ALWAYS,
                             0,
                             NULL));
    if (!file) {
      return HRESULTFromLastError();
    }
  }

  const int kBufferSize = 1024;
  char buffer[kBufferSize];
  uint32 size = 0;
  while (true) {
    if (is_aborted_) {
      return E_ABORT;
    }

    DWORD bytes_read = 0;
    if (wininet_.InternetReadFile(request_handle_,
                                  buffer,
                                  kBufferSize,
                                  &bytes_read) && bytes_read) {
      size += bytes_read;
      if (max_size && size > max_size) {
        return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
      }
      if (response_body) {
        response_body->insert(response_body->end(),
                              buffer,
                              buffer + bytes_read);
      } else {
        DWORD bytes_written = 0;
        if (!::WriteFile(get(file), buffer, bytes_read, &bytes_written, NULL)) {
          hr = HRESULTFromLastError();
          break;
        }
      }
    } else {
      break;
    }
  }

  return hr;
}

HRESULT InetHttpClient::QueryProxyAuthScheme(AuthScheme* auth_scheme) {
  ASSERT1(auth_scheme);

  *auth_scheme = NO_AUTH_SCHEME;

  // Find out authentication scheme by communicating with google server via
  // proxy server and checking the response header to find out the
  // authentication scheme the proxy server requires.

  // Send the request to query google server only if we have not got 407 error.
  HRESULT hr = S_OK;
  if (status_ != HTTP_STATUS_PROXY_AUTH_REQ) {
    hr = SendRequest(kGoogleHttpServer, kDefaultHttpProxyPort, _T("/"));
  }
  if (SUCCEEDED(hr) && status_ == HTTP_STATUS_PROXY_AUTH_REQ) {
    CString response_headers;
    hr = GetAllResponseHeaders(&response_headers);
    if (SUCCEEDED(hr)) {
      const TCHAR* response_headers_str = response_headers.GetString();
      CString auth_scheme_str;
      AuthScheme curr_auth_scheme = NO_AUTH_SCHEME;
      while (AtlRE::FindAndConsume(&response_headers_str,
                                   proxy_auth_header_regex_,
                                   &auth_scheme_str)) {
        AuthScheme other_auth_scheme = NO_AUTH_SCHEME;
        if (auth_scheme_str.CompareNoCase(kNegotiateAuthScheme) == 0) {
          other_auth_scheme = AUTH_SCHEME_NEGOTIATE;
        } else if (auth_scheme_str.CompareNoCase(kNTLMAuthScheme) == 0) {
          other_auth_scheme = AUTH_SCHEME_NTLM;
        } else if (auth_scheme_str.CompareNoCase(kDigestAuthScheme) == 0) {
          other_auth_scheme = AUTH_SCHEME_DIGEST;
        } else if (auth_scheme_str.CompareNoCase(kBasicAuthScheme) == 0) {
          other_auth_scheme = AUTH_SCHEME_BASIC;
        } else {
          other_auth_scheme = UNKNOWN_AUTH_SCHEME;
        }
        if (static_cast<int>(other_auth_scheme) >
            static_cast<int>(curr_auth_scheme)) {
          curr_auth_scheme = other_auth_scheme;
        }
      }
      *auth_scheme = curr_auth_scheme;
      return S_OK;
    }
  }

  return E_FAIL;
}

// We do not support Auto-proxy for WinInet because jsproxy.dll seems to be
// very instable and cause crashes from time to time.
HRESULT InetHttpClient::GetAutoProxyForUrl(const TCHAR*, CString*) {
  return E_NOTIMPL;
}

HRESULT InetHttpClient::GetDefaultProxyConfiguration(AccessType* access_type,
                                                     CString* proxy,
                                                     CString* proxy_bypass) {
  DWORD size_needed = 0;
  if (wininet_.InternetQueryOption(NULL,
                                   INTERNET_OPTION_PROXY,
                                   NULL,
                                   &size_needed) ||
      ::GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
    UTIL_LOG(LEVEL_ERROR, (_T("[InternetQueryOption failed]")));
    return E_FAIL;
  }

  scoped_array<byte> proxy_info_buffer(new byte[size_needed]);
  if (!wininet_.InternetQueryOption(NULL,
                                    INTERNET_OPTION_PROXY,
                                    proxy_info_buffer.get(),
                                    &size_needed)) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LEVEL_ERROR, (_T("[InternetQueryOption failed][0x%08x]"), hr));
    return hr;
  }

  INTERNET_PROXY_INFO* proxy_info = reinterpret_cast<INTERNET_PROXY_INFO*>(
                                        proxy_info_buffer.get());

  if (access_type) {
    if (proxy_info->dwAccessType == INTERNET_OPEN_TYPE_DIRECT) {
      *access_type = NO_PROXY;
    } else if (proxy_info->dwAccessType == INTERNET_OPEN_TYPE_PROXY) {
      *access_type = NAMED_PROXY;
    } else {
      *access_type = AUTO_PROXY_AUTO_DETECT;
    }
  }

  if (proxy) {
    // proxy_info->lpszProxy points to an ASCII string.
    *proxy = reinterpret_cast<const char*>(proxy_info->lpszProxy);
  }

  if (proxy_bypass) {
    // proxy_info->lpszProxyBypass points to an ASCII string.
    *proxy_bypass = reinterpret_cast<const char*>(proxy_info->lpszProxyBypass);
  }

  return S_OK;
}

// TODO(omaha): check connection by requesting a well known url instead of
// using WinInet. The WinInet code does an ICMP ping apparently, which is
// usually blocked by firewalls.
bool InetHttpClient::CheckConnection() {
  return wininet_.InternetCheckConnection(
      kHttpProtoScheme kProtoSuffix kGoogleHttpServer _T("/"),
      FLAG_ICC_FORCE_CONNECTION, 0) == TRUE;
}

HRESULT InetHttpClient::InternalCrackUrl(const CString& url, bool to_escape,
                                         URL_COMPONENTS* url_components) {
  ASSERT(!url.IsEmpty(), (_T("")));
  ASSERT1(url_components);

  if (!wininet_.InternetCrackUrl(url.GetString(),
                                 url.GetLength(),
                                 to_escape ? ICU_ESCAPE : 0,
                                 url_components)) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LEVEL_ERROR, (_T("[InternetCrackUrl failed][0x%08x]"), hr));
    return hr;
  }

  return S_OK;
}

void CALLBACK InetHttpClient::StatusCallback(HINTERNET handle,
                                             DWORD_PTR context,
                                             DWORD status,
                                             void* info,
                                             DWORD info_len) {
  // Unlike WinHttp, the ip addresses are not unicode strings.
  CString info_string;
  switch (status) {
    case INTERNET_STATUS_RESOLVING_NAME:
      status = CALLBACK_STATUS_RESOLVING_NAME;
      break;
    case INTERNET_STATUS_NAME_RESOLVED:
      info_string = static_cast<char*>(info);
      info = info_string.GetBuffer();
      status = CALLBACK_STATUS_NAME_RESOLVED;
      break;
    case INTERNET_STATUS_CONNECTING_TO_SERVER:
      info_string = static_cast<char*>(info);
      info = info_string.GetBuffer();
      status = CALLBACK_STATUS_CONNECTING_TO_SERVER;
      break;
    case INTERNET_STATUS_CONNECTED_TO_SERVER:
      info_string = static_cast<char*>(info);
      info = info_string.GetBuffer();
      status = CALLBACK_STATUS_CONNECTED_TO_SERVER;
      break;
    case INTERNET_STATUS_SENDING_REQUEST:
      status = CALLBACK_STATUS_SENDING_REQUEST;
      break;
    case INTERNET_STATUS_REQUEST_SENT:
      status = CALLBACK_STATUS_REQUEST_SENT;
      break;
    case INTERNET_STATUS_RECEIVING_RESPONSE:
      status = CALLBACK_STATUS_RECEIVING_RESPONSE;
      break;
    case INTERNET_STATUS_RESPONSE_RECEIVED:
      status = CALLBACK_STATUS_RESPONSE_RECEIVED;
      break;
    case INTERNET_STATUS_CLOSING_CONNECTION:
      status = CALLBACK_STATUS_CLOSING_CONNECTION;
      break;
    case INTERNET_STATUS_CONNECTION_CLOSED:
      status = CALLBACK_STATUS_CONNECTION_CLOSED;
      break;
    case INTERNET_STATUS_REDIRECT:
      status = CALLBACK_STATUS_REDIRECT;
      break;
    default:
      return;
  };
  HttpClient::StatusCallback(handle, context, status, info, info_len);
}

// WinInetVTable: delay-loading of WININET.DLL.
// Handling of and protection of Wininet APIs.
//   Dynamically load to wininet.dll.
//   Wrap all calls in an Structured Exception handler - wininet is buggy and
//   throws access violations
WinInetVTable::WinInetVTable() { Clear(); }
WinInetVTable::~WinInetVTable() { Unload(); }

template <typename T>
bool WinInetVTable::GPA(const char* function_name,
                        T& function_pointer) {      // NO LINT
  ASSERT(function_name, (L""));
  function_pointer = reinterpret_cast<T>(::GetProcAddress(library_,
                                                          function_name));
  return NULL != function_pointer;
}

bool WinInetVTable::Load() {
  if (Loaded()) {
    return true;
  }

  Clear();

  {
    library_ = ::LoadLibrary(_T("wininet"));
  }
  if (!library_) {
    return false;
  }

  bool all_valid = (
         GPA("HttpAddRequestHeadersW",       HttpAddRequestHeaders_pointer))
      & (GPA("HttpEndRequestW",              HttpEndRequest_pointer))
      & (GPA("HttpOpenRequestW",             HttpOpenRequest_pointer))
      & (GPA("HttpQueryInfoW",               HttpQueryInfo_pointer))
      & (GPA("HttpSendRequestExW",           HttpSendRequestEx_pointer))
      & (GPA("HttpSendRequestW",             HttpSendRequest_pointer))
      & (GPA("InternetCloseHandle",          InternetCloseHandle_pointer))
      & (GPA("InternetConnectW",             InternetConnect_pointer))
      & (GPA("InternetGetConnectedStateExW", InternetGetConnectedStateEx_pointer))
      & (GPA("InternetOpenW",                InternetOpen_pointer))
      & (GPA("InternetOpenUrlW",             InternetOpenUrl_pointer))
      & (GPA("InternetQueryDataAvailable",   InternetQueryDataAvailable_pointer))
      & (GPA("InternetReadFile",             InternetReadFile_pointer))
      & (GPA("InternetReadFileExW",          InternetReadFileEx_pointer))
      & (GPA("InternetSetStatusCallbackW",   InternetSetStatusCallback_pointer))
      & (GPA("InternetGetCookieW",           InternetGetCookie_pointer))
      & (GPA("InternetSetCookieW",           InternetSetCookie_pointer))
      & (GPA("InternetAutodial",             InternetAutodial_pointer))
      & (GPA("InternetAutodialHangup",       InternetAutodialHangup_pointer))
      & (GPA("InternetQueryOptionW",         InternetQueryOption_pointer))
      & (GPA("InternetSetOptionW",           InternetSetOption_pointer))
      & (GPA("InternetCheckConnectionW",     InternetCheckConnection_pointer))
      & (GPA("InternetCrackUrlW",            InternetCrackUrl_pointer));

  if (!all_valid) {
    Unload();
  }

  return all_valid;
}

void WinInetVTable::Unload() {
  if (library_) {
    ::FreeLibrary(library_);
    library_ = NULL;
  }
  Clear();
}

void WinInetVTable::Clear() {
  ::memset(this, 0, sizeof(*this));
}

#define PROTECT_WRAP(function, proto, call, result_type, result_error_value)  \
result_type WinInetVTable::function proto {                  \
    result_type result;                                      \
    __try {                                                  \
      result = function##_pointer call;                      \
    }                                                        \
    __except(EXCEPTION_EXECUTE_HANDLER) {                    \
      result = result_error_value;                           \
    }                                                        \
    return result;                                           \
}

PROTECT_WRAP(HttpAddRequestHeaders, (HINTERNET a, const TCHAR* b, DWORD c, DWORD d), (a, b, c, d), BOOL, FALSE);
PROTECT_WRAP(HttpEndRequest, (HINTERNET a, LPINTERNET_BUFFERS b, DWORD c, DWORD_PTR d), (a, b, c, d), BOOL, FALSE);
PROTECT_WRAP(HttpOpenRequest, (HINTERNET a, const TCHAR* b, const TCHAR* c, const TCHAR* d, const TCHAR* e, const TCHAR** f, DWORD g, DWORD_PTR h), (a, b, c, d, e, f, g, h), HINTERNET, NULL);
PROTECT_WRAP(HttpQueryInfo, (HINTERNET a, DWORD b, LPVOID c, LPDWORD d, LPDWORD e), (a, b, c, d, e), BOOL, FALSE);
PROTECT_WRAP(HttpSendRequestEx, (HINTERNET a, LPINTERNET_BUFFERS b, LPINTERNET_BUFFERS c, DWORD d, DWORD_PTR e), (a, b, c, d, e), BOOL, FALSE);
PROTECT_WRAP(HttpSendRequest, (HINTERNET a, const TCHAR* b, DWORD c, LPVOID d, DWORD e), (a, b, c, d, e), BOOL, FALSE);
PROTECT_WRAP(InternetCloseHandle, (HINTERNET a), (a), BOOL, FALSE);
PROTECT_WRAP(InternetConnect, (HINTERNET a, const TCHAR* b, INTERNET_PORT c, const TCHAR* d, const TCHAR* e, DWORD f, DWORD g, DWORD h), (a, b, c, d, e, f, g, h), HINTERNET, NULL);
PROTECT_WRAP(InternetGetConnectedStateEx, (LPDWORD a, char* b, DWORD c, DWORD d), (a, b, c, d), BOOL, FALSE);
PROTECT_WRAP(InternetOpen, (const TCHAR* a, DWORD b, const TCHAR* c, const TCHAR* d, DWORD e), (a, b, c, d, e), HINTERNET, NULL);
PROTECT_WRAP(InternetOpenUrl, (HINTERNET a, const TCHAR* b, const TCHAR* c, DWORD d, DWORD e, DWORD_PTR f), (a, b, c, d, e, f), HINTERNET, NULL);
PROTECT_WRAP(InternetQueryDataAvailable, (HINTERNET a, LPDWORD b, DWORD c, DWORD d), (a, b, c, d), BOOL, FALSE);
PROTECT_WRAP(InternetReadFile, (HINTERNET a, LPVOID b, DWORD c, LPDWORD d), (a, b, c, d), BOOL, FALSE);
PROTECT_WRAP(InternetReadFileEx, (HINTERNET a, LPINTERNET_BUFFERS b, DWORD c, DWORD_PTR d), (a, b, c, d), HINTERNET, NULL);
PROTECT_WRAP(InternetSetStatusCallback, (HINTERNET a, INTERNET_STATUS_CALLBACK b), (a, b), INTERNET_STATUS_CALLBACK, INTERNET_INVALID_STATUS_CALLBACK);
PROTECT_WRAP(InternetGetCookie, (const TCHAR* a, const TCHAR* b, TCHAR* c, LPDWORD d), (a, b, c, d), BOOL, FALSE);
PROTECT_WRAP(InternetSetCookie, (const TCHAR* a, const TCHAR* b, const TCHAR* c), (a, b, c), BOOL, FALSE);
PROTECT_WRAP(InternetAutodial, (DWORD a, HWND b), (a, b), BOOL, FALSE);
PROTECT_WRAP(InternetAutodialHangup, (DWORD a), (a), BOOL, FALSE);
PROTECT_WRAP(InternetQueryOption, (HINTERNET a, DWORD b, LPVOID c, LPDWORD d), (a, b, c, d), BOOL, FALSE);
PROTECT_WRAP(InternetSetOption, (HINTERNET a, DWORD b, LPVOID c, DWORD d), (a, b, c, d), BOOL, FALSE);
PROTECT_WRAP(InternetCheckConnection, (const TCHAR* a, DWORD b, DWORD c), (a, b, c), BOOL, FALSE);
PROTECT_WRAP(InternetCrackUrl, (const TCHAR* a, DWORD b, DWORD c, LPURL_COMPONENTS d), (a, b, c, d), BOOL, FALSE);
#endif

