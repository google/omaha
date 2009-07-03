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

#ifndef OMAHA_TOOLS_SRC_OMAHACOMPATIBILITY_HTTPSERVER_DOWNLOAD_HANDLER_H_
#define OMAHA_TOOLS_SRC_OMAHACOMPATIBILITY_HTTPSERVER_DOWNLOAD_HANDLER_H_

#include <windows.h>
#include <atlstr.h>
#include "omaha/tools/omahacompatibility/common/config.h"
#include "omaha/tools/omahacompatibility/httpserver/request.h"
#include "omaha/tools/omahacompatibility/httpserver/response.h"
#include "omaha/tools/omahacompatibility/httpserver/url_handler.h"

namespace omaha {

// Handler for the download path on the server. Returns the size and the
// local path of the file to be returned for the request.
class DownloadHandler : public UrlHandler {
 public:
  explicit DownloadHandler(const CString& url_path) : UrlHandler(url_path) {}
  virtual ~DownloadHandler() {}
  HRESULT AddDownloadFile(const ConfigResponse& response);
  virtual HRESULT HandleRequest(const HttpRequest& request,
                                HttpResponse* response);
 private:
  ConfigResponses responses_;
};

}  // namespace omaha

#endif  // OMAHA_TOOLS_SRC_OMAHACOMPATIBILITY_HTTPSERVER_DOWNLOAD_HANDLER_H_
