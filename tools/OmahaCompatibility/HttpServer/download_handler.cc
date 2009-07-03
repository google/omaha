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


#include "omaha/tools/omahacompatibility/httpserver/download_handler.h"
#include "omaha/common/debug.h"
#include "omaha/common/logging.h"
#include "omaha/common/path.h"

namespace omaha {

HRESULT DownloadHandler::HandleRequest(const HttpRequest& request,
                                       HttpResponse* response) {
  CORE_LOG(L1, (_T("[DownloadHandler::HandleRequest]")));
  ASSERT1(response);

  for (size_t i = 0; i < responses_.size(); ++i) {
    CString requested_file = GetFileFromPath(request.path());
    if (GetFileFromPath(responses_[i].local_file_name) == requested_file) {
      // We know about the file that was asked for.
      response->set_size(responses_[i].size);
      response->set_file_name(responses_[i].local_file_name);
      return S_OK;
    }
  }

  return S_OK;
}

HRESULT DownloadHandler::AddDownloadFile(const ConfigResponse& response) {
  CORE_LOG(L1, (_T("[DownloadHandler::AddDownloadFile]")));
  responses_.push_back(response);
  return S_OK;
}

}  // namespace omaha
