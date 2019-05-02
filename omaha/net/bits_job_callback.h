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

#ifndef OMAHA_NET_BITS_JOB_CALLBACK_H_
#define OMAHA_NET_BITS_JOB_CALLBACK_H_

#include <windows.h>
#include <bits.h>
#include <atlbase.h>
#include <atlcom.h>
#include "base/basictypes.h"
#include "omaha/base/synchronized.h"

namespace omaha {

class BitsRequest;

// BitsJobCallback receives notifications that a BITS job is complete, has been
// modified, or is in error.
class ATL_NO_VTABLE BitsJobCallback
    : public CComObjectRootEx<CComObjectThreadModel>,
      public IBackgroundCopyCallback {
 public:
  BitsJobCallback() {}
  virtual ~BitsJobCallback() {
    bits_request_ = NULL;
  }

  static HRESULT Create(BitsRequest* bits_request,
                        BitsJobCallback** bits_job_callback);

  void RemoveReferenceToBitsRequest();

  BEGIN_COM_MAP(BitsJobCallback)
    COM_INTERFACE_ENTRY(IBackgroundCopyCallback)
  END_COM_MAP()

  // IBackgroundCopyCallback methods.
  STDMETHODIMP JobTransferred(IBackgroundCopyJob* job);
  STDMETHODIMP JobError(IBackgroundCopyJob* job, IBackgroundCopyError* error);
  STDMETHODIMP JobModification(IBackgroundCopyJob* job, DWORD reserved);

 private:
  BitsRequest* bits_request_;
  LLock lock_;

  DISALLOW_COPY_AND_ASSIGN(BitsJobCallback);
};

}   // namespace omaha

#endif  // OMAHA_NET_BITS_JOB_CALLBACK_H_

