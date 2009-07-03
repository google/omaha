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


// OneClick browser-specific callback class for NPAPI based browsers.

#ifndef OMAHA_PLUGINS_ONECLICK_BROWSER_CALLBACK_NPAPI_H__
#define OMAHA_PLUGINS_ONECLICK_BROWSER_CALLBACK_NPAPI_H__

#include "omaha/plugins/oneclick_browser_callback.h"

namespace omaha {

class OneClickBrowserCallbackNpapi : public OneClickBrowserCallback {
 public:
  OneClickBrowserCallbackNpapi();
  virtual ~OneClickBrowserCallbackNpapi();

  // Initializes the callback class with the NPP and two NPVariant values which
  // represent the Javascript functions to be called.  NPAPI passes Javascript
  // functions as an NPObject wrapped within an NPVariant.
  // The NPVariant parameters are optional (can be internally NULL) which means
  // we won't call a function in those cases.
  HRESULT Initialize(NPP npp,
                     NPVariant success_callback,
                     NPVariant failure_callback);

  // Overrides from abstract base class.
  virtual void DoSuccessCallback();
  virtual void DoFailureCallback(HRESULT hr_error);

 private:
  NPP npp_;
  NPObject* success_callback_;
  NPObject* failure_callback_;

  DISALLOW_EVIL_CONSTRUCTORS(OneClickBrowserCallbackNpapi);
};

}  // namespace omaha

#endif  // OMAHA_PLUGINS_ONECLICK_BROWSER_CALLBACK_NPAPI_H__

