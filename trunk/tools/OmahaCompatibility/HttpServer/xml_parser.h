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

#ifndef OMAHA_TOOLS_SRC_OMAHACOMPATIBILITY_HTTPSERVER_XML_PARSER_H_
#define OMAHA_TOOLS_SRC_OMAHACOMPATIBILITY_HTTPSERVER_XML_PARSER_H_

#include <vector>
#include "omaha/goopdate/request.h"
#include "omaha/goopdate/update_response_data.h"
#include "omaha/goopdate/update_response.h"
#include "omaha/worker/app_request.h"

namespace omaha {

// Represents the server response to a get, if the request is a ping,
// the server just responds with a ok. For now we only support
// responses for update checks with update or no-update. Later on
// extend the is_update_response to be a enum
struct ServerResponse {
  ServerResponse()
    : is_ping(false),
      is_update_response(false) {}

  bool is_ping;
  bool is_update_response;
  CString guid;
  UpdateResponseData response_data;
};

typedef std::vector<ServerResponse> ServerResponses;

HRESULT ParseUpdateCheck(const CString& post_string,
                         AppRequestDataVector* request);

HRESULT BuildUpdateResponse(const ServerResponses& update_response,
                            CString* response);

}  // namespace omaha

#endif  // OMAHA_TOOLS_SRC_OMAHACOMPATIBILITY_HTTPSERVER_XML_PARSER_H_
