// Copyright 2009 Google Inc.
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

#include "omaha/goopdate/package_cache.h"

#include <shlwapi.h>
#include <algorithm>
#include <vector>

#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/file.h"
#include "omaha/base/logging.h"
#include "omaha/base/path.h"
#include "omaha/base/string.h"
#include "omaha/base/signatures.h"
#include "omaha/base/signaturevalidator.h"
#include "omaha/base/utils.h"
#include "omaha/common/config_manager.h"
#include "omaha/goopdate/package_cache_internal.h"
#include "omaha/goopdate/worker_metrics.h"

namespace omaha {

namespace internal {

bool PackageSortByTimePredicate(const PackageInfo& package1,
                                const PackageInfo& package2) {
  return ::CompareFileTime(&package1.file_time, &package2.file_time) > 0;
}

bool IsSpecialDirectoryFindData(const WIN32_FIND_DATA& find_data) {
  return find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY &&
         (String_StrNCmp(find_data.cFileName, _T("."), 2, false) == 0 ||
          String_StrNCmp(find_data.cFileName, _T(".."), 3, false) == 0);
}

bool IsSubDirectoryFindData(const WIN32_FIND_DATA& find_data) {
  return find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY &&
         !IsSpecialDirectoryFindData(find_data);
}

bool IsFileFindData(const WIN32_FIND_DATA& find_data) {
  return ((find_data.dwFileAttributes == FILE_ATTRIBUTE_NORMAL) ||
          !(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY));
}

HRESULT FindAllPackagesInfo(const CString& cache_root,
                            std::vector<PackageInfo>* packages_info) {
  ASSERT1(packages_info);
  ASSERT1(packages_info->empty());
  return FindDirectoryPackagesInfo(cache_root,
                                   CACHE_DIRECTORY_ROOT,
                                   packages_info);
}

HRESULT FindAppPackagesInfo(const CString& app_dir,
                            std::vector<PackageInfo>* packages_info) {
  return FindDirectoryPackagesInfo(app_dir, CACHE_DIRECTORY_APP, packages_info);
}

HRESULT FindVersionPackagesInfo(const CString& version_dir,
                                std::vector<PackageInfo>* packages_info) {
  return FindDirectoryPackagesInfo(version_dir,
                                   CACHE_DIRECTORY_VERSION,
                                   packages_info);
}

HRESULT FindFilePackagesInfo(const CString& version_dir,
                             const WIN32_FIND_DATA& find_data,
                             std::vector<PackageInfo>* packages_info) {
  PackageInfo package_info;
  package_info.file_name = ConcatenatePath(version_dir, find_data.cFileName);
  package_info.file_time = find_data.ftCreationTime;
  package_info.file_size.LowPart = find_data.nFileSizeLow;
  package_info.file_size.HighPart = find_data.nFileSizeHigh;
  packages_info->push_back(package_info);

  return S_OK;
}

HRESULT FindDirectoryPackagesInfo(const CString& dir_path,
                                  CacheDirectoryType dir_type,
                                  std::vector<PackageInfo>* packages_info) {
  CORE_LOG(L4, (_T("[FindDirectoryPackagesInfo][%s][%d]"), dir_path, dir_type));

  WIN32_FIND_DATA find_data = {0};
  scoped_hfind hfind(::FindFirstFile(dir_path + _T("\\*"), &find_data));
  if (!hfind) {
    HRESULT hr = HRESULTFromLastError();
    CORE_LOG(L4, (_T("[FindDirectoryPackagesInfo failed][0x%x]"), hr));
    return hr;
  }

  HRESULT hr = S_OK;
  do {
    switch (dir_type) {
      case CACHE_DIRECTORY_ROOT:
        if (IsSubDirectoryFindData(find_data)) {
          CString app_dir = ConcatenatePath(dir_path, find_data.cFileName);
          hr = FindAppPackagesInfo(app_dir, packages_info);
        }
        break;
      case CACHE_DIRECTORY_APP:
        if (IsSubDirectoryFindData(find_data)) {
          CString version_dir = ConcatenatePath(dir_path, find_data.cFileName);
          hr = FindVersionPackagesInfo(version_dir, packages_info);
        }
        break;
      case CACHE_DIRECTORY_VERSION:
        if (IsFileFindData(find_data)) {
          hr = FindFilePackagesInfo(dir_path, find_data, packages_info);
        }
        break;
    }

    if (FAILED(hr)) {
      CORE_LOG(L4, (_T("[FindDirectoryPackagesInfo failed][0x%x]"), hr));
      return hr;
    }
  } while (::FindNextFile(get(hfind), &find_data));

  return S_OK;
}

void SortPackageInfoByTime(std::vector<PackageInfo>* packages_info) {
  std::sort(packages_info->begin(),
            packages_info->end(),
            PackageSortByTimePredicate);
}

HRESULT FileCopy(File* source_file, const CString& destination) {
  ASSERT1(source_file);

  File destination_file;
  HRESULT hr = destination_file.Open(destination, true, false);
  if (FAILED(hr)) {
    return hr;
  }

  hr = source_file->SeekToBegin();
  if (FAILED(hr)) {
    return hr;
  }

  byte buffer[4096] = {};
  uint32 bytes_read = 0;
  do {
    hr = source_file->Read(arraysize(buffer), buffer, &bytes_read);
    if (FAILED(hr)) {
      return hr;
    }

    if (!bytes_read) {
      return S_OK;
    }

    uint32 bytes_written(0);
    hr = destination_file.Write(buffer, bytes_read, &bytes_written);
    if (FAILED(hr)) {
      return hr;
    }

    if (bytes_written != bytes_read) {
      return E_UNEXPECTED;
    }
  } while (bytes_read > 0);

  return S_OK;
}

}  // namespace internal

PackageCache::PackageCache() {
  cache_time_limit_days_ =
    ConfigManager::Instance()->GetPackageCacheExpirationTimeDays(NULL);

  cache_size_limit_bytes_ = 1024 * 1024 * static_cast<uint64>(
    ConfigManager::Instance()->GetPackageCacheSizeLimitMBytes(NULL));
}

PackageCache::~PackageCache() {
}

HRESULT PackageCache::Initialize(const CString& cache_root) {
  CORE_LOG(L3, (_T("[PackageCache::Initialize][%s]"), cache_root));

  __mutexScope(cache_lock_);

  if (!IsAbsolutePath(cache_root)) {
    return E_INVALIDARG;
  }

  HRESULT hr = CreateDir(cache_root, NULL);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[CreateDir failed][0x%x][%s]"), hr, cache_root));
    return hr;
  }

