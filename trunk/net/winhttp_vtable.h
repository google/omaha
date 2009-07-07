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
// WinHttpVTable provides dynamic loading of winhttp.dll.

// Runtime requirements: WinHTTP 5.1 is now an operating-system component of
// the following systems:
// - Windows Vista
// - Windows Server 2003 family
// - Windows XP SP1
// - Windows 2000 SP3 (except Datacenter Server)

#ifndef OMAHA_NET_WINHTTP_VTABLE_H__
#define OMAHA_NET_WINHTTP_VTABLE_H__

#include <windows.h>
#include <winhttp.h>
#include "omaha/common/debug.h"

namespace omaha {

class WinHttpVTable {
 public:
  BOOL WinHttpAddRequestHeaders(HINTERNET, const TCHAR*, DWORD, DWORD);
  BOOL WinHttpCheckPlatform();
  BOOL WinHttpCloseHandle(HINTERNET);
  HINTERNET WinHttpConnect(HINTERNET, const TCHAR*, INTERNET_PORT, DWORD);
  BOOL WinHttpCrackUrl(const TCHAR*, DWORD, DWORD, URL_COMPONENTS*);
  BOOL WinHttpCreateUrl(URL_COMPONENTS*, DWORD, TCHAR*, DWORD*);
  BOOL WinHttpGetDefaultProxyConfiguration(WINHTTP_PROXY_INFO*);
  BOOL WinHttpGetIEProxyConfigForCurrentUser(
           WINHTTP_CURRENT_USER_IE_PROXY_CONFIG*);
  HINTERNET WinHttpOpen(const TCHAR*, DWORD, const TCHAR*, const TCHAR*, DWORD);
  HINTERNET WinHttpOpenRequest(HINTERNET,
                               const TCHAR*,
                               const TCHAR*,
                               const TCHAR*,
                               const TCHAR*,
                               const TCHAR**,
                               DWORD);
  BOOL WinHttpQueryAuthSchemes(HINTERNET, DWORD*, DWORD*, DWORD*);
  BOOL WinHttpQueryDataAvailable(HINTERNET, DWORD*);
  BOOL WinHttpQueryHeaders(HINTERNET,
                           DWORD,
                           const TCHAR*,
                           void*,
                           DWORD*,
                           DWORD*);
  BOOL WinHttpQueryOption(HINTERNET, DWORD, void*, DWORD*);
  BOOL WinHttpReadData(HINTERNET, void*, DWORD, DWORD*);
  BOOL WinHttpWriteData(HINTERNET, const void*, DWORD, DWORD*);
  BOOL WinHttpReceiveResponse(HINTERNET, void*);
  BOOL WinHttpSendRequest(HINTERNET,
                          const TCHAR*,
                          DWORD,
                          void*,
                          DWORD,
                          DWORD,
                          DWORD_PTR);
  BOOL WinHttpSetCredentials(HINTERNET,
                             DWORD,
                             DWORD,
                             const TCHAR*,
                             const TCHAR*,
                             void*);
  BOOL WinHttpSetOption(HINTERNET, DWORD, void*, DWORD);
  WINHTTP_STATUS_CALLBACK WinHttpSetStatusCallback(HINTERNET,
                                                   WINHTTP_STATUS_CALLBACK,
                                                   DWORD,
                                                   DWORD_PTR);
  BOOL WinHttpSetTimeouts(HINTERNET, int, int, int, int);
  BOOL WinHttpGetProxyForUrl(HINTERNET,
                             const TCHAR*,
                             WINHTTP_AUTOPROXY_OPTIONS*,
                             WINHTTP_PROXY_INFO*);
  BOOL WinHttpDetectAutoProxyConfigUrl(DWORD, TCHAR**);
  BOOL WinHttpSetDefaultProxyConfiguration(WINHTTP_PROXY_INFO*);

  WinHttpVTable() { Clear(); }
  ~WinHttpVTable() { Unload(); }

  // Loads library, snap links.
  bool Load();

  // Unloads library, clear links.
  void Unload();

  bool IsLoaded() { return NULL != library_; }

 private:
  void Clear() { memset(this, 0, sizeof(*this)); }

  template <typename T>
  bool GPA(const char* function_name, T* function_pointer) {
    ASSERT1(function_name);
    *function_pointer = reinterpret_cast<T>(::GetProcAddress(library_,
                                                             function_name));
    return NULL != *function_pointer;
  }

