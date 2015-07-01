// Copyright 2010 Google Inc.
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

#ifndef OMAHA_GOOPDATE_UPDATE_REQUEST_UTILS_H_
#define OMAHA_GOOPDATE_UPDATE_REQUEST_UTILS_H_

#include <windows.h>
#include "omaha/common/update_request.h"

namespace omaha {

class App;

namespace update_request_utils {

// TODO(omaha): missing unit test.
//
// Builds an UpdateRequest object by adding an xml request corresponding to
// the App object passed as a parameter. This function avoids creating an empty
// element in the cases where an update checks is not requested and no ping
// events exist.
void BuildRequest(const App* app,
                  bool is_update_check,
                  xml::UpdateRequest* update_request);

}  // namespace update_request_utils

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_UPDATE_REQUEST_UTILS_H_

