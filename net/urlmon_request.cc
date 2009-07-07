// Copyright 2008-2009 Google Inc.
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

#include "omaha/net/urlmon_request.h"
#include <winhttp.h>
#include <atlbase.h>
#include <atlcom.h>
#include <atlcomcli.h>
#include <vector>
#include "omaha/common/error.h"
#include "omaha/common/file.h"
#include "omaha/common/logging.h"
#include "omaha/common/string.h"
#include "omaha/common/utils.h"
#include "omaha/net/bind_status_callback.h"
#include "omaha/net/http_client.h"
#include "omaha/net/network_request.h"
#include "omaha/net/network_config.h"

namespace omaha {

const DWORD response_headers_needed[] = {
  WINHTTP_QUERY_CONTENT_TYPE,
  WINHTTP_QUERY_ETAG,
  WINHTTP_QUERY_SET_COOKIE,
  WINHTTP_QUERY_FLAG_REQUEST_HEADERS | WINHTTP_QUERY_USER_AGENT,
  WINHTTP_QUERY_RAW_HEADERS_CRLF,
};

UrlmonRequest::UrlmonRequest()
    : request_buffer_(NULL),
      request_buffer_length_(0),
      http_status_code_(0),
      is_cancelled_(false) {
  NET_LOG(L3, (_T("[UrlmonRequest::UrlmonRequest]")));
  user_agent_.Format(_T("%s;urlmon"), NetworkConfig::GetUserAgent());
}

UrlmonRequest::~UrlmonRequest() {
  Close();
}

HRESULT UrlmonRequest::Close() {
  http_status_code_ = 0;
  response_body_.clear();
  raw_response_headers_.Empty();
  response_headers_map_.clear();
  return S_OK;
}

CComBSTR BuildRequestHeaders(const CString& user_agent,
                             const CString& additional_headers) {
  CString headers_to_send;
  if (!user_agent.IsEmpty()) {
    headers_to_send += _T("User-Agent: ");
    headers_to_send += user_agent;
    headers_to_send = String_MakeEndWith(headers_to_send, _T("\r\n"), false);
  }

  if (!additional_headers.IsEmpty()) {
    headers_to_send += additional_headers;
    headers_to_send = String_MakeEndWith(headers_to_send, _T("\r\n"), false);
  }

  return CComBSTR(headers_to_send);
}

HRESULT UrlmonRequest::ProcessResponseHeaders(
                            const CComVariant& headers,
                            const CComSafeArray<DWORD>& headers_needed) {
  if (headers.vt != (VT_ARRAY | VT_BSTR)) {
    return E_FAIL;
  }

  CComSafeArray<BSTR> response_headers(headers.parray);
  DWORD count = response_headers.GetCount();
  if (count != headers_needed.GetCount()) {
    return E_FAIL;
  }

  response_headers_map_.clear();
  LONG lower_bound = response_headers.GetLowerBound();
  LONG upper_bound = response_headers.GetUpperBound();
  for (int i = lower_bound; i <= upper_bound; ++i) {
    response_headers_map_.insert(std::make_pair(headers_needed[i],
                                                response_headers[i]));
    NET_LOG(L3, (_T("[ProcessResponseHeaders][%d][%s]"),
                 i, response_headers[i]));
  }
  raw_response_headers_ = response_headers_map_[WINHTTP_QUERY_RAW_HEADERS_CRLF];
  return S_OK;
}

HRESULT UrlmonRequest::ProcessResponseFile(const CComBSTR& cache_filename) {
  if (!cache_filename.Length()) {
    // Response with no body.
    return S_OK;
  }

  if (!filename_.IsEmpty()) {
    // The caller expects the response to be stored in the target file.
    return File::Copy(cache_filename, filename_, true);
  }

  response_body_.clear();
  HRESULT hr = ReadEntireFileShareMode(cache_filename,
                                       0,
                                       FILE_SHARE_READ,
                                       &response_body_);
  if (FAILED(hr) || response_body_.empty()) {
    return hr;
  }
  return S_OK;
}

HRESULT UrlmonRequest::SendRequest(BSTR url,
                                   BSTR post_data,
                                   BSTR request_headers,
                                   VARIANT response_headers_needed,
                                   CComVariant* response_headers,
                                   DWORD* response_code,
                                   BSTR* cache_filename) {
  HRESULT hr = BindStatusCallback::CreateAndSend(url,
                                                 post_data,
                                                 request_headers,
                                                 response_headers_needed,
                                                 response_headers,
                                                 response_code,
                                                 cache_filename);
  NET_LOG(L3, (_T("[UrlmonRequest::SendRequest][0x%x][%d][%s]"),
               hr, *response_code, *cache_filename));
  if (!*response_code) {
    return FAILED(hr) ? hr : E_UNEXPECTED;
  }

  return S_OK;
}

HRESULT UrlmonRequest::Send() {
  NET_LOG(L3, (_T("[UrlmonRequest::Send]")));
  if (is_cancelled_) {
    return OMAHA_NET_E_REQUEST_CANCELLED;
  }

  ASSERT1(url_.Length() > 0);
  CComBSTR headers_to_send(BuildRequestHeaders(user_agent(),
                                               additional_headers_));
  CComBSTR post_data;
  if (request_buffer_) {
    post_data.AppendBytes(static_cast<const char*>(request_buffer_),
                          request_buffer_length_);
  }
  CComSafeArray<DWORD> headers_needed(arraysize(response_headers_needed));
  for (size_t i = 0; i < arraysize(response_headers_needed); ++i) {
    headers_needed.SetAt(i, response_headers_needed[i]);
  }
  CComVariant response_headers;
  CComBSTR cache_filename;

  HRESULT hr = SendRequest(url_,
                           post_data,
                           headers_to_send,
                           CComVariant(headers_needed),
                           &response_headers,
                           &http_status_code_,
                           &cache_filename);
  if (FAILED(hr)) {
    NET_LOG(LW, (_T("[UrlmonRequest::Send][0x%08x]"), hr));
    return hr;
  }

  VERIFY1(SUCCEEDED(ProcessResponseHeaders(response_headers, headers_needed)));
  VERIFY1(SUCCEEDED(ProcessResponseFile(cache_filename)));
  return S_OK;
}

// TODO(omaha): cancel the underlying IBindStatusCallback object.
HRESULT UrlmonRequest::Cancel() {
  NET_LOG(L2, (_T("[UrlmonRequest::Cancel]")));
  ::InterlockedExchange(&is_cancelled_, true);
  return S_OK;
}

}  // namespace omaha

