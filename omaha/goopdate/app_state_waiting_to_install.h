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

#ifndef OMAHA_GOOPDATE_APP_STATE_WAITING_TO_INSTALL_H_
#define OMAHA_GOOPDATE_APP_STATE_WAITING_TO_INSTALL_H_

#include "base/basictypes.h"
#include "omaha/goopdate/app_state.h"

namespace omaha {

namespace fsm {

class AppStateWaitingToInstall : public AppState {
 public:
  AppStateWaitingToInstall();
  virtual ~AppStateWaitingToInstall() {}

  // These calls are legal in this state but do nothing. This can occur when the
  // app has already been downloaded and the client calls AppBundle::install().
  virtual void Download(App* app, DownloadManagerInterface* download_manager);
  virtual void QueueInstall(App* app);

  virtual void Install(App* app, InstallManagerInterface* install_manager);
  virtual void Installing(App* app);

 private:
  DISALLOW_COPY_AND_ASSIGN(AppStateWaitingToInstall);
};

}  // namespace fsm

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_APP_STATE_WAITING_TO_INSTALL_H_
