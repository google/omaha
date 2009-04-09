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

#include "omaha/tools/omahacompatibility/httpserver/update_check_handler.h"
#include <windows.h>
#include "omaha/common/debug.h"
#include "omaha/common/logging.h"
#include "omaha/goopdate/goopdate_utils.h"
#include "omaha/tools/omahacompatibility/common/error.h"
#include "omaha/tools/omahacompatibility/httpserver/request.h"
#include "omaha/tools/omahacompatibility/httpserver/xml_parser.h"

namespace omaha {

UpdateCheckHandler::UpdateCheckHandler(const CString& url_path,
                                       PingObserver* observer)
    : UrlHandler(url_path),
      ping_observer_(observer) {
  CORE_LOG(L1, (_T("[UpdateCheckHandler]")));
}

// Private method that goes over the request, i.e. the update checks or
// the pings for each application and constructs an appropriate response.
// For pings we dont do anything, and for update checks we construct the
// update response based on the config information.
HRESULT UpdateCheckHandler::BuildResponse(const AppRequestDataVector& request,
                                          ServerResponses* responses) {
  ASSERT1(responses);

  for (size_t i = 0; i < request.size(); ++i) {
    AppData app_data = request[i].app_data();
    ServerResponse response;
    response.guid = GuidToString(app_data.app_guid());

    if (request[i].num_ping_events() > 0) {
      // This is a ping request. We only handle the ping and nothing else.
      response.is_ping = true;
      if (ping_observer_) {
        ping_observer_->Observe(request[i]);
      }
    } else {
      // We assume this is an update check and send back an appropriate
      // response.
      ConfigResponse config_app_response;
      HRESULT hr = FindResponse(app_data.app_guid(),
                                app_data.version(),
                                app_data.ap(),
                                &config_app_response);
      if (SUCCEEDED(hr)) {
        response.is_update_response = true;
        response.response_data.set_url(config_app_response.url);
        response.response_data.set_hash(config_app_response.hash);
        response.response_data.set_needs_admin(config_app_response.needs_admin ?
                                               omaha::NEEDS_ADMIN_YES :
                                               omaha::NEEDS_ADMIN_NO);
        response.response_data.set_size(config_app_response.size);
      }
      // Continuing here in the failed case will cause the
      // is_update_response to be false,
      // causing the server to respond with no-update.
    }
    responses->push_back(response);
  }

  return S_OK;
}

HRESULT UpdateCheckHandler::HandleRequest(const HttpRequest& http_request,
                                          HttpResponse* response) {
  CORE_LOG(L1, (_T("[UpdateCheckHandler::HandleRequest]")));
  ASSERT1(response);
  ASSERT1(http_request.http_verb() == HttpVerbPOST);

  // Parse the request.
  CORE_LOG(L1, (_T("[Request %s]"), http_request.content()));
  AppRequestDataVector request;
  HRESULT hr = ParseUpdateCheck(http_request.content(), &request);
  if (FAILED(hr)) {
    CORE_LOG(LW, (_T("[ParseUpdateCheck failed]")));
    return hr;
  }

  // Map the request to a response.
  ServerResponses responses;
  hr = BuildResponse(request, &responses);
  if (FAILED(hr)) {
    return hr;
  }

  // Convert the response into the response string.
  CString update_response_str;
  hr = BuildUpdateResponse(responses, &update_response_str);
  if (FAILED(hr)) {
    CORE_LOG(LW, (_T("[BuildUpdateResponse failed]")));
    return hr;
  }

  CORE_LOG(L1, (_T("[Response %s]"), update_response_str));
  response->set_response_str(update_response_str);

  return S_OK;
}

HRESULT UpdateCheckHandler::AddAppVersionResponse(
    const ConfigResponse& response) {
  CORE_LOG(L1, (_T("[UpdateCheckHandler::AddAppVersionResponse]")));
  // TODO(omaha): Add duplicate checking.

  config_responses_.push_back(response);
  return S_OK;
}

HRESULT UpdateCheckHandler::FindResponse(GUID guid,
                                         const CString& version,
                                         const CString& ap,
                                         ConfigResponse* response) {
  CORE_LOG(L1, (_T("[UpdateCheckHandler::FindResponse]")));
  ASSERT1(response);

  // The idea here is that we go over the config responses, for a request
  // that contains 0.0.0.0 in the request, or empty we return the initial
  // response, i.e. the first installer we read.
  // If the version is not empty, then we match it with the previous config
  // version, i.e. the version that should have been installed,
  // and return a response if there is a match.
  if (version.IsEmpty() || version == _T("0.0.0.0")) {
    // This is the initial response.
    *response = config_responses_[0];
    return S_OK;
  }

  for (size_t i = 1; i < config_responses_.size(); ++i) {
    if (::IsEqualGUID(config_responses_[i].guid, guid) &&
        _T("update_app") == ap &&
        version == config_responses_[i - 1].version) {
      // This is the second response.
      *response = config_responses_[i];
      return S_OK;
    }
  }

  return E_FAIL;
}

}  // namespace omaha
