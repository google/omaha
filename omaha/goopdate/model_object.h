// Copyright 2009 Google Inc.
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

// Defines the base class of classes in the model. Provides access to the root
// of the model. The ModelObject locks and unlocks the ATL module in the
// constructor and destructor respectively. The ATL Module needs to be around
// until all the model objects are destroyed.

#ifndef OMAHA_GOOPDATE_MODEL_OBJECT_H_
#define OMAHA_GOOPDATE_MODEL_OBJECT_H_

#include <windows.h>
#include "base/basictypes.h"
#include "omaha/base/debug.h"
#include "omaha/base/utils.h"

namespace omaha {

class Model;

bool IsModelLockedByCaller(const Model* model);

class ModelObject {
 public:
  Model* model() {
    return omaha::interlocked_exchange_pointer(&model_, model_);
  }

  const Model* model() const {
    return omaha::interlocked_exchange_pointer(&model_, model_);
  }

 protected:
  explicit ModelObject(Model* model) : model_(NULL) {
    ASSERT1(model);
    ASSERT1(IsModelLockedByCaller(model));

    _pAtlModule->Lock();
    omaha::interlocked_exchange_pointer(&model_, model);
  }

  ~ModelObject() {
    omaha::interlocked_exchange_pointer(&model_, static_cast<Model*>(NULL));
    _pAtlModule->Unlock();
  }

 private:
  // C++ root of the object model. Not owned by this instance.
  mutable Model* volatile model_;

  DISALLOW_COPY_AND_ASSIGN(ModelObject);
};

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_MODEL_OBJECT_H_

