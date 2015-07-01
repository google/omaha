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

#ifndef OMAHA_GOOPDATE_UPDATE_RESPONSE_UTILS_H_
#define OMAHA_GOOPDATE_UPDATE_RESPONSE_UTILS_H_

#include <windows.h>
#include <vector>
#include "omaha/common/update_response.h"

namespace omaha {

class App;

namespace update_response_utils {

const xml::response::App* GetApp(const xml::response::Response& response,
                                 const CString& appid);

// Checks the status of the "untrusted" data element. Returns S_OK if the status
// is "ok" or GOOPDATEINSTALL_E_INVALID_UNTRUSTED_DATA otherwise.
HRESULT ValidateUntrustedData(const std::vector<xml::response::Data>& data);

// Retrieves the install_data string corresponding to the install_data_index
// in the response data object. Returns an error if the status of the data
// object is not ok or the index is not found.
HRESULT GetInstallData(const std::vector<xml::response::Data>& data,
                       const CString& index,
                       CString* value);

// Builds an App object from its corresponding representation in the
// update response.
HRESULT BuildApp(const xml::UpdateResponse* update_response,
                 HRESULT code,
                 App* app);

// Returns the result of the update response for an app. The string member of
// the result is formatted in the specified language and could include
// the value of the |app_name| parameter in some cases.
xml::UpdateResponseResult GetResult(const xml::UpdateResponse* update_response,
                                    const CString& appid,
                                    const CString& app_name,
                                    const CString& language);

// Returns true if the update response contains an update for Omaha.
bool IsOmahaUpdateAvailable(const xml::UpdateResponse* update_response);

// Extracts a set of experiment label deltas from a response, merges them with
// existing labels in the Registry, and writes the resulting set back.
HRESULT ApplyExperimentLabelDeltas(bool is_machine,
                                   const xml::UpdateResponse* update_response);

}  // namespace update_response_utils

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_UPDATE_RESPONSE_UTILS_H_

