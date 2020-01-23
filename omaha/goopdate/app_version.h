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

// Defines the AppVersion COM object exposed by the model.

#ifndef OMAHA_GOOPDATE_APP_VERSION_H_
#define OMAHA_GOOPDATE_APP_VERSION_H_

#include <atlbase.h>
#include <atlcom.h>
#include <vector>

#include "base/basictypes.h"
#include "goopdate/omaha3_idl.h"
#include "omaha/base/constants.h"
#include "omaha/common/install_manifest.h"
#include "omaha/goopdate/com_wrapper_creator.h"
#include "omaha/goopdate/model_object.h"

namespace omaha {

class App;
class AppBundle;
class Package;

class AppVersion : public ModelObject {
 public:
  explicit AppVersion(App* app);
  virtual ~AppVersion();

  // IAppVersion.
  STDMETHOD(get_version)(BSTR* version);
  STDMETHOD(get_packageCount)(long* count);  // NOLINT
  STDMETHOD(get_package)(long index, Package** package);  // NOLINT

  App* app();
  const App* app() const;

  CString version() const;
  void set_version(const CString& version);

  const xml::InstallManifest* install_manifest() const;
  void set_install_manifest(xml::InstallManifest* install_manifest);

  // Returns the number of packages that make up this app version.
  size_t GetNumberOfPackages() const;

  // Gets the package at the specified index.
  Package* GetPackage(size_t index);
  const Package* GetPackage(size_t index) const;

  // Adds a package to this app version.
  HRESULT AddPackage(const CString& filename, uint32 size,
                     const CString& expected_hash);

  // Returns the list of download servers to use in order of preference.
  const std::vector<CString>& download_base_urls() const;

  // Adds a download server to the list of fallbacks. Servers are attempted in
  // the order they are added.
  HRESULT AddDownloadBaseUrl(const CString& server_url);

 private:
  // product version "pv".
  CString version_;

  std::unique_ptr<xml::InstallManifest> install_manifest_;

  std::vector<Package*> packages_;

  std::vector<CString> download_base_urls_;

  App* app_;

  DISALLOW_COPY_AND_ASSIGN(AppVersion);
};

class ATL_NO_VTABLE AppVersionWrapper
    : public ComWrapper<AppVersionWrapper, AppVersion>,
      public IDispatchImpl<IAppVersion,
                           &__uuidof(IAppVersion),
                           &CAtlModule::m_libid,
                           kMajorTypeLibVersion,
                           kMinorTypeLibVersion> {
 public:
  // IAppVersion.
  STDMETHOD(get_version)(BSTR* version);
  STDMETHOD(get_packageCount)(long* count);  // NOLINT
  STDMETHOD(get_package)(long index, IDispatch** package);  // NOLINT

 protected:
  AppVersionWrapper() {}
  virtual ~AppVersionWrapper() {}

  BEGIN_COM_MAP(AppVersionWrapper)
    COM_INTERFACE_ENTRY(IAppVersion)
    COM_INTERFACE_ENTRY(IDispatch)
  END_COM_MAP()

 private:
  typedef ComWrapper<AppVersionWrapper, AppVersion> Creator;
  friend class Creator;
};

// Copies all packages of an app version to the specified directory.
HRESULT CopyAppVersionPackages(const AppVersion* app_version,
                               const CString& dir);

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_APP_VERSION_H_
