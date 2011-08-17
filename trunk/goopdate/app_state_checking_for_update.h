// Copyright 2009-2010 Google Inc.
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

#ifndef OMAHA_GOOPDATE_APP_STATE_CHECKING_FOR_UPDATE_H_
#define OMAHA_GOOPDATE_APP_STATE_CHECKING_FOR_UPDATE_H_

#include "base/basictypes.h"
#include "omaha/goopdate/app_state.h"

namespace omaha {

namespace fsm {

class AppStateCheckingForUpdate : public AppState {
 public:
  AppStateCheckingForUpdate();
  virtual ~AppStateCheckingForUpdate() {}

  virtual void PostUpdateCheck(App* app,
                               HRESULT result,
                               xml::UpdateResponse* update_response);

 private:
  void HandleUpdateAvailable(App* app, HRESULT code, const CString& message);
  void HandleUpdateDeferred(App* app, HRESULT code, const CString& message);
  void HandleNoUpdate(App* app, HRESULT code, const CString& message);
  void HandleErrorResponse(App* app, HRESULT code, const CString& message);

  void PersistUpdateCheckSuccessfullySent(const App& app);

  xml::UpdateResponse* update_response_;

  DISALLOW_COPY_AND_ASSIGN(AppStateCheckingForUpdate);
};

}  // namespace fsm

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_APP_STATE_CHECKING_FOR_UPDATE_H_