  cache_root_ = cache_root;

  return S_OK;
}

bool PackageCache::IsCached(const Key& key, const CString& hash) const {
  CORE_LOG(L3, (_T("[PackageCache::IsCached][key '%s'][hash %s]"),
                key.ToString(), hash));

  __mutexScope(cache_lock_);

  CString filename;
  HRESULT hr = BuildCacheFileNameForKey(key, &filename);
  if (FAILED(hr)) {
    return false;
  }

  return File::Exists(filename) && SUCCEEDED(VerifyHash(filename, hash));
}

HRESULT PackageCache::Put(const Key& key,
                          File* source_file,
                          const CString& hash) {
  ASSERT1(source_file);

  ++metric_worker_package_cache_put_total;
  CORE_LOG(L3, (_T("[PackageCache::Put][key '%s'][hash %s]"),
                key.ToString(), hash));

  __mutexScope(cache_lock_);

  if (key.app_id().IsEmpty() || key.version().IsEmpty() ||
      key.package_name().IsEmpty() ) {
    return E_INVALIDARG;
  }

  CString destination_file;
  HRESULT hr = BuildCacheFileNameForKey(key, &destination_file);
  CORE_LOG(L3, (_T("[destination file '%s']"), destination_file));
  if (FAILED(hr)) {
    return hr;
  }

  hr = CreateDir(GetDirectoryFromPath(destination_file), NULL);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[failed to create cache directory][0x%08x][%s]"),
                  hr, destination_file));
    return hr;
  }

  // TODO(omaha): consider not overwriting the file if the file is
  // in the cache and it is valid.

  hr = internal::FileCopy(source_file, destination_file);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[failed to copy file to cache][0x%08x][%s]"),
                  hr, destination_file));
    return hr;
  }

  hr = VerifyHash(destination_file, hash);
  if (FAILED(hr)) {
    CORE_LOG(LE,
        (_T("[failed to verify hash for file '%s'][expected hash %s]"),
        destination_file, hash));
    VERIFY1(::DeleteFile(destination_file));
    return hr;
  }

  ++metric_worker_package_cache_put_succeeded;
  return S_OK;
}

