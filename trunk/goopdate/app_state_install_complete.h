// Copyright 2009 Google Inc.
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

#ifndef OMAHA_GOOPDATE_APP_STATE_INSTALL_COMPLETE_H_
#define OMAHA_GOOPDATE_APP_STATE_INSTALL_COMPLETE_H_

#include "base/basictypes.h"
#include "omaha/goopdate/app_state.h"

namespace omaha {

namespace fsm {

class AppStateInstallComplete : public AppState {
 public:
  explicit AppStateInstallComplete(App* app);
  virtual ~AppStateInstallComplete() {}

  virtual const PingEvent* CreatePingEvent(App* app,
                                           CurrentState previous_state) const;

  virtual void Cancel(App* app);
  virtual void Error(App* app,
                     const ErrorContext& error_context,
                     const CString& message);
 private:
  DISALLOW_COPY_AND_ASSIGN(AppStateInstallComplete);
};

}  // namespace fsm

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_APP_STATE_INSTALL_COMPLETE_H_
