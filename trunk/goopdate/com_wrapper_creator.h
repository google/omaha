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

// TODO(omaha): rename the file to match the name of the class.

#ifndef OMAHA_GOOPDATE_COM_WRAPPER_CREATOR_H_
#define OMAHA_GOOPDATE_COM_WRAPPER_CREATOR_H_

#include <windows.h>
#include <atlbase.h>
#include <atlcom.h>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/scoped_ptr_address.h"
#include "base/utils.h"
#include "goopdate/omaha3_idl.h"
#include "omaha/goopdate/model_object.h"
#include "third_party/bar/shared_ptr.h"

namespace omaha {

class AppBundle;
typedef shared_ptr<AppBundle> ControllingPtr;

// Generalizes the creation of COM wrappers for a given class T.
// It requires:
//   * The wrapper class TWrapper derives from ComWrapper
//   * The wrapped class T provides access to model instance
template <typename TWrapper, typename T>
class ComWrapper : public CComObjectRootEx<CComObjectThreadModel> {
 public:

  static HRESULT Create(const ControllingPtr& controlling_ptr,
                        T* t, IDispatch** t_wrapper) {
    ASSERT1(t);
    ASSERT1(t_wrapper);

    ASSERT1(IsModelLockedByCaller(t->model()));

    scoped_ptr<TComObject> t_com_object;
    HRESULT hr = TComObject::CreateInstance(address(t_com_object));
    if (FAILED(hr)) {
      return hr;
    }

    hr = t_com_object->QueryInterface(t_wrapper);
    if (FAILED(hr)) {
      return hr;
    }

    t_com_object->model_ = t->model();
    t_com_object->controlling_ptr_ = controlling_ptr;
    t_com_object->wrapped_obj_ = t;

    t_com_object.release();
    return S_OK;
  }

 protected:
  ComWrapper() : model_(NULL), wrapped_obj_(NULL) {}

  ~ComWrapper() {}

  void FinalRelease() {
    controlling_ptr_.reset();
    wrapped_obj_ = NULL;
  }

  const Model* model() const {
    return omaha::interlocked_exchange_pointer(&model_, model_);
  }

  const ControllingPtr& controlling_ptr() const {
    ASSERT1(IsModelLockedByCaller(wrapped_obj_->model()));
    return controlling_ptr_;
  }

  T* wrapped_obj() {
    return omaha::interlocked_exchange_pointer(&wrapped_obj_, wrapped_obj_);
  }

 private:

  typedef CComObject<TWrapper> TComObject;

  // The pointer is written and read from multiple threads and it is written
  // to with the same value by the atomic pointer exchange, hence the volatile
  // and mutable cv qualifiers respectively.
  mutable Model* volatile model_;

  ControllingPtr controlling_ptr_;

  T* wrapped_obj_;

  DISALLOW_COPY_AND_ASSIGN(ComWrapper);
};

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_COM_WRAPPER_CREATOR_H_

