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

#ifndef OMAHA_GOOPDATE_APP_BUNDLE_STATE_INITIALIZED_H_
#define OMAHA_GOOPDATE_APP_BUNDLE_STATE_INITIALIZED_H_

#include "base/basictypes.h"
#include "omaha/goopdate/app_bundle_state.h"

namespace omaha {

namespace fsm {

class AppBundleStateInitialized : public AppBundleState {
 public:
  AppBundleStateInitialized()
      : AppBundleState(STATE_INITIALIZED),
        has_new_app_(false),
        has_installed_app_(false),
        has_reloaded_policy_managers_(false) {}
  virtual ~AppBundleStateInitialized() {}

  virtual HRESULT Stop(AppBundle* app_bundle);
  virtual HRESULT Pause(AppBundle* app_bundle);
  virtual HRESULT CreateApp(AppBundle* app_bundle,
                            const CString& app_id,
                            App** app);
  virtual HRESULT CreateInstalledApp(AppBundle* app_bundle,
                                     const CString& app_id,
                                     App** app);
  virtual HRESULT CreateAllInstalledApps(AppBundle* app_bundle);

  virtual HRESULT CheckForUpdate(AppBundle* app_bundle);

  virtual HRESULT UpdateAllApps(AppBundle* app_bundle);

  virtual HRESULT DownloadPackage(AppBundle* app_bundle,
                                  const CString& app_id,
                                  const CString& package_name);

 private:
  HRESULT AddInstalledApp(AppBundle* app_bundle,
                          const CString& appId,
                          App** app);

  // Adds an app to app_bundle's apps_. Takes ownership of app when successful.
  HRESULT AddApp(AppBundle* app_bundle, App* app);

  bool has_new_app_;
  bool has_installed_app_;

  bool has_reloaded_policy_managers_;

  DISALLOW_COPY_AND_ASSIGN(AppBundleStateInitialized);
};

}  // namespace fsm

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_APP_BUNDLE_STATE_INITIALIZED_H_
