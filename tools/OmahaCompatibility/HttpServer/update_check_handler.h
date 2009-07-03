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

#ifndef OMAHA_TOOLS_SRC_OMAHACOMPATIBILITY_HTTPSERVER_UPDATE_CHECK_HANDLER_H_
#define OMAHA_TOOLS_SRC_OMAHACOMPATIBILITY_HTTPSERVER_UPDATE_CHECK_HANDLER_H_

#include <windows.h>
#include <vector>
#include <atlstr.h>
#include "omaha/goopdate/request.h"
#include "omaha/tools/omahacompatibility/common/config.h"
#include "omaha/tools/omahacompatibility/common/ping_observer.h"
#include "omaha/tools/omahacompatibility/httpserver/request.h"
#include "omaha/tools/omahacompatibility/httpserver/response.h"
#include "omaha/tools/omahacompatibility/httpserver/url_handler.h"
#include "omaha/tools/omahacompatibility/httpserver/xml_parser.h"

namespace omaha {

// Handles the update check requests.
// Initially the configuration information that is used to respond
// to the requests is simple and only contains a few fields: guid and version.
//
// Future: Implement reading the server ascii protocol buffers, and use
// that as the configuration information.
class UpdateCheckHandler : public UrlHandler {
 public:
  UpdateCheckHandler(const CString& url_path, PingObserver* observer);
  virtual ~UpdateCheckHandler() {}

  HRESULT AddAppVersionResponse(const ConfigResponse& response);
  virtual HRESULT HandleRequest(const HttpRequest& request,
                                HttpResponse* response);
 private:
  // Returns the response structure that matches the guid and version
  // specified.
  HRESULT FindResponse(GUID guid,
                       const CString& version,
                       const CString& ap,
                       ConfigResponse* response);
  HRESULT BuildResponse(const AppRequestDataVector& request,
                        ServerResponses* responses);
  std::vector<ConfigResponse> config_responses_;
  PingObserver* ping_observer_;
};

}  // namespace omaha

#endif  // OMAHA_TOOLS_SRC_OMAHACOMPATIBILITY_HTTPSERVER_UPDATE_CHECK_HANDLER_H_