  // No good way to keep lines below 80 chars.
  BOOL      (CALLBACK *WinHttpAddRequestHeaders_pointer)(HINTERNET, const TCHAR*, DWORD, DWORD);    // NOLINT
  BOOL      (CALLBACK *WinHttpCheckPlatform_pointer)();
  BOOL      (CALLBACK *WinHttpCloseHandle_pointer)(HINTERNET);    // NOLINT
  HINTERNET (CALLBACK *WinHttpConnect_pointer)(HINTERNET, const TCHAR*, INTERNET_PORT, DWORD);    // NOLINT
  BOOL      (CALLBACK *WinHttpCrackUrl_pointer)(const TCHAR*, DWORD, DWORD, URL_COMPONENTS*);    // NOLINT
  BOOL      (CALLBACK *WinHttpCreateUrl_pointer)(URL_COMPONENTS*, DWORD, TCHAR*, DWORD*);        // NOLINT
  BOOL      (CALLBACK *WinHttpDetectAutoProxyConfigUrl_pointer)(DWORD, TCHAR**);    // NOLINT
  BOOL      (CALLBACK *WinHttpGetDefaultProxyConfiguration_pointer)(WINHTTP_PROXY_INFO*);    // NOLINT
  BOOL      (CALLBACK *WinHttpGetIEProxyConfigForCurrentUser_pointer)(WINHTTP_CURRENT_USER_IE_PROXY_CONFIG* pProxyConfig);    // NOLINT
  BOOL      (CALLBACK *WinHttpGetProxyForUrl_pointer)(HINTERNET, const TCHAR*, WINHTTP_AUTOPROXY_OPTIONS*, WINHTTP_PROXY_INFO*);    // NOLINT
  HINTERNET (CALLBACK *WinHttpOpen_pointer)(const TCHAR*, DWORD, const TCHAR*, const TCHAR*, DWORD);    // NOLINT
  HINTERNET (CALLBACK *WinHttpOpenRequest_pointer)(HINTERNET, const TCHAR*, const TCHAR*, const TCHAR*, const TCHAR*, const TCHAR**, DWORD);    // NOLINT
  BOOL      (CALLBACK *WinHttpQueryAuthSchemes_pointer)(HINTERNET, DWORD*, DWORD*, DWORD*);    // NOLINT
  BOOL      (CALLBACK *WinHttpQueryDataAvailable_pointer)(HINTERNET, DWORD*);    // NOLINT
  BOOL      (CALLBACK *WinHttpQueryHeaders_pointer)(HINTERNET, DWORD, const TCHAR*, void*, DWORD*, DWORD*);    // NOLINT
  BOOL      (CALLBACK *WinHttpQueryOption_pointer)(HINTERNET, DWORD, void*, DWORD*);    // NOLINT
  BOOL      (CALLBACK *WinHttpReadData_pointer)(HINTERNET, void*, DWORD, DWORD*);    // NOLINT
  BOOL      (CALLBACK *WinHttpReceiveResponse_pointer)(HINTERNET, void*);    // NOLINT
  BOOL      (CALLBACK *WinHttpSendRequest_pointer)(HINTERNET, const TCHAR*, DWORD, void*, DWORD, DWORD, DWORD_PTR);    // NOLINT
  BOOL      (CALLBACK *WinHttpSetCredentials_pointer)(HINTERNET, DWORD, DWORD, const TCHAR*, const TCHAR*, void*);    // NOLINT
  BOOL      (CALLBACK *WinHttpSetDefaultProxyConfiguration_pointer)(WINHTTP_PROXY_INFO*);   // NOLINT
  BOOL      (CALLBACK *WinHttpSetOption_pointer)(HINTERNET, DWORD, void*, DWORD);    // NOLINT
  BOOL      (CALLBACK *WinHttpSetTimeouts_pointer)(HINTERNET, int, int, int, int);    // NOLINT
  BOOL      (CALLBACK *WinHttpWriteData_pointer)(HINTERNET, LPCVOID, DWORD, DWORD*);   // NOLINT
  WINHTTP_STATUS_CALLBACK (CALLBACK *WinHttpSetStatusCallback_pointer)(HINTERNET, WINHTTP_STATUS_CALLBACK, DWORD, DWORD_PTR);    // NOLINT

  HINSTANCE library_;

