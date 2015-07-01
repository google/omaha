// Copyright 2005-2009 Google Inc.
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
// This class is similar to CComContainedObject. The primary difference is with
// respect to QueryInterface, which this class handles itself, as opposed to
// CComContainedObject that delegates that to the outer unknown.

#ifndef OMAHA_COMMON_CONTROLLED_OBJECT_H_
#define OMAHA_COMMON_CONTROLLED_OBJECT_H_

#include <atlcom.h>
#include "base/scoped_ptr.h"
#include "omaha/base/debug.h"
#include "omaha/base/logging.h"
#include "omaha/base/scoped_ptr_address.h"

namespace omaha {

template <class Base>
class ControlledObject : public CComContainedObject<Base> {
 public:
  typedef CComContainedObject<Base> BaseClass;
  typedef ControlledObject<Base> ControlledObj;

  // The base class CComContainedObject stores pv, and delegates to this
  // controlling unknown subsequent calls to the lifetime methods
  // AddRef/Release.
  explicit ControlledObject(void* pv) : BaseClass(pv) {}
  virtual ~ControlledObject() {}

  STDMETHOD(QueryInterface)(REFIID iid, void** ppv) throw() {
    return _InternalQueryInterface(iid, ppv);
  }

  // TODO(omaha): ASSERT on controlling_unknown. The unit tests need to be
  // fixed for this.
  static HRESULT WINAPI CreateInstance(IUnknown* controlling_unknown,
                                       ControlledObj** pp) throw() {
    ASSERT1(pp);
    if (!controlling_unknown) {
      CORE_LOG(LW, (_T("[CreateInstance - controlling_unknown is NULL]")));
    }

    *pp = NULL;
    scoped_ptr<ControlledObj> p(new ControlledObj(controlling_unknown));
    if (!p.get()) {
      return E_OUTOFMEMORY;
    }

    HRESULT hr = p->FinalConstruct();
    if (FAILED(hr)) {
      return hr;
    }

    *pp = p.release();
    return S_OK;
  }

  template <class Q>
  HRESULT STDMETHODCALLTYPE QueryInterface(Q** pp) throw() {
    return QueryInterface(__uuidof(Q), reinterpret_cast<void**>(pp));
  }
};

}  // namespace omaha

#endif  // OMAHA_COMMON_CONTROLLED_OBJECT_H_

