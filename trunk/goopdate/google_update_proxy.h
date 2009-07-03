// Copyright 2007-2009 Google Inc.
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
// Defines SharedMemoryProxy to encapsulate marshaling and unmarshaling of
// IGoogleUpdate and other interface pointers across process boundaries.
//
// TODO(omaha): seems possible to make it general purpose and move it to common.
#ifndef OMAHA_GOOPDATE_GOOGLE_UPDATE_PROXY_H__
#define OMAHA_GOOPDATE_GOOGLE_UPDATE_PROXY_H__

#include <atlbase.h>
#include <atlsecurity.h>
#include "base/basictypes.h"
#include "goopdate/google_update_idl.h"
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/logging.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/shared_memory_ptr.h"
#include "omaha/common/utils.h"
#include "omaha/common/vistautil.h"

namespace omaha {

// Constants
const size_t kMaxSizeInterfaceMarshalData = 256;
const TCHAR* const kBrowserHttpRequestShareName = _T("IBrowserRequest2_");

struct InterfaceMarshalData {
  void InitializeSharedData(const CString&) {
    SetZero(data_);
    size_ = 0;
  }
  size_t size_;
  uint8 data_[kMaxSizeInterfaceMarshalData];
};

class SharedMemoryAttributes {
 public:
  SharedMemoryAttributes(const TCHAR* shared_memory_name,
                         const CSecurityDesc& security_attributes)
      : shared_memory_name_(shared_memory_name),
        security_attributes_(security_attributes) {
  }
  const CString& GetSharedMemoryName() { return shared_memory_name_; }
  LPSECURITY_ATTRIBUTES GetSecurityAttributes() {
    return &security_attributes_;
  }

 private:
  CString shared_memory_name_;
  CSecurityAttributes security_attributes_;
  DISALLOW_EVIL_CONSTRUCTORS(SharedMemoryAttributes);
};

extern SharedMemoryAttributes low_integrity_attributes;
extern SharedMemoryAttributes high_integrity_attributes;

template <typename InterfaceType, typename LockType>
class SharedMemoryProxy {
 public:
  SharedMemoryProxy(bool read_only, SharedMemoryAttributes* attributes)
      : shared_memory_ptr_(attributes->GetSharedMemoryName(),
                           attributes->GetSecurityAttributes(),
                           NULL,
                           read_only) {
    CORE_LOG(L3, (_T("[SharedMemoryProxy::SharedMemoryProxy]")));
  }

  ~SharedMemoryProxy() {
    CORE_LOG(L3, (_T("[SharedMemoryProxy::~SharedMemoryProxy]")));
  }

  HRESULT GetObject(InterfaceType** interface_ptr) {
    ASSERT1(interface_ptr);
    ASSERT1(*interface_ptr == NULL);

    CORE_LOG(L3, (_T("[SharedMemoryProxy::GetObject]")));

    return UnmarshalInterface(interface_ptr);
  }

  HRESULT RegisterObject(InterfaceType* interface_ptr) {
    ASSERT1(interface_ptr);
    CORE_LOG(L3, (_T("[SharedMemoryProxy::RegisterObject]")));

    return MarshalInterface(interface_ptr);
  }

  HRESULT RevokeObject() {
    CORE_LOG(L3, (_T("[SharedMemoryProxy::RevokeObject]")));

    if (!shared_memory_ptr_) {
      OPT_LOG(LEVEL_ERROR, (_T("[Shared memory ptr error]")));
      return GOOPDATE_E_INVALID_SHARED_MEMORY_PTR;
    }

    shared_memory_ptr_->size_ = 0;
    SetZero(shared_memory_ptr_->data_);

    return S_OK;
  }

