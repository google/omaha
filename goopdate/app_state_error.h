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

#ifndef OMAHA_GOOPDATE_APP_STATE_ERROR_H_
#define OMAHA_GOOPDATE_APP_STATE_ERROR_H_

#include "base/basictypes.h"
#include "omaha/goopdate/app_state.h"

namespace omaha {

namespace fsm {

// The error state is idempotent. Further transitions from error state into
// itself are allowed but have no effect. Therefore, the first error wins. One
// scenario where this occurs is canceling the app. Canceling the app is not
// blocking and it moves the app in the error state right away. The Error
// method is called a second time, as the actual cancel code executes in the
// thread pool.
class AppStateError : public AppState {
 public:
  AppStateError();
  virtual ~AppStateError() {}

  virtual const PingEvent* CreatePingEvent(App* app,
                                           CurrentState previous_state) const;

  // These calls are legal in this state but do nothing. This can occur when
  // this app has encountered an error but bundle is still being processed.
  // For instance, when cancelling a bundle during a download, the applications
  // transition right away in the error state. The cancel event is handled at
  // some point in the future. Depending on a race condition, the downloads may
  // have succeeded or failed due to the cancellation. The race condition is
  // resolved when the transition call reaches this object and the call is
  // ignored.
  virtual void DownloadComplete(App* app);
  virtual void MarkReadyToInstall(App* app);

  // These calls are legal in this state but do nothing. This can occur when
  // this app has encountered an error or has been canceled but bundle is still
  // being processed.
  virtual void PreUpdateCheck(App* app, xml::UpdateRequest* update_request);
  virtual void PostUpdateCheck(App* app,
                               HRESULT result,
                               xml::UpdateResponse* update_response);
  virtual void QueueDownload(App* app);
  virtual void QueueDownloadOrInstall(App* app);
  virtual void Download(App* app, DownloadManagerInterface* download_manager);
  virtual void QueueInstall(App* app);
  virtual void Install(App* app, InstallManagerInterface* install_manager);

  // Canceling while in a terminal state has no effect.
  virtual void Cancel(App* app);

  virtual void Error(App* app,
                     const ErrorContext& error_context,
                     const CString& message);

 private:
  DISALLOW_COPY_AND_ASSIGN(AppStateError);
};

}  // namespace fsm

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_APP_STATE_ERROR_H_
