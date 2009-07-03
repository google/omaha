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
// app_request.h:  Class that encapsulates a product-level request for
// install/update that is converted into the XML that goes to the server.  Will
// hold one AppRequestData for the main product and 0..n AppRequestData objects
// in a list for the components of the product that we know/care about.

#ifndef OMAHA_WORKER_APP_REQUEST_H__
#define OMAHA_WORKER_APP_REQUEST_H__

#include <windows.h>
#include <atlstr.h>
#include <vector>
#include "base/basictypes.h"
#include "omaha/common/debug.h"
#include "omaha/worker/app_request_data.h"

namespace omaha {

class AppRequest {
 public:
  AppRequest() {}
  explicit AppRequest(const AppRequestData& request_data) {
    request_data_ = request_data;
  }
  ~AppRequest() {}

  void set_request_data(const AppRequestData& request_data) {
    request_data_ = request_data;
  }
  const AppRequestData& request_data() const { return request_data_; }
  void AddComponentRequest(const AppRequestData& component_data) {
    components_.push_back(component_data);
  }

  AppRequestDataVector::const_iterator components_begin() const {
    return components_.begin();
  }

  AppRequestDataVector::const_iterator components_end() const {
    return components_.end();
  }

  size_t num_components() const { return components_.size(); }

 private:
  AppRequestData request_data_;
  AppRequestDataVector components_;
};

typedef std::vector<AppRequest> AppRequestVector;

}  // namespace omaha.

#endif  // OMAHA_WORKER_APP_REQUEST_H__

