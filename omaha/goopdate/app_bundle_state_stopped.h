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

#ifndef OMAHA_GOOPDATE_APP_BUNDLE_STATE_STOPPED_H_
#define OMAHA_GOOPDATE_APP_BUNDLE_STATE_STOPPED_H_

#include "base/basictypes.h"
#include "omaha/goopdate/app_bundle_state.h"

namespace omaha {

namespace fsm {

class AppBundleStateStopped : public AppBundleState {
 public:
  AppBundleStateStopped()
      : AppBundleState(STATE_STOPPED) {}
  virtual ~AppBundleStateStopped() {}

 private:

  virtual HRESULT Stop(AppBundle* app_bundle);

  virtual HRESULT CompleteAsyncCall(AppBundle* app_bundle);

  DISALLOW_COPY_AND_ASSIGN(AppBundleStateStopped);
};

}  // namespace fsm

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_APP_BUNDLE_STATE_STOPPED_H_

