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

#ifndef OMAHA_GOOPDATE_APP_BUNDLE_STATE_PAUSED_H_
#define OMAHA_GOOPDATE_APP_BUNDLE_STATE_PAUSED_H_

#include "base/basictypes.h"
#include "omaha/goopdate/app_bundle_state.h"

namespace omaha {

namespace fsm {

class AppBundleStatePaused : public AppBundleState {
 public:
  AppBundleStatePaused()
      : AppBundleState(STATE_PAUSED),
        is_async_call_complete_(false) {}
  virtual ~AppBundleStatePaused() {}

  // TODO(omaha): What should Stop() do?
  virtual HRESULT Pause(AppBundle* app_bundle);
  virtual HRESULT Resume(AppBundle* app_bundle);

  virtual HRESULT CompleteAsyncCall(AppBundle* app_bundle);

 private:
  // It is possible that an asynchronous operation will complete while in the
  // paused state because pausing itself is asynchronous. Therefore,
  // CompleteAsyncCall() may be called in this state. Remember whether
  // it was so that we can transition to the correct state on Resume().
  bool is_async_call_complete_;

  DISALLOW_COPY_AND_ASSIGN(AppBundleStatePaused);
};

}  // namespace fsm

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_APP_BUNDLE_STATE_PAUSED_H_
