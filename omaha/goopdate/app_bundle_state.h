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

#ifndef OMAHA_GOOPDATE_APP_BUNDLE_STATE_H_
#define OMAHA_GOOPDATE_APP_BUNDLE_STATE_H_

#include <windows.h>
#include <atlstr.h>
#include "base/basictypes.h"

namespace omaha {

class App;
class AppBundle;
class AppBundleTest;
class WebServicesClientInterface;

namespace fsm {

// Defines the interface for encapsulating behavior associated with a particular
// state of the AppBundle object.
// All state transition calls should go through AppBundle, meaning it is the
// only class that should use this interface.
class AppBundleState {
 public:
  virtual ~AppBundleState() {}

  virtual HRESULT put_altTokens(AppBundle* app_bundle,
                                ULONG_PTR impersonation_token,
                                ULONG_PTR primary_token,
                                DWORD caller_proc_id);
  virtual HRESULT put_sessionId(AppBundle* app_bundle, BSTR session_id);
  virtual HRESULT Initialize(AppBundle* app_bundle);
  virtual HRESULT CreateApp(AppBundle* app_bundle,
                            const CString& app_id,
                            App** app);
  virtual HRESULT CreateInstalledApp(AppBundle* app_bundle,
                                     const CString& app_id,
                                     App** app);
  virtual HRESULT CreateAllInstalledApps(AppBundle* app_bundle);

  virtual HRESULT CheckForUpdate(AppBundle* app_bundle);
  virtual HRESULT Download(AppBundle* app_bundle);
  virtual HRESULT Install(AppBundle* app_bundle);

  virtual HRESULT UpdateAllApps(AppBundle* app_bundle);

  virtual HRESULT Stop(AppBundle* app_bundle);
  virtual HRESULT Pause(AppBundle* app_bundle);
  virtual HRESULT Resume(AppBundle* app_bundle);

  virtual HRESULT DownloadPackage(AppBundle* app_bundle,
                                  const CString& app_id,
                                  const CString& package_name);

  virtual HRESULT CompleteAsyncCall(AppBundle* app_bundle);

  virtual bool IsBusy() const;

 protected:
  enum BundleState {
    STATE_INIT,
    STATE_INITIALIZED,
    STATE_BUSY,
    STATE_READY,
    STATE_PAUSED,
    STATE_STOPPED,
  };


  explicit AppBundleState(BundleState state) : state_(state) {}

  // These functions provide pass-through access to private AppBundle members.
  // TODO(omaha): remove asserts and implement inline.
  void AddAppToBundle(AppBundle* app_bundle, App* app);
  bool IsPendingNonBlockingCall(AppBundle* app_bundle);

  HRESULT DoDownloadPackage(AppBundle* app_bundle,
                            const CString& app_id,
                            const CString& package_name);

  // After calling this method, the old state (this object) is deleted and the
  // code may not reference the members of the old state anymore.
  void ChangeState(AppBundle* app_bundle, AppBundleState* state);

  HRESULT HandleInvalidStateTransition(AppBundle* app_bundle,
                                       const TCHAR* function_name);

 private:
  friend class AppBundleTest;

  BundleState state_;

  DISALLOW_COPY_AND_ASSIGN(AppBundleState);
};

}  // namespace fsm

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_APP_BUNDLE_STATE_H_
