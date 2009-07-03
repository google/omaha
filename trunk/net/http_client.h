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
// HttpClient provides a wrapper for WinHTTP and some helpers to make it
// easy to call it from c++.

// Runtime requirements: WinHTTP 5.1 is now an operating-system component of
// the following systems:
// - Windows Vista
// - Windows Server 2003 family
// - Windows XP SP1
// - Windows 2000 SP3 (except Datacenter Server)
//
// TODO(omaha): fix prototype of several methods to take size_t instead of int
// TODO(omaha): there are many overloads that do not have to be virtual.
// TODO(omaha): in the end HttpClient is just a c++ wrapper for WinHttp. Some
//              refactoring may be needed, for example to provide access to
//              the internal session and connection handles. It may make sense
//              to reuse these handles, most likely the session handle.
// TODO(omaha): the idea of implementing the winhttp interface on top of
//              wininet seems impractical now, as they may be subtle
//              differences in behavior. The fallback on wininet when winhttp
//              is not available could be better done by using the higher
//              level urlmon functions, which perhaps we should do anyway,
//              just in case winhttp is not working for some reason or bug.
//              Since now we are not doing downloads in any of our long lived
//              processes, the handle and memory leaks do not affect us much.

#ifndef OMAHA_NET_HTTP_CLIENT_H__
#define OMAHA_NET_HTTP_CLIENT_H__

#include <windows.h>
#include <atlstr.h>

#if !defined(_WININET_)
#include <winhttp.h>    // NOLINT

  // The following definitions are missing in winhttp.h
#define INTERNET_MAX_HOST_NAME_LENGTH   256
#define INTERNET_MAX_USER_NAME_LENGTH   128
#define INTERNET_MAX_PASSWORD_LENGTH    128
#define INTERNET_MAX_PORT_NUMBER_LENGTH 5
#define INTERNET_MAX_PORT_NUMBER_VALUE  65535
#define INTERNET_MAX_PATH_LENGTH        2048
#define INTERNET_MAX_SCHEME_LENGTH      32
#define INTERNET_MAX_URL_LENGTH         (INTERNET_MAX_SCHEME_LENGTH \
                                        + sizeof("://") \
                                        + INTERNET_MAX_PATH_LENGTH)
#endif

// This is a definition we made up for convenience, just to be consistent with
// the other types of network access.
#define WINHTTP_ACCESS_TYPE_AUTO_DETECT 2

#include "base/basictypes.h"
#include "omaha/common/object_factory.h"

namespace omaha {

const TCHAR* const kHttpGetMethod = _T("GET");
const TCHAR* const kHttpPostMethod = _T("POST");
const TCHAR* const kHttpContentLengthHeader = _T("Content-Length");
const TCHAR* const kHttpContentTypeHeader = _T("Content-Type");
const TCHAR* const kHttpLastModifiedHeader = _T("Last-Modified");
const TCHAR* const kHttpIfModifiedSinceHeader = _T("If-Modified-Since");
const TCHAR* const kHttpPostTextContentType =
                _T("application/x-www-form-urlencoded");
const TCHAR* const kHttpPostRawContentType = _T("application/octet-stream");
const TCHAR* const kHttpBinaryContentType =_T("binary");
const TCHAR* const kHttpXmlContentType =_T("application/xml");

class HttpClient {
 public:
  enum HttpStack {WINHTTP, WININET};

  // Use registry for network configuration info.
  static const int kAccessTypeDefaultProxy  = 0;
  // Unconditional direct connection.
  static const int kAccessTypeNoProxy       = 1;
  // Use the specified proxy server.
  static const int kAccessTypeNamedProxy    = 3;

  // GetFactory and DeleteFactory methods below are not thread safe.
  // The caller must initialize and cleanup the factory before going
  // multithreaded.
  //
  // Gets the singleton instance of the object factory.
  typedef Factory<HttpClient, HttpStack> Factory;
  static Factory& GetFactory();

  // Cleans up the object factory.
  static void DeleteFactory();

  struct ProxyInfo {
    uint32 access_type;
    const TCHAR* proxy;
    const TCHAR* proxy_bypass;
  };