 private:
  // Helpers.
  HRESULT MarshalInterface(InterfaceType* interface_ptr) {
    CORE_LOG(L3, (_T("[SharedMemoryProxy::MarshalInterface]")));

    if (!shared_memory_ptr_) {
      OPT_LOG(LEVEL_ERROR, (_T("[Shared memory ptr error]")));
      return GOOPDATE_E_INVALID_SHARED_MEMORY_PTR;
    }

    // Marshal the interface.
    scoped_hglobal hglobal(::GlobalAlloc(GHND, 0));
    if (!valid(hglobal)) {
      OPT_LOG(LEVEL_ERROR, (_T("[GlobalAlloc failed]")));
      return E_OUTOFMEMORY;
    }

    CComPtr<IStream> stream;
    HRESULT hr = ::CreateStreamOnHGlobal(get(hglobal), false, &stream);
    if (FAILED(hr)) {
      OPT_LOG(LEVEL_ERROR, (_T("[CreateStreamOnHGlobal failed][0x%08x]"), hr));
      return hr;
    }

    // MSHLFLAGS_TABLEWEAK results in CO_E_OBJNOTREG if unmarshaling multiple
    // times, so using MSHLFLAGS_TABLESTRONG.
    hr = ::CoMarshalInterface(stream,
                              __uuidof(InterfaceType),
                              interface_ptr,
                              MSHCTX_LOCAL,
                              NULL,
                              MSHLFLAGS_TABLESTRONG);
    if (FAILED(hr)) {
      OPT_LOG(LEVEL_ERROR, (_T("[CoMarshalInterface failed][0x%08x]"), hr));
      return hr;
    }

    // Copy out the marshaled data.
    STATSTG stat = {0};
    hr = stream->Stat(&stat, STATFLAG_NONAME);
    if (FAILED(hr)) {
      OPT_LOG(LEVEL_ERROR, (_T("[IStream::Stat failed][0x%08x]"), hr));
      return hr;
    }
    int64 size = static_cast<int64>(stat.cbSize.QuadPart);
    if (!size || size > kMaxSizeInterfaceMarshalData) {
      OPT_LOG(LEVEL_ERROR, (_T("[Bad size][%I64d]"), size));
      return GOOPDATE_E_INVALID_INTERFACE_MARSHAL_SIZE;
    }

    byte* data = reinterpret_cast<byte*>(::GlobalLock(get(hglobal)));
    if (!data) {
      return HRESULTFromLastError();
    }
    memcpy(shared_memory_ptr_->data_, data, stat.cbSize.LowPart);
    shared_memory_ptr_->size_ = stat.cbSize.LowPart;

    ::GlobalUnlock(get(hglobal));

    return S_OK;
  }

  HRESULT UnmarshalInterface(InterfaceType** interface_ptr) {
    CORE_LOG(L3, (_T("[SharedMemoryProxy::UnmarshalInterface]")));

    if (!shared_memory_ptr_) {
      OPT_LOG(LEVEL_ERROR, (_T("[Shared memory ptr error]")));
      return GOOPDATE_E_INVALID_SHARED_MEMORY_PTR;
    }

    size_t size = shared_memory_ptr_->size_;
    if (!size || size > kMaxSizeInterfaceMarshalData) {
      CORE_LOG(LEVEL_ERROR, (_T("[bad size][%d]"), size));
      return GOOPDATE_E_INVALID_INTERFACE_MARSHAL_SIZE;
    }

    // Unmarshal the interface.
    scoped_hglobal hglobal(::GlobalAlloc(GPTR, size));
    if (!valid(hglobal)) {
      OPT_LOG(LEVEL_ERROR, (_T("[GlobalAlloc failed]")));
      return E_OUTOFMEMORY;
    }
    memcpy(get(hglobal), shared_memory_ptr_->data_, size);

    CComPtr<IStream> stream;
    HRESULT hr = ::CreateStreamOnHGlobal(get(hglobal), false, &stream);
    if (FAILED(hr)) {
      OPT_LOG(LEVEL_ERROR, (_T("[CreateStreamOnHGlobal failed][0x%08x]"), hr));
      return hr;
    }

    hr = ::CoUnmarshalInterface(stream,
                                __uuidof(InterfaceType),
                                reinterpret_cast<void **>(interface_ptr));
    if (FAILED(hr)) {
      OPT_LOG(LEVEL_ERROR, (_T("[CoUnmarshalInterface failed][0x%08x]"), hr));
      return hr;
    }

    return S_OK;
  }

  SharedMemoryPtr<LockType, InterfaceMarshalData> shared_memory_ptr_;
  DISALLOW_EVIL_CONSTRUCTORS(SharedMemoryProxy);
};

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_GOOGLE_UPDATE_PROXY_H__

