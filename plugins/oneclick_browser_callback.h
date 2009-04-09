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

// OneClick browser-specific callback classes.  This is the abstract base class
// for a browser-callback class to derive from.

#ifndef OMAHA_PLUGINS_ONECLICK_BROWSER_CALLBACK_H__
#define OMAHA_PLUGINS_ONECLICK_BROWSER_CALLBACK_H__

#include <windows.h>

#include "base/basictypes.h"

namespace omaha {

class OneClickBrowserCallback {
 public:
  OneClickBrowserCallback() {}
  virtual ~OneClickBrowserCallback() {}

  virtual void DoSuccessCallback() = 0;
  virtual void DoFailureCallback(HRESULT hr_error) = 0;

 private:
  DISALLOW_EVIL_CONSTRUCTORS(OneClickBrowserCallback);
};

}  // namespace omaha

#endif  // OMAHA_PLUGINS_ONECLICK_BROWSER_CALLBACK_H__

