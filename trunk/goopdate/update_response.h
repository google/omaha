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
//
// update_response.h:  The hierarchical response for a product and its
// components from a server request for install/update.

#ifndef OMAHA_GOOPDATE_UPDATE_RESPONSE_H__
#define OMAHA_GOOPDATE_UPDATE_RESPONSE_H__

#include <windows.h>
#include <atlstr.h>
#include <map>
#include "base/basictypes.h"
#include "omaha/common/debug.h"
#include "omaha/common/utils.h"
#include "omaha/goopdate/update_response_data.h"

namespace omaha {

class UpdateResponse {
 public:
  UpdateResponse() {}
  explicit UpdateResponse(const UpdateResponseData& response_data) {
    response_data_ = response_data;
  }
  ~UpdateResponse() {}

  void set_update_response_data(const UpdateResponseData& response_data) {
    response_data_ = response_data;
  }

  const UpdateResponseData& update_response_data() const {
    return response_data_;
  }

  // Adds the component. Returns false if the component already exists, and
  // does not overwrite the value.
  bool AddComponentResponseData(const UpdateResponseData& component_data) {
    std::pair<GUID, UpdateResponseData> data(component_data.guid(),
                                             component_data);
    return components_.insert(data).second;
  }

  UpdateResponseDatas::const_iterator components_begin() const {
    return components_.begin();
  }

  UpdateResponseDatas::const_iterator components_end() const {
    return components_.end();
  }

  bool IsComponentPresent(const GUID& guid) const {
    return components_.find(guid) != components_.end();
  }

  const UpdateResponseData& GetComponentData(const GUID& guid) const {
    UpdateResponseDatas::const_iterator iter = components_.find(guid);
    ASSERT1(iter != components_.end());
    return (*iter).second;
  }

  size_t num_components() const { return components_.size(); }

 private:
  UpdateResponseData response_data_;
  UpdateResponseDatas components_;
};

// Map of UpdateResponses, key=Guid, value=UpdateResponse.
typedef std::map<GUID, UpdateResponse, GuidComparer> UpdateResponses;

}  // namespace omaha.

#endif  // OMAHA_GOOPDATE_UPDATE_RESPONSE_H__

