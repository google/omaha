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

// Defines the root of the object model. This is not exposed as a COM object.

#ifndef OMAHA_GOOPDATE_MODEL_H_
#define OMAHA_GOOPDATE_MODEL_H_

#ifndef _ATL_FREE_THREADED
#error Must use _ATL_FREE_THREADED to avoid differences in member offsets.
#endif

#include <windows.h>
#include <vector>
#include "base/basictypes.h"
#include "base/debug.h"
#include "base/scoped_ptr.h"
#include "base/synchronized.h"
#include "omaha/goopdate/app.h"
#include "omaha/goopdate/app_bundle.h"
#include "omaha/goopdate/app_version.h"
#include "omaha/goopdate/current_state.h"
#include "omaha/goopdate/package.h"
#include "third_party/bar/shared_ptr.h"

namespace omaha {

class WorkerModelInterface;

class Model {
 public:
  explicit Model(WorkerModelInterface* worker);
  virtual ~Model();

  const Lockable& lock() const { return lock_; }

  // Returns true if the model lock is held by the calling thread.
  bool IsLockedByCaller() const {
    return ::GetCurrentThreadId() == lock_.GetOwner();
  }

  // Creates an AppBundle object in the model.
  shared_ptr<AppBundle> CreateAppBundle(bool is_machine);

  // Removes the AppBundle objects that have no outstanding strong references.
  void CleanupExpiredAppBundles();

  size_t GetNumberOfAppBundles() const;

  shared_ptr<AppBundle> GetAppBundle(size_t index) const;

  // Initiates an update check for all apps in the bundle.
  HRESULT CheckForUpdate(AppBundle* app_bundle);

  // Initiates download of files necessary to install all apps in the bundle.
  HRESULT Download(AppBundle* app_bundle);

  // Initiates Download, if necessary, and install all app in the bundle.
  HRESULT DownloadAndInstall(AppBundle* app_bundle);

  // Initiates an update of all registered apps and performs periodic tasks
  // related to all apps. Primarily for use by Omaha's /ua client. Includes
  // update check, download and install.
  HRESULT UpdateAllApps(AppBundle* app_bundle);

  HRESULT Stop(AppBundle* app_bundle);
  HRESULT Pause(AppBundle* app_bundle);
  HRESULT Resume(AppBundle* app_bundle);

  HRESULT DownloadPackage(Package* package);
  HRESULT GetPackage(const Package* package, const CString& dir) const;

  bool IsPackageAvailable(const Package* package) const;

  HRESULT PurgeAppLowerVersions(const CString& app_id,
                                const CString& version) const;

 private:
  typedef weak_ptr<AppBundle> AppBundleWeakPtr;

  // Serializes access to the model objects. Consider replacing with SWMR lock.
  LLock lock_;

  std::vector<AppBundleWeakPtr> app_bundles_;
  WorkerModelInterface* worker_;

  DISALLOW_COPY_AND_ASSIGN(Model);
};

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_MODEL_H_
