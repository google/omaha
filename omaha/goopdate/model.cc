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

#include "omaha/goopdate/model.h"
#include <algorithm>
#include <functional>
#include "omaha/base/debug.h"
#include "omaha/base/logging.h"
#include "omaha/goopdate/worker.h"

namespace omaha {

Model::Model(WorkerModelInterface* worker) : worker_(NULL) {
  CORE_LOG(L3, (_T("[Model::Model]")));
  ASSERT1(worker);

  // Initialization of the object must be thread safe.
  __mutexScope(lock_);
  worker_ = worker;
}

Model::~Model() {
  CORE_LOG(L3, (_T("[Model::~Model]")));

  __mutexScope(lock_);
  ASSERT1(app_bundles_.empty());
  worker_ = NULL;
}

shared_ptr<AppBundle> Model::CreateAppBundle(bool is_machine) {
  __mutexScope(lock_);

  shared_ptr<AppBundle> app_bundle(new AppBundle(is_machine, this));
  app_bundles_.push_back(AppBundleWeakPtr(app_bundle));

  const int lock_count(worker_->Lock());

  CORE_LOG(L3, (_T("[Model::CreateAppBundle][bundle %p][module lock %i]"),
      app_bundle.get(), lock_count));

  return app_bundle;
}

// Erases the expired AppBundle weak pointers.
void Model::CleanupExpiredAppBundles() {
  __mutexScope(lock_);

  typedef std::vector<AppBundleWeakPtr>::iterator Iterator;
  Iterator it = remove_if(app_bundles_.begin(),
                          app_bundles_.end(),
                          std::mem_fun_ref(&AppBundleWeakPtr::expired));

  const size_t num_bundles = distance(it, app_bundles_.end());

  app_bundles_.erase(it, app_bundles_.end());
  for (size_t i = 0; i != num_bundles; ++i) {
    const int lock_count(worker_->Unlock());

    CORE_LOG(L3, (_T("[Model::CleanupExpiredAppBundles][module unlock %i]"),
        lock_count));
  }
}

size_t Model::GetNumberOfAppBundles() const {
  __mutexScope(lock_);
  return app_bundles_.size();
}

shared_ptr<AppBundle> Model::GetAppBundle(size_t index) const {
  __mutexScope(lock_);
  ASSERT1(!app_bundles_[index].expired());
  return app_bundles_[index].lock();
}

HRESULT Model::CheckForUpdate(AppBundle* app_bundle) {
  CORE_LOG(L3, (_T("[Model::CheckForUpdate][0x%p]"), app_bundle));

  __mutexScope(lock_);

  return worker_->CheckForUpdateAsync(app_bundle);
}

HRESULT Model::Download(AppBundle* app_bundle) {
  CORE_LOG(L3, (_T("[Model::Download][0x%p]"), app_bundle));

  __mutexScope(lock_);

  return worker_->DownloadAsync(app_bundle);
}

HRESULT Model::DownloadAndInstall(AppBundle* app_bundle) {
  CORE_LOG(L3, (_T("[Model::DownloadAndInstall][0x%p]"), app_bundle));

  __mutexScope(lock_);

  return worker_->DownloadAndInstallAsync(app_bundle);
}

HRESULT Model::UpdateAllApps(AppBundle* app_bundle) {
  CORE_LOG(L3, (_T("[Model::UpdateAllApps][0x%p]"), app_bundle));

  __mutexScope(lock_);

  return worker_->UpdateAllAppsAsync(app_bundle);
}

HRESULT Model::Stop(AppBundle* app_bundle) {
  CORE_LOG(L3, (_T("[Model::Stop][0x%p]"), app_bundle));

  __mutexScope(lock_);

  return worker_->Stop(app_bundle);
}

HRESULT Model::Pause(AppBundle* app_bundle) {
  CORE_LOG(L3, (_T("[Model::Pause][0x%p]"), app_bundle));

  __mutexScope(lock_);

  return worker_->Pause(app_bundle);
}

HRESULT Model::Resume(AppBundle* app_bundle) {
  CORE_LOG(L3, (_T("[Model::Resume][0x%p]"), app_bundle));

  __mutexScope(lock_);

  return worker_->Resume(app_bundle);
}

HRESULT Model::DownloadPackage(Package* package) {
  CORE_LOG(L3, (_T("[Model::DownloadPackage][0x%p]"), package));

  __mutexScope(lock_);

  return worker_->DownloadPackageAsync(package);
}

HRESULT Model::GetPackage(const Package* package, const CString& dir) const {
  __mutexScope(lock_);

  return worker_->GetPackage(package, dir);
}

bool Model::IsPackageAvailable(const Package* package) const {
  __mutexScope(lock_);

  return worker_->IsPackageAvailable(package);
}

HRESULT Model::PurgeAppLowerVersions(const CString& app_id,
                                     const CString& version) const {
  __mutexScope(lock_);

  return worker_->PurgeAppLowerVersions(app_id, version);
}

}  // namespace omaha