  struct CurrentUserIEProxyConfig {
    bool auto_detect;
    const TCHAR* auto_config_url;
    const TCHAR* proxy;
    const TCHAR* proxy_bypass;
  };

  struct AutoProxyOptions {
    uint32 flags;
    uint32 auto_detect_flags;
    const TCHAR* auto_config_url;
    bool auto_logon_if_challenged;
  };

  virtual ~HttpClient() {}

  // Initializes the use of http functions by loading the corresponding http
  // stack.
  virtual HRESULT Initialize() = 0;

  // Adds one or more http request headers to the http request.
  virtual HRESULT AddRequestHeaders(HINTERNET request,
                                    const TCHAR* headers,
                                    int length,
                                    uint32 modifiers) = 0;

  // Determines whether WinHTTP is available.
  virtual HRESULT CheckPlatform() = 0;

  // Closes an http handle.
  virtual HRESULT Close(HINTERNET handle) = 0;

  // Specifies the initial server and port of an http request.
  virtual HRESULT Connect(HINTERNET session_handle,
                          const TCHAR* server,
                          int port,
                          HINTERNET* connection_handle) = 0;

  // Breaks urls into component parts.
  virtual HRESULT CrackUrl(const TCHAR* url,
                           uint32 flags,
                           CString* scheme,
                           CString* server,
                           int* port,
                           CString* url_path,
                           CString* extra_info) = 0;

  // Builds a url from component parts.
  virtual HRESULT CreateUrl(const TCHAR* scheme,
                            const TCHAR* server,
                            int port,
                            const TCHAR* url_path,
                            const TCHAR* extra_info,
                            uint32 flags,
                            CString* url) = 0;


  // Finds the url for the proxy auto-configuration (PAC) file.
  // It reports the url of the PAC file but it does not download the file.
  virtual HRESULT DetectAutoProxyConfigUrl(uint32 flags,
                                           CString* auto_config_url) = 0;

  // Gets the proxy configuration from the registry.
  virtual HRESULT GetDefaultProxyConfiguration(ProxyInfo* proxy_info) = 0;

  // Gets the Internet Explorer configuration for the current user.
  virtual HRESULT GetIEProxyConfiguration(
                      CurrentUserIEProxyConfig* proxy_info) = 0;

  // Gets the proxy information for an url. This function implements the
  // web proxy auto-discovery (WPAD) protocol for automatically configuring
  // the proxy settings for an http request. The WPAD protocol downloads a
  // proxy auto-configuration (PAC) file, which is a script that identifies
  // the proxy server to use for a given target url.
  virtual HRESULT GetProxyForUrl(HINTERNET session_handle,
                                 const TCHAR* url,
                                 const AutoProxyOptions* auto_proxy_options,
                                 ProxyInfo* proxy_info) = 0;

  // Opens a new http session. The http session contains user specific state
  // such as cookies, proxy, and authentication credentials.
  virtual HRESULT Open(const TCHAR* user_agent,
                       uint32 access_type,
                       const TCHAR* proxy_name,
                       const TCHAR* proxy_bypass,
                       HINTERNET* session_handle) = 0;

  // Specifies the http request.
  virtual HRESULT OpenRequest(HINTERNET connection_handle,
                              const TCHAR* verb,
                              const TCHAR* uri,
                              const TCHAR* version,
                              const TCHAR* referrer,
                              const TCHAR** accept_types,
                              uint32 flags,
                              HINTERNET* request_handle) = 0;

  // Returns the authorization schemes that are supported by the server or
  // the proxy server.
  virtual HRESULT QueryAuthSchemes(HINTERNET request_handle,
                                   uint32* supported_schemes,
                                   uint32* first_scheme,
                                   uint32* auth_target) = 0;

  // Returns the amount of data, in bytes, available to be read.
  virtual HRESULT QueryDataAvailable(HINTERNET request_handle,
                                     DWORD* num_bytes) = 0;

  // Retrieves header information associated with the request.
  virtual HRESULT QueryHeaders(HINTERNET request_handle,
                               uint32 info_level,
                               const TCHAR* name,
                               void* buffer,
                               DWORD* buffer_length,
                               DWORD* index) = 0;

