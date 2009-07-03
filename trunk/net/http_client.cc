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
// The code below is not thread safe.

#include "omaha/net/http_client.h"

#include "omaha/common/debug.h"

namespace omaha {

HttpClient::Factory* HttpClient::factory_ = NULL;

HttpClient::Factory& HttpClient::GetFactory() {
  if (!factory_) {
    factory_ = new Factory();
  }
  return *factory_;
}

void HttpClient::DeleteFactory() {
  delete factory_;
  factory_ = NULL;
}

HttpClient* CreateHttpClient() {
  HttpClient* http_client =
    HttpClient::GetFactory().CreateObject(HttpClient::WINHTTP);
  if (!http_client) {
    http_client = HttpClient::GetFactory().CreateObject(HttpClient::WININET);
  }
  return http_client;
}

CString HttpClient::BuildRequestHeader(const TCHAR* name, const TCHAR* value) {
  ASSERT1(name && *name);
  ASSERT1(value && *value);
  CString header;
  header.Format(_T("%s: %s\r\n"), name, value);
  return header;
}

HttpClient::StatusCodeClass HttpClient::GetStatusCodeClass(int status_code) {
  ASSERT1(!status_code ||
      (HTTP_STATUS_FIRST <= status_code && status_code <= HTTP_STATUS_LAST));
  return static_cast<StatusCodeClass>(status_code / 100 * 100);
}

HRESULT HttpClient::QueryHeadersString(HINTERNET request_handle,
                                       uint32 info_level,
                                       const TCHAR* name,
                                       CString* value,
                                       DWORD* index) {
  ASSERT1(value);

  DWORD num_bytes = 0;
  HRESULT hr = QueryHeaders(request_handle,
                            info_level,
                            name,
                            NULL,
                            &num_bytes,
                            index);
  if (hr != HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER)) {
    return hr;
  }
  CString val;
  hr = QueryHeaders(request_handle,
                    info_level,
                    name,
                    val.GetBuffer(num_bytes/sizeof(TCHAR)),
                    &num_bytes,
                    index);
  if (FAILED(hr)) {
    return hr;
  }
  ASSERT1(num_bytes);
  val.ReleaseBufferSetLength(num_bytes/sizeof(TCHAR));
  *value = val;
  return S_OK;
}

HRESULT HttpClient::QueryHeadersInt(HINTERNET request_handle,
                                    uint32 info_level,
                                    const TCHAR* name,
                                    int* value,
                                    DWORD* index) {
  ASSERT1(value);
  info_level |= WINHTTP_QUERY_FLAG_NUMBER;
  DWORD value_size = sizeof(*value);
  HRESULT hr = QueryHeaders(request_handle,
                            info_level,
                            name,
                            value,
                            &value_size,
                            index);
  if (FAILED(hr)) {
    return hr;
  }
  ASSERT1(value_size == sizeof(*value));
  return S_OK;
}

HRESULT HttpClient::QueryOptionString(HINTERNET handle,
                                      uint32 option,
                                      CString* value) {
  ASSERT1(value);
  DWORD num_bytes = 0;
  HRESULT hr = QueryOption(handle, option, NULL, &num_bytes);
  DWORD last_error = ::GetLastError();
  if (hr != HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER)) {
    return hr;
  }
  ASSERT1(num_bytes);
  CString val;
  hr = QueryOption(handle, option, val.GetBuffer(num_bytes), &num_bytes);
  if (FAILED(hr)) {
    return hr;
  }
  ASSERT1(num_bytes);
  val.ReleaseBufferSetLength(num_bytes/sizeof(TCHAR));
  *value = val;
  return S_OK;
}

HRESULT HttpClient::QueryOptionInt(HINTERNET handle,
                                   uint32 option,
                                   int* value) {
  ASSERT1(value);
  DWORD val = 0;
  DWORD num_bytes = sizeof(val);
  HRESULT hr = QueryOption(handle, option, &val, &num_bytes);
  if (FAILED(hr)) {
    return hr;
  }
  ASSERT1(num_bytes == sizeof(val));
  *value = val;
  return S_OK;
}

HRESULT HttpClient::SetOptionString(HINTERNET handle,
                                    uint32 option,
                                    const TCHAR* value) {
  ASSERT1(value);
  const void* buffer = value;
  DWORD buffer_length = _tcslen(value) * sizeof(TCHAR);
  return SetOption(handle, option, buffer, buffer_length);
}

HRESULT HttpClient::SetOptionInt(HINTERNET handle, uint32 option, int value) {
  DWORD val = value;
  return SetOption(handle, option, &val, sizeof(val));
}

}  // namespace omaha

