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
// product_data.h: Class encapsulates the hierarchy of products and components.

#ifndef OMAHA_WORKER_PRODUCT_DATA_H__
#define OMAHA_WORKER_PRODUCT_DATA_H__

#include <windows.h>
#include <atlstr.h>
#include <vector>
#include "base/basictypes.h"
#include "omaha/common/debug.h"
#include "omaha/worker/application_data.h"

namespace omaha {

// TODO(omaha):  This hierarchy pattern is similar between ProductData,
// AppRequest, UpdateResponse.  We should make a common class or a template to
// handle this instead of duplicating the pattern.
class ProductData {
 public:
  ProductData() {}
  explicit ProductData(const AppData& app_data) { app_data_ = app_data; }

  void set_app_data(const AppData& app_data) {
    app_data_ = app_data;
  }
  const AppData& app_data() const { return app_data_; }
  void AddComponent(const AppData& component_data) {
    components_.push_back(component_data);
  }

  AppDataVector::const_iterator components_begin() const {
    return components_.begin();
  }

  AppDataVector::const_iterator components_end() const {
    return components_.end();
  }

  size_t num_components() const { return components_.size(); }

 private:
  AppData app_data_;
  AppDataVector components_;
};

typedef std::vector<ProductData> ProductDataVector;

}  // namespace omaha.

#endif  // OMAHA_WORKER_PRODUCT_DATA_H__

