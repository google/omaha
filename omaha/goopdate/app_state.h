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

#ifndef OMAHA_GOOPDATE_APP_STATE_H_
#define OMAHA_GOOPDATE_APP_STATE_H_

#include <windows.h>
#include <atlstr.h>

#include "base/basictypes.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/common/ping_event.h"
#include "omaha/goopdate/installer_result_info.h"
#include "goopdate/omaha3_idl.h"

namespace omaha {

class  App;
struct ErrorContext;
class  DownloadManagerInterface;
class  InstallManagerInterface;

namespace xml {

class UpdateRequest;
class UpdateResponse;

}  // namespace xml

namespace fsm {

// Defines the interface for encapsulating behavior associated with a particular
// state of the App object.
// All state transition calls should go through App, meaning it is the only
// class that should use this interface.
class AppState {
 public:
  virtual ~AppState() {}

  // Design note: avoid switch and conditional statements on CurrentState. They
  // break polymorphism and they are a poor substitute for RTTI.
  CurrentState state() const { return state_; }

  // Creates a ping event for the current state. The caller owns the returned
  // object.
  virtual const PingEvent* CreatePingEvent(App* app,
                                           CurrentState previous_state) const;

  virtual void QueueUpdateCheck(App* app);

  virtual void PreUpdateCheck(App* app, xml::UpdateRequest* update_request);

  virtual void PostUpdateCheck(App* app,
                               HRESULT result,
                               xml::UpdateResponse* update_response);

  virtual void QueueDownload(App* app);

  // Queues the download for a download and install operation.
  virtual void QueueDownloadOrInstall(App* app);

  virtual void Download(App* app, DownloadManagerInterface* download_manager);

  virtual void Downloading(App* app);

  virtual void DownloadComplete(App* app);

  virtual void MarkReadyToInstall(App* app);

  virtual void QueueInstall(App* app);

  virtual void Install(App* app, InstallManagerInterface* install_manager);

  virtual void Installing(App* app);

  virtual void ReportInstallerComplete(App* app,
                                       const InstallerResultInfo& result_info);

  virtual void Pause(App* app);

  virtual void Cancel(App* app);

  virtual void Error(App* app,
                     const ErrorContext& error_context,
                     const CString& message);

 protected:
  explicit AppState(CurrentState state) : state_(state) {}

  // After calling this method, the old state (this object) is deleted and the
  // code may not reference the members of the old state anymore.
  void ChangeState(App* app, AppState* app_state);

  void HandleInvalidStateTransition(App* app, const TCHAR* function_name);

  static PingEvent::Results GetCompletionResult(const App& app);

  void HandleGroupPolicyError(App* app, HRESULT code);

 private:
  // TODO(omaha): rename to CurrentStateId or similar.
  const CurrentState state_;

  DISALLOW_COPY_AND_ASSIGN(AppState);
};

}  // namespace fsm

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_APP_STATE_H_