HRESULT PackageCache::Get(const Key& key,
                          const CString& destination_file,
                          const CString& hash) const {
  CORE_LOG(L3, (_T("[PackageCache::Get][key '%s'][dest file '%s'][hash '%s']"),
      key.ToString(), destination_file, hash));

  __mutexScope(cache_lock_);

  if (key.app_id().IsEmpty() || key.version().IsEmpty() ||
      key.package_name().IsEmpty() ) {
    return E_INVALIDARG;
  }

  CString source_file;
  HRESULT hr = BuildCacheFileNameForKey(key, &source_file);
  CORE_LOG(L3, (_T("[source file '%s']"), source_file));
  if (FAILED(hr)) {
    return hr;
  }

  if (!File::Exists(source_file)) {
    return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
  }

  hr = VerifyHash(source_file, hash);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[failed to verify hash for file '%s'][expected hash %s]"),
        source_file, hash));
    return hr;
  }

  return File::Copy(source_file, destination_file, true);
}

HRESULT PackageCache::Purge(const Key& key) {
  CORE_LOG(L3, (_T("[PackageCache::Purge][key '%s']"), key.ToString()));

  __mutexScope(cache_lock_);

  return Delete(key.app_id(), key.version(), key.package_name());
}

HRESULT PackageCache::PurgeVersion(const CString& app_id,
                                   const CString& version) {
  CORE_LOG(L3, (_T("[PackageCache::PurgeVersion][app_id '%s'][version '%s']"),
                app_id, version));

  __mutexScope(cache_lock_);

  return Delete(app_id, version, _T(""));
}

HRESULT PackageCache::PurgeApp(const CString& app_id) {
  CORE_LOG(L3, (_T("[PackageCache::PurgeApp][app_id '%s']"), app_id));

  __mutexScope(cache_lock_);

  return Delete(app_id, _T(""), _T(""));
}

HRESULT PackageCache::PurgeAppLowerVersions(const CString& app_id,
                                            const CString& version) {
  CORE_LOG(L3, (_T("[PackageCache::PurgeAppLowerVersions][%s][%s]"),
                app_id, version));

  __mutexScope(cache_lock_);

  ULONGLONG my_version = VersionFromString(version);
  if (!my_version) {
    return E_INVALIDARG;
  }

  CString app_id_path;
  HRESULT hr = BuildCacheFileName(app_id, CString(), CString(), &app_id_path);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[BuildCacheFileName fail][%s][0x%x]"), app_id_path, hr));
    return hr;
  }

  WIN32_FIND_DATA find_data = {0};
  scoped_hfind hfind(::FindFirstFile(app_id_path + _T("\\*"), &find_data));
  if (!hfind) {
    hr = HRESULTFromLastError();
    CORE_LOG(L4, (_T("[FindFirstFile failed][%s][0x%x]"), app_id_path, hr));
    return hr;
  }

  do {
    if (internal::IsSpecialDirectoryFindData(find_data)) {
      continue;
    }
    ASSERT1(internal::IsSubDirectoryFindData(find_data));

    ULONGLONG found_version = VersionFromString(find_data.cFileName);
    if (!found_version || found_version >= my_version) {
      CORE_LOG(L2, (_T("[Not purging version][%s]"), find_data.cFileName));
      continue;
    }

    CString version_dir = ConcatenatePath(app_id_path, find_data.cFileName);
    hr = DeleteBeforeOrAfterReboot(version_dir);
    CORE_LOG(L3, (_T("[Purge version][%s][0x%x]"), version_dir, hr));
  } while (::FindNextFile(get(hfind), &find_data));

  return S_OK;
}

HRESULT PackageCache::PurgeAll() {
  CORE_LOG(L3, (_T("[PackageCache::PurgeAll]")));

  __mutexScope(cache_lock_);

  // Deletes the cache root including all the cache entries.
  HRESULT hr = Delete(_T(""), _T(""), _T(""));
  if (FAILED(hr)) {
    return hr;
  }

  // Recreate the cache root.
  hr = CreateDir(cache_root_, NULL);
  if (FAILED(hr)) {
    CORE_LOG(LW, (_T("[CreateDir failed][0x%x][%s]"), hr, cache_root_));
    return hr;
  }

  return hr;
}

