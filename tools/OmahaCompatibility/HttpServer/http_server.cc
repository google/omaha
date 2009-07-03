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


#include "omaha/tools/omahacompatibility/httpserver/http_server.h"

#include <Windows.h>
#include <http.h>
#include <winhttp.h>
#include "omaha/common/error.h"
#include "omaha/common/debug.h"
#include "omaha/common/logging.h"
#include "omaha/common/string.h"
#include "omaha/common/utils.h"
#include "omaha/common/reg_key.h"

namespace omaha {

#define MAX_BUFFER_SIZE 4048
#define INTERNET_MAX_PATH_LENGTH 2048

HttpServer::HttpServer(const CString& host, int port)
    : host_(host),
      port_(port) {
  CORE_LOG(L1, (_T("[HttpServer]")));
  url_prefix_.Format(_T("http://%s:%d"), host, port);
}

HttpServer::~HttpServer() {
  CORE_LOG(L1, (_T("[~HttpServer]")));
  ::HttpTerminate(HTTP_INITIALIZE_SERVER, NULL);

  std::map<CString, UrlHandler*>::iterator iter = handlers_.begin();
  for (; iter != handlers_.end(); ++iter) {
    std::pair<CString, UrlHandler*> pair = *iter;
    VERIFY1(::HttpRemoveUrl(get(request_handle_),
                            pair.first) != NO_ERROR);
    delete pair.second;
  }

  handlers_.clear();
}

HRESULT HttpServer::Initialize() {
  CORE_LOG(L1, (_T("[Initialize]")));
  HTTPAPI_VERSION version = HTTPAPI_VERSION_1;
  int ret = ::HttpInitialize(version,
                             HTTP_INITIALIZE_SERVER,
                             NULL);
  if (ret != NO_ERROR) {
    return HRESULT_FROM_WIN32(ret);
  }

  ret = ::HttpCreateHttpHandle(address(request_handle_), NULL);
  if (ret != NO_ERROR) {
    return HRESULT_FROM_WIN32(ret);
  }
  ASSERT1(get(request_handle_));

  return S_OK;
}

HRESULT HttpServer::AddUrlHandler(UrlHandler* handler) {
  CORE_LOG(L1, (_T("[AddUrlHandler]")));
  ASSERT1(handler);

  CString full_url = url_prefix_ + handler->get_url_path();
  int ret = ::HttpAddUrl(get(request_handle_),
                         full_url, NULL);
  if (ret != NO_ERROR) {
    return HRESULT_FROM_WIN32(ret);
  }

  ASSERT1(handlers_.find(handler->get_url_path()) == handlers_.end());
  handlers_[handler->get_url_path()] = handler;
  return S_OK;
}

UrlHandler* HttpServer::GetHandler(const HttpRequest& req) {
  std::map<CString, UrlHandler*>::const_iterator iter = handlers_.begin();
  for (; iter != handlers_.end(); ++iter) {
    UrlHandler* handler = (*iter).second;

    // Determine if the handler starts with the path that is specified
    // in the request.
    if (req.path().Find(handler->get_url_path()) == 0) {
      return handler;
    }
  }

  ASSERT1(false);
  return NULL;
}

HRESULT HttpServer::ReadRequest(HttpRequest* request) {
  ASSERT1(request);

  TCHAR buffer[MAX_BUFFER_SIZE] = {0};
  HTTP_REQUEST* req = reinterpret_cast<HTTP_REQUEST*>(buffer);
  int ret = ::HttpReceiveHttpRequest(get(request_handle_),
    HTTP_NULL_ID,
    HTTP_RECEIVE_REQUEST_FLAG_COPY_BODY,
    req,
    MAX_BUFFER_SIZE,
    NULL,
    NULL);

  if (ret != NO_ERROR) {
    HRESULT hr =  HRESULT_FROM_WIN32(ret);
    CORE_LOG(L1, (_T("[Failed to read request.][0x%08x]"), hr));
    return hr;
  }

  // Figure out the query string and the path.
  CString url_path(req->pRawUrl);
  int idx = url_path.ReverseFind(_T('?'));
  CString query_str;
  CString path;
  if (idx != -1) {
    query_str = url_path.Right(url_path.GetLength() - idx - 1);
    path = url_path.Left(idx);
  } else {
    path = url_path;
  }

  // Set the values on the request.
  request->set_http_verb(req->Verb);
  request->set_path(path);
  request->set_http_request(*req);
  request->set_query_str(query_str);

  if (req->Verb == HttpVerbPOST) {
    DWORD bytes_received = 0;
    char output_buffer[MAX_BUFFER_SIZE] = {0};
    ret = ::HttpReceiveRequestEntityBody(get(request_handle_),
      req->RequestId,
      0,
      output_buffer,
      MAX_BUFFER_SIZE,
      &bytes_received,
      NULL);

    if (ret != NO_ERROR) {
      HRESULT hr =  HRESULT_FROM_WIN32(ret);
      CORE_LOG(L1, (_T("[Failed to read request.][0x%08x]"), hr));
      return hr;
    }
    CString content = Utf8ToWideChar(output_buffer, bytes_received);
    request->set_content(content);
  }

  return S_OK;
}

HRESULT HttpServer::SetHeader(HTTP_RESPONSE* http_response,
                              HTTP_HEADER_ID header_id,
                              const char* value) {
  ASSERT1(http_response);

  http_response->Headers.KnownHeaders[header_id].pRawValue = value;
  http_response->Headers.KnownHeaders[header_id].RawValueLength =
        static_cast<USHORT>(strlen(value));
  return S_OK;
}

HRESULT HttpServer::SendResponse(const HttpResponse& response) {
  const HttpRequest& request = response.request();
  const char* const kOk = "OK";

  HTTP_RESPONSE http_response = {0};
  http_response.StatusCode = 200;

  // Send back an ok.
  http_response.pReason = kOk;
  http_response.ReasonLength = static_cast<USHORT>(strlen(kOk));

  // No special headers for now.
  HTTP_RESPONSE_HEADERS headers = {0};
  headers.UnknownHeaderCount = 0;
  headers.TrailerCount = 0;
  http_response.Headers = headers;

  scoped_hfile handle;
  CStringA request_body_utf8;
  CStringA content_range_header;
  int http_response_flag = 0;

  HTTP_DATA_CHUNK chunk;
  memset(&chunk, 0, sizeof(HTTP_DATA_CHUNK));

  // Based on the verb build a response.
  if (request.http_verb() == HttpVerbPOST) {
    // Add a known header of content type.
    SetHeader(&http_response, HttpHeaderContentType, "text/xml; charset=UTF-8");

    // Send back the content.
    chunk.DataChunkType = HttpDataChunkFromMemory;
    CString response_str = response.response_str();
    request_body_utf8 = CT2A(response_str, CP_UTF8);

    chunk.FromMemory.pBuffer = request_body_utf8.GetBuffer();
    chunk.FromMemory.BufferLength = request_body_utf8.GetLength();

    http_response.EntityChunkCount = 1;
    http_response.pEntityChunks = &chunk;

  } else if (request.http_verb() == HttpVerbHEAD) {
    // Add the content length, type, and content range headers.
    SetHeader(&http_response, HttpHeaderContentType,
              "application/octet-stream");
    char size_str[MAX_PATH] = {0};
    _itoa(response.size(), size_str, 10);
    SetHeader(&http_response, HttpHeaderContentLength, size_str);
    SetHeader(&http_response, HttpHeaderAcceptRanges, "bytes");

  } else if (request.http_verb() == HttpVerbGET) {
    SetHeader(&http_response, HttpHeaderContentType,
              "application/octet-stream");
    char size_str[MAX_PATH] = {0};
    _itoa(response.size(), size_str, 10);
    SetHeader(&http_response, HttpHeaderAcceptRanges, "bytes");

    reset(handle, ::CreateFile(response.file_name(), GENERIC_READ,
                               FILE_SHARE_READ, NULL, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, NULL));
    if (!handle) {
      return HRESULTFromLastError();
    }

    chunk.DataChunkType = HttpDataChunkFromFileHandle;

    // Determine the file portion to send back to the client.
    // If the client has sent a range request, honor that.
    int start_file_pos = 0;
    int end_file_pos = 0;
    HTTP_REQUEST tmp_request = request.http_request();
    if (tmp_request.Headers.KnownHeaders[HttpHeaderRange].RawValueLength != 0) {
      // The client send a content length header. We need to honor this.
      CStringA content_range(
          tmp_request.Headers.KnownHeaders[HttpHeaderRange].pRawValue);

      int idx = content_range.Find('-');
      ASSERT1(idx != -1);
      end_file_pos = atoi(content_range.Right(
            content_range.GetLength() - idx - 1));
      int st_idx = content_range.Find('=');
      ASSERT1(st_idx != -1);
      start_file_pos = atoi(content_range.Mid(st_idx + 1, idx));

      // Set the Content-range header in the response.
      content_range_header.Format("bytes %d-%d/%d",
                                  start_file_pos,
                                  end_file_pos, response.size());
      SetHeader(&http_response, HttpHeaderContentRange, content_range_header);

      // Since this is a range request, we set the http response code to
      // partial response.
      http_response.StatusCode = 206;

      // Set the value of the human readable text to partial-content.
      const char* const kPartialContent = "Partial Content";
      http_response.pReason = kPartialContent;
      http_response.ReasonLength = static_cast<USHORT>(strlen(kPartialContent));
    }

    // Send back the entire file or part of it.
    HTTP_BYTE_RANGE byte_range = {0};
    byte_range.StartingOffset.HighPart = 0;
    byte_range.StartingOffset.LowPart = start_file_pos;
    if (end_file_pos == 0) {
      byte_range.Length.QuadPart = HTTP_BYTE_RANGE_TO_EOF;
    } else {
      byte_range.Length.QuadPart = end_file_pos - start_file_pos + 1;
    }

    chunk.FromFileHandle.ByteRange = byte_range;
    chunk.FromFileHandle.FileHandle = get(handle);

    http_response.EntityChunkCount = 1;
    http_response.pEntityChunks = &chunk;
  }

  DWORD bytes_sent = 0;
  int ret = ::HttpSendHttpResponse(
      get(request_handle_),
      request.http_request().RequestId,
      http_response_flag,
      &http_response,
      NULL,
      &bytes_sent,
      NULL,
      0,
      NULL,
      NULL);
  if (ret != NO_ERROR) {
    return HRESULT_FROM_WIN32(ret);
  }

  return S_OK;
}

HRESULT HttpServer::Start() {
  CORE_LOG(L1, (_T("[Start]")));

  while (true) {
    HttpRequest request;
    HRESULT hr = ReadRequest(&request);
    if (FAILED(hr)) {
      CORE_LOG(L1, (_T("[ReadRequest failed.][0x%08x]"), hr));
      continue;
    }

    HttpResponse response(request);
    UrlHandler* handler = GetHandler(request);
    if (handler == NULL) {
      continue;
    }

    hr = handler->HandleRequest(request, &response);
    if (FAILED(hr)) {
      CORE_LOG(L1, (_T("[HandlerRequest failed.][0x%08x]"), hr));
      continue;
    }

    hr = SendResponse(response);
    if (FAILED(hr)) {
      CORE_LOG(L1, (_T("[SendRequest failed.][0x%08x]"), hr));
      continue;
    }
  }

  return S_OK;
}

}  // namespace omaha
