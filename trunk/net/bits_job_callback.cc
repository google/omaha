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

#include "omaha/net/bits_job_callback.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/utils.h"
#include "omaha/net/bits_request.h"
#include "omaha/net/bits_utils.h"

namespace omaha {

HRESULT BitsJobCallback::Create(BitsRequest* bits_request,
                                BitsJobCallback** bits_job_callback) {
  ASSERT1(bits_request);
  ASSERT1(bits_job_callback);

  *bits_job_callback = NULL;
  scoped_ptr<CComObjectNoLock<BitsJobCallback> > callback_obj(
      new CComObjectNoLock<BitsJobCallback>);
  if (callback_obj == NULL) {
    return E_OUTOFMEMORY;
  }

  callback_obj->AddRef();
  callback_obj->bits_request_ = bits_request;
  *bits_job_callback = callback_obj.release();

  return S_OK;
}

void BitsJobCallback::RemoveReferenceToBitsRequest() {
  __mutexScope(lock_);
  bits_request_ = NULL;
}

HRESULT BitsJobCallback::JobTransferred(IBackgroundCopyJob* bits_job) {
  UNREFERENCED_PARAMETER(bits_job);

  __mutexScope(lock_);
  if (bits_request_) {
    bits_request_->OnBitsJobStateChanged();
  }

  // Returns S_OK to avoid BITS continuing to call this callback.
  return S_OK;
}

HRESULT BitsJobCallback::JobError(IBackgroundCopyJob* bits_job,
                                  IBackgroundCopyError* error) {
  UNREFERENCED_PARAMETER(bits_job);
  UNREFERENCED_PARAMETER(error);

  __mutexScope(lock_);
  if (bits_request_) {
    bits_request_->OnBitsJobStateChanged();
  }

  // Returns S_OK to avoid BITS continuing to call this callback.
  return S_OK;
}

HRESULT BitsJobCallback::JobModification(IBackgroundCopyJob* bits_job,
                                         DWORD reserved) {
  ASSERT1(bits_job);
  UNREFERENCED_PARAMETER(bits_job);
  UNREFERENCED_PARAMETER(reserved);

  __mutexScope(lock_);
  if (bits_request_) {
    bits_request_->OnBitsJobStateChanged();
  }
  return S_OK;
}

}  // namespace omaha

