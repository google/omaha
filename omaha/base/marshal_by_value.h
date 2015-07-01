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
//
// This is an implementation of IMarshal that always marshals by value. class T
// needs to derive from MarshalByValue, and expose IMarshal through
// QueryInterface. class T also needs to expose the IPersistStream-style methods
// GetSizeMax(), Save(), and Load().

#ifndef OMAHA_BASE_MARSHAL_BY_VALUE_H_
#define OMAHA_BASE_MARSHAL_BY_VALUE_H_

#include <atlbase.h>
#include "omaha/base/debug.h"

namespace omaha {

template <class T>
class ATL_NO_VTABLE MarshalByValue : public IMarshal {
 public:
  STDMETHOD(GetUnmarshalClass)(REFIID, void*, DWORD, void*, DWORD,
                               CLSID* clsid) {
    ASSERT1(clsid);
    *clsid = T::GetObjectCLSID();
    return S_OK;
  }

  STDMETHOD(ReleaseMarshalData)(IStream* stream) {
    UNREFERENCED_PARAMETER(stream);
    return S_OK;
  }

  STDMETHOD(DisconnectObject)(DWORD) {
    return S_OK;
  }

  STDMETHOD(GetMarshalSizeMax)(REFIID, void*, DWORD, void*, DWORD,
                               DWORD* size) {
    ASSERT1(size);

    T* persist = static_cast<T*>(this);
    ULARGE_INTEGER size_max = {0};
    HRESULT hr = persist->GetSizeMax(&size_max);
    if (FAILED(hr)) {
      return hr;
    }

    *size = size_max.LowPart;
    return S_OK;
  }

  STDMETHOD(MarshalInterface)(IStream* stream, REFIID, void*, DWORD, void*,
                              DWORD) {
    ASSERT1(stream);

    T* persist = static_cast<T*>(this);
    return persist->Save(stream, FALSE);
  }

  STDMETHOD(UnmarshalInterface)(IStream* stream, REFIID iid, void** ptr) {
    ASSERT1(stream);
    ASSERT1(ptr);

    T* persist = static_cast<T*>(this);
    HRESULT hr = persist->Load(stream);
    if (FAILED(hr)) {
      return hr;
    }

    return persist->QueryInterface(iid, ptr);
  }
};

}  // namespace omaha

#endif  // OMAHA_BASE_MARSHAL_BY_VALUE_H_

