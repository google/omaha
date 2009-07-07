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


#ifndef OMAHA_TOOLS_SRC_OMAHACOMPATIBILITY_HTTPSERVER_URL_HANDLER_H_
#define OMAHA_TOOLS_SRC_OMAHACOMPATIBILITY_HTTPSERVER_URL_HANDLER_H_

#include <windows.h>
#include <atlstr.h>
#include <vector>
#include "omaha/tools/omahacompatibility/httpserver/request.h"
#include "omaha/tools/omahacompatibility/httpserver/response.h"

namespace omaha {

// Base class that represents a handler for a path to the HttpServer.
class UrlHandler {
 public:
  explicit UrlHandler(const CString& url_path) : url_path_(url_path) {
    url_path_.MakeLower();
  }
  virtual ~UrlHandler() {}
  CString get_url_path() const { return url_path_; }
  virtual HRESULT HandleRequest(const HttpRequest& request,
                                HttpResponse* response) = 0;

 private:
  CString url_path_;
};

}  // namespace omaha

#endif  // OMAHA_TOOLS_SRC_OMAHACOMPATIBILITY_HTTPSERVER_URL_HANDLER_H_
