// Copyright 2007-2010 Google Inc.
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

#ifndef OMAHA_GOOPDATE_DOWNLOAD_MANAGER_H_
#define OMAHA_GOOPDATE_DOWNLOAD_MANAGER_H_

#include <windows.h>
#include <atlstr.h>
#include <memory>
#include <vector>

#include "base/basictypes.h"

namespace omaha {

class App;
struct ErrorContext;
class File;
class HttpClient;
struct Lockable;        // TODO(omaha): make Lockable a class.
class NetworkRequest;
class Package;
class PackageCache;

// Public interface for the DownloadManager.
class DownloadManagerInterface {
 public:
  virtual ~DownloadManagerInterface() {}
  virtual HRESULT Initialize() = 0;
  virtual HRESULT PurgeAppLowerVersions(const CString& app_id,
                                        const CString& version) = 0;
  virtual HRESULT CachePackage(const Package* package,
                               File* source_file,
                               const CString* source_file_path) = 0;
  virtual HRESULT DownloadApp(App* app) = 0;
  virtual HRESULT GetPackage(const Package* package,
                             const CString& dir) const = 0;
  virtual bool IsPackageAvailable(const Package* package) const = 0;
  virtual void Cancel(App* app) = 0;
  virtual void CancelAll() = 0;
  virtual bool IsBusy() const = 0;
};

class DownloadManager : public DownloadManagerInterface {
 public:
  explicit DownloadManager(bool is_machine);
  virtual ~DownloadManager();

  virtual HRESULT Initialize();

  virtual HRESULT PurgeAppLowerVersions(const CString& app_id,
                                        const CString& version);

  virtual HRESULT CachePackage(const Package* package,
                               File* source_file,
                               const CString* source_file_path);

  // Downloads the specified app and stores its packages in the package cache.
  //
  // This is a blocking call. All errors are reported through the return value.
  // Callers may use GetMessageForError() to convert this error value to an
  // error message. Progress is reported via the NetworkRequestCallback
  // method on the Package objects.
  virtual HRESULT DownloadApp(App* app);

  // Retrieves a package from the cache, if the package is locally available.
  virtual HRESULT GetPackage(const Package* package, const CString& dir) const;

  // Returns true if the specified package is in the package cache.
  virtual bool IsPackageAvailable(const Package* package) const;

  // Cancels the download of specified app and makes DownloadApp return to the
  // caller at some point in the future. Cancel can be called multiple times
  // until the DownloadApp returns.
  virtual void Cancel(App* app);

  // Cancels download of all apps currently downloading.
  virtual void CancelAll();

  // Returns true if applications are downloading.
  virtual bool IsBusy() const;

  // Returns a formatted message for the specified error in given language.
  static CString GetMessageForError(const ErrorContext& error_context,
                                    const CString& language);

 private:
  // Maintains per-app download state.
  class State {
   public:
    State(App* app, NetworkRequest* network_request);
    ~State();

    App* app() const { return app_; }

    NetworkRequest* network_request() const;

    HRESULT CancelNetworkRequest();

   private:
    // Not owned by this object.
    App* app_;

    std::unique_ptr<NetworkRequest> network_request_;

    DISALLOW_COPY_AND_ASSIGN(State);
  };

  // Creates a download state corresponding to the app. The state object is
  // owned by the download manager. A pointer to the state object is returned
  // to the caller.
  HRESULT CreateStateForApp(App* app, State** state);

  HRESULT DeleteStateForApp(App* app);

  HRESULT DoDownloadPackage(Package* package, State* state);
  HRESULT DoDownloadPackageFromUrl(const CString& url,
                                   const CString& filename,
                                   Package* package,
                                   State* state);

  HRESULT EnsureSignatureIsValid(const CString& file_path);

  bool is_machine() const;

  CString package_cache_root() const;

  PackageCache* package_cache();
  const PackageCache* package_cache() const;

  const Lockable& lock() const;

  // Returns the full path to a unique filename.
  static HRESULT BuildUniqueFileName(const CString& filename,
                                     CString* unique_filename);

  // Locks shared instance state for concurrent downloads. This lock is
  // owned by this class.
  mutable Lockable* volatile lock_;

  bool is_machine_;

  // The root of the package_cache.
  CString package_cache_root_;

  std::vector<State*> download_state_;

  std::unique_ptr<PackageCache> package_cache_;

  friend class DownloadManagerTest;
  DISALLOW_COPY_AND_ASSIGN(DownloadManager);
};

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_DOWNLOAD_MANAGER_H_
