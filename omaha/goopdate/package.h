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

// Defines the Package COM object exposed by the model.

// TODO(omaha3): Protect all public members with the model lock and assert in
// all non-public members that the model has been locked by the caller.

#ifndef OMAHA_GOOPDATE_PACKAGE_H_
#define OMAHA_GOOPDATE_PACKAGE_H_

#include <atlbase.h>
#include <atlcom.h>
#include "base/basictypes.h"
#include "goopdate/omaha3_idl.h"
#include "omaha/base/constants.h"
#include "omaha/base/time.h"
#include "omaha/common/progress_sampler.h"
#include "omaha/goopdate/com_wrapper_creator.h"
#include "omaha/goopdate/model_object.h"
// TODO(omaha): Consider implementing the NetworkRequestCallback portion in a
// PImpl or similar pattern. As it is, every file that includes model.h also
// becomes dependent on most of net/.
#include "omaha/net/network_request.h"

namespace omaha {

class AppVersion;
struct Lockable;

class Package
    : public ModelObject,
      public NetworkRequestCallback {
 public:
  explicit Package(AppVersion* parent_app_version);
  virtual ~Package();

  STDMETHOD(get)(BSTR dir) const;
  STDMETHOD(get_isAvailable)(VARIANT_BOOL* is_available) const;
  STDMETHOD(get_filename)(BSTR* filename) const;

  // NetworkRequestCallback.
  virtual void OnProgress(int bytes,
                          int bytes_total,
                          int status,
                          const TCHAR* status_text);
  virtual void OnRequestBegin();
  virtual void OnRequestRetryScheduled(time64 next_download_retry_time);

  void SetFileInfo(const CString& filename, uint64 size, const CString& hash);

  // Returns the name of the file specified in the manifest.
  CString filename() const;
  // Returns the expected size of the file in bytes.
  uint64 expected_size() const;
  // Returns expected file hashes.
  CString expected_hash() const;

  uint64 bytes_downloaded() const;

  time64 next_download_retry_time() const;

  AppVersion* app_version();
  const AppVersion* app_version() const;

  LONG GetEstimatedRemainingDownloadTimeMs() const;

 private:
  // Weak reference to the parent of the package.
  AppVersion* app_version_;

  // The name of the package as it appears in the manifest.
  CString filename_;
  uint64 expected_size_;
  CString expected_hash_;

  int bytes_downloaded_;
  int bytes_total_;
  time64 next_download_retry_time_;

  ProgressSampler<int> progress_sampler_;

  // True if the package is being downloaded.
  // TODO(omaha): implement this.
  bool is_downloading_;

  DISALLOW_COPY_AND_ASSIGN(Package);
};

class ATL_NO_VTABLE PackageWrapper
    : public ComWrapper<PackageWrapper, Package>,
      public IDispatchImpl<IPackage,
                           &__uuidof(IPackage),
                           &CAtlModule::m_libid,
                           kMajorTypeLibVersion,
                           kMinorTypeLibVersion> {
 public:
  // IPackage.
  STDMETHOD(get)(BSTR dir);
  STDMETHOD(get_isAvailable)(VARIANT_BOOL* is_available);
  STDMETHOD(get_filename)(BSTR* filename);

 protected:
  PackageWrapper() {}
  virtual ~PackageWrapper() {}

  BEGIN_COM_MAP(PackageWrapper)
    COM_INTERFACE_ENTRY(IPackage)
    COM_INTERFACE_ENTRY(IDispatch)
  END_COM_MAP()
};

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_PACKAGE_H_