  // Queries an internet option.
  virtual HRESULT QueryOption(HINTERNET handle,
                              uint32 option,
                              void* buffer,
                              DWORD* buffer_length) = 0;

  // Reads response data.
  virtual HRESULT ReadData(HINTERNET request_handle,
                           void* buffer,
                           DWORD buffer_length,
                           DWORD* bytes_read) = 0;

  // Waits to receive the response to an HTTP request. When ReceiveResponse
  // completes successfully, the status code and response headers have been
  // received.
  virtual HRESULT ReceiveResponse(HINTERNET request_handle) = 0;

  // Sends the request to the server. It also allows to send optional data
  // for POST requests.
  virtual HRESULT SendRequest(HINTERNET request_handle,
                              const TCHAR* headers,
                              DWORD headers_length,
                              const void* optional_data,
                              DWORD optional_data_length,
                              DWORD content_length) = 0;

  // Sets the authentication credentials.
  virtual HRESULT SetCredentials(HINTERNET request_handle,
                                 uint32 auth_targets,
                                 uint32 auth_scheme,
                                 const TCHAR* user_name,
                                 const TCHAR* password) = 0;

  // Sets the proxy configuration in registry.
  virtual HRESULT SetDefaultProxyConfiguration(const ProxyInfo& proxy_info) = 0;

  // Sets an internet option, either for the session or the request handle.
  virtual HRESULT SetOption(HINTERNET handle,
                            uint32 option,
                            const void* buffer,
                            DWORD buffer_length) = 0;

  typedef void (__stdcall *StatusCallback)(HINTERNET handle,
                                           uint32 context,
                                           uint32 status,
                                           void* status_information,
                                           size_t status_info_length);
  virtual StatusCallback SetStatusCallback(HINTERNET handle,
                                           StatusCallback callback,
                                           uint32 flags) = 0;

  // Sets timeout values for this http request.
  virtual HRESULT SetTimeouts(HINTERNET handle,
                              int resolve_timeout_ms,
                              int connect_timeout_ms,
                              int send_timeout_ms,
                              int receive_timeout_ms) = 0;

  // Writes data to the server.
  virtual HRESULT WriteData(HINTERNET request_handle,
                            const void* buffer,
                            DWORD bytes_to_write,
                            DWORD* bytes_written) = 0;

  //
  // Http helpers.
  //
  // Builds one http header entry, with the following format:
  //    name: value\r\n
  static CString BuildRequestHeader(const TCHAR* name, const TCHAR* value);

  enum StatusCodeClass {
    STATUS_CODE_NOCODE        = 0,
    STATUS_CODE_INFORMATIONAL = 100,
    STATUS_CODE_SUCCESSFUL    = 200,
    STATUS_CODE_REDIRECTION   = 300,
    STATUS_CODE_CLIENT_ERROR  = 400,
    STATUS_CODE_SERVER_ERROR  = 500
  };
  // Returns the class of a status code, such as 100, 200...
  static StatusCodeClass GetStatusCodeClass(int status_code);

  HRESULT QueryHeadersString(HINTERNET request_handle,
                             uint32 info_level,
                             const TCHAR* name,
                             CString* value,
                             DWORD* index);
  HRESULT QueryHeadersInt(HINTERNET request_handle,
                          uint32 info_level,
                          const TCHAR* name,
                          int* value,
                          DWORD* index);

  HRESULT QueryOptionString(HINTERNET handle, uint32 option, CString* value);
  HRESULT QueryOptionInt(HINTERNET handle, uint32 option, int* value);

  HRESULT SetOptionString(HINTERNET handle, uint32 option, const TCHAR* value);
  HRESULT SetOptionInt(HINTERNET handle, uint32 option, int value);

 protected:
  HttpClient() {}

 private:
  static Factory* factory_;
  DISALLOW_EVIL_CONSTRUCTORS(HttpClient);
};

// Creates an http client, depending on what is available on the platform.
// WinHttp is preferred over WinInet.
HttpClient* CreateHttpClient();

}  // namespace omaha

#endif  // OMAHA_NET_HTTP_CLIENT_H__