FILETIME PackageCache::GetCacheExpirationTime() const {
  FILETIME current_time = {0};
  ::GetSystemTimeAsFileTime(&current_time);
  ULARGE_INTEGER now = {0};
  now.LowPart = current_time.dwLowDateTime;
  now.HighPart = current_time.dwHighDateTime;

  const uint64 kNum100NanoSecondsInDay = 1000LL * 1000 * 10 * kSecondsPerDay;
  now.QuadPart -= kNum100NanoSecondsInDay * cache_time_limit_days_;

  FILETIME expiration_time = {0};
  expiration_time.dwLowDateTime = now.LowPart;
  expiration_time.dwHighDateTime = now.HighPart;

  return expiration_time;
}

HRESULT PackageCache::PurgeOldPackagesIfNecessary() const {
  __mutexScope(cache_lock_);

  std::vector<internal::PackageInfo> packages_info;
  HRESULT hr = internal::FindAllPackagesInfo(cache_root_, &packages_info);

  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[internal::FindAllPackagesInfo failed][0x%x]"), hr));
    return hr;
  }
  internal::SortPackageInfoByTime(&packages_info);

  FILETIME expiration_time = GetCacheExpirationTime();

  // Delete cached package based on the package info.
  std::vector<internal::PackageInfo>::const_iterator it;
  uint64 total_cache_size = 0;
  for (it = packages_info.begin(); it != packages_info.end(); ++it) {
    total_cache_size += it->file_size.QuadPart;
    if (total_cache_size > cache_size_limit_bytes_) {
      break;  // Remaining packages should be deleted as size limit is reached.
    }
    if (::CompareFileTime(&it->file_time, &expiration_time) < 0) {
      break;  // Remaining packages should be deleted as they are expired.
    }
  }

  for (; it != packages_info.end(); ++it) {
    hr = DeleteBeforeOrAfterReboot(it->file_name);
  }

  return hr;
}

HRESULT PackageCache::Delete(const CString& app_id,
                             const CString& version,
                             const CString& package_name) {
  CString filename;
  HRESULT hr = BuildCacheFileName(app_id, version, package_name, &filename);
  CORE_LOG(L3, (_T("[PackageCache::Delete '%s']"), filename));
  if (FAILED(hr)) {
    return hr;
  }

  return DeleteBeforeOrAfterReboot(filename);
}

CString PackageCache::cache_root() const {
  __mutexScope(cache_lock_);

  ASSERT1(!cache_root_.IsEmpty());
  ASSERT1(File::Exists(cache_root_));

  return cache_root_;
}

uint64 PackageCache::Size() const {
  uint64 result(0);
  return SUCCEEDED(GetDirectorySize(cache_root_, &result)) ? result : 0;
}

HRESULT PackageCache::BuildCacheFileNameForKey(const Key& key,
                                               CString* filename) const {
  ASSERT1(filename);

  return BuildCacheFileName(key.app_id(),
                            key.version(),
                            key.package_name(),
                            filename);
}

HRESULT PackageCache::BuildCacheFileName(const CString& app_id,
                                         const CString& version,
                                         const CString& package_name,
                                         CString* filename) const {
  ASSERT1(filename);

  // Validate the package name does not contain the "..".
  if (package_name.Find(_T("..")) != -1) {
    return E_INVALIDARG;
  }

  CString tmp_filename;
  tmp_filename = ConcatenatePath(cache_root_, app_id);
  tmp_filename = ConcatenatePath(tmp_filename, version);
  tmp_filename = ConcatenatePath(tmp_filename, package_name);

  *filename = tmp_filename;

  return S_OK;
}

HRESULT PackageCache::VerifyHash(const CString& filename,
                                 const CString& expected_hash) {
  CORE_LOG(L3, (_T("[PackageCache::VerifyHash][%s][%s]"),
           filename, expected_hash));
  HighresTimer verification_timer;

  std::vector<CString> files;
  files.push_back(filename);

  HRESULT hr = VerifyFileHashSha256(files, expected_hash);
  CORE_LOG(L3, (_T("[PackageCache::VerifyHash completed][0x%08x][%d ms]"),
                hr, verification_timer.GetElapsedMs()));
  return hr;
}

}  // namespace omaha

