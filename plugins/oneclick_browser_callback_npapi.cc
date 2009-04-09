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
// OneClick helper functions.

// Note:  oneclick_browser_callback_npapi.h cannot be included before system
// headers due to conflicts between base/windows code and NPAPI
// definitions.

#pragma warning(push)
#pragma warning(disable : 4201 4265)
// 4201: nonstandard extension used : nameless struct/union
// 4265: class has virtual functions, but destructor is not virtual

#include "base/scoped_ptr.h"
#include "omaha/plugins/npOneClick.h"
#include "omaha/plugins/oneclick_browser_callback_npapi.h"

#pragma warning(pop)

namespace omaha {

OneClickBrowserCallbackNpapi::OneClickBrowserCallbackNpapi()
    : npp_(NULL), success_callback_(NULL), failure_callback_(NULL) {
}

OneClickBrowserCallbackNpapi::~OneClickBrowserCallbackNpapi() {
  if (success_callback_) {
    NPN_ReleaseObject(success_callback_);
    success_callback_ = NULL;
  }
  if (failure_callback_) {
    NPN_ReleaseObject(failure_callback_);
    failure_callback_ = NULL;
  }
}

HRESULT OneClickBrowserCallbackNpapi::Initialize(NPP npp,
                                                 NPVariant success_callback,
                                                 NPVariant failure_callback) {
  npp_ = npp;

  if (NPVARIANT_IS_OBJECT(success_callback)) {
    success_callback_ = NPVARIANT_TO_OBJECT(success_callback);
    NPN_RetainObject(success_callback_);
  }

  if (NPVARIANT_IS_OBJECT(failure_callback)) {
    failure_callback_ = NPVARIANT_TO_OBJECT(failure_callback);
    NPN_RetainObject(failure_callback_);
  }

  return S_OK;
}

void OneClickBrowserCallbackNpapi::DoSuccessCallback() {
  CORE_LOG(L2, (_T("[DoSuccessCallback entered]")));

  if (!success_callback_) {
    CORE_LOG(L2, (_T("[DoSuccessCallback entered][NO success_callback_]")));
    return;
  }

  CORE_LOG(L2, (_T("[DoSuccessCallback entered][valid success_callback_]")));
  NPVariant rval;
  NULL_TO_NPVARIANT(rval);

  NPN_InvokeDefault(npp_,
                    success_callback_,
                    NULL,
                    0,
                    &rval);
}

void OneClickBrowserCallbackNpapi::DoFailureCallback(HRESULT hr_error) {
  CORE_LOG(L2, (_T("[DoFailureCallback entered][hr_error=0x%x]"), hr_error));

  if (!failure_callback_) {
    CORE_LOG(L2, (_T("[DoFailureCallback entered][NO failure_callback_]")));
    return;
  }

  CORE_LOG(L2, (_T("[DoFailureCallback entered][valid failure_callback_]")));
  NPVariant rval;
  NULL_TO_NPVARIANT(rval);

  NPVariant arg;
  NULL_TO_NPVARIANT(arg);
  INT32_TO_NPVARIANT(hr_error, arg);

  NPN_InvokeDefault(npp_,
                    failure_callback_,
                    &arg,
                    1,
                    &rval);
}

}  // namespace omaha

