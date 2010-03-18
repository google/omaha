// Copyright 2007-2010 Google Inc.
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
// goopdate server request

#include "omaha/goopdate/request.h"
#include <stdlib.h>
#include <vector>
#include "omaha/common/commontypes.h"
#include "omaha/common/debug.h"
#include "omaha/common/logging.h"
#include "omaha/common/omaha_version.h"
#include "omaha/goopdate/config_manager.h"
#include "omaha/goopdate/goopdate_utils.h"

namespace omaha {

Request::Request(bool is_machine)
    : is_machine_(is_machine),
      version_(GetVersionString()),
      test_source_(ConfigManager::Instance()->GetTestSource()) {
  GUID req_id = GUID_NULL;
  VERIFY1(SUCCEEDED(::CoCreateGuid(&req_id)));
  request_id_ = GuidToString(req_id);
  VERIFY1(SUCCEEDED(goopdate_utils::GetOSInfo(&os_version_,
                                              &os_service_pack_)));
}

Request::~Request() {
}

void Request::AddAppRequest(const AppRequest& app_request) {
  app_requests_.push_back(app_request);
}

}  // namespace
