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


// OneClick browser-specific callback class for IE/ActiveX.

#ifndef OMAHA_PLUGINS_ONECLICK_BROWSER_CALLBACK_ACTIVEX_H__
#define OMAHA_PLUGINS_ONECLICK_BROWSER_CALLBACK_ACTIVEX_H__

#include <atlbase.h>
#include <atlcom.h>

#include "omaha/plugins/oneclick_browser_callback.h"

namespace omaha {

class OneClickBrowserCallbackActiveX : public OneClickBrowserCallback {
 public:
  OneClickBrowserCallbackActiveX();
  virtual ~OneClickBrowserCallbackActiveX();

  // Initialize the callback with two VARIANT values which are Javascript
  // functions to be called.  IE passes Javascript classes as an IDispatch
  // wrapped inside a VARIANT.
  // The VARIANT parameters are optional which means we won't call a function in
  // those cases.
  HRESULT Initialize(VARIANT* success_callback, VARIANT* failure_callback);

  // Overrides from abstract base class.
  virtual void DoSuccessCallback();
  virtual void DoFailureCallback(HRESULT hr_error);

 private:
  CComPtr<IDispatch> success_callback_dispatch_;
  CComPtr<IDispatch> failure_callback_dispatch_;

  // Returns true if var is VT_NULL or VT_EMPTY.
  bool VariantIsNullOrUndefined(const VARIANT* var);

  DISALLOW_EVIL_CONSTRUCTORS(OneClickBrowserCallbackActiveX);
};

}  // namespace omaha

#endif  // OMAHA_PLUGINS_ONECLICK_BROWSER_CALLBACK_ACTIVEX_H__