  DISALLOW_EVIL_CONSTRUCTORS(WinHttpVTable);
};

#define PROTECT_WRAP(function, proto, call, result_type, result_error_value)  \
inline result_type WinHttpVTable::function proto {         \
  return function##_pointer call;                          \
}

// No good way to keep lines below 80 chars.
PROTECT_WRAP(WinHttpAddRequestHeaders, (HINTERNET a, const TCHAR* b, DWORD c, DWORD d), (a, b, c, d), BOOL, FALSE);    // NOLINT
PROTECT_WRAP(WinHttpCheckPlatform, (), (), BOOL, FALSE);
PROTECT_WRAP(WinHttpCloseHandle, (HINTERNET a), (a), BOOL, FALSE);    // NOLINT
PROTECT_WRAP(WinHttpConnect, (HINTERNET a, const TCHAR* b, INTERNET_PORT c, DWORD d), (a, b, c, d), HINTERNET, NULL);    // NOLINT
PROTECT_WRAP(WinHttpCrackUrl, (const TCHAR* a, DWORD b, DWORD c, LPURL_COMPONENTS d), (a, b, c, d), BOOL, FALSE);    // NOLINT
PROTECT_WRAP(WinHttpCreateUrl, (URL_COMPONENTS* a, DWORD b, TCHAR* c, DWORD* d), (a, b, c, d), BOOL, FALSE);    // NOLINT
PROTECT_WRAP(WinHttpDetectAutoProxyConfigUrl, (DWORD a, TCHAR** b), (a, b), BOOL, FALSE);   // NOLINT
PROTECT_WRAP(WinHttpGetDefaultProxyConfiguration, (WINHTTP_PROXY_INFO* a), (a), BOOL, FALSE);    // NOLINT
PROTECT_WRAP(WinHttpGetIEProxyConfigForCurrentUser, (WINHTTP_CURRENT_USER_IE_PROXY_CONFIG* a), (a),  BOOL, FALSE);    // NOLINT
PROTECT_WRAP(WinHttpGetProxyForUrl, (HINTERNET a, const TCHAR* b, WINHTTP_AUTOPROXY_OPTIONS* c, WINHTTP_PROXY_INFO* d), (a, b, c, d), BOOL, FALSE);    // NOLINT
PROTECT_WRAP(WinHttpOpen, (const TCHAR* a, DWORD b, const TCHAR* c, const TCHAR* d, DWORD e), (a, b, c, d, e), HINTERNET, NULL);    // NOLINT
PROTECT_WRAP(WinHttpOpenRequest, (HINTERNET a, const TCHAR* b, const TCHAR* c, const TCHAR* d, const TCHAR* e, const TCHAR** f, DWORD g), (a, b, c, d, e, f, g), HINTERNET, NULL);    // NOLINT
PROTECT_WRAP(WinHttpQueryAuthSchemes, (HINTERNET a, DWORD* b, DWORD* c, DWORD* d), (a, b, c, d), BOOL, FALSE);    // NOLINT
PROTECT_WRAP(WinHttpQueryDataAvailable, (HINTERNET a, DWORD* b), (a, b), BOOL, FALSE);    // NOLINT
PROTECT_WRAP(WinHttpQueryHeaders, (HINTERNET a, DWORD b, const TCHAR* c, void* d, DWORD* e, DWORD* f), (a, b, c, d, e, f), BOOL, FALSE);    // NOLINT
PROTECT_WRAP(WinHttpQueryOption, (HINTERNET a, DWORD b, void* c, DWORD* d), (a, b, c, d), BOOL, FALSE);    // NOLINT
PROTECT_WRAP(WinHttpReadData, (HINTERNET a, void* b, DWORD c, DWORD* d), (a, b, c, d), BOOL, FALSE);    // NOLINT
PROTECT_WRAP(WinHttpReceiveResponse, (HINTERNET a, void* b), (a, b), BOOL, FALSE);    // NOLINT
PROTECT_WRAP(WinHttpSendRequest, (HINTERNET a, const TCHAR* b, DWORD c, void* d, DWORD e, DWORD f, DWORD_PTR g), (a, b, c, d, e, f, g), BOOL, FALSE);    // NOLINT
PROTECT_WRAP(WinHttpSetCredentials, (HINTERNET a, DWORD b, DWORD c, const TCHAR* d, const TCHAR* e, void* f), (a, b, c, d, e, f), BOOL, FALSE);    // NOLINT
PROTECT_WRAP(WinHttpSetDefaultProxyConfiguration, (WINHTTP_PROXY_INFO* a), (a), BOOL, FALSE);    // NOLINT
PROTECT_WRAP(WinHttpSetOption, (HINTERNET a, DWORD b, void* c, DWORD d), (a, b, c, d), BOOL, FALSE);    // NOLINT
PROTECT_WRAP(WinHttpSetStatusCallback, (HINTERNET a, WINHTTP_STATUS_CALLBACK b, DWORD c, DWORD_PTR d), (a, b, c, d), WINHTTP_STATUS_CALLBACK, WINHTTP_INVALID_STATUS_CALLBACK);    // NOLINT
PROTECT_WRAP(WinHttpSetTimeouts, (HINTERNET a, int b, int c, int d, int e), (a, b, c, d, e), BOOL, FALSE);    // NOLINT
PROTECT_WRAP(WinHttpWriteData, (HINTERNET a, LPCVOID b, DWORD c, DWORD* d), (a, b, c, d), BOOL, FALSE);    // NOLINT

}   // namespace omaha

#endif  // OMAHA_NET_WINHTTP_VTABLE_H__

