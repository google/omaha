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

#include "omaha/goopdate/app_version.h"

#include <atlsafe.h>

#include "omaha/base/debug.h"
#include "omaha/base/logging.h"
#include "omaha/base/synchronized.h"
#include "omaha/base/utils.h"
#include "omaha/goopdate/model.h"

namespace omaha {

AppVersion::AppVersion(App* app)
    : ModelObject(app->model()),
      app_(app) {
}

// Destruction of App objects happens within the scope of their parent,
// which controls the locking.
AppVersion::~AppVersion() {
  ASSERT1(model()->IsLockedByCaller());

  for (size_t i = 0; i < packages_.size(); ++i) {
    delete packages_[i];
  }
}

CString AppVersion::version() const {
  __mutexScope(model()->lock());
  return version_;
}

void AppVersion::set_version(const CString& version) {
  __mutexScope(model()->lock());
  version_ = version;
}

App* AppVersion::app() {
  __mutexScope(model()->lock());
  return app_;
}

const App* AppVersion::app() const {
  __mutexScope(model()->lock());
  return app_;
}

// TODO(omaha3): It's unfortunate that the manifest can be empty. We need to
// make a copy anyway in UpdateResponse::BuildApp, so maybe this class
// should just expose a manifest object created in the constructor. On the
// other hand, current_version may not have a manifest, so this would be an
// empty object. Because the manifest must be created, AppData must friend
// InstallManager tests and other tests that need a manifest. This could
// probably be solved through mocking too.
const xml::InstallManifest* AppVersion::install_manifest() const {
  __mutexScope(model()->lock());
  return install_manifest_.get();
}

void AppVersion::set_install_manifest(xml::InstallManifest* install_manifest) {
  __mutexScope(model()->lock());
  ASSERT1(install_manifest);
  install_manifest_.reset(install_manifest);
}

size_t AppVersion::GetNumberOfPackages() const {
  __mutexScope(model()->lock());
  return packages_.size();
}

HRESULT AppVersion::AddPackage(const CString& filename,
                               uint32 size,
                               const CString& hash) {
  if (hash.IsEmpty()) {
    return E_INVALIDARG;
  }

  __mutexScope(model()->lock());
  Package* package = new Package(this);
  package->SetFileInfo(filename, size, hash);
  packages_.push_back(package);
  return S_OK;
}

Package* AppVersion::GetPackage(size_t index) {
  __mutexScope(model()->lock());

  if (index >= GetNumberOfPackages()) {
    ASSERT1(false);
    return NULL;
  }

  return packages_[index];
}

const Package* AppVersion::GetPackage(size_t index) const {
  __mutexScope(model()->lock());

  if (index >= GetNumberOfPackages()) {
    ASSERT1(false);
    return NULL;
  }

  return packages_[index];
}

const std::vector<CString>& AppVersion::download_base_urls() const {
  __mutexScope(model()->lock());
  ASSERT1(!download_base_urls_.empty());
  return download_base_urls_;
}

HRESULT AppVersion::AddDownloadBaseUrl(const CString& base_url) {
  __mutexScope(model()->lock());
  ASSERT1(!base_url.IsEmpty());
  download_base_urls_.push_back(base_url);
  return S_OK;
}

// IAppVersion.
STDMETHODIMP AppVersion::get_version(BSTR* version) {
  __mutexScope(model()->lock());
  ASSERT1(version);
  *version = version_.AllocSysString();
  return S_OK;
}

STDMETHODIMP AppVersion::get_packageCount(long* count) {  // NOLINT
  __mutexScope(model()->lock());

  const size_t num_packages = GetNumberOfPackages();
  if (num_packages > LONG_MAX) {
    return E_FAIL;
  }

  *count = static_cast<long>(num_packages);  // NOLINT

  return S_OK;
}

STDMETHODIMP AppVersion::get_package(long index, Package** package) {  // NOLINT
  __mutexScope(model()->lock());

  if (index < 0 || static_cast<size_t>(index) >= GetNumberOfPackages()) {
    return HRESULT_FROM_WIN32(ERROR_INVALID_INDEX);
  }

  *package = GetPackage(index);
  return S_OK;
}

STDMETHODIMP AppVersionWrapper::get_version(BSTR* version) {
  __mutexScope(model()->lock());
  return wrapped_obj()->get_version(version);
}

STDMETHODIMP AppVersionWrapper::get_packageCount(long* count) {  // NOLINT
  __mutexScope(model()->lock());
  return wrapped_obj()->get_packageCount(count);
}

STDMETHODIMP AppVersionWrapper::get_package(long index,  // NOLINT
                                            IDispatch** package) {
  __mutexScope(model()->lock());

  Package* p = NULL;
  HRESULT hr = wrapped_obj()->get_package(index, &p);
  if (FAILED(hr)) {
    return hr;
  }

  return PackageWrapper::Create(controlling_ptr(), p, package);
}

HRESULT CopyAppVersionPackages(const AppVersion* app_version,
                               const CString& dir) {
  ASSERT1(app_version);

  HRESULT hr(CreateDir(dir, NULL));
  if (FAILED(hr)) {
    return hr;
  }

  for (size_t i = 0; i != app_version->GetNumberOfPackages(); ++i) {
    const Package* package = app_version->GetPackage(i);
    ASSERT1(package);
    if (package) {
      hr = package->get(CComBSTR(dir));
      if (FAILED(hr)) {
        CORE_LOG(LE, (_T("[Package::get failed][%s][%s][0x%x]"),
            package->filename(), dir, hr));
        return hr;
      }
    }
  }

  return S_OK;
}

}  // namespace omaha
