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

#ifndef OMAHA_GOOPDATE_PACKAGE_CACHE_H_
#define OMAHA_GOOPDATE_PACKAGE_CACHE_H_

#include <windows.h>
#include <atlstr.h>
#include <vector>
#include "base/basictypes.h"
#include "base/synchronized.h"
#include "omaha/base/safe_format.h"

namespace omaha {

class File;

class PackageCache {
 public:
  // Defines the key that uniquely identifies the packages in the cache.
  // The key uses a default version value in the case the application does
  // not provide a version string.
  class Key {
   public:
    Key(const CString& app_id,
        const CString& version,
        const CString& package_name)
        : app_id_(app_id),
          version_(version.IsEmpty() ? CString(_T("0.0.0.0")) : version),
          package_name_(package_name) {}

    CString app_id() const { return app_id_; }
    CString version() const { return version_; }
    CString package_name() const { return package_name_; }

    CString ToString() const {
      CString result;
      SafeCStringFormat(&result, _T("appid=%s; version=%s; package_name=%s"),
                        app_id_, version_, package_name_);
      return result;
    }

   private:
    const CString app_id_;
    const CString version_;
    const CString package_name_;

    DISALLOW_COPY_AND_ASSIGN(Key);
  };

  PackageCache();
  ~PackageCache();

  HRESULT Initialize(const CString& cache_root);

  HRESULT Put(const Key& key,
              File* source_file,
              const CString& hash);

  HRESULT Get(const Key& key,
              const CString& destination_file,
              const CString& hash) const;

  bool IsCached(const Key& key, const CString& hash) const;

  HRESULT Purge(const Key& key);

  HRESULT PurgeVersion(const CString& app_id, const CString& version);

  HRESULT PurgeApp(const CString& app_id);

  // Purges version directories lower than a 'version' of the form 1.2.3.4. If
  // the version format is not recognized, returns E_INVALIDARG.
  HRESULT PurgeAppLowerVersions(const CString& app_id, const CString& version);

  HRESULT PurgeAll();

  // Purges expired packages and keeps total cache size below the limit by
  // purging oldest ones.
  HRESULT PurgeOldPackagesIfNecessary() const;

  // Returns the total size of all files in the cache. Returns 0 if the size
  // cannot be determined or the cache is empty.
  uint64 Size() const;

  CString cache_root() const;

  static HRESULT VerifyHash(const CString& filename,
                            const CString& expected_hash);

 private:
  friend class PackageCacheTest;

  HRESULT BuildCacheFileNameForKey(const Key& key, CString* filename) const;
  HRESULT BuildCacheFileName(const CString& app_id,
                             const CString& version,
                             const CString& package_name,
                             CString* filename) const;

  // Deletes the cache entries that match the app_id, version, and package_name.
  // If the parameters are empty, the function deletes the packages of versions
  // of apps, respectively.
  HRESULT Delete(const CString& app_id,
                 const CString& version,
                 const CString& package_name);

  // Returns the cache expiration time. All files in the cache before that time
  // are considered as expired and should be purged.
  FILETIME GetCacheExpirationTime() const;

  // The cache duration, specified as a count of days.  (This is converted to
  // an absolute time by GetCacheExpirationTime().)
  int cache_time_limit_days_;

  // The maximum allowed cache size, in bytes. If the cache grows over this
  // size, files will be purged using a least-recently-added metric.
  uint64 cache_size_limit_bytes_;

  CString cache_root_;

  LLock cache_lock_;

  DISALLOW_COPY_AND_ASSIGN(PackageCache);
};

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_PACKAGE_CACHE_H_

