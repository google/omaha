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

#ifndef OMAHA_TOOLS_SRC_OMAHACOMPATIBILITY_HTTPSERVER_HTTP_SERVER_H_
#define OMAHA_TOOLS_SRC_OMAHACOMPATIBILITY_HTTPSERVER_HTTP_SERVER_H_

#include <windows.h>
#include <map>
#include "omaha/common/scoped_any.h"
#include "omaha/tools/omahacompatibility/httpserver/url_handler.h"
#include "omaha/tools/omahacompatibility/httpserver/request.h"

namespace omaha {

// The base http server, it takes in a bunch of handlers for
// the appropriate paths and invokes them based on the requests
// that are sent to the server. This is NOT a general purpose
// server, and handles only the requests necessary to enable
// omaha client to function correctly.
class HttpServer {
 public:
  HttpServer(const CString& host, int port);
  ~HttpServer();
  HRESULT Initialize();
  HRESULT AddUrlHandler(UrlHandler* handler);
  HRESULT Start();

 private:
  HRESULT Terminate();
  UrlHandler* GetHandler(const HttpRequest& url);
  HRESULT ReadRequest(HttpRequest* request);
  HRESULT SendResponse(const HttpResponse& response);
  HRESULT SetHeader(HTTP_RESPONSE* http_response,
                    HTTP_HEADER_ID header_id,
                    const char* value);

  scoped_handle request_handle_;
  std::map<CString, UrlHandler*> handlers_;
  CString host_;
  CString url_prefix_;
  int port_;
};

}  // namespace omaha

#endif  //  OMAHA_TOOLS_SRC_OMAHACOMPATIBILITY_HTTPSERVER_HTTP_SERVER_H_
